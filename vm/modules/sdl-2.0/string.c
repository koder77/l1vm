/*
 * This file string.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2019
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
 *
 * Includes code from "flow" my Nano VM GUI server.
 */

#include "../../../include/global.h"


size_t strlen_safe (const U1 * str, int maxlen)
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

int strright (U1 *dst, U1 *src, int chars)
{
    int i, src_pos, src_len;

    src_len = strlen_safe (src, MAXLINELEN);
    src_pos = src_len - chars;

    if (src_pos < 0)
    {
        dst[0] = '\0';
        return (-1);
    }

    for (i = 0; i < chars + 1; i++)
    {
        dst[i] = src[src_pos];
        src_pos++;
    }

    return (1);
}

int strleft (U1 *dst, U1 *src, int chars)
{
    int i, src_len;

    src_len = strlen_safe (src, MAXLINELEN);

    if (chars > src_len)
    {
        dst[0] = '\0';
        return (-1);
    }

    for (i = 0; i < chars; i++)
    {
        dst[i] = src[i];
    }
    dst[i] = '\0';

    return (1);
}

int strremoveleft (U1 *dst, U1 *src, int pos)
{
    int i, j, src_len;

    src_len = strlen_safe (src, MAXLINELEN);

    if (pos < 1 || pos > src_len)
    {
        dst[0] = '\0';
        return (-1);
    }

    if (pos == src_len)
    {
        /* remove last char */

        for (i = 0; i < src_len - 1; i++)
        {
            dst[i] = src[i];
        }
        dst[i] = '\0';
    }
    else
    {
        for (i = 0; i < pos - 1; i++)
        {
            dst[i] = src[i];
        }

        for (j = pos; j < src_len; j++)
        {
            dst[i] = src[j];
            i++;
        }
        dst[i] = '\0';
    }

    return (1);
}

int strremoveright (U1 *dst, U1 *src, int pos)
{
    int i, j, src_len;

    src_len = strlen_safe (src, MAXLINELEN);

    if (pos < 0 || pos > src_len)
    {
        dst[0] = '\0';
        return (-1);
    }

    if (pos == src_len - 1)
    {
        /* remove last char */

        for (i = 0; i < src_len - 1; i++)
        {
            dst[i] = src[i];
        }
        dst[i] = '\0';
    }
    else
    {
        for (i = 0; i < pos; i++)
        {
            dst[i] = src[i];
        }

        for (j = pos + 1; j < src_len; j++)
        {
            dst[i] = src[j];
            i++;
        }
        dst[i] = '\0';
    }

    return (1);
}

int strinsertchar (U1 *dst, U1 *src, U1 chr, int pos)
{
    int i, j, src_len;

    src_len = strlen_safe (src, MAXLINELEN);

    if (pos < 0 || pos > src_len)
    {
        dst[0] = '\0';
        return (-1);
    }

    if (pos == src_len)
    {
        /* add char at end of string */

        for (i = 0; i < src_len; i++)
        {
            dst[i] = src[i];
        }
        dst[i] = chr;
        dst[i + 1] = '\0';
    }
    else
    {
        for (i = 0; i < pos; i++)
        {
            dst[i] = src[i];
        }

        dst[i] = chr;
        i++;

        for (j = pos; j < src_len; j++)
        {
            dst[i] = src[j];
            i++;
        }
        dst[i] = '\0';
    }

    return (1);
}

U1 *my_strcpy (U1 *destination, const U1 *source)
{
	int i = 0;

	while (source[i] != '\0')
	{
		destination[i] = source[i];
		i++;
	}

	destination[i] = '\0';
	return (destination);
}
