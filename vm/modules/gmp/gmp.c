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
#include <gmp.h>
#include "../../../include/stack.h"


#define MAX_FLOAT_NUM 256

static mpf_t mpf_float[MAX_FLOAT_NUM];


U1 *gmp_set_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
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

	mpf_init2 (mpf_float[float_index], 64);	// set to zero

	//printf ("mpf_set_float: '%s'\n", (const char *) &data[numstring_address]);

	if (mpf_set_str (mpf_float[float_index], (const char *) &data[numstring_address], num_base) != 0)
	{
		printf ("gmp_set_float: ERROR can't set float number: '%s'\n", &data[numstring_address]);
		return (NULL);
	}
	return (sp);
}

// calculate float ============================================================

U1 *gmp_add_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
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

	mpf_add (mpf_float[float_index_res], mpf_float[float_index_x], mpf_float[float_index_y]);
	return (sp);
}

U1 *gmp_sub_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
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

	mpf_sub (mpf_float[float_index_res], mpf_float[float_index_x], mpf_float[float_index_y]);
	return (sp);
}

U1 *gmp_mul_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
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

	mpf_mul (mpf_float[float_index_res], mpf_float[float_index_x], mpf_float[float_index_y]);
	return (sp);
}

U1 *gmp_div_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
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

	mpf_div (mpf_float[float_index_res], mpf_float[float_index_x], mpf_float[float_index_y]);
	return (sp);
}

// print float number =========================================================

U1 *gmp_print_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_format_address ALIGN;

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

	gmp_printf ((char *) &data[float_format_address], mpf_float[float_index_x]);
	return (sp);
}

U1 *gmp_prints_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index ALIGN;
	S8 numstring_address_dest ALIGN;
	S8 numstring_len ALIGN;
	S8 float_format_address ALIGN;

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

	gmp_snprintf ((char *) &data[numstring_address_dest], numstring_len, (char *) &data[float_format_address], mpf_float[float_index]);
	return (sp);
}

U1 *gmp_sqrt_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
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

	mpf_sqrt (mpf_float[float_index_res], mpf_float[float_index_x]);
	return (sp);
}

U1 *gmp_pow_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8  num_pow ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_div_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &num_pow, sp, sp_top);
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

	mpf_pow_ui (mpf_float[float_index_res], mpf_float[float_index_x], num_pow);
	return (sp);
}

U1 *gmp_neg_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
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

	mpf_neg (mpf_float[float_index_res], mpf_float[float_index_x]);
	return (sp);
}

U1 *gmp_abs_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
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

	mpf_abs (mpf_float[float_index_res], mpf_float[float_index_x]);
	return (sp);
}

U1 *gmp_mul_2exp_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 num ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_div_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &num, sp, sp_top);
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

	mpf_mul_2exp (mpf_float[float_index_res], mpf_float[float_index_x], num);
	return (sp);
}

U1 *gmp_div_2exp_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 num ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("gmp_div_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &num, sp, sp_top);
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

	mpf_div_2exp (mpf_float[float_index_res], mpf_float[float_index_x], num);
	return (sp);
}

// compare ====================================================================

U1 *gmp_cmp_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_y ALIGN;
	S8 ret ALIGN;

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

	ret = mpf_cmp (mpf_float[float_index_x], mpf_float[float_index_y]);
	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("gmp_cmp_float: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}
