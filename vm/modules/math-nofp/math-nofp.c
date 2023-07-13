/*
 * This file math-nofp.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2021
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

 // floating point math for CPUs without floating point opcodes

#include "../../../include/global.h"
#include <math.h>
#include "../../../include/stack.h"

#include "fp16.h"

// protos
#include "mt64.h"
// S2 memory_bounds (S8 start, S8 offset_access);

2 init_memory_bounds (struct data_info *data_info_orig, S8 data_info_ind_orig)
{
	return (0);
}


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
	S8 value ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("acosdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_acos (value);

	sp = stpushi (returnval, sp, sp_bottom);
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
	S8 value ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("asindouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_asin (value);

	sp = stpushi (returnval, sp, sp_bottom);
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
	S8 value ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("atandouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_atan (value);

	sp = stpushi (returnval, sp, sp_bottom);
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
	S8 xvalue ALIGN;
	S8 yvalue ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &yvalue, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("atan2double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &xvalue, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("atan2double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_atan2 (yvalue, xvalue);

	sp = stpushi (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("acosdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *cosdouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 value ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cosdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_cos (value);

	sp = stpushi (returnval, sp, sp_bottom);
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
	S8 value ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("coshdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_cosh (value);

	sp = stpushi (returnval, sp, sp_bottom);
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
	S8 value ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("sindouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_sin (value);

	sp = stpushi (returnval, sp, sp_bottom);
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
	S8 value ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("sinhdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_sinh (value);

	sp = stpushi (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("sinhdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *tandouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 value ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("tandouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_tan (value);

	sp = stpushi (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("tandouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *tanhdouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 value ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("tanhdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_tanh (value);

	sp = stpushi (returnval, sp, sp_bottom);
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
	S8 value ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("expdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_exp (value);

	sp = stpushi (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("expdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *log10double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 value ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("log10double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_log (value);

	sp = stpushi (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("log10double: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *fmoddouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 xvalue ALIGN;
	S8 yvalue ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &yvalue, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("fmoddouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &xvalue, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("fmoddouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_mod (xvalue, yvalue);

	sp = stpushi (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("fmoddouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *powdouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 xvalue ALIGN;
	S8 yvalue ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &yvalue, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("powdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &xvalue, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("powdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_pow (xvalue, yvalue);

	sp = stpushi (returnval, sp, sp_bottom);
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
	S8 value ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ceildouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_ceil (value);

	sp = stpushi (returnval, sp, sp_bottom);
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
	S8 value ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("fabsdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_abs (value);

	sp = stpushi (returnval, sp, sp_bottom);
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
	S8 value ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("floordouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_floor (value);

	sp = stpushi (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("floordouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *sqrtdouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 value ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("sqrtdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_sqrt (value);

	sp = stpushi (returnval, sp, sp_bottom);
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
	S8 value ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("logdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_log (value);

	sp = stpushi (returnval, sp, sp_bottom);
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
	S8 value ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("log2double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_log2 (value);

	sp = stpushi (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("logdouble: ERROR: stack corrupt!\n");
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
	S8 rand_fp ALIGN;

	rand_double = genrand64_real1 ();
	rand_fp = fp16_double2fp (rand_double);

	sp = stpushi (rand_fp, sp, sp_bottom);
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

// standard math +, -, *, /

U1 *adddouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 xvalue ALIGN;
	S8 yvalue ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &yvalue, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("adddouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &xvalue, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("adddouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_add (xvalue, yvalue);

	sp = stpushi (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("adddouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *subdouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 xvalue ALIGN;
	S8 yvalue ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &yvalue, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("subouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &xvalue, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("subdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_sub (xvalue, yvalue);

	sp = stpushi (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("subdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *muldouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 xvalue ALIGN;
	S8 yvalue ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &yvalue, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("muldouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &xvalue, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("muldouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_mul (xvalue, yvalue);

	sp = stpushi (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("mulouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *divdouble (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 xvalue ALIGN;
	S8 yvalue ALIGN;
	S8 returnval ALIGN;

	sp = stpopi ((U1 *) &yvalue, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("divdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &xvalue, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("divdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_div (xvalue, yvalue);

	sp = stpushi (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("divdouble: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

// conversion functions =======================================================

U1 *double2fp (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	F8 value ALIGN;
	S8 returnval ALIGN;

	sp = stpopd ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("double2fp: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_double2fp (value);

	sp = stpushi (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("double2fp: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *fp2double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 value ALIGN;
	F8 returnval ALIGN;

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("fp2double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	returnval = fp16_fp2double (value);

	sp = stpushd (returnval, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("fp2double: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}
