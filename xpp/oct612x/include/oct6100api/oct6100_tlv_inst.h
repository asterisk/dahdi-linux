/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_tlv_inst.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all defines, macros, and structures pertaining to the file
	oct6100_tlv.c.  All elements defined in this file are for public
	usage of the API.  All instate elements are defined in the
	oct6100_tlv_inst.h file.

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

$Octasic_Revision: 7 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_TLV_INST_H__
#define __OCT6100_TLV_INST_H__

/*****************************  INCLUDE FILES  *******************************/

/*****************************  DEFINES  *************************************/

/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_TLV_OFFSET_
{
	/* The dword offset contain the number of dword from a base address to reach the desired dword.
	
		i.e. usDwordOffset = (total bit offset) / 32; */

	UINT16	usDwordOffset;
	
	/* The bit offset will contain the bit offset required to right shift the DWORD read and obtain
	   the desired value. This field is depend on the field size.
	
		i.e. byBitOffset = 31 - ((total bit offset) % 32) - byFieldSize; */

	UINT8	byBitOffset;
	UINT8	byFieldSize;

} tOCT6100_TLV_OFFSET, *tPOCT6100_TLV_OFFSET;

typedef struct _OCT6100_TLV_TONE_INFO_
{
	UINT32	ulToneID;
	UINT32	ulDetectionPort;	

	UINT8	aszToneName[ cOCT6100_TLV_MAX_TONE_NAME_SIZE ];



} tOCT6100_TLV_TONE_INFO, *tPOCT6100_TLV_TONE_INFO;

#endif /* __OCT6100_TLV_INST_H__ */
