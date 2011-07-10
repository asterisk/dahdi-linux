/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: octdef.h

    Copyright (c) 2001-2007 Octasic Inc.

Description: 

	Common system definitions.

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

$Octasic_Revision: 12 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/

#ifndef __OCTDEF_H__
#define __OCTDEF_H__

/*--------------------------------------------------------------------------
	C language
----------------------------------------------------------------------------*/

#ifdef __cplusplus
extern "C" {
#endif

/*****************************  INCLUDE FILES  *******************************/

/*--------------------------------------------------------------------------
	Get Platform Dependency headers 
----------------------------------------------------------------------------*/
#include "octosdependant.h"


/*--------------------------------------------------------------------------
	Common Type definitions
----------------------------------------------------------------------------*/
#include "octtype.h"

/*****************************  DEFINES  *************************************/

/* List of functions to skip compiling since we don't use them */
#include "digium_unused.h"



/*--------------------------------------------------------------------------
	Miscellaneous constants
----------------------------------------------------------------------------*/

#ifndef PROTO
#define PROTO extern
#endif

/* Generic return codes. */
#define cOCTDEF_RC_OK		0		/* Generic Ok */
#define cOCTDEF_RC_ERROR	1		/* Generic Error */

/* Default return values of all OCTAPI functions. */
#ifndef GENERIC_OK
#define GENERIC_OK			0x00000000
#endif

#ifndef GENERIC_ERROR
#define GENERIC_ERROR		0x00000001
#endif

#ifndef GENERIC_BAD_PARAM
#define GENERIC_BAD_PARAM	0x00000002
#endif

/* Defines of boolean expressions (TRUE/FALSE) */
#ifndef FALSE
#define FALSE (BOOL)0
#endif

#ifndef TRUE
#define TRUE  (BOOL)1
#endif

/*--------------------------------------------------------------------------
	DLL Import-Export
----------------------------------------------------------------------------*/

#ifdef OCT_WINENV
#define DLLIMP	__declspec( dllimport )
#define DLLEXP	__declspec( dllexport ) 
#else
#define DLLIMP	
#define DLLEXP	
#endif

/*--------------------------------------------------------------------------
	C language
----------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* __OCTDEF_H__ */
