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

//  l1comp RISC compiler
//

#include "../include/global.h"
#include "../include/opcodes.h"
#include "translate.h"

S8 linenum ALIGN = 1;

S8 label_ind ALIGN = -1;

struct ast
{
	U1 expr[MAXEXPRESSION][MAXARGS][MAXLINELEN];
	S4 expr_max;
	S4 expr_args[MAXEXPRESSION]; 						// number of arguments in expression
	S4 expr_reg[MAXEXPRESSION];							// registers of expression calculations = target registers
	U1 expr_type[MAXEXPRESSION];						// type of register (INTEGER or DOUBLE)
};

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

U1 inline_asm = 0;		// set to one, if inline assembly is used
U1 comp_aot = 0;        // set to one, if AOT COMPILE code block

U1 optimize_if = 0;		// set to one to optimize if call


// protos
U1 checkdigit (U1 *str);
S8 get_temp_int (void);
F8 get_temp_double (void);
char *fgets_uni (char *str, int len, FILE *fptr);

// mem.c
void dealloc_array_U1 (U1** array, size_t n_rows);
U1** alloc_array_U1 (size_t n_rows, size_t n_columns);

// if
void init_if (void);
S4 get_if_pos (void);
S4 get_act_if (void);
U1 get_if_label (S4 ind, U1 *label);
U1 get_else_label (S4 ind, U1 *label);
U1 get_endif_label (S4 ind, U1 *label);
void set_endif_finished (S4 ind);

void init_while (void);
S4 get_while_pos (void);
S4 get_act_while (void);
U1 get_while_label (S4 ind, U1 *label);
S4 get_while_lab (S4 ind);
void set_wend (S4 ind);

void init_for (void);
S4 get_for_pos (void);
S4 get_act_for (void);
U1 get_for_label (S4 ind, U1 *label);
U1 get_for_label_2 (S4 ind, U1 *label);
S4 get_for_lab (S4 ind);
U1 get_for_label_end (S4 ind, U1 *label);
void set_for_end (S4 ind);

// string
size_t strlen_safe (const char * str, int maxlen);


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

// if optimize helper function ==========================================================

S4 get_if_optimize_reg (U1 *code_line)
{
	S4 pos, i, j, str_len;
	U1 if_found = 0;
	U1 reg_num[256];
	S4 reg;

	str_len = strlen_safe ((const char *) code_line, MAXLINELEN);
	for (i = EQI; i <= LSEQD; i++)
	{
		pos = searchstr (code_line, opcode[i].op, 0, 0, TRUE);
		if (pos != -1)
		{
			if_found = 1;
			break;
		}
	}
	if (if_found == 0)
	{
		// no if found, return -1
		return (-1);
	}

	// get last argument, it's the needed register number
	pos = searchstr (code_line, (unsigned char *) ",", 0, 0, TRUE);
	if (pos != -1)
	{
		pos = searchstr (code_line, (unsigned char *) ",", pos + 1, 0, TRUE);
		j = 0;
		for (i = pos + 1; i < str_len; i++)
		{
			reg_num[j] = code_line[i];
			j++;
		}
		reg_num[j] = '\0';
		reg = atoi ((const char *) reg_num);

		return (reg);
	}
	else
	{
		return (-1);
	}
}


// register tracking functions
void set_regi (S4 reg, U1 *name)
{
	strcpy ((char *) regi[reg], (const char *) name);
}

void set_regd (S4 reg, U1 *name)
{
	strcpy ((char *) regd[reg], (const char *) name);
}

S4 get_regi (U1 *name)
{
	S4 i;

	for (i = 0; i < MAXREG; i++)
	{
		if (strcmp ((const char *) regi[i], (const char *) name) == 0)
		{
			return (i);
		}
	}
	return (-1);
}

S4 get_regd (U1 *name)
{
	S4 i;

	for (i = 0; i < MAXREG; i++)
	{
		if (strcmp ((const char *) regd[i], (const char *) name) == 0)
		{
			return (i);
		}
	}
	return (-1);
}

void init_registers (void)
{
	S4 i;

	for (i = 0; i < MAXREG; i++)
	{
		set_regi (i, (U1 *) "");
		set_regd (i, (U1 *) "");
	}
}

S4 get_free_regi (void)
{
	S4 i;

	for (i = 1; i < MAXREG; i++)
	{
		if (strcmp ((const char *) regi[i], "") == 0)
		{
			return (i);
		}
	}

	return (-1);
}

S4 get_free_regd (void)
{
	S4 i;

	for (i = 1; i < MAXREG; i++)
	{
		if (strcmp ((const char *) regd[i], "") == 0)
		{
			return (i);
		}
	}

	return (-1);
}

S2 checkdef (U1 *name)
{
	S4 i;

	if (name[0] == ':')
	{
		// label: set checked
		return (0);
	}

	if (checkdigit (name) == TRUE)
	{
		// is number not variable name: set checked
		return (0);
	}

	for (i = 0; i <= data_ind; i++)
	{
		if (strcmp ((const char *) name, (const char *) data_info[i].name) == 0)
		{
			// no error
			return (0);
		}
	}
	// error -> variable not defined!!
	printf ("checkdef: error: line %lli: variable '%s' not defined!\n", linenum, name);
	return (1);
}

S2 getvartype (U1 *name)
{
	S4 i;
	S4 ret = -1;

	if (name[0] == ':')
	{
		// it is a label, beginning with a colon char
		return (LABEL);
	}

	for (i = 0; i <= data_ind; i++)
	{
		if (strcmp ((const char *) name, (const char *) data_info[i].name) == 0)
		{
			switch (data_info[i].type)
			{
				case BYTE:
				case WORD:
				case DOUBLEWORD:
				case QUADWORD:
					ret = INTEGER;
					break;

				case DOUBLEFLOAT:
					ret = DOUBLE;
					break;

				case STRING:
					ret = STRING;
			}
			break;
		}
	}

	return (ret);
}

S2 getvartype_real (U1 *name)
{
	S4 i;
	S4 ret = -1;

	if (name[0] == ':')
	{
		// it is a label, beginning with a colon char
		return (LABEL);
	}

	for (i = 0; i <= data_ind; i++)
	{
		if (strcmp ((const char *) name, (const char *) data_info[i].name) == 0)
		{
			switch (data_info[i].type)
			{
				case BYTE:
					ret = BYTE;
					break;

				case WORD:
					ret = WORD;
					break;

				case DOUBLEWORD:
					ret = DOUBLEWORD;
					break;

				case QUADWORD:
					ret = QUADWORD;
					break;

				case DOUBLEFLOAT:
					ret = DOUBLE;
					break;

				case STRING:
					ret = STRING;
			}
			break;
		}
	}

	return (ret);
}

S8 get_variable_is_array (U1 *name)
{
	S4 i;

	for (i = 0; i <= data_ind; i++)
	{
		if (strcmp ((const char *) name, (const char *) data_info[i].name) == 0)
		{
			// found array, return aray size
			// printf ("get_variable_is_array: '%s' size: %lli\n", name, data_info[i].size);

			return (data_info[i].size);
		}
	}
	// error!!!
	return (0);
}


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

S2 get_ast (U1 *line)
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

	S4 i;
	for (i = 0; i < MAXBRACKETLEVEL; i++)
	{
		ast[i].expr_max = 0;
	}

	// printf ("> '%s'\n", line);

	// printf ("> %s", line);
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
			if (line[pos] == '(')
			{
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
						if (line[pos] != ' ' && line[pos] != ',' && line[pos] != '\n' && line[pos] != ')')
						{
							ast[ast_ind].expr[exp_ind][arg_ind][arg_pos] = line[pos];
							arg_pos++;
							if (arg_pos >= MAXLINELEN)
							{
								printf ("error: line %lli: argument too long!\n", linenum);
								return (1);
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
							ast[ast_ind].expr[exp_ind][arg_ind][arg_pos] = '\0';
							// printf ("[ %s ]\n", ast[ast_ind].expr[exp_ind][arg_ind]);

							if (line[pos] == ')')
							{
									// next char is open bracket, new expression next
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

S2 parse_line (U1 *line, S2 start, S2 end)
{
    S4 level, j, last_arg, last_arg_2, t, v, reg, reg2, reg3, target, e, exp;
	U1 ok;
	S8 i ALIGN;
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

	U1 set_loadreg = 0;

	if (get_ast (line) != 0)
	{
		// parsing error
		return (1);
	}

	/*
	struct ast
	{
		U1 expr[MAXEXPRESSION][MAXARGS][MAXLINELEN];
		S4 expr_max;
		S4 expr_args[MAXEXPRESSION]; 						// number of arguments in expression
	};
	*/

	// walking the AST
	for (level = ast_level; level >= 0; level--)
	{
		// printf ("level: %i, expr max: %i\n", level, ast[level].expr_max);
		if (ast[level].expr_max > -1)
		{
			for (j = 0; j <= ast[level].expr_max; j++)
			{
				// printf ("ast level %i:  j: %i, expression args: %i\n", level, j, ast[level].expr_args[j]);
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

						if (strcmp ((const char *) ast[level].expr[j][1], "byte") == 0)
						{
							data_info[data_ind].type = BYTE;
							data_info[data_ind].type_size = sizeof (U1);
							strcpy ((char *) data_info[data_ind].type_str, "B");
							ok = 1;
						}

						if (strcmp ((const char *) ast[level].expr[j][1], "int16") == 0)
						{
							data_info[data_ind].type = WORD;
							data_info[data_ind].type_size = sizeof (S2);
							strcpy ((char *) data_info[data_ind].type_str, "W");
							ok = 1;
						}

						if (strcmp ((const char *) ast[level].expr[j][1], "int32") == 0)
						{
							data_info[data_ind].type = DOUBLEWORD;
							data_info[data_ind].type_size = sizeof (S4);
							strcpy ((char *) data_info[data_ind].type_str, "D");
							ok = 1;
						}

						if (strcmp ((const char *) ast[level].expr[j][1], "int64") == 0)
						{
							data_info[data_ind].type = QUADWORD;
							data_info[data_ind].type_size = sizeof (S8);
							strcpy ((char *) data_info[data_ind].type_str, "Q");
							ok = 1;
						}

						if (strcmp ((const char *) ast[level].expr[j][1], "double") == 0)
						{
							data_info[data_ind].type = DOUBLEFLOAT;
							data_info[data_ind].type_size = sizeof (F8);
							strcpy ((char *) data_info[data_ind].type_str, "F");
							ok = 1;
						}

						if (strcmp ((const char *) ast[level].expr[j][1], "string") == 0)
						{
                            // printf ("DEBUG: string data type!\n");

							data_info[data_ind].type = STRING;
							data_info[data_ind].type_size = sizeof (U1);
							strcpy ((char *) data_info[data_ind].type_str, "B");
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

						// printf ("DEBUG: data size: %lli\n", data_info[data_ind].size);

						// Set real string size, if 0 length was set or "s" was set.
						if ((data_info[data_ind].size == 0 || ast[level].expr[j][2][0] == 's') && data_info[data_ind].type == STRING)
						{
							// set string size automatically:
							data_info[data_ind].size = strlen_safe ((char *) ast[level].expr[j][4], MAXLINELEN) - 1;
							// set size string:
							sprintf ((char *) ast[level].expr[j][2], "%lli", data_info[data_ind].size);
							printf ("SET: '%s' size to %lli\n", data_info[data_ind].name, data_info[data_ind].size);
						}

						// value
						if (ast[level].expr_args[j] >= 4)
						{
							// printf ("DEBUG: assign value...\n");

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
								// string

								// printf ("DEBUG: data string: '%s'\n", ast[level].expr[j][4]);

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
								strcat ((char *) data[data_line], ", ;");
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

							// printf ("data_line: ast expr args: %i", ast[level].expr_args[j]);

							if (ast[level].expr_args[j] > 4 && data_info[data_ind].type != STRING)
							{
								for (i = 5; i <= ast[level].expr_args[j]; i++)
								{
									if ((data_info[data_ind].type >= BYTE && data_info[data_ind].type <= QUADWORD) || data_info[data_ind].type == DOUBLEFLOAT)
									{
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
								strcat ((char *) data[data_line], ", ;");
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
											// printf ("DEBUG: assign array variable to variable.\n");

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

									if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
									{
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

											// printf ("assign value expression\n");

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


												// set_regi (target, ast[level].expr[j][last_arg - 1]);
												// set_regi (reg, (U1 *) ast[level].expr[j][last_arg - 1]);
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
															// set_regi (reg2, ast[level].expr[j][e]); BUG???
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
															// set_regi (reg2, ast[level].expr[j][e]); BUG???
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

											// printf ("%s\n", code_temp);

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

											// set_regi (target, ast[level].expr[j][last_arg - 1]);
											// set_regi (reg, (U1 *) ast[level].expr[j][last_arg - 1]);
											set_regi (reg, (U1 *) "");
											continue;
										}
									}

									if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
									{
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
												/// strcat ((char *) code[code_line], (const char *) ast[level].expr[j][e]); BUG???
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

										// printf ("%s\n", code_temp);

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

										// set_regd (target, ast[level].expr[j][last_arg - 1]);
										// set_regd (reg, (U1 *) ast[level].expr[j][last_arg - 1]);
										set_regd (reg, (U1 *) "");
										continue;
									}

									ok = 1;
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
									// printf ("intr0/intr1 || pushb/pullb expression...\n ");

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
											// printf ("get_regi: %s\n", ast[level].expr[j][e]);

											reg = get_regi (ast[level].expr[j][e]);
											// printf ("reg: %i\n", reg);
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
											// printf ("get string: %s\n", ast[level].expr[j][e]);

											strcpy ((char *) str, (const char *) ast[level].expr[j][e]);
											strcat ((char *) str, "addr");

											reg = get_regi (str);
											// printf ("reg: %i\n", reg);
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
											//printf ("intr0: %i,: '%s'\n", e, ast[level].expr[j][e]);

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
											strcpy ((char *) code_temp, "stpushi I");
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
											strcpy ((char *) code_temp, "stpushd F");
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
									// load double registers
									for (e = MAXREG - 1; e >= 0; e--)
									{
										if (save_regd[e] == 1)
										{
											strcpy ((char *) code_temp, "stpopd F");
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
											strcpy ((char *) code_temp, "stpopi I");
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

									init_registers ();

									continue;
								}

								// ((x y <) f =)
								// (f if)

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
												// printf ("reg: %i\n", reg);
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

												// printf ("IF OPTIMIZED found reg: %lli\n", reg);

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
												// printf ("reg: %i\n", reg);
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

												// printf ("IF OPTIMIZED found reg: %lli\n", reg);

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

								if ( strcmp ((const char *) ast[level].expr[j][last_arg], "else") == 0)
								{
									if_pos = get_act_if ();
									if (if_pos == -1)
									{
										printf ("error: line %lli: else: if not set!\n", linenum);
										return (1);
									}

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
											// printf ("reg: %i\n", reg);
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
											// printf ("reg: %i\n", reg);
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
										printf ("compile: error: while: out of memory for-list\n");
										return (FALSE);
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
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "call") == 0)
								{
									// function call with arguments

									// first of all use save register code

									// save integer registers
									for (e = 0; e < MAXREG; e++)
									{
										if (strcmp ((const char *) regi[e], "") != 0)
										{
											strcpy ((char *) code_temp, "stpushi I");
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
											strcpy ((char *) code_temp, "stpushd F");
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

												// printf ("get_regi: %s\n", ast[level].expr[j][e]);

												reg = get_regi (ast[level].expr[j][e]);
												// printf ("reg: %i\n", reg);
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
											if (getvartype (ast[level].expr[j][e]) == DOUBLE)
											{
												strcpy ((char *) code_temp, "stpushd ");

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

												// printf ("get string: %s\n", ast[level].expr[j][e]);

												strcpy ((char *) str, (const char *) ast[level].expr[j][e]);
												strcat ((char *) str, "addr");

												reg = get_regi (str);
												// printf ("reg: %i\n", reg);
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

											if (set_loadreg == 1)
											{
												// load double registers
												for (e = MAXREG - 1; e >= 0; e--)
												{
													if (save_regd[e] == 1)
													{
														strcpy ((char *) code_temp, "stpopd F");
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
														strcpy ((char *) code_temp, "stpopi I");
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

												init_registers ();

												set_loadreg = 0;
											}
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
							}
							else
							{
								printf ("error: line %lli: unknown opcode!\n", linenum);
								return (1);
							}

							// printf ("DEBUG: code_temp opcode: '%s'\n", code_temp);

							if (level < ast_level)
							{
								// printf ("level < ast_level\n");
								// printf ("use previous registers\n");

								e = level + 1;
								// printf ("level: %i\n", e);
								int found_exp = 0;
								int found_level = 0;
								int target_reg = 0;

								for (exp = 0; exp <= ast[e].expr_max; exp = exp + 2)
								{
									// if (ast[e].expr_reg[ast[e].expr_max] != -1)
									//{
									// printf ("ast exp reg: %i\n", ast[e - 1].expr_reg[exp]);

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

									// printf ("DEBUG: expression: '%s'\n", code[code_line]);

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

									// printf ("%s\n", code_temp);

									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									strcpy ((char *) code[code_line], (const char *) code_temp);

									strcpy ((char *) code_temp, "pullqw ");
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

									// printf ("%s\n", code_temp);

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
							}

							for (v = 0; v < last_arg_2; v++)
							{
								if (checkdef (ast[level].expr[j][v]) != 0)
								{
									return (1);
								}
								if (getvartype (ast[level].expr[j][v]) == INTEGER)
								{
									// printf ("get_regi: %s\n", ast[level].expr[j][v]);

									reg = get_regi (ast[level].expr[j][v]);
									// printf ("reg: %i\n", reg);
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

                                            // if (translate[t].assemb_op != STPOPB && translate[t].assemb_op != STPOPI)
                                            // {
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
                                            //}

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

                                           // if (translate[t].assemb_op != STPOPD)
                                            //{
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
										// }
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

								// printf ("%s\n", code_temp);
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

							// printf ("%s", code[code_line]);
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
            convtabs (rbuf);                    /* convert the funny tabs into spaces! */
            slen = strlen_safe ((const char *) rbuf, MAXLINELEN);

			// printf ("[ %s ]\n", rbuf);

            pos = searchstr (rbuf, (U1 *) REM_SB, 0, 0, TRUE);
            if (pos == -1)
            {
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

				pos = searchstr (rbuf, (U1 *) COMP_AOT_SB, 0, 0, TRUE);
				if (pos != -1)
				{
					comp_aot = 1;
					continue;
				}

				pos = searchstr (rbuf, (U1 *) COMP_AOT_END_SB, 0, 0, TRUE);
				if (pos != -1)
				{
					comp_aot = 0;
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
					ret = parse_line (rbuf, 0, slen - 1);
					if (ret != 0)
					{
						printf ("> %s", rbuf);
						err = 1;

						if (ret == 2)
						{
							// ERROR brackets don't match
							fclose (fptr);
							return (ret);
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

int main (int ac, char *av[])
{
    printf ("l1com <file> [-lines] [max linenumber]\n");
	printf ("\nCompiler for bra(et, a programming language with brackets ;-)\n");
	printf ("0.9.9 (C) 2017-2019 Stefan Pietzonke\n");

	init_ast ();
	init_if ();
	init_while ();
	init_for ();

    if (ac < 2)
    {
        exit (1);
    }

	if (ac == 4)
	{
		if (strcmp (av[2], "-lines") == 0)
		{
			line_len = atoi (av[3]);
			printf ("max line len set to: %lli lines\n", line_len);
		}
	}

	data = alloc_array_U1 (line_len, MAXLINELEN);
	if (data == NULL)
	{
		printf ("error: can't allocate %lli lines for data!\n", line_len);
		cleanup ();
		exit (1);
	}

	code = alloc_array_U1 (line_len, MAXLINELEN);
	if (code == NULL)
	{
		printf ("error: can't allocate %lli lines for code!\n", line_len);
		cleanup ();
		exit (1);
	}

    if (parse ((U1 *) av[1]) == 1)
	{
		printf ("ERRORS! can't read source file!\n");
		cleanup ();
		exit (1);
	}

	if (write_asm ((U1 *) av[1]) == 1)
	{
		printf ("ERRORS! can't write assembly file!\n");
		cleanup ();
		exit (1);
	}

	cleanup ();
	printf ("ok!\n");
	exit (0);
}
