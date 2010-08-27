/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:		octapi_llman_private.h

Copyright (c) 2001 Octasic Inc. All rights reserved.
    
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

$Octasic_Revision: 13 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#ifndef __OCTAPI_LLMAN_PRIVATE_H__
#define __OCTAPI_LLMAN_PRIVATE_H__

#include "octdef.h"


/**************************************** llm_alloc section **********************************************/


/*	Most basic linked list model.
	LLM_STR contains a list of "number_of_items" that
	are each "unassigned" or "assigned". When requesting
	a new element, llm_alloc must choose an "unassigned"
	element. An element that is deallocated will be last
	to be allocated.
*/

typedef struct _LLM_ALLOC
{
	UINT32 *linked_list; 	/* Each item is either used (0xFFFFFFFE)*/
							/* or unused (pointer to next unused item, 0xFFFFFFFF means last item reached).*/
	UINT32 next_avail_num;	/* Points to the next available item in linked list. (0xFFFFFFFF means none available)*/
	UINT32 number_of_items;	/* Total number of items in linked list.*/
	UINT32 allocated_items;	/* Allocated items in linked list.*/

} LLM_ALLOC;

typedef struct _TLLM_ALLOC_NODE_
{
	UINT32 value; 		/* Each item is either used (0xFFFFFFFE)*/
						/* or unused (pointer to next unused item, 0xFFFFFFFF means last item reached).*/
	UINT32 timeout[2];	/* Timeout value that must be exceeded for the node to be considered free again.*/

} TLLM_ALLOC_NODE;


typedef struct _TLLM_ALLOC_
{
	TLLM_ALLOC_NODE *linked_list;	/* List of nodes used by the link list.*/

	UINT32 next_avail_num;	/* Points to the next available item in linked list. (0xFFFFFFFF means none available)*/
	UINT32 number_of_items;	/* Total number of items in linked list.*/
	UINT32 allocated_items;	/* Allocated items in linked list.*/

	UINT32 number_of_timeout;	/* Number of block currently in timeout.*/
	UINT32 next_timeout_num;	/* Points to the next block currently in timeout.*/
	UINT32 last_timeout_num;	/* Last node of the timeout list.*/

	UINT32 last_known_time[2];	/* last known time.*/

} TLLM_ALLOC;

/*
void octapi_llm_alloc_build_structure(void *l, LLM_ALLOC ** ls);
*/
/**************************************** llm_alloc section **********************************************/



/**************************************** llm_list section **********************************************/
/*	This section contains memory structures and functions used
	to maintain a variable number of lists (FIFOs) that each
	have a variable amount of items. A total amount of items
	can be assigned through-out all the lists. Each item in
	each list contains a UINT32 specified by the software using
	the lists. Each used item in the list is accessible through
	it's position in the list. */

typedef struct _LLM_LIST_HEAD
{
	UINT32 list_length;	/* Current number of items in the list.*/
						/* 0xFFFFFFFF means that the list is not used.*/
	UINT32 head_pointer;	/* Number of the item in the item pool that is the first of this list.*/
						/* 0xFFFFFFFF indicates end-of-list link.*/
	UINT32 tail_pointer;	/* Number of the item in the item pool that is the last of this list.*/

	/* Item cache (pointer within the list of the last accessed item):*/
	UINT32 cache_item_number;	/* Number of the last accessed item in the list. 0xFFFFFFFF indicates invalid cache.*/
	UINT32 cache_item_pointer;	/* Number of the last accessed item in the item pool.*/
} LLM_LIST_HEAD;

typedef struct _LLM_LIST_ITEM
{
	UINT32 forward_link;	/* Number of the item in the item pool that is next in this list.*/
						/* 0xFFFFFFFF indicates end-of-list link.*/

	/* User item info (variable size)*/
	UINT32 user_info[1];
} LLM_LIST_ITEM;

typedef struct _LLM_LIST
{
	UINT32 user_info_bytes;	/* In bytes, size of the user info in a single item.*/
	UINT32 user_info_size;	/* In bytes, size of the user info in a single item.*/
	UINT32 item_size;

	UINT32 head_alloc_size;
	UINT32 total_items;
	UINT32 assigned_items;

	UINT32 total_lists;
	UINT32 assigned_lists;

	UINT32 next_empty_item;	/* Contains a pointer to the next empty item in the*/
							/* item pool.*/

	/* Table of all the possible list heads:*/
	LLM_LIST_HEAD * lh;
	void * list_head_alloc;	/* LLM_ALLOC structure used for list head allocation!*/

	/* Table of the list items:*/
	LLM_LIST_ITEM * li;
} LLM_LIST;


/**********************************************************************************/
/* These structures are are used by the Llm2 functions to creates lists of ordered
   items based on a key given by the user when a new node is inserted in a list. */
typedef struct _LLM2_LIST_HEAD
{
	UINT32 list_length;	/* Current number of items in the list.*/
						/* 0xFFFFFFFF means that the list is not used.*/
	UINT32 head_pointer;	/* Number of the item in the item pool that is the first of this list.*/
						/* 0xFFFFFFFF indicates end-of-list link.*/
	UINT32 tail_pointer;	/* Number of the item in the item pool that is the last of this list.*/

} LLM2_LIST_HEAD;

typedef struct _LLM2_LIST_ITEM
{
	UINT32 forward_link;	/* Number of the item in the item pool that is next in this list.*/
						/* 0xFFFFFFFF indicates end-of-list link.*/
	UINT32 key;			/* Key used to order the entries.*/

	/* User item info (variable size)*/
	UINT32 user_info[1];
} LLM2_LIST_ITEM;

typedef struct _LLM2_LIST
{
	UINT32 user_info_bytes;	/* In bytes, size of the user info in a single item.*/
	UINT32 user_info_size;	/* In bytes, size of the user info in a single item.*/
	UINT32 item_size;

	UINT32 head_alloc_size;
	UINT32 total_items;
	UINT32 assigned_items;

	UINT32 total_lists;
	UINT32 assigned_lists;

	UINT32 next_empty_item;	/* Contains a pointer to the next empty item in the*/
							/* item pool.*/

	/* Table of all the possible list heads:*/
	LLM2_LIST_HEAD * lh;
	void * list_head_alloc;	/* LLM_ALLOC structure used for list head allocation!*/

	/* Table of the list items:*/
	LLM2_LIST_ITEM * li;
} LLM2_LIST;

/*void octapi_llm_list_build_structure(void *l, LLM_LIST ** ls);*/
LLM_LIST_ITEM * OctApiLlmListGetItemPointer( LLM_LIST * ls, UINT32 item_number );
LLM2_LIST_ITEM * OctApiLlm2ListGetItemPointer( LLM2_LIST * ls, UINT32 item_number );
UINT32	OctApiTllmCheckTimeoutList( TLLM_ALLOC *ls, UINT32 current_time[2] );
VOID * OctApiLlmMemCpy( VOID *f_pvDestination, const VOID * f_pvSource, UINT32 f_ulSize );
/**************************************** llm_list section **********************************************/





#endif /*__OCTAPI_LLMAN_PRIVATE_H__*/
