/*
 * This file gmp.c is part of L1vm.
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
#include <iostream>
#include <mpreal.h>

#include "../../../include/stack.h"

using mpfr::mpreal;
using std::cout;
using std::endl;


#define MAX_FLOAT_NUM 256

static mpreal mpf_float[MAX_FLOAT_NUM];


size_t strlen_safe (const char * str, int maxlen)
{
	 long long int i = 0;

	 while (1)
	 {
	 	if (str[i] != '\0')
		{
			i++;
		}
		else
		{
			return (i);
		}
		if (i > maxlen)
		{
			return (0);
		}
	}
}

extern "C" U1 *mp_set_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index ALIGN;
	S8 numstring_address ALIGN;
	S8 num_base ALIGN;

	sp = stpopi ((U1 *) &float_index, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_set_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &num_base, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_set_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &numstring_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_set_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index >= MAX_FLOAT_NUM)
	{
		printf ("mp_set_float: ERROR float index out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	// printf ("DEBUG: mp_set_float: '%s'\n", (const char *) &data[numstring_address]);

	mpf_float[float_index] = mpfr::mpreal ((const char *) &data[numstring_address]);
	return (sp);
}


// print float number =========================================================

extern "C" U1 *mp_print_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_format_address ALIGN;
	S8 precision_out ALIGN;

	sp = stpopi ((U1 *) &precision_out, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_print_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_format_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_print_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_print_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index_x >= MAX_FLOAT_NUM)
	{
		printf ("mp_print_float: ERROR float index x out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	cout.precision (precision_out);
	cout << mpf_float[float_index_x];

	return (sp);
}

S8 get_float_string (U1 *numstr, U1 numlen, S8 float_index, S8 precision)
{
	S8 i ALIGN;
	S8 j ALIGN;
	mp_exp_t exponent;
	U1 new_numstr[100000];
	S8 strlen_num ALIGN;

	mpfr_get_str ((char *) &new_numstr, &exponent, 10, precision, mpf_float[float_index].mpfr_srcptr(), GMP_RNDN);
	strlen_num = strlen_safe ((const char*) new_numstr, 99999);
	if (strlen_num + 2 > numlen)
	{
		// error to little space in output numstr string!
		return (1);
	}

	exponent++;
	j = 0;
	for (i = 0; i < strlen_num; i++)
	{
		if (new_numstr[i] == '-')
		{
			exponent++;
		}

		if (i == exponent - 1)
		{
			numstr[j] = '.';
			j++;
		}
		numstr[j] = new_numstr[i];
		j++;
	}

	// check if there are zeroes at end of string
	// and cut off them
	// read string from end to beginning
	for (i = strlen_num; i >= 0; i--)
	{
		// printf ("numstr: %c\n", numstr[i]);

		if (numstr[i] == '0')
		{
			numstr[i] = '\0';
		}
		else
		{
			break;
		}
	}
	return (0);
}

extern "C" U1 *mp_prints_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index ALIGN;
	S8 numstring_address_dest ALIGN;
	S8 numstring_len ALIGN;
	S8 float_format_address ALIGN;
	S8 precision ALIGN;

	sp = stpopi ((U1 *) &precision, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_prints_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_format_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_prints_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &numstring_len, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_prints_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &numstring_address_dest, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_prints_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_prints_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index >= MAX_FLOAT_NUM)
	{
		printf ("mp_prints_float: ERROR float index out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (get_float_string (&data[numstring_address_dest], numstring_len, float_index, precision) != 0)
	{
		printf ("mp_prints_float: ERROR output string overflow!\n");
		return (NULL);
	}

	return (sp);
}

// cleanup ====================================================================
extern "C" U1 *mp_cleanup (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	mpfr_free_cache ();
	return (sp);
}

// calculate float ============================================================

extern "C" U1 *mp_add_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_y ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_add_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_add_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_add_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index_x >= MAX_FLOAT_NUM)
	{
		printf ("mp_add_float: ERROR float index x out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_y >= MAX_FLOAT_NUM)
	{
		printf ("mp_add_float: ERROR float index y out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_res >= MAX_FLOAT_NUM)
	{
		printf ("mp_add_float: ERROR float index res out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	mpf_float[float_index_res] = mpf_float[float_index_x] + mpf_float[float_index_y];
	return (sp);
}

extern "C" U1 *mp_sub_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_y ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_sub_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_sub_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_sub_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index_x >= MAX_FLOAT_NUM)
	{
		printf ("mp_sub_float: ERROR float index x out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_y >= MAX_FLOAT_NUM)
	{
		printf ("mp_sub_float: ERROR float index y out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_res >= MAX_FLOAT_NUM)
	{
		printf ("mp_sub_float: ERROR float index res out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	mpf_float[float_index_res] = mpf_float[float_index_x] - mpf_float[float_index_y];
	return (sp);
}

extern "C" U1 *mp_mul_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_y ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_mul_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_mul_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_mul_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index_x >= MAX_FLOAT_NUM)
	{
		printf ("mp_mul_float: ERROR float index x out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_y >= MAX_FLOAT_NUM)
	{
		printf ("mp_mul_float: ERROR float index y out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_res >= MAX_FLOAT_NUM)
	{
		printf ("mp_mul_float: ERROR float index res out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	mpf_float[float_index_res] = mpf_float[float_index_x] * mpf_float[float_index_y];
	return (sp);
}

extern "C" U1 *mp_div_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_y ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_div_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_div_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_div_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index_x >= MAX_FLOAT_NUM)
	{
		printf ("mp_div_float: ERROR float index x out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_y >= MAX_FLOAT_NUM)
	{
		printf ("mp_div_float: ERROR float index y out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_res >= MAX_FLOAT_NUM)
	{
		printf ("mp_div_float: ERROR float index res out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	mpf_float[float_index_res] = mpf_float[float_index_x] + mpf_float[float_index_y];
	return (sp);
}

extern "C" U1 *mp_sqr_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_sqr_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_sqr_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_sqr_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = sqr (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_sqrt_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_sqrt_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_sqrt_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_sqrt_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = sqrt (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_cbrt_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_cbrt_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_cbrt_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_cbrt_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = cbrt (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_pow_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_pow_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 a ALIGN;

sp = stpopi ((U1 *) &a, sp, sp_top);
if (sp == NULL)
{
printf ("mp_pow_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (a >= MAX_FLOAT_NUM)
{
printf ("mp_pow_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 b ALIGN;

sp = stpopi ((U1 *) &b, sp, sp_top);
if (sp == NULL)
{
printf ("mp_pow_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (b >= MAX_FLOAT_NUM)
{
printf ("mp_pow_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = pow (mpf_float[a],mpf_float[b]);
return (sp);
}

extern "C" U1 *mp_fabs_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_fabs_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_fabs_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_fabs_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = fabs (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_abs_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_abs_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_abs_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_abs_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = abs (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_dim_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_dim_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 a ALIGN;

sp = stpopi ((U1 *) &a, sp, sp_top);
if (sp == NULL)
{
printf ("mp_dim_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (a >= MAX_FLOAT_NUM)
{
printf ("mp_dim_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 b ALIGN;

sp = stpopi ((U1 *) &b, sp, sp_top);
if (sp == NULL)
{
printf ("mp_dim_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (b >= MAX_FLOAT_NUM)
{
printf ("mp_dim_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = dim (mpf_float[a],mpf_float[b]);
return (sp);
}

extern "C" U1 *mp_log_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_log_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_log_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_log_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = log (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_log2_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_log2_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_log2_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_log2_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = log2 (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_logb_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_logb_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_logb_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_logb_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = logb (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_log10_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_log10_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_log10_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_log10_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = log10 (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_exp2_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_exp2_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_exp2_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_exp2_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = exp2 (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_exp10_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_exp10_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_exp10_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_exp10_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = exp10 (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_log1p_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_log1p_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_log1p_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_log1p_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = log1p (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_expm1_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_expm1_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_expm1_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_expm1_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = expm1 (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_cos_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_cos_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_cos_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_cos_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = cos (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_sin_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_sin_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_sin_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_sin_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = sin (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_tan_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_tan_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_tan_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_tan_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = tan (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_sec_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_sec_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_sec_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_sec_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = sec (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_csc_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_csc_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_csc_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_csc_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = csc (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_cot_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_cot_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_cot_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_cot_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = cot (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_acos_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_acos_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_acos_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_acos_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = acos (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_asin_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_asin_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_asin_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_asin_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = asin (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_atan_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_atan_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_atan_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_atan_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = atan (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_atan2_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_atan2_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 y ALIGN;

sp = stpopi ((U1 *) &y, sp, sp_top);
if (sp == NULL)
{
printf ("mp_atan2_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (y >= MAX_FLOAT_NUM)
{
printf ("mp_atan2_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 x ALIGN;

sp = stpopi ((U1 *) &x, sp, sp_top);
if (sp == NULL)
{
printf ("mp_atan2_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (x >= MAX_FLOAT_NUM)
{
printf ("mp_atan2_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = atan2 (mpf_float[y],mpf_float[x]);
return (sp);
}

extern "C" U1 *mp_acot_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_acot_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_acot_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_acot_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = acot (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_asec_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_asec_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_asec_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_asec_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = asec (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_acsc_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_acsc_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_acsc_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_acsc_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = acsc (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_cosh_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_cosh_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_cosh_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_cosh_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = cosh (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_sinh_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_sinh_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_sinh_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_sinh_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = sinh (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_tanh_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_tanh_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_tanh_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_tanh_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = tanh (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_sech_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_sech_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_sech_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_sech_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = sech (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_csch_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_csch_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_csch_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_csch_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = csch (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_coth_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_coth_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_coth_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_coth_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = coth (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_acosh_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_acosh_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_acosh_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_acosh_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = acosh (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_asinh_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_asinh_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_asinh_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_asinh_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = asinh (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_atanh_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_atanh_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_atanh_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_atanh_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = atanh (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_acoth_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_acoth_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_acoth_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_acoth_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = acoth (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_asech_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_asech_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_asech_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_asech_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = asech (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_acsch_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_acsch_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_acsch_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_acsch_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = acsch (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_hypot_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_hypot_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 x ALIGN;

sp = stpopi ((U1 *) &x, sp, sp_top);
if (sp == NULL)
{
printf ("mp_hypot_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (x >= MAX_FLOAT_NUM)
{
printf ("mp_hypot_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 y ALIGN;

sp = stpopi ((U1 *) &y, sp, sp_top);
if (sp == NULL)
{
printf ("mp_hypot_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (y >= MAX_FLOAT_NUM)
{
printf ("mp_hypot_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = hypot (mpf_float[x],mpf_float[y]);
return (sp);
}

extern "C" U1 *mp_eint_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_eint_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_eint_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_eint_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = eint (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_gamma_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_gamma_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_gamma_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_gamma_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = gamma (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_tgamma_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_tgamma_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_tgamma_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_tgamma_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = tgamma (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_lngamma_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_lngamma_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_lngamma_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_lngamma_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = lngamma (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_zeta_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_zeta_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_zeta_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_zeta_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = zeta (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_erf_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_erf_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_erf_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_erf_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = erf (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_erfc_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_erfc_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_erfc_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_erfc_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = erfc (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_bessely0_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_bessely0_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_bessely0_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_bessely0_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = bessely0 (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_bessely1_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_bessely1_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_bessely1_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_bessely1_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = bessely1 (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_fma_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_fma_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v1 ALIGN;

sp = stpopi ((U1 *) &v1, sp, sp_top);
if (sp == NULL)
{
printf ("mp_fma_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v1 >= MAX_FLOAT_NUM)
{
printf ("mp_fma_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v2 ALIGN;

sp = stpopi ((U1 *) &v2, sp, sp_top);
if (sp == NULL)
{
printf ("mp_fma_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v2 >= MAX_FLOAT_NUM)
{
printf ("mp_fma_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v3 ALIGN;

sp = stpopi ((U1 *) &v3, sp, sp_top);
if (sp == NULL)
{
printf ("mp_fma_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v3 >= MAX_FLOAT_NUM)
{
printf ("mp_fma_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = fma (mpf_float[v1],mpf_float[v2],mpf_float[v3]);
return (sp);
}

extern "C" U1 *mp_fms_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_fms_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v1 ALIGN;

sp = stpopi ((U1 *) &v1, sp, sp_top);
if (sp == NULL)
{
printf ("mp_fms_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v1 >= MAX_FLOAT_NUM)
{
printf ("mp_fms_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v2 ALIGN;

sp = stpopi ((U1 *) &v2, sp, sp_top);
if (sp == NULL)
{
printf ("mp_fms_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v2 >= MAX_FLOAT_NUM)
{
printf ("mp_fms_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v3 ALIGN;

sp = stpopi ((U1 *) &v3, sp, sp_top);
if (sp == NULL)
{
printf ("mp_fms_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v3 >= MAX_FLOAT_NUM)
{
printf ("mp_fms_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = fms (mpf_float[v1],mpf_float[v2],mpf_float[v3]);
return (sp);
}

extern "C" U1 *mp_agm_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_agm_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v1 ALIGN;

sp = stpopi ((U1 *) &v1, sp, sp_top);
if (sp == NULL)
{
printf ("mp_agm_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v1 >= MAX_FLOAT_NUM)
{
printf ("mp_agm_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 v2 ALIGN;

sp = stpopi ((U1 *) &v2, sp, sp_top);
if (sp == NULL)
{
printf ("mp_agm_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v2 >= MAX_FLOAT_NUM)
{
printf ("mp_agm_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = agm (mpf_float[v1],mpf_float[v2]);
return (sp);
}

extern "C" U1 *mp_li2_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_li2_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_li2_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_li2_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = li2 (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_fmod_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_fmod_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 x ALIGN;

sp = stpopi ((U1 *) &x, sp, sp_top);
if (sp == NULL)
{
printf ("mp_fmod_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (x >= MAX_FLOAT_NUM)
{
printf ("mp_fmod_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 y ALIGN;

sp = stpopi ((U1 *) &y, sp, sp_top);
if (sp == NULL)
{
printf ("mp_fmod_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (y >= MAX_FLOAT_NUM)
{
printf ("mp_fmod_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = fmod (mpf_float[x],mpf_float[y]);
return (sp);
}

extern "C" U1 *mp_rec_sqrt_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_rec_sqrt_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_rec_sqrt_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_rec_sqrt_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = rec_sqrt (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_digamma_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_digamma_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_digamma_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_digamma_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = digamma (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_ai_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_ai_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_ai_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_ai_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = ai (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_rint_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_rint_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_rint_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_rint_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = rint (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_ceil_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_ceil_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_ceil_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_ceil_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = ceil (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_floor_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_floor_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_floor_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_floor_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = floor (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_round_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_round_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_round_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_round_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = round (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_trunc_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_trunc_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_trunc_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_trunc_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = trunc (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_rint_ceil_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_rint_ceil_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_rint_ceil_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_rint_ceil_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = rint_ceil (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_rint_floor_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_rint_floor_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_rint_floor_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_rint_floor_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = rint_floor (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_rint_round_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_rint_round_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_rint_round_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_rint_round_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = rint_round (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_rint_trunc_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_rint_trunc_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_rint_trunc_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_rint_trunc_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = rint_trunc (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_frac_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_frac_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 v ALIGN;

sp = stpopi ((U1 *) &v, sp, sp_top);
if (sp == NULL)
{
printf ("mp_frac_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (v >= MAX_FLOAT_NUM)
{
printf ("mp_frac_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = frac (mpf_float[v]);
return (sp);
}

extern "C" U1 *mp_remainder_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_remainder_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 x ALIGN;

sp = stpopi ((U1 *) &x, sp, sp_top);
if (sp == NULL)
{
printf ("mp_remainder_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (x >= MAX_FLOAT_NUM)
{
printf ("mp_remainder_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 y ALIGN;

sp = stpopi ((U1 *) &y, sp, sp_top);
if (sp == NULL)
{
printf ("mp_remainder_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (y >= MAX_FLOAT_NUM)
{
printf ("mp_remainder_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = remainder (mpf_float[x],mpf_float[y]);
return (sp);
}

extern "C" U1 *mp_nexttoward_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_nexttoward_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 x ALIGN;

sp = stpopi ((U1 *) &x, sp, sp_top);
if (sp == NULL)
{
printf ("mp_nexttoward_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (x >= MAX_FLOAT_NUM)
{
printf ("mp_nexttoward_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 y ALIGN;

sp = stpopi ((U1 *) &y, sp, sp_top);
if (sp == NULL)
{
printf ("mp_nexttoward_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (y >= MAX_FLOAT_NUM)
{
printf ("mp_nexttoward_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = nexttoward (mpf_float[x],mpf_float[y]);
return (sp);
}

extern "C" U1 *mp_nextabove_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_nextabove_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 x ALIGN;

sp = stpopi ((U1 *) &x, sp, sp_top);
if (sp == NULL)
{
printf ("mp_nextabove_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (x >= MAX_FLOAT_NUM)
{
printf ("mp_nextabove_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = nextabove (mpf_float[x]);
return (sp);
}

extern "C" U1 *mp_nextbelow_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_nextbelow_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 x ALIGN;

sp = stpopi ((U1 *) &x, sp, sp_top);
if (sp == NULL)
{
printf ("mp_nextbelow_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (x >= MAX_FLOAT_NUM)
{
printf ("mp_nextbelow_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = nextbelow (mpf_float[x]);
return (sp);
}

extern "C" U1 *mp_fmax_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_fmax_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 x ALIGN;

sp = stpopi ((U1 *) &x, sp, sp_top);
if (sp == NULL)
{
printf ("mp_fmax_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (x >= MAX_FLOAT_NUM)
{
printf ("mp_fmax_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 y ALIGN;

sp = stpopi ((U1 *) &y, sp, sp_top);
if (sp == NULL)
{
printf ("mp_fmax_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (y >= MAX_FLOAT_NUM)
{
printf ("mp_fmax_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = fmax (mpf_float[x],mpf_float[y]);
return (sp);
}

extern "C" U1 *mp_fmin_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
S8 float_index_res ALIGN;

sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
if (sp == NULL)
{
printf ("mp_fmin_float: ERROR: stack corrupt!\n");
return (NULL);
}

S8 x ALIGN;

sp = stpopi ((U1 *) &x, sp, sp_top);
if (sp == NULL)
{
printf ("mp_fmin_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (x >= MAX_FLOAT_NUM)
{
printf ("mp_fmin_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


S8 y ALIGN;

sp = stpopi ((U1 *) &y, sp, sp_top);
if (sp == NULL)
{
printf ("mp_fmin_float: ERROR: stack corrupt!\n");
return (NULL);
}

if (y >= MAX_FLOAT_NUM)
{
printf ("mp_fmin_float: ERROR float index x out of range! Must be 0 - %i", MAX_FLOAT_NUM);
return (NULL);
}


mpf_float[float_index_res] = fmin (mpf_float[x],mpf_float[y]);
return (sp);
}
