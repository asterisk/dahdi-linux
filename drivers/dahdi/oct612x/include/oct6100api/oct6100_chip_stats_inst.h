/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_chip_stats_inst.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all defines, macros, and structures pertaining to the file
	oct6100_chip_stats.c.  All elements defined in this file are for public
	usage of the API.  All private elements are defined in the
	oct6100_chip_stats_priv.h file.

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

$Octasic_Revision: 21 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_CHIP_STATS_INST_H__
#define __OCT6100_CHIP_STATS_INST_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/


/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_API_CHIP_ERROR_STATS_
{
	UINT8	fFatalChipError;

	UINT32	ulInternalReadTimeoutCnt;
	UINT32	ulSdramRefreshTooLateCnt;
	UINT32	ulPllJitterErrorCnt;
	
	/* Internal tone detector error counter. */
	UINT32	ulToneDetectorErrorCnt;

	UINT32	ulOverflowToneEventsCnt;

	UINT32	ulH100OutOfSyncCnt;
	UINT32	ulH100ClkABadCnt;
	UINT32	ulH100ClkBBadCnt;
	UINT32	ulH100FrameABadCnt;
	

	
} tOCT6100_API_CHIP_ERROR_STATS, *tPOCT6100_API_CHIP_ERROR_STATS;

typedef struct _OCT6100_API_CHIP_STATS_
{
	UINT16	usNumberChannels;
	UINT16	usNumberBiDirChannels;
	UINT16	usNumberTsiCncts;
	UINT16	usNumberConfBridges;
	UINT16	usNumberPlayoutBuffers;
	UINT16	usNumEcChanUsingMixer;

	UINT32	ulPlayoutMemUsed;
	UINT16	usNumberActiveBufPlayoutPorts;

	UINT16	usNumberPhasingTssts;
	UINT16	usNumberAdpcmChans;
	
} tOCT6100_API_CHIP_STATS, *tPOCT6100_API_CHIP_STATS;

#endif /* __OCT6100_CHIP_STATS_INST_H__ */
