/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_debug_inst.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all defines, macros, and structures pertaining to the file
	oct6100_debug.c.  All elements defined in this file are for public
	usage of the API.  All private elements are defined in the
	oct6100_debug_priv.h file.

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

$Octasic_Revision: 10 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_DEBUG_INST_H__
#define __OCT6100_DEBUG_INST_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/


/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_API_DEBUG_
{
	/* Information from the TLVs. */
	UINT32	ulDebugEventSize;
	UINT32	ulMatrixBaseAddress;
	UINT32	ulDebugChanStatsByteSize;
	UINT32	ulDebugChanLiteStatsByteSize;
	UINT32	ulHotChannelSelectBaseAddress;
	UINT32	ulMatrixTimestampBaseAddress;
	UINT32	ulAfWritePtrByteOffset;
	UINT32	ulRecordedPcmEventByteSize;
	UINT32	ulMatrixWpBaseAddress;

	/* Pouch counter presence in the image. */
	UINT8	fPouchCounter;

	/* Record channel indexes. */
	UINT16	usRecordMemIndex;
	UINT16	usRecordChanIndex;

	UINT16	usRecordRinRoutTsiMemIndex;
	UINT16	usRecordSinSoutTsiMemIndex;
	
	/* Debug channel information.*/
	UINT16	usCurrentDebugChanIndex;

	/* Matrix event mask. */
	UINT16	usMatrixCBMask;

	/* If data is being dumped now. */
	UINT8	fDebugDataBeingDumped;

	/* Index of the last event retrieved. */
	UINT16	usLastDebugEventIndex;

	/* Number of events to retrieve. */
	UINT16	usNumEvents;

	/* Chip debug event write ptr. */
	UINT16	usChipDebugEventWritePtr;

	/* Hot channel read data. */
	UINT16	ausHotChannelData[ 2 ];

	/* Last PCM sample index. */
	UINT32	ulLastPcmSampleIndex;

	/* Last AF log read pointer. */
	UINT16	usLastAfLogReadPtr;

	/* AF log hardware write pointer. */
	UINT16	usAfLogWritePtr;

	/* Last tone event index retrieved. */
	UINT16	usLastToneEventIndex;

	/* Whether the image version string has been copied in the user buffer. */
	BOOL	fImageVersionCopied;

	/* Whether the api version string has been copied in the user buffer. */
	BOOL	fApiVersionCopied;

	/* Total number of bytes that will be returned for the current dump. */
	UINT32	ulDebugDataTotalNumBytes;

	/* Field to detect if the ISR is called present? */
	BOOL	fIsIsrCalledField;

	/* Remaining number of bytes that will be returned for the current dump. */
	UINT32	ulDebugDataRemainingNumBytes;

	/* AF events control block size. */
	UINT32	ulAfEventCbByteSize;

	/* Current user selected data mode.  Must be kept constant throughout a debug session. */
	UINT32	ulCurrentGetDataMode;

} tOCT6100_API_DEBUG, *tPOCT6100_API_DEBUG;

#endif /* __OCT6100_DEBUG_INST_H__ */
