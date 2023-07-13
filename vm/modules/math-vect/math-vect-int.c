/*
 * This file math-vect-int.c is part of L1vm.
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
#include "../../../include/stack.h"

// protos

extern S2 memory_bounds (S8 start, S8 offset_access);

extern struct data_info data_info[MAXDATAINFO];
extern S8 data_info_ind;

S2 init_memory_bounds (struct data_info *data_info_orig, S8 data_info_ind_orig)
{
	memcpy (&data_info, &data_info_orig, sizeof (data_info_orig));
	data_info_ind = data_info_ind_orig;

	return (0);
}

// 64 bit integer functions
// math vector functions two arrays  ==========================================
U1 *mvect_add_int (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 i ALIGN;
	S8 offset ALIGN = 8;
	S8 number ALIGN = 0;
	S8 *src_ptr;
	S8 *dst_ptr;

	// get args from stack
	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_add_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &number, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_add_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_add_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_add_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_add_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK 
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_add_int ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_add_int ERROR: src overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_dst_ptr, start * offset) != 0)
	{
		printf ("mvect_add_int ERROR: dst overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0)
	{
		printf ("mvect_add_int ERROR: dst overflow!\n");
		return (NULL);
	}
	#endif

	for (i = start; i <= end; i++)
	{
		src_ptr = (S8 *) &data[array_data_src_ptr + (i * offset)];
		dst_ptr = (S8 *) &data[array_data_dst_ptr + (i * offset)];
		*dst_ptr = *src_ptr + number;
	}

	return (sp);
}

U1 *mvect_sub_int (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 i ALIGN;
	S8 offset ALIGN = 8;
	S8 number ALIGN = 0;
	S8 *src_ptr;
	S8 *dst_ptr;

	// get args from stack
	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sub_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &number, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sub_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sub_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sub_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sub_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK 
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_sub_int ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_sub_int ERROR: src overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_dst_ptr, start * offset) != 0)
	{
		printf ("mvect_sub_int ERROR: dst  overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0)
	{
		printf ("mvect_sub_int ERROR: dst overflow!\n");
		return (NULL);
	}
	#endif

	for (i = start; i <= end; i++)
	{
		src_ptr = (S8 *) &data[array_data_src_ptr + (i * offset)];
		dst_ptr = (S8 *) &data[array_data_dst_ptr + (i * offset)];
		*dst_ptr = *src_ptr - number;
	}

	return (sp);
}

U1 *mvect_mul_int (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 i ALIGN;
	S8 offset ALIGN = 8;
	S8 number ALIGN = 0;
	S8 *src_ptr;
	S8 *dst_ptr;

	// get args from stack
	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_mul_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &number, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_mul_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_mul_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_mul_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_mul_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK 
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_mul_int ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_mul_int ERROR: src overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_dst_ptr, start * offset) != 0)
	{
		printf ("mvect_mul_int ERROR: dst overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0)
	{
		printf ("mvect_mul_int ERROR: dst overflow!\n");
		return (NULL);
	}
	#endif

	for (i = start; i <= end; i++)
	{
		src_ptr = (S8 *) &data[array_data_src_ptr + (i * offset)];
		dst_ptr = (S8 *) &data[array_data_dst_ptr + (i * offset)];
		*dst_ptr = *src_ptr * number;
	}

	return (sp);
}

U1 *mvect_div_int (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 i ALIGN;
	S8 offset ALIGN = 8;
	S8 number ALIGN = 0;
	S8 *src_ptr;
	S8 *dst_ptr;

	// get args from stack
	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_div_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &number, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_div_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_div_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_div_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_div_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	// sense check
	if (number == 0)
	{
		printf ("mvect_div_int: ERROR division by zero!\n");
		return (NULL);
	}

	#if BOUNDSCHECK 
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_div_int ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_div_int ERROR: src overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_dst_ptr, start * offset) != 0)
	{
		printf ("mvect_div_int ERROR: dst overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0)
	{
		printf ("mvect_div_int ERROR: dst overflow!\n");
		return (NULL);
	}
	#endif

	for (i = start; i <= end; i++)
	{
		src_ptr = (S8 *) &data[array_data_src_ptr + (i * offset)];
		dst_ptr = (S8 *) &data[array_data_dst_ptr + (i * offset)];
		*dst_ptr = *src_ptr / number;
	}

	return (sp);
}

// math vector functions three arrays  ========================================
U1 *mvect_add_int_array (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_src2_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 i ALIGN;
	S8 offset ALIGN = 8;
	S8 *src_ptr;
	S8 *src2_ptr;
	S8 *dst_ptr;

	// get args from stack
	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_add_int_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_add_int_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_add_int_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src2_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_add_int_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_add_int_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK 
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_add_int_array ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_add_int_array ERROR: src overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_src2_ptr, start * offset) != 0)
	{
		printf ("mvect_add_int_array ERROR: src2 overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src2_ptr, end * offset) != 0)
	{
		printf ("mvect_add_int_array ERROR: src2 overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_dst_ptr, start * offset) != 0)
	{
		printf ("mvect_add_int_array ERROR: dst overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0)
	{
		printf ("mvect_add_int_array ERROR: dst overflow!\n");
		return (NULL);
	}
	#endif

	for (i = start; i <= end; i++)
	{
		src_ptr = (S8 *) &data[array_data_src_ptr + (i * offset)];
		src2_ptr = (S8 *) &data[array_data_src2_ptr + (i * offset)];
		dst_ptr = (S8 *) &data[array_data_dst_ptr + (i * offset)];
		*dst_ptr = *src_ptr + *src2_ptr;
	}

	return (sp);
}

U1 *mvect_sub_int_array (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_src2_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 i ALIGN;
	S8 offset ALIGN = 8;
	S8 *src_ptr;
	S8 *src2_ptr;
	S8 *dst_ptr;

	// get args from stack
	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sub_int_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sub_int_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sub_int_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src2_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sub_int_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sub_int_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK 
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_sub_int_array ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_sub_int_array ERROR: src overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_src2_ptr, start * offset) != 0)
	{
		printf ("mvect_sub_int_array ERROR: src2 overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src2_ptr, end * offset) != 0)
	{
		printf ("mvect_sub_int_array ERROR: src2 overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_dst_ptr, start * offset) != 0)
	{
		printf ("mvect_sub_int_array ERROR: dst overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0)
	{
		printf ("mvect_sub_int_array ERROR: dst overflow!\n");
		return (NULL);
	}
	#endif

	for (i = start; i <= end; i++)
	{
		src_ptr = (S8 *) &data[array_data_src_ptr + (i * offset)];
	    src2_ptr = (S8 *) &data[array_data_src2_ptr + (i * offset)];
	    dst_ptr = (S8 *) &data[array_data_dst_ptr + (i * offset)];
	    *dst_ptr = *src_ptr - *src2_ptr;
	}

	return (sp);
}

U1 *mvect_mul_int_array (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_src2_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 i ALIGN;
	S8 offset ALIGN = 8;
	S8 *src_ptr;
	S8 *src2_ptr;
	S8 *dst_ptr;

	// get args from stack
	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_mul_int_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_mul_int_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_mul_int_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src2_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_mul_int_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_mul_int_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK 
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_mul_int_array ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_mul_int_array ERROR: src overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_src2_ptr, start * offset) != 0)
	{
		printf ("mvect_mul_int_array ERROR: src2 overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src2_ptr, end * offset) != 0)
	{
		printf ("mvect_mul_int_array ERROR: src2 overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_dst_ptr, start * offset) != 0)
	{
		printf ("mvect_mul_int_array ERROR: dst overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0)
	{
		printf ("mvect_mul_int_array ERROR: dst overflow!\n");
		return (NULL);
	}
	#endif

	for (i = start; i <= end; i++)
	{
		src_ptr = (S8 *) &data[array_data_src_ptr + (i * offset)];
	    src2_ptr = (S8 *) &data[array_data_src2_ptr + (i * offset)];
	    dst_ptr = (S8 *) &data[array_data_dst_ptr + (i * offset)];
	    *dst_ptr = *src_ptr * *src2_ptr;
	}

	return (sp);
}

U1 *mvect_div_int_array (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 array_data_src2_ptr ALIGN;
	S8 array_data_dst_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 i ALIGN;
	S8 offset ALIGN = 8;
	S8 *src_ptr;
	S8 *src2_ptr;
	S8 *dst_ptr;

	// get args from stack
	sp = stpopi ((U1 *) &array_data_dst_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_div_int_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_div_int_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_div_int_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src2_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_div_int_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_div_int_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK 
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_div_int_array ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_div_int_array ERROR: src overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_src2_ptr, start * offset) != 0)
	{
		printf ("mvect_div_int_array ERROR: src2 overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src2_ptr, end * offset) != 0)
	{
		printf ("mvect_div_int_array ERROR: src2 overflow!\n");
		return (NULL);
	}

	if (memory_bounds (array_data_dst_ptr, start * offset) != 0)
	{
		printf ("mvect_div_int_array ERROR: dst overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_dst_ptr, end * offset) != 0)
	{
		printf ("mvect_div_int_array ERROR: dst overflow!\n");
		return (NULL);
	}
	#endif

	for (i = start; i <= end; i++)
	{
		src_ptr = (S8 *) &data[array_data_src_ptr + (i * offset)];
	    src2_ptr = (S8 *) &data[array_data_src2_ptr + (i * offset)];
	    dst_ptr = (S8 *) &data[array_data_dst_ptr + (i * offset)];

		if (*src2_ptr == 0)
		{
			printf ("mvect_div_int_array: ERROR division by zero!\n");
			return (NULL);
		}

	    *dst_ptr = *src_ptr / *src2_ptr;
	}

	return (sp);
}
