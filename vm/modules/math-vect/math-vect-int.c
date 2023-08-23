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

struct data_info data_info[MAXDATAINFO];
S8 data_info_ind;

S2 init_memory_bounds (struct data_info *data_info_orig, S8 data_info_ind_orig)
{
	memcpy (&data_info, &data_info_orig, sizeof (data_info_orig));
	data_info_ind = data_info_ind_orig;

	return (0);
}

// 64 bit array min/max value of array functions
U1 *mvect_min_int (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 i ALIGN = 0;
	S8 offset ALIGN = 8;
	S8 number ALIGN = 0;
	S8 min ALIGN = 0;
	S8 *src_ptr;

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_min_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_min_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_min_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_min_int ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_min_int ERROR: src overflow!\n");
		return (NULL);
	}
	#endif

	src_ptr = (S8 *) &data[array_data_src_ptr + (i * offset)];
	min = *src_ptr;

	for (i = start; i <= end; i++)
	{
		src_ptr = (S8 *) &data[array_data_src_ptr + (i * offset)];
		number = *src_ptr;
        if (number < min)
		{
			min = number;
		}
	}

	sp = stpushi (min, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("mvect_min_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

U1 *mvect_max_int (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 i ALIGN = 0;
	S8 offset ALIGN = 8;
	S8 number ALIGN = 0;
	S8 max ALIGN = 0;
	S8 *src_ptr;

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_max_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_max_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_max_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_max_int ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_max_int ERROR: src overflow!\n");
		return (NULL);
	}
	#endif

	src_ptr = (S8 *) &data[array_data_src_ptr + (i * offset)];
	max = *src_ptr;

	for (i = start; i <= end; i++)
	{
		src_ptr = (S8 *) &data[array_data_src_ptr + (i * offset)];
		number = *src_ptr;
        if (number > max)
		{
			max = number;
		}
	}

	sp = stpushi (max, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("mvect_max_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

// sort functions ==============================================================
int compare_int_inc (const void* a, const void* b)
{
    S8 arg1 = *(const S8*) a;
    S8 arg2 = *(const S8*) b;

    if (arg1 < arg2) return -1;
    if (arg1 > arg2) return 1;
    return 0;

    // return (arg1 > arg2) - (arg1 < arg2); // possible shortcut
    // return arg1 - arg2; // erroneous shortcut (fails if INT_MIN is present)
}

int compare_int_dec (const void* a, const void* b)
{
    S8 arg1 = *(const S8*) a;
    S8 arg2 = *(const S8*) b;

    if (arg1 > arg2) return -1;
    if (arg1 < arg2) return 1;
    return 0;

    // return (arg1 > arg2) - (arg1 < arg2); // possible shortcut
    // return arg1 - arg2; // erroneous shortcut (fails if INT_MIN is present)
}

U1 *mvect_sort_int_inc (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;
	S8 *src_ptr;

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sort_int_inc: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sort_int_inc: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_sort_int_inc ERROR: src overflow!\n");
		return (NULL);
	}
	#endif

	src_ptr = (S8 *) &data[array_data_src_ptr];

    qsort (src_ptr, end + 1, sizeof (S8), compare_int_inc);
	return (sp);
}

U1 *mvect_sort_int_dec (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 end ALIGN;
	S8 offset ALIGN = 8;
	S8 *src_ptr;

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sort_int_dec: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_sort_int_dec: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_sort_int_dec ERROR: src overflow!\n");
		return (NULL);
	}
	#endif

	src_ptr = (S8 *) &data[array_data_src_ptr];

    qsort (src_ptr, end + 1, sizeof (S8), compare_int_dec);
	return (sp);
}

// array average calculation
U1 *mvect_average_int (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 array_data_src_ptr ALIGN;
	S8 start ALIGN;
	S8 end ALIGN;
	S8 i ALIGN = 0;
	S8 offset ALIGN = 8;
	S8 number ALIGN = 0;
	S8 sum ALIGN = 0;
	F8 average ALIGN = 0.0;
	S8 *src_ptr;

	sp = stpopi ((U1 *) &end, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_average_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_average_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_src_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mvect_average_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (array_data_src_ptr, start * offset) != 0)
	{
		printf ("mvect_average_int ERROR: src overflow!\n");
		return (NULL);
	}
	if (memory_bounds (array_data_src_ptr, end * offset) != 0)
	{
		printf ("mvect_average_int ERROR: src overflow!\n");
		return (NULL);
	}
	#endif

	for (i = start; i <= end; i++)
	{
		src_ptr = (S8 *) &data[array_data_src_ptr + (i * offset)];
		number = *src_ptr;

		sum = sum + number;
	}

	// calculate average
	average = (F8) sum / (end + 1);

	sp = stpushd (average, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("mvect_avrage_int: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
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
