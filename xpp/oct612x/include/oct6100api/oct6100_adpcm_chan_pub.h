/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_adpcm_chan_pub.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	File containing all defines, macros, and structures pertaining to the file
	oct6100_adpcm_chan.c.  All elements defined in this file are for public
	usage of the API.  All private elements are defined in the
	oct6100_adpcm_chan_priv.h file.

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

$Octasic_Revision: 5 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCT6100_ADPCM_CHAN_PUB_H__
#define __OCT6100_ADPCM_CHAN_PUB_H__

/*****************************  INCLUDE FILES  *******************************/


/*****************************  DEFINES  *************************************/


/*****************************  TYPES  ***************************************/

typedef struct _OCT6100_ADPCM_CHAN_OPEN_
{
	PUINT32	pulChanHndl;

	UINT32	ulInputTimeslot;
	UINT32	ulInputStream;
	UINT32	ulInputNumTssts;
	UINT32	ulInputPcmLaw;

	UINT32	ulOutputTimeslot;
	UINT32	ulOutputStream;
	UINT32	ulOutputNumTssts;
	UINT32	ulOutputPcmLaw;

	UINT32	ulChanMode;			/* Encoding or decoding. */

	UINT32	ulEncodingRate;
	UINT32	ulDecodingRate;		

	UINT32	ulAdpcmNibblePosition;

} tOCT6100_ADPCM_CHAN_OPEN, *tPOCT6100_ADPCM_CHAN_OPEN;

typedef struct _OCT6100_ADPCM_CHAN_CLOSE_
{
	UINT32	ulChanHndl;

} tOCT6100_ADPCM_CHAN_CLOSE, *tPOCT6100_ADPCM_CHAN_CLOSE;


/************************** FUNCTION PROTOTYPES  *****************************/


UINT32 Oct6100AdpcmChanOpenDef(
				OUT		tPOCT6100_ADPCM_CHAN_OPEN				f_pAdpcmChanOpen );
UINT32 Oct6100AdpcmChanOpen(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_ADPCM_CHAN_OPEN				f_pAdpcmChanOpen );

UINT32 Oct6100AdpcmChanCloseDef(
				OUT		tPOCT6100_ADPCM_CHAN_CLOSE				f_pAdpcmChanClose );
UINT32 Oct6100AdpcmChanClose(
				IN OUT	tPOCT6100_INSTANCE_API					f_pApiInstance,
				IN OUT	tPOCT6100_ADPCM_CHAN_CLOSE				f_pAdpcmChanClose );

#endif /* __OCT6100_ADPCM_CHAN_PUB_H__ */
