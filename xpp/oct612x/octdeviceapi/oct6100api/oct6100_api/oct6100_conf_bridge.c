/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: oct6100_conf_bridge.c

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	This file contains all functions related to a conference bridge.  Procedures
	needed to open/close a bridge, add/remove a participant to a conference 
	bridge, mute/unmute a participant, etc..  are all present in this source 
	file.

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

$Octasic_Revision: 146 $

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
#include "oct6100api/oct6100_mixer_inst.h"
#include "oct6100api/oct6100_conf_bridge_inst.h"

#include "oct6100api/oct6100_interrupts_pub.h"
#include "oct6100api/oct6100_chip_open_pub.h"
#include "oct6100api/oct6100_channel_pub.h"
#include "oct6100api/oct6100_mixer_pub.h"
#include "oct6100api/oct6100_conf_bridge_pub.h"

#include "oct6100_chip_open_priv.h"
#include "oct6100_miscellaneous_priv.h"
#include "oct6100_memory_priv.h"
#include "oct6100_tsst_priv.h"
#include "oct6100_channel_priv.h"
#include "oct6100_mixer_priv.h"
#include "oct6100_conf_bridge_priv.h"


/****************************  PUBLIC FUNCTIONS  *****************************/


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ConfBridgeOpen

Description:    This function opens a conference bridge.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pConfBridgeOpen		Pointer to conference bridge open structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ConfBridgeOpenDef
UINT32 Oct6100ConfBridgeOpenDef(
				tPOCT6100_CONF_BRIDGE_OPEN			f_pConfBridgeOpen )
{
	f_pConfBridgeOpen->pulConfBridgeHndl = NULL;
	f_pConfBridgeOpen->fFlexibleConferencing = FALSE;

	return cOCT6100_ERR_OK;
}
#endif


#if !SKIP_Oct6100ConfBridgeOpen
UINT32 Oct6100ConfBridgeOpen(
				tPOCT6100_INSTANCE_API				f_pApiInstance,
				tPOCT6100_CONF_BRIDGE_OPEN			f_pConfBridgeOpen )
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
		ulFncRes = Oct6100ConfBridgeOpenSer( f_pApiInstance, f_pConfBridgeOpen );
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

Function:		Oct6100ConfBridgeClose

Description:    This function closes a conference bridge.  A conference
				bridge can only be closed if no participants are present on 
				the bridge.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pConfBridgeClose		Pointer to conference bridge close structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ConfBridgeCloseDef
UINT32 Oct6100ConfBridgeCloseDef(
				tPOCT6100_CONF_BRIDGE_CLOSE			f_pConfBridgeClose )
{
	f_pConfBridgeClose->ulConfBridgeHndl = cOCT6100_INVALID_HANDLE;

	return cOCT6100_ERR_OK;
}
#endif


#if !SKIP_Oct6100ConfBridgeClose
UINT32 Oct6100ConfBridgeClose(
				tPOCT6100_INSTANCE_API				f_pApiInstance,
				tPOCT6100_CONF_BRIDGE_CLOSE			f_pConfBridgeClose )
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
		ulFncRes = Oct6100ConfBridgeCloseSer( f_pApiInstance, f_pConfBridgeClose );
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

Function:		Oct6100ConfBridgeChanAdd

Description:    This function adds an echo channel (participant) to a 
				conference bridge.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pConfBridgeAdd		Pointer to conference bridge channel addition structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ConfBridgeChanAddDef
UINT32 Oct6100ConfBridgeChanAddDef(
				tPOCT6100_CONF_BRIDGE_CHAN_ADD		f_pConfBridgeAdd )
{
	f_pConfBridgeAdd->ulConfBridgeHndl = cOCT6100_INVALID_HANDLE;
	f_pConfBridgeAdd->ulChannelHndl = cOCT6100_INVALID_HANDLE;
	f_pConfBridgeAdd->ulInputPort = cOCT6100_CHANNEL_PORT_SOUT;
	f_pConfBridgeAdd->ulListenerMaskIndex = cOCT6100_INVALID_VALUE;
	f_pConfBridgeAdd->ulListenerMask = 0;
	f_pConfBridgeAdd->fMute = FALSE;
	f_pConfBridgeAdd->ulTappedChannelHndl = cOCT6100_INVALID_HANDLE;

	return cOCT6100_ERR_OK;
}
#endif


#if !SKIP_Oct6100ConfBridgeChanAdd
UINT32 Oct6100ConfBridgeChanAdd(
				tPOCT6100_INSTANCE_API				f_pApiInstance,
				tPOCT6100_CONF_BRIDGE_CHAN_ADD		f_pConfBridgeAdd )
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
		ulFncRes = Oct6100ConfBridgeChanAddSer( f_pApiInstance, f_pConfBridgeAdd );
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

Function:		Oct6100ConfBridgeChanRemove

Description:    This function removes an echo channel (participant) from a 
				conference bridge.  All participants can be removed from
				the bridge if a special flag (fRemoveAll) is set to TRUE.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pConfBridgeRemove		Pointer to conference bridge channel removal structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ConfBridgeChanRemoveDef
UINT32 Oct6100ConfBridgeChanRemoveDef(
				tPOCT6100_CONF_BRIDGE_CHAN_REMOVE	f_pConfBridgeRemove )
{
	f_pConfBridgeRemove->ulChannelHndl = cOCT6100_INVALID_HANDLE;
	f_pConfBridgeRemove->ulConfBridgeHndl = cOCT6100_INVALID_HANDLE;
	f_pConfBridgeRemove->fRemoveAll = FALSE;

	return cOCT6100_ERR_OK;
}
#endif


#if !SKIP_Oct6100ConfBridgeChanRemove
UINT32 Oct6100ConfBridgeChanRemove(
			tPOCT6100_INSTANCE_API				f_pApiInstance,
			tPOCT6100_CONF_BRIDGE_CHAN_REMOVE	f_pConfBridgeRemove )
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
		ulFncRes = Oct6100ConfBridgeChanRemoveSer( f_pApiInstance, f_pConfBridgeRemove );
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

Function:		Oct6100ConfBridgeChanMute

Description:    This function mutes a participant present on a conference bridge.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pConfBridgeMute		Pointer to conference bridge channel mute structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ConfBridgeChanMuteDef
UINT32 Oct6100ConfBridgeChanMuteDef(
				tPOCT6100_CONF_BRIDGE_CHAN_MUTE		f_pConfBridgeMute )
{
	f_pConfBridgeMute->ulChannelHndl = cOCT6100_INVALID_HANDLE;

	return cOCT6100_ERR_OK; 
}
#endif


#if !SKIP_Oct6100ConfBridgeChanMute
UINT32 Oct6100ConfBridgeChanMute(
				tPOCT6100_INSTANCE_API				f_pApiInstance,
				tPOCT6100_CONF_BRIDGE_CHAN_MUTE		f_pConfBridgeMute )
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
		ulFncRes = Oct6100ConfBridgeChanMuteSer( f_pApiInstance, f_pConfBridgeMute );
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

Function:		Oct6100ConfBridgeChanUnMute

Description:    This function unmutes a channel on a bridge. The other member 
				of the conference will start to hear this participant again.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pConfBridgeUnMute		Pointer to conference bridge channel unmute structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ConfBridgeChanUnMuteDef
UINT32 Oct6100ConfBridgeChanUnMuteDef(
				tPOCT6100_CONF_BRIDGE_CHAN_UNMUTE	f_pConfBridgeUnMute )
{
	f_pConfBridgeUnMute->ulChannelHndl = cOCT6100_INVALID_HANDLE;

	return cOCT6100_ERR_OK;
}
#endif

#if !SKIP_Oct6100ConfBridgeChanUnMute
UINT32 Oct6100ConfBridgeChanUnMute(
				tPOCT6100_INSTANCE_API				f_pApiInstance,
				tPOCT6100_CONF_BRIDGE_CHAN_UNMUTE	f_pConfBridgeUnMute )
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
		ulFncRes = Oct6100ConfBridgeChanUnMuteSer( f_pApiInstance, f_pConfBridgeUnMute );
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

Function:		Oct6100ConfBridgeDominantSpeakerSet

Description:    This function sets a participant present on a conference 
				bridge as the dominant speaker.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to 
						keep the present state of the chip and all its 
						resources.

f_pConfBridgeDominant	Pointer to conference bridge dominant speaker 
						structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ConfBridgeDominantSpeakerSetDef
UINT32 Oct6100ConfBridgeDominantSpeakerSetDef(
				tPOCT6100_CONF_BRIDGE_DOMINANT_SPEAKER_SET	f_pConfBridgeDominantSpeaker )
{
	f_pConfBridgeDominantSpeaker->ulChannelHndl = cOCT6100_INVALID_HANDLE;
	f_pConfBridgeDominantSpeaker->ulConfBridgeHndl = cOCT6100_INVALID_HANDLE;

	return cOCT6100_ERR_OK; 
}
#endif


#if !SKIP_Oct6100ConfBridgeDominantSpeakerSet
UINT32 Oct6100ConfBridgeDominantSpeakerSet(
				tPOCT6100_INSTANCE_API						f_pApiInstance,
				tPOCT6100_CONF_BRIDGE_DOMINANT_SPEAKER_SET	f_pConfBridgeDominantSpeaker )
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
		ulFncRes = Oct6100ConfBridgeDominantSpeakerSetSer( f_pApiInstance, f_pConfBridgeDominantSpeaker );
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

Function:		Oct6100ConfBridgeMaskChange

Description:    This function changes the mask of a flexible bridge participant.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to 
							keep the present state of the chip and all its 
							resources.

f_pConfBridgeMaskChange		Pointer to conference bridge change of mask 
							structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ConfBridgeMaskChangeDef
UINT32 Oct6100ConfBridgeMaskChangeDef(
				tPOCT6100_CONF_BRIDGE_MASK_CHANGE			f_pConfBridgeMaskChange )
{
	f_pConfBridgeMaskChange->ulChannelHndl = cOCT6100_INVALID_HANDLE;
	f_pConfBridgeMaskChange->ulNewListenerMask = 0x0;

	return cOCT6100_ERR_OK; 
}
#endif


#if !SKIP_Oct6100ConfBridgeMaskChange
UINT32 Oct6100ConfBridgeMaskChange(
				tPOCT6100_INSTANCE_API						f_pApiInstance,
				tPOCT6100_CONF_BRIDGE_MASK_CHANGE			f_pConfBridgeMaskChange )
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
		ulFncRes = Oct6100ConfBridgeMaskChangeSer( f_pApiInstance, f_pConfBridgeMaskChange );
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

Function:		Oct6100ConfBridgeGetStats

Description:    This function returns the stats for a conference bridge.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pConfBridgeStats		Pointer to conference bridge channel stats structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ConfBridgeGetStatsDef
UINT32 Oct6100ConfBridgeGetStatsDef(
				tPOCT6100_CONF_BRIDGE_STATS			f_pConfBridgeStats )
{
	f_pConfBridgeStats->ulConfBridgeHndl = cOCT6100_INVALID_HANDLE;
	f_pConfBridgeStats->ulNumChannels = cOCT6100_INVALID_STAT;
	f_pConfBridgeStats->ulNumTappedChannels = cOCT6100_INVALID_STAT;
	f_pConfBridgeStats->fFlexibleConferencing = cOCT6100_INVALID_STAT;

	return cOCT6100_ERR_OK;
}
#endif


#if !SKIP_Oct6100ConfBridgeGetStats
UINT32 Oct6100ConfBridgeGetStats(
				tPOCT6100_INSTANCE_API				f_pApiInstance,
				tPOCT6100_CONF_BRIDGE_STATS			f_pConfBridgeStats )
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
		ulFncRes = Oct6100ConfBridgeGetStatsSer( f_pApiInstance, f_pConfBridgeStats );
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

Function:		Oct6100ApiGetConfBridgeSwSizes

Description:    Gets the sizes of all portions of the API instance pertinent
				to the management of conference bridges.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pOpenChip				Pointer to chip configuration struct.
f_pInstSizes			Pointer to struct containing instance sizes.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiGetConfBridgeSwSizes
UINT32 Oct6100ApiGetConfBridgeSwSizes(
				IN		tPOCT6100_CHIP_OPEN				f_pOpenChip,
				OUT		tPOCT6100_API_INSTANCE_SIZES	f_pInstSizes )
{
	UINT32	ulTempVar;
	UINT32	ulResult;

	/* Calculate memory needed for conference bridge list. */
	if ( f_pOpenChip->ulMaxConfBridges == 0 && f_pOpenChip->fEnableChannelRecording == TRUE )
		f_pOpenChip->ulMaxConfBridges = 1;
	f_pInstSizes->ulConfBridgeList = f_pOpenChip->ulMaxConfBridges * sizeof( tOCT6100_API_CONF_BRIDGE );
	
	/* Calculate memory needed for conference bridge allocation software. */
	if ( f_pOpenChip->ulMaxConfBridges > 0 )
	{
		/* Get size of bridge allocation memory */
		ulResult = OctapiLlmAllocGetSize( f_pOpenChip->ulMaxConfBridges, &f_pInstSizes->ulConfBridgeAlloc );
		if ( ulResult != cOCT6100_ERR_OK )
			return cOCT6100_ERR_FATAL_1C;

		/* Check if the user wants to build flexible conference bridges. */
		if ( f_pOpenChip->ulMaxFlexibleConfParticipants > 0 )
		{
			/* Allocate the lowest quantity according to what the user requested. */
			if ( f_pOpenChip->ulMaxFlexibleConfParticipants < ( f_pOpenChip->ulMaxConfBridges * cOCT6100_MAX_FLEX_CONF_PARTICIPANTS_PER_BRIDGE ) )
				f_pInstSizes->ulFlexConfParticipantsList = f_pOpenChip->ulMaxFlexibleConfParticipants * sizeof( tOCT6100_API_FLEX_CONF_PARTICIPANT );
			else
			{
				f_pOpenChip->ulMaxFlexibleConfParticipants = f_pOpenChip->ulMaxConfBridges * cOCT6100_MAX_FLEX_CONF_PARTICIPANTS_PER_BRIDGE;
				f_pInstSizes->ulFlexConfParticipantsList = f_pOpenChip->ulMaxConfBridges * cOCT6100_MAX_FLEX_CONF_PARTICIPANTS_PER_BRIDGE * sizeof( tOCT6100_API_FLEX_CONF_PARTICIPANT );
			}

			/* Get size of flexible conferencing participants allocation memory */
			ulResult = OctapiLlmAllocGetSize( f_pOpenChip->ulMaxFlexibleConfParticipants, &f_pInstSizes->ulFlexConfParticipantsAlloc );
			if ( ulResult != cOCT6100_ERR_OK )
				return cOCT6100_ERR_FATAL_1C;
		}
		else
		{
			f_pInstSizes->ulFlexConfParticipantsList  = 0;
			f_pInstSizes->ulFlexConfParticipantsAlloc  = 0;
		}
	}
	else
	{
		f_pInstSizes->ulMixerEventList = 0;		
		f_pInstSizes->ulMixerEventAlloc  = 0;
		f_pInstSizes->ulConfBridgeAlloc  = 0;

		/* Make sure flexible conferencing is not used. */
		f_pInstSizes->ulFlexConfParticipantsList  = 0;
		f_pInstSizes->ulFlexConfParticipantsAlloc  = 0;
	}

	/* Calculate memory needed for list and allocation software serialization. */
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulConfBridgeList, ulTempVar )
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulConfBridgeAlloc, ulTempVar )

	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulFlexConfParticipantsList, ulTempVar )
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulFlexConfParticipantsAlloc, ulTempVar )

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiConfBridgeSwInit

Description:    Initializes all elements of the instance structure associated
				to conference bridges.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiConfBridgeSwInit
UINT32 Oct6100ApiConfBridgeSwInit(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance )
{
	tPOCT6100_SHARED_INFO				pSharedInfo;
	tPOCT6100_API_CONF_BRIDGE			pConfBridgeList;
	tPOCT6100_API_FLEX_CONF_PARTICIPANT	pFlexConfParticipantList;
	PVOID	pFlexConfPartipantsAlloc;
	UINT32	ulMaxFlexConfParicipants;
	PVOID	pConfBridgeAlloc;
	UINT32	ulMaxConfBridges;
	UINT32	ulResult;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Get the maximum number of conference bridges. */
	ulMaxConfBridges = pSharedInfo->ChipConfig.usMaxConfBridges;
	
	/*===================================================================*/
	/* Set all entries in the conference bridge list to unused. */

	mOCT6100_GET_CONF_BRIDGE_LIST_PNT( pSharedInfo, pConfBridgeList );

	/* Initialize the conference bridge allocation software to "all free". */
	if ( ulMaxConfBridges > 0 )
	{
		/* Clear the bridge memory */	
		Oct6100UserMemSet( pConfBridgeList, 0x00, ulMaxConfBridges * sizeof( tOCT6100_API_CONF_BRIDGE ));

		mOCT6100_GET_CONF_BRIDGE_ALLOC_PNT( pSharedInfo, pConfBridgeAlloc )
		
		ulResult = OctapiLlmAllocInit( &pConfBridgeAlloc, ulMaxConfBridges );
		if ( ulResult != cOCT6100_ERR_OK )
			return cOCT6100_ERR_FATAL_1E;
	}
	/*===================================================================*/


	/*===================================================================*/
	/* Set all entries in the flexible conferencing participant list to unused. */

	/* Get the maximum number of flexible conferencing participants. */
	ulMaxFlexConfParicipants = pSharedInfo->ChipConfig.usMaxFlexibleConfParticipants;

	mOCT6100_GET_FLEX_CONF_PARTICIPANT_LIST_PNT( pSharedInfo, pFlexConfParticipantList );

	/* Initialize the flexible conferencing allocation software. */
	if ( ulMaxFlexConfParicipants > 0 )
	{
		UINT32 i, ulEventIndex;
		
		/* Clear the participants memory */	
		Oct6100UserMemSet( pFlexConfParticipantList, 0x00, ulMaxFlexConfParicipants * sizeof( tOCT6100_API_FLEX_CONF_PARTICIPANT ));

		mOCT6100_GET_FLEX_CONF_PARTICIPANT_ALLOC_PNT( pSharedInfo, pFlexConfPartipantsAlloc )
		
		ulResult = OctapiLlmAllocInit( &pFlexConfPartipantsAlloc, ulMaxFlexConfParicipants );
		if ( ulResult != cOCT6100_ERR_OK )
			return cOCT6100_ERR_FATAL_1E;

		/* Initialize the conferencing indexes. */
		for ( i = 0; i < ulMaxFlexConfParicipants; i ++ )
		{
			for ( ulEventIndex = 0; ulEventIndex < cOCT6100_MAX_FLEX_CONF_PARTICIPANTS_PER_BRIDGE; ulEventIndex ++ )
				pFlexConfParticipantList[ i ].ausLoadOrAccumulateEventIndex[ ulEventIndex ] = cOCT6100_INVALID_INDEX;
		}
	}
	
	/*===================================================================*/
	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ConfBridgeOpenSer

Description:    Open a conference bridge. Note that no chip resources are 
				allocated until a channel is added to the bridge.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pConfBridgeOpen		Pointer to conference bridge configuration structure.  
						The handle identifying the conference bridge in all 
						future function calls is returned in this structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ConfBridgeOpenSer
UINT32 Oct6100ConfBridgeOpenSer(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_CONF_BRIDGE_OPEN		f_pConfBridgeOpen )
{
	UINT16	usBridgeIndex;
	UINT32	ulResult;

	/* Check the user's configuration of the conference bridge for errors. */
	ulResult = Oct6100ApiCheckBridgeParams( f_pApiInstance, f_pConfBridgeOpen );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Reserve all resources needed by the conference bridge. */
	ulResult = Oct6100ApiReserveBridgeResources( f_pApiInstance, &usBridgeIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Update the new conference bridge's entry in the conference bridge list. */
	ulResult = Oct6100ApiUpdateBridgeEntry( f_pApiInstance, f_pConfBridgeOpen, usBridgeIndex);
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckBridgeParams

Description:    Checks the user's conference bridge open configuration for errors.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pConfBridgeOpen		Pointer to conference bridge configuration structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckBridgeParams
UINT32 Oct6100ApiCheckBridgeParams(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_CONF_BRIDGE_OPEN		f_pConfBridgeOpen )
{
	/* Check for errors. */
	if ( f_pApiInstance->pSharedInfo->ChipConfig.usMaxConfBridges == 0 )
		return cOCT6100_ERR_CONF_BRIDGE_DISABLED;
	
	if ( f_pConfBridgeOpen->pulConfBridgeHndl == NULL )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;
	
	if ( f_pApiInstance->pSharedInfo->ImageInfo.fConferencing == FALSE )
		return cOCT6100_ERR_NOT_SUPPORTED_CONF_BRIDGE;

	if ( f_pConfBridgeOpen->fFlexibleConferencing != FALSE
		&& f_pConfBridgeOpen->fFlexibleConferencing != TRUE )
		return cOCT6100_ERR_CONF_BRIDGE_FLEX_CONF;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReserveBridgeResources

Description:    Reserves all resources needed for the new conference bridge.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.
	
f_pusBridgeIndex		Allocated entry in the API conference bridge list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReserveBridgeResources
UINT32 Oct6100ApiReserveBridgeResources(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				OUT		PUINT16							f_pusBridgeIndex )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	UINT32	ulResult;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;
	
	/* Reserve an entry in the conference bridge list. */
	ulResult = Oct6100ApiReserveBridgeEntry( f_pApiInstance, f_pusBridgeIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiUpdateBridgeEntry

Description:    Updates the new conference bridge's entry in the conference 
				bridge list.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pConfBridgeOpen		Pointer to conference bridge configuration structure.
f_usBridgeIndex			Allocated entry in API conference bridge list.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiUpdateBridgeEntry
UINT32 Oct6100ApiUpdateBridgeEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_CONF_BRIDGE_OPEN		f_pConfBridgeOpen,
				IN		UINT16							f_usBridgeIndex )
{
	tPOCT6100_API_CONF_BRIDGE	pBridgeEntry;
	tPOCT6100_API_CONF_BRIDGE	pTempBridgeEntry;

	/*================================================================================*/
	/* Obtain a pointer to the new conference bridge's list entry. */

	mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBridgeEntry, f_usBridgeIndex )

	/* No clients are currently connected to the bridge. */
	pBridgeEntry->usNumClients = 0;
	/* Nobody is tapped for now. */
	pBridgeEntry->usNumTappedClients = 0;
	pBridgeEntry->usFirstLoadEventPtr = cOCT6100_INVALID_INDEX;
	pBridgeEntry->usFirstSubStoreEventPtr = cOCT6100_INVALID_INDEX;
	pBridgeEntry->usLastSubStoreEventPtr = cOCT6100_INVALID_INDEX;

	pBridgeEntry->usSilenceLoadEventPtr = cOCT6100_INVALID_INDEX;

	pBridgeEntry->usLoadIndex = cOCT6100_INVALID_INDEX;

	/* Now update the bridge pointer. */
	if ( f_pApiInstance->pSharedInfo->MiscVars.usNumBridgesOpened == 0 )
	{
		pBridgeEntry->usNextBridgePtr = cOCT6100_INVALID_INDEX;
		pBridgeEntry->usPrevBridgePtr = cOCT6100_INVALID_INDEX;

		/* Set the global first bridge to this bridge. */
		f_pApiInstance->pSharedInfo->MiscVars.usFirstBridge = f_usBridgeIndex;
	}
	else	/* Insert this bridge at the head of the bridge list.*/
	{
		if ( f_pApiInstance->pSharedInfo->MiscVars.usFirstBridge == cOCT6100_INVALID_INDEX )
			return cOCT6100_ERR_FATAL_22;

		mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( f_pApiInstance->pSharedInfo, pTempBridgeEntry, f_pApiInstance->pSharedInfo->MiscVars.usFirstBridge )
	
		if ( pTempBridgeEntry->fReserved != TRUE )
			return cOCT6100_ERR_FATAL_23;

		/* Modify the old first entry. */
		pTempBridgeEntry->usPrevBridgePtr = f_usBridgeIndex;

		/* Modify current pointer. */
		pBridgeEntry->usPrevBridgePtr = cOCT6100_INVALID_INDEX;
		pBridgeEntry->usNextBridgePtr = f_pApiInstance->pSharedInfo->MiscVars.usFirstBridge;

		/* Set the new first bridge of the list. */
		f_pApiInstance->pSharedInfo->MiscVars.usFirstBridge = f_usBridgeIndex;
	}

	/* Form handle returned to user. */
	*f_pConfBridgeOpen->pulConfBridgeHndl = cOCT6100_HNDL_TAG_CONF_BRIDGE | (pBridgeEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | f_usBridgeIndex;

	/* Remember whether or not we are a flexible conference bridge. */
	pBridgeEntry->fFlexibleConferencing = (UINT8)( f_pConfBridgeOpen->fFlexibleConferencing & 0xFF );

	/* Finally, mark the conference bridge as opened. */
	pBridgeEntry->fReserved = TRUE;
	
	/* Increment the number of conference bridge opened. */
	f_pApiInstance->pSharedInfo->ChipStats.usNumberConfBridges++;
	f_pApiInstance->pSharedInfo->MiscVars.usNumBridgesOpened++;

	/*================================================================================*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ConfBridgeCloseSer

Description:    Closes a conference bridge. Note that no client must be present
				on the bridge for the bridge to be closed.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pConfBridgeClose		Pointer to conference bridge close structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ConfBridgeCloseSer
UINT32 Oct6100ConfBridgeCloseSer(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_CONF_BRIDGE_CLOSE			f_pConfBridgeClose )
{
	UINT16	usBridgeIndex;
	UINT32	ulResult;

	/* Verify that all the parameters given match the state of the API. */
	ulResult = Oct6100ApiAssertBridgeParams( f_pApiInstance, f_pConfBridgeClose, &usBridgeIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/* Release all resources associated to the conference bridge. */
	ulResult = Oct6100ApiReleaseBridgeResources( f_pApiInstance, usBridgeIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/* Invalidate the handle. */
	f_pConfBridgeClose->ulConfBridgeHndl = cOCT6100_INVALID_HANDLE;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiAssertBridgeParams

Description:    Checks the user's conference bridge close configuration for errors.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pConfBridgeClose		Pointer to conference bridge close structure.
f_pusBridgeIndex		Pointer to API instance conference bridge index.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiAssertBridgeParams
UINT32 Oct6100ApiAssertBridgeParams(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_CONF_BRIDGE_CLOSE			f_pConfBridgeClose,
				OUT		PUINT16								f_pusBridgeIndex )
{
	tPOCT6100_API_CONF_BRIDGE	pBridgeEntry;
	UINT32						ulEntryOpenCnt;

	/* Check the provided handle. */
	if ( (f_pConfBridgeClose->ulConfBridgeHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CONF_BRIDGE )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	*f_pusBridgeIndex = (UINT16)( f_pConfBridgeClose->ulConfBridgeHndl & cOCT6100_HNDL_INDEX_MASK );
	if ( *f_pusBridgeIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxConfBridges )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBridgeEntry, *f_pusBridgeIndex )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = (f_pConfBridgeClose->ulConfBridgeHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pBridgeEntry->fReserved != TRUE )
		return cOCT6100_ERR_CONF_BRIDGE_NOT_OPEN;
	if ( pBridgeEntry->usNumClients != 0 )
		return cOCT6100_ERR_CONF_BRIDGE_ACTIVE_DEPENDENCIES;
	if ( ulEntryOpenCnt != pBridgeEntry->byEntryOpenCnt )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseBridgeResources

Description:    Release all resources reserved for the conference bridge.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_usBridgeIndex			Allocated external memory block for the conference bridge.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseBridgeResources
UINT32 Oct6100ApiReleaseBridgeResources(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usBridgeIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_CONF_BRIDGE	pBridgeEntry;
	tPOCT6100_API_CONF_BRIDGE	pTempBridgeEntry;
	UINT32	ulResult;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Release the entry from the conference bridge list. */
	ulResult = Oct6100ApiReleaseBridgeEntry( f_pApiInstance, f_usBridgeIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return cOCT6100_ERR_FATAL_24;

	mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBridgeEntry, f_usBridgeIndex );

	/* Remove the bridge entry from the bridge list. */
	if ( pSharedInfo->MiscVars.usNumBridgesOpened == 1 )
	{
		/* This bridge was the only one opened. */
		pSharedInfo->MiscVars.usFirstBridge = cOCT6100_INVALID_INDEX;
	}
	else if ( pSharedInfo->MiscVars.usNumBridgesOpened > 1 )
	{
		/* There are more then one bridge open, must update the list. */
		if ( pBridgeEntry->usPrevBridgePtr != cOCT6100_INVALID_INDEX )
		{
			/* There is a valid entry before this bridge, let's update this entry. */
			mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( f_pApiInstance->pSharedInfo, pTempBridgeEntry, pBridgeEntry->usPrevBridgePtr );
			
			pTempBridgeEntry->usNextBridgePtr = pBridgeEntry->usNextBridgePtr;
		}

		if ( pBridgeEntry->usNextBridgePtr != cOCT6100_INVALID_INDEX )
		{
			/* There is a valid entry after this bridge, let's update this entry. */
			mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( f_pApiInstance->pSharedInfo, pTempBridgeEntry, pBridgeEntry->usNextBridgePtr );

			pTempBridgeEntry->usPrevBridgePtr = pBridgeEntry->usPrevBridgePtr;
		}

		if ( pSharedInfo->MiscVars.usFirstBridge == f_usBridgeIndex )
		{
			/* This entry was the first of the list, make the next one be the first now. */
			pSharedInfo->MiscVars.usFirstBridge = pBridgeEntry->usNextBridgePtr;
		}
	}
	else
	{
		/* Variable has become out of sync. */
		return cOCT6100_ERR_FATAL_25;
	}

	/*=============================================================*/
	/* Update the conference bridge's list entry. */

	/* Mark the bridge as closed. */
	pBridgeEntry->fFlexibleConferencing = FALSE;
	pBridgeEntry->fReserved = FALSE;
	pBridgeEntry->byEntryOpenCnt++;

	/* Decrement the number of conference bridges opened. */
	pSharedInfo->MiscVars.usNumBridgesOpened--;
	pSharedInfo->ChipStats.usNumberConfBridges--;

	/*=============================================================*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ConfBridgeChanAddSer

Description:    Adds an echo channel (participant) to a conference bridge. 

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pConfBridgeAdd		Pointer to conference bridge channel add structure.  

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ConfBridgeChanAddSer
UINT32 Oct6100ConfBridgeChanAddSer(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		tPOCT6100_CONF_BRIDGE_CHAN_ADD	f_pConfBridgeAdd )
{
	UINT16	usBridgeIndex;
	UINT16	usChanIndex;
	UINT16	usLoadEventIndex;
	UINT16	usSubStoreEventIndex;
	UINT16	usCopyEventIndex;
	UINT32	ulInputPort;
	UINT8	fFlexibleConfBridge;
	UINT32	ulListenerMaskIndex;
	UINT32	ulListenerMask;
	UINT16	usTapChanIndex;
	UINT16	usTapBridgeIndex;
	UINT8	fMute;
	UINT8	fTap;
	UINT32	ulResult;

	/* Check the validity of the channel and conference bridge given. */
	ulResult = Oct6100ApiCheckBridgeAddParams( 
									f_pApiInstance, 
									f_pConfBridgeAdd, 
									&usBridgeIndex, 
									&usChanIndex, 
									&fMute, 
									&ulInputPort, 
									&fFlexibleConfBridge, 
									&ulListenerMaskIndex, 
									&ulListenerMask,
									&fTap,
									&usTapChanIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
		return ulResult;

	/* Reserve all resources needed by the conference bridge. */
	ulResult = Oct6100ApiReserveBridgeAddResources( 
									f_pApiInstance, 
									usBridgeIndex, 
									usChanIndex, 
									ulInputPort, 
									fFlexibleConfBridge, 
									ulListenerMaskIndex, 
									ulListenerMask,
									fTap,
									&usLoadEventIndex, 
									&usSubStoreEventIndex, 
									&usCopyEventIndex,
									&usTapBridgeIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Reserve all resources needed by the conference bridge. */
	ulResult = Oct6100ApiBridgeEventAdd( 
									f_pApiInstance, 
									usBridgeIndex, 
									usChanIndex, 
									fFlexibleConfBridge, 
									usLoadEventIndex, 
									usSubStoreEventIndex, 
									usCopyEventIndex, 
									ulInputPort, 
									fMute, 
									ulListenerMaskIndex, 
									ulListenerMask,
									fTap,
									usTapBridgeIndex,
									usTapChanIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckBridgeAddParams

Description:	Check the validity of the channel and conference bridge given.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.
f_pConfBridgeAdd		Pointer to conference bridge channenl add structure.  
f_pusBridgeIndex		Extracted bridge index where this channel should be 
						added.
f_pusChannelIndex		Extracted channel index related to the channel handle
						to be added to the bridge.
f_pfMute				Whether to mute this channel in the bridge or not.
f_pulInputPort			Input port where the channel signal should be
						copied from.
f_pfFlexibleConfBridge	If this is a flexible conference bridge.
f_pulListenerMaskIndex	Index of the listener in this flexible conference bridge.
f_pulListenerMask		Mask of listeners in this flexible conference bridge.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckBridgeAddParams
UINT32 Oct6100ApiCheckBridgeAddParams(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_CONF_BRIDGE_CHAN_ADD		f_pConfBridgeAdd,
				OUT		PUINT16								f_pusBridgeIndex, 
				OUT		PUINT16								f_pusChannelIndex,
				OUT		PUINT8								f_pfMute,
				OUT		PUINT32								f_pulInputPort, 
				OUT		PUINT8								f_pfFlexibleConfBridge,
				OUT		PUINT32								f_pulListenerMaskIndex,
				OUT		PUINT32								f_pulListenerMask,
				OUT		PUINT8								f_pfTap,
				OUT		PUINT16								f_pusTapChannelIndex )
{
	tPOCT6100_API_CONF_BRIDGE	pBridgeEntry;
	tPOCT6100_API_CHANNEL		pEchoChanEntry;
	UINT32	ulEntryOpenCnt;
	UINT8	byTapChannelLaw;

	/* Check for errors. */
	if ( f_pApiInstance->pSharedInfo->ChipConfig.usMaxConfBridges == 0 )
		return cOCT6100_ERR_CONF_BRIDGE_DISABLED;

	if ( f_pApiInstance->pSharedInfo->ChipConfig.usMaxConfBridges == 1 &&
		 f_pApiInstance->pSharedInfo->ChipConfig.fEnableChannelRecording == TRUE )
		return cOCT6100_ERR_CONF_BRIDGE_DISABLED;

	if ( f_pConfBridgeAdd->ulConfBridgeHndl == cOCT6100_INVALID_HANDLE )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	if ( f_pConfBridgeAdd->ulChannelHndl == cOCT6100_INVALID_HANDLE )
		return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_ADD_INVALID_HANDLE;
	
	if( f_pConfBridgeAdd->ulInputPort != cOCT6100_CHANNEL_PORT_SOUT
		&& f_pConfBridgeAdd->ulInputPort != cOCT6100_CHANNEL_PORT_RIN )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_INPUT_PORT;

	if ( f_pConfBridgeAdd->fMute != TRUE && f_pConfBridgeAdd->fMute != FALSE )
		return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_ADD_MUTE;

	/*=====================================================================*/
	/* Check the conference bridge handle. */

	if ( (f_pConfBridgeAdd->ulConfBridgeHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CONF_BRIDGE )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	*f_pusBridgeIndex = (UINT16)( f_pConfBridgeAdd->ulConfBridgeHndl & cOCT6100_HNDL_INDEX_MASK );
	if ( *f_pusBridgeIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxConfBridges )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBridgeEntry, *f_pusBridgeIndex )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = (f_pConfBridgeAdd->ulConfBridgeHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pBridgeEntry->fReserved != TRUE )
		return cOCT6100_ERR_CONF_BRIDGE_NOT_OPEN;
	if ( ulEntryOpenCnt != pBridgeEntry->byEntryOpenCnt )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	/* When we a flexible conference bridge, more things need to be checked. */
	if ( pBridgeEntry->fFlexibleConferencing == TRUE )
	{
		/* Check if flexible conferencing has been activated. */
		if ( f_pApiInstance->pSharedInfo->ChipConfig.usMaxFlexibleConfParticipants == 0 )
			return cOCT6100_ERR_CONF_BRIDGE_FLEX_CONF_DISABLED;

		/* Check the number of clients on the bridge. */
		if ( pBridgeEntry->usNumClients >= cOCT6100_MAX_FLEX_CONF_PARTICIPANTS_PER_BRIDGE )
			return cOCT6100_ERR_CONF_BRIDGE_FLEX_CONF_PARTICIPANT_CNT;

		/* Check if the listener index in a flexible bridge is valid. */
		if ( f_pConfBridgeAdd->ulListenerMaskIndex == cOCT6100_INVALID_VALUE
			|| f_pConfBridgeAdd->ulListenerMaskIndex >= cOCT6100_MAX_FLEX_CONF_PARTICIPANTS_PER_BRIDGE )
			return cOCT6100_ERR_CONF_BRIDGE_FLEX_CONF_LISTENER_MASK_INDEX;
	}

	if ( f_pConfBridgeAdd->ulTappedChannelHndl != cOCT6100_INVALID_HANDLE )
	{
		if ( pBridgeEntry->fFlexibleConferencing == TRUE )
			return cOCT6100_ERR_CONF_BRIDGE_FLEX_CONF_TAP_NOT_SUPPORTED;
	}

	/*=====================================================================*/
	

	/*=====================================================================*/
	/* Check the channel handle. */

	if ( (f_pConfBridgeAdd->ulChannelHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CHANNEL )
		return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_ADD_INVALID_HANDLE;

	*f_pusChannelIndex = (UINT16)( f_pConfBridgeAdd->ulChannelHndl & cOCT6100_HNDL_INDEX_MASK );
	if ( *f_pusChannelIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxChannels )
		return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_ADD_INVALID_HANDLE;

	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pEchoChanEntry, *f_pusChannelIndex )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = (f_pConfBridgeAdd->ulChannelHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pEchoChanEntry->fReserved != TRUE )
		return cOCT6100_ERR_CONF_BRIDGE_NOT_OPEN;
	if ( ulEntryOpenCnt != pEchoChanEntry->byEntryOpenCnt )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;
	if ( pEchoChanEntry->usBridgeIndex != cOCT6100_INVALID_INDEX )
		return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_ALREADY_ON_BRIDGE;
	if ( pEchoChanEntry->fBiDirChannel == TRUE )
		return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_BIDIR;
	/* Law conversion is not allowed on a conference bridge. */
	if ( ( pEchoChanEntry->TdmConfig.usRinTimeslot != cOCT6100_UNASSIGNED )
		&& ( pEchoChanEntry->TdmConfig.usRoutTimeslot != cOCT6100_UNASSIGNED ) )
	{
		if ( pEchoChanEntry->TdmConfig.byRinPcmLaw != pEchoChanEntry->TdmConfig.byRoutPcmLaw )
			return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_LAW_CONVERSION;
	}
	if ( ( pEchoChanEntry->TdmConfig.usSinTimeslot != cOCT6100_UNASSIGNED )
		&& ( pEchoChanEntry->TdmConfig.usSoutTimeslot != cOCT6100_UNASSIGNED ) )
	{
		if ( pEchoChanEntry->TdmConfig.bySinPcmLaw != pEchoChanEntry->TdmConfig.bySoutPcmLaw )
			return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_LAW_CONVERSION;
	}
	if ( pEchoChanEntry->fRinRoutCodecActive == TRUE || pEchoChanEntry->fSinSoutCodecActive == TRUE )
		return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_ADD_CODEC_ACTIVE;
	if ( pEchoChanEntry->fEnableExtToneDetection == TRUE )
		return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_ADD_EXT_TONE_ENABLED;
	if ( pEchoChanEntry->usCopyEventCnt != 0x0 )
		return cOCT6100_ERR_CONF_BRIDGE_COPY_EVENTS;

	/* If the bridge is flexible, few more things need to be checked. */
	if ( pBridgeEntry->fFlexibleConferencing == TRUE )
	{
		tPOCT6100_SHARED_INFO	pSharedInfo;
		UINT16					usChannelIndex;
		UINT32					ulResult = cOCT6100_ERR_OK;

		/* Obtain local pointer to shared portion of instance. */
		pSharedInfo = f_pApiInstance->pSharedInfo;

		/* Check if the listener index has been used by another channel in the specified bridge. */
		for ( usChannelIndex = 0; ( usChannelIndex < pSharedInfo->ChipConfig.usMaxChannels ) && ( ulResult == cOCT6100_ERR_OK ) ; usChannelIndex++ )
		{
			mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChanEntry, usChannelIndex );
			
			/* Channel reserved? */
			if ( ( usChannelIndex != ( *f_pusChannelIndex ) ) && ( pEchoChanEntry->fReserved == TRUE ) )
			{
				/* On current bridge? */
				if ( pEchoChanEntry->usBridgeIndex == ( *f_pusBridgeIndex ) )
				{
					tPOCT6100_API_FLEX_CONF_PARTICIPANT pCurrentParticipant;

					mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pCurrentParticipant, pEchoChanEntry->usFlexConfParticipantIndex );

					/* Check if this participant has the same listener index. */
					if ( f_pConfBridgeAdd->ulListenerMaskIndex == pCurrentParticipant->ulListenerMaskIndex )
						return cOCT6100_ERR_CONF_BRIDGE_FLEX_CONF_LISTENER_INDEX_USED;
				}
			}
		}
	}

	if ( f_pConfBridgeAdd->ulTappedChannelHndl != cOCT6100_INVALID_HANDLE )
	{
		/* For internal logic, make sure the mute flag is set to false. */
		f_pConfBridgeAdd->fMute = FALSE;

		/* Force input port to Sout for logic below. */
		f_pConfBridgeAdd->ulInputPort = cOCT6100_CHANNEL_PORT_SOUT;

		/* Keep law to check for conversion. */
		/* Check if the same law. */
		byTapChannelLaw = pEchoChanEntry->TdmConfig.bySoutPcmLaw;

		/* Check the tap handle. */
		if ( (f_pConfBridgeAdd->ulTappedChannelHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CHANNEL )
			return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_ADD_INVALID_TAP_HANDLE;

		*f_pusTapChannelIndex = (UINT16)( f_pConfBridgeAdd->ulTappedChannelHndl & cOCT6100_HNDL_INDEX_MASK );
		if ( *f_pusTapChannelIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxChannels )
			return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_ADD_INVALID_TAP_HANDLE;

		mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pEchoChanEntry, *f_pusTapChannelIndex )

		/* Extract the entry open count from the provided handle. */
		ulEntryOpenCnt = (f_pConfBridgeAdd->ulTappedChannelHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

		/* Check for errors. */
		if ( pEchoChanEntry->fReserved != TRUE )
			return cOCT6100_ERR_CONF_BRIDGE_NOT_OPEN;
		if ( ulEntryOpenCnt != pEchoChanEntry->byEntryOpenCnt )
			return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_ADD_INVALID_TAP_HANDLE;
		if ( pEchoChanEntry->usBridgeIndex == cOCT6100_INVALID_INDEX )
			return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_TAP_NOT_ON_BRIDGE;
		if ( pEchoChanEntry->usBridgeIndex != *f_pusBridgeIndex )
			return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_TAP_NOT_ON_SAME_BRIDGE;

		/* We can only tap a channel added on the Sout port. */
		if ( pEchoChanEntry->usSinCopyEventIndex == cOCT6100_INVALID_INDEX )
			return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_ADD_TAP_SOUT_ONLY;

		/* Check if already tapped. */
		if ( pEchoChanEntry->fBeingTapped == TRUE )
			return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_ADD_ALREADY_TAPPED;
	}

	/*=====================================================================*/

	/* Return the tap flag. */
	if ( f_pConfBridgeAdd->ulTappedChannelHndl != cOCT6100_INVALID_HANDLE )
	{
		*f_pfTap = TRUE;
	}
	else
	{
		*f_pfTap = FALSE;
	}

	/* Return the mute config specified. */
	*f_pfMute = (UINT8)( f_pConfBridgeAdd->fMute & 0xFF );

	/* Return the input port specified. */
	*f_pulInputPort = f_pConfBridgeAdd->ulInputPort;

	/* Return whether we are in the flexible conference bridge case. */
	*f_pfFlexibleConfBridge = pBridgeEntry->fFlexibleConferencing;

	/* Return the listener mask index as specified. */
	*f_pulListenerMaskIndex = f_pConfBridgeAdd->ulListenerMaskIndex;

	/* Return the listener mask as specified. */
	*f_pulListenerMask = f_pConfBridgeAdd->ulListenerMask;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReserveBridgeAddResources

Description:    Reserves all resources needed for the addition of a channel to 
				the conference bridge.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.
f_usBridgeIndex				Bridge index of the bridge where this channel is added.
f_usChanIndex				Channel index of the channel to be added to the bridge.
f_ulInputPort				Input port where to copy samples from.
f_fFlexibleConfBridge		If this is a flexible conference bridge.
f_ulListenerMaskIndex		Index of the listener in this flexible conference bridge.
f_ulListenerMask			Mask of listeners in this flexible conference bridge.
f_pusLoadEventIndex			Load event index within the API's list of mixer event.
f_pusSubStoreEventIndex		Sub-Store event index within the API's list of mixer event.
f_pusCopyEventIndex			Copy event index within the API's list of mixer event.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReserveBridgeAddResources
UINT32 Oct6100ApiReserveBridgeAddResources(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usBridgeIndex,
				IN		UINT16							f_usChanIndex,
				IN		UINT32							f_ulInputPort,
				IN		UINT8							f_fFlexibleConfBridge,
				IN		UINT32							f_ulListenerMaskIndex,
				IN		UINT32							f_ulListenerMask,
				IN		UINT8							f_fTap,
				OUT		PUINT16							f_pusLoadEventIndex,
				OUT		PUINT16							f_pusSubStoreEventIndex,
				OUT		PUINT16							f_pusCopyEventIndex,
				OUT		PUINT16							f_pusTapBridgeIndex )
{
	tPOCT6100_API_CHANNEL			pChanEntry;
	tPOCT6100_SHARED_INFO			pSharedInfo;
	tPOCT6100_API_CHANNEL			pTempEchoChanEntry;
	UINT32	ulResult;
	UINT32	ulTempVar;
	UINT16	usChannelIndex;
	BOOL	fLoadEventReserved = FALSE;
	BOOL	fStoreEventReserved = FALSE;
	BOOL	fCopyEventReserved = FALSE;
	BOOL	fExtraSinTsiReserved = FALSE;
	BOOL	fExtraRinTsiReserved = FALSE;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Get a pointer to the channel's list entry. */
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pChanEntry, f_usChanIndex )

	/* Resources must be reserved according to the type of bridge we are adding to. */
	if ( f_fFlexibleConfBridge == TRUE )
	{
		tPOCT6100_API_FLEX_CONF_PARTICIPANT pNewParticipant;
		tPOCT6100_API_FLEX_CONF_PARTICIPANT pCurrentParticipant;

		/*========================================================================*/
		/* If we are in the flexible conferencing case, things are a little       */
		/* different.  We create a mixer for every participant instead of the     */
		/* usual same mixer for everyone.  For example, if we have 3 participants */
		/* of type client - agent - coach, we build the mixers as follows:        */
		/*                                                                        */
		/*   Client:            - Load Agent                                      */
		/*                      - Store                                           */
		/*                                                                        */
		/*   Agent:             - Load Client                                     */
		/*                      - Accumulate Coach                                */
		/*                      - Store                                           */
		/*                                                                        */
		/*   Coach:             - Load Client                                     */
		/*                      - Accumulate Agent                                */
		/*                      - Store                                           */
		/*                                                                        */
		/*========================================================================*/

		/* First reserve a flexible conferencing participant entry. */
		ulResult = Oct6100ApiReserveFlexConfParticipantEntry( f_pApiInstance, &pChanEntry->usFlexConfParticipantIndex );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pNewParticipant, pChanEntry->usFlexConfParticipantIndex );

		/* Reserve an entry for the store event in the mixer memory. */
		ulResult = Oct6100ApiReserveMixerEventEntry( f_pApiInstance, f_pusSubStoreEventIndex );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			/* If using the SOUT port, we must copy this entry */
			if( f_ulInputPort == cOCT6100_CHANNEL_PORT_SOUT )
			{
				/* Reserve an entry for the copy event in the Mixer memory. */
				ulResult = Oct6100ApiReserveMixerEventEntry( f_pApiInstance, f_pusCopyEventIndex );
				if ( ulResult == cOCT6100_ERR_OK )
				{
					fCopyEventReserved = TRUE;

					/* Reserve a SIN copy entry if none were reserved before.*/
					if ( pChanEntry->usExtraSinTsiMemIndex == cOCT6100_INVALID_INDEX )
					{
						/* Reserve an entry for the extra tsi chariot memory. */
						ulResult = Oct6100ApiReserveTsiMemEntry(	f_pApiInstance, 
																	&pChanEntry->usExtraSinTsiMemIndex );
						if ( ulResult == cOCT6100_ERR_OK )
							fExtraSinTsiReserved = TRUE;
					}
				}
			}
			else /* if( f_ulInputPort == cOCT6100_CHANNEL_PORT_RIN ) */
			{
				/* Reserve a RIN copy entry if none were reserved before.*/
				if ( pChanEntry->usExtraRinTsiMemIndex == cOCT6100_INVALID_INDEX )
				{
					/* Reserve an entry for the extra tsi chariot memory. */
					ulResult = Oct6100ApiReserveTsiMemEntry(	f_pApiInstance, 
																&pChanEntry->usExtraRinTsiMemIndex );
					if ( ulResult == cOCT6100_ERR_OK )
						fExtraRinTsiReserved = TRUE;
				}				
			}

			/* Must travel all clients of this conference and reserve a load or accumulate event for */
			/* all participants which can hear us. */

			/* Search through the list of API channel entry for the ones on to this bridge.*/
			for ( usChannelIndex = 0; ( usChannelIndex < pSharedInfo->ChipConfig.usMaxChannels ) && ( ulResult == cOCT6100_ERR_OK ) ; usChannelIndex++ )
			{
				mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pTempEchoChanEntry, usChannelIndex );
				
				/* Channel reserved? */
				if ( ( usChannelIndex != f_usChanIndex ) && ( pTempEchoChanEntry->fReserved == TRUE ) )
				{
					/* On current bridge? */
					if ( pTempEchoChanEntry->usBridgeIndex == f_usBridgeIndex )
					{
						mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pCurrentParticipant, pTempEchoChanEntry->usFlexConfParticipantIndex );

						/* Check if we can hear this participant. */
						if ( ( f_ulListenerMask & ( 0x1 << pCurrentParticipant->ulListenerMaskIndex ) ) == 0x0 )
						{
							/* Must reserve a load or accumulate entry mixer event here! */
							ulResult = Oct6100ApiReserveMixerEventEntry( f_pApiInstance, &pNewParticipant->ausLoadOrAccumulateEventIndex[ pCurrentParticipant->ulListenerMaskIndex ] );
							if ( ulResult != cOCT6100_ERR_OK )
							{
								/* Most probably, the hardware is out of mixer events. */
								break;
							}
						}

						/* Check if this participant can hear us. */
						if ( ( pCurrentParticipant->ulListenerMask & ( 0x1 << f_ulListenerMaskIndex ) ) == 0x0 )
						{
							/* Must reserve a load or accumulate entry mixer event here! */
							ulResult = Oct6100ApiReserveMixerEventEntry( f_pApiInstance, &pCurrentParticipant->ausLoadOrAccumulateEventIndex[ f_ulListenerMaskIndex ] );
							if ( ulResult != cOCT6100_ERR_OK )
							{
								/* Most probably, the hardware is out of mixer events. */
								break;
							}
						}
					}
				}
			}

			/* If an error is returned, make sure everything is cleaned up properly. */
			if ( ulResult != cOCT6100_ERR_OK )
			{
				/* Release the flexible conferencing participant entry. */
				ulTempVar = Oct6100ApiReleaseFlexConfParticipantEntry( f_pApiInstance, pChanEntry->usFlexConfParticipantIndex );
				if ( ulTempVar != cOCT6100_ERR_OK )
					return ulTempVar;
				
				pChanEntry->usFlexConfParticipantIndex = cOCT6100_INVALID_INDEX;

				/* Release the substore event in the mixer memory. */
				ulTempVar = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, *f_pusSubStoreEventIndex );
				if ( ulTempVar != cOCT6100_ERR_OK )
					return ulTempVar;

				if ( fCopyEventReserved == TRUE )
				{
					/* Release the copy event in the mixer memory. */
					ulTempVar = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, *f_pusCopyEventIndex );
					if ( ulTempVar != cOCT6100_ERR_OK )
						return ulTempVar;
				}

				if ( fExtraSinTsiReserved == TRUE )
				{
					/* Release the extra Sin TSI in TSI memory. */
					ulTempVar = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance, pChanEntry->usExtraSinTsiMemIndex );
					if ( ulTempVar != cOCT6100_ERR_OK )
						return ulTempVar;

					pChanEntry->usExtraSinTsiMemIndex = cOCT6100_INVALID_INDEX;
				}

				if ( fExtraRinTsiReserved == TRUE )
				{
					/* Release the extra Rin TSI in TSI memory. */
					ulTempVar = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance, pChanEntry->usExtraRinTsiMemIndex );
					if ( ulTempVar != cOCT6100_ERR_OK )
						return ulTempVar;

					pChanEntry->usExtraRinTsiMemIndex = cOCT6100_INVALID_INDEX;
				}
				
				/* Search through the list of API channel entry for the ones on to this bridge. */
				for ( usChannelIndex = 0; usChannelIndex < pSharedInfo->ChipConfig.usMaxChannels; usChannelIndex++ )
				{
					mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pTempEchoChanEntry, usChannelIndex );
					
					/* Channel reserved? */
					if ( ( usChannelIndex != f_usChanIndex ) && ( pTempEchoChanEntry->fReserved == TRUE ) )
					{
						/* On current bridge? */
						if ( pTempEchoChanEntry->usBridgeIndex == f_usBridgeIndex )
						{
							mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pCurrentParticipant, pTempEchoChanEntry->usFlexConfParticipantIndex );

							/* Check if we can hear this participant. */
							if ( ( f_ulListenerMask & ( 0x1 << pCurrentParticipant->ulListenerMaskIndex ) ) == 0x0 )
							{
								/* If the load or event entry in the mixer memory was reserved. */
								if ( pNewParticipant->ausLoadOrAccumulateEventIndex[ pCurrentParticipant->ulListenerMaskIndex ] != cOCT6100_INVALID_INDEX )
								{
									/* Must release the load or accumulate entry mixer event. */
									ulTempVar = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pNewParticipant->ausLoadOrAccumulateEventIndex[ pCurrentParticipant->ulListenerMaskIndex ] );
									if ( ulTempVar != cOCT6100_ERR_OK )
										return ulTempVar;

									pNewParticipant->ausLoadOrAccumulateEventIndex[ pCurrentParticipant->ulListenerMaskIndex ] = cOCT6100_INVALID_INDEX;
								}
							}

							/* Check this participant can hear us. */
							if ( ( pCurrentParticipant->ulListenerMask & ( 0x1 << f_ulListenerMaskIndex ) ) == 0x0 )
							{
								/* If the load or event entry in the mixer memory was reserved. */
								if ( pCurrentParticipant->ausLoadOrAccumulateEventIndex[ f_ulListenerMaskIndex ] != cOCT6100_INVALID_INDEX )
								{
									/* Must release the load or accumulate entry mixer event. */
									ulTempVar = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pCurrentParticipant->ausLoadOrAccumulateEventIndex[ f_ulListenerMaskIndex ] );
									if ( ulTempVar != cOCT6100_ERR_OK )
										return ulTempVar;

									pCurrentParticipant->ausLoadOrAccumulateEventIndex[ f_ulListenerMaskIndex ] = cOCT6100_INVALID_INDEX;
								}
							}
						}
					}
				}

				return ulResult;
			}
		}
		else /* if ( ulResult != cOCT6100_ERR_OK ) */
		{
			ulTempVar = Oct6100ApiReleaseFlexConfParticipantEntry( f_pApiInstance, pChanEntry->usFlexConfParticipantIndex );
			if ( ulTempVar != cOCT6100_ERR_OK )
				return ulTempVar;

			pChanEntry->usFlexConfParticipantIndex = cOCT6100_INVALID_INDEX;
			
			/* Return the error code to the user.  The mixer event allocation failed. */
			return ulResult;
		}

		/*=======================================================================*/
	}
	else /* if ( f_fFlexibleConfBridge == FALSE ) */
	{
		/*=======================================================================*/
		/* Normal conferencing. */		

		/* Reserve an entry for the load event in the mixer memory. */
		ulResult = Oct6100ApiReserveMixerEventEntry( f_pApiInstance, f_pusLoadEventIndex );
		if ( ulResult == cOCT6100_ERR_OK )
		{
			fLoadEventReserved = TRUE;
			/* Reserve an entry for the substract and store event in the mixer memory. */
			ulResult = Oct6100ApiReserveMixerEventEntry( f_pApiInstance, f_pusSubStoreEventIndex );
			if ( ulResult == cOCT6100_ERR_OK )
			{
				fStoreEventReserved = TRUE;

				/* If using the SOUT port, we must copy this entry */
				if( f_ulInputPort == cOCT6100_CHANNEL_PORT_SOUT )
				{
					/* Reserve an entry for the copy event in the mixer memory. */
					ulResult = Oct6100ApiReserveMixerEventEntry( f_pApiInstance, f_pusCopyEventIndex );
					if ( ulResult == cOCT6100_ERR_OK )
					{
						fCopyEventReserved = TRUE;

						/* Reserve a SIN copy entry if none were reserved before. */
						if ( pChanEntry->usExtraSinTsiMemIndex == cOCT6100_INVALID_INDEX )
						{
							/* Reserve an entry for the extra tsi chariot memory. */
							ulResult = Oct6100ApiReserveTsiMemEntry(	f_pApiInstance, 
																		&pChanEntry->usExtraSinTsiMemIndex );

							if ( ulResult == cOCT6100_ERR_OK )
								fExtraSinTsiReserved = TRUE;
						}
					}
				}
			}
		}

		if ( ( ulResult == cOCT6100_ERR_OK ) && ( f_fTap == TRUE ) )
		{
			/* Reserve a "tap" bridge. */
			tOCT6100_CONF_BRIDGE_OPEN	ConfBridgeOpen;
			UINT32						ulTapBridgeHndl = 0;

			Oct6100ConfBridgeOpenDef( &ConfBridgeOpen );

			ConfBridgeOpen.pulConfBridgeHndl = &ulTapBridgeHndl;

			ulResult = Oct6100ConfBridgeOpenSer( f_pApiInstance, &ConfBridgeOpen );

			*f_pusTapBridgeIndex = (UINT16)( ulTapBridgeHndl & cOCT6100_HNDL_INDEX_MASK );
		}

		if ( ulResult != cOCT6100_ERR_OK )
		{
			if ( fLoadEventReserved == TRUE )
			{
				ulTempVar = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, *f_pusLoadEventIndex );
				if ( ulTempVar != cOCT6100_ERR_OK )
					return ulTempVar;
			}
		
			if ( fStoreEventReserved == TRUE )
			{
				ulTempVar = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, *f_pusSubStoreEventIndex );
				if ( ulTempVar != cOCT6100_ERR_OK )
					return ulTempVar;
			}
			
			if ( fCopyEventReserved == TRUE )
			{
				ulTempVar = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, *f_pusCopyEventIndex );
				if ( ulTempVar != cOCT6100_ERR_OK )
					return ulTempVar;
			}

			if ( fExtraSinTsiReserved == TRUE )
			{
				ulTempVar = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance, pChanEntry->usExtraSinTsiMemIndex );
				if ( ulTempVar != cOCT6100_ERR_OK )
					return ulTempVar;

				pChanEntry->usExtraSinTsiMemIndex = cOCT6100_INVALID_INDEX;
			}

			return ulResult;
		}

		/*=======================================================================*/
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiBridgeEventAdd

Description:    Add the event into the global event list of the chip and update
				the bridge and channel structures.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to 
							keep the present state of the chip and all its 
							resources.
f_usBridgeIndex				Index of the current bridge in the API list.
f_usChanIndex				Index of the current channel in the API list.
f_fFlexibleConfBridge		If this is a flexible conference bridge.
f_usLoadEventIndex			Allocated entry for the Load event of the 
							channel.
f_usSubStoreEventIndex		Allocated entry for the substract and store 
							event of the channel.
f_usCopyEventIndex			Allocated entry for the copy event of the 
							channel.
f_ulInputPort				Input port where to copy samples from.
f_fMute						Mute flag indicating if the channel is added in 
							a mute state.
f_ulListenerMaskIndex		Index of the listener in this flexible conference bridge.
f_ulListenerMask			Mask of listeners in this flexible conference bridge.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiBridgeEventAdd
UINT32 Oct6100ApiBridgeEventAdd(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usBridgeIndex, 
				IN		UINT16							f_usChanIndex,
				IN		UINT8							f_fFlexibleConfBridge,
				IN		UINT16							f_usLoadEventIndex,
				IN		UINT16							f_usSubStoreEventIndex,
				IN		UINT16							f_usCopyEventIndex,
				IN		UINT32							f_ulInputPort,
				IN		UINT8							f_fMute, 
				IN		UINT32							f_ulListenerMaskIndex,
				IN		UINT32							f_ulListenerMask,
				IN		UINT8							f_fTap,
				IN		UINT16							f_usTapBridgeIndex,
				IN		UINT16							f_usTapChanIndex )
{
	tPOCT6100_API_CONF_BRIDGE		pBridgeEntry;

	tPOCT6100_API_MIXER_EVENT		pLoadEventEntry;
	tPOCT6100_API_MIXER_EVENT		pSubStoreEventEntry;
	tPOCT6100_API_MIXER_EVENT		pTempEntry;

	tPOCT6100_API_CHANNEL			pEchoChanEntry;
	tPOCT6100_API_CHANNEL			pTapEchoChanEntry = NULL;
	tPOCT6100_API_CHANNEL			pTempEchoChanEntry;

	tPOCT6100_SHARED_INFO			pSharedInfo;
	tOCT6100_WRITE_PARAMS			WriteParams;

	UINT32	ulResult;
	UINT16	usChannelIndex;
	UINT16	usLastSubStoreEventIndex;
	UINT16	usLastLoadEventIndex;

	BOOL	fAddSinCopy = FALSE;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;
	
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	/* Get the bridge and channel entries of interest. */
	if ( f_fTap == FALSE )
	{
		mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( pSharedInfo, pBridgeEntry, f_usBridgeIndex );
	}
	else
	{
		mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( pSharedInfo, pBridgeEntry, f_usTapBridgeIndex );
		mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pTapEchoChanEntry, f_usTapChanIndex );
	}
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChanEntry, f_usChanIndex );

	if ( f_fFlexibleConfBridge == TRUE )
	{
		tPOCT6100_API_FLEX_CONF_PARTICIPANT pNewParticipant;
		tPOCT6100_API_FLEX_CONF_PARTICIPANT pCurrentParticipant;

		mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pNewParticipant, pEchoChanEntry->usFlexConfParticipantIndex );

		/* Search through the list of API channel entry for the ones onto this bridge. */
		for ( usChannelIndex = 0; usChannelIndex < pSharedInfo->ChipConfig.usMaxChannels; usChannelIndex++ )
		{
			mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pTempEchoChanEntry, usChannelIndex );
			
			/* Channel reserved? */
			if ( ( usChannelIndex != f_usChanIndex ) && ( pTempEchoChanEntry->fReserved == TRUE ) )
			{
				/* On current bridge? */
				if ( pTempEchoChanEntry->usBridgeIndex == f_usBridgeIndex )
				{
					mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pCurrentParticipant, pTempEchoChanEntry->usFlexConfParticipantIndex );

					/* Check if we can hear this participant. */
					if ( ( pTempEchoChanEntry->fMute == FALSE ) && ( ( f_ulListenerMask & ( 0x1 << pCurrentParticipant->ulListenerMaskIndex ) ) == 0x0 ) )
					{
						/* First create/update the current channel's mixer. */
						ulResult = Oct6100ApiBridgeAddParticipantToChannel(
													f_pApiInstance,
													f_usBridgeIndex,
													usChannelIndex,
													f_usChanIndex,
													pNewParticipant->ausLoadOrAccumulateEventIndex[ pCurrentParticipant->ulListenerMaskIndex ],
													f_usSubStoreEventIndex,
													f_usCopyEventIndex,
													pCurrentParticipant->ulInputPort,
													f_ulInputPort );
						if ( ulResult != cOCT6100_ERR_OK )
							return ulResult;
					}

					/* Check if this participant can hear us. */
					if ( ( f_fMute == FALSE ) && ( ( pCurrentParticipant->ulListenerMask & ( 0x1 << f_ulListenerMaskIndex ) ) == 0x0 ) )
					{
						/* Then create/update this channel's mixer. */
						ulResult = Oct6100ApiBridgeAddParticipantToChannel(
													f_pApiInstance,
													f_usBridgeIndex,
													f_usChanIndex,
													usChannelIndex,
													pCurrentParticipant->ausLoadOrAccumulateEventIndex[ f_ulListenerMaskIndex ],
													pTempEchoChanEntry->usSubStoreEventIndex,
													pTempEchoChanEntry->usSinCopyEventIndex,
													f_ulInputPort,
													pCurrentParticipant->ulInputPort );
						if ( ulResult != cOCT6100_ERR_OK )
							return ulResult;

						/* Check if the Rin silence event can be cleared now that the */
						/* channel has been added to a conference. */
						if ( ( pCurrentParticipant->fFlexibleMixerCreated == TRUE )
							&& ( pTempEchoChanEntry->usRinSilenceEventIndex != cOCT6100_INVALID_INDEX ) )
						{
							/* Remove the event from the list. */
							ulResult = Oct6100ApiMixerEventRemove(	f_pApiInstance,
																	pTempEchoChanEntry->usRinSilenceEventIndex,
																	cOCT6100_EVENT_TYPE_SOUT_COPY );
							if ( ulResult != cOCT6100_ERR_OK )
								return ulResult;

							ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pTempEchoChanEntry->usRinSilenceEventIndex );
							if ( ulResult != cOCT6100_ERR_OK  )
								return cOCT6100_ERR_FATAL_DF;

							pTempEchoChanEntry->usRinSilenceEventIndex = cOCT6100_INVALID_INDEX;
						}
					}
				}
			}
		}

		/* Check if the mixer for the destination channel has been created. */
		if ( pNewParticipant->fFlexibleMixerCreated == FALSE )
		{
			/* Save store event index that might be used for next channel added. */
			pEchoChanEntry->usSubStoreEventIndex = f_usSubStoreEventIndex;
		}
		else
		{
			/* Check if the Rin silence event can be cleared now that the */
			/* channel has been added to a conference. */
			if ( pEchoChanEntry->usRinSilenceEventIndex != cOCT6100_INVALID_INDEX )
			{
				/* Remove the event from the list.*/
				ulResult = Oct6100ApiMixerEventRemove(	f_pApiInstance,
														pEchoChanEntry->usRinSilenceEventIndex,
														cOCT6100_EVENT_TYPE_SOUT_COPY );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;

				ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pEchoChanEntry->usRinSilenceEventIndex );
				if ( ulResult != cOCT6100_ERR_OK )
					return cOCT6100_ERR_FATAL_DF;

				pEchoChanEntry->usRinSilenceEventIndex = cOCT6100_INVALID_INDEX;
			}
		}

		pNewParticipant->ulListenerMaskIndex = f_ulListenerMaskIndex;
		pNewParticipant->ulListenerMask = f_ulListenerMask;

		/* Remember this channel's input port. */
		pNewParticipant->ulInputPort = f_ulInputPort;

		/*=======================================================================*/
	}
	else /* if ( f_fFlexibleConfBridge == FALSE ) */
	{
		/* Configure the SIN copy mixer entry and memory - if using the SOUT port. */
		if ( ( f_ulInputPort == cOCT6100_CHANNEL_PORT_SOUT ) && ( f_fTap == FALSE ) )
		{
			if ( pEchoChanEntry->usSinTsstIndex != cOCT6100_INVALID_INDEX )
			{
				ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
																  pEchoChanEntry->usSinTsstIndex,
																  pEchoChanEntry->usExtraSinTsiMemIndex,
																  pEchoChanEntry->TdmConfig.bySinPcmLaw );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;
			}

			/* If the silence TSI is loaded on this port, update with the extra sin TSI. */
			if ( pEchoChanEntry->usSinSilenceEventIndex != cOCT6100_INVALID_INDEX )
			{
				WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pEchoChanEntry->usSinSilenceEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );

				WriteParams.ulWriteAddress += 2;
				WriteParams.usWriteData = pEchoChanEntry->usExtraSinTsiMemIndex;

				mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;
			}
		}

		mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pLoadEventEntry, f_usLoadEventIndex );
		mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pSubStoreEventEntry, f_usSubStoreEventIndex );

		/*=======================================================================*/
		/* Program the Load event.*/
		WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usLoadEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
		
		if ( ( f_fMute == FALSE ) || ( f_fTap == TRUE ) )
		{
			if ( pBridgeEntry->usLoadIndex != cOCT6100_INVALID_INDEX )
			{
				WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_ACCUMULATE;

				/* Set the event type. */
				pLoadEventEntry->usEventType = cOCT6100_MIXER_CONTROL_MEM_ACCUMULATE;

				if ( f_fTap == TRUE )
					return cOCT6100_ERR_FATAL_D1;
			}
			else /* pBridgeEntry->usLoadIndex == cOCT6100_INVALID_INDEX */
			{
				WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_LOAD;

				/* Modify the bridge entry to show store the new load index.*/
				pBridgeEntry->usLoadIndex = f_usLoadEventIndex;

				/* Set the event type.*/
				pLoadEventEntry->usEventType = cOCT6100_MIXER_CONTROL_MEM_LOAD;
			}

			/* Select the TSI memory index according to the source port. */
			if ( f_ulInputPort == cOCT6100_CHANNEL_PORT_SOUT )
			{
				if ( f_fTap == FALSE )
				{
					WriteParams.usWriteData |= pEchoChanEntry->usSinSoutTsiMemIndex;
				}
				else
				{
					tPOCT6100_API_CONF_BRIDGE	pTempBridgeEntry;
					UINT16						usTempWriteData;
					UINT32						ulTempWriteAddress;

					/* Save temp write data before trying to clear the Rin TSST. */
					usTempWriteData = WriteParams.usWriteData;
					ulTempWriteAddress = WriteParams.ulWriteAddress;
					
					/* Clear the Rin TSST if used. */
					if ( pTapEchoChanEntry->usRinTsstIndex != cOCT6100_INVALID_INDEX )
					{
						/* Deactivate the TSST entry.*/
						WriteParams.ulWriteAddress = cOCT6100_TSST_CONTROL_MEM_BASE + ( pTapEchoChanEntry->usRinTsstIndex * cOCT6100_TSST_CONTROL_MEM_ENTRY_SIZE );
						WriteParams.usWriteData  = 0x0000;

						mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
						if ( ulResult != cOCT6100_ERR_OK )
							return ulResult;
					}

					/* Reassign write data that might have been cleared by write above. */
					WriteParams.usWriteData = usTempWriteData;
					WriteParams.ulWriteAddress = ulTempWriteAddress;
					WriteParams.usWriteData |= pTapEchoChanEntry->usRinRoutTsiMemIndex;

					/* Remember that this channel is being tapped by us. */
					pTapEchoChanEntry->fBeingTapped = TRUE;
					pTapEchoChanEntry->usTapChanIndex = f_usChanIndex;

					mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( pSharedInfo, pTempBridgeEntry, f_usBridgeIndex );

					pTempBridgeEntry->usNumTappedClients++;
				}
			}
			else /* if ( f_ulInputPort == cOCT6100_CHANNEL_PORT_RIN ) */
			{
				WriteParams.usWriteData |= pEchoChanEntry->usRinRoutTsiMemIndex;
			}
		}
		else /* f_fMute == TRUE */
		{
			/* Do not load the sample if the channel is muted. */
			if ( pBridgeEntry->usNumClients == 0 )
			{
				/* If the participant to be added is muted, and it would cause the conference to */
				/* be completely muted, load the silence TSI. */
				WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_LOAD;
				WriteParams.usWriteData |= 1534; /* TSI index 1534 reserved for silence */

				/* We know for sure that the load of the bridge will be the silence one. */
				pBridgeEntry->usSilenceLoadEventPtr = f_usLoadEventIndex;
			}
			else
			{
				/* Do nothing! */
				WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_NO_OP;
			}
			
			/* Set the event type. */
			pLoadEventEntry->usEventType = cOCT6100_MIXER_CONTROL_MEM_NO_OP;
		}
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/*=======================================================================*/
		
		/*=======================================================================*/
		/* Program the Substract and store event.*/
		WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usSubStoreEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
		
		if ( ( f_fMute == FALSE ) && ( f_fTap == FALSE ) )
		{
			WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_SUB_STORE;
			/* Select the TSI memory index and PCM law according to the source port. */
			if ( f_ulInputPort == cOCT6100_CHANNEL_PORT_SOUT )
			{
				WriteParams.usWriteData |= pEchoChanEntry->TdmConfig.bySoutPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;
				WriteParams.usWriteData |= pEchoChanEntry->usSinSoutTsiMemIndex;
			}
			else /* if ( f_ulInputPort == cOCT6100_CHANNEL_PORT_RIN ) */
			{
				WriteParams.usWriteData |= pEchoChanEntry->TdmConfig.byRinPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;
				WriteParams.usWriteData |= pEchoChanEntry->usRinRoutTsiMemIndex;
			}
			
			/* Set the event type. */
			pSubStoreEventEntry->usEventType = cOCT6100_MIXER_CONTROL_MEM_SUB_STORE;
		}
		else /* f_fMute == TRUE */
		{
			/* Do not substore the sample if the channel is muted. */
			WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_STORE;

			/* Select the PCM law according to the source port. */
			if ( f_ulInputPort == cOCT6100_CHANNEL_PORT_SOUT )
			{
				WriteParams.usWriteData |= pEchoChanEntry->TdmConfig.bySoutPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;
			}
			else /* if ( f_ulInputPort == cOCT6100_CHANNEL_PORT_RIN ) */
			{
				WriteParams.usWriteData |= pEchoChanEntry->TdmConfig.byRinPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;
			}
			/* Set the event type. */
			pSubStoreEventEntry->usEventType = cOCT6100_MIXER_CONTROL_MEM_STORE;
		}

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress += 2;
		WriteParams.usWriteData = pEchoChanEntry->usRinRoutTsiMemIndex;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/*=======================================================================*/


		/*=======================================================================*/
		/* Program the Copy event - if using the SOUT port */
		if ( ( f_ulInputPort == cOCT6100_CHANNEL_PORT_SOUT ) && ( f_fTap == FALSE ) )
		{
			WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usCopyEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
			
			WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_COPY;
			WriteParams.usWriteData |= pEchoChanEntry->usExtraSinTsiMemIndex;
			WriteParams.usWriteData |= pEchoChanEntry->TdmConfig.bySinPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;

			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK  )
				return ulResult;

			WriteParams.ulWriteAddress += 2;
			WriteParams.usWriteData = pEchoChanEntry->usSinSoutTsiMemIndex;

			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK  )
				return ulResult;

			/* Set add copy event flag. */
			fAddSinCopy = TRUE;

			/* For sure. */
			pEchoChanEntry->fCopyEventCreated = TRUE;
		}
		else if ( f_fTap == TRUE )
		{
			/* Accumulate the tapped channel's voice instead of building a copy event. */
			
			WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usCopyEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
			
			WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_ACCUMULATE;
			WriteParams.usWriteData |= pTapEchoChanEntry->usSinSoutTsiMemIndex;

			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Link to next operation. */
			WriteParams.ulWriteAddress += 4;
			WriteParams.usWriteData = f_usSubStoreEventIndex;

			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Update the software model. */
			mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEntry, f_usCopyEventIndex );

			pTempEntry->usSourceChanIndex = f_usTapChanIndex;
			pTempEntry->usEventType = cOCT6100_MIXER_CONTROL_MEM_ACCUMULATE;
			pTempEntry->usNextEventPtr = f_usSubStoreEventIndex;
			pTempEntry->usDestinationChanIndex = cOCT6100_INVALID_INDEX;
			pTempEntry->fReserved = TRUE;
		}
		/*=======================================================================*/

		/*=======================================================================*/
		/* Now insert the event into the list.*/
		if ( pBridgeEntry->usNumClients == 0 )
		{
			/* This is the first entry for this bridge. Insert the two events at the head
			   of the list just after the last sub-store event.*/
			if ( f_fTap == FALSE )
			{
				ulResult = Oct6100ApiGetPrevLastSubStoreEvent( f_pApiInstance, f_usBridgeIndex, pBridgeEntry->usFirstLoadEventPtr, &usLastSubStoreEventIndex );
				if ( ulResult != cOCT6100_ERR_OK )
				{
					if ( ulResult == cOCT6100_ERR_CONF_MIXER_EVENT_NOT_FOUND )
					{
						if ( pSharedInfo->MixerInfo.usLastSoutCopyEventPtr == cOCT6100_INVALID_INDEX )
						{
							usLastSubStoreEventIndex = cOCT6100_MIXER_HEAD_NODE;
						}
						else
						{
							usLastSubStoreEventIndex = pSharedInfo->MixerInfo.usLastSoutCopyEventPtr;
						}
					}
					else
					{
						return cOCT6100_ERR_FATAL_26;
					}
				}
			}
			else
			{
				if ( pSharedInfo->MixerInfo.usLastSoutCopyEventPtr == cOCT6100_INVALID_INDEX )
				{
					usLastSubStoreEventIndex = cOCT6100_MIXER_HEAD_NODE;
				}
				else
				{
					usLastSubStoreEventIndex = pSharedInfo->MixerInfo.usLastSoutCopyEventPtr;
				}
			}

			/* An Entry was found, now, modify it's value.*/
			mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEntry, usLastSubStoreEventIndex );

			/* Set the Sub-Store event first.*/
			pSubStoreEventEntry->usEventType = cOCT6100_MIXER_CONTROL_MEM_SUB_STORE;
			pSubStoreEventEntry->usNextEventPtr = pTempEntry->usNextEventPtr;

			/*=======================================================================*/
			/* Program the Sub-Store event. */
			WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usSubStoreEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
			WriteParams.ulWriteAddress += 4;
			WriteParams.usWriteData = (UINT16)( pSubStoreEventEntry->usNextEventPtr );
			
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/*=======================================================================*/

			/* Set the load/accumulate event now.*/
			if ( f_fTap == FALSE )
			{
				pLoadEventEntry->usNextEventPtr = f_usSubStoreEventIndex;
			}
			else
			{
				/* This is a little tricky, we use the copy event index for accumulating the tapped channel's voice. */
				pLoadEventEntry->usNextEventPtr = f_usCopyEventIndex;
			}

			/*=======================================================================*/
			/* Program the load/accumulate event. */
			WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usLoadEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
			WriteParams.ulWriteAddress += 4;

			WriteParams.usWriteData = (UINT16)( pLoadEventEntry->usNextEventPtr );
			
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/*=======================================================================*/

			/* Now modify the previous last Sub Store event from another bridge. */
			pTempEntry->usNextEventPtr = f_usLoadEventIndex;

			/*=======================================================================*/
			/* Modify the last node of the other bridge. */
			WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( usLastSubStoreEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
			WriteParams.ulWriteAddress += 4;

			WriteParams.usWriteData = (UINT16)( pTempEntry->usNextEventPtr );
			
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/*=======================================================================*/
			
			/* Set the event pointer info in the bridge stucture. */
			pBridgeEntry->usFirstLoadEventPtr = f_usLoadEventIndex;
			pBridgeEntry->usFirstSubStoreEventPtr = f_usSubStoreEventIndex;
			pBridgeEntry->usLastSubStoreEventPtr = f_usSubStoreEventIndex;

			/* Update the global mixer pointers. */
			if ( pSharedInfo->MixerInfo.usFirstBridgeEventPtr == cOCT6100_INVALID_INDEX )
			{
				/* This bridge is the first to generate mixer event. */
				pSharedInfo->MixerInfo.usFirstBridgeEventPtr = f_usLoadEventIndex;
				pSharedInfo->MixerInfo.usLastBridgeEventPtr	 = f_usSubStoreEventIndex;	
			}
			else if ( pSharedInfo->MixerInfo.usLastBridgeEventPtr == usLastSubStoreEventIndex )
			{
				/* The two entries were added at the end of the bridge section, */
				/* change only the last pointer. */
				pSharedInfo->MixerInfo.usLastBridgeEventPtr  = f_usSubStoreEventIndex;
			}
			else if ( usLastSubStoreEventIndex == cOCT6100_MIXER_HEAD_NODE ||
					  usLastSubStoreEventIndex == pSharedInfo->MixerInfo.usLastSoutCopyEventPtr )
			{
				/* The two entries were added at the start of the bridge section, */
				/* change only the first pointer. */
				pSharedInfo->MixerInfo.usFirstBridgeEventPtr = f_usLoadEventIndex;
			}
		}
		else /* pBridgeEntry->usNumClients != 0 */
		{
			/* For sanity. */
			if ( f_fTap == TRUE )
				return cOCT6100_ERR_FATAL_D2;
			
			/*=======================================================================*/
			/* Program the Load event. */

			/* Now find the last load entry of this bridge. */
			ulResult = Oct6100ApiGetPreviousEvent( f_pApiInstance, pBridgeEntry->usFirstLoadEventPtr, pBridgeEntry->usFirstSubStoreEventPtr, 0, &usLastLoadEventIndex );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Add the load/accumulate event to the list. */
			mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEntry, usLastLoadEventIndex  );
			
			/* Set the load event now. */
			pLoadEventEntry->usNextEventPtr = pTempEntry->usNextEventPtr;

			WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usLoadEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
			WriteParams.ulWriteAddress += 4;

			WriteParams.usWriteData = (UINT16)( pLoadEventEntry->usNextEventPtr );
			
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/*=======================================================================*/


			/*=======================================================================*/
			/* Modify the previous last load event. */

			/* Now modify the previous last load event. */
			pTempEntry->usNextEventPtr = f_usLoadEventIndex;

			WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( usLastLoadEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
			WriteParams.ulWriteAddress += 4;

			WriteParams.usWriteData = (UINT16)( pTempEntry->usNextEventPtr );
			
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/*=======================================================================*/


			/*=======================================================================*/
			/* Program the Sub-Store event. */

			usLastSubStoreEventIndex = pBridgeEntry->usLastSubStoreEventPtr;

			mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEntry, usLastSubStoreEventIndex );

			/* Set the Sub-Store event first. */
			pSubStoreEventEntry->usNextEventPtr = pTempEntry->usNextEventPtr;

			WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usSubStoreEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
			WriteParams.ulWriteAddress += 4;

			WriteParams.usWriteData = (UINT16)( pSubStoreEventEntry->usNextEventPtr );
			
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/*=======================================================================*/


			/*=======================================================================*/
			/* Modify the previous last sub store event of the bridge. */

			/* Now modify the last Load event of the bridge. */
			pTempEntry->usNextEventPtr = f_usSubStoreEventIndex;

			WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( usLastSubStoreEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
			WriteParams.ulWriteAddress += 4;

			WriteParams.usWriteData = (UINT16)( pTempEntry->usNextEventPtr );
			
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/*=======================================================================*/

			/* Update the bridge pointers. */
			pBridgeEntry->usLastSubStoreEventPtr = f_usSubStoreEventIndex;

			/* Check if modification to the global mixer pointer are required. */
			if ( pSharedInfo->MixerInfo.usLastBridgeEventPtr == usLastSubStoreEventIndex )
			{
				/* We have a new last bridge pointer. */
				 pSharedInfo->MixerInfo.usLastBridgeEventPtr = f_usSubStoreEventIndex;
			}	
		}

		if ( f_fTap == TRUE )
		{
			if ( pEchoChanEntry->usRinTsstIndex == cOCT6100_INVALID_INDEX )
			{
				/* Remove the mute on the Rin port. */

				UINT32 ulTempData;
				UINT32 ulMask;
				UINT32 ulBaseAddress = cOCT6100_CHANNEL_ROOT_BASE + ( f_usChanIndex * cOCT6100_CHANNEL_ROOT_SIZE ) + pSharedInfo->MemoryMap.ulChanRootConfOfst;

				/* Configure the level control. */

				UINT32 ulFeatureBytesOffset = pSharedInfo->MemoryMap.RinLevelControlOfst.usDwordOffset * 4;
				UINT32 ulFeatureBitOffset	 = pSharedInfo->MemoryMap.RinLevelControlOfst.byBitOffset;
				UINT32 ulFeatureFieldLength = pSharedInfo->MemoryMap.RinLevelControlOfst.byFieldSize;

				ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
													pEchoChanEntry,
													ulBaseAddress + ulFeatureBytesOffset,
													&ulTempData);
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;

				/* Clear previous value set in the feature field.*/
				mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

				ulTempData &= (~ulMask);

				/* Set the DC filter to pass through.*/
				ulTempData |= ( cOCT6100_PASS_THROUGH_LEVEL_CONTROL << ulFeatureBitOffset );

				/* First read the DWORD where the field is located. */
				ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
												pEchoChanEntry,
												ulBaseAddress + ulFeatureBytesOffset,
												ulTempData);
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;
			}
		}

		/* Set the event entries as reserved. */
		pLoadEventEntry->fReserved = TRUE;
		pSubStoreEventEntry->fReserved = TRUE;

		/* Store the event indexes into the channel structure. */
		pEchoChanEntry->usLoadEventIndex = f_usLoadEventIndex;
		pEchoChanEntry->usSubStoreEventIndex = f_usSubStoreEventIndex;

		/* Check if must insert the Sin copy event in the list. */
		if ( fAddSinCopy == TRUE )
		{
			/* Now insert the Sin copy event into the list. */
			ulResult = Oct6100ApiMixerEventAdd( f_pApiInstance,
												f_usCopyEventIndex,
												cOCT6100_EVENT_TYPE_SIN_COPY,
												f_usChanIndex );
		}

		/* Check if the Rin silence event can be cleared now that the */
		/* channel has been added to a conference. */
		if ( ( f_fTap == FALSE ) && ( pEchoChanEntry->usRinSilenceEventIndex != cOCT6100_INVALID_INDEX ) )
		{
			/* Remove the event from the list. */
			ulResult = Oct6100ApiMixerEventRemove(	f_pApiInstance,
													pEchoChanEntry->usRinSilenceEventIndex,
													cOCT6100_EVENT_TYPE_SOUT_COPY );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pEchoChanEntry->usRinSilenceEventIndex );
			if ( ulResult != cOCT6100_ERR_OK )
				return cOCT6100_ERR_FATAL_DF;

			pEchoChanEntry->usRinSilenceEventIndex = cOCT6100_INVALID_INDEX;
		}
	}

	/* Configure the RIN copy mixer entry and memory - if using the RIN port. */
	if ( ( f_fFlexibleConfBridge == TRUE ) && ( f_ulInputPort == cOCT6100_CHANNEL_PORT_RIN ) )
	{
		if ( pEchoChanEntry->usRinTsstIndex != cOCT6100_INVALID_INDEX )
		{
			ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
															  pEchoChanEntry->usRinTsstIndex,
															  pEchoChanEntry->usExtraRinTsiMemIndex,
															  pEchoChanEntry->TdmConfig.byRinPcmLaw );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	}

	if ( pBridgeEntry->fDominantSpeakerSet == TRUE )
	{
		/* Dominant speaker is another channel.  Set accordingly for this new conference channel. */
		ulResult = Oct6100ApiBridgeSetDominantSpeaker( f_pApiInstance, f_usChanIndex, pBridgeEntry->usDominantSpeakerChanIndex );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}
	else
	{
		/* No dominant speaker set on this bridge yet. */
		ulResult = Oct6100ApiBridgeSetDominantSpeaker( f_pApiInstance, f_usChanIndex, cOCT6100_CONF_DOMINANT_SPEAKER_UNASSIGNED );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/* Update the bridge entry. */
	pBridgeEntry->usNumClients++;

	/* Store the bridge index into the channel structure. */
	pEchoChanEntry->usBridgeIndex = f_usBridgeIndex;

	/* Store the copy event index into the channel structure. */
	if ( f_ulInputPort == cOCT6100_CHANNEL_PORT_SOUT )
	{
		pEchoChanEntry->usSinCopyEventIndex	= f_usCopyEventIndex;
	}
	else
	{
		pEchoChanEntry->usSinCopyEventIndex	= cOCT6100_INVALID_INDEX;
	}

	/* Remember if the channel is muted in the conference. */
	pEchoChanEntry->fMute = f_fMute;

	/* Remember if the channel is a tap in a conference. */
	pEchoChanEntry->fTap = f_fTap;

	/* We start by not being tapped. */
	pEchoChanEntry->fBeingTapped = FALSE;
	pEchoChanEntry->usTapChanIndex = cOCT6100_INVALID_INDEX;

	/* Remember the tap bridge index if necessary. */
	if ( pEchoChanEntry->fTap == TRUE )
	{
		pEchoChanEntry->usTapBridgeIndex = f_usTapBridgeIndex;
	}
	else
	{
		pEchoChanEntry->usTapBridgeIndex = cOCT6100_INVALID_INDEX;
	}

	/* Indicate that the extra SIN TSI is currently in used by the conference bridge. */
	if ( f_ulInputPort == cOCT6100_CHANNEL_PORT_SOUT )
	{
		pEchoChanEntry->usExtraSinTsiDependencyCnt++;
	}

	/* Indicate that the extra RIN TSI is currently in used by the conference bridge. */
	if ( ( f_fFlexibleConfBridge == TRUE ) && ( f_ulInputPort == cOCT6100_CHANNEL_PORT_RIN ) )
	{
		pEchoChanEntry->usExtraRinTsiDependencyCnt++;
	}

	/* Update the chip stats structure. */
	pSharedInfo->ChipStats.usNumEcChanUsingMixer++;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiBridgeAddParticipantToChannel

Description:    Used for the flexible conference bridges.  Insert a participant
				onto a channel that is on a conference.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance					Pointer to API instance. This memory is used to keep the
								present state of the chip and all its resources.
f_usBridgeIndex					Bridge index where this channel is located.
f_usSourceChannelIndex			Source channel to copy voice from.
f_usDestinationChannelIndex		Destination channel to store resulting voice to.
f_usLoadOrAccumulateEventIndex	Load or Accumulate allocated event index.
f_usStoreEventIndex				Store allocated event index.
f_usCopyEventIndex				Copy allocated event index.
f_ulSourceInputPort				Source input port of the conference for this channel.
f_ulDestinationInputPort		Destination input port of the conference for this channel.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiBridgeAddParticipantToChannel
UINT32	Oct6100ApiBridgeAddParticipantToChannel(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance,
				IN		UINT16					f_usBridgeIndex, 
				IN		UINT16					f_usSourceChannelIndex,
				IN		UINT16					f_usDestinationChannelIndex,
				IN		UINT16					f_usLoadOrAccumulateEventIndex,
				IN		UINT16					f_usStoreEventIndex,
				IN		UINT16					f_usCopyEventIndex,
				IN		UINT32					f_ulSourceInputPort,
				IN		UINT32					f_ulDestinationInputPort )
{
	tPOCT6100_API_CONF_BRIDGE			pBridgeEntry;

	tPOCT6100_API_MIXER_EVENT			pLoadEventEntry;
	tPOCT6100_API_MIXER_EVENT			pStoreEventEntry;
	tPOCT6100_API_MIXER_EVENT			pTempEntry;

	tPOCT6100_API_CHANNEL				pSourceChanEntry;
	tPOCT6100_API_CHANNEL				pDestinationChanEntry;

	tPOCT6100_API_FLEX_CONF_PARTICIPANT	pDestinationParticipant;

	tPOCT6100_SHARED_INFO			pSharedInfo;
	tOCT6100_WRITE_PARAMS			WriteParams;

	UINT32							ulResult;
	UINT16							usLastSubStoreEventIndex;
	UINT16							usLastLoadEventIndex;
	BOOL							fInsertCopy = FALSE;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;
	
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBridgeEntry, f_usBridgeIndex );

	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pSourceChanEntry, f_usSourceChannelIndex );
	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pDestinationChanEntry, f_usDestinationChannelIndex );
	mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( f_pApiInstance->pSharedInfo, pDestinationParticipant, pDestinationChanEntry->usFlexConfParticipantIndex );

	mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pLoadEventEntry, f_usLoadOrAccumulateEventIndex );
	mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pStoreEventEntry, f_usStoreEventIndex );

	/* Check if we are creating the first event for this channel. */
	if ( pDestinationParticipant->fFlexibleMixerCreated == FALSE )
	{
		/*=======================================================================*/
		/* Before creating the participant's flexible mixer, make sure the extra Sin */
		/* mixer event is programmed correctly for sending the voice stream to the right place. */

		/* Configure the SIN copy mixer entry and memory - if using the SOUT port. */
		if ( f_ulDestinationInputPort == cOCT6100_CHANNEL_PORT_SOUT )
		{
			if ( pDestinationChanEntry->usSinTsstIndex != cOCT6100_INVALID_INDEX )
			{
				ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
																  pDestinationChanEntry->usSinTsstIndex,
																  pDestinationChanEntry->usExtraSinTsiMemIndex,
																  pDestinationChanEntry->TdmConfig.bySinPcmLaw );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;
			}

			/* If the silence TSI is loaded on this port, update with the extra sin TSI. */
			if ( pDestinationChanEntry->usSinSilenceEventIndex != cOCT6100_INVALID_INDEX )
			{
				WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pDestinationChanEntry->usSinSilenceEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );

				WriteParams.ulWriteAddress += 2;
				WriteParams.usWriteData = pDestinationChanEntry->usExtraSinTsiMemIndex;

				mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;
			}
		}
		/*=======================================================================*/

		
		/*=======================================================================*/
		/* Program the load event.  This is the first event for this new destination channel. */

		/* First set the TSI buffer where the resulting stream should be written to. */
		WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usLoadOrAccumulateEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );

		/* For sure, we are loading. */
		WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_LOAD;

		/* Select the TSI memory index according to the source port. */
		if ( f_ulSourceInputPort == cOCT6100_CHANNEL_PORT_SOUT )
		{
			WriteParams.usWriteData |= pSourceChanEntry->usSinSoutTsiMemIndex;
		}
		else /* if ( f_ulSourceInputPort == cOCT6100_CHANNEL_PORT_RIN ) */
		{
			WriteParams.usWriteData |= pSourceChanEntry->usExtraRinTsiMemIndex;
		}

		/* Set the event type. */
		pLoadEventEntry->usEventType = cOCT6100_MIXER_CONTROL_MEM_LOAD;

		/* Set the source channel index. */
		pLoadEventEntry->usSourceChanIndex = f_usSourceChannelIndex;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/*=======================================================================*/



		/*=======================================================================*/
		/* Program the store event. */

		WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usStoreEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
		
		WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_STORE;

		/* Select the TSI memory index and PCM law according to the source port. */
		if ( f_ulDestinationInputPort == cOCT6100_CHANNEL_PORT_SOUT )
		{
			WriteParams.usWriteData |= pDestinationChanEntry->TdmConfig.bySoutPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;
			WriteParams.usWriteData |= pDestinationChanEntry->usSinSoutTsiMemIndex;
		}
		else /* if ( f_ulDestinationInputPort == cOCT6100_CHANNEL_PORT_RIN ) */
		{
			WriteParams.usWriteData |= pDestinationChanEntry->TdmConfig.byRinPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;
			WriteParams.usWriteData |= pDestinationChanEntry->usRinRoutTsiMemIndex;
		}
		
		/* Set the event type. */
		pStoreEventEntry->usEventType = cOCT6100_MIXER_CONTROL_MEM_STORE;

		/* Set the destination channel index. */
		pStoreEventEntry->usDestinationChanIndex = f_usDestinationChannelIndex;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress += 2;
		WriteParams.usWriteData = pDestinationChanEntry->usRinRoutTsiMemIndex;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/*=======================================================================*/



		/*=======================================================================*/
		/* Program the Copy event - if using the SOUT port */

		if ( ( f_ulDestinationInputPort == cOCT6100_CHANNEL_PORT_SOUT ) && ( pDestinationChanEntry->fCopyEventCreated == FALSE ) )
		{
			/* The copy event has not been created, create it once for the life of the participant on the bridge. */
			WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usCopyEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
			
			WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_COPY;
			WriteParams.usWriteData |= pDestinationChanEntry->usExtraSinTsiMemIndex;
			WriteParams.usWriteData |= pDestinationChanEntry->TdmConfig.bySinPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;

			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			WriteParams.ulWriteAddress += 2;
			WriteParams.usWriteData = pDestinationChanEntry->usSinSoutTsiMemIndex;

			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			pDestinationChanEntry->fCopyEventCreated = TRUE;

			/* Set insert copy flag. */
			fInsertCopy = TRUE;
		}
		
		/*=======================================================================*/



		/*=======================================================================*/
		/*=======================================================================*/
		/* Now, insert the events into the current list. */
		/*=======================================================================*/
		/*=======================================================================*/

		/* This is the first entry for this channel. Insert the two events at the head */
		/* of the list just after the last Sub-Store or Store event. */
		ulResult = Oct6100ApiGetPrevLastSubStoreEvent( f_pApiInstance, f_usBridgeIndex, f_usLoadOrAccumulateEventIndex, &usLastSubStoreEventIndex );
		if ( ulResult != cOCT6100_ERR_OK )
		{
			if ( ulResult == cOCT6100_ERR_CONF_MIXER_EVENT_NOT_FOUND )
			{
				if ( pSharedInfo->MixerInfo.usLastSoutCopyEventPtr == cOCT6100_INVALID_INDEX )
				{
					usLastSubStoreEventIndex = cOCT6100_MIXER_HEAD_NODE;
				}
				else
				{
					usLastSubStoreEventIndex = pSharedInfo->MixerInfo.usLastSoutCopyEventPtr;
				}
			}
			else
			{
				return cOCT6100_ERR_FATAL_26;
			}
		}

		/* An entry was found, now, modify it's value. */
		mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEntry, usLastSubStoreEventIndex );

		/*=======================================================================*/


		/*=======================================================================*/
		/* Link the store event first. */
		
		pStoreEventEntry->usNextEventPtr = pTempEntry->usNextEventPtr;

		/* Link the store event. */
		WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usStoreEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
		WriteParams.ulWriteAddress += 4;
		WriteParams.usWriteData = (UINT16)( pStoreEventEntry->usNextEventPtr );
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/*=======================================================================*/


		/*=======================================================================*/
		/* Link the load event now.*/

		pLoadEventEntry->usNextEventPtr = f_usStoreEventIndex;

		/* Link the load event.*/
		WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usLoadOrAccumulateEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
		WriteParams.ulWriteAddress += 4;

		WriteParams.usWriteData = (UINT16)( pLoadEventEntry->usNextEventPtr );
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/*=======================================================================*/


		/*=======================================================================*/
		/* Now modify the previous last Sub-Store or Store event from another bridge, */
		/* such that it links to us. */

		pTempEntry->usNextEventPtr = f_usLoadOrAccumulateEventIndex;

		/* Modify the last node of the other bridge. */
		WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( usLastSubStoreEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
		WriteParams.ulWriteAddress += 4;

		WriteParams.usWriteData = (UINT16)( pTempEntry->usNextEventPtr );
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/*=======================================================================*/


		/*=======================================================================*/
		/* Set the event pointer info in the bridge stucture. */

		if ( pBridgeEntry->usFirstLoadEventPtr == cOCT6100_INVALID_INDEX )
		{
			/* We only do this once in case of the flexible conference bridges. */
			pBridgeEntry->usFirstLoadEventPtr = f_usLoadOrAccumulateEventIndex;
			pBridgeEntry->usFirstSubStoreEventPtr = f_usStoreEventIndex;
		}

		pBridgeEntry->usLastSubStoreEventPtr = f_usStoreEventIndex;

		/* Update the global mixer pointers. */
		if ( pSharedInfo->MixerInfo.usFirstBridgeEventPtr == cOCT6100_INVALID_INDEX )
		{
			/* This bridge is the first to generate mixer event. */
			pSharedInfo->MixerInfo.usFirstBridgeEventPtr = f_usLoadOrAccumulateEventIndex;
			pSharedInfo->MixerInfo.usLastBridgeEventPtr	 = f_usStoreEventIndex;	
		}
		else if ( pSharedInfo->MixerInfo.usLastBridgeEventPtr == usLastSubStoreEventIndex )
		{
			/* The two entries were added at the end of the bridge section, */
			/* change only the last pointer. */
			pSharedInfo->MixerInfo.usLastBridgeEventPtr = f_usStoreEventIndex;
		}
		else if ( usLastSubStoreEventIndex == cOCT6100_MIXER_HEAD_NODE ||
				  usLastSubStoreEventIndex == pSharedInfo->MixerInfo.usLastSoutCopyEventPtr )
		{
			/* The two entries were added at the start of the bridge section, */
			/* change only the first pointer.*/
			pSharedInfo->MixerInfo.usFirstBridgeEventPtr = f_usLoadOrAccumulateEventIndex;
		}

		/*=======================================================================*/


		/*=======================================================================*/
		/* Insert the copy event if needed in the mixer's list. */

		if ( fInsertCopy == TRUE )
		{
			/* Now insert the Sin copy event into the list. */
			ulResult = Oct6100ApiMixerEventAdd( f_pApiInstance,
												f_usCopyEventIndex,
												cOCT6100_EVENT_TYPE_SIN_COPY,
												f_usDestinationChannelIndex );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}

		/*=======================================================================*/


		/*=======================================================================*/
		/* Update the status of the instance structures. */

		pDestinationParticipant->fFlexibleMixerCreated = TRUE;

		/* Set the event entries as reserved. */
		pLoadEventEntry->fReserved = TRUE;
		pStoreEventEntry->fReserved = TRUE;

		/* Store the event indexes into the channel structure. */
		pDestinationChanEntry->usLoadEventIndex = f_usLoadOrAccumulateEventIndex;
		pDestinationChanEntry->usSubStoreEventIndex = f_usStoreEventIndex;

		/*=======================================================================*/
	}
	else /* if ( pDestinationChanEntry->fFlexibleMixerCreated == TRUE ) */
	{
		/*=======================================================================*/
		/* Program the Accumulate event. */

		/* First set the TSI buffer where the resulting stream should be written to. */
		WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usLoadOrAccumulateEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );

		/* For sure, we are accumulating. */
		WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_ACCUMULATE;

		/* Select the TSI memory index according to the source port. */
		if ( f_ulSourceInputPort == cOCT6100_CHANNEL_PORT_SOUT )
		{
			WriteParams.usWriteData |= pSourceChanEntry->usSinSoutTsiMemIndex;
		}
		else /* if ( f_ulSourceInputPort == cOCT6100_CHANNEL_PORT_RIN ) */
		{
			WriteParams.usWriteData |= pSourceChanEntry->usExtraRinTsiMemIndex;
		}

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/*=======================================================================*/



		/*=======================================================================*/
		/*=======================================================================*/
		/* Now, insert the Accumulate event into the current list. */
		/*=======================================================================*/
		/*=======================================================================*/

		/* Use the Load entry of this channel. */
		usLastLoadEventIndex = pDestinationChanEntry->usLoadEventIndex;

		/* Add the Accumulate event to the list. */
		mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEntry, usLastLoadEventIndex );
		
		/* Set the accumulate event now. */
		pLoadEventEntry->usNextEventPtr = pTempEntry->usNextEventPtr;

		WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usLoadOrAccumulateEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
		WriteParams.ulWriteAddress += 4;

		WriteParams.usWriteData = (UINT16)( pLoadEventEntry->usNextEventPtr );
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/*=======================================================================*/



		/*=======================================================================*/
		/* Modify the previous Load event. */

		/* Now modify the previous Load event. */
		pTempEntry->usNextEventPtr = f_usLoadOrAccumulateEventIndex;

		WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( usLastLoadEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
		WriteParams.ulWriteAddress += 4;

		WriteParams.usWriteData = (UINT16)( pTempEntry->usNextEventPtr );
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/*=======================================================================*/


		
		/*=======================================================================*/
		/* Update the status of the instance structures. */

		/* Set the Accumulate event entry as reserved. */
		pLoadEventEntry->fReserved = TRUE;
		/* Set the Event type. */
		pLoadEventEntry->usEventType = cOCT6100_MIXER_CONTROL_MEM_ACCUMULATE;
		/* Set the source channel index. */
		pLoadEventEntry->usSourceChanIndex = f_usSourceChannelIndex;

		/*=======================================================================*/
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ConfBridgeChanRemoveSer

Description:    Removes an echo channel from a conference bridge. 

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pConfBridgeRemove		Pointer to conference bridge channel remove structure.  

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ConfBridgeChanRemoveSer
UINT32 Oct6100ConfBridgeChanRemoveSer(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CONF_BRIDGE_CHAN_REMOVE		f_pConfBridgeRemove )
{
	UINT16	usBridgeIndex;
	UINT16	usChanIndex = 0;
	UINT16	usLoadEventIndex;
	UINT16	usSubStoreEventIndex;
	UINT16	usCopyEventIndex;
	UINT32	ulResult;
	UINT8	fFlexibleConfBridge;
	UINT8	fTap;

	/* Check the validity of the channel and conference bridge given. */
	ulResult = Oct6100ApiCheckChanRemoveParams(
										f_pApiInstance, 
										f_pConfBridgeRemove, 
										&usBridgeIndex, 
										&usChanIndex, 
										&fFlexibleConfBridge, 
										&fTap,
										&usLoadEventIndex, 
										&usSubStoreEventIndex, 
										&usCopyEventIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Release all resources reserved for the conference bridge. */
	ulResult = Oct6100ApiReleaseChanEventResources( 
										f_pApiInstance, 
										f_pConfBridgeRemove, 
										usBridgeIndex, 
										usChanIndex, 
										fFlexibleConfBridge, 
										usLoadEventIndex, 
										usSubStoreEventIndex, 
										usCopyEventIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Clear the memory entry for this channel within the bridge. */
	ulResult = Oct6100ApiBridgeEventRemove( 
										f_pApiInstance, 
										f_pConfBridgeRemove, 
										usBridgeIndex, 
										usChanIndex, 
										fFlexibleConfBridge, 
										usLoadEventIndex, 
										usSubStoreEventIndex, 
										usCopyEventIndex,
										fTap );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckChanRemoveParams

Description:	Check the validity of the channel and conference bridge given.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.
f_pConfBridgeRemove			Pointer to conference bridge channenl add structure.  
f_pusBridgeIndex			Pointer to the bridge index.
f_pfFlexibleConfBridge		If this is a flexible conference bridge
f_pusChannelIndex			Pointer to the channel index to be added to the bridge.
f_pusLoadEventIndex			Pointer to the load mixer event.
f_pusSubStoreEventIndex		Pointer to the sub-store mixer event.
f_pusCopyEventIndex			Pointer to the copy mixer event.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckChanRemoveParams
UINT32 Oct6100ApiCheckChanRemoveParams(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_CONF_BRIDGE_CHAN_REMOVE	f_pConfBridgeRemove,
				OUT		PUINT16								f_pusBridgeIndex, 
				OUT		PUINT16								f_pusChannelIndex,
				OUT		PUINT8								f_pfFlexibleConfBridge,
				OUT		PUINT8								f_pfTap,
				OUT		PUINT16								f_pusLoadEventIndex,
				OUT		PUINT16								f_pusSubStoreEventIndex,
				OUT		PUINT16								f_pusCopyEventIndex )
{
	UINT32						ulEntryOpenCnt;
	tPOCT6100_API_CHANNEL		pEchoChanEntry;
	tPOCT6100_API_CONF_BRIDGE	pBridgeEntry;

	/* Verify if the remove all flag is valid. */
	if ( f_pConfBridgeRemove->fRemoveAll != TRUE && 
		 f_pConfBridgeRemove->fRemoveAll != FALSE )
		return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_REMOVE_ALL;

	/* Check the channel handle only if the remove all flag is set to FALSE. */
	if ( f_pConfBridgeRemove->fRemoveAll == FALSE )
	{
		/*=====================================================================*/
		/* Check the channel handle. */

		if ( (f_pConfBridgeRemove->ulChannelHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CHANNEL )
			return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

		*f_pusChannelIndex = (UINT16)( f_pConfBridgeRemove->ulChannelHndl & cOCT6100_HNDL_INDEX_MASK );
		if ( *f_pusChannelIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxChannels )
			return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

		mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pEchoChanEntry, *f_pusChannelIndex )

		/* Extract the entry open count from the provided handle. */
		ulEntryOpenCnt = (f_pConfBridgeRemove->ulChannelHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

		/* Check for errors. */
		if ( pEchoChanEntry->fReserved != TRUE )
			return cOCT6100_ERR_CONF_BRIDGE_NOT_OPEN;
		if ( ulEntryOpenCnt != pEchoChanEntry->byEntryOpenCnt )
			return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;
		if ( pEchoChanEntry->fBeingTapped == TRUE )
			return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_TAP_DEPENDENCY;

		/*=====================================================================*/

		*f_pusBridgeIndex = pEchoChanEntry->usBridgeIndex;
		*f_pusLoadEventIndex = pEchoChanEntry->usLoadEventIndex;
		*f_pusSubStoreEventIndex = pEchoChanEntry->usSubStoreEventIndex;
		*f_pusCopyEventIndex = pEchoChanEntry->usSinCopyEventIndex;

		/* Check if the channel is really part of the bridge. */
		if ( *f_pusBridgeIndex == cOCT6100_INVALID_INDEX )
			return cOCT6100_ERR_CONF_BRIDGE_CHAN_NOT_ON_BRIDGE;

		mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBridgeEntry, *f_pusBridgeIndex )

		/* Return whether this is a flexible bridge or not. */
		*f_pfFlexibleConfBridge = pBridgeEntry->fFlexibleConferencing;

		/* Return whether this is a tap or not. */
		*f_pfTap = pEchoChanEntry->fTap;
	}
	else /* f_pConfBridgeRemove->fRemoveAll == TRUE */
	{
		/* Check the provided handle. */
		if ( (f_pConfBridgeRemove->ulConfBridgeHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CONF_BRIDGE )
			return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

		*f_pusBridgeIndex = (UINT16)( f_pConfBridgeRemove->ulConfBridgeHndl & cOCT6100_HNDL_INDEX_MASK );
		if ( *f_pusBridgeIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxConfBridges )
			return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

		mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBridgeEntry, *f_pusBridgeIndex )

		/* Extract the entry open count from the provided handle. */
		ulEntryOpenCnt = (f_pConfBridgeRemove->ulConfBridgeHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

		/* Check for errors. */
		if ( pBridgeEntry->fReserved != TRUE )
			return cOCT6100_ERR_CONF_BRIDGE_NOT_OPEN;
		if ( ulEntryOpenCnt != pBridgeEntry->byEntryOpenCnt )
			return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

		/* This information is not currently available. */
		*f_pusLoadEventIndex = cOCT6100_INVALID_INDEX;
		*f_pusSubStoreEventIndex = cOCT6100_INVALID_INDEX;
		*f_pusCopyEventIndex = cOCT6100_INVALID_INDEX;

		/* Return whether this is a flexible bridge or not. */
		*f_pfFlexibleConfBridge = pBridgeEntry->fFlexibleConferencing;

		*f_pfTap = FALSE;
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseChanEventResources

Description:    Release all resources reserved to the channel part of the 
				conference bridge.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.
f_pConfBridgeRemove			Pointer to conference bridge channel add structure.  
f_usBridgeIndex				Index of the bridge structure within the API's bridge list.
f_usChanIndex				Index of the channel structure within the API's channel list
f_fFlexibleConfBridge		If this is a flexible conference bridge.
f_usLoadEventIndex			Index of the load mixer event.
f_usSubStoreEventIndex		Index of the sub-store mixer event.
f_usCopyEventIndex			Index of the copy mixer event.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseChanEventResources
UINT32 Oct6100ApiReleaseChanEventResources(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_CONF_BRIDGE_CHAN_REMOVE	f_pConfBridgeRemove,
				IN		UINT16								f_usBridgeIndex, 
				IN		UINT16								f_usChanIndex,
				IN		UINT8								f_fFlexibleConfBridge,
				IN		UINT16								f_usLoadEventIndex,
				IN		UINT16								f_usSubStoreEventIndex,
				IN		UINT16								f_usCopyEventIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	tPOCT6100_API_CHANNEL		pEchoChanEntry;
	UINT32	ulResult;
	UINT32	i;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	if ( f_fFlexibleConfBridge == TRUE )
	{
		tPOCT6100_API_FLEX_CONF_PARTICIPANT pParticipant;
		tPOCT6100_API_FLEX_CONF_PARTICIPANT pTempParticipant;

		if ( f_pConfBridgeRemove->fRemoveAll == FALSE )
		{
			tPOCT6100_API_CHANNEL		pTempEchoChanEntry;

			mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChanEntry, f_usChanIndex );
			mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pParticipant, pEchoChanEntry->usFlexConfParticipantIndex );

			/* Release an entry for the store event in the mixer memory. */
			ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, f_usSubStoreEventIndex );
			if ( ulResult != cOCT6100_ERR_OK )
			{
				return ulResult;
			}

			/* Release an entry for the Sin copy event in the mixer memory. */
			/* This value can be invalid if the Rin port was used - no need to release. */
			if ( f_usCopyEventIndex != cOCT6100_INVALID_INDEX )
			{
				ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, f_usCopyEventIndex );
				if ( ulResult != cOCT6100_ERR_OK )
				{
					return ulResult;
				}
			}

			/* This value can be 0 if the Rin port was used - no need to release. */
			if ( pEchoChanEntry->usExtraSinTsiDependencyCnt == 1 )
			{
				/* Release the extra TSI entry.*/
				ulResult = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance, pEchoChanEntry->usExtraSinTsiMemIndex );
				if ( ulResult != cOCT6100_ERR_OK )
				{
					return ulResult;
				}
			}

			/* This value can be 0 if the Sout port was used - no need to release. */
			if ( pEchoChanEntry->usExtraRinTsiDependencyCnt == 1 )
			{
				/* Release the extra TSI entry.*/
				ulResult = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance, pEchoChanEntry->usExtraRinTsiMemIndex );
				if ( ulResult != cOCT6100_ERR_OK )
				{
					return ulResult;
				}
			}

			/* Must travel all clients of this conference and release the load or accumulate events for */
			/* all participants which can hear us and vice versa. */

			/* Search through the list of API channel entry for the ones on to this bridge. */
			for ( i = 0; ( i < pSharedInfo->ChipConfig.usMaxChannels ) && ( ulResult == cOCT6100_ERR_OK ); i++ )
			{
				mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pTempEchoChanEntry, i );
			
				/* Channel reserved? */
				if ( ( i != f_usChanIndex ) && pTempEchoChanEntry->fReserved == TRUE )
				{
					/* On current bridge? */
					if ( pTempEchoChanEntry->usBridgeIndex == f_usBridgeIndex )
					{
						mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pTempParticipant, pTempEchoChanEntry->usFlexConfParticipantIndex );

						/* Check if we can hear this participant. */
						if ( ( pParticipant->ulListenerMask & ( 0x1 << pTempParticipant->ulListenerMaskIndex ) ) == 0x0 )
						{
							/* Must release the allocated mixer event. */
							ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pParticipant->ausLoadOrAccumulateEventIndex[ pTempParticipant->ulListenerMaskIndex ] );
							if ( ulResult != cOCT6100_ERR_OK )
							{
								return ulResult;
							}
							
							pParticipant->ausLoadOrAccumulateEventIndex[ pTempParticipant->ulListenerMaskIndex ] = cOCT6100_INVALID_INDEX;
						}

						/* Check if this participant can hear us. */
						if ( ( pTempParticipant->ulListenerMask & ( 0x1 << pParticipant->ulListenerMaskIndex ) ) == 0x0 )
						{
							/* Must release the allocated mixer event. */
							ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pTempParticipant->ausLoadOrAccumulateEventIndex[ pParticipant->ulListenerMaskIndex ] );
							if ( ulResult != cOCT6100_ERR_OK )
							{
								return ulResult;
							}

							pTempParticipant->ausLoadOrAccumulateEventIndex[ pParticipant->ulListenerMaskIndex ] = cOCT6100_INVALID_INDEX;
						}
					}
				}
			}
		}
		else /* f_pConfBridgeRemove->fRemoveAll == TRUE */
		{
			UINT32 ulListenerMaskIndex;

			ulResult = cOCT6100_ERR_OK;
			
			/* Search through the list of API channel entry for the ones on to this bridge.*/
			for ( i = 0; ( i < pSharedInfo->ChipConfig.usMaxChannels ) && ( ulResult == cOCT6100_ERR_OK ); i++ )
			{
				mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChanEntry, i );
				
				/* Channel reserved? */
				if ( pEchoChanEntry->fReserved == TRUE )
				{
					/* On current bridge? */
					if ( pEchoChanEntry->usBridgeIndex == f_usBridgeIndex )
					{
						mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pParticipant, pEchoChanEntry->usFlexConfParticipantIndex );

						/* Release an entry for the Store event in the Mixer memory. */
						ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pEchoChanEntry->usSubStoreEventIndex );
						if ( ulResult != cOCT6100_ERR_OK )
						{
							return ulResult;
						}

						/* Release an entry for the Sin copy event in the Mixer memory. */
						/* This value can be invalid if the Rin port was used - no need to release. */
						if ( pEchoChanEntry->usSinCopyEventIndex != cOCT6100_INVALID_INDEX )
						{
							ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pEchoChanEntry->usSinCopyEventIndex );
							if ( ulResult != cOCT6100_ERR_OK )
							{
								return ulResult;
							}
						}

						/* This value can be 0 if the Rin port was used - no need to release. */
						if ( pEchoChanEntry->usExtraSinTsiDependencyCnt == 1 )
						{
							/* Release the extra TSI entry.*/
							ulResult = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance, pEchoChanEntry->usExtraSinTsiMemIndex );
							if ( ulResult != cOCT6100_ERR_OK )
							{
								return ulResult;
							}
						}

						/* This value can be 0 if the Sout port was used - no need to release. */
						if ( pEchoChanEntry->usExtraRinTsiDependencyCnt == 1 )
						{
							/* Release the extra TSI entry.*/
							ulResult = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance, pEchoChanEntry->usExtraRinTsiMemIndex );
							if ( ulResult != cOCT6100_ERR_OK )
							{
								return ulResult;
							}
						}

						/* Check if something can be freed. */
						for ( ulListenerMaskIndex = 0; ulListenerMaskIndex < cOCT6100_MAX_FLEX_CONF_PARTICIPANTS_PER_BRIDGE; ulListenerMaskIndex ++ )
						{
							if ( pParticipant->ausLoadOrAccumulateEventIndex[ ulListenerMaskIndex ] != cOCT6100_INVALID_INDEX )
							{
								/* Must release the allocated mixer event. */
								ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pParticipant->ausLoadOrAccumulateEventIndex[ ulListenerMaskIndex ] );
								if ( ulResult != cOCT6100_ERR_OK )
								{
									return ulResult;
								}
								
								pParticipant->ausLoadOrAccumulateEventIndex[ ulListenerMaskIndex ] = cOCT6100_INVALID_INDEX;
							}
						}
					}
				}
			}

			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	}
	else /* if ( f_fFlexibleConfBridge == FALSE ) */
	{
		if ( f_pConfBridgeRemove->fRemoveAll == FALSE )
		{
			mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChanEntry, f_usChanIndex );
			
			/* Release the entry for the load event in the mixer memory. */
			ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, f_usLoadEventIndex );
			if ( ulResult != cOCT6100_ERR_OK )
			{
				return ulResult;
			}

			/* Release an entry for the substract and store event in the mixer memory. */
			ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, f_usSubStoreEventIndex );
			if ( ulResult != cOCT6100_ERR_OK )
			{
				return ulResult;
			}

			/* Release an entry for the Sin copy event in the Mixer memory. */
			/* This value can be invalid if the Rin port was used - no need to release. */
			if ( f_usCopyEventIndex != cOCT6100_INVALID_INDEX )
			{
				ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, f_usCopyEventIndex );
				if ( ulResult != cOCT6100_ERR_OK )
				{
					return ulResult;
				}
			}

			/* This value can be 0 if the Rin port was used - no need to release. */
			if ( pEchoChanEntry->usExtraSinTsiDependencyCnt == 1 )
			{
				/* Release the extra TSI entry. */
				ulResult = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance, pEchoChanEntry->usExtraSinTsiMemIndex );
				if ( ulResult != cOCT6100_ERR_OK )
				{
					return ulResult;
				}
			}
		}
		else /* f_pConfBridgeRemove->fRemoveAll == TRUE */
		{
			/* Search through the list of API channel entry for the ones on to the specified bridge.*/
			for ( i = 0; i < pSharedInfo->ChipConfig.usMaxChannels; i++ )
			{
				mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChanEntry, i );
				
				if ( pEchoChanEntry->fReserved == TRUE )
				{
					if ( ( pEchoChanEntry->usBridgeIndex == f_usBridgeIndex ) && ( pEchoChanEntry->fTap == FALSE ) )
					{
						/* Release the entry for the load event in the mixer memory. */
						ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, 
																	  pEchoChanEntry->usLoadEventIndex );
						if ( ulResult != cOCT6100_ERR_OK )
						{
							return ulResult;
						}

						/* Release an entry for the substract and store event in the Mixer memory. */
						ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, 
																	  pEchoChanEntry->usSubStoreEventIndex );
						if ( ulResult != cOCT6100_ERR_OK )
						{
							return ulResult;
						}

						/* Release an entry for the Sin copy event in the Mixer memory. */
						/* This value can be invalid if the Rin port was used - no need to release. */
						if ( pEchoChanEntry->usSinCopyEventIndex != cOCT6100_INVALID_INDEX )
						{
							ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, 
																		  pEchoChanEntry->usSinCopyEventIndex );
							if ( ulResult != cOCT6100_ERR_OK )
							{
								return ulResult;
							}
						}

						/* This value can be 0 if the Rin port was used - no need to release. */
						if ( pEchoChanEntry->usExtraSinTsiDependencyCnt == 1 )
						{
							/* Release the extra TSI entry.*/
							ulResult = Oct6100ApiReleaseTsiMemEntry( f_pApiInstance, pEchoChanEntry->usExtraSinTsiMemIndex );
							if ( ulResult != cOCT6100_ERR_OK )
							{
								return ulResult;
							}
						}
					}
				}
			}
		}
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiBridgeEventRemove

Description:    Remove the event from the global event list of the chip and 
				update the bridge and channel structures.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.
f_pConfBridgeRemove			Pointer to a conference bridge channel remove structure.
f_usBridgeIndex				Index of the current bridge in the API list.
f_usChanIndex				Index of the current channel in the API list.
f_fFlexibleConfBridge		If this is a flexible conference bridge.
f_usLoadEventIndex			Allocated entry for the Load event of the channel.
f_usSubStoreEventIndex		Allocated entry for the substract and store event of the channel.
f_usCopyEventIndex			Allocated entry for the copy event of the channel.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiBridgeEventRemove
UINT32 Oct6100ApiBridgeEventRemove (
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_CONF_BRIDGE_CHAN_REMOVE	f_pConfBridgeRemove,
				IN		UINT16								f_usBridgeIndex, 
				IN		UINT16								f_usChanIndex,
				IN		UINT8								f_fFlexibleConfBridge,
				IN		UINT16								f_usLoadEventIndex,
				IN		UINT16								f_usSubStoreEventIndex,
				IN		UINT16								f_usCopyEventIndex,
				IN		UINT8								f_fTap )
{
	tPOCT6100_API_CONF_BRIDGE		pBridgeEntry;

	tPOCT6100_API_MIXER_EVENT		pLoadEventEntry;
	tPOCT6100_API_MIXER_EVENT		pSubStoreEventEntry;
	tPOCT6100_API_MIXER_EVENT		pCopyEventEntry = NULL;
	tPOCT6100_API_MIXER_EVENT		pTempEntry;

	tPOCT6100_API_CHANNEL			pEchoChanEntry;
	tPOCT6100_API_CHANNEL			pTempEchoChanEntry;

	tPOCT6100_SHARED_INFO			pSharedInfo;
	tOCT6100_WRITE_PARAMS			WriteParams;
	tOCT6100_READ_PARAMS			ReadParams;

	UINT32	ulResult;
	UINT16	usPreviousEventIndex;
	UINT16	usTempEventIndex;
	UINT32	ulLoopCount = 0;
	UINT16	usReadData;
	UINT16	usChannelIndex;
	UINT32	i;

	BOOL	fRemoveSinCopy = FALSE;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;
	
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;
	
	mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( pSharedInfo, pBridgeEntry, f_usBridgeIndex );
	
	/* If no client on the bridge, and the remove all option is specified, return here. */
	if ( ( pBridgeEntry->usNumClients == 0 ) && ( f_pConfBridgeRemove->fRemoveAll == TRUE ) )
		return cOCT6100_ERR_OK;

	/* Make sure the dominant speaker feature is disabled first. */
	if ( pBridgeEntry->fDominantSpeakerSet == TRUE )
	{
		/* If all channels are to be removed or if the dominant speaker is the current channel to be removed. */
		if ( ( f_pConfBridgeRemove->fRemoveAll == TRUE )
			|| ( ( f_pConfBridgeRemove->fRemoveAll == FALSE ) && ( pBridgeEntry->usDominantSpeakerChanIndex == f_usChanIndex ) ) )
		{
			/* Disable on all channels part of this conference. */

			/* Search through the list of API channel entry for the ones on to this bridge. */
			for ( usChannelIndex = 0; usChannelIndex < pSharedInfo->ChipConfig.usMaxChannels; usChannelIndex++ )
			{
				mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pTempEchoChanEntry, usChannelIndex );
				
				if ( pTempEchoChanEntry->fReserved == TRUE )
				{
					if ( pTempEchoChanEntry->usBridgeIndex == f_usBridgeIndex )
					{
						ulResult = Oct6100ApiBridgeSetDominantSpeaker( f_pApiInstance, usChannelIndex, cOCT6100_CONF_DOMINANT_SPEAKER_UNASSIGNED );
						if ( ulResult != cOCT6100_ERR_OK )
							return ulResult;
					}
				}
			}

			/* Save this in the conference bridge structure. */
			pBridgeEntry->fDominantSpeakerSet = FALSE;
			pBridgeEntry->usDominantSpeakerChanIndex = cOCT6100_INVALID_INDEX;
		}
		else
		{
			/* Only disable this current channel. */
			ulResult = Oct6100ApiBridgeSetDominantSpeaker( f_pApiInstance, f_usChanIndex, cOCT6100_CONF_DOMINANT_SPEAKER_UNASSIGNED );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	}

	if ( f_fFlexibleConfBridge == TRUE )
	{
		tPOCT6100_API_FLEX_CONF_PARTICIPANT pParticipant;
		tPOCT6100_API_FLEX_CONF_PARTICIPANT pTempParticipant;
		UINT16	ausMutePortChannelIndexes[ cOCT6100_MAX_FLEX_CONF_PARTICIPANTS_PER_BRIDGE ];
		UINT32	ulMutePortChannelIndex;

		for( ulMutePortChannelIndex = 0; ulMutePortChannelIndex < cOCT6100_MAX_FLEX_CONF_PARTICIPANTS_PER_BRIDGE; ulMutePortChannelIndex ++ )
			ausMutePortChannelIndexes[ ulMutePortChannelIndex ] = cOCT6100_INVALID_INDEX;

		if ( f_pConfBridgeRemove->fRemoveAll == FALSE )
		{
			/* The channel index is valid. */
			mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChanEntry, f_usChanIndex );
			mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pParticipant, pEchoChanEntry->usFlexConfParticipantIndex );

			/* Search through the list of API channel entry for the ones on to this bridge. */
			for ( usChannelIndex = 0; usChannelIndex < pSharedInfo->ChipConfig.usMaxChannels; usChannelIndex++ )
			{
				mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pTempEchoChanEntry, usChannelIndex );

				if ( ( usChannelIndex != f_usChanIndex ) && ( pTempEchoChanEntry->fReserved == TRUE ) )
				{
					if ( pTempEchoChanEntry->usBridgeIndex == f_usBridgeIndex )
					{
						mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pTempParticipant, pTempEchoChanEntry->usFlexConfParticipantIndex );

						/* Check if we can hear this participant. */
						if ( ( ( pParticipant->ulListenerMask & ( 0x1 << pTempParticipant->ulListenerMaskIndex ) ) == 0x0 )
							&& ( pParticipant->fFlexibleMixerCreated == TRUE ) 
							&& ( pTempEchoChanEntry->fMute == FALSE ) )
						{
							/* First update the current channel's mixer. */
							ulResult = Oct6100ApiBridgeRemoveParticipantFromChannel(
														f_pApiInstance,
														f_usBridgeIndex,
														usChannelIndex,
														f_usChanIndex,
														TRUE );
							if ( ulResult != cOCT6100_ERR_OK )
								return ulResult;
						}

						/* Check if this participant can hear us. */
						if ( ( ( pTempParticipant->ulListenerMask & ( 0x1 << pParticipant->ulListenerMaskIndex ) ) == 0x0 )
							&& ( pTempParticipant->fFlexibleMixerCreated == TRUE ) 
							&& ( pEchoChanEntry->fMute == FALSE ) )
						{
							/* Then update this channel's mixer. */
							ulResult = Oct6100ApiBridgeRemoveParticipantFromChannel(
														f_pApiInstance,
														f_usBridgeIndex,
														f_usChanIndex,
														usChannelIndex,
														TRUE );
							if ( ulResult != cOCT6100_ERR_OK )
								return ulResult;

							/* Remember to mute the port on this channel. */
							for( ulMutePortChannelIndex = 0; ulMutePortChannelIndex < cOCT6100_MAX_FLEX_CONF_PARTICIPANTS_PER_BRIDGE; ulMutePortChannelIndex ++ )
							{
								if ( ausMutePortChannelIndexes[ ulMutePortChannelIndex ] == usChannelIndex )
								{
									break;
								}
								else if ( ausMutePortChannelIndexes[ ulMutePortChannelIndex ] == cOCT6100_INVALID_INDEX )
								{
									ausMutePortChannelIndexes[ ulMutePortChannelIndex ] = usChannelIndex;
									break;
								}
							}
						}
					}
				}
			}

			/* Check if must manually clear the Sin copy event. */
			if ( ( pEchoChanEntry->usSinCopyEventIndex != cOCT6100_INVALID_INDEX )
				&& ( pEchoChanEntry->fCopyEventCreated == TRUE ) )
			{
				/* Transform event into no-operation. */
				WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pEchoChanEntry->usSinCopyEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
				WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_NO_OP;

				mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;

				/* Now remove the copy event from the event list. */
				ulResult = Oct6100ApiMixerEventRemove( f_pApiInstance, pEchoChanEntry->usSinCopyEventIndex, cOCT6100_EVENT_TYPE_SIN_COPY );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;

				pEchoChanEntry->fCopyEventCreated = FALSE;
			}

			/* Release an entry for the participant. */
			ulResult = Oct6100ApiReleaseFlexConfParticipantEntry( f_pApiInstance, pEchoChanEntry->usFlexConfParticipantIndex );
			if ( ulResult != cOCT6100_ERR_OK )
			{
				return ulResult;
			}

			/*=======================================================================*/
			/* Update the event and channel API structure */
			pEchoChanEntry->usFlexConfParticipantIndex = cOCT6100_INVALID_INDEX;
			pEchoChanEntry->usBridgeIndex = cOCT6100_INVALID_INDEX;
			pEchoChanEntry->usLoadEventIndex = cOCT6100_INVALID_INDEX;
			pEchoChanEntry->usSubStoreEventIndex = cOCT6100_INVALID_INDEX;
			pEchoChanEntry->usSinCopyEventIndex = cOCT6100_INVALID_INDEX;

			/* Indicate that the extra SIN TSI is not needed anymore by the mixer. */
			if ( pEchoChanEntry->usExtraSinTsiDependencyCnt == 1 )
			{
				pEchoChanEntry->usExtraSinTsiDependencyCnt--;
				pEchoChanEntry->usExtraSinTsiMemIndex = cOCT6100_INVALID_INDEX;
			}
			else
			{
				/* Decrement the dependency count, but do not clear the mem index. */
				pEchoChanEntry->usExtraSinTsiDependencyCnt--;
			}
			
			/* Indicate that the extra RIN TSI is not needed anymore by the mixer. */
			if ( pEchoChanEntry->usExtraRinTsiDependencyCnt == 1 )
			{
				pEchoChanEntry->usExtraRinTsiDependencyCnt--;
				pEchoChanEntry->usExtraRinTsiMemIndex = cOCT6100_INVALID_INDEX;
			}

			/* Update the chip stats structure. */
			pSharedInfo->ChipStats.usNumEcChanUsingMixer--;

			pBridgeEntry->usNumClients--;

			/* For sure we have to mute the ports of this channel to be removed. */
			ulResult = Oct6100ApiMutePorts( 
									f_pApiInstance, 
									f_usChanIndex, 
									pEchoChanEntry->usRinTsstIndex, 
									pEchoChanEntry->usSinTsstIndex,
									FALSE );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Travel through the channels that were heard by the participant removed and check if their Rin port must be muted. */
			for( ulMutePortChannelIndex = 0; ulMutePortChannelIndex < cOCT6100_MAX_FLEX_CONF_PARTICIPANTS_PER_BRIDGE; ulMutePortChannelIndex ++ )
			{
				if ( ausMutePortChannelIndexes[ ulMutePortChannelIndex ] != cOCT6100_INVALID_INDEX )
				{
					mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pTempEchoChanEntry, ausMutePortChannelIndexes[ ulMutePortChannelIndex ] );

					mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pTempParticipant, pTempEchoChanEntry->usFlexConfParticipantIndex );
					
					if ( pTempParticipant->fFlexibleMixerCreated == FALSE )
					{
						/* Check if the Rin port must be muted on this channel. */
						ulResult = Oct6100ApiMutePorts( 
												f_pApiInstance, 
												ausMutePortChannelIndexes[ ulMutePortChannelIndex ], 
												pTempEchoChanEntry->usRinTsstIndex, 
												pTempEchoChanEntry->usSinTsstIndex,
												FALSE );
						if ( ulResult != cOCT6100_ERR_OK )
							return ulResult;
					}
				}
				else /* if ( ausMutePortChannelIndexes[ ulMutePortChannelIndex ] == cOCT6100_INVALID_INDEX ) */
				{
					/* No more channels to check for muting. */
					break;
				}
			}
		}
		else /* if ( f_pConfBridgeRemove->fRemoveAll == TRUE ) */
		{
			UINT16 usMainChannelIndex;

			for ( usMainChannelIndex = 0 ; usMainChannelIndex < pSharedInfo->ChipConfig.usMaxChannels ; usMainChannelIndex++ )
			{
				mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChanEntry, usMainChannelIndex );

				/* If this channel is on the bridge we are closing all the channels. */
				if ( ( pEchoChanEntry->fReserved == TRUE ) && ( pEchoChanEntry->usBridgeIndex == f_usBridgeIndex ) )
				{
					/* Remember to mute the port on this channel. */
					for( ulMutePortChannelIndex = 0; ulMutePortChannelIndex < cOCT6100_MAX_FLEX_CONF_PARTICIPANTS_PER_BRIDGE; ulMutePortChannelIndex ++ )
					{
						if ( ausMutePortChannelIndexes[ ulMutePortChannelIndex ] == usMainChannelIndex )
						{
							break;
						}
						else if ( ausMutePortChannelIndexes[ ulMutePortChannelIndex ] == cOCT6100_INVALID_INDEX )
						{
							ausMutePortChannelIndexes[ ulMutePortChannelIndex ] = usMainChannelIndex;
							break;
						}
					}

					mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pParticipant, pEchoChanEntry->usFlexConfParticipantIndex );

					/* Search through the list of API channel entry for the ones on to this bridge. */
					for ( usChannelIndex = (UINT16)( usMainChannelIndex + 1 ); usChannelIndex < pSharedInfo->ChipConfig.usMaxChannels; usChannelIndex++ )
					{
						mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pTempEchoChanEntry, usChannelIndex );
						if ( pTempEchoChanEntry->fReserved == TRUE )
						{
							if ( pTempEchoChanEntry->usBridgeIndex == f_usBridgeIndex )
							{
								mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pTempParticipant, pTempEchoChanEntry->usFlexConfParticipantIndex );

								/* Everyone that we can hear must be removed. */
								if ( ( ( pParticipant->ulListenerMask & ( 0x1 << pTempParticipant->ulListenerMaskIndex ) ) == 0x0 )
									&& ( pParticipant->fFlexibleMixerCreated == TRUE ) 
									&& ( pTempEchoChanEntry->fMute == FALSE ) )
								{
									/* First update the current channel's mixer. */
									ulResult = Oct6100ApiBridgeRemoveParticipantFromChannel(
																f_pApiInstance,
																f_usBridgeIndex,
																usChannelIndex,
																usMainChannelIndex,
																TRUE );
									if ( ulResult != cOCT6100_ERR_OK )
										return ulResult;
								}

								/* Check if this participant can hear us. */
								if ( ( ( pTempParticipant->ulListenerMask & ( 0x1 << pParticipant->ulListenerMaskIndex ) ) == 0x0 )
									&& ( pTempParticipant->fFlexibleMixerCreated == TRUE ) 
									&& ( pEchoChanEntry->fMute == FALSE ) )
								{
									/* Then update this channel's mixer. */
									ulResult = Oct6100ApiBridgeRemoveParticipantFromChannel(
																f_pApiInstance,
																f_usBridgeIndex,
																usMainChannelIndex,
																usChannelIndex,
																TRUE );
									if ( ulResult != cOCT6100_ERR_OK )
										return ulResult;
								}
							}
						}
					}

					/* Check if must manually clear the Sin copy event. */
					if ( ( pEchoChanEntry->usSinCopyEventIndex != cOCT6100_INVALID_INDEX )
						&& ( pEchoChanEntry->fCopyEventCreated == TRUE ) )
					{
						/* Transform event into no-operation. */
						WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pEchoChanEntry->usSinCopyEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
						WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_NO_OP;

						mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
						if ( ulResult != cOCT6100_ERR_OK )
							return ulResult;

						/* Now remove the copy event from the event list. */
						ulResult = Oct6100ApiMixerEventRemove( f_pApiInstance, pEchoChanEntry->usSinCopyEventIndex, cOCT6100_EVENT_TYPE_SIN_COPY );
						if ( ulResult != cOCT6100_ERR_OK )
							return ulResult;

						pEchoChanEntry->fCopyEventCreated = FALSE;
					}

					/* Release an entry for the participant. */
					ulResult = Oct6100ApiReleaseFlexConfParticipantEntry( f_pApiInstance, pEchoChanEntry->usFlexConfParticipantIndex );
					if ( ulResult != cOCT6100_ERR_OK )
					{
						return ulResult;
					}

					/*=======================================================================*/
					/* Update the event and channel API structure */

					pEchoChanEntry->usBridgeIndex = cOCT6100_INVALID_INDEX;

					pEchoChanEntry->usLoadEventIndex = cOCT6100_INVALID_INDEX;
					pEchoChanEntry->usSubStoreEventIndex = cOCT6100_INVALID_INDEX;
					pEchoChanEntry->usSinCopyEventIndex = cOCT6100_INVALID_INDEX;

					/* Indicate that the Extra SIN TSI is not needed anymore by the mixer. */
					if ( pEchoChanEntry->usExtraSinTsiDependencyCnt == 1 )
					{
						pEchoChanEntry->usExtraSinTsiDependencyCnt--;
						pEchoChanEntry->usExtraSinTsiMemIndex = cOCT6100_INVALID_INDEX;
					}
					else
					{
						/* Decrement the dependency count, but do not clear the mem index. */
						pEchoChanEntry->usExtraSinTsiDependencyCnt--;
					}
					
					/* Indicate that the Extra RIN TSI is not needed anymore by the mixer. */
					if ( pEchoChanEntry->usExtraRinTsiDependencyCnt == 1 )
					{
						pEchoChanEntry->usExtraRinTsiDependencyCnt--;
						pEchoChanEntry->usExtraRinTsiMemIndex = cOCT6100_INVALID_INDEX;
					}

					/* Update the chip stats structure. */
					pSharedInfo->ChipStats.usNumEcChanUsingMixer--;
				}
			}

			/* Travel through the channels that were heard by the participant removed and check if their Rin port must be muted. */
			for( ulMutePortChannelIndex = 0; ulMutePortChannelIndex < cOCT6100_MAX_FLEX_CONF_PARTICIPANTS_PER_BRIDGE; ulMutePortChannelIndex ++ )
			{
				if ( ausMutePortChannelIndexes[ ulMutePortChannelIndex ] != cOCT6100_INVALID_INDEX )
				{
					mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pTempEchoChanEntry, ausMutePortChannelIndexes[ ulMutePortChannelIndex ] );

					mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pTempParticipant, pTempEchoChanEntry->usFlexConfParticipantIndex );
					
					if ( pTempParticipant->fFlexibleMixerCreated == FALSE )
					{
						/* Check if the Rin port must be muted on this channel. */
						ulResult = Oct6100ApiMutePorts( 
												f_pApiInstance, 
												ausMutePortChannelIndexes[ ulMutePortChannelIndex ], 
												pTempEchoChanEntry->usRinTsstIndex, 
												pTempEchoChanEntry->usSinTsstIndex,
												FALSE );
						if ( ulResult != cOCT6100_ERR_OK )
							return ulResult;
					}
				}
				else /* if ( ausMutePortChannelIndexes[ ulMutePortChannelIndex ] == cOCT6100_INVALID_INDEX ) */
				{
					/* No more channels to check for muting. */
					break;
				}

				/* Clear the flexible conf bridge participant index. */
				pTempEchoChanEntry->usFlexConfParticipantIndex = cOCT6100_INVALID_INDEX;
			}

			/* No more clients on bridge. */
			pBridgeEntry->usNumClients = 0;
		}
	}
	else /* if ( f_fFlexibleConfBridge == FALSE ) */
	{
		if ( f_pConfBridgeRemove->fRemoveAll == FALSE )
		{
			/* The channel index is valid. */
			mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChanEntry, f_usChanIndex );

			if ( f_fTap == TRUE )
			{
				mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( pSharedInfo, pBridgeEntry, pEchoChanEntry->usTapBridgeIndex );
			}

			/* Get a pointer to the event entry. */
			if ( f_usCopyEventIndex != cOCT6100_INVALID_INDEX )
				mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pCopyEventEntry, f_usCopyEventIndex );
			mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pSubStoreEventEntry, f_usSubStoreEventIndex );
			mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pLoadEventEntry, f_usLoadEventIndex );

			/*=======================================================================*/
			/* Check if have to modify the silence load event. */

			if ( pBridgeEntry->usNumClients != 1 )
			{
				if ( pBridgeEntry->usSilenceLoadEventPtr != cOCT6100_INVALID_INDEX )
				{
					if ( pBridgeEntry->usSilenceLoadEventPtr == f_usLoadEventIndex )
					{
						/* Make sure the next event becomes the silence event. */
						WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pLoadEventEntry->usNextEventPtr * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );

						WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_LOAD;
						WriteParams.usWriteData |= 1534; /* TSI index 1534 reserved for silence */

						mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
						if ( ulResult != cOCT6100_ERR_OK )
							return ulResult;
						
						/* Update the software model to remember the silence load. */
						pBridgeEntry->usSilenceLoadEventPtr = pLoadEventEntry->usNextEventPtr;
					}
					else
					{
						/* Somebody else is the silence event, no need to worry. */
					}
				}
			}

			/*=======================================================================*/


			/*=======================================================================*/
			/* Clear the Load event. */

			/* First verify if the event to be removed was a load event. */
			if ( f_usLoadEventIndex == pBridgeEntry->usLoadIndex )
			{
				/* Change the next entry if one is present to a load event to keep the bridge alive. */
				if ( pBridgeEntry->usNumClients == 1 )
				{
					/* There is no other entry on the bridge, no need to search for an Accumulate event. */
					pBridgeEntry->usLoadIndex = cOCT6100_INVALID_INDEX;

					/* Clear the silence event, for sure it's invalid. */
					pBridgeEntry->usSilenceLoadEventPtr = cOCT6100_INVALID_INDEX;
				}
				else
				{
					/* Search for an accumulate event to tranform into a Load event. */
					usTempEventIndex = pLoadEventEntry->usNextEventPtr;
					ulLoopCount = 0;

					/* Find the copy entry before the entry to remove. */
					mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEntry, usTempEventIndex );

					while( pTempEntry->usEventType != cOCT6100_MIXER_CONTROL_MEM_SUB_STORE && 
						   pTempEntry->usEventType != cOCT6100_MIXER_CONTROL_MEM_STORE )
					{
						if ( pTempEntry->usEventType == cOCT6100_MIXER_CONTROL_MEM_ACCUMULATE )
						{
							/* Change this entry into a load event. */
							ReadParams.ulReadAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( usTempEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
							mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
							if ( ulResult != cOCT6100_ERR_OK )
								return ulResult;

							WriteParams.ulWriteAddress = ReadParams.ulReadAddress;
							WriteParams.usWriteData = (UINT16)(( usReadData & 0x1FFF ) | cOCT6100_MIXER_CONTROL_MEM_LOAD);

							mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
							if ( ulResult != cOCT6100_ERR_OK )
								return ulResult;
							
							/* Set this entry as the load index. */
							pBridgeEntry->usLoadIndex = usTempEventIndex;

							/* Update the software model. */
							pTempEntry->usEventType = cOCT6100_MIXER_CONTROL_MEM_LOAD;

							/* Stop searching. */
							break;
						}
						
						/* Go to the next entry into the list. */
						usTempEventIndex = pTempEntry->usNextEventPtr;
						mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEntry, usTempEventIndex );

						ulLoopCount++;
						if ( ulLoopCount == cOCT6100_MAX_LOOP )
							return cOCT6100_ERR_FATAL_9B;
					}
				}
			}
			
			WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usLoadEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
			WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_NO_OP;

			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/*=======================================================================*/
			
			/*=======================================================================*/
			/* Clear the substract and store event. */
			WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usSubStoreEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
			WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_NO_OP;
			
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/*=======================================================================*/

			/*=======================================================================*/
			/* Clear the Copy event - if needed. */

			if ( f_usCopyEventIndex != cOCT6100_INVALID_INDEX )
			{
				/* Transform event into no-operation. */
				WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usCopyEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
				WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_NO_OP;

				mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;
				
				if ( f_fTap == FALSE )
				{
					/* Set remove Sin copy event flag to remove the event from the mixer's list. */
					fRemoveSinCopy = TRUE;

					/* Clear the copy event created flag. */
					pEchoChanEntry->fCopyEventCreated = FALSE;
				}
			}

			/*=======================================================================*/
		
			
			/*=======================================================================*/
			/* Now remove the event from the event list. */
			
			/* Look for the entry that is pointing at the first entry of our bridge. */
			if ( f_fTap == FALSE )
			{
				ulResult = Oct6100ApiGetPrevLastSubStoreEvent( f_pApiInstance, f_usBridgeIndex, pBridgeEntry->usFirstLoadEventPtr, &usPreviousEventIndex );
			}
			else
			{
				ulResult = Oct6100ApiGetPrevLastSubStoreEvent( f_pApiInstance, pEchoChanEntry->usTapBridgeIndex, pBridgeEntry->usFirstLoadEventPtr, &usPreviousEventIndex );
			}
			
			if ( ulResult != cOCT6100_ERR_OK )
			{
				/* If the entry was not found, we now check for the Sout copy event section/list. */
				if ( ulResult == cOCT6100_ERR_CONF_MIXER_EVENT_NOT_FOUND )
				{
					if ( pSharedInfo->MixerInfo.usLastSoutCopyEventPtr == cOCT6100_INVALID_INDEX )
					{
						/* No Sout copy, it has to be the head node. */
						usPreviousEventIndex = cOCT6100_MIXER_HEAD_NODE;
					}
					else
					{
						/* Use the last Sout copy event. */
						usPreviousEventIndex = pSharedInfo->MixerInfo.usLastSoutCopyEventPtr;
					}
				}
				else
				{
					return cOCT6100_ERR_FATAL_27;
				}
			}

			if ( pBridgeEntry->usNumClients == 1 )
			{
				/* An entry was found, now, modify it's value. */
				mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEntry, usPreviousEventIndex );

				/* Now modify the previous last Sub Store event from another bridge. */
				pTempEntry->usNextEventPtr = pSubStoreEventEntry->usNextEventPtr;

				/*=======================================================================*/
				/* Modify the last node of the previous bridge to point to the next bridge. */
				WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( usPreviousEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
				WriteParams.ulWriteAddress += 4;

				WriteParams.usWriteData = (UINT16)( pTempEntry->usNextEventPtr );
				
				mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;

				/*=======================================================================*/
				
				/* Set the event pointer info in the bridge stucture. */
				pBridgeEntry->usFirstLoadEventPtr = cOCT6100_INVALID_INDEX;
				pBridgeEntry->usFirstSubStoreEventPtr = cOCT6100_INVALID_INDEX;
				pBridgeEntry->usLastSubStoreEventPtr = cOCT6100_INVALID_INDEX;

				/*=======================================================================*/
				/* Update the global mixer pointers. */
				if ( pSharedInfo->MixerInfo.usFirstBridgeEventPtr == f_usLoadEventIndex &&
					 pSharedInfo->MixerInfo.usLastBridgeEventPtr  == f_usSubStoreEventIndex )
				{
					/* There is no more bridge entry in the mixer link list. */
					pSharedInfo->MixerInfo.usFirstBridgeEventPtr = cOCT6100_INVALID_INDEX;
					pSharedInfo->MixerInfo.usLastBridgeEventPtr  = cOCT6100_INVALID_INDEX;
				}
				else if ( pSharedInfo->MixerInfo.usFirstBridgeEventPtr == f_usLoadEventIndex )
				{
					pSharedInfo->MixerInfo.usFirstBridgeEventPtr = pSubStoreEventEntry->usNextEventPtr;
				}
				else if ( pSharedInfo->MixerInfo.usLastBridgeEventPtr == f_usSubStoreEventIndex )
				{
					pSharedInfo->MixerInfo.usLastBridgeEventPtr = usPreviousEventIndex;
				}
				/*=======================================================================*/

				if ( f_fTap == TRUE )
				{
					/* The channel being tapped is not tapped anymore.  */
					/* There is no direct way of finding the tap, so loop through all channels and find the */
					/* tapped channel index. */
					for ( usChannelIndex = 0; usChannelIndex < pSharedInfo->ChipConfig.usMaxChannels; usChannelIndex++ )
					{
						mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pTempEchoChanEntry, usChannelIndex );

						if ( pTempEchoChanEntry->usTapChanIndex == f_usChanIndex )
						{
							tPOCT6100_API_CONF_BRIDGE	pTempBridgeEntry;

							pTempEchoChanEntry->fBeingTapped = FALSE;
							pTempEchoChanEntry->usTapChanIndex = cOCT6100_INVALID_INDEX;

							mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( pSharedInfo, pTempBridgeEntry, f_usBridgeIndex );

							pTempBridgeEntry->usNumTappedClients--;
							
							/* Re-assign Rin TSST for tapped channel. */
							if ( pTempEchoChanEntry->usRinTsstIndex != cOCT6100_INVALID_INDEX )
							{
								ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
																				  pTempEchoChanEntry->usRinTsstIndex,
																				  pTempEchoChanEntry->usRinRoutTsiMemIndex,
																				  pTempEchoChanEntry->TdmConfig.byRinPcmLaw );
								if ( ulResult != cOCT6100_ERR_OK )
									return ulResult;
							}

							break;
						}
					}

					/* Check if our model is broken. */
					if ( usChannelIndex == pSharedInfo->ChipConfig.usMaxChannels )
						return cOCT6100_ERR_FATAL_D3;
				}
			}
			else /* pBridgeEntry->usNumClients > 1 */
			{
				if ( pBridgeEntry->usFirstLoadEventPtr != f_usLoadEventIndex )
				{
					/* Now find the load entry of this bridge pointing at this load event */
					ulResult = Oct6100ApiGetPreviousEvent( f_pApiInstance, pBridgeEntry->usFirstLoadEventPtr, f_usLoadEventIndex, 0, &usPreviousEventIndex );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;
				}

				/* Remove the load event to the list. */
				mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEntry, usPreviousEventIndex );
				
				/* Now modify the previous last Sub Store event from another bridge. */
				pTempEntry->usNextEventPtr = pLoadEventEntry->usNextEventPtr;

				/*=======================================================================*/
				/* Modify the previous node. */
				WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( usPreviousEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
				WriteParams.ulWriteAddress += 4;

				WriteParams.usWriteData = (UINT16)( pTempEntry->usNextEventPtr );
				
				mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;

				/*=======================================================================*/

				/* Now find the last load entry of this bridge ( the one pointing at the first sub-store event ). */
				if ( pBridgeEntry->usFirstSubStoreEventPtr == f_usSubStoreEventIndex )
				{
					/* Must start with the first load to get the entry before the first sub store. */
					ulResult = Oct6100ApiGetPreviousEvent( f_pApiInstance, pBridgeEntry->usFirstLoadEventPtr, f_usSubStoreEventIndex, 0, &usPreviousEventIndex );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;
				}
				else
				{
					/* Must start with the first load to get the entry before the first sub store. */
					ulResult = Oct6100ApiGetPreviousEvent( f_pApiInstance, pBridgeEntry->usFirstSubStoreEventPtr, f_usSubStoreEventIndex, 0, &usPreviousEventIndex );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;
				}

				mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEntry, usPreviousEventIndex );
				mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pSubStoreEventEntry, f_usSubStoreEventIndex );

				/* Now modify the last load event of the bridge. */
				pTempEntry->usNextEventPtr = pSubStoreEventEntry->usNextEventPtr;

				/*=======================================================================*/
				/* Modify the last node of the other bridge. */

				WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( usPreviousEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
				WriteParams.ulWriteAddress += 4;

				WriteParams.usWriteData = (UINT16)( pTempEntry->usNextEventPtr );
				
				mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;

				/*=======================================================================*/

				/*=======================================================================*/
				/* Update the bridge pointers. */

				if ( pBridgeEntry->usFirstLoadEventPtr == f_usLoadEventIndex )
					pBridgeEntry->usFirstLoadEventPtr = pLoadEventEntry->usNextEventPtr;

				if ( pBridgeEntry->usFirstSubStoreEventPtr == f_usSubStoreEventIndex )
					pBridgeEntry->usFirstSubStoreEventPtr = pSubStoreEventEntry->usNextEventPtr;

				if ( pBridgeEntry->usLastSubStoreEventPtr == f_usSubStoreEventIndex )
					pBridgeEntry->usLastSubStoreEventPtr = usPreviousEventIndex;
			
				/*=======================================================================*/


				/*=======================================================================*/
				/* Update the global mixer pointers. */

				if ( pSharedInfo->MixerInfo.usFirstBridgeEventPtr == f_usLoadEventIndex )
				{
					pSharedInfo->MixerInfo.usFirstBridgeEventPtr = pLoadEventEntry->usNextEventPtr;
				}

				if ( pSharedInfo->MixerInfo.usLastBridgeEventPtr == f_usSubStoreEventIndex )
				{
					pSharedInfo->MixerInfo.usLastBridgeEventPtr = usPreviousEventIndex;
				}
				/*=======================================================================*/

			}

			/* Check if must remove the Sin copy event from the event list. */
			if ( fRemoveSinCopy == TRUE )
			{
				/* Now remove the copy event from the event list. */
				ulResult = Oct6100ApiMixerEventRemove( f_pApiInstance, f_usCopyEventIndex, cOCT6100_EVENT_TYPE_SIN_COPY );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;
			}

			/* Get the channel. */
			mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChanEntry, f_usChanIndex );

			/* Reprogram the TSST entry correctly if the Extra SIN TSI entry was released. */
			if ( ( pEchoChanEntry->usExtraSinTsiDependencyCnt == 1 ) && ( f_fTap == FALSE ) )
			{
				if ( pEchoChanEntry->usSinTsstIndex != cOCT6100_INVALID_INDEX )
				{
					ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
																	  pEchoChanEntry->usSinTsstIndex,
																	  pEchoChanEntry->usSinSoutTsiMemIndex,
																	  pEchoChanEntry->TdmConfig.bySinPcmLaw );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;
				}

				/* If the silence TSI is loaded on this port, update with the original sin TSI. */
				if ( pEchoChanEntry->usSinSilenceEventIndex != cOCT6100_INVALID_INDEX )
				{
					WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pEchoChanEntry->usSinSilenceEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );

					WriteParams.ulWriteAddress += 2;
					WriteParams.usWriteData = pEchoChanEntry->usSinSoutTsiMemIndex;

					mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;
				}
			}
			/* Set the event entries as free. */
			pLoadEventEntry->fReserved		= FALSE;
			pLoadEventEntry->usEventType	= cOCT6100_INVALID_INDEX;
			pLoadEventEntry->usNextEventPtr = cOCT6100_INVALID_INDEX;

			pSubStoreEventEntry->fReserved		= FALSE;
			pSubStoreEventEntry->usEventType	= cOCT6100_INVALID_INDEX;
			pSubStoreEventEntry->usNextEventPtr = cOCT6100_INVALID_INDEX;

			if ( pCopyEventEntry != NULL )
			{
				pCopyEventEntry->fReserved		= FALSE;
				pCopyEventEntry->usEventType	= cOCT6100_INVALID_INDEX;
				pCopyEventEntry->usNextEventPtr = cOCT6100_INVALID_INDEX;
			}

			pBridgeEntry->usNumClients--;

			/*=======================================================================*/
			/* Update the event and channel API structure */
			pEchoChanEntry->usBridgeIndex = cOCT6100_INVALID_INDEX;
			pEchoChanEntry->usLoadEventIndex = cOCT6100_INVALID_INDEX;
			pEchoChanEntry->usSubStoreEventIndex = cOCT6100_INVALID_INDEX;
			pEchoChanEntry->usSinCopyEventIndex = cOCT6100_INVALID_INDEX;

			/* Indicate that the Extra SIN TSI is not needed anymore by the mixer. */
			if ( pEchoChanEntry->usExtraSinTsiDependencyCnt == 1 )
			{
				pEchoChanEntry->usExtraSinTsiDependencyCnt--;
				pEchoChanEntry->usExtraSinTsiMemIndex = cOCT6100_INVALID_INDEX;
			}
			else
			{
				/* Decrement the dependency count, but do not clear the mem index. */
				pEchoChanEntry->usExtraSinTsiDependencyCnt--;
			}

			/* Update the chip stats structure. */
			pSharedInfo->ChipStats.usNumEcChanUsingMixer--;

			if ( f_fTap == TRUE )
			{
				/* Can now close the bridge. */
				tOCT6100_CONF_BRIDGE_CLOSE	BridgeClose;

				Oct6100ConfBridgeCloseDef( &BridgeClose );

				BridgeClose.ulConfBridgeHndl = cOCT6100_HNDL_TAG_CONF_BRIDGE | (pBridgeEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | pEchoChanEntry->usTapBridgeIndex;

				ulResult = Oct6100ConfBridgeCloseSer( f_pApiInstance, &BridgeClose );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;

				pEchoChanEntry->usTapBridgeIndex = cOCT6100_INVALID_INDEX;
				pEchoChanEntry->fTap = FALSE;
			}

			/* Check if the Rin port must be muted. */
			ulResult = Oct6100ApiMutePorts( 
									f_pApiInstance, 
									f_usChanIndex, 
									pEchoChanEntry->usRinTsstIndex, 
									pEchoChanEntry->usSinTsstIndex,
									FALSE );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/*=======================================================================*/
		}
		else /* f_ulBridgeChanRemove->fRemoveAll == TRUE ) */
		{
			UINT16 usNextEventPtr;

			/* Save the next event pointer before invalidating everything. */
			mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pSubStoreEventEntry, pBridgeEntry->usLastSubStoreEventPtr );

			usNextEventPtr = pSubStoreEventEntry->usNextEventPtr;

			/* Search through the list of API channel entry for the ones on to the specified bridge. */
			for ( i = 0; i < pSharedInfo->ChipConfig.usMaxChannels; i++ )
			{
				mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChanEntry, i );
				
				if ( pEchoChanEntry->fReserved == TRUE )
				{
					if ( ( pEchoChanEntry->usBridgeIndex == f_usBridgeIndex ) && ( pEchoChanEntry->fTap == FALSE ) )
					{
						/* Check if we are being tapped.  If so, remove the channel that taps us from the conference. */
						/* The removal of the channel will make sure the Rin TSST is re-assigned. */
						if ( pEchoChanEntry->fBeingTapped == TRUE )
						{
							tOCT6100_CONF_BRIDGE_CHAN_REMOVE	ChanRemove;

							mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pTempEchoChanEntry, pEchoChanEntry->usTapChanIndex );

							ulResult = Oct6100ConfBridgeChanRemoveDef( &ChanRemove );
							if ( ulResult != cOCT6100_ERR_OK )
								return ulResult;
							
							ChanRemove.ulChannelHndl = cOCT6100_HNDL_TAG_CHANNEL | (pTempEchoChanEntry->byEntryOpenCnt << cOCT6100_ENTRY_OPEN_CNT_SHIFT) | pEchoChanEntry->usTapChanIndex;

							ulResult = Oct6100ConfBridgeChanRemoveSer( f_pApiInstance, &ChanRemove );
							if ( ulResult != cOCT6100_ERR_OK )
								return ulResult;
						}
					
						/*=======================================================================*/
						/* Clear the Load event. */
						WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pEchoChanEntry->usLoadEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
						WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_NO_OP;

						mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
						if ( ulResult != cOCT6100_ERR_OK )
							return ulResult;

						/*=======================================================================*/
						
						/*=======================================================================*/
						/* Clear the Substract and store event. */
						WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pEchoChanEntry->usSubStoreEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
						WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_NO_OP;
						
						mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
						if ( ulResult != cOCT6100_ERR_OK )
							return ulResult;
						/*=======================================================================*/

						/*=======================================================================*/
						/* Clear the SIN copy event.*/
						
						if ( pEchoChanEntry->usSinCopyEventIndex != cOCT6100_INVALID_INDEX )
						{
							/* Transform event into no-operation. */
							WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pEchoChanEntry->usSinCopyEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
							WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_NO_OP;
							
							mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
							if ( ulResult != cOCT6100_ERR_OK )
								return ulResult;

							/* Get a pointer to the event entry. */
							mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pCopyEventEntry, pEchoChanEntry->usSinCopyEventIndex );

							/* Update the next event pointer if required. */
							if ( usNextEventPtr == pEchoChanEntry->usSinCopyEventIndex )
								usNextEventPtr = pCopyEventEntry->usNextEventPtr;

							/* Now remove the copy event from the event list. */
							ulResult = Oct6100ApiMixerEventRemove( f_pApiInstance, pEchoChanEntry->usSinCopyEventIndex, cOCT6100_EVENT_TYPE_SIN_COPY );
							if ( ulResult != cOCT6100_ERR_OK )
								return ulResult;

							/* Clear the copy event created flag. */
							pEchoChanEntry->fCopyEventCreated = FALSE;
						}

						/*=======================================================================*/


						/*=======================================================================*/
						/* Update the event and channel API structure */

						/* Reprogram the TSST entry correctly if the Extra SIN TSI entry was released.*/
						if ( pEchoChanEntry->usExtraSinTsiDependencyCnt == 1 )
						{
							if ( pEchoChanEntry->usSinTsstIndex != cOCT6100_INVALID_INDEX )
							{
								ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
																				  pEchoChanEntry->usSinTsstIndex,
																				  pEchoChanEntry->usSinSoutTsiMemIndex,
																				  pEchoChanEntry->TdmConfig.bySinPcmLaw );
								if ( ulResult != cOCT6100_ERR_OK )
									return ulResult;
							}

							/* If the silence TSI is loaded on this port, update with the original Sin TSI. */
							if ( pEchoChanEntry->usSinSilenceEventIndex != cOCT6100_INVALID_INDEX )
							{
								WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pEchoChanEntry->usSinSilenceEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );

								WriteParams.ulWriteAddress += 2;
								WriteParams.usWriteData = pEchoChanEntry->usSinSoutTsiMemIndex;

								mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
								if ( ulResult != cOCT6100_ERR_OK )
									return ulResult;
							}
						}

						mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pLoadEventEntry, pEchoChanEntry->usLoadEventIndex );
						mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pSubStoreEventEntry, pEchoChanEntry->usSubStoreEventIndex );

						/* Set the event entries as free. */
						pLoadEventEntry->fReserved		= FALSE;
						pLoadEventEntry->usEventType	= cOCT6100_INVALID_EVENT;
						pLoadEventEntry->usNextEventPtr	= cOCT6100_INVALID_INDEX;

						pSubStoreEventEntry->fReserved		= FALSE;
						pSubStoreEventEntry->usEventType	= cOCT6100_INVALID_EVENT;
						pSubStoreEventEntry->usNextEventPtr	= cOCT6100_INVALID_INDEX;

						if ( pCopyEventEntry != NULL )
						{
							pCopyEventEntry->fReserved		= FALSE;
							pCopyEventEntry->usEventType	= cOCT6100_INVALID_EVENT;
							pCopyEventEntry->usNextEventPtr	= cOCT6100_INVALID_INDEX;
						}

						/* Indicate that the Extra SIN TSI is not needed anymore by the mixer. */
						if ( pEchoChanEntry->usExtraSinTsiDependencyCnt == 1 )
						{
							pEchoChanEntry->usExtraSinTsiDependencyCnt--;
							pEchoChanEntry->usExtraSinTsiMemIndex = cOCT6100_INVALID_INDEX;
						}
						else
						{
							/* Decrement the dependency count, but do not clear the mem index. */
							pEchoChanEntry->usExtraSinTsiDependencyCnt--;
						}

						/* Invalidate the channel entry. */
						pEchoChanEntry->usLoadEventIndex = cOCT6100_INVALID_INDEX;
						pEchoChanEntry->usSubStoreEventIndex = cOCT6100_INVALID_INDEX;
						pEchoChanEntry->usSinCopyEventIndex = cOCT6100_INVALID_INDEX;

						/* Update the chip stats structure. */
						pSharedInfo->ChipStats.usNumEcChanUsingMixer--;

						/*=======================================================================*/
					}
				}
			}
		
			ulResult = Oct6100ApiGetPrevLastSubStoreEvent( f_pApiInstance, f_usBridgeIndex, pBridgeEntry->usFirstLoadEventPtr, &usPreviousEventIndex );
			if ( ulResult != cOCT6100_ERR_OK )
			{
				if ( cOCT6100_ERR_CONF_MIXER_EVENT_NOT_FOUND == ulResult )
				{
					if ( pSharedInfo->MixerInfo.usLastSoutCopyEventPtr == cOCT6100_INVALID_INDEX )
					{
						usPreviousEventIndex = cOCT6100_MIXER_HEAD_NODE;
					}
					else
					{
						usPreviousEventIndex = pSharedInfo->MixerInfo.usLastSoutCopyEventPtr;
					}
				}
				else
				{
					return cOCT6100_ERR_FATAL_28;
				}
			}

			/* An Entry was found, now, modify it's value. */
			mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEntry, usPreviousEventIndex );

			/* Now modify the previous last Sub Store event from another bridge.*/
			/* It will now point at the next bridge, or copy events. */
			pTempEntry->usNextEventPtr = usNextEventPtr;

			/*=======================================================================*/
			/* Modify the last node of the other bridge. */
			WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( usPreviousEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
			WriteParams.ulWriteAddress += 4;

			WriteParams.usWriteData = pTempEntry->usNextEventPtr;
			
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
			/*=======================================================================*/
			
			/*=======================================================================*/
			/* Update the global mixer pointers. */
			if ( pSharedInfo->MixerInfo.usFirstBridgeEventPtr == pBridgeEntry->usFirstLoadEventPtr &&
				 pSharedInfo->MixerInfo.usLastBridgeEventPtr  == pBridgeEntry->usLastSubStoreEventPtr )
			{
				/* This bridge was the only one with event in the list. */
				pSharedInfo->MixerInfo.usFirstBridgeEventPtr = cOCT6100_INVALID_INDEX;
				pSharedInfo->MixerInfo.usLastBridgeEventPtr  = cOCT6100_INVALID_INDEX;
			}
			else if ( pSharedInfo->MixerInfo.usFirstBridgeEventPtr == pBridgeEntry->usFirstLoadEventPtr )
			{
				/* This bridge was the first bridge. */
				pSharedInfo->MixerInfo.usFirstBridgeEventPtr = usNextEventPtr;
			}
			else if ( pSharedInfo->MixerInfo.usLastBridgeEventPtr == pBridgeEntry->usLastSubStoreEventPtr )
			{
				/* This bridge was the last bridge.*/
				pSharedInfo->MixerInfo.usLastBridgeEventPtr = usPreviousEventIndex;
			}
			/*=======================================================================*/

			/* Set the event pointer info in the bridge stucture. */
			pBridgeEntry->usFirstLoadEventPtr = cOCT6100_INVALID_INDEX;
			pBridgeEntry->usFirstSubStoreEventPtr = cOCT6100_INVALID_INDEX;
			pBridgeEntry->usLastSubStoreEventPtr = cOCT6100_INVALID_INDEX;
			pBridgeEntry->usLoadIndex = cOCT6100_INVALID_INDEX;

			pBridgeEntry->usSilenceLoadEventPtr = cOCT6100_INVALID_INDEX;

			/* Set the number of clients to 0. */
			pBridgeEntry->usNumClients = 0;

			/* Search through the list of API channel entry for the ones on to the specified bridge. */
			for ( i = 0; i < pSharedInfo->ChipConfig.usMaxChannels; i++ )
			{
				mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChanEntry, i );
				
				if ( pEchoChanEntry->fReserved == TRUE )
				{
					if ( ( pEchoChanEntry->usBridgeIndex == f_usBridgeIndex ) && ( pEchoChanEntry->fTap == FALSE ) )
					{
						pEchoChanEntry->usBridgeIndex = cOCT6100_INVALID_INDEX;

						/* Check if the Rin port must be muted. */
						ulResult = Oct6100ApiMutePorts( 
												f_pApiInstance, 
												(UINT16)( i & 0xFFFF ), 
												pEchoChanEntry->usRinTsstIndex, 
												pEchoChanEntry->usSinTsstIndex,
												FALSE );
						if ( ulResult != cOCT6100_ERR_OK )
							return ulResult;
					}
				}
			}
		}
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiBridgeRemoveParticipantFromChannel

Description:    This will remove a flexible conference participant from
				a channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.
f_usBridgeIndex				Bridge index where this channel is located.
f_usSourceChannelIndex		Source channel to copy voice from.
f_usDestinationChannelIndex	Destination channel to store resulting voice to.
f_fRemovePermanently		Whether to remove permanently this participant.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiBridgeRemoveParticipantFromChannel
UINT32 Oct6100ApiBridgeRemoveParticipantFromChannel(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance, 
				IN		UINT16					f_usBridgeIndex,
				IN		UINT16					f_usSourceChannelIndex,
				IN		UINT16					f_usDestinationChannelIndex,
				IN		UINT8					f_fRemovePermanently )
{
	tPOCT6100_API_CONF_BRIDGE			pBridgeEntry;

	tPOCT6100_API_MIXER_EVENT			pLoadEventEntry;
	tPOCT6100_API_MIXER_EVENT			pStoreEventEntry;
	tPOCT6100_API_MIXER_EVENT			pCopyEventEntry;
	tPOCT6100_API_MIXER_EVENT			pTempEntry;
	tPOCT6100_API_MIXER_EVENT			pLoadTempEntry;
	tPOCT6100_API_MIXER_EVENT			pLastEventEntry;

	tPOCT6100_API_CHANNEL				pDestinationChanEntry;

	tPOCT6100_API_FLEX_CONF_PARTICIPANT	pDestinationParticipant;

	tPOCT6100_SHARED_INFO				pSharedInfo;
	tOCT6100_WRITE_PARAMS				WriteParams;
	tOCT6100_READ_PARAMS				ReadParams;

	UINT32								ulResult;
	UINT32								ulLoopCount;
	UINT16								usLoadOrAccumulateEventIndex;
	UINT16								usTempEventIndex;
	UINT16								usPreviousEventIndex;
	UINT16								usLastEventIndex;

	UINT16								usReadData;
	BOOL								fLastEvent = FALSE;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;
	
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;

	mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBridgeEntry, f_usBridgeIndex );

	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pDestinationChanEntry, f_usDestinationChannelIndex );
	mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( f_pApiInstance->pSharedInfo, pDestinationParticipant, pDestinationChanEntry->usFlexConfParticipantIndex );

	/* Check if the mixer has been created on this channel. */
	if ( pDestinationParticipant->fFlexibleMixerCreated == TRUE )
	{
		/*=======================================================================*/
		/* Clear the Load or Accumulate event.*/

		usTempEventIndex = pDestinationChanEntry->usLoadEventIndex;
		ulLoopCount = 0;

		/* Find the Load or Accumulate event entry. */
		mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pLoadEventEntry, usTempEventIndex );
		mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pStoreEventEntry, pDestinationChanEntry->usSubStoreEventIndex );
		mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEntry, usTempEventIndex );

		pLastEventEntry = pLoadEventEntry;
		usLastEventIndex = usTempEventIndex;

		while( pTempEntry->usEventType != cOCT6100_MIXER_CONTROL_MEM_SUB_STORE && 
			   pTempEntry->usEventType != cOCT6100_MIXER_CONTROL_MEM_STORE )
		{
			/* If this is the entry we are looking for. */
			if ( pTempEntry->usSourceChanIndex == f_usSourceChannelIndex )
			{
				/* Check if this is a Load or Accumulate event. */
				if ( pTempEntry->usEventType == cOCT6100_MIXER_CONTROL_MEM_LOAD )
				{
					/* This is the first entry.  Check if next entry is an accumulate. */
					pLoadTempEntry = pTempEntry;
					mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEntry, pTempEntry->usNextEventPtr );

					if ( pTempEntry->usEventType == cOCT6100_MIXER_CONTROL_MEM_ACCUMULATE )
					{
						/* Change this entry into a Load event. */
						ReadParams.ulReadAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pLoadTempEntry->usNextEventPtr * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
						mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
						if ( ulResult != cOCT6100_ERR_OK )
							return ulResult;

						WriteParams.ulWriteAddress = ReadParams.ulReadAddress;
						WriteParams.usWriteData = (UINT16)(( usReadData & 0x1FFF ) | cOCT6100_MIXER_CONTROL_MEM_LOAD);

						mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
						if ( ulResult != cOCT6100_ERR_OK )
							return ulResult;
						
						/* Update the channel information with this new load event. */
						pDestinationChanEntry->usLoadEventIndex = pLoadTempEntry->usNextEventPtr;

						/* Update the software model. */
						pTempEntry->usEventType = cOCT6100_MIXER_CONTROL_MEM_LOAD;

						/* Get the previous event. */
						ulResult = Oct6100ApiGetPreviousEvent( f_pApiInstance, cOCT6100_MIXER_HEAD_NODE, usTempEventIndex, 0, &usPreviousEventIndex );
						if ( ulResult != cOCT6100_ERR_OK )
							return ulResult;

						mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pLastEventEntry, usPreviousEventIndex );
						usLastEventIndex = usPreviousEventIndex;

						/* Stop searching. */
						break;
					}
					else if ( pTempEntry->usEventType == cOCT6100_MIXER_CONTROL_MEM_STORE )
					{
						/* Get back the event to remove. */
						mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEntry, usTempEventIndex );

						/* This is the only event on this channel so we can clear everything up. */
						fLastEvent = TRUE;

						/* Get the previous event. */
						ulResult = Oct6100ApiGetPreviousEvent( f_pApiInstance, cOCT6100_MIXER_HEAD_NODE, usTempEventIndex, 0, &usPreviousEventIndex );
						if ( ulResult != cOCT6100_ERR_OK )
							return ulResult;

						mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pLastEventEntry, usPreviousEventIndex );
						usLastEventIndex = usPreviousEventIndex;

						/* Stop searching. */
						break;
					}
					else
					{
						/* Software model is broken. */
						return cOCT6100_ERR_FATAL_C5;
					}
					
				}
				else if ( pTempEntry->usEventType == cOCT6100_MIXER_CONTROL_MEM_ACCUMULATE )
				{
					/* Simply remove the entry. */

					/* Get the previous event. */
					ulResult = Oct6100ApiGetPreviousEvent( f_pApiInstance, cOCT6100_MIXER_HEAD_NODE, usTempEventIndex, 0, &usPreviousEventIndex );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;

					mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pLastEventEntry, usPreviousEventIndex );
					usLastEventIndex = usPreviousEventIndex;

					/* Stop searching. */
					break;
				}
				else
				{
					/* Software model is broken. */
					return cOCT6100_ERR_FATAL_C6;
				}
			}

			/* Go to the next entry into the list. */
			usTempEventIndex = pTempEntry->usNextEventPtr;
			mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEntry, usTempEventIndex );

			ulLoopCount++;
			if ( ulLoopCount == cOCT6100_MAX_LOOP )
				return cOCT6100_ERR_FATAL_C8;
		}

		/* Check if we found what we were looking for. */
		if ( pTempEntry->usEventType == cOCT6100_MIXER_CONTROL_MEM_STORE 
			|| pTempEntry->usEventType == cOCT6100_MIXER_CONTROL_MEM_SUB_STORE )
		{
			/* Software model is broken. */
			return cOCT6100_ERR_FATAL_C7;
		}

		/*=======================================================================*/


		/*=======================================================================*/
		/* Clear the Store event - if needed. */

		if ( fLastEvent == TRUE )
		{
			WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pDestinationChanEntry->usSubStoreEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
			WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_NO_OP;
			
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}

		/*=======================================================================*/


		/*=======================================================================*/
		/* Clear the Load or Accumulate event. */

		WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( usTempEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
		WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_NO_OP;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Save this event index.  It's the Load or Accumulate we want to remove from the list later. */
		usLoadOrAccumulateEventIndex = usTempEventIndex;

		/*=======================================================================*/


		/*=======================================================================*/
		/* Clear the Copy event - if needed. */
		
		if ( ( fLastEvent == TRUE ) && ( pDestinationChanEntry->usSinCopyEventIndex != cOCT6100_INVALID_INDEX ) && ( f_fRemovePermanently == TRUE ) )
		{
			/* Transform event into no-operation. */
			WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pDestinationChanEntry->usSinCopyEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
			WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_NO_OP;

			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* The event remove from the list will be done below. */

			/* Clear the copy event created flag. */
			pDestinationChanEntry->fCopyEventCreated = FALSE;
		}

		/*=======================================================================*/


		/*=======================================================================*/
		/*=======================================================================*/
		/* Remove the events from the mixer event list.*/
		/*=======================================================================*/
		/*=======================================================================*/

		/*=======================================================================*/
		/* Remove the Load or Accumulate event from the event list. */
		
		if ( fLastEvent == FALSE )
		{
			/*=======================================================================*/
			/* Remove the Accumulate event from the event list. */

			/* We saved the Load or Accumulate event above.  We also saved the previous event.  Use those. */
			mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pLoadEventEntry, usLoadOrAccumulateEventIndex );
			
			/* Now modify the previous last event. */
			pLastEventEntry->usNextEventPtr = pLoadEventEntry->usNextEventPtr;

			/* Modify the previous node. */
			WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( usLastEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
			WriteParams.ulWriteAddress += 4;

			WriteParams.usWriteData = pLastEventEntry->usNextEventPtr;
			
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
			
			/* Check if this is the first load event on the bridge. */
			if ( pBridgeEntry->usFirstLoadEventPtr == usLoadOrAccumulateEventIndex )
			{
				pBridgeEntry->usFirstLoadEventPtr = pLoadEventEntry->usNextEventPtr;
			}

			/* Check if this was the first load of all bridges. */
			if ( pSharedInfo->MixerInfo.usFirstBridgeEventPtr == usLoadOrAccumulateEventIndex )
			{
				pSharedInfo->MixerInfo.usFirstBridgeEventPtr = pLoadEventEntry->usNextEventPtr;
			}

			/*=======================================================================*/
		}
		else /* if ( fLastEvent == TRUE ) */
		{
			/*=======================================================================*/
			/* Remove the Load event from the event list. */

			/* Look for the entry that is pointing at the first entry of our mixer. */
			ulResult = Oct6100ApiGetPreviousEvent( f_pApiInstance, cOCT6100_MIXER_HEAD_NODE, usLoadOrAccumulateEventIndex, 0, &usPreviousEventIndex );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* An Entry was found, now, modify it's value. */
			mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEntry, usPreviousEventIndex );

			/* Check if this is a Sout copy event. */
			if ( pTempEntry->usEventType == cOCT6100_MIXER_CONTROL_MEM_COPY )
			{
				/* No more previous bridges. */
			}

			/* Now modify the previous last Store or Sub-Store or Head-Node event from another bridge/channel. */
			pTempEntry->usNextEventPtr = pStoreEventEntry->usNextEventPtr;

			/*=======================================================================*/


			/*=======================================================================*/
			/* Modify the last node of the previous bridge/channel to point to the next bridge. */

			WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( usPreviousEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
			WriteParams.ulWriteAddress += 4;

			WriteParams.usWriteData = pTempEntry->usNextEventPtr;
			
			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/*=======================================================================*/
			
			
			/*=======================================================================*/
			/* Set the event pointer info in the bridge stucture. */

			if ( pBridgeEntry->usFirstLoadEventPtr == pDestinationChanEntry->usLoadEventIndex )
			{
				UINT16					usChannelIndex;
				tPOCT6100_API_CHANNEL	pTempEchoChanEntry;

				pBridgeEntry->usFirstSubStoreEventPtr = cOCT6100_INVALID_INDEX;
				pBridgeEntry->usFirstLoadEventPtr = cOCT6100_INVALID_INDEX;

				/* Find the next channel in this conference that could give us valid values. */
				for ( usChannelIndex = 0; usChannelIndex < pSharedInfo->ChipConfig.usMaxChannels; usChannelIndex++ )
				{
					mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pTempEchoChanEntry, usChannelIndex );

					if ( ( usChannelIndex != f_usDestinationChannelIndex ) && ( pTempEchoChanEntry->fReserved == TRUE ) )
					{
						if ( pTempEchoChanEntry->usBridgeIndex == f_usBridgeIndex )
						{
							tPOCT6100_API_FLEX_CONF_PARTICIPANT	pTempParticipant;

							mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( f_pApiInstance->pSharedInfo, pTempParticipant, pTempEchoChanEntry->usFlexConfParticipantIndex );

							if ( pTempParticipant->fFlexibleMixerCreated == TRUE )
							{
								pBridgeEntry->usFirstSubStoreEventPtr = pTempEchoChanEntry->usSubStoreEventIndex;
								pBridgeEntry->usFirstLoadEventPtr = pTempEchoChanEntry->usLoadEventIndex;
								break;
							}
						}
					}
				}
			}

			/* Reprogram the TSST entry correctly if the extra SIN TSI entry was released. */
			if ( ( pDestinationChanEntry->usExtraSinTsiDependencyCnt == 1 ) && ( f_fRemovePermanently == TRUE ) )
			{
				if ( pDestinationChanEntry->usSinTsstIndex != cOCT6100_INVALID_INDEX )
				{
					ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
																	  pDestinationChanEntry->usSinTsstIndex,
																	  pDestinationChanEntry->usSinSoutTsiMemIndex,
																	  pDestinationChanEntry->TdmConfig.bySinPcmLaw );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;
				}

				/* If the silence TSI is loaded on this port, update with the original sin TSI. */
				if ( pDestinationChanEntry->usSinSilenceEventIndex != cOCT6100_INVALID_INDEX )
				{
					WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pDestinationChanEntry->usSinSilenceEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );

					WriteParams.ulWriteAddress += 2;
					WriteParams.usWriteData = pDestinationChanEntry->usSinSoutTsiMemIndex;

					mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;
				}
			}

			/* Reprogram the TSST entry correctly if the extra RIN TSI entry was released. */
			if ( ( pDestinationChanEntry->usExtraRinTsiDependencyCnt == 1 ) && ( f_fRemovePermanently == TRUE ) )
			{
				if ( pDestinationChanEntry->usRinTsstIndex != cOCT6100_INVALID_INDEX )
				{
					ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
																	  pDestinationChanEntry->usRinTsstIndex,
																	  pDestinationChanEntry->usRinRoutTsiMemIndex,
																	  pDestinationChanEntry->TdmConfig.byRinPcmLaw );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;
				}
			}

			/*=======================================================================*/
			/* Update the global mixer pointers. */

			if ( pSharedInfo->MixerInfo.usFirstBridgeEventPtr == usLoadOrAccumulateEventIndex &&
				 pSharedInfo->MixerInfo.usLastBridgeEventPtr  == pDestinationChanEntry->usSubStoreEventIndex )
			{
				/* There is no more bridge entry in the mixer link list. */
				pSharedInfo->MixerInfo.usFirstBridgeEventPtr = cOCT6100_INVALID_INDEX;
				pSharedInfo->MixerInfo.usLastBridgeEventPtr  = cOCT6100_INVALID_INDEX;
			}
			else if ( pSharedInfo->MixerInfo.usFirstBridgeEventPtr == usLoadOrAccumulateEventIndex )
			{
				pSharedInfo->MixerInfo.usFirstBridgeEventPtr = pStoreEventEntry->usNextEventPtr;
			}
			else if ( pSharedInfo->MixerInfo.usLastBridgeEventPtr == pDestinationChanEntry->usSubStoreEventIndex )
			{
				pSharedInfo->MixerInfo.usLastBridgeEventPtr = usPreviousEventIndex;
			}

			/*=======================================================================*/


			/*=======================================================================*/
			/* Check if must remove the Sin copy event from the list. */

			if ( ( pDestinationChanEntry->usSinCopyEventIndex != cOCT6100_INVALID_INDEX ) && ( f_fRemovePermanently == TRUE ) )
			{
				/* Now remove the copy event from the event list. */
				ulResult = Oct6100ApiMixerEventRemove( f_pApiInstance, pDestinationChanEntry->usSinCopyEventIndex, cOCT6100_EVENT_TYPE_SIN_COPY );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;
			}

			/*=======================================================================*/



			/*=======================================================================*/

			if ( f_fRemovePermanently == TRUE )
			{
				/* Set the event entries as free. */
				pLoadEventEntry->fReserved		= FALSE;
				pLoadEventEntry->usEventType	= cOCT6100_INVALID_EVENT;
				pLoadEventEntry->usNextEventPtr = cOCT6100_INVALID_INDEX;

				pStoreEventEntry->fReserved		= FALSE;
				pStoreEventEntry->usEventType	= cOCT6100_INVALID_EVENT;
				pStoreEventEntry->usNextEventPtr = cOCT6100_INVALID_INDEX;

				if ( pDestinationChanEntry->usSinCopyEventIndex != cOCT6100_INVALID_INDEX )
				{
					mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pCopyEventEntry, pDestinationChanEntry->usSinCopyEventIndex );
					
					pCopyEventEntry->fReserved		= FALSE;
					pCopyEventEntry->usEventType	= cOCT6100_INVALID_EVENT;
					pCopyEventEntry->usNextEventPtr = cOCT6100_INVALID_INDEX;
				}
			}

			/* Flexible mixer for this channel not created anymore. */
			pDestinationParticipant->fFlexibleMixerCreated = FALSE;

			/*=======================================================================*/
		}

		/*=======================================================================*/
	}
	else /* if ( pDestinationChanEntry->fFlexibleMixerCreated == FALSE ) */
	{
		/* This point should never be reached. */
		return cOCT6100_ERR_FATAL_C9;
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ConfBridgeChanMuteSer

Description:    Mute an echo channel present on a conference bridge. 

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pConfBridgeMute		Pointer to conference bridge mute structure.  

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ConfBridgeChanMuteSer
UINT32 Oct6100ConfBridgeChanMuteSer(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_CONF_BRIDGE_CHAN_MUTE		f_pConfBridgeMute )
{
	UINT16	usChanIndex;
	UINT16	usLoadEventIndex;
	UINT16	usSubStoreEventIndex;
	UINT32	ulResult;
	UINT8	fFlexibleConferencing;

	/* Check the validity of the channel and conference bridge given. */
	ulResult = Oct6100ApiCheckBridgeMuteParams( 
										f_pApiInstance, 
										f_pConfBridgeMute, 
										&usChanIndex, 
										&usLoadEventIndex, 
										&usSubStoreEventIndex, 
										&fFlexibleConferencing );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Modify all resources needed by the conference bridge. */
	ulResult = Oct6100ApiUpdateBridgeMuteResources( 
										f_pApiInstance, 
										usChanIndex, 
										usLoadEventIndex, 
										usSubStoreEventIndex,
										fFlexibleConferencing );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckBridgeMuteParams

Description:	Check the validity of the channel and conference bridge given.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.

f_pConfBridgeMute			Pointer to conference bridge channel mute structure.  
f_pusChannelIndex		Pointer to a channel index.
f_pusLoadEventIndex		Pointer to a load mixer event index.
f_pusSubStoreEventIndex	Pointer to a sub-store mixer event index.
f_pfFlexibleConfBridge	If this is a flexible conference bridge.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckBridgeMuteParams
UINT32 Oct6100ApiCheckBridgeMuteParams(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_CONF_BRIDGE_CHAN_MUTE		f_pConfBridgeMute,
				OUT		PUINT16								f_pusChannelIndex,
				OUT		PUINT16								f_pusLoadEventIndex,
				OUT		PUINT16								f_pusSubStoreEventIndex, 
				OUT		PUINT8								f_pfFlexibleConfBridge )
{
	tPOCT6100_API_CONF_BRIDGE		pBridgeEntry;
	tPOCT6100_API_CHANNEL			pEchoChanEntry;
	UINT32	ulEntryOpenCnt;

	/* Check for errors. */
	if ( f_pApiInstance->pSharedInfo->ChipConfig.usMaxConfBridges == 0 )
		return cOCT6100_ERR_CONF_BRIDGE_DISABLED;
	
	if ( f_pConfBridgeMute->ulChannelHndl == cOCT6100_INVALID_HANDLE )
		return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_ADD_INVALID_HANDLE;

	/*=====================================================================*/
	/* Check the channel handle.*/

	if ( (f_pConfBridgeMute->ulChannelHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CHANNEL )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	*f_pusChannelIndex = (UINT16)( f_pConfBridgeMute->ulChannelHndl & cOCT6100_HNDL_INDEX_MASK );
	if ( *f_pusChannelIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxChannels )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pEchoChanEntry, *f_pusChannelIndex )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = (f_pConfBridgeMute->ulChannelHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pEchoChanEntry->fReserved != TRUE )
		return cOCT6100_ERR_CONF_BRIDGE_NOT_OPEN;
	if ( ulEntryOpenCnt != pEchoChanEntry->byEntryOpenCnt )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	/* Check if the channel is bound to a conference bridge. */
	if ( pEchoChanEntry->usBridgeIndex == cOCT6100_INVALID_INDEX )
		return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_MUTE_INVALID_HANDLE;

	/* Check if channel is already muted. */
	if ( pEchoChanEntry->fMute == TRUE )
		return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_MUTE_ALREADY_MUTED;

	/* Check if this is a tap channel, which is always mute. */
	if ( pEchoChanEntry->fTap == TRUE )
		return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_TAP_ALWAYS_MUTE;

	/*=====================================================================*/

	/*=====================================================================*/
	/* Check the conference bridge handle. */

	if ( pEchoChanEntry->usBridgeIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxConfBridges )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBridgeEntry, pEchoChanEntry->usBridgeIndex )

	/* Check for errors. */
	if ( pBridgeEntry->fReserved != TRUE )
		return cOCT6100_ERR_CONF_BRIDGE_NOT_OPEN;

	if ( pBridgeEntry->fFlexibleConferencing == FALSE )
	{
		/* Check the event entries.*/
		if ( pEchoChanEntry->usLoadEventIndex == cOCT6100_INVALID_INDEX )
			return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_MUTE_INVALID_HANDLE;

		if ( pEchoChanEntry->usSubStoreEventIndex == cOCT6100_INVALID_INDEX )
			return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_MUTE_INVALID_HANDLE;
	}

	/*=====================================================================*/

	/* Return the config of the channel and all other important information. */
	*f_pusSubStoreEventIndex = pEchoChanEntry->usSubStoreEventIndex;
	*f_pusLoadEventIndex = pEchoChanEntry->usLoadEventIndex;
	*f_pfFlexibleConfBridge = pBridgeEntry->fFlexibleConferencing;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiUpdateBridgeMuteResources

Description:    Modify the conference bridge entry for this channel in order 
				to mute the specified channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.

f_usChanIndex				Index of the channel to be muted.	
f_usLoadEventIndex			Allocated entry for the Load event of the channel.
f_usSubStoreEventIndex		Allocated entry for the substract and store event of the channel.
f_fFlexibleConfBridge		If this is a flexible conference bridge.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiUpdateBridgeMuteResources
UINT32 Oct6100ApiUpdateBridgeMuteResources(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usChanIndex,
				IN		UINT16							f_usLoadEventIndex,
				IN		UINT16							f_usSubStoreEventIndex, 
				IN		UINT8							f_fFlexibleConfBridge )
{
	tOCT6100_WRITE_PARAMS			WriteParams;
	tOCT6100_READ_PARAMS			ReadParams;

	tPOCT6100_API_CHANNEL			pEchoChanEntry;
	tPOCT6100_SHARED_INFO			pSharedInfo;

	tPOCT6100_API_CONF_BRIDGE		pBridgeEntry;

	tPOCT6100_API_MIXER_EVENT		pLoadEventEntry;
	tPOCT6100_API_MIXER_EVENT		pSubStoreEventEntry;
	tPOCT6100_API_MIXER_EVENT		pTempEntry;
	UINT32	ulResult;
	UINT16	usTempEventIndex;
	UINT32	ulLoopCount;
	UINT16	usReadData;

	BOOL	fCreateSilenceLoad = FALSE;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;
	
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;
	
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChanEntry, f_usChanIndex );
	mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBridgeEntry,  pEchoChanEntry->usBridgeIndex )

	if ( f_fFlexibleConfBridge == TRUE )
	{
		tPOCT6100_API_CHANNEL				pTempEchoChanEntry;
		UINT16								usChannelIndex;
		tPOCT6100_API_FLEX_CONF_PARTICIPANT pParticipant;
		tPOCT6100_API_FLEX_CONF_PARTICIPANT pTempParticipant;

		UINT16	ausMutePortChannelIndexes[ cOCT6100_MAX_FLEX_CONF_PARTICIPANTS_PER_BRIDGE ];
		UINT32	ulMutePortChannelIndex;

		for( ulMutePortChannelIndex = 0; ulMutePortChannelIndex < cOCT6100_MAX_FLEX_CONF_PARTICIPANTS_PER_BRIDGE; ulMutePortChannelIndex ++ )
			ausMutePortChannelIndexes[ ulMutePortChannelIndex ] = cOCT6100_INVALID_INDEX;

		mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pParticipant, pEchoChanEntry->usFlexConfParticipantIndex );

		/* Search through the list of API channel entry for the ones on to this bridge. */
		for ( usChannelIndex = 0; usChannelIndex < pSharedInfo->ChipConfig.usMaxChannels; usChannelIndex++ )
		{
			mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pTempEchoChanEntry, usChannelIndex );

			if ( ( usChannelIndex != f_usChanIndex ) && ( pTempEchoChanEntry->fReserved == TRUE ) )
			{
				if ( pTempEchoChanEntry->usBridgeIndex == pEchoChanEntry->usBridgeIndex )
				{
					mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pTempParticipant, pTempEchoChanEntry->usFlexConfParticipantIndex );

					/* Check if this participant can hear us. */
					if ( ( ( pTempParticipant->ulListenerMask & ( 0x1 << pParticipant->ulListenerMaskIndex ) ) == 0x0 )
						&& ( pTempParticipant->fFlexibleMixerCreated == TRUE ) )
					{
						/* Then update this channel's mixer. */
						ulResult = Oct6100ApiBridgeRemoveParticipantFromChannel(
													f_pApiInstance,
													pEchoChanEntry->usBridgeIndex,
													f_usChanIndex,
													usChannelIndex,
													FALSE );
						if ( ulResult != cOCT6100_ERR_OK )
							return ulResult;

						if ( pTempParticipant->fFlexibleMixerCreated == FALSE )
						{
							/* Remember to mute the port on this channel. */
							for( ulMutePortChannelIndex = 0; ulMutePortChannelIndex < cOCT6100_MAX_FLEX_CONF_PARTICIPANTS_PER_BRIDGE; ulMutePortChannelIndex ++ )
							{
								if ( ausMutePortChannelIndexes[ ulMutePortChannelIndex ] == usChannelIndex )
								{
									break;
								}
								else if ( ausMutePortChannelIndexes[ ulMutePortChannelIndex ] == cOCT6100_INVALID_INDEX )
								{
									ausMutePortChannelIndexes[ ulMutePortChannelIndex ] = usChannelIndex;
									break;
								}
							}
						}
					}
				}
			}
		}

		/* Travel through the channels that were heard by the participant removed and check if their Rin port must be muted. */
		for( ulMutePortChannelIndex = 0; ulMutePortChannelIndex < cOCT6100_MAX_FLEX_CONF_PARTICIPANTS_PER_BRIDGE; ulMutePortChannelIndex ++ )
		{
			if ( ausMutePortChannelIndexes[ ulMutePortChannelIndex ] != cOCT6100_INVALID_INDEX )
			{
				mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pTempEchoChanEntry, ausMutePortChannelIndexes[ ulMutePortChannelIndex ] );

				mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pTempParticipant, pTempEchoChanEntry->usFlexConfParticipantIndex );
				
				if ( pTempParticipant->fFlexibleMixerCreated == FALSE )
				{
					/* Check if the Rin port must be muted on this channel. */
					ulResult = Oct6100ApiMutePorts( 
											f_pApiInstance, 
											ausMutePortChannelIndexes[ ulMutePortChannelIndex ], 
											pTempEchoChanEntry->usRinTsstIndex, 
											pTempEchoChanEntry->usSinTsstIndex,
											FALSE );
					if ( ulResult != cOCT6100_ERR_OK )
					{
						if ( ulResult == cOCT6100_ERR_MIXER_ALL_MIXER_EVENT_ENTRY_OPENED )
						{
							UINT32 ulTempResult;

							/* Cleanup resources, unmute channel... */
							ulTempResult = Oct6100ApiUpdateBridgeUnMuteResources(
											f_pApiInstance,
											f_usChanIndex,
											f_usLoadEventIndex,
											f_usSubStoreEventIndex, 
											TRUE );
							if ( ulTempResult != cOCT6100_ERR_OK )
								return ulTempResult;
							else
								return ulResult;
						}
						else
						{
							return ulResult;
						}
					}
				}
			}
			else /* if ( ausMutePortChannelIndexes[ ulMutePortChannelIndex ] == cOCT6100_INVALID_INDEX ) */
			{
				/* No more channels to check for muting. */
				break;
			}
		}
	}
	else /* if ( f_fFlexibleConfBridge == FALSE ) */
	{
		mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pLoadEventEntry, f_usLoadEventIndex );
		mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pSubStoreEventEntry, f_usSubStoreEventIndex );

		/*=======================================================================*/
		/* Program the Load event. */

		/* Create silence load if this is the first event of the bridge. */
		if ( f_usLoadEventIndex == pBridgeEntry->usFirstLoadEventPtr )
			fCreateSilenceLoad = TRUE;

		/* First check if this event was a load or an accumulate event, if it's a load */
		/* we need to find a new load. */
		if ( f_usLoadEventIndex == pBridgeEntry->usLoadIndex )
		{
			/* Change the next entry if one is present to a load event to keep the bridge alive. */
			if ( pBridgeEntry->usNumClients == 1 )
			{
				/* There is no other entry on the bridge, no need to search for an Accumulate event. */
				pBridgeEntry->usLoadIndex = cOCT6100_INVALID_INDEX;
			}
			else
			{
				/* Search for an accumulate event to tranform into a Load event. */
				usTempEventIndex = pLoadEventEntry->usNextEventPtr;
				ulLoopCount = 0;

				/* Find the copy entry before the entry to remove. */
				mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEntry, usTempEventIndex );

				while( pTempEntry->usEventType != cOCT6100_MIXER_CONTROL_MEM_SUB_STORE && 
					   pTempEntry->usEventType != cOCT6100_MIXER_CONTROL_MEM_STORE )
				{
					if ( pTempEntry->usEventType == cOCT6100_MIXER_CONTROL_MEM_ACCUMULATE )
					{
						/* Change this entry into a load event. */
						ReadParams.ulReadAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( usTempEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
						mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
						if ( ulResult != cOCT6100_ERR_OK )
							return ulResult;

						WriteParams.ulWriteAddress = ReadParams.ulReadAddress;
						WriteParams.usWriteData = (UINT16)(( usReadData & 0x1FFF ) | cOCT6100_MIXER_CONTROL_MEM_LOAD);

						mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
						if ( ulResult != cOCT6100_ERR_OK )
							return ulResult;
						
						/* Set this entry as the load index. */
						pBridgeEntry->usLoadIndex = usTempEventIndex;

						/* Update the software model. */
						pTempEntry->usEventType = cOCT6100_MIXER_CONTROL_MEM_LOAD;

						/* Stop searching. */
						break;
					}
					
					/* Go to the next entry into the list. */
					usTempEventIndex = pTempEntry->usNextEventPtr;
					mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEntry, usTempEventIndex );

					ulLoopCount++;
					if ( ulLoopCount == cOCT6100_MAX_LOOP )
						return cOCT6100_ERR_FATAL_9B;
				}
			}
		}

		WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usLoadEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );

		/* Do not load the sample if the channel is muted. */
		if ( fCreateSilenceLoad == TRUE )
		{
			if ( pBridgeEntry->usSilenceLoadEventPtr == cOCT6100_INVALID_INDEX )
			{
				/* Instead of No-oping, load the silence TSI, to make sure the other conferences before us are not heard. */
				WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_LOAD;
				WriteParams.usWriteData |= 1534; /* TSI index 1534 reserved for silence */

				/* Remember the silence load event. */
				pBridgeEntry->usSilenceLoadEventPtr = f_usLoadEventIndex;
			}
			else
			{
				/* Do nothing. */
				WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_NO_OP;
			}
		}
		else
		{
			/* Do nothing. */
			WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_NO_OP;
		}

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Update the software model. */
		pLoadEventEntry->usEventType = cOCT6100_MIXER_CONTROL_MEM_NO_OP;

		/*=======================================================================*/
		
		/*=======================================================================*/
		/* Program the Substract and store event. */
		WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usSubStoreEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
		
		/* Do not load the sample if the channel is muted. */
		WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_STORE;

		/* If we have an extra Sin copy event, we know we are using the Sout port as a source. */
		if ( pEchoChanEntry->usSinCopyEventIndex != cOCT6100_INVALID_INDEX )
		{
			/* Sout input. */
			WriteParams.usWriteData |= pEchoChanEntry->TdmConfig.bySoutPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;
		}
		else /* if ( pEchoChanEntry->usSinCopyEventIndex == cOCT6100_INVALID_INDEX ) */
		{
			/* Rin input. */
			WriteParams.usWriteData |= pEchoChanEntry->TdmConfig.byRinPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;
		}

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Update the software model. */
		pSubStoreEventEntry->usEventType = cOCT6100_MIXER_CONTROL_MEM_STORE;

		/*=======================================================================*/
	}

	/* Update the channel entry API structure */
	pEchoChanEntry->fMute = TRUE;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ConfBridgeChanUnMuteSer

Description:    UnMute an echo channel present on a conference bridge. 

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pConfBridgeUnMute		Pointer to conference bridge channel unmute structure.  

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ConfBridgeChanUnMuteSer
UINT32 Oct6100ConfBridgeChanUnMuteSer(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CONF_BRIDGE_CHAN_UNMUTE		f_pConfBridgeUnMute )
{
	UINT16	usChanIndex;
	UINT16	usLoadEventIndex;
	UINT16	usSubStoreEventIndex;
	UINT8	fFlexibleConfBridge;
	UINT32	ulResult;

	/* Check the validity of the channel and conference bridge given. */
	ulResult = Oct6100ApiCheckBridgeUnMuteParams( 
											f_pApiInstance, 
											f_pConfBridgeUnMute, 
											&usChanIndex, 
											&usLoadEventIndex, 
											&usSubStoreEventIndex, 
											&fFlexibleConfBridge );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Modify all resources needed by the conference bridge. */
	ulResult = Oct6100ApiUpdateBridgeUnMuteResources( 
											f_pApiInstance, 
											usChanIndex, 
											usLoadEventIndex, 
											usSubStoreEventIndex, 
											fFlexibleConfBridge );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckBridgeUnMuteParams

Description:	Check the validity of the channel and conference bridge given.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.

f_pConfBridgeUnMute			Pointer to conference bridge channel unmute structure.  
f_pusChannelIndex			Pointer to the channel index fo the channel to be unmuted.
f_pusLoadEventIndex			Pointer to the load index of the channel.
f_pusSubStoreEventIndex		Pointer to the sub-store event of the channel.
f_pfFlexibleConfBridge		If this is a flexible conference bridge.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckBridgeUnMuteParams
UINT32 Oct6100ApiCheckBridgeUnMuteParams(
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance,
				IN		tPOCT6100_CONF_BRIDGE_CHAN_UNMUTE	f_pConfBridgeUnMute,
				OUT		PUINT16								f_pusChannelIndex,
				OUT		PUINT16								f_pusLoadEventIndex,
				OUT		PUINT16								f_pusSubStoreEventIndex, 
				OUT		PUINT8								f_pfFlexibleConfBridge )
{
	tPOCT6100_API_CONF_BRIDGE		pBridgeEntry;
	tPOCT6100_API_CHANNEL			pEchoChanEntry;
	UINT32	ulEntryOpenCnt;

	/* Check for errors. */
	if ( f_pApiInstance->pSharedInfo->ChipConfig.usMaxConfBridges == 0 )
		return cOCT6100_ERR_CONF_BRIDGE_DISABLED;
	
	if ( f_pConfBridgeUnMute->ulChannelHndl == cOCT6100_INVALID_HANDLE )
		return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_ADD_INVALID_HANDLE;

	/*=====================================================================*/
	/* Check the channel handle.*/

	if ( (f_pConfBridgeUnMute->ulChannelHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CHANNEL )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	*f_pusChannelIndex = (UINT16)( f_pConfBridgeUnMute->ulChannelHndl & cOCT6100_HNDL_INDEX_MASK );
	if ( *f_pusChannelIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxChannels )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pEchoChanEntry, *f_pusChannelIndex )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = (f_pConfBridgeUnMute->ulChannelHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pEchoChanEntry->fReserved != TRUE )
		return cOCT6100_ERR_CONF_BRIDGE_NOT_OPEN;
	if ( ulEntryOpenCnt != pEchoChanEntry->byEntryOpenCnt )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	/* Check if the channel is bound to a conference bridge.*/
	if ( pEchoChanEntry->usBridgeIndex == cOCT6100_INVALID_INDEX )
		return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_MUTE_INVALID_HANDLE;

	/* Check if channel is already muted.*/
	if ( pEchoChanEntry->fMute == FALSE )
		return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_MUTE_NOT_MUTED;

	/*=====================================================================*/

	/*=====================================================================*/
	/* Check the conference bridge handle. */

	if (  pEchoChanEntry->usBridgeIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxConfBridges )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBridgeEntry,  pEchoChanEntry->usBridgeIndex )

	/* Check for errors. */
	if ( pBridgeEntry->fReserved != TRUE )
		return cOCT6100_ERR_CONF_BRIDGE_NOT_OPEN;
	
	/* Check the event entries.*/
	if ( pBridgeEntry->fFlexibleConferencing == FALSE )
	{
		if ( pEchoChanEntry->usLoadEventIndex == cOCT6100_INVALID_INDEX )
			return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_MUTE_INVALID_HANDLE;

		/* Check the event entries.*/
		if ( pEchoChanEntry->usSubStoreEventIndex == cOCT6100_INVALID_INDEX )
			return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_MUTE_INVALID_HANDLE;
	}

	/*=====================================================================*/

	/* Return the config of the channel and all other important information.*/
	*f_pusSubStoreEventIndex = pEchoChanEntry->usSubStoreEventIndex;
	*f_pusLoadEventIndex = pEchoChanEntry->usLoadEventIndex;
	*f_pfFlexibleConfBridge = pBridgeEntry->fFlexibleConferencing;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiUpdateBridgeUnMuteResources

Description:    Modify the conference bridge entry for this channel in order 
				to un-mute the specified channel.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.

f_usChanIndex				Index of the channel to be unmuted.	
f_usLoadEventIndex			Allocated entry for the Load event of the channel.
f_usSubStoreEventIndex		Allocated entry for the substract and store event of the channel.
f_fFlexibleConfBridge		If this is a flexible conference bridge.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiUpdateBridgeUnMuteResources
UINT32 Oct6100ApiUpdateBridgeUnMuteResources(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usChanIndex,
				IN		UINT16							f_usLoadEventIndex,
				IN		UINT16							f_usSubStoreEventIndex, 
				IN		UINT8							f_fFlexibleConfBridge )
{
	tOCT6100_WRITE_PARAMS			WriteParams;
	tOCT6100_READ_PARAMS			ReadParams;

	tPOCT6100_API_CHANNEL			pEchoChanEntry;
	tPOCT6100_SHARED_INFO			pSharedInfo;

	tPOCT6100_API_CONF_BRIDGE		pBridgeEntry;

	tPOCT6100_API_MIXER_EVENT		pLoadEventEntry;
	tPOCT6100_API_MIXER_EVENT		pSubStoreEventEntry;
	tPOCT6100_API_MIXER_EVENT		pTempEntry;
	UINT32	ulResult;
	UINT16	usTempEventIndex;
	UINT32	ulLoopCount;
	UINT16	usReadData;
	
	UINT16	usLoadEventType		= cOCT6100_MIXER_CONTROL_MEM_ACCUMULATE;
	UINT16	usPreviousLoadIndex = cOCT6100_INVALID_INDEX;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;
	
	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	ReadParams.pProcessContext = f_pApiInstance->pProcessContext;

	ReadParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;
	ReadParams.pusReadData = &usReadData;

	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChanEntry, f_usChanIndex );
	mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBridgeEntry,  pEchoChanEntry->usBridgeIndex )
	
	if ( f_fFlexibleConfBridge == TRUE )
	{
		tPOCT6100_API_CHANNEL				pTempEchoChanEntry;
		UINT16								usChannelIndex;
		tPOCT6100_API_FLEX_CONF_PARTICIPANT pParticipant;
		tPOCT6100_API_FLEX_CONF_PARTICIPANT pTempParticipant;

		mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pParticipant, pEchoChanEntry->usFlexConfParticipantIndex );

		/* Before doing anything, check if the copy events must be created. */
		if ( ( pParticipant->ulInputPort == cOCT6100_CHANNEL_PORT_SOUT ) && ( pEchoChanEntry->fCopyEventCreated == FALSE ) )
		{
			/* The copy event has not been created, create it once for the life of the participant on the bridge. */
			WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pEchoChanEntry->usSinCopyEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
			
			WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_COPY;
			WriteParams.usWriteData |= pEchoChanEntry->usExtraSinTsiMemIndex;
			WriteParams.usWriteData |= pEchoChanEntry->TdmConfig.bySinPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;

			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			WriteParams.ulWriteAddress += 2;
			WriteParams.usWriteData = pEchoChanEntry->usSinSoutTsiMemIndex;

			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			/* Now insert the Sin copy event into the list. */
			ulResult = Oct6100ApiMixerEventAdd( f_pApiInstance,
												pEchoChanEntry->usSinCopyEventIndex,
												cOCT6100_EVENT_TYPE_SIN_COPY,
												f_usChanIndex );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;

			pEchoChanEntry->fCopyEventCreated = TRUE;
		}
		
		/* Search through the list of API channel entry for the ones onto this bridge. */
		for ( usChannelIndex = 0; usChannelIndex < pSharedInfo->ChipConfig.usMaxChannels; usChannelIndex++ )
		{
			mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pTempEchoChanEntry, usChannelIndex );
			
			/* Channel reserved? */
			if ( ( usChannelIndex != f_usChanIndex ) && ( pTempEchoChanEntry->fReserved == TRUE ) )
			{
				/* On current bridge? */
				if ( pTempEchoChanEntry->usBridgeIndex == pEchoChanEntry->usBridgeIndex )
				{
					mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pTempParticipant, pTempEchoChanEntry->usFlexConfParticipantIndex );

					/* Check if this participant can hear us. */
					if ( ( pTempParticipant->ulListenerMask & ( 0x1 << pParticipant->ulListenerMaskIndex ) ) == 0x0 )
					{
						/* Then create/update this channel's mixer. */
						ulResult = Oct6100ApiBridgeAddParticipantToChannel(
													f_pApiInstance,
													pEchoChanEntry->usBridgeIndex,
													f_usChanIndex,
													usChannelIndex,
													pTempParticipant->ausLoadOrAccumulateEventIndex[ pParticipant->ulListenerMaskIndex ],
													pTempEchoChanEntry->usSubStoreEventIndex,
													pTempEchoChanEntry->usSinCopyEventIndex,
													pParticipant->ulInputPort,
													pTempParticipant->ulInputPort );
						if ( ulResult != cOCT6100_ERR_OK )
							return ulResult;

						/* Check if the Rin silence event can be cleared now that the */
						/* channel has unmuted. */
						if ( ( pTempParticipant->fFlexibleMixerCreated == TRUE )
							&& ( pTempEchoChanEntry->usRinSilenceEventIndex != cOCT6100_INVALID_INDEX ) )
						{
							/* Remove the event from the list. */
							ulResult = Oct6100ApiMixerEventRemove(	f_pApiInstance,
																	pTempEchoChanEntry->usRinSilenceEventIndex,
																	cOCT6100_EVENT_TYPE_SOUT_COPY );
							if ( ulResult != cOCT6100_ERR_OK )
								return ulResult;

							ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pTempEchoChanEntry->usRinSilenceEventIndex );
							if ( ulResult != cOCT6100_ERR_OK )
								return cOCT6100_ERR_FATAL_DF;

							pTempEchoChanEntry->usRinSilenceEventIndex = cOCT6100_INVALID_INDEX;
						}
					}
				}
			}
		}
	}
	else /* if ( f_fFlexibleConfBridge == FALSE ) */
	{
		mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pLoadEventEntry, f_usLoadEventIndex );
		mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pSubStoreEventEntry, f_usSubStoreEventIndex );

		/*=======================================================================*/
		/* Program the Load event. */

		/* Before reactivating this event, check what type of event this event must be. */
		if ( f_usLoadEventIndex == pBridgeEntry->usFirstLoadEventPtr ||
			 pBridgeEntry->usLoadIndex == cOCT6100_INVALID_INDEX )
		{
			/* This event must become a Load event. */
			usLoadEventType = cOCT6100_MIXER_CONTROL_MEM_LOAD;
			pBridgeEntry->usLoadIndex = f_usLoadEventIndex;
		}

		usTempEventIndex = pBridgeEntry->usFirstLoadEventPtr;
		mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEntry, usTempEventIndex );

		ulLoopCount = 0;

		while( pTempEntry->usEventType != cOCT6100_MIXER_CONTROL_MEM_SUB_STORE && 
			   pTempEntry->usEventType != cOCT6100_MIXER_CONTROL_MEM_STORE )
		{
			if ( pTempEntry->usEventType == cOCT6100_MIXER_CONTROL_MEM_LOAD )
			{
				usPreviousLoadIndex = usTempEventIndex;
			}

			/* Check if the previous load event is before or after the event about to be unmuted. */
			if ( pTempEntry->usNextEventPtr == f_usLoadEventIndex )
			{
				if ( usPreviousLoadIndex == cOCT6100_INVALID_INDEX )
				{
					/* We did not find a load event before our node, this mean this one */
					/* is about to become the new load event. */
					usLoadEventType = cOCT6100_MIXER_CONTROL_MEM_LOAD;
				}
			}
			
			/* Go to the next entry into the list. */
			usTempEventIndex = pTempEntry->usNextEventPtr;
			mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEntry, usTempEventIndex );

			ulLoopCount++;
			if ( ulLoopCount == cOCT6100_MAX_LOOP )
				return cOCT6100_ERR_FATAL_9B;
		}	
		
		/* Now program the current event node. */
		WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usLoadEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
		
		WriteParams.usWriteData = usLoadEventType;

		/* If we have an extra Sin copy event, we know we are using the Sout port as a source. */
		if ( pEchoChanEntry->usSinCopyEventIndex != cOCT6100_INVALID_INDEX )
		{
			/* Sout source */
			WriteParams.usWriteData |= pEchoChanEntry->usSinSoutTsiMemIndex;
		}
		else
		{
			/* Rin source */
			WriteParams.usWriteData |= pEchoChanEntry->usRinRoutTsiMemIndex;
		}
		
		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
		
		/* Update the software event to reflect the hardware. */
		pLoadEventEntry->usEventType = usLoadEventType;

		/* Check if we need to change another node. */
		if ( usLoadEventType == cOCT6100_MIXER_CONTROL_MEM_LOAD )
		{
			if ( usPreviousLoadIndex != cOCT6100_INVALID_INDEX )
			{
				/* Now program the old load event. */
				ReadParams.ulReadAddress =  cOCT6100_MIXER_CONTROL_MEM_BASE + ( usPreviousLoadIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
			
				mOCT6100_DRIVER_READ_API( ReadParams, ulResult );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;
				
				WriteParams.ulWriteAddress = ReadParams.ulReadAddress;
				WriteParams.usWriteData = (UINT16)(( usReadData & 0x1FFF ) | cOCT6100_MIXER_CONTROL_MEM_ACCUMULATE );
				
				mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;
		
				/* Update the software event to reflect the hardware. */
				mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( pSharedInfo, pTempEntry, usPreviousLoadIndex );
				pTempEntry->usEventType = cOCT6100_MIXER_CONTROL_MEM_ACCUMULATE;
			}
		}

		/*=======================================================================*/
		
		/*=======================================================================*/
		/* Program the Substract and store event. */
		WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( f_usSubStoreEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );
		
		WriteParams.usWriteData = cOCT6100_MIXER_CONTROL_MEM_SUB_STORE;
		/* If we have an extra Sin copy event, we know we are using the Sout port as a source. */
		if ( pEchoChanEntry->usSinCopyEventIndex != cOCT6100_INVALID_INDEX )
		{
			/* Sout port source */
			WriteParams.usWriteData |= pEchoChanEntry->TdmConfig.bySoutPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;
			WriteParams.usWriteData |= pEchoChanEntry->usSinSoutTsiMemIndex;
		}
		else
		{
			/* Rin port source */
			WriteParams.usWriteData |= pEchoChanEntry->TdmConfig.byRinPcmLaw << cOCT6100_MIXER_CONTROL_MEM_LAW_OFFSET;
			WriteParams.usWriteData |= pEchoChanEntry->usRinRoutTsiMemIndex;
		}

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		WriteParams.ulWriteAddress += 2;
		WriteParams.usWriteData = pEchoChanEntry->usRinRoutTsiMemIndex;

		mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;

		/* Update the software event to reflect the hardware. */
		pSubStoreEventEntry->usEventType = cOCT6100_MIXER_CONTROL_MEM_SUB_STORE;

		/*=======================================================================*/


		/*=======================================================================*/
		/* Check if have to remove silence load event. */

		if ( pBridgeEntry->usSilenceLoadEventPtr != cOCT6100_INVALID_INDEX )
		{
			if ( pBridgeEntry->usSilenceLoadEventPtr == f_usLoadEventIndex )
			{
				/* Clear the silence load event ptr. */
				pBridgeEntry->usSilenceLoadEventPtr = cOCT6100_INVALID_INDEX;
			}
		}
	}

	/* Update the channel entry API structure */
	pEchoChanEntry->fMute = FALSE;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ConfBridgeDominantSpeakerSetSer

Description:    This function sets the dominant speaker of a bridge.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to 
						keep the present state of the chip and all its 
						resources.

f_pConfBridgeDominant	Pointer to conference bridge dominant speaker 
						structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ConfBridgeDominantSpeakerSetSer
UINT32 Oct6100ConfBridgeDominantSpeakerSetSer(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInstance,
				IN		tPOCT6100_CONF_BRIDGE_DOMINANT_SPEAKER_SET	f_pConfBridgeDominantSpeaker )
{
	UINT16	usChanIndex;
	UINT16	usBridgeIndex;
	UINT32	ulResult;

	/* Check the validity of the channel handle given. */
	ulResult = Oct6100ApiCheckBridgeDominantSpeakerParams( f_pApiInstance, f_pConfBridgeDominantSpeaker, &usChanIndex, &usBridgeIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Modify all resources needed by the conference bridge. */
	ulResult = Oct6100ApiUpdateBridgeDominantSpeakerResources( f_pApiInstance, usChanIndex, usBridgeIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckBridgeDominantSpeakerParams

Description:	Check the validity of the channel given for setting the
				dominant speaker.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.
f_pConfBridgeDominant	Pointer to conference bridge channel dominant speaker structure.  
f_pusChannelIndex		Pointer to a channel index.
f_pusChannelIndex		Pointer to a bridge index.
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckBridgeDominantSpeakerParams
UINT32 Oct6100ApiCheckBridgeDominantSpeakerParams(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInstance,
				IN		tPOCT6100_CONF_BRIDGE_DOMINANT_SPEAKER_SET	f_pConfBridgeDominantSpeaker,
				OUT		PUINT16										f_pusChannelIndex,
				OUT		PUINT16										f_pusBridgeIndex )
{
	tPOCT6100_API_CONF_BRIDGE	pBridgeEntry;
	tPOCT6100_API_CHANNEL		pEchoChanEntry;
	UINT32	ulEntryOpenCnt;
	BOOL	fCheckEntryOpenCnt = FALSE;

	/* Check for errors. */
	if ( f_pApiInstance->pSharedInfo->ChipConfig.usMaxConfBridges == 0 )
		return cOCT6100_ERR_CONF_BRIDGE_DISABLED;
	
	if ( f_pConfBridgeDominantSpeaker->ulChannelHndl == cOCT6100_INVALID_HANDLE )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	/*=====================================================================*/
	/* Check the channel handle. */

	if ( f_pConfBridgeDominantSpeaker->ulChannelHndl != cOCT6100_CONF_NO_DOMINANT_SPEAKER_HNDL )
	{
		if ( (f_pConfBridgeDominantSpeaker->ulChannelHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CHANNEL )
			return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

		*f_pusChannelIndex = (UINT16)( f_pConfBridgeDominantSpeaker->ulChannelHndl & cOCT6100_HNDL_INDEX_MASK );
		if ( *f_pusChannelIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxChannels )
			return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

		mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pEchoChanEntry, *f_pusChannelIndex )

		/* Extract the entry open count from the provided handle. */
		ulEntryOpenCnt = (f_pConfBridgeDominantSpeaker->ulChannelHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

		/* Check for errors. */
		if ( pEchoChanEntry->fReserved != TRUE )
			return cOCT6100_ERR_CONF_BRIDGE_NOT_OPEN;
		if ( ulEntryOpenCnt != pEchoChanEntry->byEntryOpenCnt )
			return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

		/* Check if the channel is bound to a conference bridge. */
		if ( pEchoChanEntry->usBridgeIndex == cOCT6100_INVALID_INDEX )
			return cOCT6100_ERR_CONF_BRIDGE_CHAN_NOT_ON_BRIDGE;

		/* Check if the NLP is enabled on this channel. */
		if ( pEchoChanEntry->VqeConfig.fEnableNlp == FALSE )
			return cOCT6100_ERR_CONF_BRIDGE_NLP_MUST_BE_ENABLED;

		/* Check if conferencing noise reduction is enabled on this channel. */
		if ( pEchoChanEntry->VqeConfig.fSoutConferencingNoiseReduction == FALSE )
			return cOCT6100_ERR_CONF_BRIDGE_CNR_MUST_BE_ENABLED;

		/* Check if this is a tap channel.  If it is, it will never be the dominant speaker! */
		if ( pEchoChanEntry->fTap == TRUE )
			return cOCT6100_ERR_CONF_BRIDGE_CHANNEL_TAP_ALWAYS_MUTE;

		/* Set the bridge index. */
		*f_pusBridgeIndex = pEchoChanEntry->usBridgeIndex;
	}
	else
	{
		/* Set this such that there is no dominant speaker on this conference bridge. */
		*f_pusChannelIndex = cOCT6100_CONF_DOMINANT_SPEAKER_UNASSIGNED;

		/* Check the conference bridge handle. */
		if ( (f_pConfBridgeDominantSpeaker->ulConfBridgeHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CONF_BRIDGE )
			return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

		/* Set the bridge index. */
		*f_pusBridgeIndex = (UINT16)( f_pConfBridgeDominantSpeaker->ulConfBridgeHndl & cOCT6100_HNDL_INDEX_MASK );

		/* Extract the entry open count from the provided handle. */
		ulEntryOpenCnt = (f_pConfBridgeDominantSpeaker->ulConfBridgeHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;
		fCheckEntryOpenCnt = TRUE;
	}

	/*=====================================================================*/

	/*=====================================================================*/

	if ( *f_pusBridgeIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxConfBridges )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBridgeEntry, *f_pusBridgeIndex )

	/* Check for errors. */
	if ( pBridgeEntry->fReserved != TRUE )
		return cOCT6100_ERR_CONF_BRIDGE_NOT_OPEN;
	if ( fCheckEntryOpenCnt == TRUE )
	{
		if ( ulEntryOpenCnt != pBridgeEntry->byEntryOpenCnt )
			return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;
	}

	/*=====================================================================*/
	/* Check if dominant speaker is supported in this firmware version. */
	
	if ( f_pApiInstance->pSharedInfo->ImageInfo.fDominantSpeakerEnabled == FALSE )
		return cOCT6100_ERR_NOT_SUPPORTED_DOMINANT_SPEAKER;

	/*=====================================================================*/

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiUpdateBridgeDominantSpeakerResources

Description:    Modify the conference bridge such that the new dominant
				speaker is the one specified by the index.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.
							
f_usChanIndex				Index of the channel to be set as the dominant speaker.	
f_usBridgeIndex				Index of the bridge where this channel is on.	

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiUpdateBridgeDominantSpeakerResources
UINT32 Oct6100ApiUpdateBridgeDominantSpeakerResources(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usChanIndex,
				IN		UINT16							f_usBridgeIndex )
{
	tPOCT6100_API_CHANNEL			pEchoChanEntry;
	tPOCT6100_API_CONF_BRIDGE		pBridgeEntry;
	tPOCT6100_SHARED_INFO			pSharedInfo;

	UINT16	usChannelIndex;
	UINT32	ulResult;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Get the bridge entry for this channel. */
	mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBridgeEntry, f_usBridgeIndex )

	/* Set the dominant speaker index for all channels in this conference. */

	/* Search through the list of API channel entry for the ones on to this bridge.*/
	for ( usChannelIndex = 0; usChannelIndex < pSharedInfo->ChipConfig.usMaxChannels; usChannelIndex++ )
	{
		mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pEchoChanEntry, usChannelIndex );
		
		if ( pEchoChanEntry->fReserved == TRUE )
		{
			if ( pEchoChanEntry->usBridgeIndex == f_usBridgeIndex )
			{
				/* If we are unsetting the dominant speaker, of if it is not our channel index. */
				if ( ( f_usChanIndex == cOCT6100_CONF_DOMINANT_SPEAKER_UNASSIGNED )
					|| ( f_usChanIndex != usChannelIndex ) )
				{
					ulResult = Oct6100ApiBridgeSetDominantSpeaker( f_pApiInstance, usChannelIndex, f_usChanIndex );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;
				}
			}
		}
	}

	/* Make sure this channel is disabled. */
	if ( f_usChanIndex != cOCT6100_CONF_DOMINANT_SPEAKER_UNASSIGNED )
	{
		ulResult = Oct6100ApiBridgeSetDominantSpeaker( f_pApiInstance, f_usChanIndex, cOCT6100_CONF_DOMINANT_SPEAKER_UNASSIGNED );
		if ( ulResult != cOCT6100_ERR_OK )
			return ulResult;
	}

	/* Save this in the conference bridge structure. */
	/* This will be needed later when removing the channel. */
	pBridgeEntry->fDominantSpeakerSet = TRUE;
	pBridgeEntry->usDominantSpeakerChanIndex = f_usChanIndex;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ConfBridgeMaskChangeSer

Description:    This function changes the mask of flexible bridge
				participant.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to 
						keep the present state of the chip and all its 
						resources.

f_pConfBridgeMaskChange	Pointer to conference bridge participant mask
						change structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ConfBridgeMaskChangeSer
UINT32 Oct6100ConfBridgeMaskChangeSer(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInstance,
				IN		tPOCT6100_CONF_BRIDGE_MASK_CHANGE			f_pConfBridgeMaskChange )
{
	UINT16	usChanIndex;
	UINT16	usBridgeIndex;
	UINT32	ulResult;
	UINT32	ulNewParticipantMask;

	/* Check the validity of the channel handle given. */
	ulResult = Oct6100ApiCheckBridgeMaskChangeParams( f_pApiInstance, f_pConfBridgeMaskChange, &usChanIndex, &usBridgeIndex, &ulNewParticipantMask );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Update all resources needed by the new mask. */
	ulResult = Oct6100ApiUpdateMaskModifyResources( f_pApiInstance, usBridgeIndex, usChanIndex, ulNewParticipantMask );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Commit the changes to the chip's internal memories. */
	ulResult = Oct6100ApiBridgeUpdateMask( f_pApiInstance, usBridgeIndex, usChanIndex, ulNewParticipantMask );
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiCheckBridgeMaskChangeParams

Description:	Check the validity of the channel given for setting the
				mask.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.

f_pConfBridgeMaskChange		Pointer to conference bridge channel mask change structure.  
f_pusChannelIndex			Pointer to a channel index.
f_pusBridgeIndex			Pointer to a bridge index.
f_pulNewParticipantMask		New mask to apply for this participant.
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiCheckBridgeMaskChangeParams
UINT32 Oct6100ApiCheckBridgeMaskChangeParams(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_CONF_BRIDGE_MASK_CHANGE		f_pConfBridgeMaskChange,
				OUT		PUINT16									f_pusChannelIndex,
				OUT		PUINT16									f_pusBridgeIndex,
				OUT		PUINT32									f_pulNewParticipantMask )
{
	tPOCT6100_API_CONF_BRIDGE	pBridgeEntry;
	tPOCT6100_API_CHANNEL		pEchoChanEntry;
	UINT32	ulEntryOpenCnt;

	/* Check for errors. */
	if ( f_pApiInstance->pSharedInfo->ChipConfig.usMaxConfBridges == 0 )
		return cOCT6100_ERR_CONF_BRIDGE_DISABLED;
	
	if ( f_pConfBridgeMaskChange->ulChannelHndl == cOCT6100_INVALID_HANDLE )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	/*=====================================================================*/
	/* Check the channel handle.*/

	if ( (f_pConfBridgeMaskChange->ulChannelHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CHANNEL )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	*f_pusChannelIndex = (UINT16)( f_pConfBridgeMaskChange->ulChannelHndl & cOCT6100_HNDL_INDEX_MASK );
	if ( *f_pusChannelIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxChannels )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pEchoChanEntry, *f_pusChannelIndex )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = (f_pConfBridgeMaskChange->ulChannelHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pEchoChanEntry->fReserved != TRUE )
		return cOCT6100_ERR_CONF_BRIDGE_NOT_OPEN;
	if ( ulEntryOpenCnt != pEchoChanEntry->byEntryOpenCnt )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	/* Check if the channel is bound to a conference bridge. */
	if ( pEchoChanEntry->usBridgeIndex == cOCT6100_INVALID_INDEX )
		return cOCT6100_ERR_CONF_BRIDGE_CHAN_NOT_ON_BRIDGE;

	/* Set the bridge index. */
	*f_pusBridgeIndex = pEchoChanEntry->usBridgeIndex;

	/*=====================================================================*/

	/*=====================================================================*/

	if ( ( *f_pusBridgeIndex == cOCT6100_INVALID_INDEX )
		|| ( *f_pusBridgeIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxConfBridges ) )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBridgeEntry, *f_pusBridgeIndex )

	/* Check for errors. */
	if ( pBridgeEntry->fReserved != TRUE )
		return cOCT6100_ERR_CONF_BRIDGE_NOT_OPEN;

	/* Check if this is bridge is a flexible conference bridge. */
	if ( pBridgeEntry->fFlexibleConferencing == FALSE )
		return cOCT6100_ERR_CONF_BRIDGE_SIMPLE_BRIDGE;
	
	/*=====================================================================*/

	/* Return new mask to apply. */
	*f_pulNewParticipantMask = f_pConfBridgeMaskChange->ulNewListenerMask;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiUpdateMaskModifyResources

Description:    Modify/reserve all resources needed for the modification of
				the participant's mask.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.

f_usBridgeIndex				Bridge index of the bridge where this channel is residing.
f_usChanIndex				Channel index of the channel to be modified.
f_ulNewListenerMask			New mask to apply to the selected participant.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiUpdateMaskModifyResources
UINT32 Oct6100ApiUpdateMaskModifyResources(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usBridgeIndex,
				IN		UINT16							f_usChanIndex,
				IN		UINT32							f_ulNewListenerMask )
{
	tPOCT6100_API_CHANNEL				pChanEntry;
	tPOCT6100_API_CHANNEL				pTempEchoChanEntry;
	tPOCT6100_SHARED_INFO				pSharedInfo;
	tPOCT6100_API_FLEX_CONF_PARTICIPANT pParticipant;
	tPOCT6100_API_FLEX_CONF_PARTICIPANT pTempParticipant;

	UINT32	ulResult = cOCT6100_ERR_OK;
	UINT32	ulTempVar;
	UINT32	ulOldListenerMask;
	UINT16	usChannelIndex;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Get a pointer to the channel's list entry. */
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pChanEntry, f_usChanIndex )

	mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pParticipant, pChanEntry->usFlexConfParticipantIndex );

	/* Must travel all clients of this conference and reserve a load or accumulate event for */
	/* all participants which could not hear us but now can. While at it, check for events that */
	/* could be released, for example a participant that we cannot hear anymore. */

	ulOldListenerMask = pParticipant->ulListenerMask;

	/* Search through the list of API channel entry for the ones on to this bridge.*/
	for ( usChannelIndex = 0; ( usChannelIndex < pSharedInfo->ChipConfig.usMaxChannels ) && ( ulResult == cOCT6100_ERR_OK ) ; usChannelIndex++ )
	{
		mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pTempEchoChanEntry, usChannelIndex );
		
		/* Channel reserved? */
		if ( ( usChannelIndex != f_usChanIndex ) && ( pTempEchoChanEntry->fReserved == TRUE ) )
		{
			/* On current bridge? */
			if ( pTempEchoChanEntry->usBridgeIndex == f_usBridgeIndex )
			{
				mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pTempParticipant, pTempEchoChanEntry->usFlexConfParticipantIndex );

				/* Check if we can now hear this participant, but could not before. */
				if ( ( ( f_ulNewListenerMask & ( 0x1 << pTempParticipant->ulListenerMaskIndex ) ) == 0x0 )
					&& ( ( ulOldListenerMask & ( 0x1 << pTempParticipant->ulListenerMaskIndex ) ) != 0x0 ) )
				{
					/* Must reserve a load or accumulate entry mixer event here! */
					ulResult = Oct6100ApiReserveMixerEventEntry( f_pApiInstance, &pParticipant->ausLoadOrAccumulateEventIndex[ pTempParticipant->ulListenerMaskIndex ] );
					if ( ulResult != cOCT6100_ERR_OK )
					{
						/* Most probably, the hardware is out of mixer events. */
						break;
					}
				}

				/* Check if we can now NOT hear this participant, but could before. */
				if ( ( ( f_ulNewListenerMask & ( 0x1 << pTempParticipant->ulListenerMaskIndex ) ) != 0x0 )
					&& ( ( ulOldListenerMask & ( 0x1 << pTempParticipant->ulListenerMaskIndex ) ) == 0x0 ) )
				{
					/* Must release the load or accumulate entry mixer event. */
					ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pParticipant->ausLoadOrAccumulateEventIndex[ pTempParticipant->ulListenerMaskIndex ] );
					if ( ulResult != cOCT6100_ERR_OK )
					{
						break;
					}
				}
			}
		}
	}

	/* If an error is returned, make sure everything is cleaned up properly. */
	if ( ulResult != cOCT6100_ERR_OK )
	{
		/* Search through the list of API channel entry for the ones on to this bridge.*/
		for ( usChannelIndex = 0; usChannelIndex < pSharedInfo->ChipConfig.usMaxChannels; usChannelIndex++ )
		{
			mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pTempEchoChanEntry, usChannelIndex );
			
			/* Channel reserved? */
			if ( ( usChannelIndex != f_usChanIndex ) && ( pTempEchoChanEntry->fReserved == TRUE ) )
			{
				/* On current bridge? */
				if ( pTempEchoChanEntry->usBridgeIndex == f_usBridgeIndex )
				{
					mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pTempParticipant, pTempEchoChanEntry->usFlexConfParticipantIndex );

					/* Check if we can now hear this participant, but could not before. */
					if ( ( ( f_ulNewListenerMask & ( 0x1 << pTempParticipant->ulListenerMaskIndex ) ) == 0x0 )
						&& ( ( ulOldListenerMask & ( 0x1 << pTempParticipant->ulListenerMaskIndex ) ) != 0x0 ) )
					{
						/* If the load or event entry in the mixer memory was reserved. */
						if ( pParticipant->ausLoadOrAccumulateEventIndex[ pTempParticipant->ulListenerMaskIndex ] != cOCT6100_INVALID_INDEX )
						{
							/* Must release the load or accumulate entry mixer event. */
							ulTempVar = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pParticipant->ausLoadOrAccumulateEventIndex[ pTempParticipant->ulListenerMaskIndex ] );
							if ( ulTempVar != cOCT6100_ERR_OK )
								return ulTempVar;

							pParticipant->ausLoadOrAccumulateEventIndex[ pTempParticipant->ulListenerMaskIndex ] = cOCT6100_INVALID_INDEX;
						}
					}

					/* Check if we can now NOT hear this participant, but could before. */
					if ( ( ( f_ulNewListenerMask & ( 0x1 << pTempParticipant->ulListenerMaskIndex ) ) != 0x0 )
						&& ( ( ulOldListenerMask & ( 0x1 << pTempParticipant->ulListenerMaskIndex ) ) == 0x0 ) )
					{
						/* If the load or event entry in the mixer memory was reserved. */
						if ( pParticipant->ausLoadOrAccumulateEventIndex[ pTempParticipant->ulListenerMaskIndex ] == cOCT6100_INVALID_INDEX )
						{
							/* Must release the load or accumulate entry mixer event. */
							ulTempVar = Oct6100ApiReserveMixerEventEntry( f_pApiInstance, &( pParticipant->ausLoadOrAccumulateEventIndex[ pTempParticipant->ulListenerMaskIndex ] ) );
							if ( ulTempVar != cOCT6100_ERR_OK )
								return ulTempVar;
						}
					}
				}
			}
		}

		return ulResult;
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiBridgeUpdateMask

Description:    Update the participant's mask.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.
f_usBridgeIndex				Bridge index of the bridge where this channel is residing.
f_usChanIndex				Channel index of the channel to be modified.
f_ulNewListenerMask			New mask to apply to the selected participant.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiBridgeUpdateMask
UINT32 Oct6100ApiBridgeUpdateMask( 
				IN OUT	tPOCT6100_INSTANCE_API				f_pApiInstance, 
				IN		UINT16								f_usBridgeIndex, 
				IN		UINT16								f_usChanIndex, 
				IN		UINT32								f_ulNewListenerMask )
{
	tPOCT6100_API_CHANNEL				pChanEntry;
	tPOCT6100_API_CHANNEL				pTempEchoChanEntry;
	tPOCT6100_SHARED_INFO				pSharedInfo;
	tPOCT6100_API_FLEX_CONF_PARTICIPANT pParticipant;
	tPOCT6100_API_FLEX_CONF_PARTICIPANT pTempParticipant;
	tOCT6100_WRITE_PARAMS				WriteParams;

	UINT32	ulResult;
	UINT32	ulOldListenerMask;
	UINT16	usChannelIndex;

	UINT16	ausMutePortChannelIndexes[ cOCT6100_MAX_FLEX_CONF_PARTICIPANTS_PER_BRIDGE ];
	UINT32	ulMutePortChannelIndex;

	for( ulMutePortChannelIndex = 0; ulMutePortChannelIndex < cOCT6100_MAX_FLEX_CONF_PARTICIPANTS_PER_BRIDGE; ulMutePortChannelIndex ++ )
		ausMutePortChannelIndexes[ ulMutePortChannelIndex ] = cOCT6100_INVALID_INDEX;

	/* Obtain local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	WriteParams.pProcessContext = f_pApiInstance->pProcessContext;

	WriteParams.ulUserChipId = pSharedInfo->ChipConfig.ulUserChipId;

	/* Get a pointer to the channel's list entry. */
	mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pChanEntry, f_usChanIndex )

	mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pParticipant, pChanEntry->usFlexConfParticipantIndex );

	ulOldListenerMask = pParticipant->ulListenerMask;

	/* Search through the list of API channel entry for the ones onto this bridge. */
	for ( usChannelIndex = 0; usChannelIndex < pSharedInfo->ChipConfig.usMaxChannels; usChannelIndex++ )
	{
		mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pTempEchoChanEntry, usChannelIndex );
		
		/* Channel reserved? */
		if ( ( usChannelIndex != f_usChanIndex ) && ( pTempEchoChanEntry->fReserved == TRUE ) )
		{
			/* On current bridge? */
			if ( pTempEchoChanEntry->usBridgeIndex == f_usBridgeIndex )
			{
				mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pTempParticipant, pTempEchoChanEntry->usFlexConfParticipantIndex );

				/* Check if we can now hear this participant, but could not before. */
				if ( ( pTempEchoChanEntry->fMute == FALSE ) 
					&& ( ( f_ulNewListenerMask & ( 0x1 << pTempParticipant->ulListenerMaskIndex ) ) == 0x0 )
					&& ( ( ulOldListenerMask & ( 0x1 << pTempParticipant->ulListenerMaskIndex ) ) != 0x0 ) )
				{
					/* First create/update the current channel's mixer. */
					ulResult = Oct6100ApiBridgeAddParticipantToChannel(
												f_pApiInstance,
												f_usBridgeIndex,
												usChannelIndex,
												f_usChanIndex,
												pParticipant->ausLoadOrAccumulateEventIndex[ pTempParticipant->ulListenerMaskIndex ],
												pChanEntry->usSubStoreEventIndex,
												pChanEntry->usSinCopyEventIndex,
												pTempParticipant->ulInputPort,
												pParticipant->ulInputPort );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;

					if ( pParticipant->fFlexibleMixerCreated == TRUE )
					{
						/* Check if the Rin silence event can be cleared now that the */
						/* channel has been added to a conference. */
						if ( pChanEntry->usRinSilenceEventIndex != cOCT6100_INVALID_INDEX )
						{
							/* Remove the event from the list.*/
							ulResult = Oct6100ApiMixerEventRemove(	f_pApiInstance,
																	pChanEntry->usRinSilenceEventIndex,
																	cOCT6100_EVENT_TYPE_SOUT_COPY );
							if ( ulResult != cOCT6100_ERR_OK )
								return ulResult;

							ulResult = Oct6100ApiReleaseMixerEventEntry( f_pApiInstance, pChanEntry->usRinSilenceEventIndex );
							if ( ulResult != cOCT6100_ERR_OK  )
								return cOCT6100_ERR_FATAL_DF;

							pChanEntry->usRinSilenceEventIndex = cOCT6100_INVALID_INDEX;
						}
					}
				}

				/* Check if we can now NOT hear this participant, but could before. */
				if ( ( ( f_ulNewListenerMask & ( 0x1 << pTempParticipant->ulListenerMaskIndex ) ) != 0x0 )
					&& ( ( ulOldListenerMask & ( 0x1 << pTempParticipant->ulListenerMaskIndex ) ) == 0x0 )
					&& ( pParticipant->fFlexibleMixerCreated == TRUE ) 
					&& ( pTempEchoChanEntry->fMute == FALSE ) )
				{
					/* First update the current channel's mixer. */
					ulResult = Oct6100ApiBridgeRemoveParticipantFromChannel(
												f_pApiInstance,
												f_usBridgeIndex,
												usChannelIndex,
												f_usChanIndex,
												TRUE );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;
					
					if ( pParticipant->fFlexibleMixerCreated == FALSE )
					{
						/* Remember to mute the port on this channel. */
						for( ulMutePortChannelIndex = 0; ulMutePortChannelIndex < cOCT6100_MAX_FLEX_CONF_PARTICIPANTS_PER_BRIDGE; ulMutePortChannelIndex ++ )
						{
							if ( ausMutePortChannelIndexes[ ulMutePortChannelIndex ] == f_usChanIndex )
							{
								break;
							}
							else if ( ausMutePortChannelIndexes[ ulMutePortChannelIndex ] == cOCT6100_INVALID_INDEX )
							{
								ausMutePortChannelIndexes[ ulMutePortChannelIndex ] = f_usChanIndex;
								break;
							}
						}
					}
				}

				/* Clear the load or accumulate event index for this participant. */
				if ( ( ( f_ulNewListenerMask & ( 0x1 << pTempParticipant->ulListenerMaskIndex ) ) != 0x0 )
					&& ( ( ulOldListenerMask & ( 0x1 << pTempParticipant->ulListenerMaskIndex ) ) == 0x0 ) )
				{
					pParticipant->ausLoadOrAccumulateEventIndex[ pTempParticipant->ulListenerMaskIndex ] = cOCT6100_INVALID_INDEX;
				}
			}
		}

		/* Travel through the channels that were heard by the participant removed and check if their Rin port must be muted. */
		for( ulMutePortChannelIndex = 0; ulMutePortChannelIndex < cOCT6100_MAX_FLEX_CONF_PARTICIPANTS_PER_BRIDGE; ulMutePortChannelIndex ++ )
		{
			if ( ausMutePortChannelIndexes[ ulMutePortChannelIndex ] != cOCT6100_INVALID_INDEX )
			{
				mOCT6100_GET_CHANNEL_ENTRY_PNT( pSharedInfo, pTempEchoChanEntry, ausMutePortChannelIndexes[ ulMutePortChannelIndex ] );

				mOCT6100_GET_FLEX_CONF_PARTICIPANT_ENTRY_PNT( pSharedInfo, pTempParticipant, pTempEchoChanEntry->usFlexConfParticipantIndex );
				
				if ( pTempParticipant->fFlexibleMixerCreated == FALSE )
				{
					/* Check if the Rin port must be muted on this channel. */
					ulResult = Oct6100ApiMutePorts( 
											f_pApiInstance, 
											ausMutePortChannelIndexes[ ulMutePortChannelIndex ], 
											pTempEchoChanEntry->usRinTsstIndex, 
											pTempEchoChanEntry->usSinTsstIndex,
											FALSE );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;
				}
			}
			else /* if ( ausMutePortChannelIndexes[ ulMutePortChannelIndex ] == cOCT6100_INVALID_INDEX ) */
			{
				/* No more channels to check for muting. */
				break;
			}
		}
	}
	
	/* Configure the SIN copy mixer entry and memory - if using the SOUT port. */
	if ( pParticipant->ulInputPort == cOCT6100_CHANNEL_PORT_SOUT )
	{
		if ( pChanEntry->usSinTsstIndex != cOCT6100_INVALID_INDEX )
		{
			ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
															  pChanEntry->usSinTsstIndex,
															  pChanEntry->usExtraSinTsiMemIndex,
															  pChanEntry->TdmConfig.bySinPcmLaw );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}

		/* If the silence TSI is loaded on this port, update with the extra sin TSI. */
		if ( pChanEntry->usSinSilenceEventIndex != cOCT6100_INVALID_INDEX )
		{
			WriteParams.ulWriteAddress = cOCT6100_MIXER_CONTROL_MEM_BASE + ( pChanEntry->usSinSilenceEventIndex * cOCT6100_MIXER_CONTROL_MEM_ENTRY_SIZE );

			WriteParams.ulWriteAddress += 2;
			WriteParams.usWriteData = pChanEntry->usExtraSinTsiMemIndex;

			mOCT6100_DRIVER_WRITE_API( WriteParams, ulResult );
			if ( ulResult != cOCT6100_ERR_OK  )
				return ulResult;
		}
	}

	/* Configure the RIN copy mixer entry and memory - if using the RIN port. */
	if ( pParticipant->ulInputPort == cOCT6100_CHANNEL_PORT_RIN )
	{
		if ( pChanEntry->usRinTsstIndex != cOCT6100_INVALID_INDEX )
		{
			ulResult = Oct6100ApiWriteInputTsstControlMemory( f_pApiInstance,
															  pChanEntry->usRinTsstIndex,
															  pChanEntry->usExtraRinTsiMemIndex,
															  pChanEntry->TdmConfig.byRinPcmLaw );
			if ( ulResult != cOCT6100_ERR_OK )
				return ulResult;
		}
	}

	/* Save the new mask permanently in the API instance. */
	pParticipant->ulListenerMask = f_ulNewListenerMask;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ConfBridgeGetStatsSer

Description:    This function returns the statistics from the specified bridge.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_pConfBridgeStats		Pointer to conference bridge stats structure.  

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ConfBridgeGetStatsSer
UINT32 Oct6100ConfBridgeGetStatsSer(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN OUT	tPOCT6100_CONF_BRIDGE_STATS		f_pConfBridgeStats )
{
	tPOCT6100_API_CONF_BRIDGE		pBridgeEntry;
	UINT16	usConfBridgeIndex;
	UINT32	ulEntryOpenCnt;

	/* Check for errors. */
	if ( f_pApiInstance->pSharedInfo->ChipConfig.usMaxConfBridges == 0 )
		return cOCT6100_ERR_CONF_BRIDGE_DISABLED;

	/*=====================================================================*/
	/* Check the conference bridge handle. */

	/* Check the provided handle. */
	if ( (f_pConfBridgeStats->ulConfBridgeHndl & cOCT6100_HNDL_TAG_MASK) != cOCT6100_HNDL_TAG_CONF_BRIDGE )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	usConfBridgeIndex = (UINT16)( f_pConfBridgeStats->ulConfBridgeHndl & cOCT6100_HNDL_INDEX_MASK );
	if ( usConfBridgeIndex >= f_pApiInstance->pSharedInfo->ChipConfig.usMaxConfBridges )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	mOCT6100_GET_CONF_BRIDGE_ENTRY_PNT( f_pApiInstance->pSharedInfo, pBridgeEntry, usConfBridgeIndex )

	/* Extract the entry open count from the provided handle. */
	ulEntryOpenCnt = (f_pConfBridgeStats->ulConfBridgeHndl >> cOCT6100_ENTRY_OPEN_CNT_SHIFT) & cOCT6100_ENTRY_OPEN_CNT_MASK;

	/* Check for errors. */
	if ( pBridgeEntry->fReserved != TRUE )
		return cOCT6100_ERR_CONF_BRIDGE_NOT_OPEN;
	if ( ulEntryOpenCnt != pBridgeEntry->byEntryOpenCnt )
		return cOCT6100_ERR_CONF_BRIDGE_INVALID_HANDLE;

	/*=====================================================================*/

	/* Return the stats.*/
	f_pConfBridgeStats->ulNumChannels = pBridgeEntry->usNumClients;
	f_pConfBridgeStats->ulNumTappedChannels = pBridgeEntry->usNumTappedClients;
	f_pConfBridgeStats->fFlexibleConferencing = pBridgeEntry->fFlexibleConferencing;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReserveBridgeEntry

Description:    Reserves a free entry in the Bridge list.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.
f_pusBridgeIndex		List entry reserved.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReserveBridgeEntry
UINT32 Oct6100ApiReserveBridgeEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				OUT		PUINT16							f_pusBridgeIndex )
{
	PVOID	pBridgeAlloc;
	UINT32	ulResult;
	UINT32	ulBridgeIndex;

	mOCT6100_GET_CONF_BRIDGE_ALLOC_PNT( f_pApiInstance->pSharedInfo, pBridgeAlloc )

	ulResult = OctapiLlmAllocAlloc( pBridgeAlloc, &ulBridgeIndex );
	if ( ulResult != cOCT6100_ERR_OK  )
	{
		if ( ulResult == OCTAPI_LLM_NO_STRUCTURES_LEFT )
			return cOCT6100_ERR_CONF_BRIDGE_ALL_BUFFERS_OPEN;
		else
			return cOCT6100_ERR_FATAL_29;
	}

	*f_pusBridgeIndex = (UINT16)( ulBridgeIndex & 0xFFFF );

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseBridgeEntry

Description:    Release an entry from the bridge list.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance		Pointer to API instance. This memory is used to keep the
					present state of the chip and all its resources.

f_usBridgeIndex		List entry reserved.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseBridgeEntry
UINT32 Oct6100ApiReleaseBridgeEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usBridgeIndex )
{
	PVOID	pBridgeAlloc;
	UINT32	ulResult;

	mOCT6100_GET_CONF_BRIDGE_ALLOC_PNT( f_pApiInstance->pSharedInfo, pBridgeAlloc )

	ulResult = OctapiLlmAllocDealloc( pBridgeAlloc, f_usBridgeIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return cOCT6100_ERR_FATAL_2A;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiGetPrevLastSubStoreEvent

Description:    This function will search for the first valid LastSubStoreEvent 
				in a bridge located before the current bridge in the bridge 
				link list.
				
				If the function does not find an event before reaching the end
				of the mixers list, then the event head node will be used as the
				last Store or SubStore event.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance				Pointer to API instance. This memory is used to keep the
							present state of the chip and all its resources.

f_pusBridgeEntry			Bridge entry.
f_usBridgeFirstLoadEventPtr	Load index to check against.
First						valid sub store index.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiGetPrevLastSubStoreEvent
UINT32 Oct6100ApiGetPrevLastSubStoreEvent(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usBridgeIndex,
				IN		UINT16							f_usBridgeFirstLoadEventPtr,
				OUT		PUINT16							f_pusLastSubStoreEventIndex )
{
	tPOCT6100_API_MIXER_EVENT	pTempMixerEntry;
	UINT16						usNextEventPtr;
	UINT16						usHeadEventPtr;
	UINT16						usLastSubStoreEventPtr;
	UINT32						ulLoopCount = 0;
	UINT16						usCurrentPtr;
	UINT32						ulResult = cOCT6100_ERR_OK;

	/* Since we have flexible bridges, we have to */
	/* run down the list and check for the appropriate event. */

	/* Travel down the list for the last Store or Sub/Store event before the bridge. */

	if ( f_pApiInstance->pSharedInfo->MixerInfo.usLastSoutCopyEventPtr == cOCT6100_INVALID_INDEX )
	{
		/* The only node in the list then is the head node.*/
		usHeadEventPtr = cOCT6100_MIXER_HEAD_NODE;
	}
	else
	{
		usHeadEventPtr = f_pApiInstance->pSharedInfo->MixerInfo.usLastSoutCopyEventPtr;
	}

	mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( f_pApiInstance->pSharedInfo, pTempMixerEntry, usHeadEventPtr );
	usLastSubStoreEventPtr = usHeadEventPtr;
	usNextEventPtr = pTempMixerEntry->usNextEventPtr;
	usCurrentPtr = usHeadEventPtr;
	while( usCurrentPtr != f_usBridgeFirstLoadEventPtr )
	{
		if ( ( pTempMixerEntry->usEventType == cOCT6100_MIXER_CONTROL_MEM_STORE )
			|| ( pTempMixerEntry->usEventType == cOCT6100_MIXER_CONTROL_MEM_SUB_STORE ) )
		{
			usLastSubStoreEventPtr = usNextEventPtr;
		}

		/* Next pointer. */
		usCurrentPtr = usNextEventPtr;
		usNextEventPtr = pTempMixerEntry->usNextEventPtr;

		/* Check if next event pointer is valid. */
		if ( ( ( f_usBridgeFirstLoadEventPtr != usCurrentPtr ) 
			&& ( pTempMixerEntry->usNextEventPtr == cOCT6100_INVALID_INDEX ) )
				|| ( pTempMixerEntry->usNextEventPtr == cOCT6100_MIXER_HEAD_NODE ) )
			return cOCT6100_ERR_CONF_MIXER_EVENT_NOT_FOUND;

		if ( usNextEventPtr != cOCT6100_INVALID_INDEX )
			mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( f_pApiInstance->pSharedInfo, pTempMixerEntry, usNextEventPtr );
		
		ulLoopCount++;
		if ( ulLoopCount == cOCT6100_MAX_LOOP )
			return cOCT6100_ERR_FATAL_CA;
	}
	
	/* Return the result to the user. */
	*f_pusLastSubStoreEventIndex = usLastSubStoreEventPtr;

	return ulResult;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiGetPreviousEvent

Description:    This is a recursive function, it requires an entry event index and 
				will run down the list until it finds the node just before the one
				required.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_usEntryIndex			Event entry index.
f_pusBridgeEntry		Bridge entry.
f_pusPreviousIndex		Previous index.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiGetPreviousEvent
UINT32 Oct6100ApiGetPreviousEvent(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usEntryIndex,
				IN		UINT16							f_usSearchedIndex,
				IN		UINT16							f_usLoopCnt,
				OUT		PUINT16							f_pusPreviousIndex )
{
	tPOCT6100_API_MIXER_EVENT	pCurrentEntry;
	UINT32	ulResult;

	/* Get current entry to obtain the link to the previous entry. */
	mOCT6100_GET_MIXER_EVENT_ENTRY_PNT( f_pApiInstance->pSharedInfo, pCurrentEntry, f_usEntryIndex );

	/* Avoid stack overflows. */
	if ( f_usLoopCnt == cOCT6100_MAX_MIXER_EVENTS )
		return cOCT6100_ERR_FATAL_E3;

	if ( pCurrentEntry->usNextEventPtr == cOCT6100_INVALID_INDEX )
	{
		/* Event not found. */
		ulResult = cOCT6100_ERR_CONF_MIXER_EVENT_NOT_FOUND;
	}
	else if ( pCurrentEntry->usNextEventPtr == f_usSearchedIndex )
	{
		/* We found our node. */
		*f_pusPreviousIndex = f_usEntryIndex;
		ulResult = cOCT6100_ERR_OK; 
	}
	else
	{
		/* Keep searching.*/
		f_usLoopCnt++;
		ulResult = Oct6100ApiGetPreviousEvent( f_pApiInstance, pCurrentEntry->usNextEventPtr, f_usSearchedIndex, f_usLoopCnt, f_pusPreviousIndex );
	}

	return ulResult;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiBridgeSetDominantSpeaker

Description:    This function will set the index of the dominant speaker
				for the channel index specified.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance					Pointer to API instance. This memory is used to 
								keep the present state of the chip and all its 
								resources.

f_usChannelIndex				Index of the channel where the API must set the
								current dominant speaker for the conference.
f_usDominantSpeakerIndex		Index of the channel which is the dominant
								speaker in the conference.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiBridgeSetDominantSpeaker
UINT32 Oct6100ApiBridgeSetDominantSpeaker(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usChannelIndex,
				IN		UINT16							f_usDominantSpeakerIndex )
{
	UINT32	ulBaseAddress;
	UINT32	ulFeatureBytesOffset;
	UINT32	ulFeatureBitOffset;
	UINT32	ulFeatureFieldLength;
	UINT32	ulResult;
	UINT32	ulTempData;
	UINT32	ulMask;

	tPOCT6100_API_CHANNEL	pEchoChanEntry;

	mOCT6100_GET_CHANNEL_ENTRY_PNT( f_pApiInstance->pSharedInfo, pEchoChanEntry, f_usChannelIndex );

	ulBaseAddress		 = cOCT6100_CHANNEL_ROOT_BASE + ( f_usChannelIndex * cOCT6100_CHANNEL_ROOT_SIZE ) + f_pApiInstance->pSharedInfo->MemoryMap.ulChanRootConfOfst;
	ulFeatureBytesOffset = f_pApiInstance->pSharedInfo->MemoryMap.DominantSpeakerFieldOfst.usDwordOffset * 4;
	ulFeatureBitOffset	 = f_pApiInstance->pSharedInfo->MemoryMap.DominantSpeakerFieldOfst.byBitOffset;
	ulFeatureFieldLength = f_pApiInstance->pSharedInfo->MemoryMap.DominantSpeakerFieldOfst.byFieldSize;

	/* Retrieve the current configuration. */
	ulResult = oct6100_retrieve_nlp_conf_dword(f_pApiInstance,
										pEchoChanEntry,
										ulBaseAddress + ulFeatureBytesOffset,
										&ulTempData);
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;

	/* Clear previous value set in the feature field.*/
	mOCT6100_CREATE_FEATURE_MASK( ulFeatureFieldLength, ulFeatureBitOffset, &ulMask );

	ulTempData &= (~ulMask);
	ulTempData |= ( ( f_usDominantSpeakerIndex ) << ulFeatureBitOffset );

	/* Save the new dominant speaker. */
	ulResult = oct6100_save_nlp_conf_dword(f_pApiInstance,
									pEchoChanEntry,
									ulBaseAddress + ulFeatureBytesOffset,
									ulTempData);
	if ( ulResult != cOCT6100_ERR_OK )
		return ulResult;	

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReserveFlexConfParticipantEntry

Description:    Reserves a free entry in the participant list.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.
f_pusParticipantIndex	List entry reserved.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReserveFlexConfParticipantEntry
UINT32 Oct6100ApiReserveFlexConfParticipantEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				OUT		PUINT16							f_pusParticipantIndex )
{
	PVOID	pParticipantAlloc;
	UINT32	ulResult;
	UINT32	ulParticipantIndex;

	mOCT6100_GET_FLEX_CONF_PARTICIPANT_ALLOC_PNT( f_pApiInstance->pSharedInfo, pParticipantAlloc )

	ulResult = OctapiLlmAllocAlloc( pParticipantAlloc, &ulParticipantIndex );
	if ( ulResult != cOCT6100_ERR_OK )
	{
		if ( ulResult == OCTAPI_LLM_NO_STRUCTURES_LEFT )
			return cOCT6100_ERR_CONF_BRIDGE_FLEX_CONF_ALL_BUFFERS_OPEN;
		else
			return cOCT6100_ERR_FATAL_29;
	}

	*f_pusParticipantIndex = (UINT16)( ulParticipantIndex & 0xFFFF );

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseFlexConfParticipantEntry

Description:    Release an entry from the flexible conferencing participant 
				list.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep the
						present state of the chip and all its resources.

f_usParticipantIndex	List entry reserved.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseFlexConfParticipantEntry
UINT32 Oct6100ApiReleaseFlexConfParticipantEntry(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN		UINT16							f_usParticipantIndex )
{
	PVOID	pParticipantAlloc;
	UINT32	ulResult;

	mOCT6100_GET_FLEX_CONF_PARTICIPANT_ALLOC_PNT( f_pApiInstance->pSharedInfo, pParticipantAlloc )

	ulResult = OctapiLlmAllocDealloc( pParticipantAlloc, f_usParticipantIndex );
	if ( ulResult != cOCT6100_ERR_OK )
		return cOCT6100_ERR_FATAL_2A;

	return cOCT6100_ERR_OK;
}
#endif
