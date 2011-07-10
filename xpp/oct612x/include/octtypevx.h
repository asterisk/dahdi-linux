/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: octtypevx.h

    Copyright (c) 2001-2007 Octasic Inc.

Description: 

	This file defines the base storage types for the VxWorks environment.

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

$Octasic_Revision: 9 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCTTYPEVX_H__
#define __OCTTYPEVX_H__

/*--------------------------------------------------------------------------
	C language
----------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

#include "vxWorks.h" 

/* 16-bit pointer integer */
typedef unsigned short	*PUINT16;
typedef signed short	*PINT16;

/* 8-bit integer pointer */
typedef unsigned char	*PUINT8;
typedef signed char		*PINT8;

/* 32-bit integer pointer */
typedef INT32 *			PINT32;
typedef UINT32 *		PUINT32;

/* Long integer pointer */
/*Intel library for file system definition*/
#ifndef DATATYPE_H				
typedef	long 			LONG;
#endif
typedef	long *			PLONG;
typedef	unsigned long *	PULONG;

/* Short integer pointer */
typedef	short 			SHORT;
typedef	short *			PSHORT;
typedef	unsigned short *PUSHORT;

/* 8-bit integer*/
#if	(CPU!=SIMNT) && !defined(DATATYPE_H)
typedef	char 			BYTE;
#endif


typedef	BYTE *			PBYTE;

/* Character and strings */
/*Intel library for file system definition*/
#ifndef DATATYPE_H				
typedef char			CHAR;
#endif
typedef char *			PCHAR;
typedef	CHAR 			SZ;
typedef	CHAR *			PSZ;
typedef signed char		OCT_INT8;

/* Double integers */
typedef	double			DOUBLE;
typedef	double *		PDOUBLE;
typedef	float			FLOAT;
typedef	float *			PFLOAT;

typedef	void *			PVOID;

/* Booleans */
typedef	BOOL *			PBOOL;

/* Integers */
typedef int				INT;
typedef	int *			PINT;
typedef	unsigned int	PUINT;

/* Define pseudo-keywords IN and OUT if not defined yet */
#ifndef IN
#define IN		/* IN param */
#endif

#ifndef OUT
#define OUT		/* OUT param */
#endif

/* LONG LONG */
#define LLONG			signed long long
#define PLLONG			signed long long *
#define ULLONG			unsigned long long
#define PULLONG			unsigned long long *

#ifndef OPT
#define OPT		/* OPT param */
#endif

typedef	PSZ *	PPSZ;


/*--------------------------------------------------------------------------
	C language
----------------------------------------------------------------------------*/
#ifdef __cplusplus
}
#endif

#endif /* __OCTTYPEVX_H__ */
