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

$Octasic_Revision: 10 $

\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#include "apilib/octapi_largmath.h"


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLmAdd.
|
|	Description:	This function adds 2 numbers, a and b.  Number a is 
|					(alen + 1) * 32 bits long; b is (blen + 1) * 32 bits long.  The
|					result is (zlen + 1) * 32 bits long.  It the function succeeds it returns
|					GENERIC_OK, else GENERIC_ERROR.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*a					UINT32			The array containing the first number.
|	alen				USHORT			The length of array a, minus 1 (0 - 99).
|	*b					UINT32			The array containing the second number.
|	blen				USHORT			The length of array b, minus 1 (0 - 99).
|	*z					UINT32			The array containing the resulting number.
|	zlen				USHORT			The length of array z, minus 1 (0 - 99).
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLmAdd
UINT32 OctApiLmAdd(UINT32 * a,USHORT alen,UINT32 * b,USHORT blen,UINT32 * z, USHORT zlen)
{
	USHORT i;
	UINT32 temp;
	UINT32 carry=0;
	UINT32 aprim;
	UINT32 bprim;

	/* Check for array lengths.*/
	if (alen > zlen || blen > zlen) return(OCTAPI_LM_ARRAY_SIZE_MISMATCH);

	for(i=0;i<=zlen;i++)
	{
		if (i <= alen) aprim = *(a+i); else aprim = 0;
		if (i <= blen) bprim = *(b+i); else bprim = 0;
		temp = aprim + bprim + carry;

		/* Calculate carry for next time.*/
		if (carry == 0)
			if (temp < aprim) carry = 1; else carry = 0;
		else
			if (temp <= aprim) carry = 1; else carry = 0;

		/* Write new value.*/
		*(z+i) = temp;
	}

	/* Check for overflow.*/
	if (carry == 1) return(OCTAPI_LM_OVERFLOW);

	/* All is well.*/
	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLmSubtract.
|
|	Description:	This function subtracts 2 numbers, a and b.  Number a is 
|					(alen + 1) * 32 bits long; b is (blen + 1) * 32 bits long.  The result
|					is (zlen + 1) * 32 bits long.  It the function succeeds it returns
|					GENERIC_OK, else GENERIC_ERROR.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*a					UINT32			The array containing the first number.
|	alen				USHORT			The length of array a, minus 1 (0 - 99).
|	*bneg				UINT32			The array containing the second number.
|	blen				USHORT			The length of array b, minus 1 (0 - 99).
|	*z					UINT32			The array containing the resulting number.
|	zlen				USHORT			The length of array z, minus 1 (0 - 99).
|	*neg				USHORT			Indicates if the result is negative 
|										(TRUE/FALSE).
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLmSubtract
UINT32 OctApiLmSubtract(UINT32 * a,USHORT alen,UINT32 * bneg,USHORT blen,UINT32 * z,USHORT zlen,USHORT * neg)
{
	USHORT i;
	UINT32 temp;
	UINT32 carry=1;
	UINT32 aprim;
	UINT32 bprim;

	/* Check for array lengths.*/
	if (alen > zlen || blen > zlen) return(OCTAPI_LM_ARRAY_SIZE_MISMATCH);

	for(i=0;i<=zlen;i++)
	{
		if (i <= alen) aprim = *(a+i); else aprim = 0;
		if (i <= blen) bprim = ~(*(bneg+i)); else bprim = 0xFFFFFFFF;
		temp = aprim + bprim + carry;

		/* Calculate carry for next time.*/
		if (carry == 0)
			if (temp < aprim) carry = 1; else carry = 0;
		else
			if (temp <= aprim) carry = 1; else carry = 0;

		/* Write new value.*/
		*(z+i) = temp;
	}

	/* Check for overflow, which means negative number!*/
	if (carry == 0)
	{
		/* Number is not of right neg. Invert and add one to correct neg.*/
		for(i=0;i<=zlen;i++)
			*(z+i) = ~(*(z+i));

		temp = 1;
		OctApiLmAdd(&temp,0,z,zlen,z,zlen);

		*neg = TRUE;
		return(GENERIC_OK);
	}

	/* Result is positive.*/
	*neg = FALSE;
	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLmCompare.
|
|	Description:	This function compares two numbers (arrays) of equal lengths.
|					Number a is (alen + 1) * 32 bits long; b is (blen + 1) * 32 bits long.  The result
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*a					UINT32			The array containing the first number.
|	alen				USHORT			The length of array a, minus 1 (0 - 99).
|	*b					UINT32			The array containing the second number.
|	blen				USHORT			The length of array b, minus 1 (0 - 99).
|	*neg				USHORT			Result of compare.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLmCompare
UINT32 OctApiLmCompare(UINT32 * a,USHORT alen,UINT32 * bneg,USHORT blen,USHORT * neg)
{
	USHORT i;
	UINT32 temp;
	UINT32 carry=1;
	UINT32 aprim;
	UINT32 bprim;
	UINT32 zlen;

	/* Set zlen to alen or blen (which ever is longer)*/
	if (alen < blen)
		zlen = blen;
	else
		zlen = alen;

	for(i=0;i<=zlen;i++)
	{
		if (i <= alen) aprim = *(a+i); else aprim = 0;
		if (i <= blen) bprim = ~(*(bneg+i)); else bprim = 0xFFFFFFFF;
		temp = aprim + bprim + carry;

		/* Calculate carry for next time.*/
		if (carry == 0)
			if (temp < aprim) carry = 1; else carry = 0;
		else
			if (temp <= aprim) carry = 1; else carry = 0;
	}

	/* Check for overflow, which means negative number!*/
	if (carry == 0)
	{
		*neg = TRUE;
		return(GENERIC_OK);
	}

	/* Result is positive.*/
	*neg = FALSE;
	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLmSubtract.
|
|	Description:	This function multiplies 2 numbers, a and b.  Number a and 
|					b are both  (ablen + 1) * 32 bits long.  The result is twice as
|					long.  If the functions succeeds if returns GENERIC_OK, 
|					else GENERIC_ERROR.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*a					UINT32			The array containing the first number.
|	*b					UINT32			The array containing the second number.
|	ablen				USHORT			The length of arrays a and b, minus 1 (0 - 99).
|	*z					UINT32			The array containing the resulting number.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLmMultiply
UINT32 OctApiLmMultiply(UINT32 * a,UINT32 * b,USHORT ablen,UINT32 * z)
{
	USHORT i,j,k;
	USHORT nos;
	UINT32 lownum;
	UINT32 highnum;
	USHORT longnumi;
	USHORT longnumj;
	USHORT indentw,indentl;


	/* Caculate number of shorts in a and b.*/
	nos = (USHORT)((ablen+1) * 2);

	/* Clear answer word.*/
	for(i=0;i<nos;i++)
		*(z+i) = 0;

	{
		USHORT optimizea, optimizeb;
		USHORT l;
		optimizea = TRUE;
		optimizeb = TRUE;
		for(l = 1; l < ablen+1; l++)
		{
			if(*(a+l) != 0)
				optimizea = FALSE;
			if(*(b+l) != 0)
				optimizeb = FALSE;
		}
		if(*a > OCTAPI_LM_MAX_OPTIMIZE_MUL)
			optimizea = FALSE;
		if(*b > OCTAPI_LM_MAX_OPTIMIZE_MUL)
			optimizeb = FALSE;

		if(optimizea == TRUE)
		{
			for(l = 0; l < *a; l++)
				OctApiLmAdd(z, (USHORT)(nos-1), b, ablen, z, (USHORT)(nos-1));
			return(GENERIC_OK);
		}

		if(optimizeb == TRUE)
		{
			for(l = 0; l < *b; l++)
				OctApiLmAdd(z, (USHORT)(nos-1), a, ablen, z, (USHORT)(nos-1));
			return(GENERIC_OK);
		}
	}

	for(i=0;i<nos;i++)
	{
		longnumi = (USHORT)( i/2 );
		/* One iteration per short in a.*/
		if ((i%2) == 0)
			lownum = *(a+longnumi) & 0xFFFF;  /* Even word. Lower part of long.*/
		else
			lownum = *(a+longnumi)>>16;       /* Odd word. Upper part of long.*/

		for(j=0;j<nos;j++)
		{
			UINT32 product;

			longnumj = (USHORT)( j/2 );
			/* One iteration per short in a.*/
			if ((j%2) == 0)
				highnum = *(b+longnumj) & 0xFFFF;  /* Even word. Lower part of long.*/
			else
				highnum = *(b+longnumj)>>16;       /* Odd word. Upper part of long.*/

			/* Find the word indent of the answer. 0 = no indent. 1 = one word indent.*/
			indentw = (USHORT)( j+i );
			indentl = (USHORT)( indentw / 2 );

			/* Multiply both numbers.*/
			product = highnum * lownum;

			/* After multiplying both numbers, add result to end result.*/
			if ((indentw % 2) == 0) /* Even word boundary, addition in one shot!*/
			{
				UINT32 carry=0;
				UINT32 temp;
				UINT32 addme;

				for(k=indentl;k<nos;k++)
				{
					if (k==indentl) addme = product; else addme = 0;

					temp = *(z+k) + addme + carry;

					/* Calculate carry for next time.*/
					if (carry == 0)
						if (temp < addme) carry = 1; else carry = 0;
					else
						if (temp <= addme) carry = 1; else carry = 0;

					/* Set value.*/
					*(z+k) = temp;
				}

				/* Carry should always be 0.*/
				if (carry == 1) return(GENERIC_ERROR);
			}
			else /* Odd word boundary, addition in two shots.*/
			{
				UINT32 carry=0;
				UINT32 temp;
				UINT32 addme;

				for(k=indentl;k<nos;k++)
				{
					if (k==indentl) addme = product<<16;
					else if (k==(indentl+1)) addme = product>>16;
					else addme = 0;

					temp = *(z+k) + addme + carry;

					/* Calculate carry for next time.*/
					if (carry == 0)
						if (temp < addme) carry = 1; else carry = 0;
					else
						if (temp <= addme) carry = 1; else carry = 0;

					/* Set value.*/
					*(z+k) = temp;
				}

				/* Carry should always be 0.*/
				if (carry == 1) return(GENERIC_ERROR);
			}
		}
	}

	return(GENERIC_OK);
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLmDivide.
|
|	Description:	This function divides the number n by the number d.  The
|					quotient is placed in q and the remainder in r.  The arrays
|					n, d, q and r are all of the same length, namely (ndqrlen + 1).
|					If the functions succeeds if returns GENERIC_OK, else 
|					GENERIC_ERROR.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	*a					UINT32			The array containing the first number.
|	*b					UINT32			The array containing the second number.
|	ablen				USHORT			The length of arrays a and b, minus 1 (0 - 99).
|	*z					UINT32			The array containing the resulting number.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLmDivide
UINT32 OctApiLmDivide(UINT32 * n,UINT32 * d,UINT32 * q,UINT32 * r,USHORT ndqrlen)
{
	/* Proceedure for division:*/
	/* 	r = n*/
	/*		q = 0*/
	/*		shift = initial_denominator_shift (for upper '1's to be in same bit position).*/
	/*		d <<= shift;*/
	/* Start loop:*/
	/*		compare  r and d*/
	/*		if r > d then*/
	/*			r -= d;*/
	/*			write a '1' to bit "shift" of array q.*/
	/*		end if;*/
	/*		if shift == 0 then*/
	/*			return;*/
	/*		else*/
	/*			shift--;*/
	/*			d>>=1;*/
	/*			goto "Start loop:"*/
	/*		end if;*/

	UINT32 i;
	UINT32 result;
	USHORT shift,n_msb,d_msb;
	USHORT neg;
	USHORT ConditionFlag = TRUE;

	/* 	r = n*/
	for(i=0;i<=ndqrlen;i++)
		*(r+i) = *(n+i);

	/*		q = 0*/
	for(i=0;i<=ndqrlen;i++)
		*(q+i) = 0;

	/*		shift = initial_denominator_shift (for upper '1's to be in same bit position).*/
	result = OctApiLmGetMsb(d,ndqrlen,&d_msb);
	if (result != GENERIC_OK) return(result);

	result = OctApiLmGetMsb(n,ndqrlen,&n_msb);
	if (result != GENERIC_OK) return(result);

	if (d_msb == 0xFFFF) /* Division by 0.*/
		return(OCTAPI_LM_DIVISION_BY_ZERO);

	if (n_msb == 0xFFFF) /* 0/n, returns 0 R 0.*/
		return(GENERIC_OK);

	if (n_msb < d_msb) 	/* x/y, where x is smaller than y, returns 0 R x.*/
		return(GENERIC_OK);

	shift = (USHORT)( n_msb - d_msb );

	/* Shift d to match n highest bit position.*/
	result = OctApiLmShiftn(d,ndqrlen,TRUE,shift);
	if (result != GENERIC_OK) return(result);

	/* Start loop:*/
	while( ConditionFlag == TRUE )
	{
		/*		compare  r and d*/
		result = OctApiLmCompare(r,ndqrlen,d,ndqrlen,&neg);
		if (result != GENERIC_OK) return(result);

		if (neg == FALSE) /* Subtraction can be done(do it).*/
		{
			/*			r -= d;*/
			result = OctApiLmSubtract(r,ndqrlen,d,ndqrlen,r,ndqrlen,&neg);
			if (result != GENERIC_OK) return(result);

			/*			write a '1' to bit "shift" of array q.*/
			*(q+(shift/32)) |= (UINT32)0x1 << (shift%32);
		}

		/*		if shift == 0 then*/
		/*			return;*/
		if (shift == 0) return(GENERIC_OK);

		/*			shift--;*/
		/*			d>>=1;*/
		/*			goto "Start loop:"*/
		shift--;
		OctApiLmShiftRight1(d,ndqrlen);
	}

	return(GENERIC_OK);
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		octapi_lm_shifright1.
|
|	Description:	The function is for internal use only.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	N/A.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLmShiftRight1
UINT32 OctApiLmShiftRight1(UINT32 * a,USHORT alen)
{
	UINT32 i;

	/* Start with lower long and move up by one long each time,*/
	/* shifting each long to the right by one bit. The upper bit*/
	/* of the next long will have to be concatenated each time a*/
	/* loop is executed. For the last long, leave the highest bit*/
	/* intact.*/
	for(i=0;i<alen;i++)
	{
		*(a+i)>>=1; /*  Shift long by one to the right.*/
		*(a+i)|=*(a+i+1)<<31;
	}
	*(a+alen)>>=1; /*  Shift last long, leaving it's highest bit at 0.*/

	return(GENERIC_OK);
}
#endif

/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLmShiftn.
|
|	Description:	The function is for internal use only.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	N/A.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLmShiftn
UINT32 OctApiLmShiftn(UINT32 * a,USHORT alen,USHORT shiftleft,USHORT shiftn)
{
	UINT32 i;
	USHORT long_offset;
	USHORT bit_offset;

	long_offset = (USHORT)( shiftn / 32 );
	bit_offset = (USHORT)( shiftn % 32 );

	if (shiftleft == TRUE) /* Shift left.*/
	{
		for(i=alen;i<=alen;i--)
		{
			/* Fill upper bits of long.*/
			if (i >= long_offset)
				*(a+i) = *(a+i-long_offset) << bit_offset;
			else
				*(a+i) = 0;

			/* Fill lower bits of long.*/
			if (i > long_offset && bit_offset != 0)
				*(a+i) |= *(a+i-long_offset-1) >> (32-bit_offset);
		}
	}
	else /* Shift right.*/
	{
		for(i=0;i<=alen;i++)
		{
			/* Fill lower bits of long.*/
			if ((alen-i) >= long_offset)
				*(a+i) = *(a+i+long_offset) >> bit_offset;
			else
				*(a+i) = 0;

			/* Fill upper bits of long.*/
			if ((alen-i) > long_offset && bit_offset != 0)
				*(a+i) |= *(a+i+long_offset+1) << (32-bit_offset);

		}
	}

	return(GENERIC_OK);
}
#endif


/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*\
|	API UTILITIES
|
|	Function:		OctApiLmGetMsb.
|
|	Description:	The function is for internal use only.
|
|  -----------------------------------------------------------------------  
|  |   Variable        |     Type     |          Description                
|  -----------------------------------------------------------------------  
|	N/A.
|
\*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*/
#if !SKIP_OctApiLmGetMsb
UINT32 OctApiLmGetMsb(UINT32 * a,USHORT alen,USHORT * msb_pos)
{
	UINT32 i,j;
	UINT32 x;

	for(i=alen;i<=alen;i--)
	{
		if (*(a+i) == 0) continue;

      x = *(a+i);
		for(j=31;j<=31;j--)
		{
			/* Test for bit being '1'.*/
			if ((x & 0x80000000) != 0)
			{
				*msb_pos=(USHORT)(j+(32*i));
				return(GENERIC_OK);
			}

			/* Shift bit one bit position, and try again.*/
			x<<=1;
		}
	}

	/* MSB not found.*/
	*msb_pos = 0xFFFF;

	return(GENERIC_OK);
}
#endif
