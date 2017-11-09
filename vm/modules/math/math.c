/*
 * This file math.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2017
 * 
 * L1vm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * L1vm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with L1vm.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "../../../include/global.h"
#include <math.h>

// stack operations ---------------------------------------
// byte 

U1 *stpushb (U1 data, U1 *sp, U1 *sp_bottom)
{
	if (sp >= sp_bottom)
	{
		sp--;
		
		*sp = data;
		return (sp);		// success
	}
	else
	{
		// fatal ERROR: stack pointer can't go below address ZERO!
		return (NULL);		// FAIL
	}
}

U1 *stpopb (U1 *data, U1 *sp, U1 *sp_top)
{
	if (sp == sp_top)
	{
		// nothing on stack!! can't pop!!
		return (NULL);		// FAIL
	}
	
	*data = *sp;
	
	sp++;
	return (sp);			// success
}

// quadword

U1 *stpushi (S8 data, U1 *sp, U1 *sp_bottom)
{
	U1 *bptr;
	
	if (sp >= sp_bottom + 8)
	{
		// set stack pointer to lower address
		
		bptr = (U1 *) &data;
		
		sp--;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp = *bptr;
		
		return (sp);			// success
	}
	else
	{
		// fatal ERROR: stack pointer can't go below address ZERO!
		return (NULL);			// FAIL
	}
}

U1 *stpopi (U1 *data, U1 *sp, U1 *sp_top)
{
	if (sp == sp_top)
	{
		// nothing on stack!! can't pop!!
		return (NULL);			// FAIL
	}
	
	data[7] = *sp++;
	data[6] = *sp++;
	data[5] = *sp++;
	data[4] = *sp++;
	data[3] = *sp++;
	data[2] = *sp++;
	data[1] = *sp++;
	data[0] = *sp++;
	
	return (sp);			// success
}

U1 *stpushd (F8 data, U1 *sp, U1 *sp_bottom)
{
	U1 *bptr;
	
	if (sp >= sp_bottom + 8)
	{
		// set stack pointer to lower address
		
		bptr = (U1 *) &data;
		
		sp--;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp = *bptr;
		
		return (sp);			// success
	}
	else
	{
		// fatal ERROR: stack pointer can't go below address ZERO!
		return (NULL);			// FAIL
	}
}


// --------------------------------------------------------

// math functions --------------------------------------

U1 *int2double (U1 *sp, U1 *sp_top, U1 *sp_bottom)
{
	U1 err = 0;
	S8 intval;
	F8 doubleval;
	
	//printf ("int2double sp: %lli\n", (S8) sp);
	
	sp = stpopi ((U1 *) &intval, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
		printf ("int2double: ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	doubleval = (F8) intval;
	
	//printf ("int2double sp: %lli\n", (S8) sp);
	
	sp = stpushd (doubleval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		err = 1;
		printf ("int2double: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *double2int (U1 *sp, U1 *sp_top, U1 *sp_bottom)
{
	U1 err = 0;
	S8 intval;
	F8 doubleval;
	
	sp = stpopi ((U1 *) &doubleval, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
		printf ("double2int: ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	intval = (S8) ceil (doubleval);
	
	sp = stpushi (intval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		err = 1;
		printf ("double2int: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *sqrtdouble (U1 *sp, U1 *sp_top, U1 *sp_bottom)
{
	F8 value, returnval;
	
	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("sqrtdouble ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	returnval = sqrt (value);
	
	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("sqrtdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *logdouble (U1 *sp, U1 *sp_top, U1 *sp_bottom)
{
	F8 value, returnval;
	
	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("logdouble ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	returnval = log (value);
	
	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error 
		printf ("logdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *log2double (U1 *sp, U1 *sp_top, U1 *sp_bottom)
{
	F8 value, returnval;
	F8 m_ln2 = 0.69314718055994530942;
	
	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("log2double ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	returnval = log (value);
	returnval = returnval / m_ln2;
	
	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("log2double: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}
