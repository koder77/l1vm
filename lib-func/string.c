/*
 * This file string.c is part of L1vm.
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

#include "../include/global.h"

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

S2 searchstr (U1 *str, U1 *srchstr, S2 start, S2 end, U1 case_sens)
{
	/* replaces the old buggy code */

	// S2 pos = -1, str_len, srchstr_len, i = 0, new_end = 0;
	S2 pos = -1, str_len, i = 0, new_end = 0;
	str_len = strlen_safe ((const char *) str, MAXLINELEN);
	// srchstr_len = strlen_safe ((const char *) srchstr, MAXLINELEN);

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

void convtabs (U1 *str)
{
	S2 i, end;

	end = strlen_safe ((const char *) str, MAXLINELEN) - 1;

	for (i = 0; i <= end; i++)
	{
		if (str[i] == '\t')
		{
			str[i] = ' ';
		}
	}
}

S2 strip_end_commas (U1 *str)
{
	S2 end;

	end = strlen_safe ((const char *) str, MAXLINELEN) - 1;
	// printf ("strip: '%s'", str);
	if (str[0] == '@')
	{
		// inside data block, dont strip string
		return (0);
	}
	if (str[end - 2] ==',' && str[end - 1] == ' ')
	{
		// printf ("strip_end_commas found comma at line end!\n");
		str[end - 2] = '\0';
	}
	return (1);
}
