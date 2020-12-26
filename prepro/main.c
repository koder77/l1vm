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

#define DEFINE_MAX		1000		// max defines can be done


#define INCLUDE_SB		"#include"
#define DEFINE_SB		"#define"

U1 include_path[MAXLINELEN + 1];

struct define
{
	U1 def[MAXLINELEN + 1];
	U1 out[MAXLINELEN + 1];
};

struct define defines[DEFINE_MAX];
S8 defines_ind ALIGN = -1;

// protos
char *fgets_uni (char *str, int len, FILE *fptr);
size_t strlen_safe (const char * str, int maxlen);
S2 searchstr (U1 *str, U1 *srchstr, S2 start, S2 end, U1 case_sens);
void convtabs (U1 *str);

FILE *finptr;
FILE *foutptr;

S8 linenum ALIGN = 1;

S2 set_define (U1 *line_str)
{
	U1 ok = 1;
	S4 i, pos, slen, j, k;

	if (defines_ind < DEFINE_MAX - 1)
	{
		defines_ind++;
	}
	else
	{
		// error: max defines reached
		printf ("ERROR: max defines reached: %i !\n", DEFINE_MAX);
		return (1);
	}

	slen = strlen_safe ((const char*) line_str, MAXLINELEN);
	pos = searchstr (line_str, (U1 *) DEFINE_SB, 0, 0, TRUE);

	i = pos + 8; // next char after "#include"

	// get define name:
	j = 0;
	while (ok == 1)
	{
		if (line_str[i] != ' ')
		{
			defines[defines_ind].def[j] = line_str[i];
			j++;
		}
		else
		{
			ok = 0;
		}
		i++;
	}

	j = 0;
	for (k = i; k < slen; k++)
	{
		if (line_str[k] != '\n')
		{
			defines[defines_ind].out[j] = line_str[k];
		}
		j++;
	}
	defines[defines_ind].out[j] = '\0';

	// printf ("set_defines: def: '%s', out: '%s'\n", defines[defines_ind].def, defines[defines_ind].out);
	return (0);
}

S2 replace_define (U1 *line_str)
{
	U1 ok = 1;
	S4 i, pos, slen, define_len, define_out_len, j, end, n, real_end;
	S4 ind;

	U1 new_line[MAXLINELEN + 1];

	slen = strlen_safe ((const char*) line_str, MAXLINELEN);

	// check for every definition, if it is in current line!

	if (defines_ind < 0)
	{
		// nothing to replace, exit
		return (0);
	}

	ok = 1;
	while (ok == 1)
	{
		// printf ("line_str: '%s'\n", line_str);

		for (ind = 0; ind <= defines_ind; ind++)
		{
			slen = strlen_safe ((const char*) line_str, MAXLINELEN);
			// printf ("searching define: '%s'\n", defines[ind].def);
			// printf ("'%s'\n\n", line_str);
			pos = searchstr (line_str, defines[ind].def, 0, 0, TRUE);
			if (pos >= 0)
			{
				// printf ("found define at pos: %i, '%s'\n", pos, defines[ind].def);

				define_len = strlen_safe ((const char *) defines[ind].def, MAXLINELEN);

				// found define at position
				// copy part before define to new_line
				j = 0;
				for (i = 0; i < pos; i++)
				{
					new_line[j] = line_str[i];
					j++;
				}

				// copy define out to new_line
				define_out_len = strlen_safe ((const char *) defines[ind].out, MAXLINELEN);
				for (i = 0; i <  define_out_len; i++)
				{
					new_line[j] = defines[ind].out[i];
					j++;
				}

				// copy part after define to new_line
				for (i = pos + define_len; i < slen; i++)
				{
					new_line[j] = line_str[i];
					j++;
				}
				new_line[j] = '\0';
				end = strlen_safe ((const char *) new_line, MAXLINELEN);
				for (n = end; n >= 0; n--)
				{
					// check for real line ending
					if (new_line[n] != '\n')
					{
						real_end = n + 1;
						break;
					}
				}
				new_line[real_end] = '\n';
				real_end++;
				new_line[real_end] = '\0';
				strcpy ((char *) line_str, (const char *) new_line);

				// printf ("replace_define: new line: '%s'\n", line_str);
			}
			else
			{
				ok = 0;
			}
		}
	}
	return (0);
}

S2 include_file (U1 *line_str)
{
	U1 rbuf[MAXLINELEN + 1];
	U1 buf[MAXLINELEN + 1];
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
			strcpy (buf, rbuf);
			convtabs (buf);
			slen = strlen_safe ((const char *) buf, MAXLINELEN);

			// check if define is set
			replace_define (buf);

			pos = searchstr (buf, (U1 *) DEFINE_SB, 0, 0, TRUE);
            if (pos >= 0)
			{
				// printf ("DEBUG: got #define!\n");

				if (set_define (buf) != 0)
				{
					fclose (finptr);
					fclose (foutptr);
					exit (1);
				}
				continue;	// don't safe define line!
			}

			pos = searchstr (buf, (U1 *) INCLUDE_SB, 0, 0, TRUE);
            if (pos >= 0)
			{
				if (include_file (buf) != 0)
				{
					fclose (finptr);
					fclose (foutptr);
					exit (1);
				}
			}
			else
			{
				// save line
				if (fprintf (foutptr, "%s", buf) < 0)
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
	U1 buf[MAXLINELEN + 1];

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
			strcpy (buf, rbuf);
			convtabs (buf);
			slen = strlen_safe ((const char *) buf, MAXLINELEN);

			// printf ("'%s'\n", buf);

			// check if define is set
			replace_define (buf);

			pos = searchstr (buf, (U1 *) DEFINE_SB, 0, 0, TRUE);
            if (pos >= 0)
			{
				// printf ("DEBUG: got #define!\n");

				if (set_define (buf) != 0)
				{
					fclose (finptr);
					fclose (foutptr);
					exit (1);
				}
				continue;	// don't safe define line!
			}

            pos = searchstr (buf, (U1 *) INCLUDE_SB, 0, 0, TRUE);
            if (pos >= 0)
			{
				if (include_file (buf) != 0)
				{
					fclose (finptr);
					fclose (foutptr);
					exit (1);
				}
			}
			else
			{
				// save line
				if (fprintf (foutptr, "%s", buf) < 0)
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
