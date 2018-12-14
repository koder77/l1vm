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
#include "../../../include/stack.h"

// protos
void init_genrand (unsigned long s);
long genrand_int31 (void);
double genrand_real1 (void);


// math functions --------------------------------------

U1 *int2double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 intval ALIGN;
	F8 doubleval ALIGN;

	//printf ("int2double sp: %lli\n", (S8) sp);

	sp = stpopi ((U1 *) &intval, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("int2double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	doubleval = (F8) intval;

	//printf ("int2double sp: %lli\n", (S8) sp);

	sp = stpushd (doubleval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("int2double: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *double2int (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 intval ALIGN;
	F8 doubleval ALIGN;

	sp = stpopi ((U1 *) &doubleval, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("double2int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	intval = (S8) ceil (doubleval);

	sp = stpushi (intval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("double2int: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *sqrtdouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 value ALIGN;
	F8 returnval ALIGN;

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

U1 *logdouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 value ALIGN;
	F8 returnval ALIGN;

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

U1 *log2double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 value ALIGN;
	F8 returnval ALIGN;
	F8 m_ln2 ALIGN = 0.69314718055994530942;

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("log2double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = log (value) / m_ln2;

	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("log2double: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

// random number generator 

U1 *rand_init (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 startnum ALIGN;

	sp = stpopi ((U1 *) &startnum, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rand_init: ERROR: stack corrupt!\n");
		return (NULL);
	}

	init_genrand (startnum);
	return (sp);
}

U1 *rand_int (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 rand_int ALIGN;

	rand_int = genrand_int31 ();

	sp = stpushi (rand_int, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("rand_int: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *rand_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 rand_double ALIGN;

	rand_double = genrand_real1 ();

	sp = stpushd (rand_double, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("rand_double: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}
