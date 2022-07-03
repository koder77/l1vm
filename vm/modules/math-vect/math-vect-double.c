/*
 * This file math-vect-double.c is part of L1vm.
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

#include "../../../include/global.h"
#include <math.h>

// protos
U1 *stpushb (U1 data, U1 *sp, U1 *sp_bottom);
U1 *stpopb (U1 *data, U1 *sp, U1 *sp_top);
U1 *stpushi (S8 data, U1 *sp, U1 *sp_bottom);
U1 *stpopi (U1 *data, U1 *sp, U1 *sp_top);
U1 *stpushd (F8 data, U1 *sp, U1 *sp_bottom);
U1 *stpopd (U1 *data, U1 *sp, U1 *sp_top);

extern S2 memory_bounds (S8 start, S8 offset_access);


// 64 bit double floating point functions
// math vector functions two arrays
U1 *mvect_add_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 i ALIGN;
	S8 offset ALIGN = 8;
	F8 number ALIGN = 0.0;
	F8 *src_ptr;
	F8 *dst_ptr;

	// get args from stack
	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_add_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopd ((U1 *) &number, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_add_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_add_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_add_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_add_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK 
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_add_double ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_add_double ERROR: src overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_dst_ptr, start * offset) != 0)
	{
		printf ("mvect_add_double ERROR: dst overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0)
	{
		printf ("mvect_add_double ERROR: dst overflow!\n");
		return (NULL);
	}
	#endif

	for (i = start; i <= end; i++)
	{
		src_ptr = (F8 *) &data[array_data_src_ptr + (i * offset)];
		dst_ptr = (F8 *) &data[array_data_dst_ptr + (i * offset)];
		*dst_ptr = *src_ptr + number;
	}

	return (sp);
}

U1 *mvect_sub_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 i ALIGN;
	S8 offset ALIGN = 8;
	F8 number ALIGN = 0.0;
	F8 *src_ptr;
	F8 *dst_ptr;

	// get args from stack
	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sub_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopd ((U1 *) &number, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sub_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sub_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sub_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sub_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK 
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_sub_double ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_sub_double ERROR: src overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_dst_ptr, start * offset) != 0)
	{
		printf ("mvect_sub_double ERROR: dst overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0)
	{
		printf ("mvect_sub_double ERROR: dst overflow!\n");
		return (NULL);
	}
	#endif

	for (i = start; i <= end; i++)
	{
		src_ptr = (F8 *) &data[array_data_src_ptr + (i * offset)];
		dst_ptr = (F8 *) &data[array_data_dst_ptr + (i * offset)];
		*dst_ptr = *src_ptr - number;
	}

	return (sp);
}

U1 *mvect_mul_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 i ALIGN;
	S8 offset ALIGN = 8;
	F8 number ALIGN = 0.0;
	F8 *src_ptr;
	F8 *dst_ptr;

	// get args from stack
	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_mul_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopd ((U1 *) &number, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_mul_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_mul_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_mul_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_mul_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK 
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_mul_double ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_mul_double ERROR: src overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_dst_ptr, start * offset) != 0)
	{
		printf ("mvect_mul_double ERROR: dst overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0)
	{
		printf ("mvect_mul_double ERROR: dst overflow!\n");
		return (NULL);
	}
	#endif

	for (i = start; i <= end; i++)
	{
		src_ptr = (F8 *) &data[array_data_src_ptr + (i * offset)];
		dst_ptr = (F8 *) &data[array_data_dst_ptr + (i * offset)];
		*dst_ptr = *src_ptr * number;
	}

	return (sp);
}

U1 *mvect_div_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 i ALIGN;
	S8 offset ALIGN = 8;
	F8 number ALIGN = 0.0;
	F8 *src_ptr;
	F8 *dst_ptr;

	// get args from stack
	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_div_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopd ((U1 *) &number, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_div_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_div_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_div_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_div_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	// sense check for division
	if (number == 0.0)
	{
		printf ("mvect_div_double: ERROR division by zero!\n");
		return (NULL);
	}

	#if BOUNDSCHECK 
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_div_double ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_div_double ERROR: src overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_dst_ptr, start * offset) != 0)
	{
		printf ("mvect_div_double ERROR: dst overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0)
	{
		printf ("mvect_div_double ERROR: dst overflow!\n");
		return (NULL);
	}
	#endif

	for (i = start; i <= end; i++)
	{
		src_ptr = (F8 *) &data[array_data_src_ptr + (i * offset)];
		dst_ptr = (F8 *) &data[array_data_dst_ptr + (i * offset)];
		*dst_ptr = *src_ptr / number;
	}

	return (sp);
}

// math vector functions three arrays  ========================================
U1 *mvect_add_double_array (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_src2_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 i ALIGN;
	S8 offset ALIGN = 8;
	F8 *src_ptr;
	F8 *src2_ptr;
	F8 *dst_ptr;

	// get args from stack
	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_add_double_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_add_double_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_add_double_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src2_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_add_double_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_add_double_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK 
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_add_double_array ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_add_double_array ERROR: src overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_src2_ptr, start * offset) != 0)
	{
		printf ("mvect_add_double_array ERROR: src2 overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src2_ptr, end * offset) != 0)
	{
		printf ("mvect_add_double_array ERROR: src2 overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_dst_ptr, start * offset) != 0)
	{
		printf ("mvect_add_double_array ERROR: dst overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0)
	{
		printf ("mvect_add_double_array ERROR: dst overflow!\n");
		return (NULL);
	}
	#endif

	for (i = start; i <= end; i++)
	{
		src_ptr = (F8 *) &data[array_data_src_ptr + (i * offset)];
		src2_ptr = (F8 *) &data[array_data_src2_ptr + (i * offset)];
		dst_ptr = (F8 *) &data[array_data_dst_ptr + (i * offset)];
		*dst_ptr = *src_ptr + *src2_ptr;
	}

	return (sp);
}

U1 *mvect_sub_double_array (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_src2_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 i ALIGN;
	S8 offset ALIGN = 8;
	F8 *src_ptr;
	F8 *src2_ptr;
	F8 *dst_ptr;

	// get args from stack
	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sub_double_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sub_double_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sub_double_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src2_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sub_double_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sub_double_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK 
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_sub_double_array ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_sub_double_array ERROR: src overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_src2_ptr, start * offset) != 0)
	{
		printf ("mvect_sub_double_array ERROR: src2 overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src2_ptr, end * offset) != 0)
	{
		printf ("mvect_sub_double_array ERROR: src2 overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_dst_ptr, start * offset) != 0)
	{
		printf ("mvect_sub_double_array ERROR: dst overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0)
	{
		printf ("mvect_sub_double_array ERROR: dst overflow!\n");
		return (NULL);
	}
	#endif

	for (i = start; i <= end; i++)
	{
		src_ptr = (F8 *) &data[array_data_src_ptr + (i * offset)];
	    src2_ptr = (F8 *) &data[array_data_src2_ptr + (i * offset)];
	    dst_ptr = (F8 *) &data[array_data_dst_ptr + (i * offset)];
	    *dst_ptr = *src_ptr - *src2_ptr;
	}

	return (sp);
}

U1 *mvect_mul_double_array (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_src2_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 i ALIGN;
	S8 offset ALIGN = 8;
	F8 *src_ptr;
	F8 *src2_ptr;
	F8 *dst_ptr;

	// get args from stack
	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_mul_double_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_mul_double_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_mul_double_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src2_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_mul_double_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_mul_double_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK 
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_mul_double_array ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_mul_double_array ERROR: src overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_src2_ptr, start * offset) != 0)
	{
		printf ("mvect_mul_double_array ERROR: src2 overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src2_ptr, end * offset) != 0)
	{
		printf ("mvect_mul_double_array ERROR: src2 overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_dst_ptr, start * offset) != 0)
	{
		printf ("mvect_mul_double_array ERROR: dst overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0)
	{
		printf ("mvect_mul_double_array ERROR: dst overflow!\n");
		return (NULL);
	}
	#endif

	for (i = start; i <= end; i++)
	{
		src_ptr = (F8 *) &data[array_data_src_ptr + (i * offset)];
	    src2_ptr = (F8 *) &data[array_data_src2_ptr + (i * offset)];
	    dst_ptr = (F8 *) &data[array_data_dst_ptr + (i * offset)];
	    *dst_ptr = *src_ptr * *src2_ptr;
	}

	return (sp);
}

U1 *mvect_div_double_array (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_src2_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 i ALIGN;
	S8 offset ALIGN = 8;
	F8 *src_ptr;
	F8 *src2_ptr;
	F8 *dst_ptr;

	// get args from stack
	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_div_double_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_div_double_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_div_double_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src2_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_div_double_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_div_double_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK 
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_div_double_array ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_div_double_array ERROR: src overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_src2_ptr, start * offset) != 0)
	{
		printf ("mvect_div_double_array ERROR: src2 overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src2_ptr, end * offset) != 0)
	{
		printf ("mvect_div_double_array ERROR: src2 overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_dst_ptr, start * offset) != 0)
	{
		printf ("mvect_div_double_array ERROR: dst overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0)
	{
		printf ("mvect_div_double_array ERROR: dst overflow!\n");
		return (NULL);
	}
	#endif

	for (i = start; i <= end; i++)
	{
		src_ptr = (F8 *) &data[array_data_src_ptr + (i * offset)];
	    src2_ptr = (F8 *) &data[array_data_src2_ptr + (i * offset)];
	    dst_ptr = (F8 *) &data[array_data_dst_ptr + (i * offset)];

		// sense check for division
		if (*src2_ptr == 0.0)
		{
			printf ("mvect_div_double_array: ERROR division by zero!\n");
			return (NULL);
		}

	    *dst_ptr = *src_ptr / *src2_ptr;
	}

	return (sp);
}
