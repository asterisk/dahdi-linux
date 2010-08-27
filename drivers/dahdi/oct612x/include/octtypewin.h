/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: octtypewin.h

    Copyright (c) 2001-2007 Octasic Inc.

Description: 

	This file defines the base storage types for the Windows environment.
	Includes the Windows definition file and add the missing ones here. 

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

$Octasic_Revision: 16 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCTTYPEWIN_H__
#define __OCTTYPEWIN_H__

/*--------------------------------------------------------------------------
	C language
----------------------------------------------------------------------------*/
#define WIN32_LEAN_AND_MEAN	/* just get the base type definition from Windows */
#include <windows.h>

/* Disable argument not used warning */
#pragma warning( disable : 4100 )
/* Disable Level 4 warning: nonstandard extension used : translation unit is empty */
#pragma warning( disable : 4206 )

#ifdef __cplusplus
extern "C" {
#endif

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

typedef double			DOUBLE;


/* 32 bit integer */
#if ( defined( _MSC_VER ) && _MSC_VER == 1100 )
/* MFC5 compiler does not define UINT32 */
typedef unsigned int	UINT32;
typedef signed int		INT32;
typedef INT32 *			PINT32;
typedef UINT32 *		PUINT32;
#endif	/* _MSC_VER */

/* LONG LONG */
#define	LLONG			signed __int64
#define	PLLONG			signed __int64 *
#define ULLONG			unsigned __int64
#define PULLONG		    unsigned __int64 * 

/* Double integers */
typedef	double			DOUBLE;
typedef	double *		PDOUBLE;
typedef	float			FLOAT;
typedef	float *			PFLOAT;

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

#endif /* __OCTTYPEWIN_H__ */
