/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:		octapi_bt0_private.h

Copyright (c) 2001 Octasic Inc. All rights reserved.
    
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
#ifndef __OCTAPI_BT0_PRIVATE_H__
#define __OCTAPI_BT0_PRIVATE_H__



#include "octdef.h"

#define OCTAPI_BT0_LKEY_LARGER	0x0
#define OCTAPI_BT0_LKEY_SMALLER	0x1
#define OCTAPI_BT0_LKEY_EQUAL	0x2

typedef struct __OCTAPI_BT0_LINK__
{
	UINT32 node_number;
	UINT32 depth;
} OCTAPI_BT0_LINK;

typedef struct __OCTAPI_BT0_NODE__
{
	UINT32 next_free_node;	/* Number of the next node in the free node link-list.*/
	OCTAPI_BT0_LINK l[2];	/* 0 = left link; 1 = right link.*/
} OCTAPI_BT0_NODE;


typedef struct __OCTAPI_BT0__ 
{
	UINT32 number_of_items;	/* Number of items on total that can be allocated in the tree.*/
	UINT32 key_size;			/* Size is in UINT32s*/
	UINT32 data_size;		/* Size is in UINT32s*/

	/* Empty node linked-list:*/
	UINT32 next_free_node;	/* 0xFFFFFFFF means that no nodes are free.*/

	/* Tree as such:*/
	OCTAPI_BT0_NODE * node;	/* Array of nodes (number_of_items in size).*/

	/* Tree root:*/
	OCTAPI_BT0_LINK root_link;

	/* Associated key structure*/
	UINT32 * key;			/* Array of keys associated to NODEs.*/

	/* Associated data structure.*/
	UINT32 * data;			/* Array of data associated to NODEs.*/

	UINT32 invalid_value;
	UINT32 no_smaller_key;

} OCTAPI_BT0;

void OctApiBt0CorrectPointers( OCTAPI_BT0 * bb );
UINT32 OctApiBt0AddNode2( OCTAPI_BT0 * bb, OCTAPI_BT0_LINK * link, UINT32 * lkey, UINT32 new_node_number );
UINT32 OctApiBt0AddNode3( OCTAPI_BT0 * bb, OCTAPI_BT0_LINK * link, UINT32 * lkey, UINT32 *p_new_node_number );
UINT32 OctApiBt0AddNode4(OCTAPI_BT0 * bb,OCTAPI_BT0_LINK * link,UINT32 * lkey,UINT32 *p_new_node_number, UINT32 *p_prev_node_number, UINT32 state );
UINT32 OctApiBt0KeyCompare( OCTAPI_BT0 * bb, OCTAPI_BT0_LINK * link, UINT32 * lkey );
void OctApiBt0UpdateLinkDepth( OCTAPI_BT0 * bb, OCTAPI_BT0_LINK * link );
void OctApiBt0Rebalance( OCTAPI_BT0 * bb, OCTAPI_BT0_LINK * root_link );
void OctApiBt0ExternalHeavy( OCTAPI_BT0 * bb, OCTAPI_BT0_LINK * root_link );
UINT32 OctApiBt0RemoveNode2( OCTAPI_BT0 * bb, OCTAPI_BT0_LINK * link, UINT32 * lkey, OCTAPI_BT0_LINK * link_to_removed_node, UINT32 state, OCTAPI_BT0_LINK * volatile_grandparent_link );



#endif /*__OCTAPI_BT0_PRIVATE_H__*/
