/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File:  oct6100_apiud.h

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	Header file containing the definitions and prototypes that are to be
	completed by the user.

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

#ifndef __OCT6100_APIUD_H__
#define __OCT6100_APIUD_H__

/*****************************  INCLUDE FILES  *******************************/

#include "octdef.h"

/*****************************  DEFINES  *************************************/


/* Determines the maximum length of a burst of reads/writes. This value must
	be in the range 8 - 1024. This value obtains best performance if set to
	a power of 2 (i.e. 2^n). */
#define cOCT6100_MAX_RW_ACCESSES	32

/* The define used to specify that the Oct6100SeizeSerializeObject function
	is not to return until the specified serialization object has been seized. */
#define cOCT6100_WAIT_INFINITELY	0xFFFFFFFF


/* Compile option: enabling this compile option inserts code to check every
	call to a user provided function to make sure the function parameters
	are not changed, as required by the API specification. */
#define cOCT6100_USER_FUNCTION_CHECK



#define cOCT6100_GET_TIME_FAILED_0			0xFFFF0000
#define cOCT6100_GET_TIME_FAILED_1			0xFFFF0001
#define cOCT6100_GET_TIME_FAILED_2			0xFFFF0002
#define cOCT6100_GET_TIME_FAILED_3			0xFFFF0003
#define cOCT6100_GET_TIME_FAILED_4			0xFFFF0004

#define cOCT6100_CREATE_SERIAL_FAILED_0		0xFFFF0010
#define cOCT6100_CREATE_SERIAL_FAILED_1		0xFFFF0011
#define cOCT6100_CREATE_SERIAL_FAILED_2		0xFFFF0012
#define cOCT6100_CREATE_SERIAL_FAILED_3		0xFFFF0013
#define cOCT6100_CREATE_SERIAL_FAILED_4		0xFFFF0014

#define cOCT6100_DESTROY_SERIAL_FAILED_0	0xFFFF0020
#define cOCT6100_DESTROY_SERIAL_FAILED_1	0xFFFF0021
#define cOCT6100_DESTROY_SERIAL_FAILED_2	0xFFFF0022
#define cOCT6100_DESTROY_SERIAL_FAILED_3	0xFFFF0023
#define cOCT6100_DESTROY_SERIAL_FAILED_4	0xFFFF0024

#define cOCT6100_INVALID_SERIAL_HANDLE_0	0xFFFF0030
#define cOCT6100_INVALID_SERIAL_HANDLE_1	0xFFFF0031
#define cOCT6100_INVALID_SERIAL_HANDLE_2	0xFFFF0032
#define cOCT6100_INVALID_SERIAL_HANDLE_3	0xFFFF0033
#define cOCT6100_INVALID_SERIAL_HANDLE_4	0xFFFF0034

#define cOCT6100_RELEASE_SERIAL_FAILED_0	0xFFFF0040
#define cOCT6100_RELEASE_SERIAL_FAILED_1	0xFFFF0041
#define cOCT6100_RELEASE_SERIAL_FAILED_2	0xFFFF0042
#define cOCT6100_RELEASE_SERIAL_FAILED_3	0xFFFF0043
#define cOCT6100_RELEASE_SERIAL_FAILED_4	0xFFFF0044

#define cOCT6100_SEIZE_SERIAL_FAILED_0		0xFFFF0050
#define cOCT6100_SEIZE_SERIAL_FAILED_1		0xFFFF0051
#define cOCT6100_SEIZE_SERIAL_FAILED_2		0xFFFF0052
#define cOCT6100_SEIZE_SERIAL_FAILED_3		0xFFFF0053
#define cOCT6100_SEIZE_SERIAL_FAILED_4		0xFFFF0054

#define cOCT6100_DRIVER_WRITE_FAILED_0		0xFFFF0060
#define cOCT6100_DRIVER_WRITE_FAILED_1		0xFFFF0061
#define cOCT6100_DRIVER_WRITE_FAILED_2		0xFFFF0062
#define cOCT6100_DRIVER_WRITE_FAILED_3		0xFFFF0063
#define cOCT6100_DRIVER_WRITE_FAILED_4		0xFFFF0064

#define cOCT6100_DRIVER_WSMEAR_FAILED_0		0xFFFF0070
#define cOCT6100_DRIVER_WSMEAR_FAILED_1		0xFFFF0071
#define cOCT6100_DRIVER_WSMEAR_FAILED_2		0xFFFF0072
#define cOCT6100_DRIVER_WSMEAR_FAILED_3		0xFFFF0073
#define cOCT6100_DRIVER_WSMEAR_FAILED_4		0xFFFF0074

#define cOCT6100_DRIVER_WBURST_FAILED_0		0xFFFF0080
#define cOCT6100_DRIVER_WBURST_FAILED_1		0xFFFF0081
#define cOCT6100_DRIVER_WBURST_FAILED_2		0xFFFF0082
#define cOCT6100_DRIVER_WBURST_FAILED_3		0xFFFF0083
#define cOCT6100_DRIVER_WBURST_FAILED_4		0xFFFF0084

#define cOCT6100_DRIVER_READ_FAILED_0		0xFFFF0090
#define cOCT6100_DRIVER_READ_FAILED_1		0xFFFF0091
#define cOCT6100_DRIVER_READ_FAILED_2		0xFFFF0092
#define cOCT6100_DRIVER_READ_FAILED_3		0xFFFF0093
#define cOCT6100_DRIVER_READ_FAILED_4		0xFFFF0094

#define cOCT6100_DRIVER_RBURST_FAILED_0		0xFFFF00A0
#define cOCT6100_DRIVER_RBURST_FAILED_1		0xFFFF00A1
#define cOCT6100_DRIVER_RBURST_FAILED_2		0xFFFF00A2
#define cOCT6100_DRIVER_RBURST_FAILED_3		0xFFFF00A3
#define cOCT6100_DRIVER_RBURST_FAILED_4		0xFFFF00A4





/*****************************  TYPES  ***************************************/

/*Change this type if your platform uses 64bits semaphores/locks */ 
typedef UINT32 tOCT6100_USER_SERIAL_OBJECT;

typedef struct _OCT6100_GET_TIME_
{
	PVOID	pProcessContext;
	UINT32	aulWallTimeUs[ 2 ];

} tOCT6100_GET_TIME, *tPOCT6100_GET_TIME;





typedef struct _OCT6100_CREATE_SERIALIZE_OBJECT_
{
	PVOID						pProcessContext;
	PSZ							pszSerialObjName;
	tOCT6100_USER_SERIAL_OBJECT	ulSerialObjHndl;

} tOCT6100_CREATE_SERIALIZE_OBJECT, *tPOCT6100_CREATE_SERIALIZE_OBJECT;


typedef struct _OCT6100_DESTROY_SERIALIZE_OBJECT_
{
	PVOID						pProcessContext;
	tOCT6100_USER_SERIAL_OBJECT	ulSerialObjHndl;

} tOCT6100_DESTROY_SERIALIZE_OBJECT, *tPOCT6100_DESTROY_SERIALIZE_OBJECT;


typedef struct _OCT6100_SEIZE_SERIALIZE_OBJECT_
{
	PVOID						pProcessContext;
	tOCT6100_USER_SERIAL_OBJECT	ulSerialObjHndl;
	UINT32						ulTryTimeMs;

} tOCT6100_SEIZE_SERIALIZE_OBJECT, *tPOCT6100_SEIZE_SERIALIZE_OBJECT;


typedef struct _OCT6100_RELEASE_SERIALIZE_OBJECT_
{
	PVOID						pProcessContext;
	tOCT6100_USER_SERIAL_OBJECT	ulSerialObjHndl;

} tOCT6100_RELEASE_SERIALIZE_OBJECT, *tPOCT6100_RELEASE_SERIALIZE_OBJECT;


typedef struct _OCT6100_WRITE_PARAMS_
{
	PVOID	pProcessContext;

	UINT32	ulUserChipId;
	UINT32	ulWriteAddress;
	UINT16	usWriteData;

} tOCT6100_WRITE_PARAMS, *tPOCT6100_WRITE_PARAMS;


typedef struct _OCT6100_WRITE_SMEAR_PARAMS_
{
	PVOID	pProcessContext;

	UINT32	ulUserChipId;
	UINT32	ulWriteAddress;
	UINT32	ulWriteLength;
	UINT16	usWriteData;

} tOCT6100_WRITE_SMEAR_PARAMS, *tPOCT6100_WRITE_SMEAR_PARAMS;


typedef struct _OCT6100_WRITE_BURST_PARAMS_
{
	PVOID	pProcessContext;

	UINT32	ulUserChipId;
	UINT32	ulWriteAddress;
	UINT32	ulWriteLength;
	PUINT16	pusWriteData;

} tOCT6100_WRITE_BURST_PARAMS, *tPOCT6100_WRITE_BURST_PARAMS;


typedef struct _OCT6100_READ_PARAMS_
{
	PVOID	pProcessContext;

	UINT32	ulUserChipId;
	UINT32	ulReadAddress;
	PUINT16	pusReadData;

} tOCT6100_READ_PARAMS, *tPOCT6100_READ_PARAMS;


typedef struct _OCT6100_READ_BURST_PARAMS_
{
	PVOID	pProcessContext;

	UINT32	ulUserChipId;
	UINT32	ulReadAddress;
	UINT32	ulReadLength;
	PUINT16	pusReadData;

} tOCT6100_READ_BURST_PARAMS, *tPOCT6100_READ_BURST_PARAMS;








/************************** FUNCTION PROTOTYPES  *****************************/

/* Time function. */
UINT32 Oct6100UserGetTime(
				IN OUT	tPOCT6100_GET_TIME					f_pTime );



/* Memory management functions. */
UINT32 Oct6100UserMemSet(
				IN		PVOID								f_pAddress,
				IN		UINT32								f_ulPattern,
				IN		UINT32								f_ulLength );

UINT32 Oct6100UserMemCopy(
				IN		PVOID								f_pDestination,
				IN		const void							*f_pSource,
				IN		UINT32								f_ulLength );

/* Serialization functions. */
UINT32 Oct6100UserCreateSerializeObject(
				IN OUT tPOCT6100_CREATE_SERIALIZE_OBJECT	f_pCreate);

UINT32 Oct6100UserDestroySerializeObject(
				IN tPOCT6100_DESTROY_SERIALIZE_OBJECT		f_pDestroy);

UINT32 Oct6100UserSeizeSerializeObject(
				IN tPOCT6100_SEIZE_SERIALIZE_OBJECT			f_pSeize);

UINT32 Oct6100UserReleaseSerializeObject(
				IN tPOCT6100_RELEASE_SERIALIZE_OBJECT		f_pRelease);

/* Read/Write functions.*/
UINT32 Oct6100UserDriverWriteApi(
				IN	tPOCT6100_WRITE_PARAMS					f_pWriteParams );

UINT32 Oct6100UserDriverWriteOs(
				IN	tPOCT6100_WRITE_PARAMS					f_pWriteParams );

UINT32 Oct6100UserDriverWriteSmearApi(
				IN	tPOCT6100_WRITE_SMEAR_PARAMS			f_pSmearParams );

UINT32 Oct6100UserDriverWriteSmearOs(
				IN	tPOCT6100_WRITE_SMEAR_PARAMS			f_pSmearParams );

UINT32 Oct6100UserDriverWriteBurstApi(
				IN	tPOCT6100_WRITE_BURST_PARAMS			f_pBurstParams );

UINT32 Oct6100UserDriverWriteBurstOs(
				IN	tPOCT6100_WRITE_BURST_PARAMS			f_pBurstParams );

UINT32 Oct6100UserDriverReadApi(
				IN OUT	tPOCT6100_READ_PARAMS				f_pReadParams );

UINT32 Oct6100UserDriverReadOs(
				IN OUT	tPOCT6100_READ_PARAMS				f_pReadParams );

UINT32 Oct6100UserDriverReadBurstApi(
				IN OUT	tPOCT6100_READ_BURST_PARAMS			f_pBurstParams );

UINT32 Oct6100UserDriverReadBurstOs(
				IN OUT	tPOCT6100_READ_BURST_PARAMS			f_pBurstParams );







#endif /* __OCT6100_APIUD_H__ */
