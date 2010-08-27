/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_mask_interrupts.c

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	This file contains the mask interrupts function.

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


/*****************************  INCLUDE FILES  *******************************/

#include "oct6100api/oct6100_apimi.h"
#include "oct6100api/oct6100_apiud.h"
#include "oct6100api/oct6100_errors.h"
#include "oct6100api/oct6100_defines.h"


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100InterruptMask

Description:    The function is used to mask out the interrupt pin of the chip.  
				This function is used when a deferred procedure call treats the 
				interrupt (new interrupts must not be generated until the 
				signaled interrupt is treated).  Which chip is to have its 
				interrupts masked is determined by the mask structure, 
				f_pInterruptMask.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pInterruptMask		Pointer to the interrupt masking structure.

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
UINT32 Oct6100InterruptMaskDef(
				OUT		tPOCT6100_INTERRUPT_MASK f_pInterruptMask )
{
	f_pInterruptMask->ulUserChipIndex = cOCT6100_INVALID_VALUE;
	f_pInterruptMask->pProcessContext = NULL;


	return cOCT6100_ERR_OK;
}

UINT32 Oct6100InterruptMask(
				IN		tPOCT6100_INTERRUPT_MASK f_pInterruptMask )
{
	tOCT6100_WRITE_PARAMS	WriteParams;
	tOCT6100_READ_PARAMS	ReadParams;
	UINT32	result;
	UINT16	usReadData;

	/* Determine if the chip's interrupt pin is active.*/
	ReadParams.ulReadAddress = 0x210;
	ReadParams.pusReadData = &usReadData;
	ReadParams.pProcessContext = f_pInterruptMask->pProcessContext;

	ReadParams.ulUserChipId = f_pInterruptMask->ulUserChipIndex;
	
	result = Oct6100UserDriverReadOs( &ReadParams );
	if ( result != cOCT6100_ERR_OK ) 
		return cOCT6100_ERR_INTRPTS_RW_ERROR;

	if ( (usReadData & 0xFFFF) != 0 )
	{
		/* Chip's interrupt pin is active, so mask interrupt pin. */
		ReadParams.ulReadAddress = 0x214;
		result = Oct6100UserDriverReadOs( &ReadParams );
		if ( result != cOCT6100_ERR_OK ) 
			return cOCT6100_ERR_INTRPTS_RW_ERROR;

		/* Determine if the chip's interrupt pin is active. */
		WriteParams.pProcessContext = f_pInterruptMask->pProcessContext;

		WriteParams.ulUserChipId = f_pInterruptMask->ulUserChipIndex;
		WriteParams.ulWriteAddress = 0x214;
		WriteParams.usWriteData = (UINT16)( (usReadData & 0xC000) | 0x3FFF );
	
		result = Oct6100UserDriverWriteOs( &WriteParams );
		if ( result != cOCT6100_ERR_OK ) 
			return cOCT6100_ERR_INTRPTS_RW_ERROR;

		WriteParams.ulWriteAddress = 0x212;
		WriteParams.usWriteData = 0x8000;
	
		result = Oct6100UserDriverWriteOs( &WriteParams );
		if ( result != cOCT6100_ERR_OK ) 
			return cOCT6100_ERR_INTRPTS_RW_ERROR;
		
		return cOCT6100_ERR_OK;
	}
	
	return cOCT6100_ERR_INTRPTS_NOT_ACTIVE;
}
