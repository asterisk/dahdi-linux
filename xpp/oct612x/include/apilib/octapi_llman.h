/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  octapi_llman.h

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

$Octasic_Revision: 8 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#ifndef __OCTAPI_LLMAN_H__
#define __OCTAPI_LLMAN_H__

#include "octdef.h"

/* Error defines. */
#define OCTAPI_LLM_MEMORY_NOT_ALLOCATED			0xFFFFFFFF
#define OCTAPI_LLM_NO_STRUCTURES_LEFT			0xFFFFFFFE
#define OCTAPI_LLM_BLOCKNUM_OUT_OF_RANGE		0xFFFFFFFD
#define OCTAPI_LLM_ELEMENT_ALREADY_ASSIGNED		0xFFFFFFFC
#define OCTAPI_LLM_ELEMENT_NOT_FOUND			0xFFFFFFFB
#define OCTAPI_LLM_LIST_EMPTY					0xFFFFFFFA
#define OCTAPI_LLM_INVALID_LIST_HANDLE			0xFFFFFFF9
#define OCTAPI_LLM_TREE_NODE_ABSENT				0xFFFFFFF8
#define OCTAPI_LLM_INTERNAL_ERROR0				0xFFFFFFF7
#define OCTAPI_LLM_INTERNAL_ERROR1				0xFFFFFFF6
#define OCTAPI_LLM_INVALID_PARAMETER			0xFFFFFFF5

#define OCTAPI_LLM2_MEMORY_NOT_ALLOCATED		0xFEFFFFFF
#define OCTAPI_LLM2_NO_STRUCTURES_LEFT			0xFEFFFFFE
#define OCTAPI_LLM2_BLOCKNUM_OUT_OF_RANGE		0xFEFFFFFD
#define OCTAPI_LLM2_ELEMENT_ALREADY_ASSIGNED	0xFEFFFFFC
#define OCTAPI_LLM2_ELEMENT_NOT_FOUND			0xFEFFFFFB
#define OCTAPI_LLM2_LIST_EMPTY					0xFEFFFFFA
#define OCTAPI_LLM2_INVALID_LIST_HANDLE			0xFEFFFFF9
#define OCTAPI_LLM2_TREE_NODE_ABSENT			0xFEFFFFF8
#define OCTAPI_LLM2_INTERNAL_ERROR0				0xFEFFFFF7
#define OCTAPI_LLM2_INTERNAL_ERROR1				0xFEFFFFF6
#define OCTAPI_LLM2_INVALID_PARAMETER			0xFEFFFFF5

/* Other defines. */
#define OCTAPI_LLM_LIST_APPEND					0xFFFFFFFF
#define OCTAPI_LLM2_INSERT_ERROR				0xFFFFFFFF
#define OCTAPI_LLM2_INSERT_FIRST_NODE			0xFFFF0000
#define OCTAPI_LLM2_INSERT_LIST_NODE			0xFFFF0001
#define OCTAPI_LLM2_INSERT_LAST_NODE			0xFFFF0002
#define OCTAPI_LLM2_INSERT_BEFORE_LAST_NODE		0xFFFF0003
#define OCTAPI_LLM2_REMOVE_ERROR				0xFFFFFFFF
#define OCTAPI_LLM2_REMOVE_FIRST_NODE			0xFFFF0004
#define OCTAPI_LLM2_REMOVE_LIST_NODE			0xFFFF0005
#define OCTAPI_LLM2_REMOVE_LAST_NODE			0xFFFF0006
#define OCTAPI_LLM2_REMOVE_BEFORE_LAST_NODE		0xFFFF0007

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define octapi_llm_alloc_get_size( number_of_items, l_size )								OctapiLlmAllocGetSize( (UINT32) number_of_items,(PUINT32) l_size )
#define octapi_llm_alloc_init( l, number_of_items )											OctapiLlmAllocInit( (PVOID*) l,(UINT32) number_of_items )
#define octapi_llm_alloc_info( l, allocated_items, available_items )						OctapiLlmAllocInfo( (PVOID) l, (PUINT32) allocated_items, (PUINT32) available_items )
#define octapi_llm_alloc_alloc( l, blocknum )												OctapiLlmAllocAlloc( (PVOID) l, (PUINT32) blocknum )
#define octapi_llm_alloc_dealloc( l, blocknum )												OctapiLlmAllocDealloc( (PVOID) l,(UINT32) blocknum )
#define octapi_llm_list_get_size( number_of_items, number_of_lists, user_info_size, l_size ) OctApiLlmListGetSize( (UINT32) number_of_items,(UINT32) number_of_lists,(UINT32) user_info_size,(PUINT32) l_size )
#define octapi_llm_list_init( l, number_of_items, number_of_lists, user_info_size )			OctApiLlmListInit( (PVOID*) l,(UINT32) number_of_items,(UINT32) number_of_lists,(UINT32) user_info_size )
#define octapi_llm_list_info( l, allocated_lists, allocated_items, free_lists, free_items )	OctApiLlmListInfo( (PVOID) l,(PUINT32) allocated_lists,(PUINT32) allocated_items,(PUINT32) free_lists,(PUINT32) free_items )
#define octapi_llm_list_create( l, list_handle )											OctApiLlmListCreate( (PVOID) l,(PUINT32) list_handle )
#define octapi_llm_list_create_full( l, list_length, plist_handle )							OctApiLlmListCreateFull( (PVOID) l, (UINT32) list_length, (PUINT32) plist_handle )
#define octapi_llm_list_append_items( l,  list_handle,  num_items )							OctApiLlmListAppendItems( (PVOID) l, (UINT32) list_handle, (UINT32) num_items )
#define octapi_llm_list_append_and_set_items( l, list_handle, num_items, data_list )		OctApiLlmListAppendAndSetItems( (PVOID) l, (UINT32) list_handle, (UINT32) num_items, (PVOID) data_list )
#define octapi_llm_list_delete( l, list_handle )											OctApiLlmListDelete( (PVOID) l,(UINT32) list_handle )
#define octapi_llm_list_length( l, list_handle, number_of_items_in_list )					OctApiLlmListLength( (PVOID) l,(UINT32) list_handle, (PUINT32) number_of_items_in_list )
#define octapi_llm_list_insert_item( l, list_handle, item_number, item_data_pnt )			OctApiLlmListInsertItem( (PVOID) l,(UINT32) list_handle,(UINT32) item_number,(PVOID*) item_data_pnt )
#define octapi_llm_list_remove_item( l, list_handle, item_number )							OctApiLlmListRemoveItem( (PVOID) l,(UINT32) list_handle,(UINT32) item_number )
#define octapi_llm_list_item_data( l, list_handle, item_number, item_data_pnt )				OctApiLlmListItemData( (PVOID) l,(UINT32) list_handle,(UINT32) item_number,(PVOID*) item_data_pnt )
#define octapi_llm_list_copy_data( l, list_handle, start_item, data_length, pdata_list )	OctApiLlmListCopyData( (PVOID) l, (UINT32) list_handle, (UINT32) start_item, (UINT32) data_length, (PVOID) pdata_list )
#define octapi_llm_list_set_items( l, list_handle, start_item, data_length, pdata_list )	OctApiLlmListSetItems( (PVOID) l, (UINT32) list_handle, (UINT32) start_item, (UINT32) data_length, (PVOID) pdata_list )

/* Alloc man. */
UINT32 OctapiLlmAllocGetSize( UINT32 number_of_items,PUINT32 l_size );
UINT32 OctapiLlmAllocInit( PVOID* l,UINT32 number_of_items );
UINT32 OctapiLlmAllocInfo( PVOID l, PUINT32 allocated_items, PUINT32 available_items );
UINT32 OctapiLlmAllocAlloc( PVOID l, PUINT32 blocknum );
UINT32 OctapiLlmAllocDealloc( PVOID l,UINT32 blocknum );

/* Time managed alloc man. */
UINT32 OctApiTllmAllocGetSize( UINT32 number_of_items, PUINT32 l_size );
UINT32 OctApiTllmAllocInit( PVOID* l, UINT32 number_of_items );
UINT32 OctApiTllmAllocInfo( PVOID l, PUINT32 allocated_items, PUINT32 available_items );
UINT32 OctApiTllmAllocAlloc( PVOID l, PUINT32 blocknum, UINT32 current_time[2] );
UINT32 OctApiTllmAllocDealloc( PVOID l, UINT32 blocknum, UINT32 timeout_value, UINT32 current_time[2] );

/* List man. */
UINT32 OctApiLlmListGetSize( UINT32 number_of_items, UINT32 number_of_lists, UINT32 user_info_size, PUINT32 l_size );
UINT32 OctApiLlmListInit( PVOID* l, UINT32 number_of_items, UINT32 number_of_lists, UINT32 user_info_size );
UINT32 OctApiLlmListInfo( PVOID l, PUINT32 allocated_lists, PUINT32 allocated_items, PUINT32 free_lists, PUINT32 free_items );
UINT32 OctApiLlmListCreate( PVOID l, PUINT32 list_handle );
UINT32 OctApiLlmListCreateFull( PVOID l, UINT32 list_length, UINT32* plist_handle );
UINT32 OctApiLlmListAppendItems( PVOID l, UINT32 list_handle, UINT32 num_items );
UINT32 OctApiLlmListAppendAndSetItems( PVOID l, UINT32 list_handle, UINT32 num_items, PVOID data_list );
UINT32 OctApiLlmListDelete( PVOID l, UINT32 list_handle );
UINT32 OctApiLlmListLength( PVOID l, UINT32 list_handle, PUINT32 number_of_items_in_list );
UINT32 OctApiLlmListInsertItem( PVOID l, UINT32 list_handle, UINT32 item_number, PVOID* item_data_pnt );
UINT32 OctApiLlmListRemoveItem( PVOID l, UINT32 list_handle, UINT32 item_number );
UINT32 OctApiLlmListItemData( PVOID l, UINT32 list_handle, UINT32 item_number, PVOID* item_data_pnt );
UINT32 OctApiLlmListCopyData( PVOID l, UINT32 list_handle, UINT32 start_item, UINT32 data_length, PVOID pdata_list );
UINT32 OctApiLlmListSetItems( PVOID l, UINT32 list_handle, UINT32 start_item, UINT32 data_length, PVOID pdata_list );

/* Second list manager using a key to order info in the list. */
UINT32 OctApiLlm2ListGetSize( UINT32 number_of_items, UINT32 number_of_lists, UINT32 user_info_size, PUINT32 l_size );
UINT32 OctApiLlm2ListInit( PVOID* l,UINT32 number_of_items, UINT32 number_of_lists, UINT32 user_info_size );
UINT32 OctApiLlm2ListCreate( PVOID l, PUINT32 list_handle );
UINT32 OctApiLlm2ListLength( PVOID l, UINT32 list_handle, PUINT32 number_of_items_in_list );
UINT32 OctApiLlm2ListInsertItem(void * l, UINT32 list_handle, UINT32 item_key, void ** item_data_pnt, void ** prev_item_data_pnt, void ** prev_prev_item_data_pnt, PUINT32 insert_status_pnt );
UINT32 OctApiLlm2ListRemoveItem(void * l, UINT32 list_handle, UINT32 item_key, PUINT32 prev_item_key_pnt, PUINT32 prev_prev_item_key_pnt, PUINT32 remove_status_pnt );
UINT32 OctApiLlm2ListItemData( PVOID l, UINT32 list_handle, UINT32 item_key, PVOID* item_data_pnt, PUINT32 item_number );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __OCTAPI_LLMAN_H__ */
