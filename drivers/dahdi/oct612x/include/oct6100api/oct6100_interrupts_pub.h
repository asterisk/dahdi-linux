/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_interrupts_pub.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all defines, macros, and structures pertaining to the file
	oct6100_interrupts.c.  All elements defined in this file are for public
	usage of the API.  All private elements are defined in the
	oct6100_interrupts_priv.h file.

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

$Octasic_Revision: 23 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_INTERRUPTS_PUB_H__
#define __OCT6100_INTERRUPTS_PUB_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/


/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_INTERRUPT_CONFIGURE_
{
	UINT32	ulFatalGeneralConfig;
	UINT32	ulFatalMemoryConfig;

	UINT32	ulErrorMemoryConfig;
	UINT32	ulErrorOverflowToneEventsConfig;
	UINT32	ulErrorH100Config;

	UINT32	ulFatalMemoryTimeout;
	UINT32	ulErrorMemoryTimeout;
	UINT32	ulErrorOverflowToneEventsTimeout;
	UINT32	ulErrorH100Timeout;

} tOCT6100_INTERRUPT_CONFIGURE, *tPOCT6100_INTERRUPT_CONFIGURE;

typedef struct _OCT6100_INTERRUPT_FLAGS_
{
	BOOL	fFatalGeneral;
	UINT32	ulFatalGeneralFlags;

	BOOL	fFatalReadTimeout;
	
	BOOL	fErrorRefreshTooLate; 
	BOOL	fErrorPllJitter;

	BOOL	fErrorOverflowToneEvents;

	BOOL	fErrorH100OutOfSync;
	BOOL	fErrorH100ClkA;
	BOOL	fErrorH100ClkB;
	BOOL	fErrorH100FrameA;

	BOOL	fToneEventsPending;
	BOOL	fBufferPlayoutEventsPending;

	BOOL	fApiSynch;
	


} tOCT6100_INTERRUPT_FLAGS, *tPOCT6100_INTERRUPT_FLAGS;

/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100InterruptConfigureDef(
			OUT		tPOCT6100_INTERRUPT_CONFIGURE		f_pConfigInts );
UINT32 Oct6100InterruptConfigure(
			IN		tPOCT6100_INSTANCE_API				f_pApiInst,
			IN OUT	tPOCT6100_INTERRUPT_CONFIGURE		f_pConfigInts );

UINT32 Oct6100InterruptServiceRoutineDef(
			OUT		tPOCT6100_INTERRUPT_FLAGS			f_pIntFlags );
UINT32 Oct6100InterruptServiceRoutine(
			IN		tPOCT6100_INSTANCE_API				f_pApiInst,
			IN OUT	tPOCT6100_INTERRUPT_FLAGS			f_pIntFlags );

#endif /* __OCT6100_INTERRUPTS_PUB_H__ */

