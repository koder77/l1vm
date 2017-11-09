/*
 * This file file.c is part of L1vm.
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

// #define NULL    0L

char *fgets_uni (char *str, int len, FILE *fptr)
{
    int ch, nextch;
    int i = 0, eol = FALSE;
    char *ret;

    ch = fgetc (fptr);
    if (feof (fptr))
    {
        return (NULL);
    }
    while (! feof (fptr) || i == len - 2)
    {
        switch (ch)
        {
            case '\r':
                /* check for '\r\n\' */

                nextch = fgetc (fptr);
                if (! feof (fptr))
                {
                    if (nextch != '\n')
                    {
                        ungetc (nextch, fptr);
                    }
                }
                str[i] = '\n';
                i++; eol = TRUE;
                break;

            case '\n':
                /* check for '\n\r\' */

                nextch = fgetc (fptr);
                if (! feof (fptr))
                {
                    if (nextch != '\r')
                    {
                        ungetc (nextch, fptr);
                    }
                }
                str[i] = '\n';
                i++; eol = TRUE;
                break;

            default:
				str[i] = ch;
				i++;
                
                break;
        }

        if (eol)
        {
            break;
        }

        ch = fgetc (fptr);
    }

    if (feof (fptr))
    {
//        str[i] = '\n';
//        i++;
        str[i] = '\0';
    }
    else
    {
        str[i] = '\0';
    }

    ret = str;
    return (ret);
}

