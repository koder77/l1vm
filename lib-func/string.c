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
	/* checked: ok! */

	S2 i, j = 0, pos = -1, str_len, srchstr_len;
	U1 ok = FALSE, check = TRUE;
	S2 new_end;
	U1 new_str, new_srchstr;

	str_len = strlen_safe ((const char *) str, MAXLINELEN);
	srchstr_len = strlen_safe ((const char *) srchstr, MAXLINELEN);

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

	while (! ok)
	{
		if (case_sens)
		{
			if (str[i] == srchstr[j])
			{
				pos = i;

				/* found start of searchstring, checking now */

				if (srchstr_len > 1)
				{
					for (j = j + 1; j <= srchstr_len - 1; j++)
					{
						if (i < new_end)
						{
							i++;
						}

						if (str[i] != srchstr[j]) check = FALSE;
					}
				}
				if (check)
				{
					ok = TRUE;
				}
				else
				{
					pos = -1;
				}
			}
			if (i < new_end)
			{
				i++;
			}
			else
			{
				ok = TRUE;
			}
		}
		else
		{
			new_str = str[i];
			new_srchstr = srchstr[j];

			if (str[i] >= 97 && str[i] <= 122)
			{
				new_str = str[i] - 32;
			}
			if (srchstr[j] >= 97 && srchstr[j] <= 122)
			{
				new_srchstr = srchstr[j] - 32;
			}

			if (new_str == new_srchstr)
			{
				pos = i;

				/* found start of searchstring, checking now */

				if (srchstr_len > 1)
				{
					for (j = j + 1; j <= srchstr_len - 1; j++)
					{
						if (i < new_end)
						{
							i++;
						}

						new_str = str[i];
						new_srchstr = srchstr[j];

						if (str[i] >= 97 && str[i] <= 122)
						{
							new_str = str[i] - 32;
						}
						if (srchstr[j] >= 97 && srchstr[j] <= 122)
						{
							new_srchstr = srchstr[j] - 32;
						}

						if (new_str != new_srchstr) check = FALSE;
					}
				}
				if (check)
				{
					ok = TRUE;
				}
				else
				{
					pos = -1;
				}
			}
			if (i < new_end)
			{
				i++;
			}
			else
			{
				ok = TRUE;
			}
		}
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
