/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_chip_stats_pub.h

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

$Octasic_Revision: 59 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_CHIP_STATS_PUB_H__
#define __OCT6100_CHIP_STATS_PUB_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/


/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_CHIP_STATS_
{
	BOOL	fResetChipStats;

	UINT32	ulNumberChannels;
	UINT32	ulNumberTsiCncts;
	UINT32	ulNumberConfBridges;
	UINT32	ulNumberPlayoutBuffers;
	UINT32	ulPlayoutFreeMemSize;
	UINT32	ulNumberPhasingTssts;
	UINT32	ulNumberAdpcmChannels;

	UINT32	ulH100OutOfSynchCount;
	UINT32	ulH100ClockABadCount;
	UINT32	ulH100FrameABadCount;
	UINT32	ulH100ClockBBadCount;

	UINT32	ulInternalReadTimeoutCount;
	UINT32	ulSdramRefreshTooLateCount;
	UINT32	ulPllJitterErrorCount;
	
	UINT32	ulOverflowToneEventsCount;
	UINT32	ulSoftOverflowToneEventsCount;

	UINT32	ulSoftOverflowBufferPlayoutEventsCount;

} tOCT6100_CHIP_STATS, *tPOCT6100_CHIP_STATS;

typedef struct _OCT6100_CHIP_TONE_INFO_
{
	UINT32	ulToneID;
	UINT32	ulDetectionPort;	

	UINT8	aszToneName[ cOCT6100_TLV_MAX_TONE_NAME_SIZE ];

} tOCT6100_CHIP_TONE_INFO, *tPOCT6100_CHIP_TONE_INFO;

typedef struct _OCT6100_CHIP_IMAGE_INFO_
{
	BOOL	fBufferPlayout;
	BOOL	fAdaptiveNoiseReduction;
	BOOL	fSoutNoiseBleaching;
	BOOL	fAutoLevelControl;
	BOOL	fHighLevelCompensation;
	BOOL	fSilenceSuppression;
	
	BOOL	fAdpcm;
	BOOL	fConferencing;
	BOOL	fConferencingNoiseReduction;
	BOOL	fDominantSpeaker;

	BOOL	fAcousticEcho;
	BOOL	fAecTailLength;
	BOOL	fToneRemoval;

	BOOL	fDefaultErl;
	BOOL	fNonLinearityBehaviorA;
	BOOL	fNonLinearityBehaviorB;	
	BOOL	fPerChannelTailDisplacement;
	BOOL	fPerChannelTailLength;
	BOOL	fListenerEnhancement;
	BOOL	fRoutNoiseReduction;
	BOOL	fRoutNoiseReductionLevel;
	BOOL	fAnrSnrEnhancement;
	BOOL	fAnrVoiceNoiseSegregation;
	BOOL	fToneDisablerVqeActivationDelay;
	BOOL	fMusicProtection;
	BOOL	fDoubleTalkBehavior;
	BOOL	fIdleCodeDetection;
	BOOL	fSinLevel;

	UINT32	ulMaxChannels;
	UINT32	ulNumTonesAvailable;
	UINT32	ulToneProfileNumber;
	UINT32	ulMaxTailDisplacement;
	UINT32	ulMaxTailLength;
	UINT32	ulDebugEventSize;
	UINT32	ulMaxPlayoutEvents;
	
	UINT32	ulImageType;
	
	UINT8	szVersionNumber[ cOCT6100_VERSION_NUMBER_MAX_SIZE ];
	UINT32	ulBuildId;

	tOCT6100_CHIP_TONE_INFO	aToneInfo[ cOCT6100_MAX_TONE_EVENT ];

} tOCT6100_CHIP_IMAGE_INFO, *tPOCT6100_CHIP_IMAGE_INFO;


/************************** FUNCTION PROTOTYPES  *****************************/

UINT32 Oct6100ChipGetStatsDef(
				OUT		tPOCT6100_CHIP_STATS			f_pChipStats );

UINT32 Oct6100ChipGetStats(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInst,
				IN OUT	tPOCT6100_CHIP_STATS			f_pChipStats );

UINT32 Oct6100ChipGetImageInfoDef(
				OUT		tPOCT6100_CHIP_IMAGE_INFO		f_pChipImageInfo );

UINT32 Oct6100ChipGetImageInfo(
				IN OUT	tPOCT6100_INSTANCE_API			f_pApiInst,
				OUT		tPOCT6100_CHIP_IMAGE_INFO		f_pChipImageInfo );

#endif /* __OCT6100_CHIP_STATS_PUB_H__ */
