/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  octapi_bt0.c

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

$Octasic_Revision: 18 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#include "apilib/octapi_bt0.h"
#include "octapi_bt0_private.h"



#if !SKIP_OctApiBt0GetSize
UINT32 OctApiBt0GetSize(UINT32 number_of_items,UINT32 key_size, UINT32 data_size, UINT32 * b_size)
{
	if ((key_size % 4) != 0) return(OCTAPI_BT0_KEY_SIZE_NOT_MUTLIPLE_OF_UINT32);
	if ((data_size % 4) != 0) return(OCTAPI_BT0_DATA_SIZE_NOT_MUTLIPLE_OF_UINT32);

	*b_size = 0;
	*b_size += sizeof(OCTAPI_BT0);
	*b_size += sizeof(OCTAPI_BT0_NODE) * number_of_items;
	*b_size += key_size * number_of_items;
	*b_size += data_size * number_of_items;

	return(GENERIC_OK);
}
#endif

#if !SKIP_OctApiBt0Init
UINT32 OctApiBt0Init(void ** b,UINT32 number_of_items,UINT32 key_size, UINT32 data_size)
{
	UINT32 i;
	OCTAPI_BT0 * bb;

	/* Check input parameters.*/
	if ((key_size % 4) != 0) return(OCTAPI_BT0_KEY_SIZE_NOT_MUTLIPLE_OF_UINT32);
	if ((data_size % 4) != 0) return(OCTAPI_BT0_DATA_SIZE_NOT_MUTLIPLE_OF_UINT32);

	/* If b is not already allocated.*/
	if (*b == NULL) return(OCTAPI_BT0_MALLOC_FAILED);

	bb = (OCTAPI_BT0 *)(*b);

	/* Initialize the tree to an empty one!*/
	bb->root_link.node_number = 0xFFFFFFFF;
	bb->root_link.depth = 0;

	/* Initialize tree parameters.*/
	bb->number_of_items = number_of_items;
	bb->key_size = key_size / 4;
	bb->data_size = data_size / 4;

	/* Initialize the next free node pointer.*/
	if (number_of_items != 0)
		bb->next_free_node = 0;
	else
		bb->next_free_node = 0xFFFFFFFF;

	/* Setup the arrays.*/
	OctApiBt0CorrectPointers(bb);

	/* Initialize the Nodes to unused!*/
	for(i=0;i<number_of_items;i++)
	{
		bb->node[i].next_free_node = i + 1;
	}

	/* Last empty node points to invalid node.*/
	bb->node[number_of_items-1].next_free_node = 0xFFFFFFFF;

	bb->invalid_value = 0xFFFFFFFF;
	bb->no_smaller_key = OCTAPI_BT0_NO_SMALLER_KEY;

	return(GENERIC_OK);
}
#endif


#if !SKIP_OctApiBt0CorrectPointers
void OctApiBt0CorrectPointers(OCTAPI_BT0 * bb)
{
	bb->node = (OCTAPI_BT0_NODE *)(((BYTE *)bb) + sizeof(OCTAPI_BT0));
	bb->key = (UINT32 *)(((BYTE *)bb->node) + (sizeof(OCTAPI_BT0_NODE) * bb->number_of_items));
	bb->data = (UINT32 *)(((BYTE *)bb->key) + (sizeof(UINT32) * bb->number_of_items * bb->key_size));
}
#endif


#if !SKIP_OctApiBt0AddNode
UINT32 OctApiBt0AddNode(void * b,void * key,void ** data)
{
	OCTAPI_BT0 * bb;
	OCTAPI_BT0_NODE * new_node;
	UINT32 * lkey;
	UINT32 * nkey;
	UINT32 i;
	UINT32 new_node_number;
	UINT32 result;

	/* Load all!*/
	bb = (OCTAPI_BT0 *)(b);
	OctApiBt0CorrectPointers(bb);

	/* Check that there is at least one block left.*/
	if (bb->next_free_node == 0xFFFFFFFF) return(OCTAPI_BT0_NO_NODES_AVAILABLE);

	/* Seize the node!*/
	new_node_number = bb->next_free_node;
	new_node = &(bb->node[new_node_number]);
	bb->next_free_node = new_node->next_free_node;

	/* Register in the key and the data.*/
	lkey = ((UINT32 *)key);

	/* Find the first UINT32 of the key.*/
	nkey = &(bb->key[bb->key_size * new_node_number]);

	/* Copy the key.*/
	for(i=0;i<bb->key_size;i++)
		nkey[i] = lkey[i];

	/* Attempt to place the node. Only a "multiple hit" will cause an error.*/
	result = OctApiBt0AddNode2(bb,&(bb->root_link), lkey, new_node_number);
	if (result != GENERIC_OK) 
	{
		/* This attempt failed. Refree the node!*/
		bb->next_free_node = new_node_number;

		/* Return the error code.*/
		return(result);
	}

	/* Return the address of the data to the user.*/
	if ( bb->data_size > 0 )
		*data = (void *)(&(bb->data[bb->data_size * new_node_number]));

	return(GENERIC_OK);
}
#endif

#if !SKIP_OctApiBt0AddNode2
UINT32 OctApiBt0AddNode2(OCTAPI_BT0 * bb,OCTAPI_BT0_LINK * link,UINT32 * lkey,UINT32 new_node_number)
{
	UINT32 result;

	if (link->node_number == 0xFFFFFFFF) /* We have an empty node. Here, we shall place the new node.*/
	{
		bb->node[new_node_number].l[0].node_number = 0xFFFFFFFF;
		bb->node[new_node_number].l[0].depth = 0;
		bb->node[new_node_number].l[1].node_number = 0xFFFFFFFF;
		bb->node[new_node_number].l[1].depth = 0;

		/* OCTAPI_BT0_LINK to parent!*/
		link->node_number = new_node_number;
		link->depth = 1;				/* We are a leaf, last OCTAPI_BT0_LINK depth is 1.*/

		return(GENERIC_OK);
	}
	else /* Current node is used, check for a match and a direction.*/
	{
		OCTAPI_BT0_NODE * this_node;
		UINT32 compare;

		/* Get a pointer to this node.*/
		this_node = &(bb->node[link->node_number]);

		/* Compare this node to the lkey.*/
		compare = OctApiBt0KeyCompare(bb,link,lkey);

		if (compare == OCTAPI_BT0_LKEY_SMALLER) /* Go left.*/
		{
			result = OctApiBt0AddNode2(bb,&(this_node->l[0]), lkey, new_node_number);
			if (result != GENERIC_OK) return(result);
		}
		else if (compare == OCTAPI_BT0_LKEY_LARGER) /* Go right.*/
		{
			result = OctApiBt0AddNode2(bb,&(this_node->l[1]), lkey, new_node_number);
			if (result != GENERIC_OK) return(result);
		}
		else
		{
			return(OCTAPI_BT0_KEY_ALREADY_IN_TREE);
		}

		/* Check if this node is unbalanced by 2. If so, rebalance it:*/
		if (this_node->l[0].depth > (this_node->l[1].depth + 1) || 
				this_node->l[1].depth > (this_node->l[0].depth + 1))
		{
			OctApiBt0Rebalance(bb,link);
		}

		/* Always update the OCTAPI_BT0_LINK depth before exiting.*/
		OctApiBt0UpdateLinkDepth(bb,link);

		return(GENERIC_OK);
	}
}
#endif


#if !SKIP_OctApiBt0AddNode3
UINT32 OctApiBt0AddNode3(OCTAPI_BT0 * bb,OCTAPI_BT0_LINK * link,UINT32 * lkey,UINT32 *p_new_node_number)
{
	UINT32 result;

	if (link->node_number == 0xFFFFFFFF) /* We have an empty node. Here, we shall place the new node.*/
	{
		if ( *p_new_node_number == 0xFFFFFFFF )
			return(OCTAPI_BT0_NO_NODES_AVAILABLE);

		bb->node[*p_new_node_number].l[0].node_number = 0xFFFFFFFF;
		bb->node[*p_new_node_number].l[0].depth = 0;
		bb->node[*p_new_node_number].l[1].node_number = 0xFFFFFFFF;
		bb->node[*p_new_node_number].l[1].depth = 0;

		/* OCTAPI_BT0_LINK to parent!*/
		link->node_number = *p_new_node_number;
		link->depth = 1;				/* We are a leaf, last OCTAPI_BT0_LINK depth is 1.*/

		return(GENERIC_OK);
	}
	else /* Current node is used, check for a match and a direction.*/
	{
		OCTAPI_BT0_NODE * this_node;
		UINT32 compare;

		/* Get a pointer to this node.*/
		this_node = &(bb->node[link->node_number]);

		/* Compare this node to the lkey.*/
		compare = OctApiBt0KeyCompare(bb,link,lkey);

		if (compare == OCTAPI_BT0_LKEY_SMALLER) /* Go left.*/
		{
			result = OctApiBt0AddNode3(bb,&(this_node->l[0]), lkey, p_new_node_number);
			if (result != GENERIC_OK) return(result);
		}
		else if (compare == OCTAPI_BT0_LKEY_LARGER) /* Go right.*/
		{
			result = OctApiBt0AddNode3(bb,&(this_node->l[1]), lkey, p_new_node_number);
			if (result != GENERIC_OK) return(result);
		}
		else
		{
			*p_new_node_number = link->node_number;
			return(OCTAPI_BT0_KEY_ALREADY_IN_TREE);
		}

		/* Check if this node is unbalanced by 2. If so, rebalance it:*/
		if (this_node->l[0].depth > (this_node->l[1].depth + 1) || 
				this_node->l[1].depth > (this_node->l[0].depth + 1))
		{
			OctApiBt0Rebalance(bb,link);
		}

		/* Always update the OCTAPI_BT0_LINK depth before exiting.*/
		OctApiBt0UpdateLinkDepth(bb,link);

		return(GENERIC_OK);
	}
}
#endif

/* state 
0 -> first call to the function.
1 -> recursive call.*/
#if !SKIP_OctApiBt0AddNode4
UINT32 OctApiBt0AddNode4(OCTAPI_BT0 * bb,OCTAPI_BT0_LINK * link,UINT32 * lkey,UINT32 *p_new_node_number, UINT32 *p_prev_node_number, UINT32 state )
{
	UINT32 result;
	UINT32 *nkey;
	UINT32 *okey;

	if (link->node_number == 0xFFFFFFFF) /* We have an empty node. Here, we shall place the new node.*/
	{
		bb->node[*p_new_node_number].l[0].node_number = 0xFFFFFFFF;
		bb->node[*p_new_node_number].l[0].depth = 0;
		bb->node[*p_new_node_number].l[1].node_number = 0xFFFFFFFF;
		bb->node[*p_new_node_number].l[1].depth = 0;

		/* OCTAPI_BT0_LINK to parent!*/
		link->node_number = *p_new_node_number;
		link->depth = 1;				/* We are a leaf, last OCTAPI_BT0_LINK depth is 1.*/

		if ( state == 0 )
			*p_prev_node_number = 0xFFFFFFFF;

		return(GENERIC_OK);
	}
	else /* Current node is used, check for a match and a direction.*/
	{
		OCTAPI_BT0_NODE * this_node;
		UINT32 compare;

		/* Get a pointer to this node.*/
		this_node = &(bb->node[link->node_number]);

		/* Compare this node to the lkey.*/
		compare = OctApiBt0KeyCompare(bb,link,lkey);

		if (compare == OCTAPI_BT0_LKEY_SMALLER) /* Go left.*/
		{
			if ( state == 0 )
				*p_prev_node_number = OCTAPI_BT0_NO_SMALLER_KEY;
			
			if ( *p_prev_node_number != OCTAPI_BT0_NO_SMALLER_KEY )
			{
				/* Check if the key is the smallest one encountered yet.*/
				okey = &(bb->key[bb->key_size * (*p_prev_node_number)]);
				nkey = &(bb->key[bb->key_size * link->node_number]);
				/* If the node is key smaller then the old small one, change the value.*/
				if ( *nkey > *okey )
				{
					if ( *nkey < *lkey	)
						*p_prev_node_number = link->node_number;
				}
			}

			result = OctApiBt0AddNode4(bb,&(this_node->l[0]), lkey, p_new_node_number, p_prev_node_number, 1);
			if (result != GENERIC_OK) return(result);
		}
		else if (compare == OCTAPI_BT0_LKEY_LARGER) /* Go right.*/
		{
			if ( state == 0 )
				*p_prev_node_number = link->node_number;
			else
			{
				if ( *p_prev_node_number  == OCTAPI_BT0_NO_SMALLER_KEY )
					*p_prev_node_number = link->node_number;
				else
				{
					/* Check if the key is the smallest one encountered yet.*/
					okey = &(bb->key[bb->key_size * (*p_prev_node_number)]);
					nkey = &(bb->key[bb->key_size * link->node_number]);
					/* If the node is key smaller then the old small one, change the value.*/
					if ( *nkey > *okey )
					{
						if ( *nkey < *lkey	)
							*p_prev_node_number = link->node_number;
					}
				}
			}

			result = OctApiBt0AddNode4(bb,&(this_node->l[1]), lkey, p_new_node_number, p_prev_node_number, 1);
			if (result != GENERIC_OK) return(result);
		}
		else
		{
			*p_new_node_number = link->node_number;
			return(OCTAPI_BT0_KEY_ALREADY_IN_TREE);
		}

		/* Check if this node is unbalanced by 2. If so, rebalance it:*/
		if (this_node->l[0].depth > (this_node->l[1].depth + 1) || 
				this_node->l[1].depth > (this_node->l[0].depth + 1))
		{
			OctApiBt0Rebalance(bb,link);
		}

		/* Always update the OCTAPI_BT0_LINK depth before exiting.*/
		OctApiBt0UpdateLinkDepth(bb,link);

		return(GENERIC_OK);
	}
}
#endif

#if !SKIP_OctApiBt0KeyCompare
UINT32 OctApiBt0KeyCompare(OCTAPI_BT0 * bb,OCTAPI_BT0_LINK * link, UINT32 * lkey)
{
	UINT32 * nkey;
	UINT32 i;

	/* Find the first UINT32 of the key.*/
	nkey = &(bb->key[bb->key_size * link->node_number]);

	for(i=0;i<bb->key_size;i++)
	{
		if (lkey[i] < nkey[i])
			return(OCTAPI_BT0_LKEY_SMALLER);
		else if (lkey[i] > nkey[i])
			return(OCTAPI_BT0_LKEY_LARGER);
	}	
	
	return(OCTAPI_BT0_LKEY_EQUAL);
}
#endif


#if !SKIP_OctApiBt0UpdateLinkDepth
void OctApiBt0UpdateLinkDepth(OCTAPI_BT0 * bb,OCTAPI_BT0_LINK * link)
{
	OCTAPI_BT0_NODE * this_node;

	/* Get a pointer to this node.*/
	this_node = &(bb->node[link->node_number]);

	if (this_node->l[0].depth > this_node->l[1].depth)
		link->depth = this_node->l[0].depth + 1;
	else
		link->depth = this_node->l[1].depth + 1;
}
#endif

#if !SKIP_OctApiBt0Rebalance
void OctApiBt0Rebalance(OCTAPI_BT0 * bb,OCTAPI_BT0_LINK * root_link)
{
	if (bb->node[root_link->node_number].l[0].depth > (bb->node[root_link->node_number].l[1].depth + 1))	/* Heavy to the left.*/
	{
		/* Check if the right child of the heavy child node is causing a problem.*/
		/* If so, do a left rotate in order to make the left most child the longer one.*/
		{
			OCTAPI_BT0_LINK * heavy_link;
			heavy_link = &(bb->node[root_link->node_number].l[0]);

			if (bb->node[heavy_link->node_number].l[1].depth > bb->node[heavy_link->node_number].l[0].depth)
			{
				OctApiBt0ExternalHeavy(bb,heavy_link);
			}
		}

		/* Ready to do super rotation!*/
		{
			OCTAPI_BT0_LINK init_root_link;
			OCTAPI_BT0_LINK init_heavy_link;
			OCTAPI_BT0_LINK init_leaf_tree[3];

			/* Save pertinent initial OCTAPI_BT0_LINK information.*/
			init_root_link = *root_link;
			init_heavy_link = bb->node[root_link->node_number].l[0];
			init_leaf_tree[2] = bb->node[root_link->node_number].l[1];
			init_leaf_tree[0] = bb->node[bb->node[root_link->node_number].l[0].node_number].l[0];
			init_leaf_tree[1] = bb->node[bb->node[root_link->node_number].l[0].node_number].l[1];

			/* Restructure the tree.*/
			*root_link = init_heavy_link;
			bb->node[init_heavy_link.node_number].l[1] = init_root_link;
			bb->node[init_root_link.node_number].l[0] = init_leaf_tree[1];

			/* Reconstruct the depth of the branches.*/
			OctApiBt0UpdateLinkDepth(bb,&(bb->node[root_link->node_number].l[1]));
			OctApiBt0UpdateLinkDepth(bb,root_link);
		}
	}
	else if (bb->node[root_link->node_number].l[1].depth > (bb->node[root_link->node_number].l[0].depth + 1))	/* Heavy to the right.*/
	{
		/* Check if the right child of the heavy child node is causing a problem.*/
		/* If so, do a left rotate in order to make the left most child the longer one.*/
		{
			OCTAPI_BT0_LINK * heavy_link;
			heavy_link = &(bb->node[root_link->node_number].l[1]);

			if (bb->node[heavy_link->node_number].l[0].depth > bb->node[heavy_link->node_number].l[1].depth)
			{
				OctApiBt0ExternalHeavy(bb,heavy_link);
			}
		}

		/* Ready to do super rotation!*/
		{
			OCTAPI_BT0_LINK init_root_link;
			OCTAPI_BT0_LINK init_heavy_link;
			OCTAPI_BT0_LINK init_leaf_tree[3];

			/* Save pertinent initial OCTAPI_BT0_LINK information.*/
			init_root_link = *root_link;
			init_heavy_link = bb->node[root_link->node_number].l[1];
			init_leaf_tree[2] = bb->node[root_link->node_number].l[0];
			init_leaf_tree[0] = bb->node[bb->node[root_link->node_number].l[1].node_number].l[1];
			init_leaf_tree[1] = bb->node[bb->node[root_link->node_number].l[1].node_number].l[0];

			/* Restructure the tree.*/
			*root_link = init_heavy_link;
			bb->node[init_heavy_link.node_number].l[0] = init_root_link;
			bb->node[init_root_link.node_number].l[1] = init_leaf_tree[1];

			/* Reconstruct the depth of the branches.*/
			OctApiBt0UpdateLinkDepth(bb,&(bb->node[root_link->node_number].l[0]));
			OctApiBt0UpdateLinkDepth(bb,root_link);
		}
	}
}
#endif

/* This function does a rotation towards the outside of the tree*/
/* in order to keep the heavy branches towards the outside.*/
#if !SKIP_OctApiBt0ExternalHeavy
void OctApiBt0ExternalHeavy(OCTAPI_BT0 * bb,OCTAPI_BT0_LINK * root_link)
{
	if (bb->node[root_link->node_number].l[1].depth > bb->node[root_link->node_number].l[0].depth)	/* Exterior of tree is towards the left.*/
	{
		OCTAPI_BT0_LINK init_root_link;
		OCTAPI_BT0_LINK init_heavy_link;
		OCTAPI_BT0_LINK init_leaf_tree[3];

		/* Save pertinent initial OCTAPI_BT0_LINK information.*/
		init_root_link = *root_link;
		init_leaf_tree[0] = bb->node[root_link->node_number].l[0];
		init_heavy_link = bb->node[root_link->node_number].l[1];
		init_leaf_tree[1] = bb->node[bb->node[root_link->node_number].l[1].node_number].l[0];
		init_leaf_tree[2] = bb->node[bb->node[root_link->node_number].l[1].node_number].l[1];

		/* Restructure the tree.*/
		*root_link = init_heavy_link;
		bb->node[init_heavy_link.node_number].l[0] = init_root_link;
		bb->node[init_root_link.node_number].l[1] = init_leaf_tree[1];

		/* Reconstruct the depth of the branches.*/
		OctApiBt0UpdateLinkDepth(bb,&(bb->node[root_link->node_number].l[0]));
		OctApiBt0UpdateLinkDepth(bb,root_link);
	}
	else if (bb->node[root_link->node_number].l[0].depth > bb->node[root_link->node_number].l[1].depth)	/* Exterior of tree is towards the right.*/
	{
		OCTAPI_BT0_LINK init_root_link;
		OCTAPI_BT0_LINK init_heavy_link;
		OCTAPI_BT0_LINK init_leaf_tree[3];

		/* Save pertinent initial OCTAPI_BT0_LINK information.*/
		init_root_link = *root_link;
		init_leaf_tree[0] = bb->node[root_link->node_number].l[1];
		init_heavy_link = bb->node[root_link->node_number].l[0];
		init_leaf_tree[1] = bb->node[bb->node[root_link->node_number].l[0].node_number].l[1];
		init_leaf_tree[2] = bb->node[bb->node[root_link->node_number].l[0].node_number].l[0];

		/* Restructure the tree.*/
		*root_link = init_heavy_link;
		bb->node[init_heavy_link.node_number].l[1] = init_root_link;
		bb->node[init_root_link.node_number].l[0] = init_leaf_tree[1];

		/* Reconstruct the depth of the branches.*/
		OctApiBt0UpdateLinkDepth(bb,&(bb->node[root_link->node_number].l[1]));
		OctApiBt0UpdateLinkDepth(bb,root_link);
	}
}
#endif


/* State:*/
/* 0 = seeking node to be removed.*/
/* 1 = node found, left branch taken.*/
/* 2 = node found, right branch taken.*/
#if !SKIP_OctApiBt0RemoveNode2
UINT32 OctApiBt0RemoveNode2(OCTAPI_BT0 * bb,OCTAPI_BT0_LINK * link, UINT32 * lkey, OCTAPI_BT0_LINK * link_to_removed_node, UINT32 state, OCTAPI_BT0_LINK * volatile_grandparent_link)
{
	UINT32 result;
	OCTAPI_BT0_NODE * this_node;

	/* Get a pointer to this node.*/
	this_node = &(bb->node[link->node_number]);

	if (state == 0)
	{
		if (link->node_number == 0xFFFFFFFF) /* We have an empty node. The node we were looking for does not exist.*/
		{
			return(OCTAPI_BT0_KEY_NOT_IN_TREE);
		}
		else /* Current node is used, check for a match and a direction.*/
		{
			UINT32 compare;

			/* Compare this node to the lkey.*/
			compare = OctApiBt0KeyCompare(bb,link,lkey);

			if (compare == OCTAPI_BT0_LKEY_SMALLER) /* Go left.*/
			{
				result = OctApiBt0RemoveNode2(bb,&(bb->node[link->node_number].l[0]), lkey, link_to_removed_node, 0, NULL);
				if (result != GENERIC_OK) return(result);
			}
			else if (compare == OCTAPI_BT0_LKEY_LARGER) /* Go right.*/
			{
				result = OctApiBt0RemoveNode2(bb,&(bb->node[link->node_number].l[1]), lkey, link_to_removed_node, 0, NULL);
				if (result != GENERIC_OK) return(result);
			}
			else
			{
				link_to_removed_node = link;

				/* Keep on going down to find a replacement node.*/
				if (bb->node[link->node_number].l[0].node_number == 0xFFFFFFFF && bb->node[link->node_number].l[1].node_number == 0xFFFFFFFF)
				{
					/* Doe! No tree left! WHAT TO DO?          */
					/* Just delete the current node. That's it.*/

					/* Release the current node (restore free node link-list)*/
					bb->node[link->node_number].next_free_node = bb->next_free_node;
					bb->next_free_node = link->node_number;

					link->node_number = 0xFFFFFFFF;
					link->depth = 0;

					return(GENERIC_OK);
				}
				else if (bb->node[link->node_number].l[0].node_number != 0xFFFFFFFF)	/* Left node is present. Go left, then permanently right.*/
				{
					OCTAPI_BT0_NODE * removed_node_pnt;
					removed_node_pnt = &(bb->node[link->node_number]);

					result = OctApiBt0RemoveNode2(bb,&(removed_node_pnt->l[0]), lkey, link_to_removed_node, 1, link);
					if (result != GENERIC_OK) return(result);

					/* Caution! Once we are here, the link->node_pnt->l[0] has been modified,*/
					/* but is about to be discarded! Save it quickly!*/
					/* bb->node[link->node_number].l[0] = removed_node_pnt->l[0];*/
				}
				else /* Right node is present. Go right, then permanently left.*/
				{
					OCTAPI_BT0_NODE * removed_node_pnt;
					removed_node_pnt = &(bb->node[link->node_number]);

					result = OctApiBt0RemoveNode2(bb,&(removed_node_pnt->l[1]), lkey, link_to_removed_node, 2, link);
					if (result != GENERIC_OK) return(result);

					/* Caution! Once we are here, the link->node_pnt->l[0] has been modified,*/
					/* but is about to be discarded! Save it quickly!*/
					/* bb->node[link->node_number].l[1] = removed_node_pnt->l[1];*/
				}
			}
		}
	}
	else
	{
		/* Left side, Right-most node found! OR*/
		/* Right side, Left-most node found!*/
		if ((state == 1 && bb->node[link->node_number].l[1].node_number == 0xFFFFFFFF) ||
			(state == 2 && bb->node[link->node_number].l[0].node_number == 0xFFFFFFFF))
		{
			OCTAPI_BT0_LINK init_chosen_link;

			/* Release the current node (restore free node link-list)*/
			bb->node[link_to_removed_node->node_number].next_free_node = bb->next_free_node;
			bb->next_free_node = link_to_removed_node->node_number;

			/* Save the link to the chosen node, because it is about to be deleted.*/
			init_chosen_link = *link;

			/* Remove this node, and allow the tree to go on:*/
			{
				OCTAPI_BT0_LINK init_child_link[2];

				init_child_link[0] = bb->node[link->node_number].l[0];
				init_child_link[1] = bb->node[link->node_number].l[1];

				if (state == 1)
					*link = init_child_link[0];
				else
					*link = init_child_link[1];
			}

			/* Replace the removed node by this node.*/
			{
				OCTAPI_BT0_LINK init_removed_child_link[2];

				init_removed_child_link[0] = bb->node[link_to_removed_node->node_number].l[0];
				init_removed_child_link[1] = bb->node[link_to_removed_node->node_number].l[1];

				*link_to_removed_node = init_chosen_link;
				bb->node[init_chosen_link.node_number].l[0] = init_removed_child_link[0];
				bb->node[init_chosen_link.node_number].l[1] = init_removed_child_link[1];
			}

			return(GENERIC_OK);
		}
		else
		{
			/* Keep on going, we have not found the center most node yet!*/
			if (state == 1)
			{
				result = OctApiBt0RemoveNode2(bb,&(bb->node[link->node_number].l[1]), lkey, link_to_removed_node, state, NULL);
				if (result != GENERIC_OK) return(result);

				/* Refresh the link if our link is volatile.*/
				if (volatile_grandparent_link != NULL)
				{
					link = &(bb->node[volatile_grandparent_link->node_number].l[0]);
				}
			}
			else
			{
				result = OctApiBt0RemoveNode2(bb,&(bb->node[link->node_number].l[0]), lkey, link_to_removed_node, state, NULL);
				if (result != GENERIC_OK) return(result);

				/* Refresh the link if our link is volatile.*/
				if (volatile_grandparent_link != NULL)
				{
					link = &(bb->node[volatile_grandparent_link->node_number].l[1]);
				}
			}
		}
	}

	/* We may have messed up the tree. So patch it!*/
	/* Check if this node is unbalanced by 2. If so, rebalance it:*/
	if (this_node->l[0].depth > (this_node->l[1].depth + 1) || 
			this_node->l[1].depth > (this_node->l[0].depth + 1))
	{
		OctApiBt0Rebalance(bb,link);
	}

	/* Always update the OCTAPI_BT0_LINK depth before exiting.*/
	OctApiBt0UpdateLinkDepth(bb,link);

	return(GENERIC_OK);
}
#endif


/* State:*/
/* 0 = seeking node to be removed.*/
/* 1 = node found, left branch taken.*/
/* 2 = node found, right branch taken.*/
#if !SKIP_OctApiBt0RemoveNode3
UINT32 OctApiBt0RemoveNode3(OCTAPI_BT0 * bb,OCTAPI_BT0_LINK * link, UINT32 * lkey, OCTAPI_BT0_LINK * link_to_removed_node, UINT32 state, OCTAPI_BT0_LINK * volatile_grandparent_link, UINT32 *p_prev_node_number )
{
	UINT32 result;
	UINT32 *nkey;
	UINT32 *okey;
	OCTAPI_BT0_NODE * this_node;

	/* Get a pointer to this node.*/
	this_node = &(bb->node[link->node_number]);

	if (state == 0)
	{
		if (link->node_number == 0xFFFFFFFF) /* We have an empty node. The node we were looking for does not exist.*/
		{
			return(OCTAPI_BT0_KEY_NOT_IN_TREE);
		}
		else /* Current node is used, check for a match and a direction.*/
		{
			UINT32 compare;

			/* Compare this node to the lkey.*/
			compare = OctApiBt0KeyCompare(bb,link,lkey);

			if (compare == OCTAPI_BT0_LKEY_SMALLER) /* Go left.*/
			{
				/* Check if the key is the biggest one encountered yet.*/
				okey = &(bb->key[bb->key_size * (*p_prev_node_number)]);
				nkey = &(bb->key[bb->key_size * link->node_number]);
				/* If the node is key bigger then the old one, change the value.*/
				if ( *nkey > *okey )
				{
					if ( *nkey < *lkey	)
						*p_prev_node_number = link->node_number;
				}
	
				result = OctApiBt0RemoveNode2(bb,&(bb->node[link->node_number].l[0]), lkey, link_to_removed_node, 0, NULL);
				if (result != GENERIC_OK) return(result);
			}
			else if (compare == OCTAPI_BT0_LKEY_LARGER) /* Go right.*/
			{
				/* Check if the key is the biggest one encountered yet.*/
				okey = &(bb->key[bb->key_size * (*p_prev_node_number)]);
				nkey = &(bb->key[bb->key_size * link->node_number]);
				/* If the node is key bigger then the old one, change the value.*/
				if ( *nkey > *okey )
				{
					if ( *nkey < *lkey	)
						*p_prev_node_number = link->node_number;
				}

				result = OctApiBt0RemoveNode2(bb,&(bb->node[link->node_number].l[1]), lkey, link_to_removed_node, 0, NULL);
				if (result != GENERIC_OK) return(result);
			}
			else
			{
				link_to_removed_node = link;

				/* Keep on going down to find a replacement node.*/
				if (bb->node[link->node_number].l[0].node_number == 0xFFFFFFFF && bb->node[link->node_number].l[1].node_number == 0xFFFFFFFF)
				{
					/* Doe! No tree left! WHAT TO DO?          */
					/* Just delete the current node. That's it.*/

					/* Release the current node (restore free node link-list)*/
					bb->node[link->node_number].next_free_node = bb->next_free_node;
					bb->next_free_node = link->node_number;

					link->node_number = 0xFFFFFFFF;
					link->depth = 0;

					return(GENERIC_OK);
				}
				else if (bb->node[link->node_number].l[0].node_number != 0xFFFFFFFF)	/* Left node is present. Go left, then permanently right.*/
				{
					OCTAPI_BT0_NODE * removed_node_pnt;
					removed_node_pnt = &(bb->node[link->node_number]);

					result = OctApiBt0RemoveNode2(bb,&(removed_node_pnt->l[0]), lkey, link_to_removed_node, 1, link);
					if (result != GENERIC_OK) return(result);

					/* Caution! Once we are here, the link->node_pnt->l[0] has been modified,*/
					/* but is about to be discarded! Save it quickly!*/
					/* bb->node[link->node_number].l[0] = removed_node_pnt->l[0];*/
				}
				else /* Right node is present. Go right, then permanently left.*/
				{
					OCTAPI_BT0_NODE * removed_node_pnt;
					removed_node_pnt = &(bb->node[link->node_number]);

					result = OctApiBt0RemoveNode2(bb,&(removed_node_pnt->l[1]), lkey, link_to_removed_node, 2, link);
					if (result != GENERIC_OK) return(result);

					/* Caution! Once we are here, the link->node_pnt->l[0] has been modified,*/
					/* but is about to be discarded! Save it quickly!*/
					/* bb->node[link->node_number].l[1] = removed_node_pnt->l[1];*/
				}
			}
		}
	}
	else
	{		
		/* Check if the key is the biggest one encountered yet.*/
		okey = &(bb->key[bb->key_size * (*p_prev_node_number)]);
		nkey = &(bb->key[bb->key_size * link->node_number]);
		/* If the node is key bigger then the old one, change the value.*/
		if ( *nkey > *okey )
		{
			if ( *nkey < *lkey	)
				*p_prev_node_number = link->node_number;
		}
		
		/* Left side, Right-most node found! OR*/
		/* Right side, Left-most node found!*/
		if ((state == 1 && bb->node[link->node_number].l[1].node_number == 0xFFFFFFFF) ||
			(state == 2 && bb->node[link->node_number].l[0].node_number == 0xFFFFFFFF))
		{
			OCTAPI_BT0_LINK init_chosen_link;

			/* Release the current node (restore free node link-list)*/
			bb->node[link_to_removed_node->node_number].next_free_node = bb->next_free_node;
			bb->next_free_node = link_to_removed_node->node_number;

			/* Save the link to the chosen node, because it is about to be deleted.*/
			init_chosen_link = *link;

			/* Remove this node, and allow the tree to go on:*/
			{
				OCTAPI_BT0_LINK init_child_link[2];

				init_child_link[0] = bb->node[link->node_number].l[0];
				init_child_link[1] = bb->node[link->node_number].l[1];

				if (state == 1)
					*link = init_child_link[0];
				else
					*link = init_child_link[1];
			}

			/* Replace the removed node by this node.*/
			{
				OCTAPI_BT0_LINK init_removed_child_link[2];

				init_removed_child_link[0] = bb->node[link_to_removed_node->node_number].l[0];
				init_removed_child_link[1] = bb->node[link_to_removed_node->node_number].l[1];

				*link_to_removed_node = init_chosen_link;
				bb->node[init_chosen_link.node_number].l[0] = init_removed_child_link[0];
				bb->node[init_chosen_link.node_number].l[1] = init_removed_child_link[1];
			}

			return(GENERIC_OK);
		}
		else
		{
			/* Keep on going, we have not found the center most node yet!*/
			if (state == 1)
			{
				result = OctApiBt0RemoveNode2(bb,&(bb->node[link->node_number].l[1]), lkey, link_to_removed_node, state, NULL);
				if (result != GENERIC_OK) return(result);

				/* Refresh the link if our link is volatile.*/
				if (volatile_grandparent_link != NULL)
				{
					link = &(bb->node[volatile_grandparent_link->node_number].l[0]);
				}
			}
			else
			{
				result = OctApiBt0RemoveNode2(bb,&(bb->node[link->node_number].l[0]), lkey, link_to_removed_node, state, NULL);
				if (result != GENERIC_OK) return(result);

				/* Refresh the link if our link is volatile.*/
				if (volatile_grandparent_link != NULL)
				{
					link = &(bb->node[volatile_grandparent_link->node_number].l[1]);
				}
			}
		}
	}

	/* We may have messed up the tree. So patch it!*/
	/* Check if this node is unbalanced by 2. If so, rebalance it:*/
	if (this_node->l[0].depth > (this_node->l[1].depth + 1) || 
			this_node->l[1].depth > (this_node->l[0].depth + 1))
	{
		OctApiBt0Rebalance(bb,link);
	}

	/* Always update the OCTAPI_BT0_LINK depth before exiting.*/
	OctApiBt0UpdateLinkDepth(bb,link);

	return(GENERIC_OK);
}
#endif

#if !SKIP_OctApiBt0RemoveNode
UINT32 OctApiBt0RemoveNode(void * b,void * key)
{
	OCTAPI_BT0 * bb;
	UINT32 result;
	UINT32 * lkey;

	/* Load all!*/
	bb = (OCTAPI_BT0 *)(b);
	OctApiBt0CorrectPointers(bb);

	/* Register in the key and the data.*/
	lkey = ((UINT32 *)key);

	/* Attempt to remove the node. Only a "no hit" will cause an error.*/
	result = OctApiBt0RemoveNode2(bb,&(bb->root_link), lkey, NULL, 0, NULL);
	if (result != GENERIC_OK) return(result);

	return(GENERIC_OK);
}
#endif

#if !SKIP_OctApiBt0QueryNode2
UINT32 OctApiBt0QueryNode2(OCTAPI_BT0 * bb,OCTAPI_BT0_LINK * link,UINT32 * lkey,UINT32 * node_number)
{
	UINT32 result;

	if (link->node_number == 0xFFFFFFFF) /* We have an empty node. The node we were looking for does not exist.*/
	{
		return(OCTAPI_BT0_KEY_NOT_IN_TREE);
	}
	else /* Current node is used, check for a match and a direction.*/
	{
		UINT32 compare;

		/* Compare this node to the lkey.*/
		compare = OctApiBt0KeyCompare(bb,link,lkey);

		if (compare == OCTAPI_BT0_LKEY_SMALLER) /* Go left.*/
		{
			result = OctApiBt0QueryNode2(bb,&(bb->node[link->node_number].l[0]), lkey, node_number);
			if (result != GENERIC_OK) return(result);
		}
		else if (compare == OCTAPI_BT0_LKEY_LARGER) /* Go right.*/
		{
			result = OctApiBt0QueryNode2(bb,&(bb->node[link->node_number].l[1]), lkey, node_number);
			if (result != GENERIC_OK) return(result);
		}
		else
		{
			/* A match!*/
			*node_number = link->node_number;
		}
	}

	return(GENERIC_OK);
}
#endif


#if !SKIP_OctApiBt0QueryNode
UINT32 OctApiBt0QueryNode(void * b,void * key,void ** data)
{
	OCTAPI_BT0 * bb;
	UINT32 node_number;
	UINT32 result;
	UINT32 * lkey;

	/* Load all!*/
	bb = (OCTAPI_BT0 *)(b);
	OctApiBt0CorrectPointers(bb);

	/* Register in the key and the data.*/
	lkey = ((UINT32 *)key);

	/* Get the node number.*/
	result = OctApiBt0QueryNode2(bb,&(bb->root_link),lkey,&node_number);
	if (result != GENERIC_OK) return(result);

	/* Return the address of the data to the user.*/
	if ( bb->data_size > 0 )
		*data = (void *)(&(bb->data[bb->data_size * node_number]));

	return(GENERIC_OK);
}
#endif

#if !SKIP_OctApiBt0GetFirstNode
UINT32 OctApiBt0GetFirstNode(void * b,void ** key, void ** data)
{
	OCTAPI_BT0 * bb;
	OCTAPI_BT0_NODE * node;
	UINT32 node_number;
	UINT32 * lkey;

	/* Load all!*/
	bb = (OCTAPI_BT0 *)(b);
	OctApiBt0CorrectPointers(bb);

	/* Register in the key and the data.*/
	lkey = ((UINT32 *)key);

	/* Check if there are any keys present in the tree. */
	if (bb->root_link.node_number == 0xFFFFFFFF) return OCTAPI_BT0_NO_NODES_AVAILABLE;

	node_number = bb->root_link.node_number;
	node = &bb->node[node_number];

	/* Make our way down to the left-most node. */
	while (node->l[0].node_number != 0xFFFFFFFF)
	{
		node_number = node->l[0].node_number;
		node = &bb->node[node_number];
	}

	/* Return the address of the data to the user.*/
	if ( bb->key_size > 0 )
		*key = (void *)(&(bb->key[bb->key_size * node_number]));

	if ( bb->data_size > 0 )
		*data = (void *)(&(bb->data[bb->data_size * node_number]));

	return(GENERIC_OK);
}
#endif

#if !SKIP_OctApiBt0FindOrAddNode
UINT32 OctApiBt0FindOrAddNode(void * b,void * key,void ** data, UINT32 *fnct_result)
{
	OCTAPI_BT0 * bb;
	OCTAPI_BT0_NODE * new_node;
	UINT32 * lkey;
	UINT32 * nkey;
	UINT32 i;
	UINT32 new_node_number;
	UINT32 temp_node_number = 0;
	UINT32 result;
	UINT32 tree_already_full = FALSE;
	
	/* Load all!*/
	bb = (OCTAPI_BT0 *)(b);
	OctApiBt0CorrectPointers(bb);
	
	/* Seize the node!*/
	new_node_number = bb->next_free_node;
	/* Register in the key and the data.*/
	lkey = ((UINT32 *)key);

	/* Check that there is at least one block left.*/
	if (bb->next_free_node != 0xFFFFFFFF) 
	{

		temp_node_number = new_node_number;
		new_node = &(bb->node[new_node_number]);
		bb->next_free_node = new_node->next_free_node;
		
		/* Find the first UINT32 of the key.*/
		nkey = &(bb->key[bb->key_size * new_node_number]);
	
		/* Copy the key.*/
		for(i=0;i<bb->key_size;i++)
			nkey[i] = lkey[i];
	}
	else
		tree_already_full = TRUE;	/* Signal that the tree was already full when the function was called.*/

	/* Attempt to place the node. Only a "multiple hit" will cause an error.*/
	result = OctApiBt0AddNode3(bb,&(bb->root_link), lkey, &new_node_number); 
	switch( result )
	{
	case GENERIC_OK:
		*fnct_result = OCTAPI0_BT0_NODE_ADDDED;
		break;
	case OCTAPI_BT0_KEY_ALREADY_IN_TREE:
		*fnct_result = OCTAPI0_BT0_NODE_FOUND;
		/* This attempt did not add a new node. Refree the node!*/
		if ( tree_already_full == FALSE )
			bb->next_free_node = temp_node_number;
		result = GENERIC_OK;
		break;
	default:
		break;
	}

	if (result != GENERIC_OK) 
	{
		/* This attempt failed. Refree the node!*/
		if ( tree_already_full == FALSE )
			bb->next_free_node = new_node_number;
		
		/* Return the error code.*/
		return(result);
	}
	
	/* Return the address of the data to the user.*/
	if ( bb->data_size > 0 )
		*data = (void *)(&(bb->data[bb->data_size * new_node_number]));
	
	return(GENERIC_OK);
}
#endif


#if !SKIP_OctApiBt0AddNodeReportPrevNodeData
UINT32 OctApiBt0AddNodeReportPrevNodeData(void * b,void * key,void ** data, void ** prev_data, PUINT32 fnct_result )
{
	OCTAPI_BT0 * bb;
	OCTAPI_BT0_NODE * new_node;
	UINT32 * lkey;
	UINT32 * nkey;
	UINT32 i;
	UINT32 new_node_number;
	UINT32 temp_node_number;
	UINT32 prev_node_number;
	UINT32 result;

	/* Load all!*/
	bb = (OCTAPI_BT0 *)(b);
	OctApiBt0CorrectPointers(bb);

	/* Check that there is at least one block left.*/
	if (bb->next_free_node == 0xFFFFFFFF) return(OCTAPI_BT0_NO_NODES_AVAILABLE);

	/* Seize the node!*/
	new_node_number = bb->next_free_node;
	temp_node_number = new_node_number;
	new_node = &(bb->node[new_node_number]);
	bb->next_free_node = new_node->next_free_node;

	/* Set the previous node value */
	prev_node_number = 0xFFFFFFFF;

	/* Register in the key and the data.*/
	lkey = ((UINT32 *)key);

	/* Find the first UINT32 of the key.*/
	nkey = &(bb->key[bb->key_size * new_node_number]);

	/* Copy the key.*/
	for(i=0;i<bb->key_size;i++)
		nkey[i] = lkey[i];

	/* Attempt to place the node. Only a "multiple hit" will cause an error.*/
	result = OctApiBt0AddNode4(bb,&(bb->root_link), lkey, &new_node_number, &prev_node_number, 0);
	switch( result )
	{
	case GENERIC_OK:
		*fnct_result = OCTAPI0_BT0_NODE_ADDDED;
		break;
	case OCTAPI_BT0_KEY_ALREADY_IN_TREE:
		*fnct_result = OCTAPI0_BT0_NODE_FOUND;
		/* This attempt did not add a new node. Refree the node!*/
		bb->next_free_node = temp_node_number;
		result = GENERIC_OK;
		break;
	default:
		break;
	}

	if (result != GENERIC_OK)
	{
		/* This attempt failed. Refree the node!*/
		bb->next_free_node = new_node_number;

		/* Return the error code.*/
		return(result);
	}

	/* Return the address of the data to the user.*/
	if ( bb->data_size > 0 )
		*data = (void *)(&(bb->data[bb->data_size * new_node_number]));

	if ( bb->data_size > 0 )
	{
		if ( (prev_node_number != 0xFFFFFFFF) &&
			 (prev_node_number != OCTAPI_BT0_NO_SMALLER_KEY) &&
			 (*fnct_result == OCTAPI0_BT0_NODE_ADDDED))
			*prev_data = ( void* )(&(bb->data[bb->data_size * prev_node_number]));
		else if ( prev_node_number == 0xFFFFFFFF )
			*prev_data = ( void* )(&bb->invalid_value);
		else if ( prev_node_number == OCTAPI_BT0_NO_SMALLER_KEY )
			*prev_data = ( void* )(&bb->no_smaller_key);
	}

	return(GENERIC_OK);
}
#endif

