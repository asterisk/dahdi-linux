/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_tone_detection_priv.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all private defines, macros, structures and prototypes 
	pertaining to the file oct6100_tone_detection.c.  All elements defined in 
	this  file are for private usage of the API.  All public elements are 
	defined in the oct6100_tone_detection_pub.h file.
	
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

$Octasic_Revision: 14 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_TONE_DETECTION_PRIV_H__
#define __OCT6100_TONE_DETECTION_PRIV_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/


/*****************************  TYPES  ***************************************/


/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100ToneDetectionEnableSer(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_TONE_DETECTION_ENABLE			f_pToneDetectEnable );

UINT32 Oct6100ApiCheckToneEnableParams(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_TONE_DETECTION_ENABLE			f_pToneDetectEnable,
				OUT		PUINT32									f_pulChannelIndex,
				OUT		PUINT32									f_pulToneEventNumber,

				OUT		PUINT32									f_pulExtToneChanIndex );

UINT32 Oct6100ApiWriteToneDetectEvent(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT32									f_ulChannelIndex,
				IN		UINT32									f_ulToneEventNumber,

				IN		UINT32									f_ulExtToneChanIndex );

UINT32 Oct6100ApiUpdateChanToneDetectEntry (
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT32									f_ulChannelIndex,
				IN		UINT32									f_ulToneEventNumber,
				IN		UINT32									f_ulExtToneChanIndex );

UINT32 Oct6100ToneDetectionDisableSer(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_TONE_DETECTION_DISABLE		f_pToneDetectDisable );

UINT32 Oct6100ApiAssertToneDetectionParams(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		tPOCT6100_TONE_DETECTION_DISABLE		f_pToneDetectDisable,
				OUT		PUINT32									f_pulChannelIndex,
				OUT		PUINT32									f_pulToneEventNumber,
				OUT		PUINT32									f_pulExtToneChanIndex,

				OUT		PBOOL									f_pfDisableAll );

UINT32 Oct6100ApiClearToneDetectionEvent(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT32									f_ulChannelIndex,
				IN		UINT32									f_ulToneEventNumber,
				IN		UINT32									f_ulExtToneChanIndex,

				IN		BOOL									f_fDisableAll );

UINT32 Oct6100ApiReleaseToneDetectionEvent(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT32									f_ulChannelIndex,
				IN		UINT32									f_ulToneEventNumber,
				IN		UINT32									f_ulExtToneChanIndex,
				IN		BOOL									f_fDisableAll );

UINT32 Oct6100ApiIsSSTone(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT32									f_ulToneEventNumber,
				OUT		PBOOL									f_fSSTone );

UINT32 Oct6100ApiIs2100Tone(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN		UINT32									f_ulToneEventNumber,
				OUT		PBOOL									f_fIs2100Tone );

#endif /* __OCT6100_TONE_DETECTION_PRIV_H__ */
