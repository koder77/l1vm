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
