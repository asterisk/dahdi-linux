/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  octapi_largmath.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	Library used to perform arithmetic on integer values of an integer multiple
	of 32-bits.

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

$Octasic_Revision: 6 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#ifndef __OCTAPI_LARGMATH_H__
#define __OCTAPI_LARGMATH_H__

#include "octdef.h"

#define OCTAPI_LM_DIVISION_BY_ZERO		0xFFFF
#define OCTAPI_LM_OVERFLOW				0xFFFE
#define OCTAPI_LM_ARRAY_SIZE_MISMATCH	0xFFFD

#define OCTAPI_LM_MAX_OPTIMIZE_MUL		10

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define octapi_lm_add( a, alen, b, blen, z,  zlen )					OctApiLmAdd( (PUINT32) a, (USHORT) alen, (PUINT32) b, (USHORT) blen, (PUINT32) z, (USHORT) zlen )
#define octapi_lm_subtract( a, alen, bneg, blen, z,  zlen, neg )	OctApiLmSubtract( (PUINT32) a, (USHORT) alen, (PUINT32) bneg, (USHORT) blen, (PUINT32) z, (USHORT) zlen, (USHORT*) neg )
#define octapi_lm_compare( a, alen, bneg, blen, neg )				OctApiLmCompare( (PUINT32) a, (USHORT) alen, (PUINT32) bneg, (USHORT) blen, (USHORT*) neg )
#define octapi_lm_multiply( a, b, ablen, z )						OctApiLmMultiply( (PUINT32) a, (PUINT32) b, (USHORT) ablen, (PUINT32) z )
#define octapi_lm_divide( n, d, q, r, ndqrlen )						OctApiLmDivide( (PUINT32) n, (PUINT32) d, (PUINT32) q, (PUINT32) r, (USHORT) ndqrlen )
#define octapi_lm_shiftright1( a, alen )							OctApiLmShiftRight1( (PUINT32) a, (USHORT) alen )
#define octapi_lm_shiftn( a, alen, shiftleft, shiftn )				OctApiLmShiftn( (PUINT32) a, (USHORT) alen, (USHORT) shiftleft, (USHORT) shiftn )
#define octapi_lm_getmsb( a, alen, msb_pos )						OctApiLmGetMsb( (PUINT32) a, (USHORT) alen, (USHORT*) msb_pos )
	

UINT32 OctApiLmAdd( PUINT32 a, USHORT alen, PUINT32 b, USHORT blen, PUINT32 z, USHORT zlen );
UINT32 OctApiLmSubtract( PUINT32 a, USHORT alen, PUINT32 bneg, USHORT blen, PUINT32 z, USHORT zlen, PUSHORT neg );
UINT32 OctApiLmCompare( PUINT32 a, USHORT alen, PUINT32 bneg, USHORT blen, PUSHORT neg );
UINT32 OctApiLmMultiply( PUINT32 a, PUINT32 b, USHORT ablen, PUINT32 z );
UINT32 OctApiLmDivide( PUINT32 n, PUINT32 d, PUINT32 q, PUINT32 r, USHORT ndqrlen );
UINT32 OctApiLmShiftRight1( PUINT32 a, USHORT alen );
UINT32 OctApiLmShiftn( PUINT32 a, USHORT alen, USHORT shiftleft, USHORT shiftn );
UINT32 OctApiLmGetMsb( PUINT32 a, USHORT alen, PUSHORT msb_pos );

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __OCTAPI_LARGMATH_H__ */
