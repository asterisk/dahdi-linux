/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: oct6100_memory.c

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	This file contains the functions used to manage the allocation of memory
	blocks in external memory.

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

$Octasic_Revision: 42 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/


/*****************************  INCLUDE FILES  *******************************/

#include "octdef.h"

#include "oct6100api/oct6100_defines.h"
#include "oct6100api/oct6100_errors.h"

#include "apilib/octapi_llman.h"

#include "oct6100api/oct6100_apiud.h"
#include "oct6100api/oct6100_tlv_inst.h"
#include "oct6100api/oct6100_chip_open_inst.h"
#include "oct6100api/oct6100_chip_stats_inst.h"
#include "oct6100api/oct6100_interrupts_inst.h"
#include "oct6100api/oct6100_remote_debug_inst.h"
#include "oct6100api/oct6100_debug_inst.h"
#include "oct6100api/oct6100_playout_buf_inst.h"
#include "oct6100api/oct6100_api_inst.h"

#include "oct6100api/oct6100_interrupts_pub.h"
#include "oct6100api/oct6100_channel_pub.h"
#include "oct6100api/oct6100_chip_open_pub.h"

#include "oct6100_chip_open_priv.h"
#include "oct6100_memory_priv.h"


/****************************  PRIVATE FUNCTIONS  ****************************/


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiGetMemorySwSizes

Description:    Gets the sizes of all portions of the API instance pertinent
				to the management of the memories.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pOpenChip				Pointer to chip configuration struct.
f_pInstSizes			Pointer to struct containing instance sizes.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiGetMemorySwSizes
UINT32 Oct6100ApiGetMemorySwSizes(
				IN		tPOCT6100_CHIP_OPEN				f_pOpenChip,
				OUT		tPOCT6100_API_INSTANCE_SIZES	f_pInstSizes )
{
	UINT32	ulTempVar;
	UINT32	ulResult;
	UINT32	ulNumTsiChariots;

	/*=========================================================================*/
	/* Internal memory */

	/* Evaluate the number of available TSI memory after reserving the ones used by channels. */
	ulNumTsiChariots = cOCT6100_TOTAL_TSI_CONTROL_MEM_ENTRY - f_pOpenChip->ulMaxPhasingTssts - cOCT6100_TSI_MEM_FOR_TIMESTAMP;

	if ( f_pOpenChip->fEnableExtToneDetection == TRUE )
		ulNumTsiChariots--;

	/* Calculate memory needed for TSI memory allocation. */
	ulResult = OctapiLlmAllocGetSize( ulNumTsiChariots, &f_pInstSizes->ulTsiMemoryAlloc );
	if ( ulResult != cOCT6100_ERR_OK )
		return cOCT6100_ERR_FATAL_94;
	
	/* Calculate memory needed for conversion memory allocation. */
	ulResult = OctapiLlmAllocGetSize( cOCT6100_MAX_CONVERSION_MEMORY_BLOCKS, &f_pInstSizes->ulConversionMemoryAlloc );
	if ( ulResult != cOCT6100_ERR_OK )
		return cOCT6100_ERR_FATAL_B5;

	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulTsiMemoryAlloc, ulTempVar );
	mOCT6100_ROUND_MEMORY_SIZE( f_pInstSizes->ulConversionMemoryAlloc, ulTempVar );

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiMemorySwInit

Description:    Initializes all elements of the instance structure associated
				to memories.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiMemorySwInit
UINT32 Oct6100ApiMemorySwInit(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance )
{
	tPOCT6100_SHARED_INFO	pSharedInfo;
	PVOID	pTsiMemAlloc;
	PVOID	pAllocPnt;
	UINT32	ulResult;

	/* Get local pointer(s). */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/*=========================================================================*/
	/* Internal memory */
	
	/* Initialize the TSI memory allocation structure. */
	pSharedInfo->MemoryMap.ulNumTsiEntries = cOCT6100_TOTAL_TSI_CONTROL_MEM_ENTRY - pSharedInfo->ChipConfig.usMaxPhasingTssts - cOCT6100_TSI_MEM_FOR_TIMESTAMP;

	if ( pSharedInfo->ChipConfig.fEnableExtToneDetection == TRUE )
		pSharedInfo->MemoryMap.ulNumTsiEntries--;

	mOCT6100_GET_TSI_MEMORY_ALLOC_PNT( pSharedInfo, pTsiMemAlloc );
	
	ulResult = OctapiLlmAllocInit( &pTsiMemAlloc, pSharedInfo->MemoryMap.ulNumTsiEntries );
	if ( ulResult != cOCT6100_ERR_OK )
		return cOCT6100_ERR_FATAL_95;

	/* Initialize the conversion memory allocation structure. */
	mOCT6100_GET_CONVERSION_MEMORY_ALLOC_PNT( pSharedInfo, pAllocPnt );
	
	ulResult = OctapiLlmAllocInit( &pAllocPnt, cOCT6100_MAX_CONVERSION_MEMORY_BLOCKS );
	if ( ulResult != cOCT6100_ERR_OK )
		return cOCT6100_ERR_FATAL_B6;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiBufferPlayoutMemorySwInit

Description:    Initialize the buffer playout memory allocation working 
				structures.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiBufferPlayoutMemorySwInit
UINT32 Oct6100ApiBufferPlayoutMemorySwInit(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance )
{
	tPOCT6100_SHARED_INFO						pSharedInfo;
	tPOCT6100_API_BUFFER_PLAYOUT_MALLOC_NODE	pNode;
	UINT32										i;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Only if buffer playout will be used. */
	if ( pSharedInfo->ChipConfig.usMaxPlayoutBuffers > 0 )
	{
		mOCT6100_GET_BUFFER_MEMORY_NODE_LIST_PNT( pSharedInfo, pNode );

		/* First node contains all free memory at beginning. This node is not used, but represents the memory. */
		pNode->ulSize = ( pSharedInfo->MiscVars.ulTotalMemSize + cOCT6100_EXTERNAL_MEM_BASE_ADDRESS ) - pSharedInfo->MemoryMap.ulFreeMemBaseAddress;
		pNode->ulNext = 0; 
		pNode->ulPrevious = 0; 
		pNode->fAllocated = FALSE;
		pNode->ulStartAddress = pSharedInfo->MemoryMap.ulFreeMemBaseAddress;

		pNode++;

		/* Now create the first node of the free list, i.e. nodes that can be used later for modeling the memory. */
		pNode->ulSize = 0;
		/* Next free. */
		pNode->ulNext = 2;
		/* Last. */
		pNode->ulPrevious = ( pSharedInfo->ChipConfig.usMaxPlayoutBuffers * 2 ) - 1;
		pNode->fAllocated = FALSE;
		pNode->ulStartAddress = 0;

		pNode++;

		/* Link all the unused nodes. */
		for( i = 2; i < (UINT32)( ( pSharedInfo->ChipConfig.usMaxPlayoutBuffers * 2 ) - 1 ); i ++ )
		{
			pNode->ulNext = i + 1;
			pNode->ulPrevious = i - 1;
			pNode->ulStartAddress = 0;
			pNode->ulSize = 0;
			pNode->fAllocated = FALSE;
			pNode++;
		}

		/* Last node of the unused list. */
		pNode->fAllocated = FALSE;
		pNode->ulPrevious = ( pSharedInfo->ChipConfig.usMaxPlayoutBuffers * 2 ) - 2;
		/* Free list head. */
		pNode->ulNext = 1; 
		pNode->ulSize = 0;
		pNode->ulStartAddress = 0;

		/* Set roving pointer to first node ( which can be used! ) */
		pSharedInfo->PlayoutInfo.ulRovingNode = 0;

		/* First unused node. */
		pSharedInfo->PlayoutInfo.ulFirstUnusedNode = 1;

		/* Last unused node. */
		pSharedInfo->PlayoutInfo.ulLastUnusedNode = ( pSharedInfo->ChipConfig.usMaxPlayoutBuffers * 2 ) - 1;

		/* Number of unused nodes. */
		pSharedInfo->PlayoutInfo.ulUnusedNodeCnt = ( pSharedInfo->ChipConfig.usMaxPlayoutBuffers * 2 ) - 1;
	}
	else
	{
		pSharedInfo->PlayoutInfo.ulUnusedNodeCnt = 0;
	}

	return cOCT6100_ERR_OK;
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReserveBufferPlayoutMemoryNode

Description:    Get a free node from the unused buffer playout node list.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pulNewNode			The index of the node.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReserveBufferPlayoutMemoryNode
UINT32 Oct6100ApiReserveBufferPlayoutMemoryNode( 
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance,
				OUT		PUINT32					f_pulNewNode )
{
	tPOCT6100_SHARED_INFO						pSharedInfo;
	tPOCT6100_API_BUFFER_PLAYOUT_MALLOC_NODE	pNode;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Check if a free block is left. */
	if ( pSharedInfo->PlayoutInfo.ulUnusedNodeCnt == 0 )
	{
		/* This should not happen according to the allocated list from the beginning. */
		return cOCT6100_ERR_FATAL_CC;
	}

	/* The new node is the first in the unused list. */
	*f_pulNewNode = pSharedInfo->PlayoutInfo.ulFirstUnusedNode;

	/* Unlink this new node from the unused list. */
	mOCT6100_GET_BUFFER_MEMORY_NODE_ENTRY_PNT( pSharedInfo, pNode, *f_pulNewNode );

	pSharedInfo->PlayoutInfo.ulFirstUnusedNode = pNode->ulNext;

	pNode->ulPrevious = pSharedInfo->PlayoutInfo.ulLastUnusedNode;

	mOCT6100_GET_BUFFER_MEMORY_NODE_ENTRY_PNT( pSharedInfo, pNode, pSharedInfo->PlayoutInfo.ulLastUnusedNode );

	pNode->ulNext = pSharedInfo->PlayoutInfo.ulFirstUnusedNode;

	/* Update unused node count. */
	pSharedInfo->PlayoutInfo.ulUnusedNodeCnt--;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseBufferPlayoutMemoryNode

Description:    Release a node that is not used anymore.  Insert this node 
				into the unused list.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_ulOldNode				The index of the node.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseBufferPlayoutMemoryNode
UINT32 Oct6100ApiReleaseBufferPlayoutMemoryNode( 
					IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance,
					IN		UINT32					f_ulOldNode )
{
	tPOCT6100_SHARED_INFO						pSharedInfo;
	tPOCT6100_API_BUFFER_PLAYOUT_MALLOC_NODE	pNode;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Get the last unused node.  Insert this old node at the end of the unused list. */
	mOCT6100_GET_BUFFER_MEMORY_NODE_ENTRY_PNT( pSharedInfo, pNode, pSharedInfo->PlayoutInfo.ulLastUnusedNode );

	/* Last node points to old node. */
	pNode->ulNext = f_ulOldNode;

	/* Update old node. */
	mOCT6100_GET_BUFFER_MEMORY_NODE_ENTRY_PNT( pSharedInfo, pNode, f_ulOldNode );

	pNode->ulPrevious = pSharedInfo->PlayoutInfo.ulLastUnusedNode;
	pNode->ulNext = pSharedInfo->PlayoutInfo.ulFirstUnusedNode;
	pSharedInfo->PlayoutInfo.ulLastUnusedNode = f_ulOldNode;

	/* Keep unused node count. */
	pSharedInfo->PlayoutInfo.ulUnusedNodeCnt++;

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReserveBufferPlayoutMemory

Description:    Try to allocate requested size.
				Returns an error if malloc point could not be found.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_ulSize				Needed size.
f_pulMallocAddress		Alloc point.  This memory can now be used.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReserveBufferPlayoutMemory
UINT32 Oct6100ApiReserveBufferPlayoutMemory( 
					IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance,
					IN		UINT32					f_ulSize, 
					OUT		PUINT32					f_pulMallocAddress )
{
	tPOCT6100_SHARED_INFO						pSharedInfo;
	tPOCT6100_API_BUFFER_PLAYOUT_MALLOC_NODE	pCurrentNode;
	tPOCT6100_API_BUFFER_PLAYOUT_MALLOC_NODE	pTempNode;
	tPOCT6100_API_BUFFER_PLAYOUT_MALLOC_NODE	pNewNode;
	
	UINT32 ulCurrentBufferPlayoutMallocNode;
	UINT32 ulNewNode;
	BOOL fFoundMemory = FALSE;
	UINT32 ulResult;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;
	
	/* Requested size must be divisible by 64. */
	if ( f_ulSize % 64 )
	{
		f_ulSize = f_ulSize + ( 64 - ( f_ulSize % 64 ) );
	}

	/* Start with roving pointer. */
	ulCurrentBufferPlayoutMallocNode = pSharedInfo->PlayoutInfo.ulRovingNode;

	*f_pulMallocAddress = 0;

	/* Return an error if size requested is zero. */
	if ( f_ulSize == 0 )
	{
		return cOCT6100_ERR_BUFFER_PLAYOUT_MALLOC_ZERO;
	}

	do
	{
		mOCT6100_GET_BUFFER_MEMORY_NODE_ENTRY_PNT( pSharedInfo, pCurrentNode, ulCurrentBufferPlayoutMallocNode );

		/* Look for a free node big enough to fulfill user requested size. */
		if ( ( pCurrentNode->fAllocated == FALSE ) && ( pCurrentNode->ulSize >= f_ulSize ) )
		{
			/* Use this node! */
			pCurrentNode->fAllocated = TRUE;

			if ( pCurrentNode->ulNext != 0 )
			{
				mOCT6100_GET_BUFFER_MEMORY_NODE_ENTRY_PNT( pSharedInfo, pTempNode, pCurrentNode->ulNext );

				if( ( pTempNode->fAllocated == TRUE ) && ( pCurrentNode->ulSize > f_ulSize ) )
				{
					/* Fragmentation NOW! */

					/* Allocate new node that will contain free size. */
					ulResult = Oct6100ApiReserveBufferPlayoutMemoryNode( f_pApiInstance, &ulNewNode );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;

					mOCT6100_GET_BUFFER_MEMORY_NODE_ENTRY_PNT( pSharedInfo, pNewNode, ulNewNode );

					/* Can use this free node. */
					pNewNode->ulSize = pCurrentNode->ulSize - f_ulSize;
					pNewNode->ulStartAddress = pCurrentNode->ulStartAddress + f_ulSize;

					/* Link new node into the list. */
					pNewNode->ulNext = pCurrentNode->ulNext;
					pNewNode->ulPrevious = ulCurrentBufferPlayoutMallocNode;
					pNewNode->fAllocated = FALSE;
					pTempNode->ulPrevious = ulNewNode;
					pCurrentNode->ulNext = ulNewNode;
				}
			}
			else if ( pCurrentNode->ulSize > f_ulSize )
			{
				/* Must allocate a new free node for the rest of the space. */
				ulResult = Oct6100ApiReserveBufferPlayoutMemoryNode( f_pApiInstance, &ulNewNode );
				if ( ulResult != cOCT6100_ERR_OK )
					return ulResult;

				mOCT6100_GET_BUFFER_MEMORY_NODE_ENTRY_PNT( pSharedInfo, pNewNode, ulNewNode );

				pNewNode->ulNext = pCurrentNode->ulNext;
				pCurrentNode->ulNext = ulNewNode;
				pNewNode->ulPrevious = ulCurrentBufferPlayoutMallocNode;
				pNewNode->fAllocated = FALSE;
				pNewNode->ulSize = pCurrentNode->ulSize - f_ulSize;
				pNewNode->ulStartAddress = pCurrentNode->ulStartAddress + f_ulSize;

				mOCT6100_GET_BUFFER_MEMORY_NODE_ENTRY_PNT( pSharedInfo, pTempNode, 0 );

				/* Check for the head node that would have to be updated. */
				if ( ( ulCurrentBufferPlayoutMallocNode == 0 ) && ( pTempNode->ulPrevious == 0 ) )
					pTempNode->ulPrevious = ulNewNode;
			}
			else
			{
				/* Perfect fit. */
			}
			pCurrentNode->ulSize = f_ulSize;

			/* Update roving pointer. */
			pSharedInfo->PlayoutInfo.ulRovingNode = ulCurrentBufferPlayoutMallocNode;
			*f_pulMallocAddress = pCurrentNode->ulStartAddress;
			fFoundMemory = TRUE;
			break;
		}

		/* Next block! */
		ulCurrentBufferPlayoutMallocNode = pCurrentNode->ulNext;

	} while ( pSharedInfo->PlayoutInfo.ulRovingNode != ulCurrentBufferPlayoutMallocNode );

	if ( fFoundMemory == FALSE )
	{
		return cOCT6100_ERR_BUFFER_PLAYOUT_NO_MEMORY;
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseBufferPlayoutMemory

Description:    Free what was allocated at address.  Free is somewhat slower 
				then Malloc.  O(n), must travel through the list looking for 
				the malloc point.  Return an error if alloc point was not found.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_ulMallocAddress		Alloc point.  The memory at address will be freed.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseBufferPlayoutMemory
UINT32 Oct6100ApiReleaseBufferPlayoutMemory(
				IN OUT	tPOCT6100_INSTANCE_API	f_pApiInstance,
				IN		UINT32					f_ulMallocAddress )
{
	tPOCT6100_SHARED_INFO						pSharedInfo;
	tPOCT6100_API_BUFFER_PLAYOUT_MALLOC_NODE	pCurrentNode;
	tPOCT6100_API_BUFFER_PLAYOUT_MALLOC_NODE	pTempNode;
	tPOCT6100_API_BUFFER_PLAYOUT_MALLOC_NODE	pOldNode;

	UINT32 ulResult = cOCT6100_ERR_BUFFER_PLAYOUT_MALLOC_POINT_NOT_FOUND;
	UINT32 ulNodeToMerge;
	UINT32 ulNodeToRemove;
	UINT32 ulCurrentBufferPlayoutMallocNode;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Start from the beginning and find the alloc node. */
	ulCurrentBufferPlayoutMallocNode = 0;
	
	do
	{
		mOCT6100_GET_BUFFER_MEMORY_NODE_ENTRY_PNT( pSharedInfo, pCurrentNode, ulCurrentBufferPlayoutMallocNode );

		if ( ( pCurrentNode->ulStartAddress == f_ulMallocAddress ) && ( pCurrentNode->fAllocated == TRUE ) )
		{
			/* We found the block! */
			pCurrentNode->fAllocated = FALSE;

			/* Check if the next node can be merged. */
			if ( pCurrentNode->ulNext != 0 )
			{
				mOCT6100_GET_BUFFER_MEMORY_NODE_ENTRY_PNT( pSharedInfo, pTempNode, pCurrentNode->ulNext );

				if ( pTempNode->fAllocated == FALSE )
				{
					/* Can merge this block to us. */
					pCurrentNode->ulSize += pTempNode->ulSize;
					pTempNode->ulSize = 0;

					/* Unlink unused node. */
					ulNodeToRemove = pCurrentNode->ulNext;
					pCurrentNode->ulNext = pTempNode->ulNext;

					mOCT6100_GET_BUFFER_MEMORY_NODE_ENTRY_PNT( pSharedInfo, pTempNode, pCurrentNode->ulNext );

					pTempNode->ulPrevious = ulCurrentBufferPlayoutMallocNode;

					ulResult = Oct6100ApiReleaseBufferPlayoutMemoryNode( f_pApiInstance, ulNodeToRemove );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;
					
					/* Move roving pointer if have to. */
					if ( pSharedInfo->PlayoutInfo.ulRovingNode == ulNodeToRemove )
						pSharedInfo->PlayoutInfo.ulRovingNode = ulCurrentBufferPlayoutMallocNode;
				}
			}

			/* Check if previous node can merge. */
			if ( ulCurrentBufferPlayoutMallocNode != 0 )
			{
				mOCT6100_GET_BUFFER_MEMORY_NODE_ENTRY_PNT( pSharedInfo, pTempNode, pCurrentNode->ulPrevious );

				if ( pTempNode->fAllocated == FALSE )
				{
					ulNodeToMerge = pCurrentNode->ulPrevious;

					/* Can merge us to this node. */
					pTempNode->ulSize += pCurrentNode->ulSize;
					pCurrentNode->ulSize = 0;

					/* Unlink unused node. */
					ulNodeToRemove = ulCurrentBufferPlayoutMallocNode;

					mOCT6100_GET_BUFFER_MEMORY_NODE_ENTRY_PNT( pSharedInfo, pOldNode, ulNodeToRemove );

					pTempNode->ulNext = pOldNode->ulNext;

					mOCT6100_GET_BUFFER_MEMORY_NODE_ENTRY_PNT( pSharedInfo, pTempNode, pTempNode->ulNext );

					pTempNode->ulPrevious = ulNodeToMerge;

					pOldNode->fAllocated = FALSE;
					pOldNode->ulSize = 0;
					pOldNode->ulStartAddress = 0; 

					/* Move roving pointer if have to. */
					if ( pSharedInfo->PlayoutInfo.ulRovingNode == ulNodeToRemove )
						pSharedInfo->PlayoutInfo.ulRovingNode = ulNodeToMerge;

					/* Release this unused node. */
					ulResult = Oct6100ApiReleaseBufferPlayoutMemoryNode( f_pApiInstance, ulNodeToRemove );
					if ( ulResult != cOCT6100_ERR_OK )
						return ulResult;
				}
			}

			/* All's good! */
			ulResult = 0;
			break;
		}

		/* Next node. */
		ulCurrentBufferPlayoutMallocNode = pCurrentNode->ulNext;
		
	} while( ulCurrentBufferPlayoutMallocNode != 0 );

	return ulResult;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReserveTsiMemEntry

Description:    Reserves a TSI chariot memory entry.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_pusTsiMemIndex		Resulting index reserved in the TSI chariot memory.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReserveTsiMemEntry
UINT32 Oct6100ApiReserveTsiMemEntry(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				OUT		PUINT16						f_pusTsiMemIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	PVOID	pTsiMemAlloc;
	UINT32	ulResult;
	UINT32	ulIndex;
	UINT32	ulNumTsiB4Timestamp;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	mOCT6100_GET_TSI_MEMORY_ALLOC_PNT( pSharedInfo, pTsiMemAlloc )
	
	ulResult = OctapiLlmAllocAlloc( pTsiMemAlloc, &ulIndex );
	if ( ulResult != cOCT6100_ERR_OK )
	{
		if ( ulResult == OCTAPI_LLM_NO_STRUCTURES_LEFT )
			return cOCT6100_ERR_MEMORY_ALL_TSI_MEM_ENTRY_RESERVED;
		else
			return cOCT6100_ERR_FATAL_92;
	}


	if ( ulIndex >= cOCT6100_NUM_TSI_B4_PHASING )
	{
		/* Evaluate the number of TSI memory before the timestamp TSI. */
		ulNumTsiB4Timestamp = cOCT6100_NUM_TSI_B4_PHASING + cOCT6100_MAX_TSI_B4_TIMESTAMP - pSharedInfo->ChipConfig.usMaxPhasingTssts;
		
		if ( ulIndex >= ulNumTsiB4Timestamp )
		{
			/* + 4 for the timestamp TSI entries.*/
			*f_pusTsiMemIndex = (UINT16)( pSharedInfo->ChipConfig.usMaxPhasingTssts + ulIndex + cOCT6100_TSI_MEM_FOR_TIMESTAMP );
		}
		else /* ulIndex < ulNumTsiB4Timestamp */
		{
			*f_pusTsiMemIndex = (UINT16)( pSharedInfo->ChipConfig.usMaxPhasingTssts + ulIndex );
		}
	}
	else /* ulIndex < ulNumTsiB4Timestamp */
	{
		*f_pusTsiMemIndex = (UINT16)( ulIndex );
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseTsiMemEntry

Description:    Releases a TSI chariot memory entry specified.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to keep
						the present state of the chip and all its resources.

f_usTsiMemIndex			Index reserved in the TSI chariot memory.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseTsiMemEntry
UINT32 Oct6100ApiReleaseTsiMemEntry(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN		UINT16						f_usTsiMemIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	PVOID	pTsiMemAlloc;
	UINT32	ulResult;
	UINT32	ulIndex;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	/* Check if the entry programmed is greater then the timestamp entries. */
	if ( f_usTsiMemIndex > cOCT6100_TSST_CONTROL_TIMESTAMP_BASE_ENTRY )
		ulIndex = f_usTsiMemIndex - cOCT6100_TSI_MEM_FOR_TIMESTAMP;
	else
		ulIndex = f_usTsiMemIndex;

	/* Check if the entry programmed is greater then the phasing TSST entries. */
	if ( ulIndex > cOCT6100_TSST_CONTROL_PHASING_TSST_BASE_ENTRY )
		ulIndex -= pSharedInfo->ChipConfig.usMaxPhasingTssts;

	mOCT6100_GET_TSI_MEMORY_ALLOC_PNT( pSharedInfo, pTsiMemAlloc )
	
	ulResult = OctapiLlmAllocDealloc( pTsiMemAlloc, ulIndex );
	if ( ulResult != cOCT6100_ERR_OK )
	{
		return cOCT6100_ERR_FATAL_93;
	}

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReserveConversionMemEntry

Description:    Reserves one of the conversion memory entry 

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to 
						keep the present state of the chip and all its 
						resources.

f_pusConversionMemIndex	Resulting index reserved in the conversion memory.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReserveConversionMemEntry
UINT32 Oct6100ApiReserveConversionMemEntry(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				OUT		PUINT16						f_pusConversionMemIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	PVOID	pConversionMemAlloc;
	UINT32	ulConversionMemIndex;
	UINT32	ulResult;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	mOCT6100_GET_CONVERSION_MEMORY_ALLOC_PNT( pSharedInfo, pConversionMemAlloc )
	
	ulResult = OctapiLlmAllocAlloc( pConversionMemAlloc, &ulConversionMemIndex );
	if ( ulResult != cOCT6100_ERR_OK )
	{
		if ( ulResult == OCTAPI_LLM_NO_STRUCTURES_LEFT )
			return cOCT6100_ERR_MEMORY_ALL_CONVERSION_MEM_ENTRY_RESERVED;
		else
			return cOCT6100_ERR_FATAL_B8;
	}

	*f_pusConversionMemIndex = (UINT16)( ulConversionMemIndex & 0xFFFF );

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100ApiReleaseConversionMemEntry

Description:    Releases the conversion chariot memory entry specified.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pApiInstance			Pointer to API instance. This memory is used to 
						keep the present state of the chip and all its 
						resources.

f_usConversionMemIndex	Index reserved in the conversion chariot memory.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100ApiReleaseConversionMemEntry
UINT32 Oct6100ApiReleaseConversionMemEntry(
				IN OUT	tPOCT6100_INSTANCE_API		f_pApiInstance,
				IN		UINT16						f_usConversionMemIndex )
{
	tPOCT6100_SHARED_INFO		pSharedInfo;
	PVOID						pConversionMemAlloc;
	UINT32						ulResult;

	/* Get local pointer to shared portion of instance. */
	pSharedInfo = f_pApiInstance->pSharedInfo;

	mOCT6100_GET_CONVERSION_MEMORY_ALLOC_PNT( pSharedInfo, pConversionMemAlloc )
	
	ulResult = OctapiLlmAllocDealloc( pConversionMemAlloc, f_usConversionMemIndex );
	if ( ulResult != cOCT6100_ERR_OK )
	{
		return cOCT6100_ERR_FATAL_B7;
	}

	return cOCT6100_ERR_OK;
}
#endif
