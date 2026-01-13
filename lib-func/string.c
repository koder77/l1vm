/*
 * This file string.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (info@midnight-coding.de), 2017
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

size_t strlen_safe (const char *str, S8 maxlen)
{
	 S8 ALIGN i = 0;

	if (str == NULL) return 0;
	if (maxlen <= 0 || maxlen == 1)
	{
		return 0;
	}

	while (i < maxlen - 1) // Direktes Limit in der Schleife
	{
		if (str[i] == '\0') return i;
		i++;
	}
	return 0;
}

S8 searchstr (U1 *str, U1 *srchstr, S2 start, S2 end, U1 case_sens)
{
	/* replaces the old buggy code */
	S8 ALIGN pos = -1;
	S8 ALIGN str_len;
	S8 ALIGN i = 0;

	str_len = strlen_safe ((const char *) str, MAXLINELEN);

	U1 *sptr;
	U1 *startptr;

	if (start < 0 || start > str_len - 1)
	{
		start = 0;
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
		// old code:
		// pos = sptr - startptr;
		pos = start + sptr - startptr;
	}

	return (pos);
}

void convtabs (U1 *str)
{
	S8 ALIGN i;
	S8 ALIGN end;

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
	S8 ALIGN end;

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

S2 check_spaces (U1 *line)
{
	S4 line_len = 0;
	S4 i = 0;
	S4 pos = 0;

	line_len = strlen_safe ((const char *) line, MAXLINELEN);

	// check if (set ) line:
	pos = searchstr (line, (U1 *) "(set", 0, 0, 0);
	if (pos != -1)
	{
		// found set line, exit with ok code
		return (0);
	}

	pos = searchstr (line, (U1 *) "(", 0, 0, 0);
	if (pos == -1)
	{
		// empty line
		return (0);
	}

	for (i = pos + 1; i < line_len - 1; i++)
	{
		if (line[i] == ' ')
		{
			if (line[i + 1] == ' ')
			{
				// found double spaces: error
				return (1);
			}
		}
	}

	// all ok!
	return (0);
}
