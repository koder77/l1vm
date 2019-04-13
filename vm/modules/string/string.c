/*
 * This file string.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2018
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
#include "../../../include/stack.h"

// string functions ------------------------------------

U1 *string_len (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 slen ALIGN;
	S8 straddr ALIGN;

	sp = stpopi ((U1 *) &straddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_len: ERROR: stack corrupt!\n");
		return (NULL);
	}

	slen = strlen ((char *) &data[straddr]);

	sp = stpushi (slen, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("string_len: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *string_copy (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 strsourceaddr ALIGN;
	S8 strdestaddr ALIGN;

	sp = stpopi ((U1 *) &strsourceaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_copy: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_copy: ERROR: stack corrupt!\n");
		return (NULL);
	}

	strcpy ((char *) &data[strdestaddr], (const char *) &data[strsourceaddr]);
	return (sp);
}

U1 *string_cat (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 strsourceaddr ALIGN;
	S8 strdestaddr ALIGN;

	sp = stpopi ((U1 *) &strsourceaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_cat: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_cat: ERROR: stack corrupt!\n");
		return (NULL);
	}

	strcat ((char *) &data[strdestaddr], (const char *) &data[strsourceaddr]);
	return (sp);
}

U1 *string_int64_to_string (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 num ALIGN;
	S8 strdestaddr ALIGN;
	S8 str_len ALIGN;

	sp = stpopi ((U1 *) &str_len, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_int64_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_int64_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &num, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_int64_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (snprintf ((char *) &data[strdestaddr], str_len, "%lli", num) <= 0)
	{
		printf ("string_int64_to_string: ERROR: conversion failed!\n");
		return (NULL);
	}

	return (sp);
}

U1 *string_byte_to_hexstring (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	U1 num;
	S8 strdestaddr ALIGN;
	S8 str_len ALIGN;

	sp = stpopi ((U1 *) &str_len, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_int64_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_int64_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopb ((U1 *) &num, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_int64_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (snprintf ((char *) &data[strdestaddr], str_len, "%02x", num) <= 0)
	{
		printf ("string_int64_to_string: ERROR: conversion failed!\n");
		return (NULL);
	}

	return (sp);
}

U1 *string_double_to_string (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 strdestaddr ALIGN;
	S8 str_len ALIGN;
	F8 num ALIGN;

	sp = stpopi ((U1 *) &str_len, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_double_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_double_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopd ((U1 *) &num, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_double_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (snprintf ((char *) &data[strdestaddr], str_len, "%.10lf", num) <= 0)
	{
		printf ("string_double_to_string: ERROR: conversion failed!\n");
		return (NULL);
	}

	return (sp);
}

U1 *string_bytenum_to_string (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 strdestaddr ALIGN;
	U1 num;

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_bytenum_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopb ((U1 *) &num, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_bytenum_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	data[strdestaddr] = num;
	data[strdestaddr + 1] = '\0';

	return (sp);
}

// string array functions
U1 *string_string_to_array (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: sourceaddr, destaddr, index, stringlen, arraysize
	S8 strdestaddr ALIGN;
	S8 strsrcaddr ALIGN;
	S8 index ALIGN;
	S8 array_size ALIGN;
	S8 string_len ALIGN;
	S8 index_real ALIGN;
	S8 string_len_src ALIGN;

	sp = stpopi ((U1 *) &array_size, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_string_to_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &string_len, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_string_to_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &index, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_string_to_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_string_to_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strsrcaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_string_to_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	string_len_src = strlen ((const char *) &data[strsrcaddr]);
	if (string_len_src > string_len)
	{
		// error
		printf ("string_string_to_array: ERROR: source string overflow!\n");
		return (NULL);
	}

	index_real = index * string_len;
	if (index_real < 0 || index_real > array_size)
	{
		// error
		printf ("string_string_to_array: ERROR: destination array overflow!\n");
		return (NULL);
	}

	// printf ("DEBUG: string_string_to_array: '%s'\n", &data[strsrcaddr]);

	strcpy ((char *) &data[strdestaddr + index_real], (const char *) &data[strsrcaddr]);

	// printf ("DEBUG: string_string_to_array: array dest: '%s'\n", &data[strdestaddr + index_real]);
	return (sp);
}

U1 *string_array_to_string (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// args: sourceaddr, destaddr, index, stringlen, arraysize
	S8 strdestaddr ALIGN;
	S8 strsrcaddr ALIGN;
	S8 index ALIGN;
	S8 array_size ALIGN;
	S8 string_len ALIGN;
	S8 index_real ALIGN;
	S8 string_len_src ALIGN;

	sp = stpopi ((U1 *) &array_size, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_array_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &string_len, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_array_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &index, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_array_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_array_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strsrcaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_array_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	index_real = index * string_len;
	if (index_real < 0 || index_real > array_size)
	{
		// error
		printf ("string_array_to_string: ERROR: source array overflow!\n");
		return (NULL);
	}

	string_len_src = strlen ((const char *) &data[strsrcaddr + index_real]);
	if (string_len_src > string_len)
	{
		// error
		printf ("string_array_to_string: ERROR: source string overflow!\n");
		return (NULL);
	}

	strcpy ((char *) &data[strdestaddr], (const char *) &data[strsrcaddr + index_real]);
	return (sp);
}

U1 *string_left (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// get left side n chars of string
	S8 strsourceaddr ALIGN;
	S8 strdestaddr ALIGN;
	S8 str_len ALIGN;
	S8 strsource_len ALIGN;
	S8 i ALIGN;

	sp = stpopi ((U1 *) &str_len, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_left: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_left: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strsourceaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_left: ERROR: stack corrupt!\n");
		return (NULL);
	}

	strsource_len = strlen ((const char *) &data[strsourceaddr]);
	if (str_len < 1 || str_len > strsource_len)
	{
		// error
		printf ("string_left: ERROR: source array overflow!\n");
		return (NULL);
	}

	for (i = 0; i < str_len; i++)
	{
		data[strdestaddr + i] = data[strsourceaddr + i];
	}
	data[strdestaddr + i] = '\0';

	return (sp);
}

U1 *string_right (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// get right side n chars of string
	S8 strsourceaddr ALIGN;
	S8 strdestaddr ALIGN;
	S8 str_len ALIGN;
	S8 strsource_len ALIGN;
	S8 i ALIGN;
	S8 j ALIGN;

	sp = stpopi ((U1 *) &str_len, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_right: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_right: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strsourceaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_right: ERROR: stack corrupt!\n");
		return (NULL);
	}

	strsource_len = strlen ((const char *) &data[strsourceaddr]);
	if (str_len < 1 || str_len > strsource_len)
	{
		// error
		printf ("string_right: ERROR: source array overflow!\n");
		return (NULL);
	}

	i = 0;
	for (j = strsource_len - str_len; j < strsource_len; j++)
	{
		data[strdestaddr + i] = data[strsourceaddr + j];
		i++;
	}
	data[strdestaddr + i] = '\0';

	return (sp);
}

U1 *string_mid (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// get char at given position in source string
	S8 strsourceaddr ALIGN;
	S8 strdestaddr ALIGN;
	S8 pos ALIGN;
	S8 strsource_len ALIGN;

	sp = stpopi ((U1 *) &pos, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_mid: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_mid: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strsourceaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_mid: ERROR: stack corrupt!\n");
		return (NULL);
	}

	strsource_len = strlen ((const char *) &data[strsourceaddr]);
	if (pos < 1 || pos > strsource_len)
	{
		// error
		printf ("string_mid: ERROR: source array overflow!\n");
		return (NULL);
	}

	data[strdestaddr] = data[strsourceaddr + pos];
	data[strdestaddr + 1] = '\0';

	return (sp);
}

U1 *string_tostring (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// get char at given position in source string
	S8 strsourceaddr ALIGN;
	S8 strdestaddr ALIGN;
	S8 pos ALIGN;
	S8 strdest_len ALIGN;

	sp = stpopi ((U1 *) &pos, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_tostring: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_tostring: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strsourceaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("string_tostring: ERROR: stack corrupt!\n");
		return (NULL);
	}

	strdest_len = strlen ((const char *) &data[strdestaddr]);
	if (pos < 1 || pos > strdest_len)
	{
		// error
		printf ("string_tostring: ERROR: source array overflow!\n");
		return (NULL);
	}

	data[strdestaddr + pos] = data[strsourceaddr];

	return (sp);
}
