/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_interrupts_inst.h

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

$Octasic_Revision: 16 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_INTERRUPTS_INST_H__
#define __OCT6100_INTERRUPTS_INST_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/


/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_API_INTRPT_CONFIG_
{
	/* The configuration of each group of interrupts.  Each can have one of the
		following values:
			cOCT6100_INTRPT_DISABLE,
			cOCT6100_INTRPT_NO_TIMEOUT,
			cOCT6100_INTRPT_TIMEOUT. */
	UINT8	byFatalGeneralConfig;
	UINT8	byFatalMemoryConfig;
	UINT8	byErrorMemoryConfig;
	UINT8	byErrorOverflowToneEventsConfig;
	UINT8	byErrorH100Config;

	/* The timeout value for each interrupt group, if the corresponding
		configuration variable is set to cOCT6100_INTRPT_TIMEOUT.  This
		value is kept in mclk cycles. */
	UINT32	ulFatalMemoryTimeoutMclk;
	UINT32	ulErrorMemoryTimeoutMclk;
	UINT32	ulErrorOverflowToneEventsTimeoutMclk;
	UINT32	ulErrorH100TimeoutMclk;

} tOCT6100_API_INTRPT_CONFIG, *tPOCT6100_API_INTRPT_CONFIG;

typedef struct _OCT6100_API_INTRPT_MANAGE_
{
	/* Number of mclk cycles in 1ms. */
	UINT32	ulNumMclkCyclesIn1Ms;

	/* Whether the mclk interrupt is active. */
	UINT8	fMclkIntrptActive;
	UINT32	ulNextMclkIntrptTimeHigh;
	UINT32	ulNextMclkIntrptTimeLow;

	/* Mclk time read from registers. */
	UINT32	ulRegMclkTimeHigh;
	UINT32	ulRegMclkTimeLow;

	/* Used by the interrupt service routine. */
	UINT16	usRegister102h;
	UINT16	usRegister202h;
	UINT16	usRegister302h;
	UINT16	usRegister502h;
	UINT16	usRegister702h;

	/* The state of each interrupt group.  Can be one of the following:
		cOCT6100_INTRPT_ACTIVE,
		cOCT6100_INTRPT_WILL_TIMEOUT,
		cOCT6100_INTRPT_IN_TIMEOUT,
		cOCT6100_INTRPT_WILL_DISABLED. */
	UINT16	byFatalGeneralState;
	UINT16	byFatalMemoryState;
	UINT16	byErrorMemoryState;
	UINT16	byErrorOverflowToneEventsState;
	UINT16	byErrorH100State;

	/* The time at which each disabled interrupt was disabled, in mclk cycles. */
	UINT32	ulFatalMemoryDisableMclkHigh;
	UINT32	ulFatalMemoryDisableMclkLow;
	UINT32	ulErrorMemoryDisableMclkHigh;
	UINT32	ulErrorMemoryDisableMclkLow;
	UINT32	ulErrorOverflowToneEventsDisableMclkHigh;
	UINT32	ulErrorOverflowToneEventsDisableMclkLow;
	UINT32	ulErrorH100DisableMclkHigh;
	UINT32	ulErrorH100DisableMclkLow;

	/* The time at which each disabled interrupt group is to be reenabled,
		in number of mclk cycles. */
	UINT32	ulFatalGeneralEnableMclkHigh;
	UINT32	ulFatalGeneralEnableMclkLow;
	UINT32	ulFatalMemoryEnableMclkHigh;
	UINT32	ulFatalMemoryEnableMclkLow;
	UINT32	ulErrorMemoryEnableMclkHigh;
	UINT32	ulErrorMemoryEnableMclkLow;
	UINT32	ulErrorOverflowToneEventsEnableMclkHigh;
	UINT32	ulErrorOverflowToneEventsEnableMclkLow;
	UINT32	ulErrorH100EnableMclkHigh;
	UINT32	ulErrorH100EnableMclkLow;
	
	/* If this is set, buffer playout events are pending. */
	UINT8	fBufferPlayoutEventsPending;
	/* If this is set, tone events are pending. */
	UINT8	fToneEventsPending;
	


	UINT8	fIsrCalled;

} tOCT6100_API_INTRPT_MANAGE, *tPOCT6100_API_INTRPT_MANAGE;

#endif /* __OCT6100_INTERRUPTS_INST_H__ */
