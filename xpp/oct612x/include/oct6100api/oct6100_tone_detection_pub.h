/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_tone_detection_pub.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all defines, macros, and structures pertaining to the file
	oct6100_tone_detection.c.  All elements defined in this file are for public
	usage of the API.  All private elements are defined in the
	oct6100_tone_detection_priv.h file.

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

#ifndef __OCT6100_TONE_DETECTION_PUB_H__
#define __OCT6100_TONE_DETECTION_PUB_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/


/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_TONE_DETECTION_ENABLE_
{
	UINT32	ulChannelHndl;
	UINT32	ulToneNumber;

} tOCT6100_TONE_DETECTION_ENABLE, *tPOCT6100_TONE_DETECTION_ENABLE;

typedef struct _OCT6100_TONE_DETECTION_DISABLE_
{
	UINT32	ulChannelHndl;
	UINT32	ulToneNumber;
	BOOL	fDisableAll;

} tOCT6100_TONE_DETECTION_DISABLE, *tPOCT6100_TONE_DETECTION_DISABLE;

/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100ToneDetectionEnableDef(
				OUT		tPOCT6100_TONE_DETECTION_ENABLE				f_pBufferLoad );
UINT32 Oct6100ToneDetectionEnable(	
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInstance,
				IN OUT	tPOCT6100_TONE_DETECTION_ENABLE				f_pBufferLoad );

UINT32 Oct6100ToneDetectionDisableDef(
				OUT		tPOCT6100_TONE_DETECTION_DISABLE			f_pBufferUnload );
UINT32 Oct6100ToneDetectionDisable(
				IN OUT	tPOCT6100_INSTANCE_API						f_pApiInstance,
				IN OUT	tPOCT6100_TONE_DETECTION_DISABLE			f_pBufferUnload );

#endif /* __OCT6100_TONE_DETECTION_PUB_H__ */
