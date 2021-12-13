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

// libsodium for random number generator seed
#include <sodium.h>

// protos
#include "mt64.h"
S2 memory_bounds (S8 start, S8 offset_access);

static U1 init_libsodium = 1;

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

	sp = stpopd ((U1 *) &doubleval, sp, sp_top);
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

// string to number convertions ===============================================

U1 *string_to_int (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 value ALIGN;
	S8 string_addr ALIGN;
	char *endp;

	sp = stpopi ((U1 *) &string_addr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_to_int ERROR: stack corrupt!\n");
		return (NULL);
	}

	// convert
	value = strtoll ((const char *) &data[string_addr], &endp, 10);

	// return value
	sp = stpushi (value, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("string_to_int: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *string_to_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 value ALIGN;
	S8 string_addr ALIGN;
	char *endp;

	sp = stpopi ((U1 *) &string_addr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_to_double ERROR: stack corrupt!\n");
		return (NULL);
	}

	value = strtod ((const char *) &data[string_addr], &endp);

	// return value
	sp = stpushd (value, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("string_to_double: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

// ============================================================================

U1 *acosdouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 value ALIGN;
	F8 returnval ALIGN;

	sp = stpopd ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("acosdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = acos (value);

	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("acosdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *asindouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 value ALIGN;
	F8 returnval ALIGN;

	sp = stpopd ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("asindouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = asin (value);

	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("asindouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *atandouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 value ALIGN;
	F8 returnval ALIGN;

	sp = stpopd ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("atandouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = atan (value);

	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("atandouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *atan2double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 xvalue ALIGN;
	F8 yvalue ALIGN;
	F8 returnval ALIGN;

	sp = stpopd ((U1 *) &yvalue, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("atan2double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopd ((U1 *) &xvalue, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("atan2double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = atan2 (yvalue, xvalue);

	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("atan2double: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *cosdouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 value ALIGN;
	F8 returnval ALIGN;

	sp = stpopd ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cosdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = cos (value);

	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("cosdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *coshdouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 value ALIGN;
	F8 returnval ALIGN;

	sp = stpopd ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("coshdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = cosh (value);

	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("coshdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *sindouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 value ALIGN;
	F8 returnval ALIGN;

	sp = stpopd ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("sindouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = sin (value);

	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("sindouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *sinhdouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 value ALIGN;
	F8 returnval ALIGN;

	sp = stpopd ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("sinhdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = sinh (value);

	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("sinhdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *tanhdouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 value ALIGN;
	F8 returnval ALIGN;

	sp = stpopd ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("tanhdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = tanh (value);

	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("tanhdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *expdouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 value ALIGN;
	F8 returnval ALIGN;

	sp = stpopd ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("expdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = exp (value);

	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("expdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *frexpdouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 xvalue ALIGN;
	S8 exponent ALIGN;
	F8 returnval ALIGN;

	sp = stpopd ((U1 *) &xvalue, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("frexpdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = frexp (xvalue, (int *) &exponent);

	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("frexpdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpushi (exponent, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("frexpdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *ldexpdouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 xvalue ALIGN;
	S8 exponent ALIGN;
	F8 returnval ALIGN;

	sp = stpopi ((U1 *) &exponent, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ldexpdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopd ((U1 *) &xvalue, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ldexpdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = ldexp (xvalue, exponent);

	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("ldexpdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *log10double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 value ALIGN;
	F8 returnval ALIGN;

	sp = stpopd ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("log10double ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = log10 (value);

	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("log10double: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *modfdouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 xvalue ALIGN;
	F8 intpart ALIGN;
	F8 returnval ALIGN;

	sp = stpopd ((U1 *) &xvalue, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("modfdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = modf (xvalue, &intpart);

	sp = stpushd (intpart, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("modfdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("modfdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *powdouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 xvalue ALIGN;
	F8 yvalue ALIGN;
	F8 returnval ALIGN;

	sp = stpopd ((U1 *) &yvalue, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("powdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopd ((U1 *) &xvalue, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("powdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = pow (xvalue, yvalue);

	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("powdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *ceildouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 value ALIGN;
	F8 returnval ALIGN;

	sp = stpopd ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ceildouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = ceil (value);

	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("ceildouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *fabsdouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 value ALIGN;
	F8 returnval ALIGN;

	sp = stpopd ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("fabsdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fabs (value);

	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("fabsdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *floordouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 value ALIGN;
	F8 returnval ALIGN;

	sp = stpopd ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("floordouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = floor (value);

	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("floordouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *fmoddouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 xvalue ALIGN;
	F8 yvalue ALIGN;
	F8 returnval ALIGN;

	sp = stpopd ((U1 *) &yvalue, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("fmoddouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopd ((U1 *) &xvalue, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("fmoddouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fmod (xvalue, yvalue);

	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("fmoddouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *sqrtdouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 value ALIGN;
	F8 returnval ALIGN;

	sp = stpopd ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("sqrtdouble: ERROR: stack corrupt!\n");
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

	sp = stpopd ((U1 *) &value, sp, sp_top);
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

	sp = stpopd ((U1 *) &value, sp, sp_top);
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
	U1 *startnum_ptr = (U1 *) &startnum;
	S8 i ALIGN;
	U1 buffer[8];

	sp = stpopi ((U1 *) &startnum, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rand_init: ERROR: stack corrupt!\n");
		return (NULL);
	}

	// libsodium random number seed init
	if (init_libsodium == 1)
	{
		if (sodium_init () < 0)
		{
			// error can't init lib sodium
			printf ("rand_init: ERROR: can't initialize lib sodium!\n");
			return (NULL);
		}
		else
		{
			init_libsodium = 0;		// library init done!!!
		}
	}

	randombytes_buf (buffer, sizeof (buffer));

	// generate random 64 bit startnum
	for (i = 0; i < 8; i++)
	{
		*startnum_ptr = buffer[i];
		startnum_ptr++;
	}

	// initialize pseudo random number generator with strong seed
	init_genrand64 (startnum);
	return (sp);
}

U1 *rand_int (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 rand_int ALIGN;

	rand_int = genrand64_int64 ();

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

	rand_double = genrand64_real1 ();

	sp = stpushd (rand_double, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("rand_double: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *rand_int_max (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// return S8 int of random range from 0 - max_int

	S8 rand_int ALIGN;
	S8 rand_max ALIGN;
	S8 rand_d ALIGN;

	sp = stpopi ((U1 *) &rand_max, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rand_init_max: ERROR: stack corrupt!\n");
		return (NULL);
	}

	rand_d = genrand64_int64 ();
	rand_int = (S8) rand_d % (rand_max + 1);

	if (rand_int < 0)
	{
		rand_int = rand_int * -1;
	}

	// printf ("rand: %lli\n", rand_int);

	// rand() % (max_number + 1 - minimum_number) + minimum_number
	// output = min + (rand() % static_cast<int>(max - min + 1))

	sp = stpushi (rand_int, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("rand_int_max: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *double_rounded_string (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
  	S8 deststr_len ALIGN;
	S8 number_of_digits ALIGN;
	F8 number ALIGN;
	S8 deststraddr ALIGN;
	S8 ret ALIGN;
	S8 offset ALIGN;

	sp = stpopi ((U1 *) &deststr_len, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("double_rounded_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &deststraddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("double_rounded_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &number_of_digits, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("double_rounded_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

  	sp = stpopd ((U1 *) &number, sp, sp_top);
  	if (sp == NULL)
  	{
  		// error
  		printf ("double_rounded_string: ERROR: stack corrupt!\n");
  		return (NULL);
  	}

	if (number_of_digits < 0 || number_of_digits > 10)
	{
		printf ("double_rounded_string: ERROR: number of digits out of range!\n");
  		return (NULL);
	}

	offset = number_of_digits;
	
	#if BOUNDSCHECK && ! __MACH__
	if (memory_bounds (deststraddr, offset) != 0)
	{
		printf ("double_rounded_string: ERROR dest string overflow!\n");
		return (NULL);
	}
	#endif

	switch (number_of_digits)
	{
		case 1:
			ret = snprintf ((char *) &data[deststraddr], deststr_len, "%.1lf", number);
			break;

		case 2:
			ret = snprintf ((char *) &data[deststraddr], deststr_len, "%.2lf", number);
			break;

		case 3:
			ret = snprintf ((char *) &data[deststraddr], deststr_len, "%.3lf", number);
			break;

		case 4:
			ret = snprintf ((char *) &data[deststraddr], deststr_len, "%.4lf", number);
			break;

		case 5:
			ret = snprintf ((char *) &data[deststraddr], deststr_len, "%.5lf", number);
			break;

		case 6:
			ret = snprintf ((char *) &data[deststraddr], deststr_len, "%.6lf", number);
			break;

		case 7:
			ret = snprintf ((char *) &data[deststraddr], deststr_len, "%.7lf", number);
			break;

		case 8:
			ret = snprintf ((char *) &data[deststraddr], deststr_len, "%.8lf", number);
			break;

		case 9:
			ret = snprintf ((char *) &data[deststraddr], deststr_len, "%.9lf", number);
			break;

		case 10:
			ret = snprintf ((char *) &data[deststraddr], deststr_len, "%.10lf", number);
			break;
	}

	if (! (ret >= 0 || ret < deststr_len))
	{
		printf ("double_rounded_string: ERROR: can't convert to string!\n");
		return (NULL);
	}

	return (sp);
}
