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

// for regular expression function
#include <regex.h>

// protos

S2 memory_bounds (S8 start, S8 offset_access);


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

// string functions ------------------------------------

U1 *string_len (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 slen ALIGN;
	S8 straddr ALIGN;

	sp = stpopi ((U1 *) &straddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_len: ERROR: stack corrupt!\n");
		return (NULL);
	}

	slen = strlen_safe ((char *) &data[straddr], MAXLINELEN);

	sp = stpushi (slen, sp, sp_bottom);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_len: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *string_copy (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 strsourceaddr ALIGN;
	S8 strdestaddr ALIGN;
	S8 offset ALIGN;

	sp = stpopi ((U1 *) &strsourceaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_copy: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_copy: ERROR: stack corrupt!\n");
		return (NULL);
	}

	offset = strlen_safe ((char *) &data[strsourceaddr], MAXLINELEN);

	#if BOUNDSCHECK
	if (memory_bounds (strdestaddr, offset) != 0)
	{
		printf ("string_copy: ERROR: dest string overflow!\n");
		return (NULL);
	}
	#endif

	strcpy ((char *) &data[strdestaddr], (const char *) &data[strsourceaddr]);
	return (sp);
}

U1 *string_cat (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 strsourceaddr ALIGN;
	S8 strdestaddr ALIGN;
	S8 offset_src ALIGN;
	S8 offset_dst ALIGN;

	sp = stpopi ((U1 *) &strsourceaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_cat: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_cat: ERROR: stack corrupt!\n");
		return (NULL);
	}

	offset_src = strlen_safe ((char *) &data[strsourceaddr], MAXLINELEN);
	offset_dst = strlen_safe ((char *) &data[strdestaddr], MAXLINELEN);

	#if BOUNDSCHECK
	if (memory_bounds (strdestaddr, offset_src + offset_dst) != 0)
	{
		printf ("string_cat: ERROR: dest string overflow!\n");
		return (NULL);
	}
	#endif

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
		// ERROR:
		printf ("string_int64_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_int64_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &num, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_int64_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (strdestaddr, str_len) != 0)
	{
		printf ("string_int64_to_string: ERROR: dest string overflow!\n");
		return (NULL);
	}
	#endif

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
		// ERROR:
		printf ("string_byte_to_hexstring: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_byte_to_hexstring: ERROR: stack corrupt!\n");
		return (NULL);
	}

	//sp = stpopb ((U1 *) &num, sp, sp_top);
	sp = stpopi ((U1 *) &num, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_byte_to_hexstring: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (strdestaddr, str_len) != 0)
	{
		printf ("string_byte_to_hexstring: ERROR: dest string overflow!\n");
		return (NULL);
	}
	#endif

	if (snprintf ((char *) &data[strdestaddr], str_len, "%02x", num) <= 0)
	{
		printf ("string_byte_to_hexstring: ERROR: conversion failed!\n");
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
		// ERROR:
		printf ("string_double_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_double_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopd ((U1 *) &num, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_double_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (strdestaddr, str_len) != 0)
	{
		printf ("string_double_to_string: ERROR: dest string overflow!\n");
		return (NULL);
	}
	#endif

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
		// ERROR:
		printf ("string_bytenum_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	// sp = stpopb ((U1 *) &num, sp, sp_top);
	sp = stpopi ((U1 *) &num, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_bytenum_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (strdestaddr, 1) != 0)
	{
		printf ("string_bytenum_to_string: ERROR: dest string overflow!\n");
		return (NULL);
	}
	#endif

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
		// ERROR:
		printf ("string_string_to_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &string_len, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_string_to_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &index, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_string_to_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_string_to_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strsrcaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_string_to_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	string_len_src = strlen_safe ((const char *) &data[strsrcaddr], MAXLINELEN);
	if (string_len_src > string_len)
	{
		// ERROR:
		printf ("string_string_to_array: ERROR: source string overflow!\n");
		return (NULL);
	}

	index_real = index * string_len;
	if (index_real < 0 || index_real > array_size)
	{
		// ERROR:
		printf ("string_string_to_array: ERROR: destination array overflow!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (strdestaddr + index_real, string_len_src) != 0)
	{
		printf ("string_string_to_array: ERROR: dest string overflow!\n");
		return (NULL);
	}
	#endif

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
		// ERROR:
		printf ("string_array_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &string_len, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_array_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &index, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_array_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_array_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strsrcaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_array_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	index_real = index * string_len;
	if (index_real < 0 || index_real > array_size)
	{
		// ERROR:
		printf ("string_array_to_string: ERROR: source array overflow!\n");
		return (NULL);
	}

	string_len_src = strlen_safe ((const char *) &data[strsrcaddr + index_real], MAXLINELEN);
	if (string_len_src > string_len)
	{
		// ERROR:
		printf ("string_array_to_string: ERROR: source string overflow!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (strdestaddr, string_len_src) != 0)
	{
		printf ("string_array_to_string: ERROR: dest string overflow!\n");
		return (NULL);
	}
	#endif

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
		// ERROR:
		printf ("string_left: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_left: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strsourceaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_left: ERROR: stack corrupt!\n");
		return (NULL);
	}

	strsource_len = strlen_safe ((const char *) &data[strsourceaddr], MAXLINELEN);
	if (str_len < 1 || str_len > strsource_len)
	{
		// ERROR:
		printf ("string_left: ERROR: source array overflow!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (strdestaddr, str_len) != 0)
	{
		printf ("string_left: ERROR: dest string overflow!\n");
		return (NULL);
	}
	#endif

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
		// ERROR:
		printf ("string_right: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_right: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strsourceaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_right: ERROR: stack corrupt!\n");
		return (NULL);
	}

	strsource_len = strlen_safe ((const char *) &data[strsourceaddr], MAXLINELEN);
	if (str_len < 1 || str_len > strsource_len)
	{
		// ERROR:
		printf ("string_right: ERROR: source array overflow!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (strdestaddr, str_len) != 0)
	{
		printf ("string_right: ERROR: dest string overflow!\n");
		return (NULL);
	}
	#endif

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
		// ERROR:
		printf ("string_mid: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_mid: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strsourceaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_mid: ERROR: stack corrupt!\n");
		return (NULL);
	}

	strsource_len = strlen_safe ((const char *) &data[strsourceaddr], MAXLINELEN);
	if (pos < 0 || pos > strsource_len)
	{
		// ERROR:
		printf ("string_mid: ERROR: source array overflow!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (strdestaddr, 1) != 0)
	{
		printf ("string_mid: ERROR: dest string overflow!\n");
		return (NULL);
	}
	#endif

	data[strdestaddr] = data[strsourceaddr + pos];
	data[strdestaddr + 1] = '\0';

	return (sp);
}

U1 *string_to_string (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 strsourceaddr ALIGN;
	S8 strdestaddr ALIGN;
	S8 pos ALIGN;
	S8 strdest_len ALIGN;

	sp = stpopi ((U1 *) &pos, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strsourceaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	strdest_len = strlen_safe ((const char *) &data[strdestaddr], MAXLINELEN);
	if (pos < 0 || pos > strdest_len)
	{
		// ERROR:
		printf ("string_to_string: ERROR: source array overflow!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (strdestaddr, pos) != 0)
	{
		printf ("string_to_string: ERROR: dest string overflow!\n");
		return (NULL);
	}
	#endif

	data[strdestaddr + pos] = data[strsourceaddr];

	return (sp);
}

U1 *string_compare (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 strsourceaddr ALIGN;
	S8 str2addr ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &str2addr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_compare: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strsourceaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_compare: ERROR: stack corrupt!\n");
		return (NULL);
	}

	ret = strcmp ((const char *) &data[strsourceaddr], (const char *) &data[str2addr]);

	// ret = 0 strings are identical
	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_compare: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *stringmem_to_string (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// get string from big mem string pos into a simple string
	// using separator char
	//
	// this is usefull for a txt file loaded into a byte array variable

	S8 strsourceaddr ALIGN;
	S8 strdestaddr ALIGN;
	S8 pos ALIGN;
	S8 destindex ALIGN;;
	S8 destsize ALIGN;
	S8 stringmemsize ALIGN;
	S8 separator ALIGN; 	// char which marks end of current big memory string
	S8 i ALIGN;

	sp = stpopi ((U1 *) &separator, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("stringmem_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &destsize, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("stringmem_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &stringmemsize, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("stringmem_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &pos, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("stringmem_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("stringmem_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strsourceaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("stringmem_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	// set dest string to zero
	for (i = 0; i < destsize; i++)
	{
		data[strdestaddr + i] = '\0';
	}

	// try to get string
	destindex = 0;
	while (1)
	{
		if (pos < stringmemsize)
		{
			if (destindex < destsize - 1)
			{
				#if BOUNDSCHECK
				if (memory_bounds (strsourceaddr, pos) != 0)
				{
					printf ("stringmem_to_string: ERROR: dest string overflow!\n");
					pos = -1;
					break;
				}
				#endif

				if (data[strsourceaddr + pos] == separator)
				{
					data[strdestaddr + destindex] = '\0';
					break;
				}
				data[strdestaddr + destindex] = data[strsourceaddr + pos];
				pos++;
				destindex++;
			}
			else
			{
				// end of dest string
				data[strdestaddr + destindex] = '\0';
				break;
			}
		}
		else
		{
			// end of mem string
			data[strdestaddr + destindex] = '\0';
			break;
		}
	}
	// return (pos)

	sp = stpushi (pos, sp, sp_bottom);
	if (sp == NULL)
	{
		// ERROR:
		printf ("stringmem_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

U1 *stringmem_search_string (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// get string from big mem string pos into a simple string
	// using separator char
	//
	// this is usefull for a txt file loaded into a byte array variable

	S8 strsourceaddr ALIGN;
	S8 strsearchaddr ALIGN;
	S8 pos ALIGN;
	S8 destsize ALIGN;
	S8 stringmemsize ALIGN;
	S8 startpos ALIGN;		// startpos to search string start in memory string
	S8 searchlen ALIGN;
	S8 i ALIGN;
	U1 found_string;
	S8 found_pos ALIGN;

	sp = stpopi ((U1 *) &destsize, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("stringmem_search_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &stringmemsize, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("stringmem_search_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &startpos, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("stringmem_search_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strsearchaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("stringmem_search_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strsourceaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("stringmem_search_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	searchlen = strlen_safe ((const char *) &data[strsearchaddr], 256);

	i = 0;
	while (1)
	{
		if (data[strsourceaddr + i] == data[strsearchaddr])
		{
			found_string = 1; found_pos = i;
			for (pos = 1; pos < searchlen; pos++)
			{
				i++;
				if (data[strsourceaddr + i] != data[strsearchaddr + pos])
				{
					found_string = 0;
				}
			}
			if (found_string == 1)
			{
				// all chars are equal, return string position

				// return found_pos

				printf ("stringmem_search_string: found string at pos: %lli\n", found_pos);

				sp = stpushi (found_pos, sp, sp_bottom);
				if (sp == NULL)
				{
					// ERROR:
					printf ("stringmem_search_string: ERROR: stack corrupt!\n");
					return (NULL);
				}

				return (sp);
			}
		}
		i++;
		if (i >= stringmemsize)
		{
			break;
		}
	}

	// return -1 -> string not found
	sp = stpushi (-1, sp, sp_bottom);
	if (sp == NULL)
	{
		// ERROR:
		printf ("stringmem_search_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

U1 *string_regex (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// regular expression match search

	S8 strsourceaddr ALIGN;
	S8 strregexaddr ALIGN;
	S8 ret ALIGN;

	regex_t regex;

	sp = stpopi ((U1 *) &strregexaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_regex: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strsourceaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_regex: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (regcomp (&regex, (const char *) &data[strregexaddr], REG_EXTENDED) != 0)
	{
		printf ("string_regex: ERROR can't compile regex: '%s'\n", (const char *) &data[strregexaddr]);

		// return -1, error exit!!
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// ERROR:
			printf ("string_regex: ERROR: stack corrupt!\n");
			return (NULL);
		}
	}

	// do regex search
	ret = regexec (&regex, (const char *) &data[strsourceaddr], 0, NULL, 0);

	// return regex return code
	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_regex: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

// normal string search functions =============================================

S2 searchstr (U1 *str, U1 *srchstr, S2 start, S2 end)
{
	/* replaces the old buggy code */

	S2 pos = -1, str_len, i = 0, new_end = 0;
	str_len = strlen_safe ((const char *) str, MAXLINELEN);

	U1 *sptr;
	U1 *startptr;

	if (start < 0 || start > str_len - 1)
	{
		i = 0;
	}
	else
	{
		i = start;
	}

	if (end == 0)
	{
		new_end = str_len - 1;
	}
	else
	{
		new_end = end;
	}

	startptr = str;
	if (start > 0)
	{
		startptr = startptr + start;
	}

	sptr = (U1 *) strstr ((const char *) startptr, (const char *) srchstr);
	if (sptr)
	{
		// get position of substring
		pos = sptr - startptr;
	}

	return (pos);
}

U1 *string_search (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 strsourceaddr ALIGN;
	S8 strsearchaddr ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &strsearchaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_search: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strsourceaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_search: ERROR: stack corrupt!\n");
		return (NULL);
	}

	ret = searchstr (&data[strsourceaddr], &data[strsearchaddr], 0, 0);

	// return search return code
	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_search: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}
