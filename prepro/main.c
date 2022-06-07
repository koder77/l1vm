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

#define DEFINE_MAX			1000		// max defines can be done
#define ARGS_MAX			30			// max macro args

#define DEFINE_EMPTY		10

#define INCLUDE_SB			"#include"
#define DEFINE_SB			"#define"
#define FUNC_SB				"#func"

// for @name tackon on functions
#define FUNC_VAR_NAMES_SB	"#var"
#define FUNC_END_SB			"funcend"

#define COMMENT_SB			">>"
#define COMMENT_COMP_SB		"//"

U1 include_path[MAXSTRLEN + 1];

struct define
{
	U1 type; 					// 0 = normal define, 1 = macro
	U1 def[MAXSTRLEN + 1];
	U1 out[MAXSTRLEN + 1];
	U1 args[ARGS_MAX][MAXSTRLEN + 1];
	S2 args_num;				// number of arguments
};

struct define defines[DEFINE_MAX];
S8 defines_ind ALIGN = -1;

// for @func name tackon
U1 var_tackon_set = 0;
S8 replace_varname_index = 0;

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

	defines[defines_ind].type = 0;	// normal define

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

S2 set_varname (U1 *line_str)
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
	pos = searchstr (line_str, (U1 *) FUNC_VAR_NAMES_SB, 0, 0, TRUE);

	defines[defines_ind].type = 0;	// normal define

	i = pos + 5; // next char after "#var"

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

	// printf ("set_varname: def: '%s', out: '%s'\n", defines[defines_ind].def, defines[defines_ind].out);
	return (0);
}


S2 replace_str (U1 *line_str, U1 *search_str, U1 *replace_str)
{
	S4 i, pos, slen, define_len, define_out_len, j, end, n, real_end;
	U1 ok;
	U1 new_line[MAXSTRLEN + 1];

	slen = strlen_safe ((const char*) line_str, MAXLINELEN);

	ok = 1;
	while (ok == 1)
	{
		// printf ("DEBUG: replace_str: line_str: '%s'\n", line_str);
		// printf ("DEBUG: replace_str: search_str: '%s'\n", search_str);

		pos = searchstr (line_str, search_str, 0, 0, TRUE);
		if (pos >= 0)
		{
			// printf ("DEBUG: replace_str: found search string...\n");
			define_len = strlen_safe ((const char *) search_str, MAXLINELEN);

			// found define at position
			// copy part before define to new_line
			j = 0;
			for (i = 0; i < pos; i++)
			{
				new_line[j] = line_str[i];
				j++;
			}

			// copy define out to new_line
			define_out_len = strlen_safe ((const char *) replace_str, MAXLINELEN);
			for (i = 0; i < define_out_len; i++)
			{
				new_line[j] = replace_str[i];
				j++;
			}

			if (pos + define_len < slen)
			{
				// copy part after define to new_line
				for (i = pos + define_len; i < slen; i++)
				{
					new_line[j] = line_str[i];
					j++;
				}
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
			slen = strlen_safe ((const char*) line_str, MAXLINELEN);
		}
		else
		{
			ok = 0;
		}
	}
	return (0);
}

S2 replace_define (U1 *line_str)
{
	S4 ind;

	U1 new_line[MAXSTRLEN + 1];

	// printf ("DEBUG: replace_define: start...\n");

	strcpy ((char *) new_line, (const char *) line_str);
	// printf ("DEBUG: replace_define: old linestr: '%s'\n", new_line);

	for (ind = 0; ind <= defines_ind; ind++)
	{
		if (defines[ind].type == 0)
		{
			// printf ("DEBUG: replace_define: replace: '%s'\n", defines[ind].def);
			replace_str (new_line, defines[ind].def, defines[ind].out);
		}
	}
	strcpy ((char *) line_str, (const char *) new_line);
	// printf ("DEBUG: replace_define: new linestr: '%s'\n\n", line_str);
	return (0);
}

S2 replace_varname (U1 *line_str)
{
	S4 ind;

	U1 new_line[MAXSTRLEN + 1];

	// printf ("DEBUG: replace_define: start...\n");

	if (var_tackon_set == 1)
	{
		strcpy ((char *) new_line, (const char *) line_str);
		// printf ("DEBUG: replace_define: old linestr: '%s'\n", new_line);

		for (ind = 0; ind <= defines_ind; ind++)
		{
			if (defines[ind].type == 0)
			{
				// printf ("DEBUG: replace_define: replace: '%s'\n", defines[ind].def);
				replace_str (new_line, defines[ind].def, defines[ind].out);
				replace_varname_index = ind;
			}
		}
		strcpy ((char *) line_str, (const char *) new_line);
	}
	// printf ("DEBUG: replace_define: new linestr: '%s'\n\n", line_str);
	return (0);
}

S2 set_macro (U1 *line_str)
{
	U1 ok = 1, arg_loop;
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
	pos = searchstr (line_str, (U1 *) FUNC_SB, 0, 0, TRUE);

	defines[defines_ind].type = 1;	// macro

	i = pos + 6; // next char after "#func"

	// get define name:
	j = 0;
	while (ok == 1)
	{
		if (line_str[i] != ' ' && line_str[i] != '(')
		{
			defines[defines_ind].def[j] = line_str[i];
			j++;
		}
		else
		{
			defines[defines_ind].def[j] = '\0';
			ok = 0;
		}
		i++;
	}

	// printf ("DEBUG: macro name: '%s'\n", defines[defines_ind].def);

	// get define args
	defines[defines_ind].args_num = -1;

	ok = 0; k = 0;
	while (ok == 0)
	{
		if (defines[defines_ind].args_num < ARGS_MAX)
		{
			defines[defines_ind].args_num++;
		}
		else
		{
			printf ("ERROR: set_macro: no argument space free!\n");
			return (1);
		}

		arg_loop = 1;
		while (arg_loop == 1)
		{
			// printf ("DEBUG: args_num: %i\n", defines[defines_ind].args_num);
			// printf ("DEBUG: line_str[i]: '%c'\n", line_str[i]);

			if (line_str[i] != ' ' && line_str[i] != ',' && line_str[i] != '(' && line_str[i] != ')')
			{
				defines[defines_ind].args[defines[defines_ind].args_num][k] = line_str[i];
				k++;
			}
			else
			{
				if (line_str[i] == ',' || line_str[i] == ')')
				{
					// set argument end
					defines[defines_ind].args[defines[defines_ind].args_num][k] = '\0';
					// printf ("DEBUG: arg: '%s'\n", defines[defines_ind].args[defines[defines_ind].args_num]);
					k = 0;
					arg_loop = 0;
				}
				if (line_str[i] == ')')
				{
					// found end of macro args
					ok = 1;
					k = 0;
					arg_loop = 0;
				}
			}
			i++;
		}
	}

	// search for ":" and get the macro
	pos = searchstr (line_str, (U1 *) ":", 0, 0, TRUE);
	if (pos < 0)
	{
		printf ("ERROR: set_macro: no macro start: ':' found!\n");
		return (1);
	}

	pos++;

	j = 0;
	for (k = pos; k < slen; k++)
	{
		if (line_str[k] != '\n')
		{
			defines[defines_ind].out[j] = line_str[k];
		}
		j++;
	}
	defines[defines_ind].out[j] = '\0';

	return (0);
}

S2 replace_macro (U1 *line_str)
{
	U1 ok = 1;
	S4 i, pos, slen, k;
	S4 ind;
	S4 defines_len;

	U1 new_line[MAXSTRLEN + 1];
	U1 arg[MAXSTRLEN + 1];
	S4 arg_ind = -1;

	slen = strlen_safe ((const char*) line_str, MAXLINELEN);

	// check for every definition, if it is in current line!

	if (defines_ind < 0)
	{
		// nothing to replace, exit
		return (0);
	}

	ok = 1; arg_ind = -1;
	while (ok == 1)
	{
		// printf ("line_str: '%s'\n", line_str);

		for (ind = 0; ind <= defines_ind; ind++)
		{
			if (defines[ind].type == 1)
			{
				// is macro
				// copy macro to new_line
				strcpy ((char *) new_line, (const char *) defines[ind].out);

				// printf ("DEBUG: replace_macro: line_str: '%s'\n", line_str);
				// printf ("DEBUG: replace_macro: macro: '%s'\n", new_line);

				slen = strlen_safe ((const char *) line_str, MAXLINELEN);
				// printf ("searching define: '%s'\n", defines[ind].def);
				// printf ("'%s'\n\n", line_str);

				defines_len = strlen_safe ((char *) defines[ind].def, MAXLINELEN);
				pos = searchstr (line_str, defines[ind].def, 0, 0, TRUE);
				if (pos >= 0)
				{
					// printf ("DEBUG: replace_macro: name: '%s'\n", defines[ind].def);
					// get arguments

					ok = 1; k = 0;
					i = pos + defines_len;
					while (ok == 1)
					{
						// printf ("DEBUG: line_str ch: '%c'\n", line_str[i]);

						if (line_str[i] != ' ' && line_str[i] != ',' && line_str[i] != '(' && line_str[i] != ')')
						{
							arg[k] = line_str[i];

							// printf ("DEBUG: arg[k]: '%c'\n", arg[k]);

							k++;
							i++;
						}
						else
						{
							if (line_str[i] == ',' || line_str[i] == ')')
							{
								if (arg_ind < defines[ind].args_num)
								{
									arg_ind++;
								}
								else
								{
									printf ("ERROR: replace_macro: too much arguments!\n");
									printf ("> '%s'\n", line_str);
									return (1);
								}

								// printf ("DEBUG: replace_macro: arg_ind: %i\n", arg_ind);

								// set argument end
								arg[k] = '\0';
								k = 0;
								// i++;

								// printf ("DEBUG: replace_macro arg: '%s'\n", arg);

								// replace arg in output line "new_line"

								slen = strlen_safe ((const char*) new_line, MAXLINELEN);
								// printf ("searching define: '%s'\n", defines[ind].def);
								// printf ("'%s'\n\n", line_str);

								replace_str (new_line, defines[ind].args[arg_ind], arg);
								// printf ("DEBUG: new_line: '%s'\n", new_line);

								// printf ("DEBUG: line_str[i]: '%c'\n", line_str[i]);
								if (line_str[i] == ')')
								{
									if (arg_ind != defines[ind].args_num)
									{
										printf ("ERROR: arguments mismatch!\n");
										printf ("> '%s'\n", line_str);
										return (1);
									}

									// found end of macro args
									strcpy ((char *) line_str, (const char *) new_line);
									slen = strlen_safe ((const char *) line_str, MAXLINELEN);

									line_str[slen] = '\n';
									line_str[slen + 1] = '\0';
									ok = 0;
								}
							}

							if (i == defines_len - 1)
							{
								ok = 0;
							}
							i++;
						}
					}
				}
				else
				{
					ok = 0;
				}
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
	U1 rbuf[MAXSTRLEN + 1];
	U1 buf[MAXSTRLEN + 1];
	U1 ok;
	char *read;
	U1 get_include_start;
	U1 get_include_name;
	S4 pos, slen;
	S4 i, j;

	S4 include_path_len, include_name_len;

	FILE *fincludeptr;
	U1 include_file_name[MAXSTRLEN + 1];
	U1 include_full_path[MAXSTRLEN + 1];

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
			strcpy ((char *) buf, (const char *) rbuf);
			convtabs (buf);
			slen = strlen_safe ((const char *) buf, MAXLINELEN);

			pos = searchstr (buf, (U1 *) COMMENT_SB, 0, 0, TRUE);
			if (pos >= 0)
			{
				// line is comment, skip it!
				continue;
			}

			pos = searchstr (buf, (U1 *) COMMENT_COMP_SB, 0, 0, TRUE);
			if (pos >= 0)
			{
				// normal compiler comment:
				// save line and continue!
				if (fprintf (foutptr, "%s", buf) < 0)
				{
					printf ("ERROR: can't write to output file!\n");
					fclose (fincludeptr);
					return (1);
				}
				continue;
			}

			pos = searchstr (buf, (U1 *) "#", 0, 0, TRUE);
			if (pos < 0)
			{
				// no definition start found, check replace
				// check if define is set
				replace_define (buf);

				// check function name to variable name tackon
				replace_varname (buf);

				// check if macro is set
				replace_macro (buf);
			}

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

			pos = searchstr (buf, (U1 *) FUNC_SB, 0, 0, TRUE);
            if (pos >= 0)
			{
				// printf ("DEBUG: got #define!\n");

				if (set_macro (buf) != 0)
				{
					fclose (finptr);
					fclose (foutptr);
					exit (1);
				}
				continue;	// don't safe define line!
			}

			pos = searchstr (buf, (U1 *) FUNC_VAR_NAMES_SB, 0, 0, TRUE);
            if (pos >= 0)
			{
				// printf ("DEBUG: got #var!\n");

				if (set_varname (buf) != 0)
				{
					fclose (finptr);
					fclose (foutptr);
					exit (1);
				}
				// replace all @ with the variable end as defined in func var macro
				var_tackon_set = 1;
				continue;	// don't safe define line!
			}

			// check for function end
			pos = searchstr (buf, (U1 *) FUNC_END_SB, 0, 0, TRUE);
            if (pos >= 0)
			{
				if (var_tackon_set == 1)
				{
					// switch function name to variable end tackon off!!!
					var_tackon_set = 0;
					// unset macro
					defines[replace_varname_index].type = DEFINE_EMPTY;
				}
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
	U1 rbuf[MAXSTRLEN + 1];                        /* read-buffer for one line */
	U1 buf[MAXSTRLEN + 1];

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
			strcpy ((char *) buf, (const char *) rbuf);
			convtabs (buf);
			slen = strlen_safe ((const char *) buf, MAXLINELEN);

			pos = searchstr (buf, (U1 *) COMMENT_SB, 0, 0, TRUE);
			if (pos >= 0)
			{
				// line is comment, skip it!
				continue;
			}

			pos = searchstr (buf, (U1 *) COMMENT_COMP_SB, 0, 0, TRUE);
			if (pos >= 0)
			{
				// normal compiler comment:
				// save line and continue!
				if (fprintf (foutptr, "%s", buf) < 0)
				{
					printf ("ERROR: can't write to output file!\n");
					fclose (finptr);
					fclose (foutptr);
					exit (1);
				}
				continue;
			}

			// printf ("'%s'\n", buf);

			pos = searchstr (buf, (U1 *) "#", 0, 0, TRUE);
			if (pos < 0)
			{
				// no definition start found, check replace
				// check if define is set
				replace_define (buf);

				// check function name to variable name tackon
				replace_varname (buf);

				// check if macro is set
				replace_macro (buf);
			}

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

			pos = searchstr (buf, (U1 *) FUNC_SB, 0, 0, TRUE);
            if (pos >= 0)
			{
				// printf ("DEBUG: got #define!\n");

				if (set_macro (buf) != 0)
				{
					fclose (finptr);
					fclose (foutptr);
					exit (1);
				}
				continue;	// don't safe define line!
			}

			pos = searchstr (buf, (U1 *) FUNC_VAR_NAMES_SB, 0, 0, TRUE);
            if (pos >= 0)
			{
				// printf ("DEBUG: got #var!\n");

				if (set_varname (buf) != 0)
				{
					fclose (finptr);
					fclose (foutptr);
					exit (1);
				}
				// replace all @ with the variable end as defined in func var macro
				var_tackon_set = 1;
				continue;	// don't safe define line!
			}

			// check for function end
			pos = searchstr (buf, (U1 *) FUNC_END_SB, 0, 0, TRUE);
            if (pos >= 0)
			{
				if (var_tackon_set == 1)
				{
					// switch function name to variable end tackon off!!!
					var_tackon_set = 0;
					// unset macro
					defines[replace_varname_index].type = DEFINE_EMPTY;
				}
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
