/*
 * This file main.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2023
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

#define SET_SB              "(set"
#define MAXVARS             4096

#define BOOL 13
#define VARTYPE_NONE 14
#define VARTYPE_STRING 15

// for @name tackon on functions
#define FUNC_VAR_NAMES_SB	"#var"
#define FUNC_END_SB			"funcend"

#define COMMENT_SB			">>"
#define COMMENT_COMP_SB		"//"

#define MULTILINE_SB        "@#"

#define CLEAR_DEFINES_SB    "#clear"    // clear all defines, reset defines index to: -1

#define NO_REPLACE_SB      "#noreplace"
#define REPLACE_SB         "#replace"

// doucumentaiion blocks start and end
// will be written in out.md Markdown file
#define DOCU_START_SB      "#docustart"
#define DOCU_END_SB        "#docuend"


#define FOR_LOOP_SET_SB    "(for-loop)"
#define FOR_LOOP_SB        "for)"

#define INTR0_SB            "intr0)"
#define INTR1_SB            "intr1)"

#define CALL_SB             "call)"
#define CALL_SHORT_SB       "!)"

#define LABEL_SB      ":"

// for generating documentation file
FILE *docuptr;
U1 documentation_on = 0;     // if set to 1 then write the following lines into docu file
U1 documentation_write = 0;  // set to 1 if text was written in program.md documentation file

U1 include_path[MAXSTRLEN + 1];
U1 include_path_two[MAXSTRLEN + 1];

U1 pass = 1; // preprocessor run pass

// for included files
#define FILES_MAX           1000

struct file
{
	S8 linenum ALIGN;
	U1 name[MAXSTRLEN];
};

struct file files[FILES_MAX];
S8 file_index ALIGN = 0;
U1 file_inside = 0;
S8 linenum ALIGN = 0;

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

// for replace on/off
U1 replace = 1;

// for all variables
struct vars
{
    U1 name[MAXSTRLEN + 1];
    U1 type;
};

S8 vars_ind ALIGN = -1;
struct vars vars[MAXVARS];

FILE *finptr;
FILE *foutptr;

// global flag set to 1 if errors found
U1 return_error = 0;

// set to true if "(for-loop)" line in program was found.
// If not then it will be inserted by the preprocessor!
U1 for_loop = 0;

// set by CLI flag in main
// warns for deprecated things in programs, such as using the intr0 and intr1 direct in code.
// not unsing the "intr.l1h" or "intr-func.l1h" includes.
U1 flag_wdeprecated = 0;

U1 error_multi_spaces = 0;
// protos
char *fgets_uni (char *str, int len, FILE *fptr);
size_t strlen_safe (const char * str, S8 maxlen);
S2 searchstr (U1 *str, U1 *srchstr, S2 start, S2 end, U1 case_sens);
void convtabs (U1 *str);
S2 get_varname_type (U1 *name);
S2 parse_set (U1 *line, U1 *setret);

S2 do_check_spaces (U1 *line)
{
	S4 line_len = 0;
	S4 i = 0;
	S4 pos = 0;

	line_len = strlen_safe ((const char *) line, MAXLINELEN);

	// check if (set ) line:
	/*pos = searchstr (line, (U1 *) "(set", 0, 0, 0);
	if (pos != -1)
	{
		// found set line, exit with ok code
		return (0);
	}
	*/

	//printf ("check_spaces: '%s'\n", line);

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
				//printf ("found multiple spaces!\n\n");
				return (1);
			}
		}
	}

	// all ok!
	return (0);
}

void clear_defines (void)
{
	// clear ALL defines
	S4 i;

	for (i = 0; i <= defines_ind; i++)
	{
		strcpy ((char *) defines[i].def, "");
		strcpy ((char *) defines[i].out, "");
		defines[i].args_num = -1;
	}

	// reset defines index
	defines_ind = -1;
}

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

	defines[defines_ind].type = 0;	      // normal define
	defines[defines_ind].args_num = -1;   // no arguments, just replace later
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
	U1 define_already_set = 0;
	S4 i, pos, slen, j, k;

	U1 new_define[MAXLINELEN];

	// printf ("DEBUG: set_varname: index: %i, '%s'\n", defines_ind, line_str);

	slen = strlen_safe ((const char*) line_str, MAXLINELEN);
	pos = searchstr (line_str, (U1 *) FUNC_VAR_NAMES_SB, 0, 0, TRUE);

	i = pos + 5; // next char after "#var"

	// get define name:
	j = 0;
	while (ok == 1)
	{
		if (line_str[i] != ' ')
		{
		    new_define[j] = line_str[i];
			j++;
		}
		else
		{
			new_define[j] = '\0';
			ok = 0;
		}
		i++;
	}

	//printf ("set_varname: new define: def: '%s'\n", new_define);

	if (defines_ind >= 1)
	{
		// check if already defined
		for (k = 0; k <= defines_ind ; k++)
		{
			if (strcmp ((const char *) defines[k].def, (const char *) new_define) == 0)
			{
				// variable already defined -> reuse
				defines_ind = k;
				define_already_set = 1;
				break;
			}
		}
	}

	//printf ("set_varname: define_already_set: %i\n", define_already_set);

	if (define_already_set == 0)
	{
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
	}


	i = pos + 5;
	ok = 1; j = 0;
	while (ok == 1)
	{
		if (line_str[i] != ' ')
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


	j = 0;
	for (k = i; k < slen; k++)
	{
		if (line_str[k] != '\n')
		{
			defines[defines_ind].out[j] = line_str[k];
			j++;
		}
	}
	defines[defines_ind].out[j] = '\0';

	defines[defines_ind].type = 0;	// normal define

	// printf ("set_varname: def: %i: '%s', out: '%s'\n\n", defines_ind, defines[defines_ind].def, defines[defines_ind].out);
	return (0);
}


S2 replace_str (U1 *line_str, U1 *search_str, U1 *replace_str)
{
	S4 i, pos, slen, define_len, define_out_len, j, end, n, real_end = 0 ;
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

					// check if argument is: 'NONE':
					if (strcmp ((const char *) defines[defines_ind].args[defines[defines_ind].args_num], "NONE") == 0)
					{
						defines[defines_ind].args_num = -1;
						ok = 1;  // exit upper loop too!
					}
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
		printf ("'%s'\n", line_str);
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

S2 check_define_type (U1 *define, U1* variable)
{
	S2 str_len = 0;
	S2 vartype = 0;

	S2 symbstr_max = 30;
	S2 compstr_max = 12;
	S2 mathstr_max = 14;
	S2 i = 0;
	const U1 symbstr[30][4] = { "+", "-", "*", "/", "+d", "-d", "*d", "/d", "<<", ">>", "&&", "||", "&", "|", "^", "%", "==", "!=", "<=", ">=", ">", "<", ">|", "<|", "==d", "!=d", ">d", "<d", ">=d", "<=d" };
    const U1 compstr[12][4] = { "==", "!=", "<=", ">=", ">", "<", "==d", "!=d", ">d", "<d", ">=d", "<=d" };
	const U1 mathstr[14][4] = { "+", "-", "*", "/", "+d", "-d", "*d", "/d", "<<", ">>", "^", "%", "&", "|" };

	str_len = strlen_safe ((const char *) define, MAXLINELEN);
	if (str_len < 2)
	{
	   // error define too short
       return (0);
	}

	if (define[0] != 'o')
	{
		vartype = get_varname_type (variable);
		if (vartype == -1)
		{
			printf ("check_define_type: ERROR: variable: '%s' not defined!\n", variable);
			return (1);
		}
	}

	// printf ("DEBUG: check_define_type: type: %i\n", vartype);

	// check define variable type
	if (define[0] == 'i')
	{
		// i = integer type
		switch (vartype)
		{
			case BYTE:
			case WORD:
			case DOUBLEWORD:
			case QUADWORD:
			case BOOL:
				return (0);   // type ok!

			default:
				return (1);   // type ERROR!
		}
	}

	if (define[0] == 'd')
	{
		// d = double variable type
		if (vartype != DOUBLEFLOAT)
		{
			return (1); // type ERROR!
		}
		else
		{
			return (0);  // type ok!
		}
	}

	if (define[0] == 's')
	{
		// s = string variable type
		switch (vartype)
		{
			case VARTYPE_STRING:
			case STRING_CONST:
				return (0);   // is string type: ok!

			default:
				return (1);  // error!

		}
	}

	if (define[0] == 'o')
	{
		// operator type: some like: = != > < + - * /
		for (i = 0; i < symbstr_max; i++)
		{
			if (strcmp ((const char *) variable, (const char *) symbstr[i]) == 0)
			{
				return (0); // all ok, symbol found!
			}
		}
		return (1); // symbol not found! error!
	}

	if (define[0] == 'c')
	{
		// compare type: some like: = != > < <= >=
		for (i = 0; i < compstr_max; i++)
		{
			if (strcmp ((const char *) variable, (const char *) compstr[i]) == 0)
			{
				return (0); // all ok, symbol found!
			}
		}
		return (1); // symbol not found! error!
	}

	if (define[0] == 'm')
	{
		// math type: some like: + - * /
		for (i = 0; i < mathstr_max; i++)
		{
			if (strcmp ((const char *) variable, (const char *) mathstr[i]) == 0)
			{
				return (0); // all ok, symbol found!
			}
		}
		return (1); // symbol not found! error!
	}

	return (0); // unknown var type, for backwards compatibility set return code 0
}

S2 replace_macro_normal (U1 *line_str)
{
	U1 ok = 1;
	S4 i, pos, slen, k;
	S4 ind;
	S4 name_pos;

	U1 new_line[MAXSTRLEN + 1];
	U1 arg[MAXSTRLEN + 1];

	U1 call_found = 0;
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
		//printf ("line_str: '%s'\n", line_str);

		for (ind = 0; ind <= defines_ind; ind++)
		{
			// check both types: 0 = normal defines, 1 = macro function defines
			if (defines[ind].type == 1 || defines[ind].type == 0)
			{
				// is macro
				// copy macro to new_line
				if (defines[ind].args_num != -1)
				{
					strcpy ((char *) new_line, (const char *) defines[ind].out);
				}
				else
				{
					// macro with no arguments
					strcpy ((char *) new_line, (const char *) line_str);
				}

				//printf ("DEBUG: replace_macro: line_str: '%s'\n", line_str);
				//printf ("DEBUG: replace_macro: macro: '%s'\n", new_line);

				slen = strlen_safe ((const char *) line_str, MAXLINELEN);
				//printf ("searching define: '%s'\n", defines[ind].def);
				//printf ("'%s'\n\n", line_str);

				name_pos = searchstr (line_str, defines[ind].def, 0, 0, TRUE);
				if (name_pos >= 0)
				{
					// printf ("replace_macro_normal: found define: '%s'\n", defines[ind].def);

					call_found = 0;
				    pos = searchstr (line_str, (U1 *) CALL_SB, 0, 0, TRUE);
					if (pos >= 0)
					{
						call_found = 1;
					}

					pos = searchstr (line_str, (U1 *) CALL_SHORT_SB, 0, 0, TRUE);
					if (pos >= 0)
					{
						call_found = 1;
					}

					if (call_found == 0)
					{
						printf ("ERROR: function call: no 'call' or '!' found!\n");
						printf ("line: %lli: '%s'\n", linenum, line_str);
						return (1);
					}

					//printf ("replace_macro_normal: macro: '%s'\n", defines[ind].def);
					//printf ("replace_macro_normal: args num = %i\n\n", defines[ind].args_num);

					if (defines[ind].args_num == -1)
					{
						//printf ("replace_macro_normal: macro: '%s' inside replace...\n", defines[ind].def);

                        if (name_pos >= 2)
						{
							// remove: '(:' bracket and colon from output string
							new_line[name_pos - 1] = ' ';
							new_line[name_pos - 2] = ' ';

							// remove '! )'
							k = 1;
							i = name_pos;
							while (k == 1)
						    {
								if (new_line[i] == '!')
							    {
									if (new_line[i + 1] == ')')
								    {
                                        new_line[i] = ' ';
										new_line[i + 1] = ' ';
										k = 0;
									}
								}
							    i++;
							}
						}
						//printf ("replace_macro_normal: line prepared: '%s'\n", new_line);

						if (defines[ind].type == 0)
						{
							// replace all found defines in new_line
							replace_define (new_line);
						}
						else
						{
							// replace all found macros in new_line
							replace_str (new_line, defines[ind].def, defines[ind].out);
						}

						// remove possible spaces after closing bracket
						slen = strlen_safe ((const char*) new_line, MAXLINELEN);

						if (new_line[slen - 2] != ')')
						{
							i = slen;
							k = 1;
							while (k == 1)
							{
								if (new_line[i] == ')')
								{
									if (i + 2 < slen - 1)
									{
										new_line[i + 1] = '\n'; // set line feed
										new_line[i + 2] = '\0'; // set end mark

										k = 0;
									}
								}
								i--;
								if (i < 0)
								{
									printf ("ERROR: no ) closing bracket found!\n");
									printf ("line: %lli: '%s'\n", linenum, line_str);
									return (1);
								}
							}
						}
						strcpy ((char *) line_str, (const char *) new_line);
						return (0);
					}

					pos = searchstr (line_str, (U1 *) "(", 0, 0, TRUE);
                    if (pos > name_pos)
					{
						printf ("ERROR: found ( after function name!\n");
						return (1);
					}

					ok = 1; k = 0;
					i = pos + 1;

					//printf ("replace_macro_normal: startpos var: %i\n", i);

					while (ok == 1)
					{
						// printf ("DEBUG: line_str ch: '%c'\n", line_str[i]);

						if (line_str[i] != ' ' && line_str[i] != ':' && line_str[i] != '(' && line_str[i] != ')')
						{
							arg[k] = line_str[i];

							// printf ("DEBUG: arg[k]: '%c'\n", arg[k]);

							k++;
							i++;
						}
						else
						{
							if (line_str[i] == ' ' || line_str[i] == ':')
							{
								if (arg_ind < defines[ind].args_num)
								{
									arg_ind++;
								}
								else
								{
									// printf ("ERROR: replace_macro: too much arguments!\n");
									// printf ("> '%s'\n", line_str);
									return (1);
								}

								// printf ("DEBUG: replace_macro: arg_ind: %i\n", arg_ind);

								// set argument end
								arg[k] = '\0';
								k = 0;
								// i++;

								//printf ("DEBUG: replace_macro arg: '%s'\n", arg);

								// replace arg in output line "new_line"

								slen = strlen_safe ((const char*) new_line, MAXLINELEN);
								// printf ("searching define: '%s'\n", defines[ind].def);
								// printf ("'%s'\n\n", line_str);

								replace_str (new_line, defines[ind].args[arg_ind], arg);
								//printf ("DEBUG: new_line: '%s'\n", new_line);

								// check variable types
								if (check_define_type (defines[ind].args[arg_ind], arg) != 0)
								{
									printf ("ERROR: macro variable type mismatch: %s not of type: %s\n", arg, defines[ind].args[arg_ind]);
									printf ("line: '%s'\n\n", new_line);
									return (1);
								}

								//printf ("DEBUG: line_str[i]: '%c'\n", line_str[i]);
								if (i == name_pos - 2)
								{
									//printf ("found macro end!\n");

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

							if (i == name_pos - 2)
							{
								//printf ("FOUND END!\n");
								ok = 0;
							}
							i++;
						}
					} // replace end
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

S2 replace_macro (U1 *line_str)
{
	U1 ok = 1;
	S4 i, pos, slen, k;
	S4 ind;
	S4 defines_len;

	U1 new_line[MAXSTRLEN + 1];
	U1 arg[MAXSTRLEN + 1];
	S4 arg_ind = -1;

	S2 vartype = 0;

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
					//printf ("DEBUG: replace_macro: name: '%s'\n", defines[ind].def);
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
									// printf ("ERROR: replace_macro: too much arguments!\n");
									// printf ("> '%s'\n", line_str);
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
								//printf ("DEBUG: define: '%s'\n", defines[ind].args[arg_ind]);
								//printf ("DEBUG: replace arg: '%s'\n", arg);
								//printf ("DEBUG: new_line: '%s'\n", new_line);

								// check variable types
								if (check_define_type (defines[ind].args[arg_ind], arg) != 0)
								{
									printf ("ERROR: macro variable type mismatch: %s not of type: %s\n", arg, defines[ind].args[arg_ind]);
									printf ("line: '%s'\n\n", new_line);
									return (1);
								}

								// printf ("DEBUG: line_str[i]: '%c'\n", line_str[i]);
								if (line_str[i] == ')')
								{
									if (arg_ind != defines[ind].args_num)
									{
										printf ("ERROR: arguments mismatch!\n");

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
					} // replace end
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
	U1 setret[MAXSTRLEN + 1];

	U1 ok;
	char *read;
	U1 get_include_start;
	U1 get_include_name;
	S4 pos, slen, quote_pos;
	S4 i, j;

	S4 include_path_len, include_name_len;

	// multiline
	S4 multi_pos;
	U1 multi_len;

	FILE *fincludeptr;
	U1 include_file_name[MAXSTRLEN + 1];
	U1 include_full_path[MAXSTRLEN + 1];

	slen = strlen_safe ((const char *) line_str, MAXLINELEN);
	pos = searchstr (line_str, (U1 *) INCLUDE_SB, 0, 0, TRUE);

	// check for C sytle quote: "
	quote_pos = searchstr (line_str, (U1 *) "\"", 0, 0, TRUE);
	if (quote_pos != -1){
		printf ("error: #include: quote found: \" !\n");
		return (1);
	}

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
		// try to load file with second include path
		strcpy ((char *) include_full_path, (const char *) include_path_two);
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
			printf ("ERROR: can't open include file: '%s' !\n", include_file_name);
			return (1);
		}
	}

	if (pass == 1)
	{
		// save incude file name in output file
		if (fprintf (foutptr, "FILE: %s\n", include_file_name) < 0)
		{
			printf ("ERROR: can't write to output file!\n");
			fclose (finptr);
			fclose (foutptr);
			fclose (docuptr);
			exit (1);
		}
	}

	if (file_index < FILES_MAX - 1)
	{
		file_index++;
	}
	else
	{
		printf ("error: file index overflow!\n");
		return (1);
	}

	files[file_index].linenum = 0;
	strcpy ((char *) files[file_index].name, (const char *) rbuf);
	file_inside = 1;

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

			if (file_inside == 0)
			{
				// inside of main file add line:
				linenum++;
			}
			else
			{
				files[file_index].linenum++;
			}

			if (documentation_on == 1 && pass == 1)
			{
				pos = searchstr (buf, (U1 *) DOCU_END_SB, 0, 0, TRUE);
				if (pos >= 0)
			    {
				   // begin of documentation blog, set flag
				   documentation_on = 0;
				   continue;
			    }

				if (fprintf (docuptr, "%s", buf) < 0)
				{
					printf ("ERROR: can't write to documentation file!\n");
				}

				documentation_write = 1;
				continue;
			}

			pos = searchstr (buf, (U1 *) REPLACE_SB, 0, 0, TRUE);
			if (pos >= 0)
			{
				replace = 1;
				continue;
			}

			pos = searchstr (buf, (U1 *) NO_REPLACE_SB, 0, 0, TRUE);
			if (pos >= 0)
			{
				replace = 0;
				continue;
			}

			pos = searchstr (buf, (U1 *) COMMENT_SB, 0, 0, TRUE);
			if (pos >= 0)
			{
				// line is comment, skip it!
				if (fprintf (foutptr, "\n") < 0)
				{
					printf ("ERROR: can't write to output file!\n");
					fclose (fincludeptr);
					return (1);
				}

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

			pos = searchstr (buf, (U1 *) DOCU_START_SB, 0, 0, TRUE);
			if (pos >= 0)
			{
				// begin of documentation blog, set flag
				documentation_on = 1;
				continue;
			}

			pos = searchstr (buf, (U1 *) "#", 0, 0, TRUE);
			if (pos < 0)
			{
				// no definition start found, check replace
				// check if define is set
                if (replace == 1)
				{
					pos = searchstr (buf, (U1 *) ":", 0, 0, TRUE);
					if (pos >= 0)
					{
						// check function name to variable name tackon
					    replace_varname (buf);

						if (replace_macro_normal (buf) != 0)
					    {
							return_error = 1;
					    }
					}
					else
					{
						replace_define (buf);

						// check function name to variable name tackon
					    replace_varname (buf);

						// check if macro is set
						if (replace_macro (buf) != 0)
					    {
							return_error = 1;
					    }
					}
				}
			}

			pos = searchstr (buf, (U1 *) DEFINE_SB, 0, 0, TRUE);
            if (pos >= 0)
			{
				// printf ("DEBUG: got #define!\n");

				if (set_define (buf) != 0)
				{
					fclose (finptr);
					fclose (foutptr);
					fclose (docuptr);
					exit (1);
				}

				// write empty line
				if (fprintf (foutptr, "\n") < 0)
				{
					printf ("ERROR: can't write to output file!\n");
					fclose (finptr);
					fclose (foutptr);
					fclose (docuptr);
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
					fclose (docuptr);
					exit (1);
				}

				// write empty line
				if (fprintf (foutptr, "\n") < 0)
				{
					printf ("ERROR: can't write to output file!\n");
					fclose (finptr);
					fclose (foutptr);
					fclose (docuptr);
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
					fclose (docuptr);
					exit (1);
				}
				// replace all @ with the variable end as defined in func var macro
				var_tackon_set = 1;

				// write empty line
				if (fprintf (foutptr, "\n") < 0)
				{
					printf ("ERROR: can't write to output file!\n");
					fclose (finptr);
					fclose (foutptr);
					fclose (docuptr);
					exit (1);
				}

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

            // check for variable definition
		    pos = searchstr (buf, (U1 *) SET_SB, 0, 0, TRUE);
			if (pos >= 0)
			{
				if (parse_set (buf, setret) != 0)
				{
					fclose (finptr);
					fclose (foutptr);
					fclose (docuptr);
					exit (1);
				}
			}

            // check for "for)"
            pos = searchstr (buf, (U1 *) FOR_LOOP_SB, 0, 0, TRUE);
            if (pos >= 0)
			{
				if (for_loop == 0)
				{
					// insert "(for-loop)" into program
					if (fprintf (foutptr, "%s", FOR_LOOP_SET_SB) < 0)
					{
						printf ("ERROR: can't write to output file!\n");
						fclose (finptr);
						fclose (foutptr);
						fclose (docuptr);
						exit (1);
					}

					for_loop = 0;  // reset flag
				}
			}

			pos = searchstr (buf, (U1 *) FOR_LOOP_SET_SB, 0, 0, TRUE);
            if (pos >= 0)
			{
				for_loop = 1;     // set flag to on
			}

            /* No warning on include files?
			if (flag_wdeprecated == 1)
			{
				pos = searchstr (buf, (U1 *) INTR0_SB, 0, 0, TRUE);
				if (pos >= 0)
				{
					printf ("Warning: direct use of intr0 is deprecated!\n");
					printf ("%s\n", buf);
					printf ("Use intr.l1h or intr-func.l1h include.\n");
				}

			    pos = searchstr (buf, (U1 *) INTR1_SB, 0, 0, TRUE);
				if (pos >= 0)
				{
					printf ("Warning: direct use of intr0 is deprecated!\n");
					printf ("%s\n", buf);
					printf ("Use intr.l1h or intr-func.l1h include.\n");
				}
			}
			*/

			pos = searchstr (buf, (U1 *) INCLUDE_SB, 0, 0, TRUE);
            if (pos >= 0)
			{
				if (include_file (buf) != 0)
				{
					fclose (finptr);
					fclose (foutptr);
					fclose (docuptr);
					exit (1);
				}
			}
			else
			{
				// check for multiline macro
				multi_pos = searchstr (buf, (U1 *) MULTILINE_SB, 0, 0, TRUE);
				if (multi_pos >=  0)
				{
					multi_len = strlen_safe ((const char *) buf, MAXLINELEN);
					if (multi_len > 1)
					{
						for (pos = 0; pos < multi_len - 1; pos++)
						{
							if (buf[pos] == '@')
							{
								if (buf[pos + 1] == '#')
								{
									// found @# in line, print new line char 10

									if (fprintf (foutptr, "%c", 10) < 0)
									{
										printf ("ERROR: can't write to output file!\n");
										fclose (finptr);
										fclose (foutptr);
										fclose (docuptr);
										exit (1);
									}
									pos = pos + 1;
								}
								else
							    {
									if (fprintf (foutptr, "%c", buf[pos]) < 0)
									{
										printf ("ERROR: can't write to output file!\n");
										fclose (finptr);
										fclose (foutptr);
										fclose (docuptr);
										exit (1);
									}
								}
							}
							else
							{
								if (fprintf (foutptr, "%c", buf[pos]) < 0)
								{
									printf ("ERROR: can't write to output file!\n");
									fclose (finptr);
									fclose (foutptr);
									fclose (docuptr);
									exit (1);
								}
							}
						}
					    // print new line char 10 at end of line

						if (fprintf (foutptr, "%c", 10) < 0)
						{
							printf ("ERROR: can't write to output file!\n");
							fclose (finptr);
							fclose (foutptr);
							fclose (docuptr);
							exit (1);
						}
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
						fclose (docuptr);
						exit (1);
					}

					// write string is immutable interrupt
					if (strlen_safe ((const char *) setret, MAXLINELEN) != 0)
				    {
						if (fprintf (foutptr, "%s", setret) < 0)
				        {
							printf ("ERROR: can't write to output file!\n");
							fclose (finptr);
							fclose (foutptr);
							fclose (docuptr);
							exit (1);
						}
					}
				}
			}
		}
		else
		{
			ok = FALSE;
		}
	}

	// save incude file END in output file
	if (fprintf (foutptr, "FILE END\n") < 0)
	{
		printf ("ERROR: can't write to output file!\n");
		fclose (finptr);
		fclose (foutptr);
		fclose (docuptr);
		exit (1);
	}

    if (file_index > 0)
	{
		file_index--;
	}
    if (file_index == 0)
	{
		file_inside = 0;
	}

	fclose (fincludeptr);
	return (0);
}

void show_info (void)
{
	printf ("l1pre <file-in> <file-out> <include-path> [optional-include-path]\n");
	printf ("\nPreprocessor for Brackets, a programming language with brackets ;-)\n");
	printf ("%s", VM_VERSION_STR);
	printf ("%s\n", COPYRIGHT_STR);
}

S2 check_if_l1com_file (char *file)
{
	FILE *fptr;
	U1 rbuf[MAXSTRLEN + 1];
    U1 ok = TRUE;
	char *read;
	S4 pos;

	fptr = fopen (file, "r");
	if (fptr == NULL)
	{
		// error can't open input file
		printf ("ERROR: can't open input file: '%s' !\n", file);
		return (1);
	}

	// scan file for: (main func)
	// read input line loop
	ok = TRUE;
	while (ok)
	{
		read = fgets_uni ((char *) rbuf, MAXLINELEN, fptr);
        if (read != NULL)
        {
			pos = searchstr (rbuf, (U1 *) "(main func)", 0, 0, TRUE);
			if (pos >= 0)
			{
				// found Brackets main start, allo ok!
			    fclose (fptr);
				return (0);
			}
		}
		else
		{
			ok = FALSE;
		}
	}
	fclose (fptr);
	// no (main func) Brackets main found!!
	return (1);
}

S2 get_varname_type (U1 *name)
{
    S8 i ALIGN = 0;

    for (i = 0; i <= vars_ind; i++)
    {
        if (strcmp ((const char *) vars[i].name, (const char *) name) == 0)
        {
            // found variable, return variable type
            return (vars[i].type);
        }
    }
    // variable not found
    return (-1);
}

U1 getvartype (U1 *type)
{
    U1 vartype = 0;

    if (strcmp ((const char *) type, "byte") == 0)
    {
        vartype = BYTE;
    }
    if (strcmp ((const char *) type, "const-byte") == 0)
    {
        vartype = BYTE;
    }
    if (strcmp ((const char *) type, "mut-byte") == 0)
    {
        vartype = BYTE;
    }
    if (strcmp ((const char *) type, "string") == 0)
    {
        vartype = VARTYPE_STRING;
    }
    if (strcmp ((const char *) type, "const-string") == 0)
    {
        vartype = STRING_CONST;
    }
    if (strcmp ((const char *) type, "mut-string") == 0)
    {
        vartype = VARTYPE_STRING;
    }

    if (strcmp ((const char *) type, "int16") == 0)
    {
        vartype = WORD;
    }
    if (strcmp ((const char *) type, "const-int16") == 0)
    {
        vartype = WORD;
    }
    if (strcmp ((const char *) type, "mut-int16") == 0)
    {
        vartype = WORD;
    }

    if (strcmp ((const char *) type, "int32") == 0)
    {
        vartype = DOUBLEWORD;
    }
    if (strcmp ((const char *) type, "const-int32") == 0)
    {
        vartype = DOUBLEWORD;
    }
    if (strcmp ((const char *) type, "mut-int32") == 0)
    {
        vartype = DOUBLEWORD;
    }

    if (strcmp ((const char *) type, "int64") == 0)
    {
        vartype = QUADWORD;
    }
    if (strcmp ((const char *) type, "const-int64") == 0)
    {
        vartype = QUADWORD;
    }
    if (strcmp ((const char *) type, "mut-int64") == 0)
    {
        vartype = QUADWORD;
    }

    if (strcmp ((const char *) type, "double") == 0)
    {
        vartype = DOUBLEFLOAT;
    }
    if (strcmp ((const char *) type, "const-double") == 0)
    {
        vartype = DOUBLEFLOAT;
    }
    if (strcmp ((const char *) type, "mut-double") == 0)
    {
        vartype = DOUBLEFLOAT;
    }

    if (strcmp ((const char *) type, "bool") == 0)
    {
        vartype = BOOL;
    }
    if (strcmp ((const char *) type, "const-bool") == 0)
    {
        vartype = BOOL;
    }
    if (strcmp ((const char *) type, "mut-bool") == 0)
    {
        vartype = BOOL;
    }

    if (strcmp ((const char *) type, "none") == 0)
    {
        vartype = VARTYPE_NONE;
    }

    return (vartype);
}

S2 parse_set (U1 *line, U1 *setret)
{
	S2 pos, type_pos, type_ind = 0, i;
    S2 name_pos, name_ind = 0;
    S2 line_len = 0;
    U1 get_type = 1;
    U1 get_name = 1;
    S2 vartype = 0;
    S2 type_len = 0;
    S2 spaces = 0;
    S2 func_pos = 0;
    U1 type[MAXSTRLEN + 1];
    U1 name[MAXSTRLEN + 1];

    line_len = strlen_safe ((const char *) line, MAXSTRLEN);

    //printf ("parse_set: '%s'\n", line);

    // check if set found
    pos = searchstr (line, (U1 *) "(set", 0, 0, TRUE);
    if (pos != -1)
    {
        func_pos = searchstr (line, (U1 *) "func)", 0, 0, TRUE);
        if (func_pos != -1)
        {
            return (0);   // function define, return!
        }

        if (vars_ind >= MAXVARS)
        {
            printf ("parse_set: variables overflow!\n");
            return (1);
        }

        type_pos = pos + 5;
        i = type_pos;

        #if DEBUG
        printf ("parse_set: set: type pos: %i, char: '%c'\n", type_pos, line[i]);
        #endif

        while (get_type == 1)
        {
            if (line[i] != ' ')
            {
                if (type_ind >= MAXSTRLEN)
                {
                    printf ("parse_set: set type overflow!\n");
                    return (1);
                }
                type[type_ind] = line[i];
                type_ind++;
            }
            else
            {
                // got end of type
                type[type_ind] = '\0';
                get_type = 0;
            }
            if (i < line_len - 1)
            {
                i++;
            }
            else
            {
                printf ("parse_set: line overflow!\n");
                return (1);
            }
        }

		#if DEBUG
        printf ("parse_set: set: vartype: %s\n", type);
        #endif

        // get vartype
        vartype = getvartype (type);
        if (vartype == 0)
        {
            // error no valid vartype
            printf ("parse_set: set: invalid variable type: %s ! \n", type);
            return (1);
        }

        type_len = strlen_safe ((const char *) type, MAXSTRLEN);
        name_pos = type_pos + type_len;


        #if DEBUG
        printf ("parse_set: name search start pos: %i\n", name_pos);
        #endif

        spaces = 0;
        i = name_pos;
        while (get_name == 1)
        {
            if (line[i] == ' ')
            {
                spaces++;
            }

            if (spaces == 2)
            {
                name_pos = i + 1;
                get_name = 0;
            }

            if (i < line_len - 1)
            {
                i++;
            }
            else
            {
                printf ("parse_set: line overflow!\n");
                return (1);
            }
        }

        get_name = 1;
        i = name_pos;
        name_ind = 0;

        #if DEBUG
        printf ("variable name start pos: %i\n", i);
        #endif

        while (get_name == 1)
        {
            if (line[i] != ' ' && line[i] != ')')
            {
                if (name_ind >= MAXSTRLEN)
                {
                    printf ("parse_set: set name overflow!\n");
                    return (1);
                }
                name[name_ind] = line[i];
                name_ind++;
            }
            else
            {
                // got end of name
                name[name_ind] = '\0';
                get_name = 0;
            }

            if (i < line_len - 1)
            {
                i++;
            }
            else
            {
                 // got end of name
                name[name_ind] = '\0';
                get_name = 0;
            }
        }

		//printf ("DEBUG: parse_set variable name: '%s'\n", name);
		//printf ("DEBUG: parse_set: set: vartype: %s\n", type);

		if (vartype == STRING_CONST)
		{
            strcpy ((char *) setret, "(34 ");
			strcat ((char *) setret, (const char *) name);
			strcat ((char *) setret, "addr 0 0 intr0)\n");

			// printf ("DEBUG: parse_set: setret: '%s'\n", setret);
		}
		else
		{
			strcpy ((char *) setret, "");
		}

        //printf ("saving variable...\n");

        if (vars_ind < MAXVARS - 1)
        {
            vars_ind++;
            strcpy ((char *) vars[vars_ind].name, (char *) name);
            vars[vars_ind].type = vartype;

            #if DEBUG
            printf ("variable: index: %lli, name: '%s', type: %lli\n\n\n", vars_ind, vars[vars_ind].name, vars[vars_ind].type);
            #endif
        }
        else
        {
            printf ("parse_set: set variable overflow!\n");
            return (1);
        }
    }
	return (0);
}

S2 check_call (U1 *line, S4 pos_call)
{
	S4 start_label = 0;
	S4 pos = 0;
	U1 check = 0;
	U1 label_name_def = 0;

	start_label = searchstr (line, (U1 *) LABEL_SB, 0, 0, TRUE);
	if (start_label < 0)
	{
		// error no colon as label start found!
		return (1);
	}

	// check if label name is before ! or call
	pos = start_label + 1;
	while (check == 0)
	{
		if (pos < pos_call)
		{
			if (line[pos] != ' ')
			{
				label_name_def = 1;
			}
			pos++;
		}
		else
		{
			check = 1;
		}
	}

	if (label_name_def == 0)
	{
		// return error, no label name set!
		return (1);
	}
	else
	{
		return (0);
	}
}

int main (int ac, char *av[])
{
	U1 rbuf[MAXSTRLEN + 1];                        /* read-buffer for one line */
	U1 buf[MAXSTRLEN + 1];
	U1 setret[MAXSTRLEN + 1];

	char *read;
	U1 ok;
	S4 pos;
    S4 slen;

	// multiline
	S4 multi_pos;
	U1 multi_len;

	if (ac < 4)
	{
		show_info ();
		exit (1);
	}

	// file check:
	if (check_if_l1com_file (av[1]) == 1)
	{
		printf ("ERROR: no Brackets main function found!\nIs this not a Brackets soure code file?\n");
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

	foutptr = fopen ("pass-1.l1com", "w");
	if (foutptr == NULL)
	{
		// error can't open input file
		printf ("ERROR: can't open output file: pass-1.l1com !\n");
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
	strcpy ((char *) include_path_two, "");

	// open documentation file for output
	docuptr = fopen ("out.md", "w");
	if (docuptr == NULL)
	{
		printf ("ERROR: can't open documentation file 'out.md' !\n");
		fclose (finptr);
		fclose (foutptr);
		exit (1);
	}

	if (ac == 5)
	{
		if (strlen_safe ((const char *) av[4], MAXLINELEN) > MAXLINELEN - 1)
		{
			printf ("ERROR: include path: '%s' too long!\n", av[4]);
			fclose (finptr);
			fclose (foutptr);
			fclose (docuptr);
			exit (1);
		}
		strcpy ((char *) include_path_two, av[4]);
		printf ("second include path: '%s'\n", include_path_two);
	}

	if (ac == 6)
	{
		if (strcmp ((const char *) av[5], "-wdeprecated") == 0)
		{
			flag_wdeprecated = 1;
		}

		if (strcmp ((const char *) av[5], "-wspaces") == 0)
		{
			error_multi_spaces = 1;
		}
	}

	if (ac == 7)
	{
		if (strcmp ((const char *) av[6], "-wspaces") == 0)
		{
			error_multi_spaces = 1;
		}
	}

	run_loop:
	// read input line loop
	ok = TRUE;
	while (ok)
	{
		// parse_set: string constant interrupt return
		strcpy ((char *) setret, "");

		read = fgets_uni ((char *) rbuf, MAXLINELEN, finptr);
        if (read != NULL)
        {
			strcpy ((char *) buf, (const char *) rbuf);
			convtabs (buf);
			slen = strlen_safe ((const char *) buf, MAXLINELEN);

			if (file_inside == 0)
			{
				// inside of main file add line:
				linenum++;
			}
			else
			{
				files[file_index].linenum++;
			}

			if (error_multi_spaces == 1)
			{
				if (do_check_spaces (buf) != 0)
				{
					printf ("error: found double spaces!\n");

					if (file_inside == 0)
					{
						printf ("line: %lli\n", linenum);
					}
					else
					{
						printf ("file: %s: line: %lli\n", files[file_index].name, files[file_index].linenum);
					}
					printf ("> %s\n\n", buf);
				}
			}

			if (documentation_on == 1 && pass == 1)
			{
				pos = searchstr (buf, (U1 *) DOCU_END_SB, 0, 0, TRUE);
				if (pos >= 0)
			    {
			     	// begin of documentation blog, set flag
			    	documentation_on = 0;
				    continue;
			    }

				if (fprintf (docuptr, "%s", rbuf) < 0)
				{
					printf ("ERROR: can't write to documentation file!\n");
				}

				documentation_write = 1;
				continue;
			}

			if (flag_wdeprecated == 1 && pass == 1)
			{
				pos = searchstr (buf, (U1 *) INTR0_SB, 0, 0, TRUE);
				if (pos >= 0)
				{
					printf ("Warning: direct use of intr0 is deprecated!\n");
					printf ("Use intr.l1h or intr-func.l1h include.\n");

					if (file_inside == 0)
					{
						printf ("line: %lli\n", linenum);
					}
					else
					{
						printf ("file: %s: line: %lli\n", files[file_index].name, files[file_index].linenum);
					}
					printf ("> %s\n\n", buf);

				}

			    pos = searchstr (buf, (U1 *) INTR1_SB, 0, 0, TRUE);
				if (pos >= 0)
				{
					printf ("Warning: direct use of intr1 is deprecated!\n");
					printf ("Use intr.l1h or intr-func.l1h include.\n");

					if (file_inside == 0)
					{
						printf ("line: %lli\n", linenum);
					}
					else
					{
						printf ("file: %s: line: %lli\n", files[file_index].name, files[file_index].linenum);
					}
					printf ("> %s\n\n", buf);
				}
			}

			pos = searchstr (buf, (U1 *) REPLACE_SB, 0, 0, TRUE);
			if (pos >= 0)
			{
				replace = 1;
				continue;
			}

			pos = searchstr (buf, (U1 *) NO_REPLACE_SB, 0, 0, TRUE);
			if (pos >= 0)
			{
				replace = 0;
				continue;
			}

			pos = searchstr (buf, (U1 *) COMMENT_SB, 0, 0, TRUE);
			if (pos >= 0)
			{
				// line is comment, skip it!
				if (fprintf (foutptr, "\n") < 0)
				{
					printf ("ERROR: can't write to output file!\n");
					fclose (finptr);
					fclose (foutptr);
					fclose (docuptr);
					return (1);
				}

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
					fclose (docuptr);
					exit (1);
				}
				continue;
			}

			pos = searchstr (buf, (U1 *) DOCU_START_SB, 0, 0, TRUE);
			if (pos >= 0)
			{
				// begin of documentation blog, set flag
				documentation_on = 1;
				continue;
			}

			// printf ("'%s'\n", buf);

			pos = searchstr (buf, (U1 *) "#", 0, 0, TRUE);
			if (pos < 0)
			{
				// no definition start found, check replace
				// check if define is set
				if (replace == 1)
				{
					pos = searchstr (buf, (U1 *) ":", 0, 0, TRUE);
					if (pos >= 0)
					{
						// check function name to variable name tackon
					    replace_varname (buf);

						if (replace_macro_normal (buf) != 0)
					    {
							return_error = 1;
					    }
					}
					else
					{
						replace_define (buf);

						// check function name to variable name tackon
					    replace_varname (buf);

						// check if macro is set
						if (replace_macro (buf) != 0)
					    {
							return_error = 1;
					    }
					}
				}
			}

			pos = searchstr (buf, (U1 *) CLEAR_DEFINES_SB, 0, 0, TRUE);
            if (pos >= 0)
			{
				clear_defines ();

				// write empty line
				if (fprintf (foutptr, "\n") < 0)
				{
					printf ("ERROR: can't write to output file!\n");
					fclose (finptr);
					fclose (foutptr);
					fclose (docuptr);
					exit (1);
				}

				continue;
			}

			pos = searchstr (buf, (U1 *) DEFINE_SB, 0, 0, TRUE);
            if (pos >= 0)
		 	{
				// printf ("DEBUG: got #define!\n");

				if (set_define (buf) != 0)
				{
					fclose (finptr);
					fclose (foutptr);
					fclose (docuptr);
					exit (1);
				}

				// write empty line
				if (fprintf (foutptr, "\n") < 0)
				{
					printf ("ERROR: can't write to output file!\n");
					fclose (finptr);
					fclose (foutptr);
					fclose (docuptr);
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
					if (file_inside == 0)
					{
						printf ("line: %lli\n", linenum);
					}
					else
					{
						printf ("file: %s: line: %lli\n", files[file_index].name, files[file_index].linenum);
					}
					printf ("> %s\n\n", buf);

					fclose (finptr);
					fclose (foutptr);
					fclose (docuptr);
					exit (1);
				}

				// write empty line
				if (fprintf (foutptr, "\n") < 0)
				{
					printf ("ERROR: can't write to output file!\n");
					fclose (finptr);
					fclose (foutptr);
					fclose (docuptr);
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
					if (file_inside == 0)
					{
						printf ("line: %lli\n", linenum);
					}
					else
					{
						printf ("file: %s: line: %lli\n", files[file_index].name, files[file_index].linenum);
					}
					printf ("> %s\n\n", buf);

					fclose (finptr);
					fclose (foutptr);
					fclose (docuptr);
					exit (1);
				}
				// replace all @ with the variable end as defined in func var macro
				var_tackon_set = 1;

				// write empty line
				if (fprintf (foutptr, "\n") < 0)
				{
					printf ("ERROR: can't write to output file!\n");
					fclose (finptr);
					fclose (foutptr);
					fclose (docuptr);
					exit (1);
				}

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

		    // check for variable definition
		    pos = searchstr (buf, (U1 *) SET_SB, 0, 0, TRUE);
			if (pos >= 0)
			{
				if (parse_set (buf, setret) != 0)
				{
					if (file_inside == 0)
					{
						printf ("line: %lli\n", linenum);
					}
					else
					{
						printf ("file: %s: line: %lli\n", files[file_index].name, files[file_index].linenum);
					}
					printf ("> %s\n\n", buf);

					fclose (finptr);
					fclose (foutptr);
					fclose (docuptr);
					exit (1);
				}
			}

            // check for "for)"
            pos = searchstr (buf, (U1 *) FOR_LOOP_SB, 0, 0, TRUE);
            if (pos >= 0)
			{
				if (for_loop == 0)
				{
					// insert "(for-loop)" into program
					if (fprintf (foutptr, "%s\n", FOR_LOOP_SET_SB) < 0)
					{
						printf ("ERROR: can't write to output file!\n");
						fclose (finptr);
						fclose (foutptr);
						fclose (docuptr);
						exit (1);
					}

					for_loop = 0;  // reset flag
				}
			}

			pos = searchstr (buf, (U1 *) FOR_LOOP_SET_SB, 0, 0, TRUE);
            if (pos >= 0)
			{
				for_loop = 1;     // set flag to on
			}

			{
				U1 call_set = 0;
				S4 pos_call = 0;
				S2 ret = 0;
			    pos = searchstr (buf, (U1 *) CALL_SHORT_SB, 0, 0, TRUE);
                if (pos >= 0)
				{
					call_set = 1;
					pos_call = pos;
				}
				if (call_set == 0)
				{
					pos = searchstr (buf, (U1 *) CALL_SB, 0, 0, TRUE);
					if (pos >= 0)
					{
						call_set = 1;
						pos_call = pos;
					}
				}
				if (call_set == 1)
				{
					// call or ! set in current line
					// check if : is set as label name start
					ret = check_call (buf, pos_call);
					if (ret == 1)
					{
						if (file_inside == 0)
						{
							printf ("line: %lli\n", linenum);
						}
						else
						{
							printf ("file: %s: line: %lli\n", files[file_index].name, files[file_index].linenum);
						}

						// error!!!
						printf ("line: %lli: '%s' error: no label name set!\n", linenum, buf);
					}
				}
			}

            pos = searchstr (buf, (U1 *) INCLUDE_SB, 0, 0, TRUE);
            if (pos >= 0)
			{
				if (include_file (buf) != 0)
				{
					fclose (finptr);
					fclose (foutptr);
					fclose (docuptr);
					exit (1);
				}
			}
			else
			{
				// check for multiline macro
				multi_pos = searchstr (buf, (U1 *) MULTILINE_SB, 0, 0, TRUE);
				if (multi_pos >=  0)
				{
					multi_len = strlen_safe ((const char *) buf, MAXLINELEN);
					if (multi_len > 1)
					{
						for (pos = 0; pos < multi_len - 1; pos++)
						{
							if (buf[pos] == '@')
							{
								if (buf[pos + 1] == '#')
								{
									// found @# in line, print new line char 10

									if (fprintf (foutptr, "%c", 10) < 0)
									{
										printf ("ERROR: can't write to output file!\n");
										fclose (finptr);
										fclose (foutptr);
										fclose (docuptr);
										exit (1);
									}
									pos = pos + 1;
								}
								else
							    {
									if (fprintf (foutptr, "%c", buf[pos]) < 0)
									{
										printf ("ERROR: can't write to output file!\n");
										fclose (finptr);
										fclose (foutptr);
										fclose (docuptr);
										exit (1);
									}
								}
							}
							else
							{
								if (fprintf (foutptr, "%c", buf[pos]) < 0)
								{
									printf ("ERROR: can't write to output file!\n");
									fclose (finptr);
									fclose (foutptr);
									fclose (docuptr);
									exit (1);
								}
							}
						}
					    // print new line char 10 at end of line

						if (fprintf (foutptr, "%c", 10) < 0)
						{
							printf ("ERROR: can't write to output file!\n");
							fclose (finptr);
							fclose (foutptr);
							fclose (docuptr);
							exit (1);
						}
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
						fclose (docuptr);
						exit (1);
					}

					// write string is immutable interrupt
					if (strlen_safe ((const char *) setret, MAXLINELEN) != 0)
					{
						if (fprintf (foutptr, "%s", setret) < 0)
						{
							printf ("ERROR: can't write to output file!\n");
							fclose (finptr);
							fclose (foutptr);
							fclose (docuptr);
							exit (1);
						}
					}
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
	fclose (docuptr);

	if (pass == 1)
	{
		pass = 2;

		file_index = 0;
		file_inside = 0;
		linenum = 0;

		// open pass-1.l1com file
		// open input file
		finptr = fopen ("pass-1.l1com", "r");
		if (finptr == NULL)
		{
			// error can't open input file
			printf ("ERROR: can't open input file: pass-1.l1com !\n");
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

		// open documentation file for output
		docuptr = fopen ("tmp.md", "w");
		if (docuptr == NULL)
		{
			printf ("ERROR: can't open docmentation file 'out.md' !\n");
			fclose (finptr);
			fclose (foutptr);
			exit (1);
		}

		goto run_loop;
	}

	// check if text was written into docu file
	if (documentation_write == 0)
	{
		// no text written delete empty "out.md" file
		remove ("out.md");
	}

	if (return_error != 0)
	{
		printf ("ERROR: found errors!\n");
	}

	exit (return_error);
}
