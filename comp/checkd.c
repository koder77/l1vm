/*
 * This file checkd.c is part of L1vm.
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

extern struct t_var t_var;

size_t strlen_safe (const char * str, int maxlen);

U1 checkdigit (U1 *str)
{
    U1 binarystr[MAXSTRLEN];
	S2 i, j, b, str_len;
    U1 ok = FALSE, check;

	// printf ("checkdigit: '%s'\n", str);

    str_len = strlen_safe ((const char *) str, MAXLINELEN);
    t_var.digitstr_type = UNKNOWN;
	t_var.base = 10;

    for (i = 0; i <= str_len - 1; i++)
    {
        if (str[i] == '.')
        {
            t_var.digitstr_type = DOUBLEFLOAT;
        }
    }

    if (t_var.digitstr_type == UNKNOWN)
    {
        /* INT, LINT, QINT or BYTE */

        i = 0;
        j = 0;
        ok = FALSE;

        while (! ok)
        {
            if ((str[i] >= '0' && str[i] <= '9') || (str[i] >= 'A' && str[i] <= 'F') || (str[i] == '-' || str[i] == '+') || str[i] == ' ')
            {
				if (str[i] >= 'A' && str[i] <= 'F')
				{
					t_var.base = 16;
				}

                if (j <= MAXLINELEN - 1 && str[i] != ' ')
                {
                    t_var.digitstr[j] = str[i];
                    j++;
                }
                else
                {
                    ok = TRUE;
                }
                check = TRUE;
            }
            else
            {
                switch (str[i])
                {
                    case 'W':
                        t_var.digitstr_type = WORD;
                        check = TRUE;
                        break;

                    case 'D':
                        t_var.digitstr_type = DOUBLEWORD;
                        check = TRUE;
                        break;

                    case 'Q':
                        t_var.digitstr_type = QUADWORD;
                        check = TRUE;
                        break;

                    case 'B':
                        t_var.digitstr_type = BYTE;
                        check = TRUE;
                        break;

					case '&':
						t_var.base = 16;
						break;

					case '$':
						t_var.base = 2;
						break;

                    case 'I':
                        t_var.digitstr_type = QUADWORD;
                        check = TRUE;
                        break;

                    case 'F':
                        t_var.digitstr_type = QUADWORD;
                        check = TRUE;
                        break;

                    default:
                        check = FALSE;
                        ok = TRUE;
                }
            }

            if (i < str_len - 1)
            {
                i++;
            }
            else
            {
                ok = TRUE;
            }
        }
    }
    else
    {
        /* DOUBLE */

        i = 0;
        j = 0;
        ok = FALSE;

        while (! ok)
        {
            if ((str[i] >= '0' && str[i] <= '9') || str[i] == '.' || str[i] == '-' || str[i] == '+' || str[i] == 'e' || str[i] == 'E' || str[i] == ' ')
            {
                if (j <= MAXLINELEN - 1 && str[i] != ' ')
                {
                    t_var.digitstr[j] = str[i];
                    j++;
                }
                else
                {
                    ok = TRUE;
                }
                check = TRUE;
            }
            else
            {
                check = FALSE;
                ok = TRUE;
            }

            if (i < str_len - 1)
            {
                i++;
            }
            else
            {
                ok = TRUE;
            }
        }
    }

    t_var.digitstr[j] = '\0';


    if (t_var.base == 2)
	{
		/* if base = 2 = binary then cut away leading zeroes */
		j = 0; b = 0;
		str_len = strlen_safe ((const char *) t_var.digitstr, MAXLINELEN);
		for (i = 0; i < str_len; i++)
		{
			if (t_var.digitstr[i] == '0')
			{
				if (j == 1)
				{
					binarystr[b] = '0';
					b++;
				}
			}

			if (t_var.digitstr[i] == '1')
			{
				j = 1;
				binarystr[b] = '1';
				b++;
			}
		}
		binarystr[b] = '\0';

		strcpy ((char *) t_var.digitstr, (const char *) binarystr);
	}

	// printf ("t_var: '%s'\n", t_var.digitstr);
	// printf ("check: %i\n\n", check);

    return (check);
}

S8 get_temp_int (void)
{
    S8 num ALIGN;
    char *endptr;

    num = (S8) strtoll ((const char *) t_var.digitstr, &endptr, t_var.base);
    if (*endptr != '\0') printf ("get_temp_int: error: make quadword!\n");

    return (num);
}

F8 get_temp_double (void)
{
    F8 num ALIGN;

    num = strtod ((const char *) t_var.digitstr, NULL);
    if (errno != 0) printf ("get_temp_double: error: make doublefloat!\n");

    return (num);
}
