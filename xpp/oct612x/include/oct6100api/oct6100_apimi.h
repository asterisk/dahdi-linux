/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_apimi.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	This file contains the declaration of all functions exported from the 
	APIMI block.  The APIMI block contains only one function:
		Oct6100InterruptMask.
	The function is used to mask out the interrupt pin of the chip.  This 
	function is used when a deferred procedure call treats the interrupt (new 
	interrupts must not be generated until the signalled interrupt is treated).

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

$Octasic_Revision: 6 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_APIMI_H__
#define __OCT6100_APIMI_H__

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/*****************************  INCLUDE FILES  *******************************/

#include "octdef.h"

/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_INTERRUPT_MASK_
{
	UINT32	ulUserChipIndex;
	PVOID	pProcessContext;


} tOCT6100_INTERRUPT_MASK, *tPOCT6100_INTERRUPT_MASK;

/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100InterruptMaskDef(
				OUT		tPOCT6100_INTERRUPT_MASK f_pInterruptMask );
UINT32 Oct6100InterruptMask(
				IN		tPOCT6100_INTERRUPT_MASK f_pInterruptMask );
 
#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __OCT6100_APIMI_H__ */
