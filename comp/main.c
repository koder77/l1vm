/*
 * This file main.c is part of L1vm.
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

//  l1com RISC compiler
//

#include "../include/global.h"
#include "../include/opcodes.h"
#include "translate.h"
#include "main.h"
#include "if.h"

S8 linenum ALIGN = 1;

S8 label_ind ALIGN = -1;
S8 call_label_ind ALIGN = -1;

struct ast ast[MAXBRACKETLEVEL];
S8 ast_level ALIGN;

S8 data_ind ALIGN = -1;

// assembly text output
U1 **data = NULL;
U1 **code = NULL;

S8 line_len ALIGN = MAXLINES;

S8 data_line ALIGN = 0;
S8 code_line ALIGN = 0;

U1 regi[MAXREG][MAXLINELEN];
U1 regd[MAXREG][MAXLINELEN];

U1 save_regi[MAXREG];
U1 save_regd[MAXREG];

struct t_var t_var;
struct data_info data_info[MAXDATAINFO];
struct label label[MAXLABELS];
struct call_label call_label[MAXLABELS];

U1 inline_asm = 0;		// set to one, if inline assembly is used

U1 optimize_if = 0;		// set to one to optimize if call

// automatic set loadreg if no more stpopi or stpopd opcode is used
U1 call_set = 0;
U1 call_loadreg = 0;

void init_ast (void)
{
	S4 i, j;
	for (i = 0; i < MAXBRACKETLEVEL; i++)
	{
		for (j = 0; j < MAXEXPRESSION; j++)
		{
			ast[i].expr_reg[j] = -2;
			ast[i].expr_type[j] = -2;
		}
	}
}

S2 check_brackets_match (U1 *line)
{
	S4 slen;
	S4 i;
	S4 open_brackets = 0, close_brackets = 0;
	U1 is_space = 0;

	slen = strlen_safe ((const char *) line, MAXLINELEN);

	for (i = 0; i < slen; i++)
	{
		if (line[i] == '{')
		{
			// RPN expression start: return OK
			return (0);
		}
		if (line[i] == '(')
		{
			open_brackets++;
		}
		if (line[i] == ')')
		{
			close_brackets++;
			// check if empty bracket
			if (i > 0)
			{
				if (line[i - 1] == '(')
				{
					// empty bracket found: FATAL ERROR!!!
					return (1);
				}
			}
			if (is_space == 1)
			{
				// empty bracket found: FATAL ERROR!!!
				return (1);
			}
		}
		if (line[i] == ' ')
		{
			is_space = 1;
		}
		else
		{
			if (line[i] != '(' && line[i] != ')')
			{
				is_space = 0;
			}
		}
	}
	if (open_brackets != close_brackets)
	{
		return (1);		// ERROR
	}
	else
	{
		return (0); 	// all ok
	}
}

S2 get_ast (U1 *line, U1 *parse_cont)
{
	S4 slen;
	S4 pos = 0, argstart;

	U1 ok = 0;
	U1 arg = 0;
	slen = strlen_safe ((const char *) line, MAXLINELEN);

	S4 arg_ind = -1, arg_pos;
	S4 ast_ind = -1;
	ast_level = -1;
	S4 exp_ind = 0;

	// set to true if continous variable number sequence found:
	// {x + y * z a =}
	*parse_cont = 0;

	S4 i;
	for (i = 0; i < MAXBRACKETLEVEL; i++)
	{
		ast[i].expr_max = 0;
	}

	if (check_brackets_match (line) == 1)
	{
		printf ("error: line %lli: brackets don't match!\n", linenum);
		// printf ("DEBUG get_ast\n");
		return (2);
	}

	// printf ("> '%s'\n", line);

	while (! ok)
	{
		// printf ("top: %c\n", line[pos]);
		if (line[pos] == ' ')
		{
			pos++;

			if (pos >= slen)
			{
				ok = 1;
			}
		}
		else
		{
			if (line[pos] == '(' || line[pos] == '{' )
			{
				// check if continous variable assign:
				if (line[pos] == '{')
				{
					*parse_cont = 1;
					// printf ("DEBUG get_ast: parse_cont return\n");
					return (0);
				}

				ast_ind++;
				if (ast_level < ast_ind)
				{
					ast_level = ast_ind;
				}

				#if DEBUG
				printf ("AST bracket level: %lli\n", ast_level);
				#endif

				pos++;
				if (pos >= slen)
				{
					ok = 1;
					break;
				}
			}
			else
			{
				argstart = pos;
				arg_pos = 0;
				arg_ind++;
				arg = 0;
				exp_ind = ast[ast_ind].expr_max;

				while (! arg)
				{
					if (line[pos] == '"')
					{
						ast[ast_ind].expr[exp_ind][arg_ind][arg_pos] = line[pos];
						arg_pos++;
						if (arg_pos >= MAXLINELEN)
						{
							printf ("error: line %lli: argument too long!\n", linenum);
							return (1);
						}
						pos++;
						while (line[pos] != '"')
						{
							ast[ast_ind].expr[exp_ind][arg_ind][arg_pos] = line[pos];
							arg_pos++;
							if (arg_pos >= MAXLINELEN)
							{
								printf ("error: line %lli: argument too long!\n", linenum);
								return (1);
							}
							pos++;

							if (pos >= slen)
							{
								ok = 1;
								break;
							}
						}

						ast[ast_ind].expr[exp_ind][arg_ind][arg_pos] = '"';
						arg_pos++;
						if (arg_pos >= MAXLINELEN)
						{
							printf ("error: line %lli: argument too long!\n", linenum);
							return (1);
						}

						pos++;
						ast[ast_ind].expr[exp_ind][arg_ind][arg_pos] = '\0';
						ast[ast_ind].expr_max = exp_ind;
						arg = 1;

						// printf ("get_ast: string: '%s'\n", ast[ast_ind].expr[exp_ind][arg_ind]);

						break;
					}
					else
					{
						if (line[pos] != ' ' && line[pos] != ',' && line[pos] != '\n' && line[pos] != ')' && line[pos] != '}')
						{
							// BUGFIX -> avoid write byte error in valgrind.
							// do check of all index varaibles!!!
							if (ast_ind >= 0 && exp_ind >= 0 && arg_ind >= 0 && arg_pos >= 0)
							{
								ast[ast_ind].expr[exp_ind][arg_ind][arg_pos] = line[pos];
								arg_pos++;
								if (arg_pos >= MAXLINELEN)
								{
									printf ("error: line %lli: argument too long!\n", linenum);
									return (1);
								}
							}
						}
						else
						{
							if (ast_ind == -1)
							{
								printf ("error: line %lli: brackets don't match!\n", linenum);
								return (2);
							}
							// printf ("ast_ind: %i, exp_ind: %i, arg_ind: %i, arg_pos: %i\n", ast_ind, exp_ind, arg_ind, arg_pos);

							// BUGFIX -> avoid write byte error in valgrind.
							// do check of all index varaibles!!!
							if (ast_ind >= 0 && exp_ind >= 0 && arg_ind >= 0 && arg_pos >= 0)
							{
								ast[ast_ind].expr[exp_ind][arg_ind][arg_pos] = '\0';
								// printf ("[ %s ]\n", ast[ast_ind].expr[exp_ind][arg_ind]);
							}

							if (line[pos] == ')' || line[pos] == '}')
							{
								// next char is open bracket, new expression next
								// BUGFIX -> avoid write byte error in valgrind.
								// do check of all index varaibles!!!
								if (ast_ind >= 0 && exp_ind >= 0)
								{
									ast[ast_ind].expr_args[exp_ind] = arg_ind;
									ast[ast_ind].expr_max = exp_ind;
									exp_ind++; arg_ind = -1;
									if (exp_ind >= MAXEXPRESSION)
									{
										printf ("error: line %lli: too much expressions!\n", linenum);
										return (1);
									}

									ast[ast_ind].expr_max = exp_ind;
									ast[ast_ind].expr_args[exp_ind] = arg_ind;

									ast_ind--; arg_ind = -1;
								}
							}
							arg = 1;

							if (pos == slen - 2)
							{
								// end of line
								ok = 1;
								break;
							}
						}
					}
					pos++;

					if (pos == slen)
					{
						ok = 1;
						break;
					}
				}

				if (arg_ind >= MAXARGS)
				{
					printf ("error: line %lli: too much arguments!\n", linenum);
					return (1);
				}
			}
		}
	}
	if (ast_ind != -1)
	{
		printf ("error: line %lli: brackets don't match!\n", linenum);
		return (2);
	}
	return (0);
}

S2 get_label_pos (U1 *labelname)
{
	S8 i ALIGN;
	S8 label_index ALIGN = -1;

	for (i = 0; i < MAXLABELS; i++)
	{
		if (strcmp ((const char *) label[i].name, (const char *) labelname) == 0)
		{
			label_index = i;
			break;
		}
	}
	return (label_index);
}

U1 *get_variable_value (S8 maxind, U1 *name)
{
	S8 i ALIGN;

	for (i = 0; i < maxind; i++)
	{
		if (strcmp ((const char *) name, (const char *) data_info[i].name) == 0)
		{
			return (data_info[i].value_str);
		}
	}
	return (NULL);		// variable name not found, return empty string
}

S2 check_for_brackets (U1 *line)
{
	S2 i, len;
	S2 brackets_open = 0, brackets_close = 0;
	S2 brackets_found = 0;

	len = strlen_safe ((const char *) line, MAXLINELEN);
	if (len > 0)
	{
		for (i = 0; i < len; i++)
		{
			if (line[i] == '(')
			{
				brackets_open++;
				brackets_found = 1;
			}
			if (line[i] == ')')
			{
				brackets_close++;
			}
		}
	}
	// check if brackets counter is 0, all opening brackets are closed or not
	if (brackets_open != brackets_close)
	{
		printf ("error: line %lli: brackets don't match!\n", linenum);
		// printf ("DEBUG check_for_brackets\n");
		return (0);
	}
	return (brackets_found);	// no brackets in line
}

S8 loadreg (void)
{
	S8 e;
	U1 str[MAXLINELEN];
	U1 code_temp[MAXLINELEN];

	// load double registers
    for (e = MAXREG - 1; e >= 0; e--)
    {
        if (save_regd[e] == 1)
        {
            strcpy ((char *) code_temp, "stpopd ");
            sprintf ((char *) str, "%lli", e);
            strcat ((char *) code_temp, (const char*) str);
            strcat ((char *) code_temp, "\n");

            code_line++;
            if (code_line >= line_len)
            {
                printf ("error: line %lli: code list full!\n", linenum);
                return (1);
            }

            strcpy ((char *) code[code_line], (const char *) code_temp);
        }
    }

    // load integer registers
    for (e = MAXREG - 1; e >= 0; e--)
    {
        if (save_regi[e] == 1)
        {
            strcpy ((char *) code_temp, "stpopi ");
            sprintf ((char *) str, "%lli", e);
            strcat ((char *) code_temp, (const char*) str);
            strcat ((char *) code_temp, "\n");

            code_line++;
            if (code_line >= line_len)
            {
                printf ("error: line %lli: code list full!\n", linenum);
                return (1);
            }

            strcpy ((char *) code[code_line], (const char *) code_temp);
        }
    }

    init_registers ();
	return (0);
}

S2 parse_line (U1 *line)
{
    S4 level, j, last_arg, last_arg_2, t, v, reg, reg2, reg3, reg4, target, e, exp;
	U1 ok;
	S8 i ALIGN;

	// for convert brackets expression to RPN
	U1 conv[MAXLINELEN];

	U1 str[MAXLINELEN];
	U1 code_temp[MAXLINELEN];

	S8 if_pos ALIGN;
	U1 if_label[MAXLINELEN];
	U1 else_label[MAXLINELEN];
	U1 endif_label[MAXLINELEN];

	S8 while_pos ALIGN;
	U1 while_label[MAXLINELEN];

	S8 for_pos ALIGN;
	U1 for_label[MAXLINELEN];

	S8 switch_pos ALIGN;

	U1 set_loadreg = 0;

	// "=" equal expression track vars:
	S8 found_let ALIGN = 0;
	S8 found_let_cont ALIGN = 0;

	// returned by get_ast ()
	// to parse: { x + y * z a = } like code stuff!
	U1 parse_cont = 0;
	S2 ret;

	// multi line array assign
	U1 array_multi = 0;

	ret = get_ast (line, &parse_cont);
	if (ret == 1)
	{
		// parsing error
		return (1);
	}
	if (ret == 2)
	{
		// fatal ERROR!!! such as brackets don't match in line!!!
		// printf ("DEBUG parse_line: brackets don't match!\n");
		return (2);
	}

	if (parse_cont)
	{
		// printf ("DEBUG parse_cont: '%s'\n", line);

		if (check_for_brackets (line) == 1)
		{
			// found brackets in math expression, convert to RPN
			if (convert (line, conv) == 1)
			{
				printf ("error: line: %lli can't convert infix to RPN!\n", linenum);
				return (1);
			}

			// printf ("DEBUG: parse line: exp: '%s'\n", line);
			// printf ("DEBUG: parse line: RPN: '%s'\n", conv);

			if (parse_rpolish (conv) != 0)
			{
				printf ("error: line: %lli can't parse part in { }\n", linenum);
				return (1);
			}
			return (0);
		}
		else
		{
			if (parse_rpolish (line) != 0)
			{
				printf ("error: line: %lli can't parse part in { }\n", linenum);
				return (1);
			}
			return (0);
		}
	}



	// walking the AST
	for (level = ast_level; level >= 0; level--)
	{
		#if DEBUG
			printf ("level: %i, expr max: %i\n", level, ast[level].expr_max);
		#endif

		if (ast[level].expr_max > -1)
		{
			for (j = 0; j <= ast[level].expr_max; j++)
			{
				#if DEBUG
   				{
					printf ("ast level %i:  j: %i, expression args: %i\n", level, j, ast[level].expr_args[j]);

					S8 exp_ind ALIGN;
					for (exp_ind = 0; exp_ind <= ast[level].expr_args[j]; exp_ind++)
					{
						printf ("exp: ind: %lli, string: '%s'\n", exp_ind, ast[level].expr[j][exp_ind]);
					}
					printf ("\n");
				}
				#endif

				if (ast[level].expr_args[j] > -1 )
				{
					if (strcmp ((const char *) ast[level].expr[j][0], "set") == 0)
					{
						// set data type of variable
						// get variable type, size, name and value

						if (ast[level].expr_args[j] < 3)
						{
							printf ("error: line %lli: wrong data definition!\n", linenum);
							return (1);
						}

						ok = 0;
						data_ind++;
						if (data_ind == MAXDATAINFO)
						{
							printf ("error: line %lli: data list full!\n", linenum);
							return (1);
						}

						// normal variable definition ===========================================
						if (strcmp ((const char *) ast[level].expr[j][1], "byte") == 0)
						{
							data_info[data_ind].type = BYTE;
							data_info[data_ind].type_size = sizeof (U1);
							strcpy ((char *) data_info[data_ind].type_str, "B");
							data_info[data_ind].constant = 0;
							ok = 1;
						}

						if (strcmp ((const char *) ast[level].expr[j][1], "int16") == 0)
						{
							data_info[data_ind].type = WORD;
							data_info[data_ind].type_size = sizeof (S2);
							strcpy ((char *) data_info[data_ind].type_str, "W");
							data_info[data_ind].constant = 0;
							ok = 1;
						}

						if (strcmp ((const char *) ast[level].expr[j][1], "int32") == 0)
						{
							data_info[data_ind].type = DOUBLEWORD;
							data_info[data_ind].type_size = sizeof (S4);
							strcpy ((char *) data_info[data_ind].type_str, "D");
							data_info[data_ind].constant = 0;
							ok = 1;
						}

						if (strcmp ((const char *) ast[level].expr[j][1], "int64") == 0)
						{
							data_info[data_ind].type = QUADWORD;
							data_info[data_ind].type_size = sizeof (S8);
							strcpy ((char *) data_info[data_ind].type_str, "Q");
							data_info[data_ind].constant = 0;
							ok = 1;
						}

						if (strcmp ((const char *) ast[level].expr[j][1], "double") == 0)
						{
							data_info[data_ind].type = DOUBLEFLOAT;
							data_info[data_ind].type_size = sizeof (F8);
							strcpy ((char *) data_info[data_ind].type_str, "F");
							data_info[data_ind].constant = 0;
							ok = 1;
						}

						if (strcmp ((const char *) ast[level].expr[j][1], "string") == 0)
						{
							data_info[data_ind].type = STRING;
							data_info[data_ind].type_size = sizeof (U1);
							strcpy ((char *) data_info[data_ind].type_str, "B");
							data_info[data_ind].constant = 0;
							ok = 1;
						}

						// const, constant variable definition ==================================
						if (strcmp ((const char *) ast[level].expr[j][1], "const-byte") == 0)
						{
							data_info[data_ind].type = BYTE;
							data_info[data_ind].type_size = sizeof (U1);
							strcpy ((char *) data_info[data_ind].type_str, "B");
							data_info[data_ind].constant = 1;
							ok = 1;
						}

						if (strcmp ((const char *) ast[level].expr[j][1], "const-int16") == 0)
						{
							data_info[data_ind].type = WORD;
							data_info[data_ind].type_size = sizeof (S2);
							strcpy ((char *) data_info[data_ind].type_str, "W");
							data_info[data_ind].constant = 1;
							ok = 1;
						}

						if (strcmp ((const char *) ast[level].expr[j][1], "const-int32") == 0)
						{
							data_info[data_ind].type = DOUBLEWORD;
							data_info[data_ind].type_size = sizeof (S4);
							strcpy ((char *) data_info[data_ind].type_str, "D");
							data_info[data_ind].constant = 1;
							ok = 1;
						}

						if (strcmp ((const char *) ast[level].expr[j][1], "const-int64") == 0)
						{
							data_info[data_ind].type = QUADWORD;
							data_info[data_ind].type_size = sizeof (S8);
							strcpy ((char *) data_info[data_ind].type_str, "Q");
							data_info[data_ind].constant = 1;
							ok = 1;
						}

						if (strcmp ((const char *) ast[level].expr[j][1], "const-double") == 0)
						{
							data_info[data_ind].type = DOUBLEFLOAT;
							data_info[data_ind].type_size = sizeof (F8);
							strcpy ((char *) data_info[data_ind].type_str, "F");
							data_info[data_ind].constant = 1;
							ok = 1;
						}

						if (strcmp ((const char *) ast[level].expr[j][1], "const-string") == 0)
						{
							data_info[data_ind].type = STRING;
							data_info[data_ind].type_size = sizeof (U1);
							strcpy ((char *) data_info[data_ind].type_str, "B");
							data_info[data_ind].constant = 1;
							ok = 1;
						}

						if (ok == 0)
						{
							printf ("error: line %lli: unknown data type!\n", linenum);
							return (1);
						}

						// name
						strcpy ((char *) data_info[data_ind].name, (const char *) ast[level].expr[j][3]);

						// size
						if (checkdigit (ast[level].expr[j][2]) != 1)
						{
							if (data_info[data_ind].type != STRING)
							{
								printf ("error: line %lli: data size not a number!\n", linenum);
								return (1);
							}
						}

						data_info[data_ind].size = get_temp_int ();

						// Set real string size, if 0 length was set or "s" was set.
						if ((data_info[data_ind].size == 0 || ast[level].expr[j][2][0] == 's') && data_info[data_ind].type == STRING)
						{
							// set string size automatically:
							data_info[data_ind].size = strlen_safe ((char *) ast[level].expr[j][4], MAXLINELEN) - 1;
							// set size string:
							sprintf ((char *) ast[level].expr[j][2], "%lli", data_info[data_ind].size);
						}

						// value
						if (ast[level].expr_args[j] >= 4)
						{
							if ((data_info[data_ind].type >= BYTE && data_info[data_ind].type <= QUADWORD) || data_info[data_ind].type == DOUBLEFLOAT)
							{
								if (checkdigit (ast[level].expr[j][4]) != 1)
								{
									if (get_variable_value (data_ind, ast[level].expr[j][4]) != 0)
									{
										strcpy ((char *) data_info[data_ind].value_str, (const char *) get_variable_value (data_ind, ast[level].expr[j][4]));
									}
									else
									{
										if (ast[level].expr[j][4][0] == '$')
										{
											// path for data filename
											strcpy ((char *) data_info[data_ind].value_str, (const char *) ast[level].expr[j][4]);
										}
										else
										{
											printf ("error: line %lli: value not a number and not a variable!\n", linenum);
											return (1);
										}
									}
								}
								else
								{
									strcpy ((char *) data_info[data_ind].value_str, (const char *) ast[level].expr[j][4]);
								}
							}

							if (data_info[data_ind].type == STRING)
							{
								strcpy ((char *) data_info[data_ind].value_str, (const char *) ast[level].expr[j][4]);
							}
						}

						if (data_line == 0)
						{
							// first data, set ".data" and start offset

							strcpy ((char *) data[data_line], ".data\n");
							data_line++;
							if (data_line >= line_len)
							{
								printf ("error: line %lli: data list full!\n", linenum);
								return (1);
							}

							data_info[data_ind].offset = 0;
							data_info[data_ind].end = data_info[data_ind].offset + (data_info[data_ind].size * data_info[data_ind].type_size);

							strcpy ((char *) data[data_line], (const char *) data_info[data_ind].type_str);
							strcat ((char *) data[data_line], ", ");
							strcat ((char *) data[data_line], (const char *) ast[level].expr[j][2]);
							strcat ((char *) data[data_line], ", ");
							strcat ((char *) data[data_line], (const char *) data_info[data_ind].name);
							strcat ((char *) data[data_line], "\n");

							data_line++;
							if (data_line >= line_len)
							{
								printf ("error: line %lli: data list full!\n", linenum);
								return (1);
							}

							strcpy ((char *) data[data_line], "@, ");
							sprintf ((char *) str, "%lli", data_info[data_ind].offset);
							strcat ((char *) data[data_line], (const char *) str);
							strcat ((char *) data[data_line], "Q, ");
							strcat ((char *) data[data_line], (const char *) data_info[data_ind].value_str);

							if (ast[level].expr_args[j] > 4)
							{
								for (i = 5; i <= ast[level].expr_args[j]; i++)
								{
									if ((data_info[data_ind].type >= BYTE && data_info[data_ind].type <= QUADWORD) || data_info[data_ind].type == DOUBLEFLOAT)
									{
										if (ast[level].expr[j][i][0] == '/')
										{
											// found end of line mark of multi line array data
											array_multi = 1;
											continue;
										}
										if (checkdigit (ast[level].expr[j][i]) != 1)
										{
											if (get_variable_value (data_ind, ast[level].expr[j][i]) != 0)
											{
												strcpy ((char *) data_info[data_ind].value_str, (const char *) get_variable_value (data_ind, ast[level].expr[j][i]));
											}
											else
											{
												printf ("error: line %lli: value not a number and not a variable!\n", linenum);
												return (1);
											}
										}
										else
										{
											strcpy ((char *) data_info[data_ind].value_str, (const char *) ast[level].expr[j][i]);
										}
									}
									strcat ((char *) data[data_line], ", ");
									strcat ((char *) data[data_line], (const char *) data_info[data_ind].value_str);
								}
								if (array_multi == 0)
								{
									strcat ((char *) data[data_line], ", ;");
								}
							}

							strcat ((char *) data[data_line], "\n");
						}
						else
						{
							// use offset from last data end
							data_info[data_ind].offset = data_info[data_ind - 1].end;
							data_info[data_ind].end = data_info[data_ind].offset + (data_info[data_ind].size * data_info[data_ind].type_size);

							data_line++;
							if (data_line >= line_len)
							{
								printf ("error: line %lli: data list full!\n", linenum);
								return (1);
							}

							strcpy ((char *) data[data_line], (const char *) data_info[data_ind].type_str);
							strcat ((char *) data[data_line], ", ");
							strcat ((char *) data[data_line], (const char *) ast[level].expr[j][2]);
							strcat ((char *) data[data_line], ", ");
							strcat ((char *) data[data_line], (const char *) data_info[data_ind].name);
							strcat ((char *) data[data_line], "\n");

							data_line++;
							if (data_line >= line_len)
							{
								printf ("error: line %lli: data list full!\n", linenum);
								return (1);
							}

							strcpy ((char *) data[data_line], "@, ");
							sprintf ((char *) str, "%lli", data_info[data_ind].offset);
							strcat ((char *) data[data_line], (const char *) str);
							strcat ((char *) data[data_line], "Q, ");
							strcat ((char *) data[data_line], (const char *) data_info[data_ind].value_str);

							if (ast[level].expr_args[j] > 4 && data_info[data_ind].type != STRING)
							{
								for (i = 5; i <= ast[level].expr_args[j]; i++)
								{
									if ((data_info[data_ind].type >= BYTE && data_info[data_ind].type <= QUADWORD) || data_info[data_ind].type == DOUBLEFLOAT)
									{
										if (ast[level].expr[j][i][0] == '/')
										{
											// found end of line mark of multi line array data
											array_multi = 1;
											continue;
										}
										if (checkdigit (ast[level].expr[j][i]) != 1)
										{
											if (get_variable_value (data_ind, ast[level].expr[j][i]) != 0)
											{
												strcpy ((char *) data_info[data_ind].value_str, (const char *) get_variable_value (data_ind, ast[level].expr[j][i]));
											}
											else
											{
												printf ("error: line %lli: value not a number and not a variable!\n", linenum);
												return (1);
											}
										}
										else
										{
											strcpy ((char *) data_info[data_ind].value_str, (const char *) ast[level].expr[j][i]);
										}
									}
									strcat ((char *) data[data_line], ", ");
									strcat ((char *) data[data_line], (const char *) data_info[data_ind].value_str);
								}
								if (array_multi == 0)
								{
									strcat ((char *) data[data_line], ", ;");
								}
							}

							strcat ((char *) data[data_line], "\n");

							if (data_info[data_ind].type == STRING)
							{
								data_line++;
								if (data_line >= line_len)
								{
									printf ("error: line %lli: data list full!\n", linenum);
									return (1);
								}

								strcpy ((char *) data[data_line], "Q, 1, ");
								strcat ((char *) data[data_line], (const char *) data_info[data_ind].name);
								strcat ((char *) data[data_line], "addr\n");

								data_line++;
								if (data_line >= line_len)
								{
									printf ("error: line %lli: data list full!\n", linenum);
									return (1);
								}

								data_ind++;
								if (data_ind == MAXDATAINFO)
								{
									printf ("error: line %lli: data list full!\n", linenum);
									return (1);
								}

								data_info[data_ind].type = QUADWORD;
								data_info[data_ind].type_size = sizeof (S8);
								strcpy ((char *) data_info[data_ind].type_str, "Q");
								strcpy ((char *) data_info[data_ind].name, (const char *) data_info[data_ind - 1].name);
								strcat ((char *) data_info[data_ind].name, "addr");

								// use offset from last data end
								data_info[data_ind].offset = data_info[data_ind - 1].end;
								data_info[data_ind].end = data_info[data_ind].offset + sizeof (S8);

								sprintf ((char *) str, "%lli", data_info[data_ind].offset);
								strcpy ((char *) data_info[data_ind].value_str, (const char *) str);

								data_line++;
								if (data_line >= line_len)
								{
									printf ("error: line %lli: data list full!\n", linenum);
									return (1);
								}

								strcpy ((char *) data[data_line], "@, ");
								strcat ((char *) data[data_line], (const char *) str);
								strcat ((char *) data[data_line], "Q, ");

								sprintf ((char *) str, "%lli", data_info[data_ind - 1].offset);
								strcat ((char *) data[data_line], (const char *) str);
								strcat ((char *) data[data_line], "Q\n");
							}
						}
					}
					else
					{
						last_arg = ast[level].expr_args[j];
						if (last_arg >= 0)
						{
							// check if STPOP opcode, to set loadreg automatically
							ok = 0;
							if (call_set == 1)
							{
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "stpopb") == 0)
							    {
									ok = 1;
								}
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "stpopi") == 0)
							    {
									ok = 1;
								}
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "stpopd") == 0)
							    {
									ok = 1;
								}
							}
							if (call_set == 1 && ok == 0)
							{
								if (loadreg () == 1)
								{
									// error loadreg
									return (1);
								}
								call_set = 0;
								call_loadreg = 1;
							}

							// reset registers ============================
							if (strcmp ((const char *) ast[level].expr[j][last_arg], "reset-reg") == 0)
							{
								// reset registers in current code position

								init_registers ();
								continue;
							}
							// ============================================

							ok = 0;
							for (t = 0; t < MAXTRANSLATE; t++)
							{
								if (strcmp ((const char *) ast[level].expr[j][last_arg], (const char *) translate[t].op) == 0)
								{
									ok = 1;
									break;
								}
							}

							if (ok == 0)
							{
								// operator not found in list!

								// check if = operator

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "=") == 0)
								{
									// do variable assign

									found_let++;
									if (found_let == 1)
									{
										found_let_cont++;
									}

									if (found_let > 1)
									{
										// already one assign was found
										// check if this is some kind of:
										// (((x y +) a =) b =)
										// expression

										#if DEBUG
											printf ("DEBUG: j: %i, last_arg: %i\n", j, ast[level].expr_args[j - 1]);
											printf ("last arg: '%s'\n", ast[level + 1].expr[j][last_arg]);
										#endif

										if (strcmp ((const char *) ast[level + 1].expr[j][last_arg], "=") == 0)
										{
												found_let_cont++;
												#if DEBUG
													printf ("DEBUG: found let continue assign!\n");
												#endif
										}
									}

									if (found_let != found_let_cont)
									{
										printf ("error: line %lli: more than one variable assign expression found: '%s' !\n", linenum, ast[level].expr[j][last_arg - 1]);
										return (1);
									}

									// check if variable to array variable assign
									if (last_arg >= 5)
									{
									if (strcmp ((const char *) ast[level].expr[j][last_arg - 1], "]") == 0 && strcmp ((const char *) ast[level].expr[j][last_arg - 3], "[") == 0)
									{
										// assign to array variable

										if (checkdef (ast[level].expr[j][last_arg - 4]) != 0)
										{
											return (1);
										}

										if (get_variable_is_array (ast[level].expr[j][last_arg - 4]) <= 1)
										{
											printf ("error: line %lli: variable '%s' is not an array!\n", linenum, ast[level].expr[j][last_arg - 4]);
											return (1);
										}

										if (checkdef (ast[level].expr[j][last_arg - 5]) != 0)
										{
											return (1);
										}

										// check if variable is constant
										if (get_var_is_const (ast[level].expr[j][last_arg - 4]) == 1)
										{
											printf ("error: line %lli: variable '%s' is constant!\n", linenum, ast[level].expr[j][last_arg - 4]);
											return (1);
										}

										if (getvartype (ast[level].expr[j][last_arg - 5]) == DOUBLE)
										{
											// printf ("DEBUG: assign to array variable.\n");

											reg = get_regd (ast[level].expr[j][last_arg - 5]);
											if (reg == -1)
											{
												// variable is not in register, load it

												reg = get_free_regd ();
												set_regd (reg, ast[level].expr[j][last_arg - 5]);

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "loadd ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 5]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");
											}

											if (checkdef (ast[level].expr[j][last_arg - 2]) != 0)
											{
												return (1);
											}
											// get array index
											reg2 = get_regi (ast[level].expr[j][last_arg - 2]);
											if (reg2 == -1)
											{
												reg2 = get_free_regi ();
												set_regi (reg2, ast[level].expr[j][last_arg - 2]);

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "loada ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 2]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg2);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");
											}

											// assign to array variable

											strcpy ((char *) code_temp, "load ");
											strcat ((char *) code_temp, (const char *) ast[level].expr[j][last_arg - 4]);
											strcat ((char *) code_temp, ", 0, ");

											reg3 = get_free_regd ();

											sprintf ((char *) str, "%i", reg3);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, "\n");

											// printf ("%s\n", code_temp);

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}

											strcpy ((char *) code[code_line], (const char *) code_temp);

											// pulld

											strcpy ((char *) code_temp, "pulld ");

											sprintf ((char *) str, "%i", reg);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, ", ");
											sprintf ((char *) str, "%i", reg3);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, ", ");
											sprintf ((char *) str, "%i", reg2);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, "\n");

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}

											strcpy ((char *) code[code_line], (const char *) code_temp);
										}
										else
										{
											reg = get_regi (ast[level].expr[j][last_arg - 5]);
											if (reg == -1)
											{
												// variable is not in register, load it

												reg = get_free_regi ();
												set_regi (reg, ast[level].expr[j][last_arg - 5]);

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "loada ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 5]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");
											}

											if (checkdef (ast[level].expr[j][last_arg - 2]) != 0)
											{
												return (1);
											}
											// get array index
											reg2 = get_regi (ast[level].expr[j][last_arg - 2]);
											if (reg2 == -1)
											{
												reg2 = get_free_regi ();
												set_regi (reg2, ast[level].expr[j][last_arg - 2]);

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "loada ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 2]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg2);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");
											}

											// assign to array variable

											strcpy ((char *) code_temp, "load ");
											strcat ((char *) code_temp, (const char *) ast[level].expr[j][last_arg - 4]);
											strcat ((char *) code_temp, ", 0, ");

											reg3 = get_free_regi ();
											// set_regd (reg, (U1 *) ast[level].expr[j][last_arg - 1]);

											sprintf ((char *) str, "%i", reg3);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, "\n");

											// printf ("%s\n", code_temp);

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}

											strcpy ((char *) code[code_line], (const char *) code_temp);

											if (checkdef (ast[level].expr[j][last_arg - 4]) != 0)
											{
												return (1);
											}

											if (getvartype_real (ast[level].expr[j][last_arg - 4]) == BYTE)
											{
												strcpy ((char *) code_temp, "pullb ");
											}

											if (getvartype_real (ast[level].expr[j][last_arg - 4]) == WORD)
											{
												strcpy ((char *) code_temp, "pullw ");
											}

											if (getvartype_real (ast[level].expr[j][last_arg - 4]) == DOUBLEWORD)
											{
												strcpy ((char *) code_temp, "pulldw ");
											}

											if (getvartype_real (ast[level].expr[j][last_arg - 4]) == QUADWORD)
											{
												strcpy ((char *) code_temp, "pullqw ");
											}

											sprintf ((char *) str, "%i", reg);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, ", ");
											sprintf ((char *) str, "%i", reg3);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, ", ");
											sprintf ((char *) str, "%i", reg2);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, "\n");

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}

											strcpy ((char *) code[code_line], (const char *) code_temp);
										}
										continue;
									}
									} // lastarg if
									// check if array variable to variable assign
									if (last_arg >= 5)
									{
									if (strcmp ((const char *) ast[level].expr[j][last_arg - 2], "]") == 0 && strcmp ((const char *) ast[level].expr[j][last_arg - 4], "[") == 0)
									{
										// assign to array variable

										if (checkdef (ast[level].expr[j][last_arg - 5]) != 0)
										{
											return (1);
										}

										if (get_variable_is_array (ast[level].expr[j][last_arg - 5]) <= 1)
										{
											printf ("error: line %lli: variable '%s' is not an array!\n", linenum, ast[level].expr[j][last_arg - 5]);
											return (1);
										}

										if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
										{
											return (1);
										}

										// check if variable is constant
										if (get_var_is_const (ast[level].expr[j][last_arg - 1]) == 1)
										{
											printf ("error: line %lli: variable '%s' is constant!\n", linenum, ast[level].expr[j][last_arg - 1]);
											return (1);
										}

										if (getvartype (ast[level].expr[j][last_arg - 1]) == DOUBLE)
										{
											// printf ("DEBUG: assign array variable to variable.\n");

											reg = get_regd (ast[level].expr[j][last_arg - 1]);
											if (reg == -1)
											{
												// variable is not in register, load it

												reg = get_free_regd ();
												set_regd (reg, ast[level].expr[j][last_arg - 1]);

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "loadd ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 1]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");
											}

											if (checkdef (ast[level].expr[j][last_arg - 3]) != 0)
											{
												return (1);
											}
											// get array index
											reg2 = get_regi (ast[level].expr[j][last_arg - 3]);
											if (reg2 == -1)
											{
												reg2 = get_free_regi ();
												set_regi (reg2, ast[level].expr[j][last_arg - 3]);

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "loada ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 3]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg2);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");
											}

											// assign array variable to variable

											strcpy ((char *) code_temp, "load ");
											strcat ((char *) code_temp, (const char *) ast[level].expr[j][last_arg - 5]);
											strcat ((char *) code_temp, ", 0, ");

											reg3 = get_free_regd ();
											// set_regd (reg, (U1 *) ast[level].expr[j][last_arg - 1]);

											sprintf ((char *) str, "%i", reg3);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, "\n");

											// printf ("%s\n", code_temp);

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}

											strcpy ((char *) code[code_line], (const char *) code_temp);

											// pulld

											strcpy ((char *) code_temp, "pushd ");

											sprintf ((char *) str, "%i", reg3);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, ", ");
											sprintf ((char *) str, "%i", reg2);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, ", ");
											sprintf ((char *) str, "%i", reg);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, "\n");

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}

											strcpy ((char *) code[code_line], (const char *) code_temp);
										}
										else
										{
											reg = get_regi (ast[level].expr[j][last_arg - 1]);
											if (reg == -1)
											{
												// variable is not in register, load it

												reg = get_free_regi ();
												set_regi (reg, ast[level].expr[j][last_arg - 1]);

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "loada ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 1]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");
											}

											if (checkdef (ast[level].expr[j][last_arg - 3]) != 0)
											{
												return (1);
											}
											// get array index
											reg2 = get_regi (ast[level].expr[j][last_arg - 3]);
											if (reg2 == -1)
											{
												reg2 = get_free_regi ();
												set_regi (reg2, ast[level].expr[j][last_arg - 3]);

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "loada ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 3]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg2);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");
											}

											// assign array variable to variable

											strcpy ((char *) code_temp, "load ");
											strcat ((char *) code_temp, (const char *) ast[level].expr[j][last_arg - 5]);
											strcat ((char *) code_temp, ", 0, ");

											reg3 = get_free_regi ();
											// set_regd (reg, (U1 *) ast[level].expr[j][last_arg - 1]);

											sprintf ((char *) str, "%i", reg3);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, "\n");

											// printf ("%s\n", code_temp);

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}

											strcpy ((char *) code[code_line], (const char *) code_temp);

											if (checkdef (ast[level].expr[j][last_arg - 5]) != 0)
											{
												return (1);
											}

											if (getvartype_real (ast[level].expr[j][last_arg - 5]) == BYTE)
											{
												strcpy ((char *) code_temp, "pushb ");
											}

											if (getvartype_real (ast[level].expr[j][last_arg - 5]) == WORD)
											{
												strcpy ((char *) code_temp, "pushw ");
											}

											if (getvartype_real (ast[level].expr[j][last_arg - 5]) == DOUBLEWORD)
											{
												strcpy ((char *) code_temp, "pushdw ");
											}

											if (getvartype_real (ast[level].expr[j][last_arg - 5]) == QUADWORD)
											{
												strcpy ((char *) code_temp, "pushqw ");
											}

											sprintf ((char *) str, "%i", reg3);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, ", ");
											sprintf ((char *) str, "%i", reg2);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, ", ");
											sprintf ((char *) str, "%i", reg);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, "\n");

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}

											strcpy ((char *) code[code_line], (const char *) code_temp);
										}
										continue;
									}
									} // lastarg if

									// assign to normal variable ==============

									if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
									{
										return (1);
									}

									// check if variable is constant
									if (get_var_is_const (ast[level].expr[j][last_arg - 1]) == 1)
									{
										printf ("error: line %lli: variable '%s' is constant!\n", linenum, ast[level].expr[j][last_arg - 1]);
										return (1);
									}

									if (getvartype (ast[level].expr[j][last_arg - 1]) == DOUBLE)
									{
										strcpy ((char *) code_temp, "load ");
										strcat ((char *) code_temp, (const char *) ast[level].expr[j][last_arg - 1]);
										strcat ((char *) code_temp, ", 0, ");

										reg = get_free_regd ();
										// set_regd (reg, (U1 *) ast[level].expr[j][last_arg - 1]);

										sprintf ((char *) str, "%i", reg);
										strcat ((char *) code_temp, (const char *) str);
										strcat ((char *) code_temp, "\n");

										// printf ("%s\n", code_temp);

										code_line++;
										if (code_line >= line_len)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (1);
										}

										strcpy ((char *) code[code_line], (const char *) code_temp);
									}

									if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
									{
										return (1);
									}
									if (getvartype (ast[level].expr[j][last_arg - 1]) == INTEGER)
									{
										strcpy ((char *) code_temp, "load ");
										strcat ((char *) code_temp, (const char *) ast[level].expr[j][last_arg - 1]);
										strcat ((char *) code_temp, ", 0, ");

										reg = get_free_regi ();
										// set_regd (reg, (U1 *) ast[level].expr[j][last_arg - 1]);

										sprintf ((char *) str, "%i", reg);
										strcat ((char *) code_temp, (const char *) str);
										strcat ((char *) code_temp, "\n");

										// printf ("%s\n", code_temp);

										code_line++;
										if (code_line >= line_len)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (1);
										}

										strcpy ((char *) code[code_line], (const char *) code_temp);

										set_regi (reg, (U1 *) "temp");

										if (last_arg == 1)
										{
											// some kind of "ret =)" assign expression
											// use target of higher level

											if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
											{
												return (1);
											}

											if (getvartype_real (ast[level].expr[j][last_arg - 1]) == BYTE)
											{
												strcpy ((char *) code_temp, "pullb ");
											}

											if (getvartype_real (ast[level].expr[j][last_arg - 1]) == WORD)
											{
												strcpy ((char *) code_temp, "pullw ");
											}

											if (getvartype_real (ast[level].expr[j][last_arg - 1]) == DOUBLEWORD)
											{
												strcpy ((char *) code_temp, "pulldw ");
											}

											if (getvartype_real (ast[level].expr[j][last_arg - 1]) == QUADWORD)
											{
												strcpy ((char *) code_temp, "pullqw ");
											}

											sprintf ((char *) str, "%i", target);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, ", ");
											sprintf ((char *) str, "%i", reg);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, ", 0\n");

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}

											strcpy ((char *) code[code_line], (const char *) code_temp);

											reg2 = get_regi (ast[level].expr[j][last_arg - 1]);
											if (reg2 != -1)
											{
												// set old value of reg2 as empty
												set_regi (reg2, (U1 *) "");
											}

											continue;
										}
										else
										{
											if (checkdef (ast[level].expr[j][last_arg - 2]) != 0)
											{
												return (1);
											}
											if (getvartype (ast[level].expr[j][last_arg - 2]) == STRING)
											{
												//
												//	load string address into integer variable
												//

												target = get_regi (ast[level].expr[j][last_arg - 1]);
												if (target == -1)
												{
													// variable is not in register, load it

													reg = get_free_regi ();
													set_regi (reg, ast[level].expr[j][last_arg - 1]);

													// write code load

													code_line++;
													if (code_line >= line_len)
													{
														printf ("error: line %lli: code list full!\n", linenum);
														return (1);
													}

													target = reg;

													strcpy ((char *) code[code_line], "loada ");
													strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 1]);
													strcat ((char *) code[code_line], ", 0, ");
													sprintf ((char *) str, "%i", reg);
													strcat ((char *) code[code_line], (const char *) str);
													strcat ((char *) code[code_line], "\n");
												}

												reg2 = get_regi (ast[level].expr[j][last_arg - 2]);
												if (reg2 == -1)
												{
													// variable is not in register, load it

													reg2 = get_free_regi ();
													set_regi (reg2, ast[level].expr[j][last_arg - 2]);

													// write code load

													code_line++;
													if (code_line >= line_len)
													{
														printf ("error: line %lli: code list full!\n", linenum);
														return (1);
													}

													strcpy ((char *) code[code_line], "load ");
													strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 2]);
													strcat ((char *) code[code_line], ", 0, ");
													sprintf ((char *) str, "%i", reg2);
													strcat ((char *) code[code_line], (const char *) str);
													strcat ((char *) code[code_line], "\n");
												}

												if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
												{
													return (1);
												}

												if (getvartype_real (ast[level].expr[j][last_arg - 1]) == BYTE)
												{
													strcpy ((char *) code_temp, "pullb ");
												}

												if (getvartype_real (ast[level].expr[j][last_arg - 1]) == WORD)
												{
													strcpy ((char *) code_temp, "pullw ");
												}

												if (getvartype_real (ast[level].expr[j][last_arg - 1]) == DOUBLEWORD)
												{
													strcpy ((char *) code_temp, "pulldw ");
												}

												if (getvartype_real (ast[level].expr[j][last_arg - 1]) == QUADWORD)
												{
													strcpy ((char *) code_temp, "pullqw ");
												}

												sprintf ((char *) str, "%i", reg2);
												strcat ((char *) code_temp, (const char *) str);
												strcat ((char *) code_temp, ", ");
												sprintf ((char *) str, "%i", target);
												strcat ((char *) code_temp, (const char *) str);
												strcat ((char *) code_temp, ", 0\n");

												set_regi (target, (U1 *) "");
												continue;
											}
											else
											{
												// integer
												if (last_arg == 1)
												{
													if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
													{
														return (1);
													}

													if (getvartype_real (ast[level].expr[j][last_arg - 1]) == BYTE)
													{
														strcpy ((char *) code_temp, "pullb ");
													}

													if (getvartype_real (ast[level].expr[j][last_arg - 1]) == WORD)
													{
														strcpy ((char *) code_temp, "pullw ");
													}

													if (getvartype_real (ast[level].expr[j][last_arg - 1]) == DOUBLEWORD)
													{
														strcpy ((char *) code_temp, "pulldw ");
													}

													if (getvartype_real (ast[level].expr[j][last_arg - 1]) == QUADWORD)
													{
														strcpy ((char *) code_temp, "pullqw ");
													}

													sprintf ((char *) str, "%i", target);
													strcat ((char *) code_temp, (const char *) str);
													strcat ((char *) code_temp, ", ");
													sprintf ((char *) str, "%i", reg);
													strcat ((char *) code_temp, (const char *) str);
													strcat ((char *) code_temp, ", 0\n");
												}
												else
												{
													// printf ("variable assign direct...\n");

													if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
													{
														return (1);
													}
													if (getvartype_real (ast[level].expr[j][last_arg - 1]) != DOUBLE)
													{
														target = get_regi (ast[level].expr[j][last_arg - 1]);
														if (target == -1)
														{
															// variable is not in register, load it

															reg2 = get_free_regi ();
															set_regi (reg2, ast[level].expr[j][last_arg - 1]);
															// write code loada

															code_line++;
															if (code_line >= line_len)
															{
																printf ("error: line %lli: code list full!\n", linenum);
																return (1);
															}

															strcpy ((char *) code[code_line], "loada ");
															strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 2]);
															strcat ((char *) code[code_line], ", 0, ");
															sprintf ((char *) str, "%i", reg2);
															strcat ((char *) code[code_line], (const char *) str);
															strcat ((char *) code[code_line], "\n");
															target = reg2;
														}
													}
													else
													{
														target = get_regd (ast[level].expr[j][last_arg - 1]);
														if (target == -1)
														{
															// variable is not in register, load it

															reg2 = get_free_regd ();
															set_regd (reg2, ast[level].expr[j][last_arg - 1]);
															// write code loada

															code_line++;
															if (code_line >= line_len)
															{
																printf ("error: line %lli: code list full!\n", linenum);
																return (1);
															}

															strcpy ((char *) code[code_line], "loadd ");
															strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 2]);
															strcat ((char *) code[code_line], ", 0, ");
															sprintf ((char *) str, "%i", reg2);
															strcat ((char *) code[code_line], (const char *) str);
															strcat ((char *) code[code_line], "\n");
															target = reg2;
														}
													}

													if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
													{
														return (1);
													}
													if (getvartype_real (ast[level].expr[j][last_arg - 1]) == BYTE)
													{
														strcpy ((char *) code_temp, "pullb ");
													}

													if (getvartype_real (ast[level].expr[j][last_arg - 1]) == WORD)
													{
														strcpy ((char *) code_temp, "pullw ");
													}

													if (getvartype_real (ast[level].expr[j][last_arg - 1]) == DOUBLEWORD)
													{
														strcpy ((char *) code_temp, "pulldw ");
													}

													if (getvartype_real (ast[level].expr[j][last_arg - 1]) == QUADWORD)
													{
														strcpy ((char *) code_temp, "pullqw ");
													}

													sprintf ((char *) str, "%i", target);
													strcat ((char *) code_temp, (const char *) str);
													strcat ((char *) code_temp, ", ");
													sprintf ((char *) str, "%i", reg);
													strcat ((char *) code_temp, (const char *) str);
													strcat ((char *) code_temp, ", 0\n");
												}
											}

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}

											strcpy ((char *) code[code_line], (const char *) code_temp);

											reg2 = get_regi (ast[level].expr[j][last_arg - 1]);
											if (reg2 != -1)
											{
												// set old value of reg2 as empty
												set_regi (reg2, (U1 *) "");
											}

											set_regi (reg, (U1 *) "");
											continue;
										}
									}

									if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
									{
										return (1);
									}

									// check if variable is constant
									if (get_var_is_const (ast[level].expr[j][last_arg - 1]) == 1)
									{
										printf ("error: line %lli: variable '%s' is constant!\n", linenum, ast[level].expr[j][last_arg - 1]);
										return (1);
									}

									if (getvartype (ast[level].expr[j][last_arg - 1]) == DOUBLE)
									{
										if (last_arg == 1)
										{
											strcpy ((char *) code_temp, "pulld ");
											sprintf ((char *) str, "%i", target);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, ", ");
											sprintf ((char *) str, "%i", reg);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, ", 0\n");
										}
										else
										{
											target = get_regd (ast[level].expr[j][last_arg - 2]);
											if (target == -1)
											{
												// variable is not in register, load it

												reg = get_free_regd ();
												set_regd (reg, ast[level].expr[j][last_arg - 2]);

												// write code loada

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "loadd ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 2]);

												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");

												target = reg;
											}

											strcpy ((char *) code_temp, "pulld ");
											sprintf ((char *) str, "%i", target);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, ", ");
											sprintf ((char *) str, "%i", reg);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, ", 0\n");
										}

										code_line++;
										if (code_line >= line_len)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (1);
										}

										strcpy ((char *) code[code_line], (const char *) code_temp);

										reg2 = get_regd (ast[level].expr[j][last_arg - 1]);
										if (reg2 != -1)
										{
											// set old value of reg2 as empty
											set_regd (reg2, (U1 *) "");
										}

										set_regd (reg, (U1 *) "");
										continue;
									}

									ok = 1;
								}
								else
								{
									// set not equal sign operator: "=" continous:

									if (ast[level].expr_args[j] >= 0)
									{
										// not < 0
										found_let_cont = 0;
									}
								}

								if (ast[level].expr[j][last_arg][0] == ':')
								{
									// operator is label name

									strcpy ((char *) code_temp, (const char *) ast[level].expr[j][last_arg]);

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcpy ((char *) code[code_line], (const char *) code_temp);
									strcat ((char *) code[code_line], "\n");

									// add name to labels list
									if (label_ind < MAXLABELS - 1)
									{
										label_ind++;
									}
									else
									{
										printf ("error: line %lli: label list full!\n", linenum);
										return (1);
									}

									label[label_ind].pos = 1;
									strcpy ((char *) label[label_ind].name, (const char *) code_temp);

									continue;
								}
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "func") == 0)
								{
									if (last_arg < 1)
									{
										printf ("error: line %lli: no function name set!\n", linenum);
										return (1);
									}

									// start of a function

									if (strcmp ((const char *) ast[level].expr[j][last_arg - 1], "main") != 0)
									{
										// not main function, initialize the registers

										init_registers ();
									}

									strcpy ((char *) code_temp, ":");
									strcat ((char *) code_temp, (const char *) ast[level].expr[j][last_arg - 1]);

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcpy ((char *) code[code_line], (const char *) code_temp);
									strcat ((char *) code[code_line], "\n");

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcpy ((char *) code[code_line], "loada zero, 0, 0\n");

									set_regi (0, (U1 *) "zero");
									strcpy ((char *) code_temp, "");

									// add name to labels list
									if (label_ind < MAXLABELS - 1)
									{
										label_ind++;
									}
									else
									{
										printf ("error: line %lli: label list full!\n", linenum);
										return (1);
									}

									label[label_ind].pos = 1;
									strcpy ((char *) label[label_ind].name, ":");
									strcat ((char *) label[label_ind].name, (const char *) ast[level].expr[j][last_arg - 1]);

									continue;
								}
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "funcend") == 0)
								{
									// end of a function

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcpy ((char *) code[code_line], (const char *) "rts");
									strcat ((char *) code[code_line], "\n");
									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "intr0") == 0 || strcmp ((const char *) ast[level].expr[j][last_arg], "intr1") == 0 || strcmp ((const char *) ast[level].expr[j][last_arg], "pushb") == 0 || strcmp ((const char *) ast[level].expr[j][last_arg], "pullb") == 0)
								{
									strcpy ((char *) code_temp, (const char *) ast[level].expr[j][last_arg]);
									strcat ((char *) code_temp, " ");

									for (e = 0; e <= last_arg - 1; e++)
									{
										if (checkdef (ast[level].expr[j][e]) != 0)
										{
											return (1);
										}
										if (getvartype (ast[level].expr[j][e]) == INTEGER)
										{
											reg = get_regi (ast[level].expr[j][e]);
											if (reg == -1)
											{
												// variable is not in register, load it
												// create new scope
												{
													S2 reg_p, vartype;

													reg = get_free_regi ();
													set_regi (reg, ast[level].expr[j][e]);

													vartype = getvartype_real (ast[level].expr[j][e]);
													code_line++;
													if (code_line >= line_len)
													{
														printf ("error: line %lli: code list full!\n", linenum);
														return (1);
													}

													switch (vartype)
													{
														case BYTE:
															// set load opcode
															strcpy ((char *) code[code_line], "load ");
															strcat ((char *) code[code_line], (const char *) ast[level].expr[j][e]);
															strcat ((char *) code[code_line], ", 0, ");
															sprintf ((char *) str, "%i", reg);
															strcat ((char *) code[code_line], (const char *) str);
															strcat ((char *) code[code_line], "\n");

															code_line++;
															if (code_line >= line_len)
															{
																printf ("error: line %lli: code list full!\n", linenum);
																return (1);
															}

															// set pushb opcode
															strcpy ((char *) code[code_line], "pushb ");
															sprintf ((char *) str, "%i", reg);
															strcat ((char *) code[code_line], (const char *) str);
															strcat ((char *) code[code_line], ", 0, ");

															// get free register
															reg_p = get_free_regi ();
															set_regi (reg_p, ast[level].expr[j][e]);

															sprintf ((char *) str, "%i", reg_p);
															strcat ((char *) code[code_line], (const char *) str);
															strcat ((char *) code[code_line], "\n");

															strcat ((char *) code_temp, (const char *) str);
															if (e < last_arg - 1)
															{
																strcat ((char *) code_temp, ", ");
															}
															break;

														case WORD:
															// set load opcode
															strcpy ((char *) code[code_line], "load ");
															strcat ((char *) code[code_line], (const char *) ast[level].expr[j][e]);
															strcat ((char *) code[code_line], ", 0, ");
															sprintf ((char *) str, "%i", reg);
															strcat ((char *) code[code_line], (const char *) str);
															strcat ((char *) code[code_line], "\n");

															code_line++;
															if (code_line >= line_len)
															{
																printf ("error: line %lli: code list full!\n", linenum);
																return (1);
															}

															// set pushw opcode
															strcpy ((char *) code[code_line], "pushw ");
															sprintf ((char *) str, "%i", reg);
															strcat ((char *) code[code_line], (const char *) str);
															strcat ((char *) code[code_line], ", 0, ");

															// get free register
															reg_p = get_free_regi ();
															set_regi (reg_p, ast[level].expr[j][e]);

															sprintf ((char *) str, "%i", reg_p);
															strcat ((char *) code[code_line], (const char *) str);
															strcat ((char *) code[code_line], "\n");

															strcat ((char *) code_temp, (const char *) str);
															if (e < last_arg - 1)
															{
																strcat ((char *) code_temp, ", ");
															}
															break;

														case DOUBLEWORD:
															// set load opcode
															strcpy ((char *) code[code_line], "load ");
															strcat ((char *) code[code_line], (const char *) ast[level].expr[j][e]);
															strcat ((char *) code[code_line], ", 0, ");
															sprintf ((char *) str, "%i", reg);
															strcat ((char *) code[code_line], (const char *) str);
															strcat ((char *) code[code_line], "\n");

															code_line++;
															if (code_line >= line_len)
															{
																printf ("error: line %lli: code list full!\n", linenum);
																return (1);
															}

															// set pushw opcode
															strcpy ((char *) code[code_line], "pushdw ");
															sprintf ((char *) str, "%i", reg);
															strcat ((char *) code[code_line], (const char *) str);
															strcat ((char *) code[code_line], ", 0, ");

															// get free register
															reg_p = get_free_regi ();
															set_regi (reg_p, ast[level].expr[j][e]);

															sprintf ((char *) str, "%i", reg_p);
															strcat ((char *) code[code_line], (const char *) str);
															strcat ((char *) code[code_line], "\n");

															strcat ((char *) code_temp, (const char *) str);
															if (e < last_arg - 1)
															{
																strcat ((char *) code_temp, ", ");
															}
															break;

														case QUADWORD:
															// write code loada
															code_line++;
															if (code_line >= line_len)
															{
																printf ("error: line %lli: code list full!\n", linenum);
																return (1);
															}

															strcpy ((char *) code[code_line], "loada ");
															strcat ((char *) code[code_line], (const char *) ast[level].expr[j][e]);
															strcat ((char *) code[code_line], ", 0, ");
															sprintf ((char *) str, "%i", reg);
															strcat ((char *) code[code_line], (const char *) str);
															strcat ((char *) code[code_line], "\n");

															strcat ((char *) code_temp, (const char *) str);
															if (e < last_arg - 1)
															{
																strcat ((char *) code_temp, ", ");
															}
															break;

														default:
															// write code loada
															code_line++;
															if (code_line >= line_len)
															{
																printf ("error: line %lli: code list full!\n", linenum);
																return (1);
															}

															strcpy ((char *) code[code_line], "loada ");
															strcat ((char *) code[code_line], (const char *) ast[level].expr[j][e]);
															strcat ((char *) code[code_line], ", 0, ");
															sprintf ((char *) str, "%i", reg);
															strcat ((char *) code[code_line], (const char *) str);
															strcat ((char *) code[code_line], "\n");

															strcat ((char *) code_temp, (const char *) str);
															if (e < last_arg - 1)
															{
																strcat ((char *) code_temp, ", ");
															}
															break;
													}
												}
											}
											else
											{
												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code_temp, (const char *) str);
												if (e < last_arg - 1)
												{
													strcat ((char *) code_temp, ", ");
												}
											}
										}

										if (checkdef (ast[level].expr[j][e]) != 0)
										{
											return (1);
										}
										if (getvartype (ast[level].expr[j][e]) == DOUBLE)
										{
											// double float type

											reg = get_regd (ast[level].expr[j][e]);
											if (reg == -1)
											{
												// variable is not in register, load it

												reg = get_free_regd ();
												set_regd (reg, ast[level].expr[j][e]);

												// write code loadd

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "loadd ");
												strcat ((char *)code[code_line], (const char *) ast[level].expr[j][e]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");

												strcat ((char *) code_temp, (const char *) str);
												if (e < last_arg - 1)
												{
													strcat ((char *) code_temp, ", ");
												}
											}
											else
											{
												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code_temp, (const char *) str);
												if (e < last_arg - 1)
												{
													strcat ((char *) code_temp, ", ");
												}
											}
										}

										if (checkdef (ast[level].expr[j][e]) != 0)
										{
											return (1);
										}
										if (getvartype (ast[level].expr[j][e]) == STRING)
										{
											strcpy ((char *) str, (const char *) ast[level].expr[j][e]);
											strcat ((char *) str, "addr");

											reg = get_regi (str);
											if (reg == -1)
											{
												// variable is not in register, load it

												reg = get_free_regi ();
												set_regi (reg, ast[level].expr[j][e]);

												// write code loada

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "loada ");
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");

												strcat ((char *) code_temp, (const char *) str);
												if (e < last_arg - 1)
												{
													strcat ((char *) code_temp, ", ");
												}
											}
											else
											{
                                                sprintf ((char *) str, "%i", reg);
                                                strcat ((char *) code_temp, (const char *) str);
                                                if (e < last_arg - 1)
                                                {
                                                    strcat ((char *) code_temp, ", ");
                                                }
											}
										}

										if (getvartype (ast[level].expr[j][e]) == -1)
										{
											// expression is a constant, just copy it!!
											strcat ((char *) code_temp, (const char *) ast[level].expr[j][e]);

											if (e < last_arg - 1)
											{
												strcat ((char *) code_temp, ", ");
											}
										}
									}
									strcat ((char *) code_temp, "\n");

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcpy ((char *) code[code_line], (const char *) code_temp);
									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "savereg") == 0)
								{
									// save integer registers
									for (e = 0; e < MAXREG; e++)
									{
										if (strcmp ((const char *) regi[e], "") != 0)
										{
											strcpy ((char *) code_temp, "stpushi ");
											sprintf ((char *) str, "%i", e);
											strcat ((char *) code_temp, (const char*) str);
											strcat ((char *) code_temp, "\n");

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}

											strcpy ((char *) code[code_line], (const char *) code_temp);
											save_regi[e] = 1;
										}
										else
										{
											save_regi[e] = 0;
										}
									}

									// save double registers
									for (e = 0; e < MAXREG; e++)
									{
										if (strcmp ((const char *) regd[e], "") != 0)
										{
											strcpy ((char *) code_temp, "stpushd ");
											sprintf ((char *) str, "%i", e);
											strcat ((char *) code_temp, (const char*) str);
											strcat ((char *) code_temp, "\n");

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}

											strcpy ((char *) code[code_line], (const char *) code_temp);
											save_regd[e] = 1;
										}
										else
										{
											save_regd[e] = 0;
										}
									}
									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "loadreg") == 0)
								{
									if (call_set == 1 && call_loadreg == 0)
									{
										if (loadreg () == 1)
										{
											return (1);
										}
										call_set = 0;
									}
									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "if") == 0 && last_arg > 0)
								{
									if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
									{
										return (1);
									}
									if (getvartype (ast[level].expr[j][last_arg - 1]) == INTEGER)
									{
										if (getvartype_real (ast[level].expr[j][last_arg - 1]) == QUADWORD)
										{
											if (optimize_if == 0)
											{
												reg = get_regi (ast[level].expr[j][last_arg - 1]);
												if (reg == -1)
												{
													// variable is not in register, load it

													reg = get_free_regi ();
													set_regi (reg, ast[level].expr[j][last_arg - 1]);

													// write code loada

													code_line++;
													if (code_line >= line_len)
													{
														printf ("error: line %lli: code list full!\n", linenum);
														return (1);
													}

													strcpy ((char *) code[code_line], "loada ");
													strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 1]);
													strcat ((char *) code[code_line], ", 0, ");
													sprintf ((char *) str, "%i", reg);
													strcat ((char *) code[code_line], (const char *) str);
													strcat ((char *) code[code_line], "\n");
												}

												if_pos = get_if_pos ();
							                	if (if_pos == -1)
								                {
							                    	printf ("compile: error: if: out of memory if-list\n");
							                    	return (FALSE);
							                	}

												get_if_label (if_pos, if_label);
												get_endif_label (if_pos, endif_label);

												// write code jmpi to if code label

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "jmpi ");
												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], ", ");
												strcat ((char *) code[code_line], (const char *) if_label);
												strcat ((char *) code[code_line], "\n");

												// write code jmpi to endif label

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "jmp ");
												strcat ((char *) code[code_line], (const char *) endif_label);
												strcat ((char *) code[code_line], "\n");

												// write code label if

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}
												strcat ((char *) code[code_line], (const char *) if_label);
												strcat ((char *) code[code_line], "\n");
											}
											else
											{
												// comment out not needed opcodes set earlier
												strcpy ((char *) code[code_line - 1], "");
												strcpy ((char *) code[code_line], "");

												reg = get_if_optimize_reg (code[code_line - 2]);
												if (reg < 0)
												{
													printf ("error: line %lli: optimize if internal error!\n", linenum);
													return (1);
												}

												if_pos = get_if_pos ();
							                	if (if_pos == -1)
								                {
							                    	printf ("compile: error: if: out of memory if-list\n");
							                    	return (FALSE);
							                	}

												get_if_label (if_pos, if_label);
												get_endif_label (if_pos, endif_label);

												// write code jmpi to if code label

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "jmpi ");
												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], ", ");
												strcat ((char *) code[code_line], (const char *) if_label);
												strcat ((char *) code[code_line], "\n");

												// write code jmpi to endif label

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "jmp ");
												strcat ((char *) code[code_line], (const char *) endif_label);
												strcat ((char *) code[code_line], "\n");

												// write code label if

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}
												strcat ((char *) code[code_line], (const char *) if_label);
												strcat ((char *) code[code_line], "\n");
											}
										}
										else
										{
											printf ("error: line %lli: if variable must be int64 type!\n", linenum);
											return (1);
										}
									}
									else
									{
										printf ("error: line %lli: if variable must be int64 type!\n", linenum);
										return (1);
									}
									continue;
								}


								// if+ -> if with else, endif
								//
								//

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "if+") == 0 && last_arg > 0)
								{
									if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
									{
										return (1);
									}
									if (getvartype (ast[level].expr[j][last_arg - 1]) == INTEGER)
									{
										if (getvartype_real (ast[level].expr[j][last_arg - 1]) == QUADWORD)
										{
											if (optimize_if == 0)
											{
												reg = get_regi (ast[level].expr[j][last_arg - 1]);
												if (reg == -1)
												{
													// variable is not in register, load it

													reg = get_free_regi ();
													set_regi (reg, ast[level].expr[j][last_arg - 1]);

													// write code loada

													code_line++;
													if (code_line >= line_len)
													{
														printf ("error: line %lli: code list full!\n", linenum);
														return (1);
													}

													strcpy ((char *) code[code_line], "loada ");
													strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 1]);
													strcat ((char *) code[code_line], ", 0, ");
													sprintf ((char *) str, "%i", reg);
													strcat ((char *) code[code_line], (const char *) str);
													strcat ((char *) code[code_line], "\n");
												}

												if_pos = get_ifplus_pos ();
							                	if (if_pos == -1)
							                	{
							                    	printf ("compile: error: if: out of memory if-list\n");
							                    	return (FALSE);
							                	}

												get_if_label (if_pos, if_label);
												get_endif_label (if_pos, endif_label);

												// write code jmpi to if code label

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "jmpi ");
												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], ", ");
												strcat ((char *) code[code_line], (const char *) if_label);
												strcat ((char *) code[code_line], "\n");

												// write code jmpi to endif label

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												get_else_label (if_pos, else_label);

												strcpy ((char *) code[code_line], "jmp ");
												strcat ((char *) code[code_line], (const char *) else_label);
												strcat ((char *) code[code_line], "\n");

												// write code label if

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}
												strcat ((char *) code[code_line], (const char *) if_label);
												strcat ((char *) code[code_line], "\n");
											}
											else
											{
												// comment out not needed opcodes set earlier
												strcpy ((char *) code[code_line - 1], "");
												strcpy ((char *) code[code_line], "");

												reg = get_if_optimize_reg (code[code_line - 2]);
												if (reg < 0)
												{
													printf ("error: line %lli: optimize if internal error!\n", linenum);
													return (1);
												}

												if_pos = get_if_pos ();
							                	if (if_pos == -1)
							                	{
							                    	printf ("compile: error: if: out of memory if-list\n");
							                    	return (FALSE);
							                	}

												get_if_label (if_pos, if_label);
												get_endif_label (if_pos, endif_label);

												// write code jmpi to if code label

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "jmpi ");
												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], ", ");
												strcat ((char *) code[code_line], (const char *) if_label);
												strcat ((char *) code[code_line], "\n");

												// write code jmpi to endif label

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												get_else_label (if_pos, else_label);

												strcpy ((char *) code[code_line], "jmp ");
												strcat ((char *) code[code_line], (const char *) else_label);
												strcat ((char *) code[code_line], "\n");

												// write code label if

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}
												strcat ((char *) code[code_line], (const char *) if_label);
												strcat ((char *) code[code_line], "\n");
											}
										}
										else
										{
											printf ("error: line %lli: if variable must be int64 type!\n", linenum);
											return (1);
										}
									}
									else
									{
										printf ("error: line %lli: if variable must be int64 type!\n", linenum);
										return (1);
									}
									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "else") == 0)
								{
									if_pos = get_act_ifplus ();
									if (if_pos == -1)
									{
										printf ("error: line %lli: else: if+ not set!\n", linenum);
										return (1);
									}

									set_else (if_pos);

									get_endif_label (if_pos, endif_label);
									get_else_label (if_pos, else_label);

									// write code jmp to if code label

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcpy ((char *) code[code_line], "jmp ");
									strcat ((char *) code[code_line], (const char *) endif_label);
									strcat ((char *) code[code_line], "\n");

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcat ((char *) code[code_line], (const char *) else_label);
									strcat ((char *) code[code_line], "\n");
									continue;
								}


								if (strcmp ((const char *) ast[level].expr[j][last_arg], "if") == 0 && last_arg == 0)
								{
									// error: no variable for if defined!

									printf ("error: line %lli: if variable not set!\n", linenum);
									return (1);
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "endif") == 0)
								{
									if_pos = get_act_if ();
									if (check_ifplus (if_pos) == 0)
									{
										// was "if+", check if else is set
										if (check_else (if_pos) != TRUE)
										{
											printf ("error: line %lli: if+ without else!\n", linenum);
											return (1);
										}
									}
									if (if_pos == -1)
									{
										printf ("error: line %lli: endif without if!\n", linenum);
										return (1);
									}

									get_endif_label (if_pos, endif_label);

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcat ((char *) code[code_line], (const char *) endif_label);
									strcat ((char *) code[code_line], "\n");

									set_endif_finished (if_pos);

									continue;
								}


								// switch statement ==============================================================
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "switch") == 0 && last_arg == 0)
								{
									switch_pos = get_switch_pos ();
									if (switch_pos == -1)
									{
										printf ("error: line %lli: switch: out of memory switch-list!\n", linenum);
										return (FALSE);
									}

									continue;
								}

								if ((strcmp ((const char *) ast[level].expr[j][last_arg], "switch-end") == 0 || strcmp ((const char *) ast[level].expr[j][last_arg], "switchend") == 0) && last_arg == 0)
								{
									switch_pos = get_act_switch ();
									if (switch_pos == -1)
									{
										printf ("error: line %lli: switchend without switch!\n", linenum);
										return (1);
									}
									
									set_switch_finished (switch_pos);

									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "break") == 0 && last_arg == 0)
								{
									if_pos = get_act_if ();

									get_endif_label (if_pos, endif_label);

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcat ((char *) code[code_line], (const char *) endif_label);
									strcat ((char *) code[code_line], "\n");

									set_endif_finished (if_pos);

									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "?") == 0 && last_arg == 2)
								{
									// it should be switch case statement: "x y ?"
									// check if variables are defined
									if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
									{
										return (1);
									}
									if (checkdef (ast[level].expr[j][last_arg - 2]) != 0)
									{
										return (1);
									}
									if (getvartype (ast[level].expr[j][last_arg - 1]) == INTEGER)
									{
										if (getvartype_real (ast[level].expr[j][last_arg - 1]) == QUADWORD)
										{
											// variable type qint
											// first variable
											reg = get_regi (ast[level].expr[j][last_arg - 1]);
											if (reg == -1)
											{
												// variable is not in register, load it

												reg = get_free_regi ();
												set_regi (reg, ast[level].expr[j][last_arg - 1]);

												// write code loada

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "loada ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 1]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");
											}

											// second variable
											reg2 = get_regi (ast[level].expr[j][last_arg - 2]);
											if (reg2 == -1)
											{
												// variable is not in register, load it

												reg2 = get_free_regi ();
												set_regi (reg2, ast[level].expr[j][last_arg - 2]);

												// write code loada

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "loada ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 2]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg2);
												strcpy ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");
											}
										}
										else
										{
											// variable is integer, but not qint!!
											// first variable
											reg3 = get_regi (ast[level].expr[j][last_arg - 1]);
											if (reg3 == -1)
											{
												// variable is not in register, load it

												reg3 = get_free_regi ();
												set_regi (reg3, ast[level].expr[j][last_arg - 1]);

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "load ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 1]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg3);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												{
													S2 vartype;

													vartype = getvartype_real (ast[level].expr[j][last_arg - 1]);
													switch (vartype)
													{
														case BYTE:
															strcpy ((char *) code[code_line], "pushb ");
															break;

														case WORD:
															strcpy ((char *) code[code_line], "pushw ");
															break;

														case DOUBLEWORD:
															strcpy ((char *) code[code_line], "pushdw ");
															break;
													}
												}

												sprintf ((char *) str, "%i", reg3);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], ", 0, ");

												reg = get_free_regi ();
												set_regi (reg, ast[level].expr[j][last_arg - 1]);

												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");
											}
											else
											{
												// use reg3
												reg = reg3;
												set_regi (reg, ast[level].expr[j][last_arg - 1]);
											}

											// second variable

											reg4 = get_regi (ast[level].expr[j][last_arg - 2]);
											if (reg4 == -1)
											{
												// variable is not in register, load it

												reg4 = get_free_regi ();
												set_regi (reg4, ast[level].expr[j][last_arg - 2]);

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "load ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 2]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg4);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												{
													S2 vartype;

													vartype = getvartype_real (ast[level].expr[j][last_arg - 2]);
													switch (vartype)
													{
														case BYTE:
															strcpy ((char *) code[code_line], "pushb ");
															break;

														case WORD:
															strcpy ((char *) code[code_line], "pushw ");
															break;

														case DOUBLEWORD:
															strcpy ((char *) code[code_line], "pushdw ");
															break;
													}
												}

												sprintf ((char *) str, "%i", reg4);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], ", 0, ");

												reg2 = get_free_regi ();
												set_regi (reg2, ast[level].expr[j][last_arg - 2]);

												sprintf ((char *) str, "%i", reg2);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");
											}
											else
											{
												// use reg4
												reg2 = reg4;
												set_regi (reg2, ast[level].expr[j][last_arg - 2]);
											}
										}

										// if: f variable, get free register
										target = get_free_regi ();
										set_regi (target, (U1 *) "temp");
										// set eqi compare opcode

										code_line++;
										if (code_line >= line_len)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (1);
										}

										strcpy ((char *) code[code_line], "eqi ");
										sprintf ((char *) str, "%i", reg);
										strcat ((char *) code[code_line], (const char *) str);
										strcat ((char *) code[code_line], ", ");

										sprintf ((char *) str, "%i", reg2);
										strcat ((char *) code[code_line], (const char *) str);
										strcat ((char *) code[code_line], ", ");

										// set target register
										sprintf ((char *) str, "%i", target);
										strcat ((char *) code[code_line], (const char *) str);
										strcat ((char *) code[code_line], "\n");

										// if like stuff:
										if_pos = get_if_pos ();
							            if (if_pos == -1)
								        {
							             	printf ("compile: error: if: out of memory if-list\n");
							            	return (FALSE);
							            }

										get_if_label (if_pos, if_label);
										get_endif_label (if_pos, endif_label);

										// write code jmpi to if code label

										code_line++;
										if (code_line >= line_len)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (1);
										}

										strcpy ((char *) code[code_line], "jmpi ");
										sprintf ((char *) str, "%i", target);
										strcat ((char *) code[code_line], (const char *) str);
										strcat ((char *) code[code_line], ", ");
										strcat ((char *) code[code_line], (const char *) if_label);
										strcat ((char *) code[code_line], "\n");

										// write code jmpi to endif label

										code_line++;
										if (code_line >= line_len)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (1);
										}

										strcpy ((char *) code[code_line], "jmp ");
										strcat ((char *) code[code_line], (const char *) endif_label);
										strcat ((char *) code[code_line], "\n");

										// write code label if

										code_line++;
										if (code_line >= line_len)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (1);
										}
										strcat ((char *) code[code_line], (const char *) if_label);
										strcat ((char *) code[code_line], "\n");
									}
									else
									{
										if (getvartype_real (ast[level].expr[j][last_arg - 1]) == DOUBLEFLOAT)
										{
											// variable type double
											reg = get_regd (ast[level].expr[j][last_arg - 1]);
											if (reg == -1)
											{
												// variable is not in register, load it

												reg = get_free_regd ();
												set_regd (reg, ast[level].expr[j][last_arg - 1]);

												// write code loada

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "loadd ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 1]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");
											}

											reg2 = get_regd (ast[level].expr[j][last_arg - 2]);
											if (reg2 == -1)
											{
												// variable is not in register, load it

												reg2 = get_free_regd ();
												set_regd (reg2, ast[level].expr[j][last_arg - 2]);

												// write code loada

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "loadd ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 2]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg2);
												strcpy ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");
											}

											// if: f variable, get free register
											reg3 = get_free_regi ();
											// set eqi compare opcode

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}

											strcpy ((char *) code[code_line], "eqd ");
											sprintf ((char *) str, "%i", reg);
											strcat ((char *) code[code_line], (const char *) str);
											strcat ((char *) code[code_line], ", ");

											sprintf ((char *) str, "%i", reg2);
											strcat ((char *) code[code_line], (const char *) str);
											strcat ((char *) code[code_line], ", ");

											// set target register
											sprintf ((char *) str, "%i", reg3);
											strcat ((char *) code[code_line], (const char *) str);
											strcat ((char *) code[code_line], "\n");

											// if like stuff:
											if_pos = get_if_pos ();
							                if (if_pos == -1)
								               {
							                    printf ("compile: error: if: out of memory if-list\n");
							                    return (FALSE);
							                }

											get_if_label (if_pos, if_label);
											get_endif_label (if_pos, endif_label);

											// write code jmpi to if code label

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}

											strcpy ((char *) code[code_line], "jmpi ");
											sprintf ((char *) str, "%i", reg3);
											strcat ((char *) code[code_line], (const char *) str);
											strcat ((char *) code[code_line], ", ");
											strcat ((char *) code[code_line], (const char *) if_label);
											strcat ((char *) code[code_line], "\n");

											// write code jmpi to endif label

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}

											strcpy ((char *) code[code_line], "jmp ");
											strcat ((char *) code[code_line], (const char *) endif_label);
											strcat ((char *) code[code_line], "\n");

											// write code label if

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}
											strcat ((char *) code[code_line], (const char *) if_label);
											strcat ((char *) code[code_line], "\n");
										}

									}
									continue;
								}

								// do/ while loop ===============================================================
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "do") == 0)
								{
									while_pos = get_while_pos ();
									if (while_pos == -1)
									{
										printf ("compile: error: while: out of memory while-list\n");
										return (FALSE);
									}

									get_while_label (while_pos, while_label);

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcpy ((char *) code[code_line], (const char *) while_label);
									strcat ((char *) code[code_line], "\n");

									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "while") == 0 && last_arg > 0)
								{
									if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
									{
										return (1);
									}
									if (getvartype (ast[level].expr[j][last_arg - 1]) == INTEGER)
									{
										if (getvartype_real (ast[level].expr[j][last_arg - 1]) == QUADWORD)
										{
											reg = get_regi (ast[level].expr[j][last_arg - 1]);
											if (reg == -1)
											{
												// variable is not in register, load it

												reg = get_free_regi ();
												set_regi (reg, ast[level].expr[j][last_arg - 1]);

												// write code loada

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "loada ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 1]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");
											}

											while_pos = get_act_while ();
											if (while_pos == -1)
											{
												printf ("error: line %lli: while: do not set!\n", linenum);
												return (1);
											}

											get_while_label (while_pos, while_label);

											// write code jmpi to while code label

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}

											strcpy ((char *) code[code_line], "jmpi ");
											sprintf ((char *) str, "%i", reg);
											strcat ((char *) code[code_line], (const char *) str);
											strcat ((char *) code[code_line], ", ");
											strcat ((char *) code[code_line], (const char *) while_label);
											strcat ((char *) code[code_line], "\n");

											set_wend (while_pos);

											continue;
										}
										else
										{
											printf ("error: line %lli: while variable must be int64 type!\n", linenum);
											return (1);
										}
									}
									else
									{
										printf ("error: line %lli: while variable must be int64 type!\n", linenum);
										return (1);
									}
								}


								// for/next loop ===============================================================

								// set top for loop label
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "for-loop") == 0)
								{
									for_pos = get_for_pos ();
									if (for_pos == -1)
									{
										printf ("compile: error: for: out of memory for-list\n");
										return (FALSE);
									}

									get_for_label (for_pos, for_label);

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcpy ((char *) code[code_line], (const char *) for_label);
									strcat ((char *) code[code_line], "\n");

									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "for") == 0 && last_arg > 0)
								{
									if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
									{
										return (1);
									}
									if (getvartype (ast[level].expr[j][last_arg - 1]) == INTEGER)
									{
										if (getvartype_real (ast[level].expr[j][last_arg - 1]) == QUADWORD)
										{
											reg = get_regi (ast[level].expr[j][last_arg - 1]);
											if (reg == -1)
											{
												// variable is not in register, load it

												reg = get_free_regi ();
												set_regi (reg, ast[level].expr[j][last_arg - 1]);

												// write code loada

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], "loada ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 1]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");
											}

											for_pos = get_act_for ();
											if (for_pos == -1)
											{
												printf ("compile: error: for: out of memory for-list\n");
												return (FALSE);
											}
											get_for_label_2 (for_pos, for_label);

											// write code jmpi to for label 2 code label, begin of for loop

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}

											strcpy ((char *) code[code_line], "jmpi ");
											sprintf ((char *) str, "%i", reg);
											strcat ((char *) code[code_line], (const char *) str);
											strcat ((char *) code[code_line], ", ");
											strcat ((char *) code[code_line], (const char *) for_label);
											strcat ((char *) code[code_line], "\n");

											get_for_label_end (for_pos, for_label);

											// write code jmpi to for label 3 code label, end of loop

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}

											strcpy ((char *) code[code_line], "jmp ");
											strcat ((char *) code[code_line], (const char *) for_label);
											strcat ((char *) code[code_line], "\n");

											get_for_label_2 (for_pos, for_label);

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}

											strcpy ((char *) code[code_line], (const char *) for_label);
											strcat ((char *) code[code_line], "\n");

											set_for (for_pos);

											continue;
										}
										else
										{
											printf ("error: line %lli: for variable must be int64 type!\n", linenum);
											return (1);
										}
									}
									else
									{
										printf ("error: line %lli: for variable must be int64 type!\n", linenum);
										return (1);
									}
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "next") == 0)
								{
									for_pos = get_act_for ();
									if (for_pos == -1)
									{
										printf ("error: line: %lli: next without for!\n", linenum);
										return (1);
									}

									if (check_for (for_pos) == FALSE)
									{
										printf ("error: line: %lli: next without for!\n", linenum);
										return (1);
									}

									get_for_label (for_pos, for_label);

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcpy ((char *) code[code_line], "jmp ");
									strcat ((char *) code[code_line], (const char *) for_label);
									strcat ((char *) code[code_line], "\n");

									// set next label end as jump point if for loop not longer is running
									get_for_label_end (for_pos, for_label);

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcpy ((char *) code[code_line], (const char *) for_label);
									strcat ((char *) code[code_line], "\n");

									set_for_end (for_pos);
									continue;
								}

								// set optimize if flag =========================================================
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "optimize-if") == 0)
								{
									optimize_if = 1;
									continue;
								}

								// remove optimize if flag ======================================================
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "optimize-if-off") == 0)
								{
									optimize_if = 0;
									continue;
								}

								// call =========================================================================
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "call") == 0 || strcmp ((const char *) ast[level].expr[j][last_arg], "!") == 0)
								{
									// function call with arguments: "call" or "!" !!!

									// first of all use save register code

									// save integer registers
									for (e = 0; e < MAXREG; e++)
									{
										if (strcmp ((const char *) regi[e], "") != 0)
										{
											strcpy ((char *) code_temp, "stpushi ");
											sprintf ((char *) str, "%i", e);
											strcat ((char *) code_temp, (const char*) str);
											strcat ((char *) code_temp, "\n");

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}

											strcpy ((char *) code[code_line], (const char *) code_temp);
											save_regi[e] = 1;
										}
										else
										{
											save_regi[e] = 0;
										}
									}

									// save double registers
									for (e = 0; e < MAXREG; e++)
									{
										if (strcmp ((const char *) regd[e], "") != 0)
										{
											strcpy ((char *) code_temp, "stpushd ");
											sprintf ((char *) str, "%i", e);
											strcat ((char *) code_temp, (const char*) str);
											strcat ((char *) code_temp, "\n");

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}

											strcpy ((char *) code[code_line], (const char *) code_temp);
											save_regd[e] = 1;
										}
										else
										{
											save_regd[e] = 0;
										}
									}

									// now check for function arguments
									// and push them

									if (last_arg >= 2)
									{
										if (ast[level].expr[j][last_arg - 1][0] == '*')
										{
											// found star: * as loadreg code
											// set loadreg opcodes on end
											set_loadreg = 1;
											call_set = 0;
											// correct arguments
											last_arg = last_arg - 1;
										}

										for (e = 0; e < last_arg - 1; e++)
										{
											if (checkdef (ast[level].expr[j][e]) != 0)
											{
												return (1);
											}
											if (getvartype (ast[level].expr[j][e]) == INTEGER)
											{
												if (getvartype_real (ast[level].expr[j][e]) == BYTE)
												{
													strcpy ((char *) code_temp, "stpushb ");
												}
												else
												{
													strcpy ((char *) code_temp, "stpushi ");
												}

												reg = get_regi (ast[level].expr[j][e]);
												if (reg == -1 || reg > -1)
												{
													if (reg > -1)
													{
														// clear old value of register
														set_regi (reg, (U1 *) "");
														reg = get_free_regi ();
														set_regi (reg, ast[level].expr[j][e]);
													}
													else
													{
														reg = get_free_regi ();
														set_regi (reg, ast[level].expr[j][e]);
													}

													// write code loada

													code_line++;
													if (code_line >= line_len)
													{
														printf ("error: line %lli: code list full!\n", linenum);
														return (1);
													}

													{
														// new scope
														S2 reg_p;

														if (getvartype_real (ast[level].expr[j][e]) == BYTE)
														{
															strcpy ((char *) code[code_line], "load ");
															strcat ((char *) code[code_line], (const char *) ast[level].expr[j][e]);
															strcat ((char *) code[code_line], ", 0, ");
															sprintf ((char *) str, "%i", reg);
															strcat ((char *) code[code_line], (const char *) str);
															strcat ((char *) code[code_line], "\n");

															code_line++;
															if (code_line >= line_len)
															{
																printf ("error: line %lli: code list full!\n", linenum);
																return (1);
															}

															// set stpushb opcode
															strcpy ((char *) code[code_line], "pushb ");
															sprintf ((char *) str, "%i", reg);
															strcat ((char *) code[code_line], (const char *) str);
															strcat ((char *) code[code_line], ", 0, ");

															// get free register
															reg_p = get_free_regi ();
															// set_regi (reg, ast[level].expr[j][e]);

															sprintf ((char *) str, "%i", reg_p);
															strcat ((char *) code[code_line], (const char *) str);
															strcat ((char *) code[code_line], "\n");

															// set register as used
															set_regi (reg_p, ast[level].expr[j][e]);
														}

														if (getvartype_real (ast[level].expr[j][e]) == WORD)
														{
															// set load opcode
															strcpy ((char *) code[code_line], "load ");
															strcat ((char *) code[code_line], (const char *) ast[level].expr[j][e]);
															strcat ((char *) code[code_line], ", 0, ");
															sprintf ((char *) str, "%i", reg);
															strcat ((char *) code[code_line], (const char *) str);
															strcat ((char *) code[code_line], "\n");

															code_line++;
															if (code_line >= line_len)
															{
																printf ("error: line %lli: code list full!\n", linenum);
																return (1);
															}

															// set pushw opcode
															strcpy ((char *) code[code_line], "pushw ");
															sprintf ((char *) str, "%i", reg);
															strcat ((char *) code[code_line], (const char *) str);
															strcat ((char *) code[code_line], ", 0, ");

															// get free register
															reg_p = get_free_regi ();

															sprintf ((char *) str, "%i", reg_p);
															strcat ((char *) code[code_line], (const char *) str);
															strcat ((char *) code[code_line], "\n");

															// set register as used
															set_regi (reg_p, ast[level].expr[j][e]);
														}

														if (getvartype_real (ast[level].expr[j][e]) == DOUBLEWORD)
														{
															// set load opcode
															strcpy ((char *) code[code_line], "load ");
															strcat ((char *) code[code_line], (const char *) ast[level].expr[j][e]);
															strcat ((char *) code[code_line], ", 0, ");
															sprintf ((char *) str, "%i", reg);
															strcat ((char *) code[code_line], (const char *) str);
															strcat ((char *) code[code_line], "\n");

															code_line++;
															if (code_line >= line_len)
															{
																printf ("error: line %lli: code list full!\n", linenum);
																return (1);
															}

															// set pushw opcode
															strcpy ((char *) code[code_line], "pushdw ");
															sprintf ((char *) str, "%i", reg);
															strcat ((char *) code[code_line], (const char *) str);
															strcat ((char *) code[code_line], ", 0, ");

															// get free register
															reg_p = get_free_regi ();

															sprintf ((char *) str, "%i", reg_p);
															strcat ((char *) code[code_line], (const char *) str);
															strcat ((char *) code[code_line], "\n");

															// set register as used
															set_regi (reg_p, ast[level].expr[j][e]);
														}

														if (getvartype_real (ast[level].expr[j][e]) == QUADWORD)
														{
															strcpy ((char *) code[code_line], "loada ");
															strcat ((char *) code[code_line], (const char *) ast[level].expr[j][e]);
															strcat ((char *) code[code_line], ", 0, ");
															sprintf ((char *) str, "%i", reg);
															strcat ((char *) code[code_line], (const char *) str);
															strcat ((char *) code[code_line], "\n");

															// set register as used
															set_regi (reg, ast[level].expr[j][e]);
														}

														strcat ((char *) code_temp, (const char *) str);
													} // new scope end
												}
												else
												{
													sprintf ((char *) str, "%i", reg);
													strcat ((char *) code_temp, (const char *) str);
												}
											}

											if (checkdef (ast[level].expr[j][e]) != 0)
											{
												return (1);
											}
											if (getvartype (ast[level].expr[j][e]) == DOUBLE)
											{
												strcpy ((char *) code_temp, "stpushd ");

												// double float type
												reg = get_regd (ast[level].expr[j][e]);
												if (reg == -1 || reg > -1)
												{
													if (reg > -1)
													{
														// clear old value of register
														set_regd (reg, (U1 *) "");
														reg = get_free_regi ();
														set_regd (reg, ast[level].expr[j][e]);
													}
													else
													{
														reg = get_free_regd ();
														set_regd (reg, ast[level].expr[j][e]);
													}

													// write code loadd

													code_line++;
													if (code_line >= line_len)
													{
														printf ("error: line %lli: code list full!\n", linenum);
														return (1);
													}

													strcpy ((char *) code[code_line], "loadd ");
													strcat ((char *) code[code_line], (const char *) ast[level].expr[j][e]);
													strcat ((char *) code[code_line], ", 0, ");
													sprintf ((char *) str, "%i", reg);
													strcat ((char *) code[code_line], (const char *) str);
													strcat ((char *) code[code_line], "\n");

													strcat ((char *) code_temp, (const char *) str);
													/*
													if (e <= last_arg - 1)
													{
														strcat ((char *) code_temp, ", ");
													}
													*/
												}
												else
												{
													sprintf ((char *) str, "%i", reg);
													strcat ((char *) code_temp, (const char *) str);
												}
											}

											if (checkdef (ast[level].expr[j][e]) != 0)
											{
												return (1);
											}
											if (getvartype (ast[level].expr[j][e]) == STRING)
											{
												strcpy ((char *) code_temp, "stpushi ");

												strcpy ((char *) str, (const char *) ast[level].expr[j][e]);
												strcat ((char *) str, "addr");

												reg = get_regi (ast[level].expr[j][e]);
												if (reg == -1 || reg > -1)
												{
													if (reg > -1)
													{
														// clear old value of register
														set_regi (reg, (U1 *) "");
														reg = get_free_regi ();
														set_regi (reg, ast[level].expr[j][e]);
													}
													else
													{
														reg = get_free_regi ();
														set_regi (reg, ast[level].expr[j][e]);
													}

													// write code loada

													code_line++;
													if (code_line >= line_len)
													{
														printf ("error: line %lli: code list full!\n", linenum);
														return (1);
													}

													strcpy ((char *) code[code_line], "loada ");
													strcat ((char *) code[code_line], (const char *) str);
													strcat ((char *) code[code_line], ", 0, ");
													sprintf ((char *) str, "%i", reg);
													strcat ((char *) code[code_line], (const char *) str);
													strcat ((char *) code[code_line], "\n");

													strcat ((char *) code_temp, (const char *) str);
													/*
													if (e <= last_arg - 1)
													{
														strcat ((char *) code_temp, ", ");
													}
													*/
												}
												else
												{
													sprintf ((char *) str, "%i", reg);
													strcat ((char *) code_temp, (const char *) str);
												}
											}

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}

											strcpy ((char *) code[code_line], (const char *) code_temp);
											strcat ((char *) code[code_line], "\n");
										}
									}

									strcpy ((char *) code_temp, "jsr ");
									strcat ((char *) code_temp, (const char *) ast[level].expr[j][last_arg - 1]);
									strcat ((char *) code_temp, "\n");

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcpy ((char *) code[code_line], (const char *) code_temp);

									// set call_label if not set already!
									if (set_call_label (ast[level].expr[j][last_arg - 1]) != 0)
									{
										printf ("error: line %lli: call label list full!\n", linenum);
										return (1);
									}

									if (set_loadreg == 1)
									{
										// load double registers
										for (e = MAXREG - 1; e >= 0; e--)
										{
											if (save_regd[e] == 1)
											{
												strcpy ((char *) code_temp, "stpopd ");
												sprintf ((char *) str, "%i", e);
												strcat ((char *) code_temp, (const char*) str);
												strcat ((char *) code_temp, "\n");

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], (const char *) code_temp);
											}
										}

										// load integer registers
										for (e = MAXREG - 1; e >= 0; e--)
										{
											if (save_regi[e] == 1)
											{
												strcpy ((char *) code_temp, "stpopi ");
												sprintf ((char *) str, "%i", e);
												strcat ((char *) code_temp, (const char*) str);
												strcat ((char *) code_temp, "\n");

												code_line++;
												if (code_line >= line_len)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (1);
												}

												strcpy ((char *) code[code_line], (const char *) code_temp);
											}
										}

										set_loadreg = 0;
									}
									else
									{
										call_set = 1;
										call_loadreg = 0;
									}
									continue;
								}

								if (ok == 0)
								{
									printf ("error: line %lli: unknown operator: '%s'\n", linenum, ast[level].expr[j][last_arg]);
									return (1);
								}
							}

							if (t < MAXTRANSLATE)
							{
								// write opcode name to code_temp

								strcpy ((char *) code_temp, (const char *) opcode[translate[t].assemb_op].op);
								strcat ((char *) code_temp, " ");

								// check if JMP, JMPI or JSR, to set call_label

								switch (translate[t].assemb_op)
								{
									case JMP:
									case JMPI:
									case JSR:
										// set call_label if not set already!
										if (set_call_label (ast[level].expr[j][last_arg - 1]) != 0)
										{
											printf ("error: line %lli: call label list full!\n", linenum);
											return (1);
										}
										break;
								}
							}
							else
							{
								printf ("error: line %lli: unknown opcode!\n", linenum);
								return (1);
							}

							// TRANSLATE LOOP END!! =======================================================================
							if (level < ast_level)
							{
								e = level + 1;
								// printf ("level: %i\n", e);
								int found_exp = 0;
								int found_level = 0;
								int target_reg = 0;

								for (exp = 0; exp <= ast[e].expr_max; exp = exp + 2)
								{
									if (ast[e].expr_reg[exp] >= 0)
									{
										sprintf ((char *) str, "%i", ast[e].expr_reg[exp]);
										strcat ((char *) code_temp, (const char *) str);
										strcat ((char *) code_temp, ", ");
										ast[e].expr_reg[exp] = -1;
										found_exp++;
										found_level = e;
									}

									if (ast[e].expr_reg[exp + 1] >= 0)
									{
										sprintf ((char *) str, "%i", ast[e].expr_reg[exp + 1]);
										strcat ((char *) code_temp, (const char *) str);
										strcat ((char *) code_temp, ", ");
										ast[e].expr_reg[exp + 1] = -1;
									}
									if (found_exp > 0) break;
								}

								if (found_exp > 0)
								{
									if (ast[e].expr_type[exp] == INTEGER)
									{
										reg = get_free_regi ();
										set_regi (reg, (U1 *) "temp");
									}

									if (ast[e].expr_type[exp] == DOUBLE)
									{
										reg = get_free_regd ();
										set_regd (reg, (U1 *) "temp");
									}

									sprintf ((char *) str, "%i", reg);
									strcat ((char *) code_temp, (const char *) str);

									target = reg;

									int t = 0;
									while (t < MAXEXPRESSION)
									{
										if (ast[found_level - 1].expr_reg[t] == -2)
										{
											target_reg = t;
											break;
										}
										t++;
									}

									ast[found_level - 1].expr_reg[target_reg] = reg;
									ast[found_level - 1].expr_type[target_reg] = ast[e].expr_type[exp];

									ast[e].expr_type[j] = ast[e - 1].expr_type[exp];

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcpy ((char *) code[code_line], (const char *) code_temp);
									strcat ((char *) code[code_line], "\n");

									#if DEBUG
										printf ("translate loop end: code_temp 3: '%s'\n", code_temp);
									#endif

									found_exp = 0;
								}

								continue;
							}

							if (t >= MAXTRANSLATE)
							{
								printf ("error: line %lli: unknown opcode!\n", linenum);
								return (1);
							}

							if (opcode[translate[t].assemb_op].args == 1)
							{
								last_arg_2 = 1;
							}
							else
							{
								if (opcode[translate[t].assemb_op].args == 2)
								{
									last_arg_2 = 2;
								}
								else
								{
									last_arg_2 = last_arg;
								}
							}

							if (last_arg_2 == 1)
							{
								if (translate[t].assemb_op == STPOPI)
								{
									target = get_regi (ast[level].expr[j][last_arg - 1]);
									if (target == -1)
									{
										// variable is not in register, load it

										reg2 = get_free_regi ();
										set_regi (reg2, ast[level].expr[j][last_arg - 1]);
										target = reg2;
									}

									strcpy ((char *) code_temp, "stpopi ");
									sprintf ((char *) str, "%i", target);
									strcat ((char *) code_temp, (const char *) str);
									strcat ((char *) code_temp, "\n");

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcpy ((char *) code[code_line], (const char *) code_temp);

									strcpy ((char *) code_temp, "load ");
									strcat ((char *) code_temp, (const char *) ast[level].expr[j][last_arg - 1]);
									strcat ((char *) code_temp, ", 0, ");

									reg = get_free_regi ();

									sprintf ((char *) str, "%i", reg);
									strcat ((char *) code_temp, (const char *) str);
									strcat ((char *) code_temp, "\n");

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcpy ((char *) code[code_line], (const char *) code_temp);

									// create new scope for setting the needed pull opcode:
									{
										S8 vartype ALIGN;
										vartype = getvartype_real (ast[level].expr[j][last_arg - 1]);
										switch (vartype)
										{
											case BYTE:
												strcpy ((char *) code_temp, "pullb ");
												break;

											case WORD:
												strcpy ((char *) code_temp, "pullw ");
												break;

											case DOUBLEWORD:
												strcpy ((char *) code_temp, "pulldw ");
												break;

											case QUADWORD:
												strcpy ((char *) code_temp, "pullqw ");
												break;
										}
									}

									sprintf ((char *) str, "%i", target);
									strcat ((char *) code_temp, (const char *) str);
									strcat ((char *) code_temp, ", ");
									sprintf ((char *) str, "%i", reg);
									strcat ((char *) code_temp, (const char *) str);
									strcat ((char *) code_temp, ", 0\n");

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcpy ((char *) code[code_line], (const char *) code_temp);

									reg = get_regi (ast[level].expr[j][last_arg - 1]);
									if (reg != -1)
									{
										// set old value of reg as empty
										set_regi (reg, (U1 *) "");
									}

									set_regi (target, ast[level].expr[j][last_arg - 1]);

									continue;
								}

								if (translate[t].assemb_op == STPOPB)
								{
									target = get_regi (ast[level].expr[j][last_arg - 1]);
									if (target == -1)
									{
										// variable is not in register, load it

										reg2 = get_free_regi ();
										set_regi (reg2, ast[level].expr[j][last_arg - 1]);
										target = reg2;
									}

									strcpy ((char *) code_temp, "stpopb ");
									sprintf ((char *) str, "%i", target);
									strcat ((char *) code_temp, (const char *) str);
									strcat ((char *) code_temp, "\n");

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcpy ((char *) code[code_line], (const char *) code_temp);


									strcpy ((char *) code_temp, "load ");
									strcat ((char *) code_temp, (const char *) ast[level].expr[j][last_arg - 1]);
									strcat ((char *) code_temp, ", 0, ");

									reg = get_free_regi ();

									sprintf ((char *) str, "%i", reg);
									strcat ((char *) code_temp, (const char *) str);
									strcat ((char *) code_temp, "\n");

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcpy ((char *) code[code_line], (const char *) code_temp);

									strcpy ((char *) code_temp, "pullb ");
									sprintf ((char *) str, "%i", target);
									strcat ((char *) code_temp, (const char *) str);
									strcat ((char *) code_temp, ", ");
									sprintf ((char *) str, "%i", reg);
									strcat ((char *) code_temp, (const char *) str);
									strcat ((char *) code_temp, ", 0\n");

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcpy ((char *) code[code_line], (const char *) code_temp);

									reg = get_regi (ast[level].expr[j][last_arg - 1]);
									if (reg != -1)
									{
										// set old value of reg as empty
										set_regi (reg, (U1 *) "");
									}

									set_regi (target, ast[level].expr[j][last_arg - 1]);

									continue;
								}

								if (translate[t].assemb_op == STPOPD)
								{
									target = get_regd (ast[level].expr[j][last_arg - 1]);
									if (target == -1)
									{
										// variable is not in register, load it

										reg2 = get_free_regd ();
										set_regd (reg2, ast[level].expr[j][last_arg - 1]);
										target = reg2;
									}

									strcpy ((char *) code_temp, "stpopd ");
									sprintf ((char *) str, "%i", target);
									strcat ((char *) code_temp, (const char *) str);
									strcat ((char *) code_temp, "\n");

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcpy ((char *) code[code_line], (const char *) code_temp);

									strcpy ((char *) code_temp, "load ");
									strcat ((char *) code_temp, (const char *) ast[level].expr[j][last_arg - 1]);
									strcat ((char *) code_temp, ", 0, ");

									reg = get_free_regd ();

									sprintf ((char *) str, "%i", reg);
									strcat ((char *) code_temp, (const char *) str);
									strcat ((char *) code_temp, "\n");

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcpy ((char *) code[code_line], (const char *) code_temp);

									strcpy ((char *) code_temp, "pulld ");
									sprintf ((char *) str, "%i", target);
									strcat ((char *) code_temp, (const char *) str);
									strcat ((char *) code_temp, ", ");
									sprintf ((char *) str, "%i", reg);
									strcat ((char *) code_temp, (const char *) str);
									strcat ((char *) code_temp, ", 0\n");

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcpy ((char *) code[code_line], (const char *) code_temp);

									reg = get_regd (ast[level].expr[j][last_arg - 1]);
									if (reg != -1)
									{
										// set old value of reg as empty
										set_regd (reg, (U1 *) "");
									}

									set_regd (target, ast[level].expr[j][last_arg - 1]);

									continue;
								}
							}

							for (v = 0; v < last_arg_2; v++)
							{
								if (checkdef (ast[level].expr[j][v]) != 0)
								{
									return (1);
								}
								if (getvartype (ast[level].expr[j][v]) == INTEGER)
								{
									reg = get_regi (ast[level].expr[j][v]);
									if (reg == -1)
									{
										// variable is not in register, load it

                                        if (translate[t].assemb_op == MOVI && v == 1)
                                        {
                                            // is target register, don't load variable into it

                                            reg = get_free_regi ();
                                            set_regi (reg, ast[level].expr[j][v]);

                                            strcat ((char *) code_temp, (const char *) str);
                                            if (v <= last_arg - 1 && last_arg_2 >= 2)
                                            {
                                                strcat ((char *) code_temp, ", ");
                                            }
                                        }
                                        else
                                        {
                                            reg = get_free_regi ();
                                            set_regi (reg, ast[level].expr[j][v]);

											// write code loada

                                            code_line++;
                                            if (code_line >= line_len)
                                            {
                                                printf ("error: line %lli: code list full!\n", linenum);
                                                return (1);
                                            }

                                            strcpy ((char *) code[code_line], "loada ");
                                            strcat ((char *) code[code_line], (const char *) ast[level].expr[j][v]);
                                            strcat ((char *) code[code_line], ", 0, ");
                                            sprintf ((char *) str, "%i", reg);
                                            strcat ((char *) code[code_line], (const char *) str);
                                            strcat ((char *) code[code_line], "\n");

                                            strcat ((char *) code_temp, (const char *) str);
                                            if (v <= last_arg - 1 && last_arg_2 >= 2)
                                            {
                                                strcat ((char *) code_temp, ", ");
                                            }
										}
									}
									else
									{
										sprintf ((char *) str, "%i", reg);
										strcat ((char *) code_temp, (const char *) str);
										if (v <= last_arg - 1 && last_arg_2 >= 2)
										{
											strcat ((char *) code_temp, ", ");
										}
									}

									if (translate[t].assemb_op == MOVI && v == 0)
                                    {
                                        // set variable register as unused, we move it!

                                        reg = get_regi (ast[level].expr[j][v]);
                                        if (reg != -1)
                                        {
                                            // set old value of reg as empty
                                            set_regi (reg, (U1 *) "");
                                        }
                                    }

                                   	if (translate[t].assemb_op == MOVI && v == 1)
                                   	{
                                       	// set target register variable name
                                       	set_regi (reg, (U1 *) ast[level].expr[j][v]);
                                       	target = reg;
                                   	}
								}

								if (checkdef (ast[level].expr[j][v]) != 0)
								{
									return (1);
								}
								if (getvartype (ast[level].expr[j][v]) == DOUBLE)
								{
									// double float type

									reg = get_regd (ast[level].expr[j][v]);
									if (reg == -1)
									{
										// variable is not in register, load it

                                        if (translate[t].assemb_op == MOVD && v == 1)
                                        {
                                            // is target register, don't load variable into it

                                            reg = get_free_regd ();
                                            set_regd (reg, ast[level].expr[j][v]);

                                            strcat ((char *) code_temp, (const char *) str);
                                            if (v <= last_arg - 1 && last_arg_2 >= 2)
                                            {
                                                strcat ((char *) code_temp, ", ");
                                            }
                                        }
                                        else
                                        {
                                            reg = get_free_regd ();
                                            set_regd (reg, ast[level].expr[j][v]);

											// write code loadd

											code_line++;
											if (code_line >= line_len)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (1);
											}

											strcpy ((char *) code[code_line], "loadd ");
											strcat ((char *)code[code_line], (const char *) ast[level].expr[j][v]);
											strcat ((char *) code[code_line], ", 0, ");
											sprintf ((char *) str, "%i", reg);
											strcat ((char *) code[code_line], (const char *) str);
											strcat ((char *) code[code_line], "\n");
										}

										strcat ((char *) code_temp, (const char *) str);
										if (v <= last_arg - 1 && last_arg_2 >= 2)
										{
											strcat ((char *) code_temp, ", ");
										}
									}
									else
									{
										sprintf ((char *) str, "%i", reg);
										strcat ((char *) code_temp, (const char *) str);
										if (v <= last_arg - 1)
										{
											strcat ((char *) code_temp, ", ");
										}
									}
								}
								if (getvartype (ast[level].expr[j][v]) == LABEL)
								{
									strcat ((char *) code_temp, (const char *) ast[level].expr[j][v]);
								}
							}

							if (last_arg_2 > 1)
							{
								if (translate[t].assemb_op != MOVI && translate[t].assemb_op != MOVD)
								{
									if (opcode[translate[t].assemb_op].type[opcode[translate[t].assemb_op].args - 1] == I_REG)
									{
										// get free integer target register
										reg = get_free_regi ();
										set_regi (reg, (U1 *) "temp");

										sprintf ((char *) str, "%i", reg);
										strcat ((char *) code_temp, (const char *) str);

										ast[level].expr_reg[j] = reg;
										ast[level].expr_type[j] = INTEGER;
										target = reg;
									}
									if (opcode[translate[t].assemb_op].type[opcode[translate[t].assemb_op].args - 1] == D_REG)
									{
										// get free double target register
										reg = get_free_regd ();
										set_regd (reg, (U1 *) "temp");

										sprintf ((char *) str, "%i", reg);
										strcat ((char *) code_temp, (const char *) str);

										ast[level].expr_reg[j] = reg;
										ast[level].expr_type[j] = DOUBLE;
										target = reg;
									}
								}
								if (translate[t].assemb_op == MOVI || translate[t].assemb_op == MOVD)
								{
									e = strlen_safe ((const char *) code_temp, MAXLINELEN);
									code_temp[e - 2] = '\0';
								}
							}
							code_line++;
							if (code_line >= line_len)
							{
								printf ("error: line %lli: code list full!\n", linenum);
								return (1);
							}

							strcpy ((char *) code[code_line], (const char *) code_temp);
							strcat ((char *) code[code_line], "\n");
						}
					}
				}
			}
		}
	}
	return (0);
}

S2 check_file_ending (U1 *name)
{
	S4 slen, i, j;
	slen = strlen_safe ((const char *) name, MAXLINES);

	for (i = 0; i < slen; i++)
	{
		if (name[i] == '.')
		{
			j = i;
			if (i + 5 < slen)
			{
				if (name[i + 1] == 'l' && name[i + 2] == '1' && name[i + 3] == 'c' && name[i + 4] == 'o' && name[i + 5] == 'm')
				{
					name[j] = '\0'; // set name end
					return (0);
				}
			}
		}
	}
	return (0);
}

S2 parse (U1 *name)
{
    FILE *fptr;
    U1 asmname[512];
    S4 slen, pos;
    U1 rbuf[MAXLINELEN + 1];                        /* read-buffer for one line */
    char *read;
	S8 ret ALIGN;

	S8 code_lines ALIGN = 0;

    slen = strlen_safe ((const char *) name, MAXLINES);
    U1 ok, err = 0;

    if (slen > 506)
    {
        printf ("ERROR: filename too long!\n");
        return (1);
    }

	// check if ".l1com" ending is on file name or not
	check_file_ending (name);
	strcpy ((char *) asmname, (const char *) name);
	strcat ((char *) asmname, ".l1com");

    fptr = fopen ((const char *) asmname, "r");
    if (fptr == NULL)
    {
        printf ("ERROR: can't open file '%s'!\n", asmname);
        return (1);
    }

    ok = TRUE;
    while (ok)
    {
        read = fgets_uni ((char *) rbuf, MAXLINELEN, fptr);
        if (read != NULL)
        {
            slen = strlen_safe ((const char *) rbuf, MAXLINELEN);
			if (slen == 0)
			{
				printf ("ERROR: line: %lli: length is 0 or too long!\n", linenum);
				return (1);
			}

			convtabs (rbuf);  /* convert the funny tabs into spaces! */
			// printf ("[ %s ]\n", rbuf);

            pos = searchstr (rbuf, (U1 *) REM_SB, 0, 0, TRUE);
            if (pos == -1)
            {
				code_lines++;

				// no comment, parse line
				pos = searchstr (rbuf, (U1 *) ASM_SB, 0, 0, TRUE);
				if (pos != -1)
				{
					inline_asm = 1;
					continue;
				}

				pos = searchstr (rbuf, (U1 *) ASM_END_SB, 0, 0, TRUE);
				if (pos != -1)
				{
					inline_asm = 0;
					continue;
				}

				// search for "@," array variable assign in more than one line:
				// @, 32Q, 10, 5,
				// @, 1234567890, 4, 3, ;
				pos = searchstr (rbuf, (U1 *) "@,", 0, 0, TRUE);
				if (pos != -1)
				{
					data_line++;
					if (data_line >= line_len)
					{
						printf ("error: line %lli: data list full!\n", linenum);
						return (1);
					}

					strcpy ((char *) data[data_line], (const char *) rbuf);
					continue;
				}


				if (inline_asm == 1)
				{
					code_line++;
					if (code_line >= line_len)
					{
						printf ("error: line %lli: code list full!\n", linenum);
						return (1);
					}

					strcpy ((char *) code[code_line], (const char *) rbuf);
				}
				else
				{
					ret = parse_line (rbuf);
					if (ret != 0)
					{
						printf ("> %s", rbuf);
						err = 1;

						if (ret == 2)
						{
							// ERROR brackets don't match
							fclose (fptr);
							return (err);
						}
					}
				}
            }
            linenum++;
        }
        else
		{
			ok = FALSE;
		}
    }
    fclose (fptr);
	printf ("\033[0mcode lines compiled: %lli\n", code_lines);
    return (err);
}

S2 write_asm (U1 *name)
{
	FILE *fptr;
	U1 asmname[512];
	S4 slen;
	S8 i ALIGN;

	slen = strlen_safe ((const char *) name, MAXLINELEN);

	if (slen > 506)
	{
		printf ("ERROR: filename too long!\n");
		return (1);
	}

	strcpy ((char *) asmname, (const char *) name);
	strcat ((char *) asmname, ".l1asm");

	fptr = fopen ((const char *) asmname, "w");
	if (fptr == NULL)
	{
		printf ("ERROR: can't open file '%s'!\n", asmname);
		return (1);
	}

	for (i = 0; i <= data_line; i++)
	{
		if (fprintf (fptr, "%s", data[i]) < 0)
		{
			fclose (fptr);
			printf ("error: writing file %s !\n", asmname);
			return (1);
		}
	}

	if (fprintf (fptr, ".dend\n.code\n") < 0)
	{
		fclose (fptr);
		printf ("error: writing file %s !\n", asmname);
		return (1);
	}

	for (i = 0; i <= code_line; i++)
	{
		if (fprintf (fptr, "%s", code[i]) < 0)
		{
			fclose (fptr);
			printf ("error: writing file %s !\n", asmname);
			return (1);
		}
	}

	if (fprintf (fptr, ".cend\n") < 0)
	{
		fclose (fptr);
		printf ("error: writing file %s !\n", asmname);
		return (1);
	}

	fclose (fptr);
	return (0);
}

void cleanup (void)
{
	if (code)
	{
		dealloc_array_U1 (code, line_len);
	}

	if (data)
	{
		dealloc_array_U1 (data, line_len);
	}
}

void show_info (void)
{
	printf ("l1com <file> [-a] [-lines] [max linenumber]\n");
	printf ("\nCompiler for bra(ets, a programming language with brackets ;-)\n");
	printf ("%s", VM_VERSION_STR);
	printf ("%s\n", COPYRIGHT_STR);
}

int main (int ac, char *av[])
{
	init_ast ();
	init_if ();
	init_while ();
	init_for ();
	init_switch ();

	init_labels ();
	init_call_labels ();

	U1 syscallstr[256] = "l1asm ";		// system syscall for assembler
	S8 ret ALIGN = 0;					// return value of assembler
	S8 arglen ALIGN;
	S8 i ALIGN;
	S8 str_len_arg ALIGN;
	S8 str_len_assembler_args ALIGN;

	U1 assembler_args[MAXLINELEN];

    if (ac < 2)
    {
		show_info ();
        exit (1);
    }

	// set assembler_args
	strcpy ((char *) assembler_args, " ");

	if (ac > 1)
	{
		for (i = 1; i < ac; i++)
		{
			arglen = strlen_safe (av[i], MAXLINELEN);

			if (arglen == 5)
			{
				if (strcmp (av[i], "-pack") == 0)
				{
					str_len_assembler_args = strlen ((const char *) assembler_args);
					if (str_len_assembler_args < MAXLINELEN - 7)
					{
						strcat ((char *) assembler_args, " -pack ");
					}
				}
			}
			if (arglen == 6)
			{
				if (strcmp (av[i], "--help") == 0 || strcmp (av[1], "-?") == 0)
				{
					show_info ();
					exit (1);
				}

				if (strcmp (av[i], "-lines") == 0)
				{
					if (i < ac - 1)
	                {
						line_len = atoi (av[i + 1]);
						printf ("max line len set to: %lli lines\n", line_len);
					}
				}

				if (strcmp (av[i], "-sizes") == 0)
				{
					if (ac > i + 1)
					{
						strcat ((char *) assembler_args, "-sizes ");
						str_len_arg = strlen (av[i + 1]);
						if (str_len_arg < MAXLINELEN - 7)
						{
							strcat ((char *) assembler_args, av[i + 1]);
							strcat ((char *) assembler_args, " ");
							str_len_assembler_args = strlen ((const char *) assembler_args);
							if (str_len_assembler_args < MAXLINELEN - 1)
							{
								str_len_arg = strlen (av[i + 2]);
								if (str_len_arg + str_len_assembler_args < MAXLINELEN - 1)
								{
									strcat ((char *) assembler_args, av[i + 2]);
									strcat ((char *) assembler_args, " ");
								}
							}
						}
					}
				}
			}
		}
	}

	data = alloc_array_U1 (line_len, MAXLINELEN);
	if (data == NULL)
	{
		printf ("\033[31merror: can't allocate %lli lines for data!\n", line_len);
		printf ("[!] %s\033[0m\n\n", av[1]);
		cleanup ();
		exit (1);
	}

	code = alloc_array_U1 (line_len, MAXLINELEN);
	if (code == NULL)
	{
		printf ("\033[31merror: can't allocate %lli lines for code!\n", line_len);
		printf ("[!] %s\033[0m\n\n", av[1]);
		cleanup ();
		exit (1);
	}

	// switch to red text
	printf ("\033[31m");

    if (parse ((U1 *) av[1]) == 1)
	{
		printf ("\033[31mERRORS! can't read source file!\n");
		printf ("[!] %s\033[0m\n\n", av[1]);
		cleanup ();
		exit (1);
	}

	if (check_labels () != 0)
	{
		printf ("\033[31mERRORS! can't find some labels!!\n");
		printf ("[!] %s\033[0m\n\n", av[1]);
		cleanup ();
		exit (1);
	}

	if (write_asm ((U1 *) av[1]) == 1)
	{
		printf ("\033[31mERRORS! can't write assembly file!\n");
		printf ("[!] %s\033[0m\n\n", av[1]);
		cleanup ();
		exit (1);
	}

	cleanup ();
	printf ("\033[0m[\u2714] %s compiled\n", av[1]);

	// run assembler
	strcat ((char *) syscallstr, av[1]);
	printf ("assembler args: '%s'\n", assembler_args);
	strcat ((char *) syscallstr, (const char *) assembler_args);
	ret = system ((const char *) syscallstr);

	exit (ret);
}
