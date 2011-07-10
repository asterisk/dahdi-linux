/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

File: oct6100_user.c

    Copyright (c) 2001-2007 Octasic Inc.
    
Description: 

	This file contains the functions provided by the user.

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

$Octasic_Revision: 28 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/


/*****************************  INCLUDE FILES  *******************************/


#include "oct6100api/oct6100_apiud.h"
#include "oct6100api/oct6100_errors.h"



/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100UserGetTime

Description:	Returns the system time in us.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pTime					Pointer to structure in which the time is returned.
 
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100UserGetTime
UINT32 Oct6100UserGetTime(
				IN OUT	tPOCT6100_GET_TIME					f_pTime )
{

	return cOCT6100_ERR_OK;
}
#endif





/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100UserMemSet

Description:	Sets f_ulLength bytes pointed to by f_pAddress to f_ulPattern.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------

f_pAddress				Address in host memory where data should be set.
f_ulPattern				Pattern to apply at the address.  This value will never
						exceed 0xFF.
f_ulLength				Length in bytes to set.
 
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100UserMemSet
UINT32 Oct6100UserMemSet(
				IN		PVOID						f_pAddress,
				IN		UINT32						f_ulPattern,
				IN		UINT32						f_ulLength )
{

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100UserMemCopy

Description:	Copy f_ulLength bytes from f_pSource to f_pDestination.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------

f_pDestination			Host data destination address.
f_pSource				Host data source address.
f_ulLength				Length in bytes to copy.
 
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100UserMemCopy
UINT32 Oct6100UserMemCopy(
				IN		PVOID						f_pDestination,
				IN		const void					*f_pSource,
				IN		UINT32						f_ulLength )
{

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100UserCreateSerializeObject

Description:	Creates a serialization object. The serialization object is
				seized via the Oct6100UserSeizeSerializeObject function.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pCreate				Pointer to structure in which the serialization object's
						handle is returned.
 
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100UserCreateSerializeObject
UINT32 Oct6100UserCreateSerializeObject(
				IN OUT	tPOCT6100_CREATE_SERIALIZE_OBJECT	f_pCreate )
{


	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100UserDestroySerializeObject

Description:	Destroys the indicated serialization object.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pDestroy				Pointer to structure containing the handle of the
						serialization object.
 
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100UserDestroySerializeObject
UINT32 Oct6100UserDestroySerializeObject(
				IN		tPOCT6100_DESTROY_SERIALIZE_OBJECT		f_pDestroy )
{	

	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100UserSeizeSerializeObject

Description:	Seizes the indicated serialization object.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pSeize				Pointer to structure containing the handle of the
						serialization object.
 
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100UserSeizeSerializeObject
UINT32 Oct6100UserSeizeSerializeObject(
				IN		tPOCT6100_SEIZE_SERIALIZE_OBJECT			f_pSeize )
{


	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100UserReleaseSerializeObject

Description:	Releases the indicated serialization object.

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pRelease				Pointer to structure containing the handle of the
						serialization object.
 
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100UserReleaseSerializeObject
UINT32 Oct6100UserReleaseSerializeObject(
				IN		tPOCT6100_RELEASE_SERIALIZE_OBJECT		f_pRelease )
{


	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100UserDriverWriteApi

Description:    Performs a write access to the chip. This function is
				accessible only from the API code entity (i.e. not from the
				APIMI code entity).

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pWriteParams			Pointer to structure containing the Params to the
						write function.
 
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100UserDriverWriteApi
UINT32 Oct6100UserDriverWriteApi(
				IN		tPOCT6100_WRITE_PARAMS					f_pWriteParams )
{



	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100UserDriverWriteOs

Description:    Performs a write access to the chip. This function is
				accessible only from the APIMI code entity (i.e. not from the
				API code entity).

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pWriteParams			Pointer to structure containing the Params to the
						write function.
 
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100UserDriverWriteOs
UINT32 Oct6100UserDriverWriteOs(
				IN		tPOCT6100_WRITE_PARAMS					f_pWriteParams )
{

	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100UserDriverWriteSmearApi

Description:    Performs a series of write accesses to the chip. The same data
				word is written to a series of addresses. The writes begin at
				the start address, and the address is incremented by the
				indicated amount for each subsequent write. This function is
				accessible only from the API code entity (i.e. not from the
				APIMI code entity).

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pSmearParams			Pointer to structure containing the parameters to the
						write smear function.
 
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100UserDriverWriteSmearApi
UINT32 Oct6100UserDriverWriteSmearApi(
				IN		tPOCT6100_WRITE_SMEAR_PARAMS			f_pSmearParams )
{




	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100UserDriverWriteSmearOs

Description:    Performs a series of write accesses to the chip. The same data
				word is written to a series of addresses. The writes begin at
				the start address, and the address is incremented by the
				indicated amount for each subsequent write. This function is
				accessible only from the APIMI code entity (i.e. not from the
				API code entity).

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pSmearParams			Pointer to structure containing the parameters to the
						write smear function.
 
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100UserDriverWriteSmearOs
UINT32 Oct6100UserDriverWriteSmearOs(
				IN		tPOCT6100_WRITE_SMEAR_PARAMS			f_pSmearParams )
{

	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100UserDriverWriteBurstApi

Description:    Performs a series of write accesses to the chip. An array of
				data words is written to a series of consecutive addresses.
				The writes begin at the start address with element 0 of the
				provided array as the data word. The address is incremented by
				two for each subsequent write. This function is accessible only
				from the API code entity (i.e. not from the APIMI code entity).

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pBurstParams			Pointer to structure containing the parameters to the
						write burst function.
 
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100UserDriverWriteBurstApi
UINT32 Oct6100UserDriverWriteBurstApi(
				IN		tPOCT6100_WRITE_BURST_PARAMS		f_pBurstParams )
{




	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100UserDriverWriteBurstOs

Description:    Performs a series of write accesses to the chip. An array of
				data words is written to a series of consecutive addresses.
				The writes begin at the start address with element 0 of the
				provided array as the data word. The address is incremented by
				two for each subsequent write. This function is accessible only
				from the API code entity (i.e. not from the APIMI code entity).

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pBurstParams			Pointer to structure containing the parameters to the
						write burst function.
 
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100UserDriverWriteBurstOs
UINT32 Oct6100UserDriverWriteBurstOs(
				IN		tPOCT6100_WRITE_BURST_PARAMS		f_pBurstParams )
{

	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100UserDriverReadApi

Description:    Performs a read access to the chip. This function is accessible
				only from the API code entity (i.e. not from the APIMI code
				entity).

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pReadParams			Pointer to structure containing the parameters to the
						read function.
 
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100UserDriverReadApi
UINT32 Oct6100UserDriverReadApi(
				IN OUT	tPOCT6100_READ_PARAMS				f_pReadParams )
{



	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100UserDriverReadOs

Description:    Performs a read access to the chip. This function is accessible
				only from the APIMI code entity (i.e. not from the API code
				entity).

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pReadParams			Pointer to structure containing the parameters to the
						read function.
 
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100UserDriverReadOs
UINT32 Oct6100UserDriverReadOs(
				IN OUT	tPOCT6100_READ_PARAMS				f_pReadParams )
{


	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100UserDriverReadBurstApi

Description:    Performs a burst of read accesses to the chip. The first read
				is performed at the start address, and the address is
				incremented by two for each subsequent read. The data is
				retunred in an array provided by the user. This function is
				accessible only from the API code entity (i.e. not from the
				APIMI code entity).

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pBurstParams			Pointer to structure containing the parameters to the
						read burst function.
 
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100UserDriverReadBurstApi
UINT32 Oct6100UserDriverReadBurstApi(
				IN OUT	tPOCT6100_READ_BURST_PARAMS			f_pBurstParams )
{



	
	return cOCT6100_ERR_OK;
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\

Function:		Oct6100UserDriverReadBurstOs

Description:    Performs a burst of read accesses to the chip. The first read
				is performed at the start address, and the address is
				incremented by two for each subsequent read. The data is
				retunred in an array provided by the user. This function is
				accessible only from the APIMI code entity (i.e. not from the
				API code entity).

-------------------------------------------------------------------------------
|	Argument		|	Description
-------------------------------------------------------------------------------
f_pBurstParams			Pointer to structure containing the parameters to the
						read burst function.
 
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_Oct6100UserDriverReadBurstOs
UINT32 Oct6100UserDriverReadBurstOs(
				IN OUT	tPOCT6100_READ_BURST_PARAMS			f_pBurstParams )
{


	return cOCT6100_ERR_OK;
}
#endif







