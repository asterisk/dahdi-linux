/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_interrupts_priv.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all private defines, macros, structures and prototypes 
	pertaining to the file oct6100_interrupts.c.  All elements defined in this 
	file are for private usage of the API.  All public elements are defined 
	in the oct6100_interrupts_pub.h file.

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

#ifndef __OCT6100_INTERRUPTS_PRIV_H__
#define __OCT6100_INTERRUPTS_PRIV_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/

#define mOCT6100_GET_INTRPT_ENABLE_TIME(	\
					ulRegMclkTimeHigh,		\
					ulRegMclkTimeLow,		\
					ulIntrptState,			\
					ulIntrptEnableMclkHigh,	\
					ulIntrptEnableMclkLow,	\
					ulIntrptTimeoutMclk,	\
					ulTimeDiff )			\
	if ( ulIntrptState == cOCT6100_INTRPT_WILL_TIMEOUT )									\
	{																						\
		ulIntrptEnableMclkLow = ulRegMclkTimeLow + ulIntrptTimeoutMclk;						\
		if ( ulIntrptEnableMclkLow < ulRegMclkTimeLow )										\
			ulIntrptEnableMclkHigh = (ulRegMclkTimeHigh + 1) & 0xFF;						\
		else																				\
			ulIntrptEnableMclkHigh = ulRegMclkTimeHigh;										\
																							\
		ulIntrptState = cOCT6100_INTRPT_IN_TIMEOUT;											\
	}																						\
																							\
	if ( ulIntrptEnableMclkLow < ulRegMclkTimeLow )											\
	{																						\
		ulTimeDiff = (cOCT6100_FFFFFFFF - ulRegMclkTimeLow - 1) + ulIntrptEnableMclkLow;	\
	}																						\
	else																					\
	{																						\
		ulTimeDiff = ulIntrptEnableMclkLow - ulRegMclkTimeLow;								\
	}

#define mOCT6100_CHECK_INTRPT_TIMEOUT(				\
					ulRegMclkTimePlus5MsHigh,		\
					ulRegMclkTimePlus5MsLow,		\
					ulIntrptDisableMclkHigh,		\
					ulIntrptDisableMclkLow,			\
					ulIntrptEnableMclkHigh,			\
					ulIntrptEnableMclkLow,			\
					ulIntrptState,					\
					fIntrptChange )					\
	/* Branch depending on whether the disable time is lesser or greater than the timeout time. */							\
	if ( ulIntrptDisableMclkLow < ulIntrptEnableMclkLow )																	\
	{																														\
		/* Disable period is over if mclk is greater than timeout time or less than disabled time. */						\
		if ( ulRegMclkTimePlus5MsLow > ulIntrptEnableMclkLow ||																\
			 ulRegMclkTimePlus5MsLow < ulIntrptDisableMclkLow ||															\
			 ulRegMclkTimePlus5MsHigh != ulIntrptEnableMclkHigh )															\
		{																													\
			fIntrptChange = TRUE;																							\
			ulIntrptState = cOCT6100_INTRPT_ACTIVE;																			\
		}																													\
	}																														\
	else																													\
	{																														\
		/* Disable period is over if mclk is lesser than disable time and greater than timeout. */							\
		if ( (ulRegMclkTimePlus5MsLow > ulIntrptEnableMclkLow && ulRegMclkTimePlus5MsLow < ulIntrptDisableMclkLow) ||		\
			 (ulRegMclkTimePlus5MsHigh != ulIntrptDisableMclkHigh && ulRegMclkTimePlus5MsHigh != ulIntrptEnableMclkHigh) )	\
		{																													\
			fIntrptChange = TRUE;																							\
			ulIntrptState = cOCT6100_INTRPT_ACTIVE;																			\
		}																													\
	}

/*****************************  TYPES  ***************************************/


/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100ApiIsrSwInit(
			IN tPOCT6100_INSTANCE_API				f_pApiInstance );
			
UINT32 Oct6100ApiIsrHwInit(
			IN	tPOCT6100_INSTANCE_API				f_pApiInstance,
			IN	tPOCT6100_INTERRUPT_CONFIGURE		f_pIntrptConfig );

UINT32 Oct6100InterruptConfigureSer(
			IN	tPOCT6100_INSTANCE_API				f_pApiInstance,
			IN	tPOCT6100_INTERRUPT_CONFIGURE		f_pIntrptConfig,
			IN	BOOL								f_fCheckParams );

UINT32 Oct6100ApiClearEnabledInterrupts(
			IN	tPOCT6100_INSTANCE_API				f_pApiInstance ); 

UINT32 Oct6100InterruptServiceRoutineSer(
			IN tPOCT6100_INSTANCE_API				f_pApiInstance,
			OUT tPOCT6100_INTERRUPT_FLAGS			f_pIntFlags );

UINT32 Oct6100ApiWriteIeRegs(
				IN	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiReadIntrptRegs(
				IN	tPOCT6100_INSTANCE_API			f_pApiInstance,
				OUT	tPOCT6100_INTERRUPT_FLAGS		f_pIntFlags,
				IN	UINT32							f_ulRegister210h );

UINT32 Oct6100ApiUpdateIntrptStates(
				IN	tPOCT6100_INSTANCE_API			f_pApiInstance,
				OUT	tPOCT6100_INTERRUPT_FLAGS		f_pIntFlags );

UINT32 Oct6100ApiWriteIntrptRegs(
				IN	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiReadChipMclkTime(
				IN	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiUpdateIntrptTimeouts(
				IN	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32 Oct6100ApiScheduleNextMclkIntrpt(
				IN	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN	UINT32							f_ulIntrptToSet );

UINT32 Oct6100ApiScheduleNextMclkIntrptSer(
				IN	tPOCT6100_INSTANCE_API			f_pApiInstance );

UINT32	Oct6100ApiCheckProcessorState(
	  			IN	tPOCT6100_INSTANCE_API			f_pApiInstance,
				IN	tPOCT6100_INTERRUPT_FLAGS		f_pIntFlags );

#endif /* __OCT6100_INTERRUPTS_PRIV_H__ */
