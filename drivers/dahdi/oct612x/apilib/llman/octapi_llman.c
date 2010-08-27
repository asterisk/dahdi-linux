/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  octapi_llman.c

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	Library used to manage allocation tables and linked lists.  The library is
	made such that only a block of contiguous memory is needed for the
	management of the linked list/allocation table.

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

$Octasic_Revision: 22 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#include "octapi_llman_private.h"
#include "apilib/octapi_llman.h"
#include "apilib/octapi_largmath.h"


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctapiLlmAllocGetSize.
|
|	Description:	This function determines the amount of memory needed to
|					manage the allocation of a fixed amount of resources.
|					The memory is measured in bytes.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	number_of_items		UINT32			The number of resources to be allocated.
|	*l_size	UINT32		UINT32			The amount of memory needed, returned.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctapiLlmAllocGetSize
UINT32 OctapiLlmAllocGetSize(UINT32 number_of_items,UINT32 * l_size)
{
	if (number_of_items == 0) return(GENERIC_BAD_PARAM);

	*l_size = (sizeof(LLM_ALLOC)) + (number_of_items * sizeof(UINT32));

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctapiLlmAllocInit.
|
|	Description:	This function intializes the LLM_ALLOC structure.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	**l					void			The memory used by the LLM_ALLOC structure.
|	number_of_items		UINT32			The number of resources to be allocated.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctapiLlmAllocInit
UINT32 OctapiLlmAllocInit(void ** l,UINT32 number_of_items)
{
	LLM_ALLOC* ls;
	UINT32 i;

	/* Check the number of items required.*/
	if (number_of_items == 0) return(GENERIC_BAD_PARAM);

	/* If no memory has been allocated yet:*/
	if (*l == NULL) return(OCTAPI_LLM_MEMORY_NOT_ALLOCATED);

	/* Build the structure before starting.*/
	ls = (LLM_ALLOC *)(*l);
	ls->linked_list = (UINT32 *)((BYTE *)ls + sizeof(LLM_ALLOC));

	ls->number_of_items = number_of_items;

	/* Linked list links all structures in ascending order.*/
	for(i=0;i<number_of_items;i++)
	{
		ls->linked_list[i] = i+1;
	}

	ls->linked_list[number_of_items - 1] = 0xFFFFFFFF; /* Invalid link.*/

	/* Next avail is 0.*/
	ls->next_avail_num = 0;

	/* Number of allocated items is null.*/
	ls->allocated_items = 0;

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctapiLlmAllocInfo.
|
|	Description:	This function returns the number of free and allocated
|					block in the LLMAN list.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*l					void			The memory used by the LLM_ALLOC structure.
|	*allocated_items	UINT32			Number of allocated items.
|	*available_items	UINT32			Number of available items.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctapiLlmAllocInfo
UINT32 OctapiLlmAllocInfo(void * l,UINT32 * allocated_items,UINT32 * available_items)
{
	LLM_ALLOC* ls;

	/* Build the structure before starting.*/
	ls = (LLM_ALLOC *)l;
	ls->linked_list = (UINT32 *)((BYTE *)ls + sizeof(LLM_ALLOC));

	*allocated_items = ls->allocated_items;
	*available_items = ls->number_of_items - ls->allocated_items;
	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctapiLlmAllocInfo.
|
|	Description:	This function allocates the resource indicated by blocknum.
|					If the resource can be allocated then GENERIC_OK is returned.  
|					Else an error.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*l					void			The memory used by the LLM_ALLOC structure.
|	*block_num			UINT32			The resource to be allocated.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctapiLlmAllocAlloc
UINT32 OctapiLlmAllocAlloc(void * l,UINT32 * blocknum)
{
	LLM_ALLOC* ls;
	UINT32 allocated_block;
	UINT32* node;

	/* Build the structure before starting.*/
	ls = (LLM_ALLOC *)l;
	ls->linked_list = (UINT32 *)((BYTE *)ls + sizeof(LLM_ALLOC));

	/* Get next available block number.*/
	allocated_block = ls->next_avail_num;

	/* Check if block is invalid.*/
	if (allocated_block == 0xFFFFFFFF)
	{
		/* Make blocknum NULL.*/
		*blocknum = 0xFFFFFFFF;

		return(OCTAPI_LLM_NO_STRUCTURES_LEFT);
	}

	node = &ls->linked_list[allocated_block];

	/* Copy next block number.*/
	ls->next_avail_num = *node;

	/* Tag as used the current block number.*/
	*node = 0xFFFFFFFE;

	/* Return proper block number.*/
	*blocknum = allocated_block;

	/* Update block usage number.*/
	ls->allocated_items++;

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctapiLlmAllocDealloc.
|
|	Description:	This function deallocates the resource indicated by blocknum.
|					If the resource is not already allocated an error is returned.
|					Else GENERIC_OK is returned.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*l					void			The memory used by the LLM_ALLOC structure.
|	block_num			UINT32			The resource to be deallocated.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctapiLlmAllocDealloc
UINT32 OctapiLlmAllocDealloc(void * l,UINT32 blocknum)
{
	LLM_ALLOC* ls;
	UINT32* node;

	/* Build the structure before starting.*/
	ls = (LLM_ALLOC *)l;
	ls->linked_list = (UINT32 *)((BYTE *)ls + sizeof(LLM_ALLOC));
	
	/* Check for null item pointer.*/
	if (blocknum == 0xFFFFFFFF) return(GENERIC_OK);

	/* Check if blocknum is within specified item range.*/
	if (blocknum >= ls->number_of_items) return(OCTAPI_LLM_BLOCKNUM_OUT_OF_RANGE);

	node = &ls->linked_list[blocknum];

	/* Check if block is really used as of now.*/
	if (*node != 0xFFFFFFFE) return(OCTAPI_LLM_MEMORY_NOT_ALLOCATED);

	/* Add link to list.*/
	*node = ls->next_avail_num;

	/* Point to returned block.*/
	ls->next_avail_num = blocknum;

	/* Update block usage number.*/
	ls->allocated_items--;

	return(GENERIC_OK);
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiTllmAllocGetSize.
|
|	Description:	This function determines the amount of memory needed to
|					manage the allocation of a fixed amount of resources.
|					The memory is measured in bytes. 
|
|					This version is a time manage version of llman.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	number_of_items		UINT32			The number of resources to be allocated.
|	*l_size	UINT32		UINT32			The amount of memory needed, returned.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiTllmAllocGetSize
UINT32 OctApiTllmAllocGetSize(UINT32 number_of_items,UINT32 * l_size)
{
	if (number_of_items == 0) return(GENERIC_BAD_PARAM);

	*l_size = (sizeof(TLLM_ALLOC)) + (number_of_items * sizeof(TLLM_ALLOC_NODE));

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiTllmAllocInit.
|
|	Description:	This function intializes the TLLM_ALLOC structure.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	**l					void			The memory used by the LLM_ALLOC structure.
|	number_of_items		UINT32			The number of resources to be allocated.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiTllmAllocInit
UINT32 OctApiTllmAllocInit(void ** l,UINT32 number_of_items)
{
	TLLM_ALLOC* ls;
	UINT32 i;

	/* Check the number of items required.*/
	if (number_of_items == 0) return(GENERIC_BAD_PARAM);

	/* If no memory has been allocated yet.*/
	if (*l == NULL) return(OCTAPI_LLM_MEMORY_NOT_ALLOCATED);

	/* Build the structure before starting.*/
	ls = (TLLM_ALLOC *)(*l);
	ls->linked_list = (TLLM_ALLOC_NODE *)((BYTE *)ls + sizeof(TLLM_ALLOC));

	ls->number_of_items = number_of_items;

	/* Linked list links all structures in ascending order.*/
	for(i=0;i<number_of_items;i++)
	{
		ls->linked_list[i].value = i+1;
	}

	ls->linked_list[number_of_items - 1].value = 0xFFFFFFFF; /* Invalid link.*/

	/* Next avail is 0.*/
	ls->next_avail_num = 0;

	/* Number of allocated items is null.*/
	ls->allocated_items = 0;

	/* Set the number of timeout entry.*/
	ls->number_of_timeout = 0;

	/* Next timeout is 0.*/
	ls->next_timeout_num = 0xFFFFFFFF;
	ls->last_timeout_num = 0xFFFFFFFF;

	/* Set the known time to 0.*/
	ls->last_known_time[ 0 ] = 0;
	ls->last_known_time[ 1 ] = 0;

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiTllmAllocInfo.
|
|	Description:	This function returns the number of free and allocated
|					block in the TLLMAN list.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*l					void			The memory used by the LLM_ALLOC structure.
|	*allocated_items	UINT32			Number of allocated items.
|	*available_items	UINT32			Number of available items.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiTllmAllocInfo
UINT32 OctApiTllmAllocInfo(void * l,UINT32 * allocated_items,UINT32 * available_items)
{
	TLLM_ALLOC* ls;

	/* Build the structure before starting.*/
	ls = (TLLM_ALLOC *)l;
	*allocated_items = ls->allocated_items;
	*available_items = ls->number_of_items - ls->allocated_items;

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiTllmAllocAlloc.
|
|	Description:	This function allocates the resource indicated by blocknum.
|					If the resource can be allocated then GENERIC_OK is returned.  
|					Else an error.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*l					void			The memory used by the LLM_ALLOC structure.
|	*block_num			UINT32			The resource to be allocated.
|	*current_time		UINT32
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiTllmAllocAlloc
UINT32 OctApiTllmAllocAlloc(void * l,UINT32 * blocknum, UINT32 *current_time)
{
	TLLM_ALLOC* ls;
	UINT32 allocated_block;
	TLLM_ALLOC_NODE* node;

	/* Build the structure before starting.*/
	ls = (TLLM_ALLOC *)l;
	ls->linked_list = (TLLM_ALLOC_NODE *)((BYTE *)ls + sizeof(TLLM_ALLOC));
	
	if ( ls->allocated_items == ls->number_of_items  && 
		 ls->next_timeout_num != 0xFFFFFFFF )
	{
		UINT32 l_ulResult;
		l_ulResult = OctApiTllmCheckTimeoutList( ls, current_time );
		if ( l_ulResult != GENERIC_OK )
			return l_ulResult;
	}
	
	/* Get next available block number.*/
	allocated_block = ls->next_avail_num;

	/* Check if block is invalid.*/
	if (allocated_block == 0xFFFFFFFF)
	{
		/* Make blocknum NULL.*/
		*blocknum = 0xFFFFFFFF;

		return(OCTAPI_LLM_NO_STRUCTURES_LEFT);
	}

	node = &ls->linked_list[allocated_block];

	/* Copy next block number.*/
	ls->next_avail_num = node->value;

	/* Tag as used the current block number.*/
	node->value = 0xFFFFFFFE;

	/* Return proper block number.*/
	*blocknum = allocated_block;

	/* Update block usage number.*/
	ls->allocated_items++;

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiTllmAllocDealloc.
|
|	Description:	This function deallocates the resource indicated by blocknum.
|					If the resource is not already allocated an error is returned.
|					Else GENERIC_OK is returned.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*l					void			The memory used by the LLM_ALLOC structure.
|	block_num			UINT32			The resource to be deallocated.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiTllmAllocDealloc
UINT32 OctApiTllmAllocDealloc(void * l,UINT32 blocknum, UINT32 timeout_value, UINT32 current_time[2])
{
	TLLM_ALLOC* ls;
	TLLM_ALLOC_NODE* node;
	UINT32	l_ulResult;

	/* Build the structure before starting.*/
	ls = (TLLM_ALLOC *)l;
	ls->linked_list = (TLLM_ALLOC_NODE *)((BYTE *)ls + sizeof(TLLM_ALLOC));
	
	/* Check for null item pointer.*/
	if (blocknum == 0xFFFFFFFF) return(GENERIC_OK);

	/* Check if blocknum is within specified item range.*/
	if (blocknum >= ls->number_of_items) return(OCTAPI_LLM_BLOCKNUM_OUT_OF_RANGE);

	if ( ls->next_timeout_num != 0xFFFFFFFF )
	{
		l_ulResult = OctApiTllmCheckTimeoutList( ls, current_time );
		if ( l_ulResult != GENERIC_OK )
			return l_ulResult;
	}

	node = &ls->linked_list[blocknum];

	/* Check if block is really used as of now.*/
	if (node->value != 0xFFFFFFFE) return(OCTAPI_LLM_MEMORY_NOT_ALLOCATED);

	/* Add link to timeout list.*/
	if ( ls->last_timeout_num != 0xFFFFFFFF )
	{
		TLLM_ALLOC_NODE* last_node;

		/* insert the node at the end of the list.*/
		node->value = 0xFFFFFFFF;
		last_node = &ls->linked_list[ ls->last_timeout_num ];
		last_node->value = blocknum;
	}
	else
	{
		/* The node is alone in the list.*/
		node->value = 0xFFFFFFFF;
		ls->next_timeout_num = blocknum;
	}

	ls->last_timeout_num = blocknum;
	ls->number_of_timeout++;

	/* Set the timeout time of the node.*/
	l_ulResult = OctApiLmAdd( current_time, 1, &timeout_value, 0, node->timeout, 1 );
	if (l_ulResult != GENERIC_OK) 
		return(l_ulResult);

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiTllmCheckTimeoutList.
|
|	Description:	This function will verify if the timeout time 
|					of all the node present in the timeout list are bigger
|					then the current time.  If so the node will be returned
|					ot the free node list.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*ls						TLLM_ALLOC		The memory used by the TLLM_ALLOC structure.
|	current_time			UINT32[2]		The current time in msec.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiTllmCheckTimeoutList
UINT32 OctApiTllmCheckTimeoutList(TLLM_ALLOC *ls, UINT32 current_time[2])
{
	UINT32	result;
	UINT32	fConditionFlag = TRUE;

	/* Free-up any pending memory before trying the allocation:*/
	if ((ls->last_known_time[0] != current_time[0] ||
		ls->last_known_time[1] != current_time[1]) && 
		(current_time[1] != 0 || current_time[0] != 0))	/* Time has changed.*/
	{
		TLLM_ALLOC_NODE *pcurrent_node;
		UINT32	current_num;
		USHORT	neg;

		/* Remember time for next time!*/
		ls->last_known_time[0] = current_time[0];
		ls->last_known_time[1] = current_time[1];

	
		while ( fConditionFlag == TRUE )
		{
			/* Get a node from the timeout list.*/
			pcurrent_node = &ls->linked_list[ ls->next_timeout_num ];
			current_num = ls->next_timeout_num;
	
			/* Check if first node has timeout.*/
			result = OctApiLmCompare(current_time,1,pcurrent_node->timeout ,1,&neg);
			if (result != GENERIC_OK) return(result);
			
			/* if the timeout tiem was exceeded, set the block as free.*/
			if ( neg == FALSE )
			{
				/* set the next node pointer.*/
				ls->next_timeout_num = pcurrent_node->value;
				ls->number_of_timeout--;

				/* reset the last pointer of the timeout list.*/
				if ( ls->number_of_timeout == 0 )
					ls->last_timeout_num = 0xFFFFFFFF;

				/* return the node the free list.*/
				pcurrent_node->value = ls->next_avail_num;
				ls->next_avail_num = current_num;
				ls->allocated_items--;
			}
			else	/* node not in timeout */
			{
				fConditionFlag = FALSE;
				break;
			}

			if ( ls->next_timeout_num == 0xFFFFFFFF )
			{
				fConditionFlag = FALSE;
				break;	/* end of timeout list.*/
			}
		}
	}

	return(GENERIC_OK);
}
#endif
/**************************************** llm_alloc section **********************************************/







/**************************************** llm_list section **********************************************/
/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLlmListGetSize
|
|	Description:	This function determines the amount of memory needed by
|					the LLM_LIST structure to manage the allocation of
|					number_of_items number of resources.  The memory is
|					measured in bytes.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	number_of_items		UINT32			The number of resources to be allocated
|										amongst all linked-lists.
|	number_of_lists		UINT32			The maximum number of linked-lists that
|										can be allocated.
|	*l_size	UINT32		UINT32			The amount of memory needed, returned.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLlmListGetSize
UINT32 OctApiLlmListGetSize(UINT32 number_of_items,UINT32 number_of_lists,UINT32 user_info_size,UINT32 * l_size)
{
	UINT32 head_alloc_size;
	UINT32 result;
	UINT32 user_info_size_roundup;

	if (number_of_items == 0) return(GENERIC_BAD_PARAM);
	if (number_of_lists == 0) return(GENERIC_BAD_PARAM);
	if (user_info_size == 0) return(GENERIC_BAD_PARAM);

	user_info_size_roundup = ((user_info_size + 3) / 4) * 4;

	result = OctapiLlmAllocGetSize(number_of_lists,&head_alloc_size);
	if(result != GENERIC_OK) return(result);

	*l_size = sizeof(LLM_LIST) + (number_of_lists * sizeof(LLM_LIST_HEAD)) + head_alloc_size + (number_of_items * (sizeof(LLM_LIST_ITEM) + user_info_size_roundup - 4));

	return(GENERIC_OK);
}
#endif

#if !SKIP_OctApiLlmListGetItemPointer
LLM_LIST_ITEM * OctApiLlmListGetItemPointer(LLM_LIST * ls, UINT32 item_number)
{
	return (LLM_LIST_ITEM *) (((BYTE *)ls->li) + (ls->item_size * item_number)) ;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLlmListInit.
|
|	Description:	This function intializes the LLM_TALLOC structure.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*l					void			The memory used by the LLM_LIST structure.
|	number_of_items		UINT32			The number of resources to be allocated
|										amongst all linked-lists.
|	number_of_lists		UINT32			The maximum number of linked-lists that
|										can be allocated.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLlmListInit
UINT32 OctApiLlmListInit(void ** l,UINT32 number_of_items,UINT32 number_of_lists,UINT32 user_info_size)
{
	LLM_LIST* ls;
	LLM_LIST_ITEM* item;
	UINT32 i;
	UINT32 head_alloc_size;
	UINT32 result;
	UINT32 user_info_size_roundup;
	UINT32 total_lists;
	BYTE* lsbyte;


	if (number_of_items == 0) return(GENERIC_BAD_PARAM);
	if (number_of_lists == 0) return(GENERIC_BAD_PARAM);
	if (user_info_size == 0) return(GENERIC_BAD_PARAM);

	user_info_size_roundup = ((user_info_size + 3) / 4) * 4;

	/* Get the size of the Alloc structure used to manage head of list structures.*/
	result = OctapiLlmAllocGetSize(number_of_lists,&head_alloc_size);
	if(result != GENERIC_OK) return(result);

	if (*l == NULL) return(OCTAPI_LLM_MEMORY_NOT_ALLOCATED);

	/* Built the structure based on the base address:*/
	ls = (LLM_LIST *)(*l);
	lsbyte = (BYTE *)ls;
	total_lists = ls->total_lists;

	ls->lh = (LLM_LIST_HEAD *)(lsbyte + sizeof(LLM_LIST));
	ls->list_head_alloc = (void *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)));
	ls->li = (LLM_LIST_ITEM *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)) + ls->head_alloc_size);

	/* Initialize parameters in the structure.*/
	ls->head_alloc_size = head_alloc_size;
	ls->user_info_bytes = user_info_size;
	ls->user_info_size = user_info_size_roundup;
	ls->total_items = number_of_items;
	ls->assigned_items = 0;
	ls->total_lists = number_of_lists;
	ls->assigned_lists = 0;
	ls->next_empty_item = 0;
	ls->item_size = sizeof(LLM_LIST_ITEM) + user_info_size_roundup - 4;

	/* Complete the build!*/
	ls = (LLM_LIST *)(*l);
	lsbyte = (BYTE *)ls;
	total_lists = ls->total_lists;

	ls->lh = (LLM_LIST_HEAD *)(lsbyte + sizeof(LLM_LIST));
	ls->list_head_alloc = (void *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)));
	ls->li = (LLM_LIST_ITEM *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)) + ls->head_alloc_size);

	/* Initialize the head of queue Alloc structure.*/
	result = OctapiLlmAllocInit(&(ls->list_head_alloc),number_of_lists);
	if(result != GENERIC_OK) return(result);

	/* Initialize the linked list of the items:*/
	for(i=0; i<number_of_items; i++)
	{
		item = (LLM_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * i);

		if (i == (number_of_items - 1))
			item->forward_link = 0xFFFFFFFF;
		else
			item->forward_link = i + 1;
	}

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLlmListInfo.
|
|	Description:	This function returns the status of the LLM_LIST structure.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*l					void			The memory used by the LLM_LIST structure.
|	*allocated_lists	UINT32			The number of linked_lists allocated.
|	*free_lists			UINT32			The number of linked_lists still free.
|	*allocated_items	UINT32			The number of items allocated to lists.
|	*free_items			UINT32			The number of items still free.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLlmListInfo
UINT32 OctApiLlmListInfo(void * l,UINT32 * allocated_lists,UINT32 * allocated_items,
									UINT32 * free_lists,UINT32 * free_items)
{
	LLM_LIST* ls;
	BYTE* lsbyte;
	UINT32 total_lists;

	/* Built the structure based on the base address:*/
	ls = (LLM_LIST *)l;
	lsbyte = (BYTE *)ls;
	total_lists = ls->total_lists;

	ls->lh = (LLM_LIST_HEAD *)(lsbyte + sizeof(LLM_LIST));
	ls->list_head_alloc = (void *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)));
	ls->li = (LLM_LIST_ITEM *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)) + ls->head_alloc_size);

	*allocated_items = ls->assigned_items;
	*free_items = ls->total_items - ls->assigned_items;

	*allocated_lists = ls->assigned_lists;
	*free_lists = ls->total_lists - ls->assigned_lists;

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLlmListCreate.
|
|	Description:	This function creates a linked list.  The target which is
|					allocated the newly created list can request additions
|					or removals from the list later on.  To target identifies
|					its list with the returned list handle.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*l					void			The memory used by the LLM_LIST structure.
|	*list_handle		UINT32			The handle to the new list, returned.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLlmListCreate
UINT32 OctApiLlmListCreate(void * l,UINT32 * list_handle)
{
	LLM_LIST* ls;
	LLM_LIST_HEAD* lh;
	UINT32 blocknum;
	UINT32 total_lists;
	UINT32 result;
	BYTE* lsbyte;

	/* Built the structure based on the base address:*/
	ls = (LLM_LIST *)l;
	lsbyte = (BYTE *)ls;
	total_lists = ls->total_lists;

	ls->lh = (LLM_LIST_HEAD *)(lsbyte + sizeof(LLM_LIST));
	ls->list_head_alloc = (void *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)));
	ls->li = (LLM_LIST_ITEM *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)) + ls->head_alloc_size);

	/* Get a list using the list head alloc structure.*/
	result = OctapiLlmAllocAlloc(ls->list_head_alloc, &blocknum);
	if (result != GENERIC_OK) return(result);

	/* The handle is the block number.*/
	*list_handle = blocknum;

	/* Initialize the list head structure.*/
	lh = &ls->lh[blocknum];
	lh->list_length = 0;
	lh->head_pointer = 0xFFFFFFFF;
	lh->tail_pointer = 0xFFFFFFFF;
	lh->cache_item_number = 0xFFFFFFFF;
	lh->cache_item_pointer = 0xFFFFFFFF;

	ls->assigned_lists++;

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLlmListDelete.
|
|	Description:	This function deletes the linked list indicated by the
|					handle list_handle.  Any items which are still allocated
|					to the list are first deallocated.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*l					void			The memory used by the LLM_LIST structure.
|	*list_handle		UINT32			The handle to the list.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLlmListDelete
UINT32 OctApiLlmListDelete(void * l,UINT32 list_handle)
{
	LLM_LIST* ls;
	LLM_LIST_HEAD* lh;
	UINT32 total_lists;
	UINT32 result;
	BYTE* lsbyte;

	/* Built the structure based on the base address:*/
	ls = (LLM_LIST *)l;
	lsbyte = (BYTE *)ls;
	total_lists = ls->total_lists;

	ls->lh = (LLM_LIST_HEAD *)(lsbyte + sizeof(LLM_LIST));
	ls->list_head_alloc = (void *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)));
	ls->li = (LLM_LIST_ITEM *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)) + ls->head_alloc_size);

	
	if (list_handle >= ls->total_lists) return(OCTAPI_LLM_BLOCKNUM_OUT_OF_RANGE);
	if (ls->lh[list_handle].list_length == 0xFFFFFFFF) return(OCTAPI_LLM_INVALID_LIST_HANDLE);

	/* Release internal list header handle...*/
	result = OctapiLlmAllocDealloc(ls->list_head_alloc,list_handle);
	if (result != GENERIC_OK) return(result);

	lh = &ls->lh[list_handle];

	/* Deallocate all items in the list!*/
	if (lh->list_length != 0)
	{
		LLM_LIST_ITEM * item;

		item = (LLM_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * lh->tail_pointer);

		/* Release the items using only the links.*/
		item->forward_link = ls->next_empty_item;
		ls->next_empty_item = lh->head_pointer;

		/* Remove items from item counter.*/
		ls->assigned_items -= lh->list_length;
	}

	lh->list_length = 0xFFFFFFFF;
	lh->head_pointer = 0xFFFFFFFF;
	lh->tail_pointer = 0xFFFFFFFF;
	lh->cache_item_number = 0xFFFFFFFF;
	lh->cache_item_pointer = 0xFFFFFFFF;

	ls->assigned_lists--;

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLlmListLength.
|
|	Description:	This function returns the number of items allocated to the
|					list indicated by the handle list_handle.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*l					void			The memory used by the LLM_LIST structure.
|	list_handle			UINT32			The handle to the list.
|	*number_of_items	UINT32			The number of items in the list, returned.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLlmListLength
UINT32 OctApiLlmListLength(void * l,UINT32 list_handle, UINT32 * number_of_items_in_list)
{
	LLM_LIST* ls;
	LLM_LIST_HEAD* lh;
	UINT32 total_lists;
	BYTE* lsbyte;

	/* Built the structure based on the base address:*/
	ls = (LLM_LIST *)l;
	lsbyte = (BYTE *)ls;
	total_lists = ls->total_lists;

	ls->lh = (LLM_LIST_HEAD *)(lsbyte + sizeof(LLM_LIST));
	ls->list_head_alloc = (void *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)));
	ls->li = (LLM_LIST_ITEM *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)) + ls->head_alloc_size);

	lh = &ls->lh[list_handle];
	
	if (list_handle >= ls->total_lists) return(OCTAPI_LLM_BLOCKNUM_OUT_OF_RANGE);
	if (lh->list_length == 0xFFFFFFFF) return(OCTAPI_LLM_INVALID_LIST_HANDLE);

	*number_of_items_in_list = lh->list_length;

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLlmListItemData
|
|	Description:	This function returns a pointer to the user data associated
|					with an item.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*l					void			The memory used by the LLM_LIST structure.
|	list_handle			UINT32			The handle to the list.
|	item_number			UINT32			The number of the list node in question.
|	**item_data_pnt		void			The pointer to the user data, returned.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLlmListItemData
UINT32 OctApiLlmListItemData(void * l,UINT32 list_handle,UINT32 item_number,void ** item_data_pnt)
{
	LLM_LIST* ls;
	LLM_LIST_HEAD* lh;
	LLM_LIST_ITEM* item;
	UINT32	cur_list_pnt;
	UINT32	cur_list_num;
	UINT32	total_lists;
	UINT32	list_length;
	BYTE*	lsbyte;
	UINT32	fConditionFlag = TRUE;

	/* Built the structure based on the base address:*/
	ls = (LLM_LIST *)l;
	lsbyte = (BYTE *)ls;
	total_lists = ls->total_lists;

	ls->lh = (LLM_LIST_HEAD *)(lsbyte + sizeof(LLM_LIST));
	ls->list_head_alloc = (void *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)));
	ls->li = (LLM_LIST_ITEM *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)) + ls->head_alloc_size);

	lh = &ls->lh[list_handle];
	list_length = lh->list_length;
	
	*item_data_pnt = NULL;
	if (list_handle >= ls->total_lists) return(OCTAPI_LLM_BLOCKNUM_OUT_OF_RANGE);
	if (list_length == 0xFFFFFFFF) return(OCTAPI_LLM_INVALID_LIST_HANDLE);
	if (list_length <= item_number)	return(OCTAPI_LLM_ELEMENT_NOT_FOUND);

	/* Determine where the search will start.*/
	if (list_length == (item_number + 1))	/* Last item in list:*/
	{
		cur_list_pnt = lh->tail_pointer;
		cur_list_num = item_number;
	}
	else if (lh->cache_item_number <= item_number)	/* Start at cache:*/
	{
		cur_list_pnt = lh->cache_item_pointer;
		cur_list_num = lh->cache_item_number;
	}
	else  /* Start at beginning:*/
	{
		cur_list_pnt = lh->head_pointer;
		cur_list_num = 0;
	}

	/* Start search from cur_list_pnt and cur_list_num.*/
	while ( fConditionFlag == TRUE )
	{
		item = (LLM_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * cur_list_pnt);

		if (cur_list_num == item_number) /* Item number found.*/
		{
			/* Write new cache entry.*/
			lh->cache_item_pointer = cur_list_pnt;
			lh->cache_item_number = cur_list_num;

			/* Get item info.*/
			*item_data_pnt = (void *)item->user_info;

			return(GENERIC_OK);
		}
		else if(item->forward_link == 0xFFFFFFFF) /* End of list found?!?*/
		{
			return(OCTAPI_LLM_INTERNAL_ERROR0);
		}
		else /* Item was not found, but continue searching.*/
		{
			cur_list_pnt = item->forward_link;
		}

		cur_list_num++;
	}

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLlmListInsertItem.
|
|	Description:	This function allocates a node to the linked list specified
|					by the handle list_handle.  The position of the new item
|					can be specified. A position of 0xFFFFFFFF means append to the 
|					list( use the OCTAPI_LLM_LIST_APPEND define for clarity); 
|                   a position of 0 mean insert at the begining of the list.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*l					void			The memory used by the LLM_LIST structure.
|	*list_handle		UINT32			The handle to the list.
|	**item_data			void			Address of the user data space for this item.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLlmListInsertItem
UINT32 OctApiLlmListInsertItem(void * l,UINT32 list_handle,UINT32 item_number,void ** item_data_pnt)
{
	LLM_LIST* ls;
	LLM_LIST_HEAD* lh;
	LLM_LIST_ITEM* free_item;
	UINT32 free_item_pnt;
	UINT32 total_lists;
	BYTE* lsbyte;
	UINT32	fConditionFlag = TRUE;

	/* Built the structure based on the base address:*/
	ls = (LLM_LIST *)l;
	lsbyte = (BYTE *)ls;
	total_lists = ls->total_lists;

	ls->lh = (LLM_LIST_HEAD *)(lsbyte + sizeof(LLM_LIST));
	ls->list_head_alloc = (void *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)));
	ls->li = (LLM_LIST_ITEM *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)) + ls->head_alloc_size);

	lh = &ls->lh[list_handle];
	
	*item_data_pnt = NULL;
	if (list_handle >= ls->total_lists) return(OCTAPI_LLM_BLOCKNUM_OUT_OF_RANGE);
	if (lh->list_length == 0xFFFFFFFF) return(OCTAPI_LLM_INVALID_LIST_HANDLE);
	if (lh->list_length < item_number && item_number != 0xFFFFFFFF)	
		return(OCTAPI_LLM_BLOCKNUM_OUT_OF_RANGE);
	if (ls->next_empty_item == 0xFFFFFFFF) return(OCTAPI_LLM_NO_STRUCTURES_LEFT);

	/* Get a free item from the free item list!*/
	free_item_pnt = ls->next_empty_item;
	free_item = (LLM_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * free_item_pnt);
	ls->next_empty_item = free_item->forward_link;

	if (item_number == 0xFFFFFFFF)
		item_number = lh->list_length;

	if (lh->list_length == 0)	/* First item and only item:*/
	{
		free_item->forward_link = 0xFFFFFFFF;
		lh->tail_pointer = free_item_pnt;
		lh->head_pointer = free_item_pnt;
	}
	else if (item_number == 0)	/* First item and but list not empty:*/
	{
		free_item->forward_link = lh->head_pointer;
		lh->head_pointer = free_item_pnt;
	}
	else if (item_number == lh->list_length)	/* Append:*/
	{
		LLM_LIST_ITEM * last_item;
		last_item = (LLM_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * lh->tail_pointer);

		last_item->forward_link = free_item_pnt;
		free_item->forward_link = 0xFFFFFFFF;
		lh->tail_pointer = free_item_pnt;
	}
	else	/* Insert:*/
	{
		LLM_LIST_ITEM * last_item = NULL;
		LLM_LIST_ITEM * item;
		UINT32 last_list_pnt;
		UINT32 cur_list_pnt;
		UINT32 cur_list_num;

		if (lh->cache_item_number < item_number)	/* Start at cache:*/
		{
			cur_list_pnt = lh->cache_item_pointer;
			cur_list_num = lh->cache_item_number;
		}
		else /* Start at beginning:*/
		{
			cur_list_pnt = lh->head_pointer;
			cur_list_num = 0;
		}

		last_list_pnt = 0xFFFFFFFF;

		/* Start search from cur_list_pnt and cur_list_num.*/
		while ( fConditionFlag == TRUE )
		{
			item = (LLM_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * cur_list_pnt);

			if (cur_list_num == item_number) /* Item number found.*/
			{
				if (last_list_pnt == 0xFFFFFFFF) return(OCTAPI_LLM_INTERNAL_ERROR1);

				free_item->forward_link = cur_list_pnt;
				last_item->forward_link = free_item_pnt;
				
				fConditionFlag = FALSE;
				break;
			}
			else if (item->forward_link == 0xFFFFFFFF) /* End of list found?!?*/
			{
				return(OCTAPI_LLM_INTERNAL_ERROR0);
			}
			else /* Item was not found, but continue searching.*/
			{
				last_item = item;
				last_list_pnt = cur_list_pnt;
				cur_list_pnt = item->forward_link;
			}

			cur_list_num++;
		}
	}

	/* Increase the list length.*/
	lh->list_length++;
	ls->assigned_items++;
	*item_data_pnt = (void *)free_item->user_info;

	/* Write new cache entry.*/
	lh->cache_item_pointer = free_item_pnt;
	lh->cache_item_number = item_number;

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLlmListCreateFull.
|
|	Description:	This function allocates the desired number of nodes to
|					the linked list specified by the handle list_handle.
|					The position of the new item can be specified. A
|					position of 0xFFFFFFFF means append to the list (use the
|					OCTAPI_LLM_LIST_APPEND define for clarity); a position
|					of 0 means insert at the begining of the list.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*l					void			The memory used by the LLM_LIST structure.
|	*list_handle		UINT32			The handle to the list.
|	**item_data			void			Address of the user data space for this item.
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLlmListCreateFull
UINT32 OctApiLlmListCreateFull(void* l, UINT32 list_length, UINT32* plist_handle)
{
	LLM_LIST* ls;
	LLM_LIST_HEAD* lh;
	LLM_LIST_ITEM* free_item;
	LLM_LIST_ITEM* last_item = NULL;
	UINT32 free_item_pnt = 0xFFFFFFFF;
	UINT32 total_lists;
	UINT32 list_handle;
	UINT32 list_length_m1;
	UINT32 next_empty_item;
	UINT32 result;
	UINT32 i;
	BYTE* lsbyte;


	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
	/* Build the structure based on the base address:*/
	ls = (LLM_LIST *)l;
	lsbyte = (BYTE *)ls;
	total_lists = ls->total_lists;

	ls->lh = (LLM_LIST_HEAD *)(lsbyte + sizeof(LLM_LIST));
	ls->list_head_alloc = (void *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)));
	ls->li = (LLM_LIST_ITEM *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)) + ls->head_alloc_size);
	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/



	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
	/* Make sure another list can be created.*/
	if (ls->assigned_lists == ls->total_lists)
		return(OCTAPI_LLM_ELEMENT_ALREADY_ASSIGNED);

	/* Make sure there are enough free nodes to fill the new list.*/
	if (list_length > (ls->total_items - ls->assigned_items))
		return(OCTAPI_LLM_ELEMENT_ALREADY_ASSIGNED);
	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/


	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
	/* Create list (i.e. get a list using the list head alloc structure.*/
	result = OctapiLlmAllocAlloc(ls->list_head_alloc, &list_handle);
	if (result != GENERIC_OK) return(result);

	/* Initialize the list head structure.*/
	lh = &ls->lh[list_handle];
	lh->list_length = 0;
	lh->head_pointer = 0xFFFFFFFF;
	lh->tail_pointer = 0xFFFFFFFF;
	lh->cache_item_number = 0xFFFFFFFF;
	lh->cache_item_pointer = 0xFFFFFFFF;

	ls->assigned_lists++;
	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/



	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
	/* Add the number of requested nodes to the list.*/
	lh = &ls->lh[list_handle];
	list_length_m1 = list_length - 1;
	next_empty_item = ls->next_empty_item;

	for (i=0; i<list_length; i++)
	{
		/* Get a free item from the free item list!*/
		free_item_pnt = next_empty_item;
		free_item = (LLM_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * free_item_pnt);
		next_empty_item = free_item->forward_link;

		/* Branch according to whether the node is the first in list, last, or in
			the middle.*/
		if (i == 0)	
		{
			/* First item.*/
			free_item->forward_link = 0xFFFFFFFF;
			lh->head_pointer = free_item_pnt;
			lh->tail_pointer = free_item_pnt;
		}
		else if (i == list_length_m1)	
		{
			/* Last item.*/
			last_item->forward_link = free_item_pnt;
			free_item->forward_link = 0xFFFFFFFF;
			lh->tail_pointer = free_item_pnt;
		}
		else
		{
			/* Node somewhere in the middle.*/
			last_item->forward_link = free_item_pnt;
		}

		/* Store pointer to free item as pointer to last item (for next iteration).*/
		last_item = free_item;
	}

	/* Store new value of next_empty_item.*/
	ls->next_empty_item = next_empty_item;

	/* Write new cache entry.*/
	lh->cache_item_pointer = free_item_pnt;
	lh->cache_item_number = list_length_m1;

	/* Set the list length.*/
	lh->list_length = list_length;
	ls->assigned_items += list_length;

	/* Return pointer to new list.*/
	*plist_handle = list_handle;

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLlmListAppendItems.
|
|	Description:	This function allocates the desired number of nodes to
|					the linked list specified by the handle list_handle.
|					The position of the new item can be specified. A
|					position of 0xFFFFFFFF means append to the list (use the
|					OCTAPI_LLM_LIST_APPEND define for clarity); a position
|					of 0 means insert at the begining of the list.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*l					void			The memory used by the LLM_LIST structure.
|	*list_handle		UINT32			The handle to the list.
|	**item_data			void			Address of the user data space for this item.
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLlmListAppendItems
UINT32 OctApiLlmListAppendItems(void* l, UINT32 list_handle, UINT32 num_items)
{
	LLM_LIST* ls;
	LLM_LIST_HEAD* lh;
	LLM_LIST_ITEM* item_list;
	LLM_LIST_ITEM* curr_item = NULL;
	LLM_LIST_ITEM* free_item;
	UINT32 curr_item_pnt = 0xFFFFFFFF;
	UINT32 total_lists;
	UINT32 next_empty_item;
	UINT32 item_size;
	UINT32 i;
	BYTE* lsbyte;


	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
	/* Build the structure based on the base address:*/
	ls = (LLM_LIST *)l;
	lsbyte = (BYTE *)ls;
	total_lists = ls->total_lists;

	ls->lh = (LLM_LIST_HEAD *)(lsbyte + sizeof(LLM_LIST));
	ls->list_head_alloc = (void *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)));
	ls->li = (LLM_LIST_ITEM *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)) + ls->head_alloc_size);
	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/



	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
	/* Make sure list handle is valid.*/
	if (list_handle >= ls->total_lists)
		return(OCTAPI_LLM_INVALID_LIST_HANDLE);

	/* Make sure there is at least one item.*/
	if (num_items == 0)
		return(OCTAPI_LLM_INVALID_PARAMETER);

	/* Make sure there are enough free nodes.*/
	if (num_items > (ls->total_items - ls->assigned_items))
		return(OCTAPI_LLM_NO_STRUCTURES_LEFT);
	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/


	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
	/* Get pointer to list structure.*/
	lh = &ls->lh[list_handle];
	if (lh->list_length == 0xFFFFFFFF)
		return(OCTAPI_LLM_INVALID_LIST_HANDLE);
	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/



	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
	/* Add the number of requested nodes to the list.*/
	item_list = ls->li;
	item_size = ls->item_size;
	next_empty_item = ls->next_empty_item;

	for (i=0; i<num_items; i++)
	{
		if (i == 0)
		{
			if (lh->head_pointer == 0xFFFFFFFF)
			{
				/* Current and next items are one and the same!*/
				curr_item = (LLM_LIST_ITEM *)((BYTE *)item_list + item_size * next_empty_item);

				/* Set new head and tail pointers.*/
				lh->head_pointer = next_empty_item;
				lh->tail_pointer = next_empty_item;

				/* Update current item pnt.*/
				curr_item_pnt = next_empty_item;

				/* Update next item.*/
				next_empty_item = curr_item->forward_link;

				/* Set first item to be only item in list.*/
				curr_item->forward_link = 0xFFFFFFFF;
			}
			else
			{
				/* Get a free item from the free item list!*/
				curr_item = (LLM_LIST_ITEM *)((BYTE *)item_list + item_size * lh->tail_pointer);
				free_item = (LLM_LIST_ITEM *)((BYTE *)item_list + item_size * next_empty_item);

				/* Have current item point to next empty item.*/
				curr_item->forward_link = next_empty_item;

				/* Update current item pnt.*/
				curr_item_pnt = next_empty_item;

				/* Update next_empty_item.*/
				next_empty_item = free_item->forward_link;

				/* Update pointers to current item and free item.*/
				curr_item = free_item;
			}
		}
		else
		{
			/* Update pointers to current item and free item.*/
			free_item = (LLM_LIST_ITEM *)((BYTE *)item_list + item_size * next_empty_item);

			/* Have current item point to next empty item.*/
			curr_item->forward_link = next_empty_item;

			/* Update current item pnt.*/
			curr_item_pnt = next_empty_item;

			/* Update next_empty_item.*/
			next_empty_item = free_item->forward_link;

			/* Update pointers to current item and free item.*/
			curr_item = free_item;
		}
	}

	/* Terminate list.*/
	if ( curr_item != NULL )
		curr_item->forward_link = 0xFFFFFFFF;

	/* Update llman structure variables.*/
	ls->next_empty_item = next_empty_item;
	ls->assigned_items += num_items;

	/* Update list variables.*/
	lh->list_length += num_items;
	lh->cache_item_pointer = curr_item_pnt;
	lh->cache_item_number = lh->list_length - 1;
	lh->tail_pointer = curr_item_pnt;

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLlmListAppendAndSetItems.
|
|	Description:
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLlmListAppendAndSetItems
UINT32 OctApiLlmListAppendAndSetItems(void* l, UINT32 list_handle, UINT32 num_items, void* data_list)
{
	LLM_LIST* ls;
	LLM_LIST_HEAD* lh;
	LLM_LIST_ITEM* item_list;
	LLM_LIST_ITEM* curr_item = NULL;
	LLM_LIST_ITEM* free_item;
	UINT32 curr_item_pnt = 0xFFFFFFFF;
	UINT32 total_lists;
	UINT32 next_empty_item;
	UINT32 user_info_bytes;
	UINT32 item_size;
	UINT32 i;
	BYTE* lsbyte;
	void* data_item;


	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
	/* Build the structure based on the base address:*/
	ls = (LLM_LIST *)l;
	lsbyte = (BYTE *)ls;
	total_lists = ls->total_lists;

	ls->lh = (LLM_LIST_HEAD *)(lsbyte + sizeof(LLM_LIST));
	ls->list_head_alloc = (void *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)));
	ls->li = (LLM_LIST_ITEM *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)) + ls->head_alloc_size);
	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/



	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
	/* Make sure list handle is valid.*/
	if (list_handle >= ls->total_lists)
		return(OCTAPI_LLM_INVALID_LIST_HANDLE);

	/* Make sure there is at least one item.*/
	if (num_items == 0)
		return(OCTAPI_LLM_INVALID_PARAMETER);

	/* Make sure there are enough free nodes.*/
	if (num_items > (ls->total_items - ls->assigned_items))
		return(OCTAPI_LLM_NO_STRUCTURES_LEFT);
	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/


	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
	/* Get pointer to list structure.*/
	lh = &ls->lh[list_handle];
	if (lh->list_length == 0xFFFFFFFF)
		return(OCTAPI_LLM_INVALID_LIST_HANDLE);
	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/



	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
	/* Add the number of requested nodes to the list.*/
	item_list = ls->li;
	user_info_bytes = ls->user_info_bytes;
	item_size = ls->item_size;
	next_empty_item = ls->next_empty_item;
	data_item = data_list;

	for (i=0; i<num_items; i++)
	{
		if (i == 0)
		{
			if (lh->head_pointer == 0xFFFFFFFF)
			{
				/* Current and next items are one and the same!*/
				curr_item = (LLM_LIST_ITEM *)((BYTE *)item_list + item_size * next_empty_item);

				/* Set new head and tail pointers.*/
				lh->head_pointer = next_empty_item;
				lh->tail_pointer = next_empty_item;

				/* Update current item pnt.*/
				curr_item_pnt = next_empty_item;

				/* Update next item.*/
				next_empty_item = curr_item->forward_link;

				/* Set first item to be only item in list.*/
				curr_item->forward_link = 0xFFFFFFFF;
			}
			else
			{
				/* Get a free item from the free item list!*/
				curr_item = (LLM_LIST_ITEM *)((BYTE *)item_list + item_size * lh->tail_pointer);
				free_item = (LLM_LIST_ITEM *)((BYTE *)item_list + item_size * next_empty_item);

				/* Have current item point to next empty item.*/
				curr_item->forward_link = next_empty_item;

				/* Update current item pnt.*/
				curr_item_pnt = next_empty_item;

				/* Update next_empty_item.*/
				next_empty_item = free_item->forward_link;

				/* Update pointers to current item and free item.*/
				curr_item = free_item;
			}
		}
		else
		{
			/* Update pointers to current item and free item.*/
			free_item = (LLM_LIST_ITEM *)((BYTE *)item_list + item_size * next_empty_item);

			/* Have current item point to next empty item.*/
			curr_item->forward_link = next_empty_item;

			/* Update current item pnt.*/
			curr_item_pnt = next_empty_item;

			/* Update next_empty_item.*/
			next_empty_item = free_item->forward_link;

			/* Update pointers to current item and free item.*/
			curr_item = free_item;
		}

		/* Copy data to new item.*/
		OctApiLlmMemCpy(curr_item->user_info, data_item, user_info_bytes);

		/* Update data_item pointer for next iteration (item).*/
		data_item = (void *)((BYTE *)data_item + user_info_bytes);
	}


	/* Terminate list.*/
	if ( curr_item != NULL )
		curr_item->forward_link = 0xFFFFFFFF;

	/* Update llman structure variables.*/
	ls->next_empty_item = next_empty_item;
	ls->assigned_items += num_items;

	/* Update list variables.*/
	lh->list_length += num_items;
	lh->cache_item_pointer = curr_item_pnt;
	lh->cache_item_number = lh->list_length - 1;
	lh->tail_pointer = curr_item_pnt;

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLlmListSetItems.
|
|	Description:	This function takes a start entry (0 to length - 1),
|					a pointer to a list of data (each item of list is the
|					size of one data unit, specified at init), and the
|					length of the data list.  From this, the data will be
|					copied from the data list to the linked list, from
|					entry start_entry to (start_entry + data_length - 1).
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLlmListSetItems
UINT32 OctApiLlmListSetItems(void* l, UINT32 list_handle, UINT32 start_item, UINT32 data_length, void* pdata_list)
{
	LLM_LIST* ls;
	LLM_LIST_HEAD* lh;
	LLM_LIST_ITEM* item = NULL;
	UINT32 total_lists;
	UINT32 item_pnt = 0xFFFFFFFF;
	UINT32 i, j;
	BYTE* lsbyte;
	void* pdata_item = NULL;


	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
	/* Build the structure based on the base address:*/
	ls = (LLM_LIST *)l;
	lsbyte = (BYTE *)ls;
	total_lists = ls->total_lists;

	ls->lh = (LLM_LIST_HEAD *)(lsbyte + sizeof(LLM_LIST));
	ls->list_head_alloc = (void *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)));
	ls->li = (LLM_LIST_ITEM *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)) + ls->head_alloc_size);
	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/



	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
	/* Make sure list handle is valid.*/
	if (list_handle >= ls->total_lists)
		return(OCTAPI_LLM_INVALID_LIST_HANDLE);
	lh = &ls->lh[list_handle];
	if (lh->list_length == 0xFFFFFFFF)
		return(OCTAPI_LLM_INVALID_LIST_HANDLE);

	/* Make sure the start_entry is within limits.*/
	if (start_item >= lh->list_length)
		return(OCTAPI_LLM_INVALID_PARAMETER);

	/* Make sure the end_entry is within limits.*/
	lh = &ls->lh[list_handle];
	if ((start_item + data_length) > lh->list_length)
		return(OCTAPI_LLM_INVALID_PARAMETER);
	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/



	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
	/* Set the data of each node.*/
	for (i=0; i<data_length; i++)
	{
		/* Obtain pointer to current item.*/
		if (i == 0)	
		{
			/* Check if location of start item is already cached.  If not, must search
				for it manually.*/
			if (start_item == (lh->cache_item_number + 1))
			{
				item = (LLM_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * lh->cache_item_pointer);
				item_pnt = item->forward_link;
				item = (LLM_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * item_pnt);
			}
			else
			{
				item = (LLM_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * lh->head_pointer);
				item_pnt = lh->head_pointer;
				for (j=0; j<start_item; j++)
				{
					item_pnt = item->forward_link;
					item = (LLM_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * item_pnt);
				}
			}

			pdata_item = (void *)((BYTE *)pdata_list + (i * ls->user_info_bytes));
		}
		else
		{
			item_pnt = item->forward_link;
			item = (LLM_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * item_pnt);

			pdata_item = (void *)((BYTE *)pdata_item + ls->user_info_bytes);
		}

		/* Set the value of the item's user data.*/
		OctApiLlmMemCpy(item->user_info, pdata_item, ls->user_info_bytes);
	}

	/* Write new cache entry.*/
	lh->cache_item_pointer = item_pnt;
	lh->cache_item_number = start_item + data_length - 1;

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLlmListCopyData.
|
|	Description:	This function takes a start entry (0 to length - 1),
|					a pointer to a list of data (each item of list is the
|					size of one data unit, specified at init), and the
|					length of the data list.  From this, the data will be
|					copied from the linked list to the data list, from
|					entry start_entry of the linked list to
|					(start_entry + data_length - 1).
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLlmListCopyData
UINT32 OctApiLlmListCopyData(void* l, UINT32 list_handle, UINT32 start_item, UINT32 data_length, void* pdata_list)
{
	LLM_LIST* ls;
	LLM_LIST_HEAD* lh;
	LLM_LIST_ITEM* item = NULL;
	UINT32 item_pnt = 0xFFFFFFFF;
	UINT32 total_lists;
	UINT32 i, j;
	BYTE* lsbyte;
	void* pdata_item = NULL;


	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
	/* Build the structure based on the base address:*/
	ls = (LLM_LIST *)l;
	lsbyte = (BYTE *)ls;
	total_lists = ls->total_lists;

	ls->lh = (LLM_LIST_HEAD *)(lsbyte + sizeof(LLM_LIST));
	ls->list_head_alloc = (void *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)));
	ls->li = (LLM_LIST_ITEM *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)) + ls->head_alloc_size);
	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/



	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
	/* Make sure list handle is valid.*/
	if (list_handle >= ls->total_lists)
		return(OCTAPI_LLM_INVALID_LIST_HANDLE);
	lh = &ls->lh[list_handle];
	if (lh->list_length == 0xFFFFFFFF)
		return(OCTAPI_LLM_INVALID_LIST_HANDLE);

	/* Make sure the start_entry is within limits.*/
	if (start_item >= lh->list_length)
		return(OCTAPI_LLM_INVALID_PARAMETER);

	/* Make sure the end_entry is within limits.*/
	lh = &ls->lh[list_handle];
	if ((start_item + data_length) > lh->list_length)
		return(OCTAPI_LLM_INVALID_PARAMETER);
	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/



	/*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*=*/
	/* Set the data of each node.*/
	for (i=0; i<data_length; i++)
	{
		/* Obtain pointer to current item.*/
		if (i == 0)	
		{
			/* Check if location of start item is already cached.  If not, must search
				for it manually.*/
			if (start_item == (lh->cache_item_number + 1))
			{
				item = (LLM_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * lh->cache_item_pointer);
				item_pnt = item->forward_link;
				item = (LLM_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * item_pnt);
			}
			else
			{
				item = (LLM_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * lh->head_pointer);
				for (j=0; j<start_item; j++)
				{
					item_pnt = item->forward_link;
					item = (LLM_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * item_pnt);
				}
			}

			pdata_item = (void *)((BYTE *)pdata_list + (i * ls->user_info_bytes));
		}
		else
		{
			item_pnt = item->forward_link;
			item = (LLM_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * item_pnt);

			pdata_item = (void *)((BYTE *)pdata_item + ls->user_info_bytes);
		}

		/* Set the value of the item's user data.*/
		OctApiLlmMemCpy(pdata_item, item->user_info, ls->user_info_bytes);
	}

	/* Write new cache entry.*/
	lh->cache_item_pointer = item_pnt;
	lh->cache_item_number = start_item + data_length - 1;

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLlmListRemoveItem.
|
|	Description:	This function deallocates a node of the linked list specified
|					by the handle list_handle.  
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*l					void			The memory used by the LLM_LIST structure.
|	list_handle			UINT32			The handle to the list.
|	item_number			UINT32			The number of the item to be removed.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLlmListRemoveItem
UINT32 OctApiLlmListRemoveItem(void * l,UINT32 list_handle,UINT32 item_number)
{
	LLM_LIST* ls;
	LLM_LIST_ITEM* freed_item = NULL;
	LLM_LIST_HEAD* lh;
	UINT32 freed_item_pnt = 0xFFFFFFFF;
	UINT32 total_lists;
	BYTE* lsbyte;
	UINT32	fConditionFlag = TRUE;

	/* Built the structure based on the base address:*/
	ls = (LLM_LIST *)l;
	lsbyte = (BYTE *)ls;
	total_lists = ls->total_lists;

	ls->lh = (LLM_LIST_HEAD *)(lsbyte + sizeof(LLM_LIST));
	ls->list_head_alloc = (void *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)));
	ls->li = (LLM_LIST_ITEM *)(lsbyte + sizeof(LLM_LIST) + (total_lists * sizeof(LLM_LIST_HEAD)) + ls->head_alloc_size);

	lh = &ls->lh[list_handle];
	
	if (list_handle >= ls->total_lists) return(OCTAPI_LLM_BLOCKNUM_OUT_OF_RANGE);
	if (lh->list_length == 0xFFFFFFFF) return(OCTAPI_LLM_INVALID_LIST_HANDLE);
	if (lh->list_length <= item_number)	return(OCTAPI_LLM_BLOCKNUM_OUT_OF_RANGE);

	if (item_number == 0 && lh->list_length == 1)/* First item and only item:*/
	{
		freed_item_pnt = lh->head_pointer;
		freed_item = (LLM_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * freed_item_pnt);

		lh->head_pointer = 0xFFFFFFFF;
		lh->tail_pointer = 0xFFFFFFFF;

		lh->cache_item_number = 0xFFFFFFFF;
		lh->cache_item_pointer = 0xFFFFFFFF;
	}
	else if (item_number == 0)	/* First item and but list not empty:*/
	{
		freed_item_pnt = ls->lh[list_handle].head_pointer;
		freed_item = (LLM_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * freed_item_pnt);

		lh->head_pointer = freed_item->forward_link;

		lh->cache_item_number = 0;
		lh->cache_item_pointer = freed_item->forward_link;
	}
	else	/* Discard non-first item! (Caution: this could be the last item!)*/
	{
		LLM_LIST_ITEM * last_item = NULL;
		LLM_LIST_ITEM * item;
		UINT32 last_list_pnt;
		UINT32 cur_list_pnt;
		UINT32 cur_list_num;

		if (lh->cache_item_number < item_number)	/* Start at cache:*/
		{
			cur_list_pnt = lh->cache_item_pointer;
			cur_list_num = lh->cache_item_number;
		}
		else /* Start at beginning:*/
		{
			cur_list_pnt = lh->head_pointer;
			cur_list_num = 0;
		}

		last_list_pnt = 0xFFFFFFFF;

		/* Start search from cur_list_pnt and cur_list_num.*/
		while( fConditionFlag == TRUE )
		{
			item = (LLM_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * cur_list_pnt);

			if (cur_list_num == item_number) /* Item number found.*/
			{
 				if (last_list_pnt == 0xFFFFFFFF) return(OCTAPI_LLM_INTERNAL_ERROR1);

				if ((item_number + 1) == lh->list_length)
				{
					lh->tail_pointer = last_list_pnt;
					last_item->forward_link = 0xFFFFFFFF;
				}
				else
				{
					last_item->forward_link = item->forward_link;
				}
				freed_item_pnt = cur_list_pnt;
				freed_item = item;

				/* Reset cache entry.*/
				lh->cache_item_pointer = last_list_pnt;
				lh->cache_item_number = cur_list_num - 1;

				fConditionFlag = FALSE;
				break;
			}
			else if (item->forward_link == 0xFFFFFFFF) /* End of list found?!?*/
			{
				return(OCTAPI_LLM_INTERNAL_ERROR0);
			}
			else /* Item was not found, but continue searching.*/
			{
				last_item = item;
				last_list_pnt = cur_list_pnt;
				cur_list_pnt = item->forward_link;
			}

			cur_list_num++;
		}
	}

	/* Decrease the list length.*/
	lh->list_length--;
	ls->assigned_items--;

	/* Return free block to free block list:*/
	freed_item->forward_link = ls->next_empty_item;
	ls->next_empty_item = freed_item_pnt;

	return(GENERIC_OK);
}
#endif

/**************************************** llm2 function section *****************************************/


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLlm2ListGetSize
|
|	Description:	This function determines the amount of memory needed by
|					the LLM2_LIST structure to manage the allocation of
|					number_of_items number of resources.  The memory is
|					measured in bytes.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	number_of_items		UINT32			The number of resources to be allocated
|										amongst all linked-lists.
|	number_of_lists		UINT32			The maximum number of linked-lists that
|										can be allocated.
|	*l_size	UINT32		UINT32			The amount of memory needed, returned.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLlm2ListGetSize
UINT32 OctApiLlm2ListGetSize(UINT32 number_of_items,UINT32 number_of_lists,UINT32 user_info_size,UINT32 * l_size)
{
	UINT32 head_alloc_size;
	UINT32 result;
	UINT32 user_info_size_roundup;

	if (number_of_items == 0) return(GENERIC_BAD_PARAM);
	if (number_of_lists == 0) return(GENERIC_BAD_PARAM);
	if (user_info_size == 0) return(GENERIC_BAD_PARAM);

	user_info_size_roundup = ((user_info_size + 3) / 4) * 4;

	result = OctapiLlmAllocGetSize(number_of_lists,&head_alloc_size);
	if(result != GENERIC_OK) return(result);

	*l_size = sizeof(LLM2_LIST) + (number_of_lists * sizeof(LLM2_LIST_HEAD)) + head_alloc_size + (number_of_items * (sizeof(LLM2_LIST_ITEM) + user_info_size_roundup - 4));

	return(GENERIC_OK);
}
#endif

#if !SKIP_OctApiLlm2ListGetItemPointer
LLM2_LIST_ITEM * OctApiLlm2ListGetItemPointer(LLM2_LIST * ls, UINT32 item_number)
{
	return (LLM2_LIST_ITEM *) (((BYTE *)ls->li) + (ls->item_size * item_number)) ;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLlm2ListInit.
|
|	Description:	This function intializes the LLM2_TALLOC structure.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*l					void			The memory used by the LLM2_LIST structure.
|	number_of_items		UINT32			The number of resources to be allocated
|										amongst all linked-lists.
|	number_of_lists		UINT32			The maximum number of linked-lists that
|										can be allocated.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLlm2ListInit
UINT32 OctApiLlm2ListInit(void ** l,UINT32 number_of_items,UINT32 number_of_lists,UINT32 user_info_size)
{
	LLM2_LIST* ls;
	LLM2_LIST_ITEM* item;
	UINT32 i;
	UINT32 head_alloc_size;
	UINT32 result;
	UINT32 user_info_size_roundup;
	UINT32 total_lists;
	BYTE* lsbyte;


	if (number_of_items == 0) return(GENERIC_BAD_PARAM);
	if (number_of_lists == 0) return(GENERIC_BAD_PARAM);
	if (user_info_size == 0) return(GENERIC_BAD_PARAM);

	user_info_size_roundup = ((user_info_size + 3) / 4) * 4;

	/* Get the size of the Alloc structure used to manage head of list structures.*/
	result = OctapiLlmAllocGetSize(number_of_lists,&head_alloc_size);
	if(result != GENERIC_OK) return(result);

	if (*l == NULL) return(OCTAPI_LLM_MEMORY_NOT_ALLOCATED);

	/* Built the structure based on the base address:*/
	ls = (LLM2_LIST *)(*l);
	lsbyte = (BYTE *)ls;
	total_lists = ls->total_lists;

	ls->lh = (LLM2_LIST_HEAD *)(lsbyte + sizeof(LLM2_LIST));
	ls->list_head_alloc = (void *)(lsbyte + sizeof(LLM2_LIST) + (total_lists * sizeof(LLM2_LIST_HEAD)));
	ls->li = (LLM2_LIST_ITEM *)(lsbyte + sizeof(LLM2_LIST) + (total_lists * sizeof(LLM2_LIST_HEAD)) + ls->head_alloc_size);

	/* Initialize parameters in the structure.*/
	ls->head_alloc_size = head_alloc_size;
	ls->user_info_bytes = user_info_size;
	ls->user_info_size = user_info_size_roundup;
	ls->total_items = number_of_items;
	ls->assigned_items = 0;
	ls->total_lists = number_of_lists;
	ls->assigned_lists = 0;
	ls->next_empty_item = 0;
	ls->item_size = sizeof(LLM2_LIST_ITEM) + user_info_size_roundup - 4;

	/* Complete the build!*/
	ls = (LLM2_LIST *)(*l);
	lsbyte = (BYTE *)ls;
	total_lists = ls->total_lists;

	ls->lh = (LLM2_LIST_HEAD *)(lsbyte + sizeof(LLM2_LIST));
	ls->list_head_alloc = (void *)(lsbyte + sizeof(LLM2_LIST) + (total_lists * sizeof(LLM2_LIST_HEAD)));
	ls->li = (LLM2_LIST_ITEM *)(lsbyte + sizeof(LLM2_LIST) + (total_lists * sizeof(LLM2_LIST_HEAD)) + ls->head_alloc_size);

	/* Initialize the head of queue Alloc structure.*/
	result = OctapiLlmAllocInit(&(ls->list_head_alloc),number_of_lists);
	if(result != GENERIC_OK) return(result);

	/* Initialize the linked list of the items:*/
	for(i=0; i<number_of_items; i++)
	{
		item = (LLM2_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * i);

		if (i == (number_of_items - 1))
			item->forward_link = 0xFFFFFFFF;
		else
			item->forward_link = i + 1;
	}

	return(GENERIC_OK);
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLlm2ListCreate.
|
|	Description:	This function creates a linked list.  The target which is
|					allocated the newly created list can request additions
|					or removals from the list later on.  To target identifies
|					its list with the returned list handle.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*l					void			The memory used by the LLM_LIST structure.
|	*list_handle		UINT32			The handle to the new list, returned.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLlm2ListCreate
UINT32 OctApiLlm2ListCreate(void * l,UINT32 * list_handle)
{
	LLM2_LIST* ls;
	LLM2_LIST_HEAD* lh;
	UINT32 blocknum;
	UINT32 total_lists;
	UINT32 result;
	BYTE* lsbyte;

	/* Built the structure based on the base address:*/
	ls = (LLM2_LIST *)l;
	lsbyte = (BYTE *)ls;
	total_lists = ls->total_lists;

	ls->lh = (LLM2_LIST_HEAD *)(lsbyte + sizeof(LLM2_LIST));
	ls->list_head_alloc = (void *)(lsbyte + sizeof(LLM2_LIST) + (total_lists * sizeof(LLM2_LIST_HEAD)));
	ls->li = (LLM2_LIST_ITEM *)(lsbyte + sizeof(LLM2_LIST) + (total_lists * sizeof(LLM2_LIST_HEAD)) + ls->head_alloc_size);

	/* Get a list using the list head alloc structure.*/
	result = OctapiLlmAllocAlloc(ls->list_head_alloc, &blocknum);
	if (result != GENERIC_OK) return(result);

	/* The handle is the block number.*/
	*list_handle = blocknum;

	/* Initialize the list head structure.*/
	lh = &ls->lh[blocknum];
	lh->list_length = 0;
	lh->head_pointer = 0xFFFFFFFF;
	lh->tail_pointer = 0xFFFFFFFF;

	ls->assigned_lists++;

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLlmListDelete.
|
|	Description:	This function deletes the linked list indicated by the
|					handle list_handle.  Any items which are still allocated
|					to the list are first deallocated.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*l					void			The memory used by the LLM2_LIST structure.
|	*list_handle		UINT32			The handle to the list.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLlm2ListDelete
UINT32 OctApiLlm2ListDelete(void * l,UINT32 list_handle)
{
	LLM2_LIST* ls;
	LLM2_LIST_HEAD* lh;
	UINT32 total_lists;
	UINT32 result;
	BYTE* lsbyte;

	/* Built the structure based on the base address:*/
	ls = (LLM2_LIST *)l;
	lsbyte = (BYTE *)ls;
	total_lists = ls->total_lists;

	ls->lh = (LLM2_LIST_HEAD *)(lsbyte + sizeof(LLM2_LIST));
	ls->list_head_alloc = (void *)(lsbyte + sizeof(LLM2_LIST) + (total_lists * sizeof(LLM2_LIST_HEAD)));
	ls->li = (LLM2_LIST_ITEM *)(lsbyte + sizeof(LLM2_LIST) + (total_lists * sizeof(LLM2_LIST_HEAD)) + ls->head_alloc_size);

	
	if (list_handle >= ls->total_lists) return(OCTAPI_LLM2_BLOCKNUM_OUT_OF_RANGE);
	if (ls->lh[list_handle].list_length == 0xFFFFFFFF) return(OCTAPI_LLM2_INVALID_LIST_HANDLE);

	/* Release internal list header handle...*/
	result = OctapiLlmAllocDealloc(ls->list_head_alloc,list_handle);
	if (result != GENERIC_OK) return(result);

	lh = &ls->lh[list_handle];

	/* Deallocate all items in the list!*/
	if (lh->list_length != 0)
	{
		LLM2_LIST_ITEM * item;

		item = (LLM2_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * lh->tail_pointer);

		/* Release the items using only the links.*/
		item->forward_link = ls->next_empty_item;
		ls->next_empty_item = lh->head_pointer;

		/* Remove items from item counter.*/
		ls->assigned_items -= lh->list_length;
	}

	lh->list_length = 0xFFFFFFFF;
	lh->head_pointer = 0xFFFFFFFF;
	lh->tail_pointer = 0xFFFFFFFF;

	ls->assigned_lists--;

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLlmListLength.
|
|	Description:	This function returns the number of items allocated to the
|					list indicated by the handle list_handle.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*l					void			The memory used by the LLM2_LIST structure.
|	list_handle			UINT32			The handle to the list.
|	*number_of_items	UINT32			The number of items in the list, returned.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLlm2ListLength
UINT32 OctApiLlm2ListLength(void * l,UINT32 list_handle, UINT32 * number_of_items_in_list)
{
	LLM2_LIST* ls;
	LLM2_LIST_HEAD* lh;
	UINT32 total_lists;
	BYTE* lsbyte;

	/* Built the structure based on the base address:*/
	ls = (LLM2_LIST *)l;
	lsbyte = (BYTE *)ls;
	total_lists = ls->total_lists;

	ls->lh = (LLM2_LIST_HEAD *)(lsbyte + sizeof(LLM2_LIST));
	ls->list_head_alloc = (void *)(lsbyte + sizeof(LLM2_LIST) + (total_lists * sizeof(LLM2_LIST_HEAD)));
	ls->li = (LLM2_LIST_ITEM *)(lsbyte + sizeof(LLM2_LIST) + (total_lists * sizeof(LLM2_LIST_HEAD)) + ls->head_alloc_size);

	lh = &ls->lh[list_handle];
	
	if (list_handle >= ls->total_lists) return(OCTAPI_LLM2_BLOCKNUM_OUT_OF_RANGE);
	if (lh->list_length == 0xFFFFFFFF) return(OCTAPI_LLM2_INVALID_LIST_HANDLE);

	*number_of_items_in_list = lh->list_length;

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLlm2ListItemData
|
|	Description:	This function returns a pointer to the user data associated
|					with an item.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*l					void			The memory used by the LLM2_LIST structure.
|	list_handle			UINT32			The handle to the list.
|	item_number			UINT32			The number of the list node in question.
|	**item_data_pnt		void			The pointer to the user data, returned.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLlm2ListItemData
UINT32 OctApiLlm2ListItemData(void * l,UINT32 list_handle,UINT32 item_key,void ** item_data_pnt, PUINT32 item_number_pnt)
{
	LLM2_LIST* ls;
	LLM2_LIST_HEAD* lh;
	LLM2_LIST_ITEM* item;
	UINT32 cur_list_pnt;
	UINT32 cur_list_key = 0xFFFFFFFF;
	UINT32 total_lists;
	UINT32 list_length;
	BYTE* lsbyte;
	UINT32	fConditionFlag = TRUE;

	/* Built the structure based on the base address:*/
	ls = (LLM2_LIST *)l;
	lsbyte = (BYTE *)ls;
	total_lists = ls->total_lists;

	ls->lh = (LLM2_LIST_HEAD *)(lsbyte + sizeof(LLM2_LIST));
	ls->list_head_alloc = (void *)(lsbyte + sizeof(LLM2_LIST) + (total_lists * sizeof(LLM2_LIST_HEAD)));
	ls->li = (LLM2_LIST_ITEM *)(lsbyte + sizeof(LLM2_LIST) + (total_lists * sizeof(LLM2_LIST_HEAD)) + ls->head_alloc_size);

	lh = &ls->lh[list_handle];
	list_length = lh->list_length;
	
	*item_data_pnt = NULL;
	*item_number_pnt = 0;
	if (list_handle >= ls->total_lists) return(OCTAPI_LLM2_BLOCKNUM_OUT_OF_RANGE);
	if (list_length == 0xFFFFFFFF) return(OCTAPI_LLM2_INVALID_LIST_HANDLE);

	/* Determine where the search will start.*/
	/* Start at beginning:*/
	cur_list_pnt = lh->head_pointer;
	item = (LLM2_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * cur_list_pnt);
	cur_list_key = item->key;
	
	/* Start search from cur_list_pnt and cur_list_num.*/
	while ( fConditionFlag == TRUE )
	{
		if (cur_list_key == item_key) /* Item key found.*/
		{
			/* Get item info.*/
			*item_data_pnt = (void *)item->user_info;

			return(GENERIC_OK);
		}
		else if(item->forward_link == 0xFFFFFFFF) /* End of list found?!?*/
		{
			return(OCTAPI_LLM2_INTERNAL_ERROR0);
		}
		else /* Item was not found, but continue searching.*/
		{
			cur_list_pnt = item->forward_link;
		}

		item = (LLM2_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * cur_list_pnt);
		cur_list_key = item->key;
		(*item_number_pnt)++;
	}

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLlm2ListInsertItem.
|
|	Description:	This function allocates a node to the linked list specified
|					by the handle list_handle.  The position of the new item
|					will be defined based on the key value.  All entry are inserted
|					in the list in incremental Key value.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*l					void			The memory used by the LLM2_LIST structure.
|	*list_handle		UINT32			The handle to the list.
|	**item_data			void			Address of the user data space for this item.
|	**prev_item_data	void			Address of the user data space for the previous item.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLlm2ListInsertItem
UINT32 OctApiLlm2ListInsertItem(void * l,UINT32 list_handle,UINT32 item_key,void ** item_data_pnt, void ** prev_item_data_pnt, void ** prev_prev_item_data_pnt, PUINT32 insert_status_pnt )
{
	LLM2_LIST* ls;
	LLM2_LIST_HEAD* lh;
	LLM2_LIST_ITEM* free_item;
	UINT32 free_item_pnt;
	UINT32 total_lists;
	BYTE* lsbyte;
	UINT32	ulPassCount = 0;
	UINT32	fConditionFlag = TRUE;

	/* Set the status of the insertion.*/
	*insert_status_pnt = OCTAPI_LLM2_INSERT_ERROR;

	/* Built the structure based on the base address:*/
	ls = (LLM2_LIST *)l;
	lsbyte = (BYTE *)ls;
	total_lists = ls->total_lists;

	ls->lh = (LLM2_LIST_HEAD *)(lsbyte + sizeof(LLM2_LIST));
	ls->list_head_alloc = (void *)(lsbyte + sizeof(LLM2_LIST) + (total_lists * sizeof(LLM2_LIST_HEAD)));
	ls->li = (LLM2_LIST_ITEM *)(lsbyte + sizeof(LLM2_LIST) + (total_lists * sizeof(LLM2_LIST_HEAD)) + ls->head_alloc_size);

	lh = &ls->lh[list_handle];
	
	*item_data_pnt = NULL;
	if (list_handle >= ls->total_lists) return(OCTAPI_LLM2_BLOCKNUM_OUT_OF_RANGE);
	if (lh->list_length == 0xFFFFFFFF) return(OCTAPI_LLM2_INVALID_LIST_HANDLE);
	if (ls->next_empty_item == 0xFFFFFFFF) return(OCTAPI_LLM2_NO_STRUCTURES_LEFT);

	/* Get a free item from the free item list!*/
	free_item_pnt = ls->next_empty_item;
	free_item = (LLM2_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * free_item_pnt);
	free_item->key = item_key;
	ls->next_empty_item = free_item->forward_link;

	if (lh->list_length == 0)	/* First item and only item:*/
	{
		free_item->forward_link = 0xFFFFFFFF;
		lh->tail_pointer = free_item_pnt;
		lh->head_pointer = free_item_pnt;
		*insert_status_pnt = OCTAPI_LLM2_INSERT_FIRST_NODE;

		/* There is no previous node information to return.*/
		*prev_item_data_pnt = NULL;
		*prev_prev_item_data_pnt = NULL;
	}
	else	/* Insert:*/
	{
		LLM2_LIST_ITEM * last_last_item = NULL;
		LLM2_LIST_ITEM * last_item = NULL;
		LLM2_LIST_ITEM * item;
		UINT32 last_list_pnt;
		UINT32 cur_list_pnt;
		UINT32 cur_list_key = 0xFFFFFFFF;

		/* Start at beginning:*/
		cur_list_pnt = lh->head_pointer;
		item = (LLM2_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * cur_list_pnt);
		cur_list_key = item->key;
		
		last_list_pnt = 0xFFFFFFFF;

		/* Start search from cur_list_pnt and cur_list_num.*/
		while ( fConditionFlag == TRUE )
		{
			/* Increment the pass count to determine if the addition will happen next to last.*/
			ulPassCount++;
	
			if (cur_list_key >= item_key) /* Item new node between the last and the curent. */
			{
				if (last_list_pnt == 0xFFFFFFFF) /* Must insert at the head of the list.*/
				{
					free_item->forward_link = cur_list_pnt;
					lh->head_pointer = free_item_pnt;
				}
				else									/* Standard insertion.*/
				{
					free_item->forward_link = cur_list_pnt;
					last_item->forward_link = free_item_pnt;
				}
			
				/* Check if the entry was made before the last one.*/
				if ( ulPassCount == lh->list_length )
					*insert_status_pnt = OCTAPI_LLM2_INSERT_BEFORE_LAST_NODE;
				else
					*insert_status_pnt = OCTAPI_LLM2_INSERT_LIST_NODE;

				fConditionFlag = FALSE;
				break;
			}
			else if (item->forward_link == 0xFFFFFFFF) /* End of list found, must insert at the end.*/
			{
				free_item->forward_link = 0xFFFFFFFF;
				item->forward_link = free_item_pnt;
				lh->tail_pointer = free_item_pnt;

				*insert_status_pnt = OCTAPI_LLM2_INSERT_LAST_NODE;

				fConditionFlag = FALSE;
				break;
			}
			else /* Item was not found, but continue searching.*/
			{
				last_last_item = last_item;
				last_item = item;
				last_list_pnt = cur_list_pnt;
				cur_list_pnt = item->forward_link;
			}

			item = (LLM2_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * cur_list_pnt);
			cur_list_key = item->key;

		}

		/* Return the previous node if possible.*/
		if ( *insert_status_pnt == OCTAPI_LLM2_INSERT_LIST_NODE ||
		     *insert_status_pnt == OCTAPI_LLM2_INSERT_BEFORE_LAST_NODE )
		{
			if ( last_item != NULL )
				*prev_item_data_pnt = (void *)last_item->user_info;

			if ( last_last_item != NULL )
				*prev_prev_item_data_pnt = (void *)last_last_item->user_info;
			else
				*prev_prev_item_data_pnt = NULL;
		}
		else
		{
			*prev_item_data_pnt = (void *)item->user_info;

			if ( ( last_last_item != NULL ) && ( last_item != NULL ) )
				*prev_prev_item_data_pnt = (void *)last_item->user_info;
			else
				*prev_prev_item_data_pnt = NULL;
		}
	}

	/* Increase the list length.*/
	lh->list_length++;
	ls->assigned_items++;
	*item_data_pnt = (void *)free_item->user_info;

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLlm2ListRemoveItem.
|
|	Description:	This function deallocates a node of the linked list specified
|					by the handle list_handle.  
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*l					void			The memory used by the LLM2_LIST structure.
|	list_handle			UINT32			The handle to the list.
|	item_key			UINT32			The key of the item to be removed.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLlm2ListRemoveItem
UINT32 OctApiLlm2ListRemoveItem(void * l,UINT32 list_handle,UINT32 item_key, PUINT32 prev_item_key_pnt, PUINT32 prev_prev_item_key_pnt, PUINT32 remove_status_pnt )
{
	LLM2_LIST* ls;
	LLM2_LIST_ITEM* freed_item = NULL;
	LLM2_LIST_HEAD* lh;
	UINT32 freed_item_pnt = 0xFFFFFFFF;
	UINT32 total_lists;
	BYTE* lsbyte;
	UINT32	fConditionFlag = TRUE;
	UINT32	ulPassCount = 0;

	/* Built the structure based on the base address:*/
	ls = (LLM2_LIST *)l;
	lsbyte = (BYTE *)ls;
	total_lists = ls->total_lists;

	/* Set the status of the removal to error as a default value.*/
	*remove_status_pnt = OCTAPI_LLM2_REMOVE_ERROR;
	
	ls->lh = (LLM2_LIST_HEAD *)(lsbyte + sizeof(LLM2_LIST));
	ls->list_head_alloc = (void *)(lsbyte + sizeof(LLM2_LIST) + (total_lists * sizeof(LLM2_LIST_HEAD)));
	ls->li = (LLM2_LIST_ITEM *)(lsbyte + sizeof(LLM2_LIST) + (total_lists * sizeof(LLM2_LIST_HEAD)) + ls->head_alloc_size);

	lh = &ls->lh[list_handle];
	
	if (list_handle >= ls->total_lists) return(OCTAPI_LLM2_BLOCKNUM_OUT_OF_RANGE);
	if (lh->list_length == 0xFFFFFFFF) return(OCTAPI_LLM2_INVALID_LIST_HANDLE);

	if (lh->list_length == 1)/* First item and only item if he matches.*/
	{
		freed_item_pnt = lh->head_pointer;
		freed_item = (LLM2_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * freed_item_pnt);

		if ( freed_item->key == item_key )
		{
			lh->head_pointer = 0xFFFFFFFF;
			lh->tail_pointer = 0xFFFFFFFF;
		}
		else
			return(OCTAPI_LLM2_INTERNAL_ERROR1);
		
		/* Indicate that there was no node prior to the one removed.*/
		*prev_item_key_pnt = 0xFFFFFFFF;
		*prev_prev_item_key_pnt = 0xFFFFFFFF;
		*remove_status_pnt = OCTAPI_LLM2_REMOVE_FIRST_NODE;
	}
	else	/* Discard non-first item! (Caution: this could be the last item!)*/
	{
		LLM2_LIST_ITEM * last_last_item = NULL;
		LLM2_LIST_ITEM * last_item = NULL;
		LLM2_LIST_ITEM * item;
		UINT32 last_list_pnt;
		UINT32 cur_list_pnt;
		UINT32 cur_list_key;

		/* Start at beginning:*/
		cur_list_pnt = lh->head_pointer;
		item = (LLM2_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * cur_list_pnt);
		cur_list_key = item->key;
		
		last_list_pnt = 0xFFFFFFFF;

		/* Start search from cur_list_pnt and cur_list_num.*/
		while( fConditionFlag == TRUE )
		{
			ulPassCount++;
			if (cur_list_key == item_key) /* Item number found.*/
			{
 				if (last_list_pnt == 0xFFFFFFFF)	/* First item in the list.*/
				{
					lh->head_pointer = item->forward_link;
					*remove_status_pnt = OCTAPI_LLM2_REMOVE_FIRST_NODE;
				}
				else if ( item->forward_link == 0xFFFFFFFF)	/* Last item of the list.*/
				{
					last_item->forward_link = 0xFFFFFFFF;
					lh->tail_pointer = last_list_pnt;
					*remove_status_pnt = OCTAPI_LLM2_REMOVE_LAST_NODE;
				}
				else
				{
					last_item->forward_link = item->forward_link;

					if ( ulPassCount == ( lh->list_length - 1 ) )
						*remove_status_pnt = OCTAPI_LLM2_REMOVE_BEFORE_LAST_NODE;
					else
						*remove_status_pnt = OCTAPI_LLM2_REMOVE_LIST_NODE;
				}
					
				freed_item_pnt = cur_list_pnt;
				freed_item = item;

				fConditionFlag = FALSE;
				break;
			}
			else if (item->forward_link == 0xFFFFFFFF) /* End of list found?!?*/
			{
				return(OCTAPI_LLM2_INTERNAL_ERROR0);
			}
			else /* Item was not found, but continue searching.*/
			{
				last_last_item = last_item;
				last_item = item;
				last_list_pnt = cur_list_pnt;
				cur_list_pnt = item->forward_link;
			}

			item = (LLM2_LIST_ITEM *)((BYTE *)ls->li + ls->item_size * cur_list_pnt);
			cur_list_key = item->key;
		}

		/* Return the key of the node before the node removed if possible.*/
		if ( last_list_pnt == 0xFFFFFFFF )
			*prev_item_key_pnt = 0xFFFFFFFF;
		else if ( last_item != NULL )
			*prev_item_key_pnt = last_item->key;

		/* Return the key of the node before before the node removed if possible.*/
		if ( last_last_item == NULL )
			*prev_prev_item_key_pnt = 0xFFFFFFFF;
		else
			*prev_prev_item_key_pnt = last_last_item->key;

	}

	/* Decrease the list length.*/
	lh->list_length--;
	ls->assigned_items--;

	/* Return free block to free block list:*/
	freed_item->forward_link = ls->next_empty_item;
	ls->next_empty_item = freed_item_pnt;

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLlmMemCpy.
|
|	Description:	This function copies data from a source to a destination.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*f_pvDestination	VOID			The destination where to copy the data.
|	*f_pvSource			VOID			The source where to copy the data from.
|	f_ulSize			UINT32			The number of bytes to copy.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLlmMemCpy
VOID * OctApiLlmMemCpy( VOID *f_pvDestination, const VOID * f_pvSource, UINT32 f_ulSize )
{
    CHAR * pbyDst;
    const CHAR * pbySrc;
    UINT32 * f_pulAlignedDst;
    const UINT32 * f_pulAlignedSrc;
    
    pbyDst = (CHAR *)f_pvDestination;
    pbySrc = (const CHAR *)f_pvSource;

    /* 
     * If the size is small, or either SRC or DST is unaligned,
     * then punt into the byte copy loop.  This should be rare.
     */
    if ( ( f_ulSize < sizeof(UINT32) ) 
		|| ( ( (unsigned long)( pbySrc ) & ( sizeof(UINT32) - 1 ) ) | ( (unsigned long)( pbyDst ) & ( sizeof(UINT32) - 1 ) ) ) )
    {
        while ( f_ulSize-- )
            *pbyDst++ = *pbySrc++;
        return f_pvDestination;
    }
    
    f_pulAlignedDst = (UINT32 *)pbyDst;
    f_pulAlignedSrc = (const UINT32 *)pbySrc;
    
    /* Copy 4X long words at a time if possible. */
    while ( f_ulSize >= 4 * sizeof(UINT32) )
    {
        *f_pulAlignedDst++ = *f_pulAlignedSrc++;
        *f_pulAlignedDst++ = *f_pulAlignedSrc++;
        *f_pulAlignedDst++ = *f_pulAlignedSrc++;
        *f_pulAlignedDst++ = *f_pulAlignedSrc++;
        f_ulSize -= 4 * sizeof(UINT32);
    } 
    
    /* Copy one long word at a time if possible. */
    while ( f_ulSize >= sizeof(UINT32) )
    {
        *f_pulAlignedDst++ = *f_pulAlignedSrc++;
        f_ulSize -= sizeof(UINT32);
    }
    
    /* Pick up any residual with a byte copier. */
    pbyDst = (CHAR *)f_pulAlignedDst;
    pbySrc = (const CHAR *)f_pulAlignedSrc;
    while ( f_ulSize-- )
        *pbyDst++ = *pbySrc++;
    
    return f_pvDestination;
}
#endif

/**************************************** llm_list section **********************************************/

