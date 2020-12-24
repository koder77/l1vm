/*
 * This file main.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2020
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

//  l1pre - L1VM preprocessor
//	include format:
// #include <some-include-file>

#include "../include/global.h"

#define INCLUDE_SB		"#include"

U1 include_path[MAXLINELEN + 1];

// protos
char *fgets_uni (char *str, int len, FILE *fptr);
size_t strlen_safe (const char * str, int maxlen);
S2 searchstr (U1 *str, U1 *srchstr, S2 start, S2 end, U1 case_sens);
void convtabs (U1 *str);

FILE *finptr;
FILE *foutptr;

S8 linenum ALIGN = 1;

S2 include_file (U1 *line_str)
{
	U1 rbuf[MAXLINELEN + 1];
	U1 ok;
	char *read;
	U1 get_include_start;
	U1 get_include_name;
	S4 pos, slen;
	S4 i, j;

	S4 include_path_len, include_name_len;

	FILE *fincludeptr;
	U1 include_file_name[MAXLINELEN + 1];
	U1 include_full_path[MAXLINELEN + 1];

	slen = strlen_safe ((const char *) line_str, MAXLINELEN);
	pos = searchstr (line_str, (U1 *) INCLUDE_SB, 0, 0, TRUE);

	// found include definition, get file name
	i = pos + 8; // next char after "#include"
	get_include_start = 0;
	while (get_include_start == 0)
	{
		if (line_str[i] != '<')
		{
			if (i < slen - 1)
			{
				i++;
			}
		}
		else
		{
			i++;
			get_include_start = 1;
		}
	}
	get_include_name = 1;
	j = 0;
	while (get_include_name == 1)
	{
		if (line_str[i] != '>')
		{
			include_file_name[j] = line_str[i];
			j++;
			i++;
		}
		else
		{
			// end of include name
			include_file_name[j] = '\0';
			get_include_name = 0;
		}
		if (i == slen - 1)
		{
			// error: no '>' found!!!
			return (1);
		}
	}

	strcpy ((char *) include_full_path, (const char *) include_path);
	include_path_len = strlen_safe ((const char *) include_full_path, MAXLINELEN);
	include_name_len = strlen_safe ((const char *) include_file_name, MAXLINELEN);
	if (include_path_len + include_name_len > MAXLINELEN)
	{
		printf ("ERROR: can't open include files, names too long!\n");
		return (1);
	}
	// cat filename to path name
	strcat ((char *) include_full_path, (const char *) include_file_name);

	fincludeptr = fopen ((const char *) include_full_path, "r");
	if (fincludeptr == NULL)
	{
		printf ("ERROR: can't open include file: '%s' !", include_file_name);
		return (1);
	}

	// read include file
	ok = TRUE;
	while (ok)
	{
		read = fgets_uni ((char *) rbuf, MAXLINELEN, fincludeptr);
		if (read != NULL)
		{
			convtabs (rbuf);
			slen = strlen_safe ((const char *) rbuf, MAXLINELEN);

			pos = searchstr (rbuf, (U1 *) INCLUDE_SB, 0, 0, TRUE);
            if (pos >= 0)
			{
				if (include_file (rbuf) != 0)
				{
					fclose (finptr);
					fclose (foutptr);
					exit (1);
				}
			}
			else
			{
				// save line
				if (fprintf (foutptr, "%s", rbuf) < 0)
				{
					printf ("ERROR: can't write to output file!\n");
					fclose (fincludeptr);
					return (1);
				}
			}
		}
		else
		{
			ok = FALSE;
		}
	}
	fclose (fincludeptr);
	return (0);
}

void show_info (void)
{
	printf ("l1pre <file-in> <file-out> <include-path>\n");
	printf ("\nPreprocessor for bra(ets, a programming language with brackets ;-)\n");
	printf ("%s", VM_VERSION_STR);
	printf ("%s\n", COPYRIGHT_STR);
}

int main (int ac, char *av[])
{
	U1 rbuf[MAXLINELEN + 1];                        /* read-buffer for one line */

	char *read;
	U1 ok;
	S4 pos, slen;

	if (ac < 4)
	{
		show_info ();
		exit (1);
	}

	// open input file
	finptr = fopen (av[1], "r");
	if (finptr == NULL)
	{
		// error can't open input file
		printf ("ERROR: can't open input file: '%s' !\n", av[1]);
		exit (1);
	}

	foutptr = fopen (av[2], "w");
	if (foutptr == NULL)
	{
		// error can't open input file
		printf ("ERROR: can't open output file: '%s' !\n", av[2]);
		fclose (finptr);
		exit (1);
	}

	if (strlen_safe ((const char *) av[3], MAXLINELEN) > MAXLINELEN - 1)
	{
		printf ("ERROR: include path: '%s' too long!\n", av[3]);
		fclose (finptr);
		fclose (foutptr);
		exit (1);
	}
	strcpy ((char *) include_path, av[3]);

	// read input line loop
	ok = TRUE;
	while (ok)
	{
		read = fgets_uni ((char *) rbuf, MAXLINELEN, finptr);
        if (read != NULL)
        {
			convtabs (rbuf);
			slen = strlen_safe ((const char *) rbuf, MAXLINELEN);

			// printf ("[ %s ]\n", rbuf);

            pos = searchstr (rbuf, (U1 *) INCLUDE_SB, 0, 0, TRUE);
            if (pos >= 0)
			{
				if (include_file (rbuf) != 0)
				{
					fclose (finptr);
					fclose (foutptr);
					exit (1);
				}
			}
			else
			{
				// save line
				if (fprintf (foutptr, "%s", rbuf) < 0)
				{
					printf ("ERROR: can't write to output file!\n");
					fclose (finptr);
					fclose (foutptr);
					exit (1);
				}
			}
		}
		else
		{
			ok = FALSE;
		}
	}
	fclose (finptr);
	fclose (foutptr);
	exit (0);
}
