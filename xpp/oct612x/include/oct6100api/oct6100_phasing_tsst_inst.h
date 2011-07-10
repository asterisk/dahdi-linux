/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_phasing_tsst_inst.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all defines, macros, and structures pertaining to the file
	oct6100_phasing_tsst.c.  All elements defined in this file are for public
	usage of the API.  All private elements are defined in the
	oct6100_phasing_tsst_priv.h file.

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

#ifndef __OCT6100_PHASING_TSST_INST_H__
#define __OCT6100_PHASING_TSST_INST_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/


/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_API_PHASING_TSST_
{
	/* Flag specifying whether the entry is used or not. */
	UINT8	fReserved;
	
	/* Count used to manage entry handles allocated to user. */
	UINT8	byEntryOpenCnt;

	/* Count of number of resources connected in some way to this buffer. */
	UINT16	usDependencyCnt;

	/* TDM timeslot and stream where the counter is read. */
	UINT16	usStream;
	UINT16	usTimeslot;

	/* Length of the phasing TSST counter. */
	UINT16	usPhasingLength;

	/* TSST control index where the counter comes from. */
	UINT16	usPhasingTsstIndex;

} tOCT6100_API_PHASING_TSST, *tPOCT6100_API_PHASING_TSST;

#endif /* __OCT6100_PHASING_TSST_INST_H__ */
