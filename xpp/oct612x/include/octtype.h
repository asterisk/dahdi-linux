/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: octtype.h

    Copyright (c) 2001-2007 Octasic Inc.

Description: 

	This file defines the base storage types.

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

$Octasic_Revision: 18 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#ifndef __OCTTYPE_H__
#define __OCTTYPE_H__

/*--------------------------------------------------------------------------
	C language
----------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

/*--------------------------------------------------------------------------
	Include target-specific header if available 
----------------------------------------------------------------------------*/
#if defined( OCT_NTDRVENV )
	#include "octtypentdrv.h"		/* All NT driver typedef */
#elif defined( OCT_WINENV )	
	#include "octtypewin.h"			/* All Win32 typedef */
#elif defined( OCT_VXENV )
	#include "octtypevx.h"			/* All VxWorks typedef */
#else
/*--------------------------------------------------------------------------
	No target-specific header  available 
----------------------------------------------------------------------------*/

#ifdef SZ
#undef SZ
#endif

/*****************************  DEFINES  *************************************/
/* 16-bit integer */
typedef unsigned short	UINT16;
typedef signed short	INT16;
typedef unsigned short	*PUINT16;
typedef signed short	*PINT16;

/* 8-bit integer */
typedef unsigned char	UINT8;
typedef signed char		INT8;
typedef signed char		OCT_INT8;
typedef unsigned char	*PUINT8;
typedef signed char		*PINT8;


/* 32 bit integer */
typedef unsigned int	UINT32;
typedef signed int		INT32;
typedef INT32 *			PINT32;
typedef UINT32 *		PUINT32;

/* Long integer */
typedef signed long		LONG;
typedef unsigned long	ULONG;
typedef	long *			PLONG;
typedef	unsigned long *	PULONG;

/* Short integer */
typedef	short			SHORT;
typedef	unsigned short	USHORT;
typedef	short *			PSHORT;
typedef	unsigned short *PUSHORT;

/* 8-bit integer*/
typedef unsigned char	BYTE;
typedef	BYTE *			PBYTE;
typedef unsigned char	UCHAR;

/* Character and strings */
typedef char			CHAR;
typedef	CHAR 			SZ;
typedef	CHAR *			PSZ;
typedef	CHAR *			PCHAR;

/* Double integers */
typedef	double			DOUBLE;
typedef	double *		PDOUBLE;
typedef	float			FLOAT;
typedef	float *			PFLOAT;

typedef	void			VOID;
typedef	void *			PVOID;

/* Booleans */
typedef	int				BOOL;
typedef	BOOL *			PBOOL;

/* Integers */
typedef	int				INT;
typedef	int *			PINT;
typedef	unsigned int	UINT;
typedef	unsigned int *	PUINT;

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

#if defined(__FreeBSD__)
#include <sys/stddef.h>
#else
#include <linux/stddef.h>
#endif

#endif

/*--------------------------------------------------------------------------
	C language
----------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif	/* __OCTTYPE_H__ */
