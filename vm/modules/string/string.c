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
	S8 slen;
	S8 straddr;

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
	S8 strsourceaddr, strdestaddr;

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
	S8 strsourceaddr, strdestaddr;

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
