/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: octmac.h

    Copyright (c) 2001-2007 Octasic Inc.

Description: 

	Common macro definitions.

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
#ifndef __OCTMAC_H__
#define __OCTMAC_H__

/*--------------------------------------------------------------------------
	C language
----------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

/*****************************  DEFINES  *************************************/

/* Combine l & h to form a 32 bit quantity. */
#define mMAKEULONG(l, h)  ((ULONG)(((USHORT)(l)) | (((ULONG)((USHORT)(h))) << 16)))

#define mLOUCHAR(w)     ((UCHAR)(w))
#define mHIUCHAR(w)     ((UCHAR)(((USHORT)(w) >> 8) & 0xff))
#define mLOUSHORT(l)    ((USHORT)((ULONG)l))
#define mHIUSHORT(l)    ((USHORT)(((ULONG)(l) >> 16) & 0xffff))
#define mLOSHORT(l)     ((SHORT)((ULONG)l))
#define mHISHORT(l)     ((SHORT)(((ULONG)(l) >> 16) & 0xffff))

/* Combine l & h to form a 16 bit quantity. */
#define mMAKEUSHORT(l, h) (((USHORT)(l)) | ((USHORT)(h)) << 8)
#define mMAKESHORT(l, h)  ((SHORT)mMAKEUSHORT(l, h))

/* Extract high and low order parts of 16 and 32 bit quantity */
#define mLOBYTE(w)       mLOUCHAR(w)
#define mHIBYTE(w)       mHIUCHAR(w)
#define mMAKELONG(l, h)   ((LONG)mMAKEULONG(l, h))

/*--------------------------------------------------------------------------
	Bite conversion macro
----------------------------------------------------------------------------*/
#define mSWAP_INT16(x) mMAKEUSHORT( mHIBYTE(x), mLOBYTE(x) )
#define mSWAP_INT32(x) mMAKEULONG( mSWAP_INT16(mHIUSHORT(x)), mSWAP_INT16(mLOUSHORT(x)) )


/* Cast any variable to an instance of the specified type. */
#define mMAKETYPE(v, type)   (*((type *)&v))

/* Calculate the byte offset of a field in a structure of type type. */
#define mFIELDOFFSET(type, field)    ((UINT32)&(((type *)0)->field))
#define mCOUNTOF(array) (sizeof(array)/sizeof(array[0]))

#define mMAX(a,b)	(((a) > (b)) ? (a) : (b))
#define mMIN(a,b)	(((a) < (b)) ? (a) : (b))

#define	mDIM(x)		(sizeof(x) / sizeof(x[0]))

#define mFROMDIGIT(ch)	((ch) - 0x30)  /* digit to char */
#define mTODIGIT(ch) 	((ch) + 0x30)  /* int to char */

#define mISLEAP(a)	( !( a % 400 ) || ( ( a % 100 ) && !( a % 4 ) ) )

#define mFOREVER		for( ;; )

#define mROUND_TO_NEXT_4( a ) ( ((a) % 4) ? ( (a) + 4 - ((a)%4) ) : (a) )

/*--------------------------------------------------------------------------
	C language
----------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* __OCTMAC_H__ */
