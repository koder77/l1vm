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


extern "C" U1 *gmp_set_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index ALIGN;
	S8 numstring_address ALIGN;
	S8 num_base ALIGN;

	sp = stpopi ((U1 *) &float_index, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_set_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &num_base, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_set_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &numstring_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_set_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index >= MAX_FLOAT_NUM)
	{
		printf ("gmp_set_float: ERROR float index out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	// printf ("DEBUG: gmp_set_float: '%s'\n", (const char *) &data[numstring_address]);

	mpf_float[float_index] = mpfr::mpreal ((const char *) &data[numstring_address]);
	return (sp);
}

// calculate float ============================================================

extern "C" U1 *gmp_add_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_y ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_add_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_add_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_add_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index_x >= MAX_FLOAT_NUM)
	{
		printf ("gmp_add_float: ERROR float index x out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_y >= MAX_FLOAT_NUM)
	{
		printf ("gmp_add_float: ERROR float index y out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_res >= MAX_FLOAT_NUM)
	{
		printf ("gmp_add_float: ERROR float index res out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	mpf_float[float_index_res] = mpf_float[float_index_x] + mpf_float[float_index_y];
	return (sp);
}

extern "C" U1 *gmp_sub_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_y ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_sub_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_sub_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_sub_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index_x >= MAX_FLOAT_NUM)
	{
		printf ("gmp_sub_float: ERROR float index x out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_y >= MAX_FLOAT_NUM)
	{
		printf ("gmp_sub_float: ERROR float index y out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_res >= MAX_FLOAT_NUM)
	{
		printf ("gmp_sub_float: ERROR float index res out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	mpf_float[float_index_res] = mpf_float[float_index_x] - mpf_float[float_index_y];
	return (sp);
}

extern "C" U1 *gmp_mul_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_y ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_mul_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_mul_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_mul_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index_x >= MAX_FLOAT_NUM)
	{
		printf ("gmp_mul_float: ERROR float index x out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_y >= MAX_FLOAT_NUM)
	{
		printf ("gmp_mul_float: ERROR float index y out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_res >= MAX_FLOAT_NUM)
	{
		printf ("gmp_mul_float: ERROR float index res out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	mpf_float[float_index_res] = mpf_float[float_index_x] * mpf_float[float_index_y];
	return (sp);
}

extern "C" U1 *gmp_div_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_y ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_div_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_div_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_div_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index_x >= MAX_FLOAT_NUM)
	{
		printf ("gmp_div_float: ERROR float index x out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_y >= MAX_FLOAT_NUM)
	{
		printf ("gmp_div_float: ERROR float index y out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_res >= MAX_FLOAT_NUM)
	{
		printf ("gmp_div_float: ERROR float index res out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	mpf_float[float_index_res] = mpf_float[float_index_x] + mpf_float[float_index_y];
	return (sp);
}

// trigonometry ===============================================================

extern "C" U1 *gmp_cos_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_cos_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_cos_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	mpf_float[float_index_res] = cos (mpf_float[float_index_x]);
	return (sp);
}

extern "C" U1 *gmp_sin_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_sin_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_sin_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	mpf_float[float_index_res] = sin (mpf_float[float_index_x]);
	return (sp);
}

extern "C" U1 *gmp_tan_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_tan_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_tan_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	mpf_float[float_index_res] = tan (mpf_float[float_index_x]);
	return (sp);
}

extern "C" U1 *gmp_sec_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_sec_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_sec_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	mpf_float[float_index_res] = sec (mpf_float[float_index_x]);
	return (sp);
}

extern "C" U1 *gmp_csc_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_csc_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_csc_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	mpf_float[float_index_res] = csc (mpf_float[float_index_x]);
	return (sp);
}

extern "C" U1 *gmp_cot_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_cot_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_cot_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	mpf_float[float_index_res] = cot (mpf_float[float_index_x]);
	return (sp);
}

extern "C" U1 *gmp_acos_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_acos_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_acos_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	mpf_float[float_index_res] = acos (mpf_float[float_index_x]);
	return (sp);
}

extern "C" U1 *gmp_asin_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_asin_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_asin_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	mpf_float[float_index_res] = acos (mpf_float[float_index_x]);
	return (sp);
}

extern "C" U1 *gmp_atan_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_atan_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_atan_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	mpf_float[float_index_res] = atan (mpf_float[float_index_x]);
	return (sp);
}

extern "C" U1 *gmp_atan2_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_y ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_atan2_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_atan2_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_atan2_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	mpf_float[float_index_res] = atan2 (mpf_float[float_index_x], mpf_float[float_index_y]);
	return (sp);
}

extern "C" U1 *gmp_acot_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_acot_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_acot_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	mpf_float[float_index_res] = acot (mpf_float[float_index_x]);
	return (sp);
}

extern "C" U1 *gmp_asec_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_asec_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_asec_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	mpf_float[float_index_res] = asec (mpf_float[float_index_x]);
	return (sp);
}

extern "C" U1 *gmp_acsc_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_acsc_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_acsc_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	mpf_float[float_index_res] = acsc (mpf_float[float_index_x]);
	return (sp);
}

// print float number =========================================================

extern "C" U1 *gmp_print_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_format_address ALIGN;
	S8 precision_out ALIGN;

	sp = stpopi ((U1 *) &precision_out, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_print_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_format_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_print_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_print_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index_x >= MAX_FLOAT_NUM)
	{
		printf ("gmp_print_float: ERROR float index x out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
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
	strlen_num = strlen ((const char*) new_numstr);
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

extern "C" U1 *gmp_prints_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
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
		printf ("gmp_prints_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_format_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_prints_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &numstring_len, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_prints_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &numstring_address_dest, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_prints_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_prints_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index >= MAX_FLOAT_NUM)
	{
		printf ("gmp_prints_float: ERROR float index out of range! Must be 0 - %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (get_float_string (&data[numstring_address_dest], numstring_len, float_index, precision) != 0)
	{
		printf ("gmp_prints_float: ERROR output string overflow!\n");
		return (NULL);
	}

	return (sp);
}

// cleanup ====================================================================
extern "C" U1 *gmp_cleanup (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	mpfr_free_cache ();
	return (sp);
}
