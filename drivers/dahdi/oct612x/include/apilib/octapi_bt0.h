/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  octapi_bt0.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	Library used to manage a binary tree of variable max size.  Library is
	made to use one block of contiguous memory to manage the tree.

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

$Octasic_Revision: 11 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#ifndef __OCTAPI_BT0_H__
#define __OCTAPI_BT0_H__

#include "octdef.h"

#define OCTAPI_BT0_BASE								0xFFFF0000
#define OCTAPI_BT0_KEY_SIZE_NOT_MUTLIPLE_OF_UINT32	OCTAPI_BT0_BASE+0x0001
#define OCTAPI_BT0_DATA_SIZE_NOT_MUTLIPLE_OF_UINT32	OCTAPI_BT0_BASE+0x0002
#define OCTAPI_BT0_MALLOC_FAILED					OCTAPI_BT0_BASE+0x0003
#define OCTAPI_BT0_NO_NODES_AVAILABLE				OCTAPI_BT0_BASE+0x0004
#define OCTAPI_BT0_KEY_ALREADY_IN_TREE				OCTAPI_BT0_BASE+0x0005
#define OCTAPI_BT0_KEY_NOT_IN_TREE					OCTAPI_BT0_BASE+0x0006

/* Possible result for Find Or Add function. */
#define OCTAPI0_BT0_NODE_FOUND		0
#define OCTAPI0_BT0_NODE_ADDDED		1

#define OCTAPI_BT0_NO_SMALLER_KEY	0xAAAAAAAA

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define octapi_bt0_get_size( number_of_items, key_size, data_size, b_size ) OctApiBt0GetSize( (UINT32) number_of_items,(UINT32) key_size, (UINT32) data_size, (PUINT32) b_size ) 
#define octapi_bt0_init( b, number_of_items, key_size, data_size )			OctApiBt0Init( (void **) b,(UINT32) number_of_items,(UINT32) key_size, (UINT32) data_size )
#define octapi_bt0_add_node( b, key, data )									OctApiBt0AddNode( (void *) b,(void *) key,(void **) data )
#define octapi_bt0_remove_node( b, key )									OctApiBt0RemoveNode( (void *) b,(void *) key )
#define octapi_bt0_query_node( b, key, data )								OctApiBt0QueryNode( (void *) b,(void *) key,(void **) data )
#define octapi_bt0_get_first_node( b, key, data )							OctApiBt0GetFirstNode( (void *) b,(void **) key, (void **) data )

UINT32 OctApiBt0GetSize( UINT32 number_of_items, UINT32 key_size, UINT32 data_size, UINT32 * b_size );
UINT32 OctApiBt0Init( void ** b, UINT32 number_of_items, UINT32 key_size, UINT32 data_size );
UINT32 OctApiBt0AddNode( void * b, void * key, void ** data );
UINT32 OctApiBt0RemoveNode( void * b, void * key );
UINT32 OctApiBt0QueryNode( void * b, void * key, void ** data );
UINT32 OctApiBt0GetFirstNode( void * b, void ** key, void ** data );
UINT32 OctApiBt0FindOrAddNode( void * b, void * key, void ** data, UINT32 *fnct_result );

UINT32 OctApiBt0AddNodeReportPrevNodeData( void * b, void * key, void ** data, void ** prev_data, UINT32 *fnct_result );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*__OCTAPI_BT0_H__*/
