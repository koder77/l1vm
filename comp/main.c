/*
 * This file main.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (info@midnight-coding.de), 2017
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
// line 1694: assign to normal variable code

#include "../include/global.h"
#include "../include/opcodes.h"
#include "translate.h"
#include "main.h"
#include "if.h"

// protos
S8 get_ranges_index (U1 *varname);

//string.c
S2 check_spaces (U1 *line);

S8 linenum ALIGN = 0;

// for included files
#define FILENAME_START_SB   "FILE:"
#define FILENAME_END_SB     "FILE END"

#define FILES_MAX           1000

struct file
{
	S8 linenum ALIGN;
	U1 name[MAXSTRLEN];
};

struct file files[FILES_MAX];
S8 file_index ALIGN = 0;
U1 file_inside = 0;

S8 label_ind ALIGN = -1;
S8 call_label_ind ALIGN = -1;

struct ast ast[MAXBRACKETLEVEL];
S8 ast_level ALIGN = 0;

S8 data_ind ALIGN = -1;

// variable ranges set
struct range ranges[MAXRANGESVAR];
S8 ranges_ind ALIGN = -1;

// assembly text output
U1 **data = NULL;
U1 **code = NULL;

S8 line_len ALIGN = MAXLINELEN;
S8 code_max_lines ALIGN = CODEMAXLINES;

S8 data_line ALIGN = 0;
S8 code_line ALIGN = 0;

U1 regi[MAXREG][MAXSTRLEN];
U1 regd[MAXREG][MAXSTRLEN];

U1 save_regi[MAXREG];
U1 save_regd[MAXREG];

struct t_var t_var;
struct data_info data_info[MAXDATAINFO];
struct data_info_var data_info_var[MAXDATAINFO];
struct label label[MAXLABELS];
struct call_label call_label[MAXLABELS];

// globals for local variable ending check only allow local and global (main) ending if set!
U1 check_varname_end = 0;

// globals for local variable ending check only allow local variable if set!
U1 check_varname_end_local_only = 0;

U1 varname_end[MAXLINELEN];

U1 inline_asm = 0;		// set to one, if inline assembly is used

U1 optimize_if = 0;		// set to one to optimize if call

// automatic set loadreg if no more stpopi or stpopd opcode is used
U1 call_set = 0;
U1 call_loadreg = 0;

// set all warnings as errors if 1
U1 warnings_as_errors = 0;

// warn if set does'nt set a value on variable definiton if 1
U1 warn_set_not_def = 0;

// don't use pull opcodes to optimize chained math expressions in "parse-rpolish.c" math expressions
U1 no_var_pull = 0;

// Set to 1 at function start. It calls reset_registers () on: if, else, endif, switch ?, for, next, do, while
U1 nested_code = 1;

// set to 1 if warning for defined but unused variables should be set
U1 warn_unused_variables = 0;

U1 error_multi_spaces = 0;

// set to 1 if inside object functions
U1 inside_object = 0;
// current object name
U1 object_name[MAXSTRLEN];

// set immutable variables flag
U1 var_immutable = 0;

// set pure function flag
// if set only pure functions with no side effects allowed to call!
U1 pure_function = 0;

// if set to 1: switch pure_function off
U1 pure_function_override = 0;

// unsafe block
U1 inside_unsafe = 0;

// forbid unsafe flag
U1 forbid_unsafe = 0;

// memory boúnds on/off
U1 memory_bounds = 1;

// emit compiler code into assembly file as comments
U1 save_compiler_line = 0;

// contracts
U1 contracts = 0;
U1 precondition = 0;
S8 precondition_code ALIGN = 0;
U1 precondition_end = 0;
U1 postcondition = 0;
S8 postcondition_code ALIGN = 0;
U1 postcondition_end = 0;


// set if linter is needed by
// LINTER
// in Brackets code!
U1 do_check_linter = 0;

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
			if (line[i] != '(' && line[i] != ')' && line[i] != ' ')
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

S2 check_pure_function (U1 *function_name)
{
	S2 slen;
	S2 i;
	S2 obj_start_pos;

	slen = strlen_safe ((const char *) function_name, MAXLINELEN);
	if (slen <= 2)
	{
		// no pure function possible name
		return (1);
	}

	if (function_name[slen - 1] == 'P')
	{
		// got something like: foobarP -> function marked as pure
		return (0);
	}

	obj_start_pos = searchstr (function_name, (U1 *) "->", 0, 0, TRUE);
	if (obj_start_pos >= 2)
	{
	    if (function_name[obj_start_pos - 1] == 'P')
		{
			// got something like: fooP->bar -> object function marked as pure
			return (0);
		}
	}

	// function not marked as pure
	return (1);
}

void repair_multi_spaces (U1 *line)
{
	S4 line_len, line_ind = 0, outbuf_ind = 0;
	U1 ok = 0;
	U1 quote = 0;
	U1 space = 0;
	U1 ch;
	U1 outbuf[MAXLINELEN];
	line_len = strlen_safe ((const char *) line, MAXSTRLEN);
    if (line_len == 0)
	{
	    return;
    }

	//printf ("repair_multi_spaces: '%s'\n", line);

	//        länge 8 zeichen
	//12345678
	//01234567
    while (ok == 0)
	{
	    if (line[line_ind] == '"')
		{
			outbuf[outbuf_ind] = line[line_ind];
			if (line_ind < line_len)
			{
				line_ind++;
			}
			if (outbuf_ind < line_len)
			{
				outbuf_ind++;
			}
			while (quote == 0)
			{
				if (line[line_ind] != '"')
				{
					outbuf[outbuf_ind] = line[line_ind];
				}
				else
				{
					// found end of set string:
					outbuf[outbuf_ind] = line[line_ind];
					if (outbuf_ind < line_len)
					{
						outbuf_ind++;
					}
					outbuf[outbuf_ind] = ')';
					if (outbuf_ind < line_len)
					{
						outbuf_ind++;
					}
					outbuf[outbuf_ind] = '\0';
					quote = 1;
					ok = 1;
					continue;
				}
				if (line_ind < line_len)
				{
					line_ind++;
				}
				if (outbuf_ind < line_len)
				{
					outbuf_ind++;
				}
			}
		}
		else
		{
			if (line[line_ind] != ' ')
			{
				outbuf[outbuf_ind] = line[line_ind];
				if (line_ind < line_len)
				{
					line_ind++;
				}
				if (outbuf_ind < line_len)
				{
					outbuf_ind++;
				}
			}
			else
			{
				space = 0;
				outbuf[outbuf_ind] = line[line_ind];
				if (outbuf_ind < line_len)
				{
					outbuf_ind++;
				}

				while (space == 0)
				{
					if (line_ind < line_len)
					{
						line_ind++;
					}
					if (line[line_ind] != ' ')
					{
						space = 1;
					}
				}
			}
		}
		if (line_ind == line_len)
		{
			ok = 1;
		}
	}
	outbuf[outbuf_ind] = '\0';
	strcpy ((char *) line, (const char *) outbuf);
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
		// brackets don't match!
		return (2);
	}

	// DEBUG
	//printf ("get_ast: > '%s'\n", line);

	if (check_old_syntax_symbols (line) == 1)
	{
		// DEBUG
		 //printf ("get_ast: found (a + b c =) expression!\n");
		*parse_cont = 1;
		return (0);
	}

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
					//printf ("DEBUG get_ast: parse_cont return\n");
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
							// do check of all index variables!!!
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
								// brackets don't match!
								return (2);
							}
							// printf ("ast_ind: %i, exp_ind: %i, arg_ind: %i, arg_pos: %i\n", ast_ind, exp_ind, arg_ind, arg_pos);

							// BUGFIX -> avoid write byte error in valgrind.
							// do check of all index variables!!!
							if (ast_ind >= 0 && exp_ind >= 0 && arg_ind >= 0 && arg_pos >= 0)
							{
								ast[ast_ind].expr[exp_ind][arg_ind][arg_pos] = '\0';
								// printf ("[ %s ]\n", ast[ast_ind].expr[exp_ind][arg_ind]);
							}

							if (line[pos] == ')' || line[pos] == '}')
							{
								// next char is open bracket, new expression next
								// BUGFIX -> avoid write byte error in valgrind.
								// do check of all index variables!!!
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
		// brackets don't match!
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

S8 loadreg (void)
{
	S8 e;
	U1 str[MAXSTRLEN];
	U1 code_temp[MAXSTRLEN];

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
            if (code_line >= code_max_lines)
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
            if (code_line >= code_max_lines)
            {
                printf ("error: line %lli: code list full!\n", linenum);
                return (3);
            }

            strcpy ((char *) code[code_line], (const char *) code_temp);
        }
    }

    init_registers ();
	return (0);
}

S2 check_for_whitespace (U1 *line)
{
	S8 str_len ALIGN;
	S8 i ALIGN;
	U1 space = ' ';
	U1 tab = 8;
	U1 ret = 13;
	U1 newline = 10;	

	str_len = strlen_safe ((const char *) line, MAXLINELEN);
	if (str_len > 0)
	{
		for (i = 0; i < str_len; i++)
		{
			// printf ("check_for_whitespace: char num: %i, ascii code: %i\n", i, line[i]);
			if (line[i] != space && line[i] != tab && line[i] != ret && line[i] != newline)
			{
				// ERROR no whitespace char found
				return (1);
			}
		}
	}
	// all OK!
	return (0);
}


S2 check_for_brackets (U1 *line)
{
	S4 pos_bracket_start, pos_bracket_end;
	S4 pos_dbracket_start, pos_dbracket_end;
	U1 found_bracket = 0;
	U1 found_dbracket = 0;

	pos_dbracket_start = searchstr (line, (U1 *) "{", 0, 0, TRUE);
	if (pos_dbracket_start > -1)
	{
		pos_dbracket_end = searchstr (line, (U1 *) "}", 0, 0, TRUE);
		if (pos_dbracket_end > -1)
		{
			if (pos_dbracket_end > pos_dbracket_start)
			{
				found_dbracket = 1;
			}
		}
		else
		{
			// dbracket closing } not found
			printf ("error: no closing bracket } found!\n");
			return (1);
		}
	}

	pos_bracket_start = searchstr (line, (U1 *) "(", 0, 0, TRUE);
	if (pos_bracket_start > -1)
	{
		pos_bracket_end = searchstr (line, (U1 *) ")", 0, 0, TRUE);
		if (pos_bracket_end > -1)
		{
			if (pos_bracket_end > pos_bracket_start)
			{
				found_bracket = 1;
			}
		}
		else
		{
			// bracket closing ) not found
			printf ("error: no closing bracket ) found!\n");
			return (1);
		}
    }

	if (found_dbracket == 0 && found_bracket == 0)
	{
		if (check_for_whitespace (line) != 0)
		{
			printf ("error: no bracket found!\n");
			return (1);
		}
	}
	return (0);
}

S2 check_for_normal_brackets (U1 *line)
{
	S4 pos_bracket_start, pos_bracket_end;
	U1 found_bracket = 0;

	pos_bracket_start = searchstr (line, (U1 *) "(", 0, 0, TRUE);
	if (pos_bracket_start > -1)
	{
		pos_bracket_end = searchstr (line, (U1 *) ")", 0, 0, TRUE);
		if (pos_bracket_end > -1)
		{
			if (pos_bracket_end > pos_bracket_start)
			{
				found_bracket = 1;
			}
		}
		else
		{
			// bracket closing ) not found
			printf ("error: no closing bracket ) found!\n");
			return (2);
		}
    }

	if (found_bracket == 1)
	{
		// normal brackets found
		return (0);
	}
	else
	{
		// no normal brackets found
		return (1);
	}
}

S2 check_for_infix_math (U1 *line)
{
	U1 buf[MAXSTRLEN];
	S4 buf_ind = 0;
	S4 ind;
	S4 start = 0;
	S4 line_len;
	U1 found_var = 0;

	if (check_for_normal_brackets (line) == 0)
	{
		return (0);
	}

	line_len = strlen_safe ((const char *) line, MAXLINELEN);
	for (ind = 0; ind < line_len; ind++)
	{
		if (line[ind] == '=')
		{
			start = ind + 2;
			break;
		}
	}
	if (start == 0)
	{
		// error no equal sign found
		return (2);
	}

	for (ind = start; ind < line_len; ind++)
	{
		if (line[ind] != ' ' && line[ind] != '}')
		{
			//printf ("check_for_infix_math: char: %c\n", line[ind]);

			if (isOperator (line[ind]) == 1)
			{
				//printf ("check_for_infix_math: found operator: %c\n", line[ind]);
				//printf ("check_for_infix_math: found_var: %i\n", found_var);

				if (found_var == 1)
				{
					// infix math found
					//printf ("check_for_infix_math: found infix expression!\n");

					return (0);
				}
				found_var = 0;
			}
			else
			{
				buf[buf_ind] = line[ind];
				buf_ind++;
			}
		}
		else
		{
			buf[buf_ind] = '\0';
			buf_ind = 0;

			//printf ("check_for_infix_math: found var: %s\n", buf);

			found_var++;
		}
	}
	return (1);
}

S2 parse_line (U1 *line)
{
    S4 level = 0, j = 0, last_arg = 0, last_arg_2 = 0, t = 0, v = 0, reg = 0, reg2 = 0, reg3 = 0, reg4 = 0, target = 0, e = 0, exp = 0;
	U1 ok = 0;
	S8 i ALIGN = 0;

	// for convert brackets expression to RPN
	U1 conv[MAXSTRLEN] = "" ;
	U1 conv_right_assign[MAXSTRLEN] = "";

	U1 str[MAXSTRLEN] = "";
	U1 code_temp[MAXSTRLEN] = "";

	S8 if_pos ALIGN = 0;
	U1 if_label[MAXSTRLEN] = "";
	U1 else_label[MAXSTRLEN] = "";
	U1 endif_label[MAXSTRLEN] = "";

	S8 while_pos ALIGN = 0;
	U1 while_label[MAXSTRLEN] = "";

	S8 for_pos ALIGN = 0;
	U1 for_label[MAXSTRLEN] = "";

	S8 switch_pos ALIGN = 0;

	U1 set_loadreg = 0;

	// "=" equal expression track vars:
	S8 found_let ALIGN = 0;
	S8 found_let_cont ALIGN = 0;

	// returned by get_ast ()
	// to parse: {a = x + y * z} like code stuff!
	U1 parse_cont = 0;
	S2 ret = 0;

	// multi line array assign
	U1 array_multi = 0;

	// for variable assign type checks
	U1 target_var_type = UNKNOWN;
	U1 expression_var_type_max = UNKNOWN;
	S2 expression_if_op = -1;						// set to 0 if math operation found. set to 1 if on if operator

	U1 boolean_var = 0;
	S2 normal_brackets = 0;

	S2 right_assign_pos = 0;

    ret = check_for_brackets (line);
	if (ret == 1)
	{
		// error:
		printf ("error: line: %lli brackets error!\n", linenum);
		return (1);
	}

	ret = get_ast (line, &parse_cont);
	if (ret == 1)
	{
		// parsing error
		return (1);
	}
	if (ret == 2)
	{
		if (check_for_whitespace (line) == 1)
		{
			printf ("error: line %lli: brackets don't match!\n", linenum);
			return (2);
		}
		else 
		{
			// printf ("line %lli: blank line, ignored!\n", linenum);
			return (0);
		}
	}

	if (contracts == 1)
	{
		if (precondition == 1 && precondition_end == 0)
		{
			precondition_code++;
		}
		if (postcondition == 1 && postcondition_end == 0)
		{
			postcondition_code++;
		}
	}

	if (parse_cont)
	{
		//printf ("DEBUG parse_cont: '%s'\n", line);

		normal_brackets = check_for_infix_math (line);
		// DEBUG
		//printf ("normal_brackets: %i\n", normal_brackets);

		if (normal_brackets == 2)
		{
			// ERROR
			return (1);
		}
		if (normal_brackets == 0)
		{
			// found brackets in math expression, convert to RPN
			//
			// check if right assign:
			// {a + b + c x =}

			right_assign_pos = searchstr (line, (U1 *) "=)", 0, 0, TRUE);
			// DEBUG
			//printf ("right_assign_pos: %i\n", right_assign_pos);

			if (right_assign_pos != -1)
			{
				// convert string to left assign
				// DEBUG
				//printf ("found right assign expression!\n");

				if (convert_right_assign (line, conv_right_assign) != 0)
				{
					printf ("error: line: %lli can't convert right assign to left assign!\n", linenum);
					return (1);
				}
				strcpy ((char *) line, (const char *) conv_right_assign);

				if (convert (line, conv) == 1)
		     	{
				    printf ("error: line: %lli can't convert infix to RPN!\n", linenum);
				    return (1);
			    }
			}
			else
			{
		  	    if (convert (line, conv) == 1)
		     	{
				    printf ("error: line: %lli can't convert infix to RPN!\n", linenum);
				    return (1);
			    }
		    }

			// printf ("DEBUG: parse line: exp: '%s'\n", line);
			// printf ("DEBUG: parse line: RPN: '%s'\n", conv);

			if (parse_rpolish (conv) != 0)
			{
				return (1);
			}
			return (0);
		}
		else
		{
			if (parse_rpolish (line) != 0)
			{
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

						// boolean variable definition ===========================================
						if (strcmp ((const char *) ast[level].expr[j][1], "bool") == 0)
						{
							data_info[data_ind].type = BYTE;
							data_info[data_ind].type_size = sizeof (U1);
							strcpy ((char *) data_info[data_ind].type_str, "B");
							data_info[data_ind].constant = 0;
							boolean_var = 1;    // set to one, for definition check later
							ok = 1;
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

						if (var_immutable == 1)
						{
							// all normal variables are set as immutable by flag
							data_info[data_ind].constant = 1;
						}

						// const, constant variable definition ==================================
						if (strcmp ((const char *) ast[level].expr[j][1], "const-bool") == 0)
						{
							data_info[data_ind].type = BYTE;
							data_info[data_ind].type_size = sizeof (U1);
							strcpy ((char *) data_info[data_ind].type_str, "B");
							data_info[data_ind].constant = 1;
							boolean_var = 1;    // set to one, for definition check later
							ok = 1;
						}

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

						// mutable variable definition ==================================
						// for use if immutable var as default is set!
						if (strcmp ((const char *) ast[level].expr[j][1], "mut-bool") == 0)
						{
							data_info[data_ind].type = BYTE;
							data_info[data_ind].type_size = sizeof (U1);
							strcpy ((char *) data_info[data_ind].type_str, "B");
							data_info[data_ind].constant = 0;
							boolean_var = 1;    // set to one, for definition check later
							ok = 1;
						}

						if (strcmp ((const char *) ast[level].expr[j][1], "mut-byte") == 0)
						{
							data_info[data_ind].type = BYTE;
							data_info[data_ind].type_size = sizeof (U1);
							strcpy ((char *) data_info[data_ind].type_str, "B");
							data_info[data_ind].constant = 0;
							ok = 1;
						}

						if (strcmp ((const char *) ast[level].expr[j][1], "mut-int16") == 0)
						{
							data_info[data_ind].type = WORD;
							data_info[data_ind].type_size = sizeof (S2);
							strcpy ((char *) data_info[data_ind].type_str, "W");
							data_info[data_ind].constant = 0;
							ok = 1;
						}

						if (strcmp ((const char *) ast[level].expr[j][1], "mut-int32") == 0)
						{
							data_info[data_ind].type = DOUBLEWORD;
							data_info[data_ind].type_size = sizeof (S4);
							strcpy ((char *) data_info[data_ind].type_str, "D");
							data_info[data_ind].constant = 0;
							ok = 1;
						}

						if (strcmp ((const char *) ast[level].expr[j][1], "mut-int64") == 0)
						{
							data_info[data_ind].type = QUADWORD;
							data_info[data_ind].type_size = sizeof (S8);
							strcpy ((char *) data_info[data_ind].type_str, "Q");
							data_info[data_ind].constant = 0;
							ok = 1;
						}

						if (strcmp ((const char *) ast[level].expr[j][1], "mut-double") == 0)
						{
							data_info[data_ind].type = DOUBLEFLOAT;
							data_info[data_ind].type_size = sizeof (F8);
							strcpy ((char *) data_info[data_ind].type_str, "F");
							data_info[data_ind].constant = 0;
							ok = 1;
						}

						if (strcmp ((const char *) ast[level].expr[j][1], "mut-string") == 0)
						{
							data_info[data_ind].type = STRING;
							data_info[data_ind].type_size = sizeof (U1);
							strcpy ((char *) data_info[data_ind].type_str, "B");
							data_info[data_ind].constant = 0;
							ok = 1;
						}

						if (ok == 0)
						{
							printf ("error: line %lli: unknown data type!\n", linenum);
							return (1);
						}

						// name
						if (checkset (ast[level].expr[j][3]) == 0)
						{
							// ERROR variable already defined!
							printf ("error: line: %lli: variable '%s' already defined!\n", linenum, ast[level].expr[j][3]);
							return (1);
						}

						strcpy ((char *) data_info[data_ind].name, (const char *) ast[level].expr[j][3]);

						// do check if bool variable name begins with uppercase B
						if (boolean_var == 1)
						{
							if (data_info[data_ind].name[0] != 'B')
                            {
								printf ("error: line %lli: boolean variable name must begin with 'B' !\n", linenum);
								return (1);
							}
						}

						{
						// check size of variable, new variable scope

						// size
						if (checkdigit (ast[level].expr[j][2]) != 1)
						{
							if (data_info[data_ind].type != STRING)
							{
								printf ("error: line %lli: data size not a number!\n", linenum);
								return (1);
							}
						}

						if (t_var.digitstr_type == DOUBLEFLOAT)
						{
							// wrong type: must be integer number
							printf ("error: line %lli: variable %s size must be of integer type!\n", linenum, ast[level].expr[j][3]);
							return (1);
						}

						S8 value ALIGN;
						U1 *ptr;
						if (data_info[data_ind].type == STRING)
						{
							if (strcmp ((const char *) ast[level].expr[j][2], "s") != 0)
						    {
								// string size is not "s", to set automatically
								value = strtoll((const char *) ast[level].expr[j][2], (char **) &ptr, 10);

								if (value <= 0)
								{
									printf ("error: line %lli: variable %s size must be 1 or higher!\n", linenum, ast[level].expr[j][3]);
									return (1);
								}
							}
                        }
						else
						{
							value = strtoll((const char *) ast[level].expr[j][2], (char **) &ptr, 10);

							if (value <= 0)
							{
								printf ("error: line %lli: variable %s size must be 1 or higher!\n", linenum, ast[level].expr[j][3]);
								return (1);
							}
						}
						}  // end of scope

						data_info[data_ind].size = get_temp_int ();

						// Set real string size, if 0 length was set or "s" was set.
						if ((data_info[data_ind].size == 0 || ast[level].expr[j][2][0] == 's') && data_info[data_ind].type == STRING)
						{
							// set string size automatically:
							data_info[data_ind].size = strlen_safe ((char *) ast[level].expr[j][4], MAXLINELEN) - 1;
							// set size string:
							sprintf ((char *) ast[level].expr[j][2], "%lli", data_info[data_ind].size);
						}

						if (ast[level].expr_args[j] == 3)
						{
							// no value set on variable definition
							if (warn_set_not_def == 1)
							{
								if (warnings_as_errors == 1)
								{
									printf ("error: line %lli: variable %s has no value set!\n", linenum, ast[level].expr[j][3]);
									return (1);
								}
								else
								{
									printf ("warning: line %lli: variable %s has no value set!\n", linenum, ast[level].expr[j][3]);
								}
							}
						}

						// value
						if (ast[level].expr_args[j] >= 4)
						{
							if ((data_info[data_ind].type >= BYTE && data_info[data_ind].type <= QUADWORD) || data_info[data_ind].type == DOUBLEFLOAT)
							{
								if (checkdigit (ast[level].expr[j][4]) != 1)
								{
									// check if variable value matches type
									if (data_info[data_ind].type == DOUBLEFLOAT && t_var.digitstr_type != DOUBLEFLOAT)
									{
										printf ("error: line %lli: value not a double number!\n", linenum);
										return (1);
									}

									if (data_info[data_ind].type != DOUBLEFLOAT && (t_var.digitstr_type == DOUBLEFLOAT || t_var.digitstr_type == STRING))
									{
										printf ("error: line %lli: value not a integer number!\n", linenum);
										return (1);
									}

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
									{
										// check if variable value is in legal range
										S8 value ALIGN;
										U1 *ptr;
										value = strtoll((const char *) ast[level].expr[j][4], (char **) &ptr, 10);

										// check if variable value matches type
										if (data_info[data_ind].type == DOUBLEFLOAT && t_var.digitstr_type != DOUBLEFLOAT)
										{
											printf ("error: line %lli: value not a double number and not a variable!\n", linenum);
											return (1);
										}

										if (data_info[data_ind].type != DOUBLEFLOAT && (t_var.digitstr_type == DOUBLEFLOAT || t_var.digitstr_type == STRING))
										{
											printf ("error: line %lli: value not a integer number and not a variable!\n", linenum);
											return (1);
										}

										switch (data_info[data_ind].type)
										{
											case BYTE:
												if (value < 0 || value > UCHAR_MAX)
												{
													printf ("error: line %lli: byte value out of range!\n", linenum);
													return (1);
												}

												if (boolean_var == 1)
												{
													if (value != 0 && value != 1)
													{
														printf ("error: line %lli: boolean value out of range! Must be 0 = false or 1 = true!\n", linenum);
														return (1);
													}
												}
												break;

											case WORD:
												if (value <  SHRT_MIN || value > SHRT_MAX)
												{
													printf ("error: line %lli: int16 value out of range!\n", linenum);
													return (1);
												}
												break;

											case DOUBLEWORD:
												if (value < -2147483648 || value > 2147483647)
												{
													printf ("error: line %lli: int32 value out of range!\n", linenum);
													return (1);
												}
												break;

											case QUADWORD:
												if (value < LLONG_MIN  || value > LLONG_MAX)
												{
													printf ("error: line %lli: int64 value out of range!\n", linenum);
													return (1);
												}
												break;
										}

										strcpy ((char *) data_info[data_ind].value_str, (const char *) ast[level].expr[j][4]);
									}
								}
							}

							if (data_info[data_ind].type == STRING)
							{
								if ((S8) (strlen_safe ((const char *) ast[level].expr[j][4], MAXLINELEN) - 1)  > data_info[data_ind].size)
								{
									printf ("error: line %lli: string variable: '%s' too small for string!\n", linenum, ast[level].expr[j][4]);
									return (1);
								}

								strcpy ((char *) data_info[data_ind].value_str, (const char *) ast[level].expr[j][4]);
							}
						}

						if (data_line == 0)
						{
							// first data, set ".data" and start offset

							strcpy ((char *) data[data_line], ".data\n");
							data_line++;

							// EDIT
							if (data_line >= code_max_lines)
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
							if (data_line >= code_max_lines)
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
								S8 array_index ALIGN = 0;
								for (i = 5; i <= ast[level].expr_args[j]; i++)
								{
									if ((data_info[data_ind].type >= BYTE && data_info[data_ind].type <= QUADWORD) || data_info[data_ind].type == DOUBLEFLOAT)
									{
										array_index++;		// count assigned array variables
										if (ast[level].expr[j][i][0] == '/')
										{
											// found end of line mark of multi line array data
											array_multi = 1;
											continue;
										}
										if (checkdigit (ast[level].expr[j][i]) != 1)
										{
											//printf ("DEBUG: digitstr_type: %i, %s\n", t_var.digitstr_type, ast[level].expr[j][i]);

											// check if variable value matches type
											if (data_info[data_ind].type == DOUBLEFLOAT && getvartype (ast[level].expr[j][i]) != DOUBLE)
											{
												printf ("error: line %lli: value not a double number!\n", linenum);
												return (1);
											}

											if (data_info[data_ind].type != DOUBLEFLOAT && (getvartype (ast[level].expr[j][i]) == DOUBLE || getvartype (ast[level].expr[j][i]) == STRING))
											{
												printf ("error: line %lli: value not an integer number!\n", linenum);
												return (1);
											}

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
											//printf ("DEBUG: digitstr_type: %i, %s\n", t_var.digitstr_type, ast[level].expr[j][i]);

											// check if variable value matches type
											if (data_info[data_ind].type == DOUBLEFLOAT && t_var.digitstr_type != DOUBLEFLOAT)
											{
												printf ("error: line %lli: value not a double number and not a variable!\n", linenum);
												return (1);
											}

											if (data_info[data_ind].type != DOUBLEFLOAT && (t_var.digitstr_type == DOUBLEFLOAT || t_var.digitstr_type == STRING))
											{
												printf ("error: line %lli: value not a integer number and not a variable!\n", linenum);
												return (1);
											}

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
								if (array_index >= atoi ((const char *) ast[level].expr[j][2]))
								{
									// error: array variable overflow!
									printf ("error: line %lli: array variable overflow!\n", linenum);
									return (1);
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
							if (data_line >= code_max_lines)
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
							if (data_line >= code_max_lines)
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
								S8 array_index ALIGN = 0;
								for (i = 5; i <= ast[level].expr_args[j]; i++)
								{
									if ((data_info[data_ind].type >= BYTE && data_info[data_ind].type <= QUADWORD) || data_info[data_ind].type == DOUBLEFLOAT)
									{
										array_index++;		// count assigned array variables
										if (ast[level].expr[j][i][0] == '/')
										{
											// found end of line mark of multi line array data
											array_multi = 1;
											continue;
										}
										if (checkdigit (ast[level].expr[j][i]) != 1)
										{
											//printf ("DEBUG: digitstr_type: %i, %s\n", t_var.digitstr_type, ast[level].expr[j][i]);

											// check if variable value matches type
											if (data_info[data_ind].type == DOUBLEFLOAT && getvartype (ast[level].expr[j][i]) != DOUBLE)
											{
												printf ("error: line %lli: value not a double number!\n", linenum);
												return (1);
											}

											if (data_info[data_ind].type != DOUBLEFLOAT && (getvartype (ast[level].expr[j][i]) == DOUBLE || getvartype (ast[level].expr[j][i]) == STRING))
											{
												printf ("error: line %lli: value not an integer number!\n", linenum);
												return (1);
											}

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
											//printf ("DEBUG: digitstr_type: %i, %s\n", t_var.digitstr_type, ast[level].expr[j][i]);

											// check if variable value matches type
											if (data_info[data_ind].type == DOUBLEFLOAT && t_var.digitstr_type != DOUBLEFLOAT)
											{
												printf ("error: line %lli: value not a double number and not a variable!\n", linenum);
												return (1);
											}

											if (data_info[data_ind].type != DOUBLEFLOAT && (t_var.digitstr_type == DOUBLEFLOAT || t_var.digitstr_type == STRING))
											{
												printf ("error: line %lli: value not an integer number and not a variable!\n", linenum);
												return (1);
											}

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
								if (array_index >= atoi ((const char *) ast[level].expr[j][2]))
								{
									// error: array variable overflow!
									printf ("error: line %lli: array variable overflow!\n", linenum);
									return (1);
								}
							}

							strcat ((char *) data[data_line], "\n");

							if (data_info[data_ind].type == STRING)
							{
								data_line++;
								if (data_line >= code_max_lines)
								{
									printf ("error: line %lli: data list full!\n", linenum);
									return (1);
								}

								strcpy ((char *) data[data_line], "Q, 1, ");
								strcat ((char *) data[data_line], (const char *) data_info[data_ind].name);
								strcat ((char *) data[data_line], "addr\n");

								data_line++;
								if (data_line >= code_max_lines)
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
								if (data_line >= code_max_lines)
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
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "stpop") == 0)
							    {
									ok = 1;
								}
							}
							if (ok == 1)
							{
								if (get_var_is_const (ast[level].expr[j][last_arg - 1]) == 1)
								{
									printf ("error: line %lli: variable '%s' is constant!\n", linenum, ast[level].expr[j][last_arg - 1]);
									return (1);
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

							// set nested_code_off
							if (strcmp ((const char *) ast[level].expr[j][last_arg], "nested-code-off") == 0)
							{
								nested_code = 0;
								continue;
							}

							// set nested_code_on
							if (strcmp ((const char *) ast[level].expr[j][last_arg], "nested-code-on") == 0)
							{
								nested_code = 1;
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

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "=") == 0 || strcmp ((const char *) ast[level].expr[j][last_arg], "m=") == 0 || strcmp ((const char *) ast[level].expr[j][last_arg], ":=") == 0)
								{
									if (set_variable_prefix (ast[level].expr[j][last_arg - 1]) == 1)
									{
										printf ("error: line %lli: variable '%s' prefix not set with \\prefix\\ !\n", linenum, ast[level].expr[j][last_arg - 1]);
										return (1);
									}

									// do variable assign
                                    // check if : "x y move =" expression
									if (last_arg >= 2 && strcmp ((const char *) ast[level].expr[j][last_arg], "m=") == 0)
									{
											if (checkdef (ast[level].expr[j][last_arg - 2]) != 0)
											{
												return (1);
											}

											if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
											{
												return (1);
											}

											target_var_type = getvartype (ast[level].expr[j][last_arg - 1]);

											if (get_var_is_const (ast[level].expr[j][last_arg - 1]) == 1)
											{
												printf ("error: line %lli: variable '%s' is constant!\n", linenum, ast[level].expr[j][last_arg - 1]);
												return (1);
											}

											if (get_ranges_index (ast[level].expr[j][last_arg - 1]) != -1)
											{
												printf ("error: line %lli: variable '%s' is range variable, use {var = } expression!\n", linenum, ast[level].expr[j][last_arg - 1]);
												return (1);
											}

										    if (target_var_type == DOUBLE)
											{
												// check if both variables are double var
												if (getvartype (ast[level].expr[j][last_arg - 2]) != DOUBLE)
												{
													printf ("error: line %lli: variable '%s' is not double var!\n", linenum, ast[level].expr[j][last_arg - 2]);
													return (1);
												}

												// check if both variables are in registers
												reg2 = get_regd (ast[level].expr[j][last_arg - 1]);
												if (reg == -1)
												{
													goto move_end;
												}

												reg = get_regd (ast[level].expr[j][last_arg - 2]);
												if (reg == -1)
												{
													goto move_end;
												}

											    code_line++;
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
												}

												strcpy ((char *) code[code_line], "movd ");
											    sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], ", ");
												sprintf ((char *) str, "%i", reg2);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");

												printf ("move: line %lli: opcode set!\n", linenum);
											}
											else
											{
												// check if both variables are integer var
												if (getvartype (ast[level].expr[j][last_arg - 2]) != INTEGER)
												{
													printf ("error: line %lli: variable '%s' is not int var!\n", linenum, ast[level].expr[j][last_arg - 2]);
													return (1);
												}

												// check if both variables are in registers
												reg2 = get_regi (ast[level].expr[j][last_arg - 1]);
												if (reg == -1)
												{
													goto move_end;
												}

												reg = get_regi (ast[level].expr[j][last_arg - 2]);
												if (reg == -1)
												{
													goto move_end;
												}

											    code_line++;
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
												}

												strcpy ((char *) code[code_line], "movi ");
											    sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], ", ");
												sprintf ((char *) str, "%i", reg2);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");

												printf ("move: line %lli: opcode set!\n", linenum);
											}
										continue;
									}
								    move_end:

									// check if target is boolean variable
									if (ast[level].expr[j][last_arg - 1][0] == 'B')
									{
										{
											U1 found_bool = 0;

											if (strcmp ((const char *) ast[level].expr[j][last_arg - 2], "true") == 0)
											{
												found_bool = 1;
											}

											if (strcmp ((const char *) ast[level].expr[j][last_arg - 2], "false") == 0)
											{
												found_bool = 1;
											}

											if (found_bool == 0)
											{
												printf ("error: line %lli: boolean variable only can be true or false!\n", linenum);
												return (1);
											}
										}
									}

									target_var_type = getvartype_real (ast[level].expr[j][last_arg - 1]);

									if (expression_var_type_max > target_var_type)
									{
										if (expression_var_type_max == DOUBLE && target_var_type == QUADWORD)
										{
											if (expression_if_op == 1)
											{
												printf ("\nfound if like cast: line %lli\n", linenum);
												printf ("> %s\n", line);
											}
											else 
											{
												printf ("\nerror: line %lli: expression assigns to target of lower precision!\n", linenum);
												return (1);
											}
										}
										else 
										{
											printf ("\nerror: line %lli: expression assigns to target of lower precision!\n", linenum);
											return (1);
										}
									}

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

										if (strcmp ((const char *) ast[level + 1].expr[j][last_arg], "=") == 0 || strcmp ((const char *) ast[level + 1].expr[j][last_arg], ":=") == 0)
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
										init_registers (); // set all registers to empty

										if (checkdef (ast[level].expr[j][last_arg - 4]) != 0)
										{
											return (1);
										}

										if (ast[level].expr[j][last_arg - 4][0] != 'P')
										{
											// variable name doesn't begin with P, check if array
											if (get_variable_is_array (ast[level].expr[j][last_arg - 4]) <= 1)
											{
												printf ("error: line %lli: variable '%s' is not an array!\n", linenum, ast[level].expr[j][last_arg - 4]);
												return (1);
											}
										}
										if (ast[level].expr[j][last_arg - 4][0] != 'P')
										{
											target_var_type = getvartype_real (ast[level].expr[j][last_arg - 4]);
										}
										else 
										{
											target_var_type = getvartype_real (ast[level].expr[j][last_arg - 5]);
										}

										if (checkdef (ast[level].expr[j][last_arg - 5]) != 0)
										{
											return (1);
										}

										// check if source variable is array
										if (get_variable_is_array(ast[level].expr[j][last_arg - 5]) > 1)
										{
											printf ("error: line %lli: variable '%s' is array!\n", linenum, ast[level].expr[j][last_arg - 5]);
											return (1);
										}

										// check if variable is constant
										if (get_var_is_const (ast[level].expr[j][last_arg - 4]) == 1)
										{
											printf ("error: line %lli: variable '%s' is constant!\n", linenum, ast[level].expr[j][last_arg - 4]);
											return (1);
										}

										if (get_ranges_index (ast[level].expr[j][last_arg - 4]) != -1)
										{
											printf ("error: line %lli: variable '%s' is range variable, use {var = } expression!\n", linenum, ast[level].expr[j][last_arg - 4]);
											return (1);
										}

										expression_var_type_max = getvartype_real (ast[level].expr[j][last_arg - 5]);
										if (expression_var_type_max > target_var_type)
										{
											printf ("\nerror: line %lli: expression assigns to target of lower precision!\n", linenum);
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
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
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

											// check if index variable is array
											if (get_variable_is_array(ast[level].expr[j][last_arg - 2]) > 1)
											{
												printf ("error: line %lli: variable '%s' is array!\n", linenum, ast[level].expr[j][last_arg - 2]);
												return (1);
											}

											// get array index
											reg2 = get_regi (ast[level].expr[j][last_arg - 2]);
											if (reg2 == -1)
											{
												reg2 = get_free_regi ();
												set_regi (reg2, ast[level].expr[j][last_arg - 2]);

												code_line++;
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
												}

												strcpy ((char *) code[code_line], "loada ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 2]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg2);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");
											}

											// assign to array variable

											if (ast[level].expr[j][last_arg - 4][0] == 'P')
											{
												// variable is pointer
												strcpy ((char *) code_temp, "loada ");
											}
											else 
											{
												strcpy ((char *) code_temp, "load ");
											}
											strcat ((char *) code_temp, (const char *) ast[level].expr[j][last_arg - 4]);
											strcat ((char *) code_temp, ", 0, ");

											reg3 = get_free_regd ();

											sprintf ((char *) str, "%i", reg3);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, "\n");

											// printf ("%s\n", code_temp);

											code_line++;
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
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
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
											}

											strcpy ((char *) code[code_line], (const char *) code_temp);
										}
										else
										{
											// fixed variable to array BUG
											reg = get_regi (ast[level].expr[j][last_arg - 5]);
											if (reg == -1)
											{
												code_line++;
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
												}

												// variable is not in register, load it

												reg = get_free_regi ();
												set_regi (reg, ast[level].expr[j][last_arg - 5]);

												// set load opcode
												strcpy ((char *) code[code_line], "load ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 5]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");

												code_line++;
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
												}

												if (getvartype_real (ast[level].expr[j][last_arg - 5]) == BYTE)
												{
													strcpy ((char *) code[code_line], "pushb ");
												}

												if (getvartype_real (ast[level].expr[j][last_arg - 5]) == WORD)
												{
													strcpy ((char *) code[code_line], "pushw ");
												}

												if (getvartype_real (ast[level].expr[j][last_arg - 5]) == DOUBLEWORD)
												{
													strcpy ((char *) code[code_line], "pushdw ");
												}

												if (getvartype_real (ast[level].expr[j][last_arg - 5]) == QUADWORD)
												{
													strcpy ((char *) code[code_line], "pushqw ");
												}

												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], ", 0, ");

												// get free register
												reg4 = get_free_regi ();
												set_regi (reg4, ast[level].expr[j][last_arg - 5]);

												sprintf ((char *) str, "%i", reg4);
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
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
												}

												strcpy ((char *) code[code_line], "loada ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 2]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg2);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");
											}

											// assign to array variable

											if (ast[level].expr[j][last_arg - 4][0] == 'P')
											{
												// variable is pointer
												strcpy ((char *) code_temp, "loada ");
											}
											else 
											{
												strcpy ((char *) code_temp, "load ");
											}
											strcat ((char *) code_temp, (const char *) ast[level].expr[j][last_arg - 4]);
											strcat ((char *) code_temp, ", 0, ");

											reg3 = get_free_regi ();
											// set_regd (reg, (U1 *) ast[level].expr[j][last_arg - 1]);

											sprintf ((char *) str, "%i", reg3);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, "\n");

											// printf ("%s\n", code_temp);

											code_line++;
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
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

											sprintf ((char *) str, "%i", reg4);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, ", ");
											sprintf ((char *) str, "%i", reg3);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, ", ");
											sprintf ((char *) str, "%i", reg2);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, "\n");

											code_line++;
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
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

										if (ast[level].expr[j][last_arg - 5][0] != 'P')
										{
											// not a pointer variable
										
											if (get_variable_is_array (ast[level].expr[j][last_arg - 5]) <= 1)
											{
												printf ("error: line %lli: variable '%s' is not an array!\n", linenum, ast[level].expr[j][last_arg - 5]);
												return (1);
											}
										}

										target_var_type = getvartype_real (ast[level].expr[j][last_arg - 1]);

										if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
										{
											return (1);
										}

										// check if source variable is array
										if (get_variable_is_array(ast[level].expr[j][last_arg - 1]) > 1)
										{
											printf ("error: line %lli: variable '%s' is array!\n", linenum, ast[level].expr[j][last_arg - 1]);
											return (1);
										}

										// check if variable is constant
										if (get_var_is_const (ast[level].expr[j][last_arg - 1]) == 1)
										{
											printf ("error: line %lli: variable '%s' is constant!\n", linenum, ast[level].expr[j][last_arg - 1]);
											return (1);
										}

										if (get_ranges_index (ast[level].expr[j][last_arg - 1]) != -1)
										{
											printf ("error: line %lli: variable '%s' is range variable, use {var = } expression!\n", linenum, ast[level].expr[j][last_arg - 1]);
											return (1);
										}

										expression_var_type_max = getvartype_real (ast[level].expr[j][last_arg - 5]);
										if (ast[level].expr[j][last_arg - 5][0] != 'P')
										{
											// not a array pointer, check target variable
											if (expression_var_type_max > target_var_type)
											{
												printf ("\nerror: line %lli: expression assigns to target of lower precision!\n", linenum);
												return (1);
											}
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
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
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

											// check if index variable is array
											if (get_variable_is_array(ast[level].expr[j][last_arg - 3]) > 1)
											{
												printf ("error: line %lli: variable '%s' is array!\n", linenum, ast[level].expr[j][last_arg - 3]);
												return (1);
											}

											// get array index
											reg2 = get_regi (ast[level].expr[j][last_arg - 3]);
											if (reg2 == -1)
											{
												reg2 = get_free_regi ();
												set_regi (reg2, ast[level].expr[j][last_arg - 3]);

												code_line++;
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
												}

												strcpy ((char *) code[code_line], "loada ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 3]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg2);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");
											}

											// assign array variable to variable

											if (ast[level].expr[j][last_arg - 5][0] == 'P')
											{
												// variable is pointer
												strcpy ((char *) code_temp, "loada ");
											}
											else 
											{
												strcpy ((char *) code_temp, "load ");
											}
											strcat ((char *) code_temp, (const char *) ast[level].expr[j][last_arg - 5]);
											strcat ((char *) code_temp, ", 0, ");

											reg3 = get_free_regd ();
											// set_regd (reg, (U1 *) ast[level].expr[j][last_arg - 1]);

											sprintf ((char *) str, "%i", reg3);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, "\n");

											// printf ("%s\n", code_temp);

											code_line++;
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
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
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
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
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
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
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
												}

												strcpy ((char *) code[code_line], "loada ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 3]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg2);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");
											}

											// assign array variable to variable

											if (ast[level].expr[j][last_arg - 5][0] == 'P')
											{
												// variable is pointer
												strcpy ((char *) code_temp, "loada ");
											}
											else 
											{
												strcpy ((char *) code_temp, "load ");
											}
											strcat ((char *) code_temp, (const char *) ast[level].expr[j][last_arg - 5]);
											strcat ((char *) code_temp, ", 0, ");

											reg3 = get_free_regi ();
											// set_regd (reg, (U1 *) ast[level].expr[j][last_arg - 1]);

											sprintf ((char *) str, "%i", reg3);
											strcat ((char *) code_temp, (const char *) str);
											strcat ((char *) code_temp, "\n");

											// printf ("%s\n", code_temp);

											code_line++;
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
											}

											strcpy ((char *) code[code_line], (const char *) code_temp);

											if (checkdef (ast[level].expr[j][last_arg - 5]) != 0)
											{
												return (1);
											}

											// check if array pointer is used
											// and set the push opcode based on the variable type we assign to!
										    if (ast[level].expr[j][last_arg - 5][0] == 'P')
											{
												if (getvartype_real (ast[level].expr[j][last_arg - 1]) == BYTE)
												{
													strcpy ((char *) code_temp, "pushb ");
												}

												if (getvartype_real (ast[level].expr[j][last_arg - 1]) == WORD)
												{
													strcpy ((char *) code_temp, "pushw ");
												}

												if (getvartype_real (ast[level].expr[j][last_arg - 1]) == DOUBLEWORD)
												{
													strcpy ((char *) code_temp, "pushdw ");
												}

												if (getvartype_real (ast[level].expr[j][last_arg - 1]) == QUADWORD)
												{
													strcpy ((char *) code_temp, "pushqw ");
												}
											}
											else
											{
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
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
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

									if (get_ranges_index (ast[level].expr[j][last_arg - 1]) != -1)
									{
										printf ("error: line %lli: variable '%s' is range variable, use {var = } expression!\n", linenum, ast[level].expr[j][last_arg - 1]);
										return (1);
									}

									target_var_type = getvartype_real (ast[level].expr[j][last_arg - 1]);

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
										if (code_line >= code_max_lines)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (3);
										}

										strcpy ((char *) code[code_line], (const char *) code_temp);
										set_regd (reg, (U1 *)"");
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
										if (code_line >= code_max_lines)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (3);
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
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
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
													if (code_line >= code_max_lines)
													{
														printf ("error: line %lli: code list full!\n", linenum);
														return (3);
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
													if (code_line >= code_max_lines)
													{
														printf ("error: line %lli: code list full!\n", linenum);
														return (3);
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

													// check if: (cast x y =) case:
													if (last_arg >= 3)
													{
														if (strcmp ((const char *) ast[level].expr[j][last_arg - 3], "cast") == 0 || (strcmp ((const char *) ast[level].expr[j][last_arg - 3], "bool") == 0))
														{
															if (strcmp ((const char *) ast[level].expr[j][last_arg - 3], "cast") == 0)
															{
																printf ("\ncast: line %lli: cast to lower precision variable!\n", linenum);
																printf ("> %s\n", line);
															}
														}
														else
														{
															printf ("\nsyntax error assign line: %lli\n", linenum);
															return (1);
														}

														// check if bool assign
														{
															U1 bool_set = 0;    // no bool expression found

															if (strcmp ((const char *) ast[level].expr[j][last_arg - 3], "bool") == 0)
															{
																// printf ("\nbool: line %lli: bool expression assign variable!\n", linenum);
																// printf ("> %s\n", line);

																if (strcmp ((const char * )ast[level].expr[j][last_arg - 2], "false") == 0)
																{
																	// false found
																	bool_set = 1;
																}
																if (strcmp ((const char * )ast[level].expr[j][last_arg - 2], "true") == 0)
																{
																	// true found
																	bool_set = 1;
																}

																if (bool_set == 0)
																{
																	printf ("\nbool: line %lli: error variable assign must be true or false!\n", linenum);
																	return (1);
																}
															}
														}
													}
													else 
													{
														target_var_type = getvartype_real (ast[level].expr[j][last_arg - 1]);
														expression_var_type_max = getvartype_real (ast[level].expr[j][last_arg - 2]);

														if (expression_var_type_max > target_var_type)
														{
															if (warnings_as_errors == 0)
															{
																printf ("\nWARNING: line %lli: variable assign to a lower precision variable!\n", linenum);
																printf ("> %s\n", line);
															}
															else 
															{
																// warnings are ERRORS!
																printf ("\nERROR: line %lli: variable assign to a lower precision variable!\n", linenum);
																return (1);
															}
														}
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
															if (code_line >= code_max_lines)
															{
																printf ("error: line %lli: code list full!\n", linenum);
																return (3);
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
															if (code_line >= code_max_lines)
															{
																printf ("error: line %lli: code list full!\n", linenum);
																return (3);
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
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
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

									if (get_ranges_index (ast[level].expr[j][last_arg - 1]) != -1)
									{
										printf ("error: line %lli: variable '%s' is range variable, use {var = } expression!\n", linenum, ast[level].expr[j][last_arg - 1]);
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
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
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
										if (code_line >= code_max_lines)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (3);
										}

										strcpy ((char *) code[code_line], (const char *) code_temp);

										reg2 = get_regd (ast[level].expr[j][last_arg - 1]);
										if (reg2 != -1)
										{
											// set old value of reg2 as empty
											set_regd (reg2, (U1 *) "");
										}

										set_regd (target, (U1 *) "");
										continue;
									}

									ok = 1;

									if (set_variable_prefix ((U1 *) "") == 1)
									{
										printf ("error: line %lli: variable '%s' prefix not set with \\prefix\\ !\n", linenum, ast[level].expr[j][last_arg - 1]);
										return (1);
									}
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
									if (last_arg != 0)
									{
										// label :foo for example is not the only argument: possible function call without "!" or "call"?
										printf ("error: line %lli: label: %s not the only argument! Is this a function call without 'call' or '!' ?\n", linenum, ast[level].expr[j][last_arg]);
										return (1);
									}

									if (search_label (ast[level].expr[j][last_arg]) == 1)
									{
										printf ("error: line %lli: label: %s already defined!\n", linenum, ast[level].expr[j][last_arg]);
										return (1);
									}

									if (nested_code == 1)
									{
										init_registers ();
									}

									strcpy ((char *) code_temp, (const char *) ast[level].expr[j][last_arg]);

									code_line++;
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
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
									nested_code = 1; // beginn of function switch on

									if (last_arg < 1)
									{
										printf ("error: line %lli: no function name set!\n", linenum);
										return (1);
									}

									// contracts flags reset
									precondition = 0;
									precondition_code = 0;
									precondition_end = 0;
									postcondition = 0;
									postcondition_code = 0;
									postcondition_end = 0;

									if (inside_object == 0)
									{
										// is standalone function not inside object!!
										// for variable ending check, set function name
										strcpy ((char *) varname_end, (const char *) ast[level].expr[j][last_arg -1]);
									}

									// check if inside object
									if (inside_object == 0 && ast[level].expr[j][last_arg - 1][0] == 'P')
									{
										// error
										printf ("error: line %lli: private object function not in object!\n", linenum);
										return (1);
									}

									if (inside_object == 1)
									{   // check if object function name is of foobar->objectname type!
										// new scope
										{
											U1 object_function_end[MAXSTRLEN];
											S8 pos;
											S8 object_function_end_len;
											S8 function_name_len;

											strcpy ((char *) object_function_end, "->");
											strcat ((char *) object_function_end, (char *) object_name);

											pos = searchstr (ast[level].expr[j][last_arg - 1], (U1 *) object_function_end, 0, 0, TRUE);
											if (pos == -1)
											{
												// objectc name end not found: ERROR!
												printf ("error: line %lli: object function end not: %s !\n", linenum, object_function_end);
												return (1);
											}

											object_function_end_len = strlen_safe ((const char *) object_function_end, MAXLINELEN);
											function_name_len = strlen_safe ((const char *) ast[level].expr[j][last_arg - 1], MAXLINELEN);

											if (function_name_len - object_function_end_len != pos)
											{
												// objectc name end not found: ERROR!
												printf ("error: line %lli: object function end not: %s !\n", linenum, object_function_end);
												return (1);
											}
											if (pos == 0)
											{
												// no function name set, only ending: ERROR!
												printf ("error: line %lli: object function name not valid: %s !\n", linenum, ast[level].expr[j][last_arg - 1]);
												return (1);
											}
										}
									}

									// start of a function

									if (strcmp ((const char *) ast[level].expr[j][last_arg - 1], "main") != 0)
									{
										// not main function, initialize the registers

										init_registers ();

										// check if pure function
										if (check_pure_function (ast[level].expr[j][last_arg - 1]) == 0)
										{
											if (check_varname_end_local_only == 0)
											{
												printf ("error: line %lli: function marked as pure, but no (variable-local-only-on) flag set: '%s'\n", linenum, line);
												return (1);
											}
											if (pure_function_override == 0)
											{
												pure_function = 1;
											}
										}
									}

									if (search_label (ast[level].expr[j][last_arg - 1]) == 1)
									{
										printf ("error: line %lli: label: %s already defined!\n", linenum, ast[level].expr[j][last_arg - 1]);
										return (1);
									}

									strcpy ((char *) code_temp, ":");
									strcat ((char *) code_temp, (const char *) ast[level].expr[j][last_arg - 1]);

									code_line++;
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
									}

									strcpy ((char *) code[code_line], (const char *) code_temp);
									strcat ((char *) code[code_line], "\n");

									code_line++;
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
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

									pure_function = 0; // reset flags!
									pure_function_override = 0;

									if (inside_unsafe == 1)
									{
										printf ("error: line %lli: unsafe-end not set before function end!\n", linenum);
										return (1);
									}

									if (contracts == 1)
									{
										if (precondition == 0)
										{
											printf ("error: line %lli: no precondition start set!\n", linenum);
											return (1);
										}
										if (precondition_end == 0)
										{
											printf ("error: line %lli: no precondition end set!\n", linenum);
											return (1);
										}
										if (postcondition == 0)
										{
											printf ("error: line %lli: no postcondition start set!\n", linenum);
											return (1);
										}
										if (postcondition_end == 0)
										{
											printf ("error: line %lli: no postcondition end set!\n", linenum);
											return (1);
										}

										if (precondition_code == 2)
										{
											printf ("error: line %lli: no precondition code set!\n", linenum);
											return (1);
										}
										if (postcondition_code == 2)
										{
											printf ("error: line %lli: no postcondition code set!\n", linenum);
											return (1);
										}
									}

									code_line++;
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
									}

									strcpy ((char *) code[code_line], (const char *) "rts");
									strcat ((char *) code[code_line], "\n");
									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "return") == 0)
								{
									// return from function

									code_line++;
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
									}

									strcpy ((char *) code[code_line], (const char *) "rts");
									strcat ((char *) code[code_line], "\n");
									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "intr0") == 0 || strcmp ((const char *) ast[level].expr[j][last_arg], "intr1") == 0 || strcmp ((const char *) ast[level].expr[j][last_arg], "pushb") == 0 || strcmp ((const char *) ast[level].expr[j][last_arg], "pullb") == 0)
								{
									strcpy ((char *) code_temp, (const char *) ast[level].expr[j][last_arg]);
									strcat ((char *) code_temp, " ");

									if (strcmp ((const char *) ast[level].expr[j][last_arg], "intr0") == 0)
									{
										if (last_arg >= 4)
										{
											if (strcmp ((const char *) ast[level].expr[j][last_arg - 4], "39") == 0)
											{
												// memory bounds on
												memory_bounds = 1;
											}

											if (strcmp ((const char *) ast[level].expr[j][last_arg - 4], "40") == 0)
											{
												// memory bounds off
												// check if inside unsafe code block

												if (inside_unsafe == 0)
												{
													printf ("error: line %lli: memory_bounds_off called outside of unsafe code block!\n", linenum);
													return (1);
												}

											}
										}
									}



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
													if (code_line >= code_max_lines)
													{
														printf ("error: line %lli: code list full!\n", linenum);
														return (3);
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
															if (code_line >= code_max_lines)
															{
																printf ("error: line %lli: code list full!\n", linenum);
																return (3);
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
															if (code_line >= code_max_lines)
															{
																printf ("error: line %lli: code list full!\n", linenum);
																return (3);
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
															if (code_line >= code_max_lines)
															{
																printf ("error: line %lli: code list full!\n", linenum);
																return (3);
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
															if (code_line >= code_max_lines)
															{
																printf ("error: line %lli: code list full!\n", linenum);
																return (3);
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
															if (code_line >= code_max_lines)
															{
																printf ("error: line %lli: code list full!\n", linenum);
																return (3);
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
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
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
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
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
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
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
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
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
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
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
                                    if (nested_code == 1)
									{
										init_registers ();
									}

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
													if (code_line >= code_max_lines)
													{
														printf ("error: line %lli: code list full!\n", linenum);
														return (3);
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
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
												}

												strcpy ((char *) code[code_line], "jmpi ");
												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], ", ");
												strcat ((char *) code[code_line], (const char *) if_label);
												strcat ((char *) code[code_line], "\n");

												// write code jmpi to endif label

												code_line++;
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
												}

												strcpy ((char *) code[code_line], "jmp ");
												strcat ((char *) code[code_line], (const char *) endif_label);
												strcat ((char *) code[code_line], "\n");

												// write code label if

												code_line++;
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
												}
												strcat ((char *) code[code_line], (const char *) if_label);
												strcat ((char *) code[code_line], "\n");
											}
											else
											{
												printf ("optimizing if: code line: %lli\n", linenum);

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
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
												}

												strcpy ((char *) code[code_line], "jmpi ");
												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], ", ");
												strcat ((char *) code[code_line], (const char *) if_label);
												strcat ((char *) code[code_line], "\n");

												// write code jmpi to endif label

												code_line++;
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
												}

												strcpy ((char *) code[code_line], "jmp ");
												strcat ((char *) code[code_line], (const char *) endif_label);
												strcat ((char *) code[code_line], "\n");

												// write code label if

												code_line++;
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
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
									if (nested_code == 1)
									{
										init_registers ();
									}

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
													if (code_line >= code_max_lines)
													{
														printf ("error: line %lli: code list full!\n", linenum);
														return (3);
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
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
												}

												strcpy ((char *) code[code_line], "jmpi ");
												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], ", ");
												strcat ((char *) code[code_line], (const char *) if_label);
												strcat ((char *) code[code_line], "\n");

												// write code jmpi to endif label

												code_line++;
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
												}

												get_else_label (if_pos, else_label);

												strcpy ((char *) code[code_line], "jmp ");
												strcat ((char *) code[code_line], (const char *) else_label);
												strcat ((char *) code[code_line], "\n");

												// write code label if

												code_line++;
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
												}
												strcat ((char *) code[code_line], (const char *) if_label);
												strcat ((char *) code[code_line], "\n");
											}
											else
											{
												printf ("optimizing if: code line: %lli\n", linenum);

												// comment out not needed opcodes set earlier
												strcpy ((char *) code[code_line - 1], "");
												strcpy ((char *) code[code_line], "");

												reg = get_if_optimize_reg (code[code_line - 2]);
												if (reg < 0)
												{
													printf ("error: line %lli: optimize if internal error!\n", linenum);
													return (1);
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
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
												}

												strcpy ((char *) code[code_line], "jmpi ");
												sprintf ((char *) str, "%i", reg);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], ", ");
												strcat ((char *) code[code_line], (const char *) if_label);
												strcat ((char *) code[code_line], "\n");

												// write code jmpi to endif label

												code_line++;
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
												}

												get_else_label (if_pos, else_label);

												strcpy ((char *) code[code_line], "jmp ");
												strcat ((char *) code[code_line], (const char *) else_label);
												strcat ((char *) code[code_line], "\n");

												// write code label if

												code_line++;
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
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
									if (nested_code == 1)
									{
										init_registers ();
									}

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
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
									}

									strcpy ((char *) code[code_line], "jmp ");
									strcat ((char *) code[code_line], (const char *) endif_label);
									strcat ((char *) code[code_line], "\n");

									code_line++;
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
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
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
									}

									strcat ((char *) code[code_line], (const char *) endif_label);
									strcat ((char *) code[code_line], "\n");

									set_endif_finished (if_pos);

								    if (nested_code == 1)
									{
										init_registers ();
									}

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

									if (nested_code == 1)
									{
										init_registers ();
									}
									continue;
								}

								if ((strcmp ((const char *) ast[level].expr[j][last_arg], "switch-end") == 0 || strcmp ((const char *) ast[level].expr[j][last_arg], "switchend") == 0) && last_arg == 0)
								{
									if (nested_code == 1)
									{
										init_registers ();
									}

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
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
									}

									strcat ((char *) code[code_line], (const char *) endif_label);
									strcat ((char *) code[code_line], "\n");

									set_endif_finished (if_pos);

									if (nested_code == 1)
									{
										init_registers ();
									}

									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "?") == 0 && last_arg == 2)
								{
									if (nested_code == 1)
									{
										init_registers ();
									}

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
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
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
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
												}

												strcpy ((char *) code[code_line], "loada ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 2]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg2);
												strcat ((char *) code[code_line], (const char *) str);
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
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
												}

												strcpy ((char *) code[code_line], "load ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 1]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg3);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");

												code_line++;
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
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
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
												}

												strcpy ((char *) code[code_line], "load ");
												strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 2]);
												strcat ((char *) code[code_line], ", 0, ");
												sprintf ((char *) str, "%i", reg4);
												strcat ((char *) code[code_line], (const char *) str);
												strcat ((char *) code[code_line], "\n");

												code_line++;
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
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
										if (code_line >= code_max_lines)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (3);
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
										if (code_line >= code_max_lines)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (3);
										}

										strcpy ((char *) code[code_line], "jmpi ");
										sprintf ((char *) str, "%i", target);
										strcat ((char *) code[code_line], (const char *) str);
										strcat ((char *) code[code_line], ", ");
										strcat ((char *) code[code_line], (const char *) if_label);
										strcat ((char *) code[code_line], "\n");

										// write code jmpi to endif label

										code_line++;
										if (code_line >= code_max_lines)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (3);
										}

										strcpy ((char *) code[code_line], "jmp ");
										strcat ((char *) code[code_line], (const char *) endif_label);
										strcat ((char *) code[code_line], "\n");

										// write code label if

										code_line++;
										if (code_line >= code_max_lines)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (3);
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
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
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
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
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
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
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
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
											}

											strcpy ((char *) code[code_line], "jmpi ");
											sprintf ((char *) str, "%i", reg3);
											strcat ((char *) code[code_line], (const char *) str);
											strcat ((char *) code[code_line], ", ");
											strcat ((char *) code[code_line], (const char *) if_label);
											strcat ((char *) code[code_line], "\n");

											// write code jmpi to endif label

											code_line++;
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
											}

											strcpy ((char *) code[code_line], "jmp ");
											strcat ((char *) code[code_line], (const char *) endif_label);
											strcat ((char *) code[code_line], "\n");

											// write code label if

											code_line++;
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
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
									if (nested_code == 1)
									{
										init_registers ();
									}

									while_pos = get_while_pos ();
									if (while_pos == -1)
									{
										printf ("compile: error: while: out of memory while-list\n");
										return (FALSE);
									}

									get_while_label (while_pos, while_label);

									code_line++;
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
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
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
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
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
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

									if (nested_code == 1)
									{
										init_registers ();
									}
								}


								// for/next loop ===============================================================

								// set top for loop label
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "for-loop") == 0)
								{
									if (nested_code == 1)
									{
										init_registers ();
									}

									for_pos = get_for_pos ();
									if (for_pos == -1)
									{
										printf ("compile: error: for: out of memory for-list\n");
										return (FALSE);
									}

									get_for_label (for_pos, for_label);

									code_line++;
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
									}

									strcpy ((char *) code[code_line], (const char *) for_label);
									strcat ((char *) code[code_line], "\n");

									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "for") == 0 && last_arg > 0)
								{
									if (nested_code == 1)
									{
										init_registers ();
									}

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
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
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
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
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
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
											}

											strcpy ((char *) code[code_line], "jmp ");
											strcat ((char *) code[code_line], (const char *) for_label);
											strcat ((char *) code[code_line], "\n");

											get_for_label_2 (for_pos, for_label);

											code_line++;
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
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
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
									}

									strcpy ((char *) code[code_line], "jmp ");
									strcat ((char *) code[code_line], (const char *) for_label);
									strcat ((char *) code[code_line], "\n");

									// set next label end as jump point if for loop not longer is running
									get_for_label_end (for_pos, for_label);

									code_line++;
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
									}

									strcpy ((char *) code[code_line], (const char *) for_label);
									strcat ((char *) code[code_line], "\n");

									set_for_end (for_pos);

									if (nested_code == 1)
									{
										init_registers ();
									}
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

								// set no var pull flag =========================================================
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "no-var-pull-on") == 0)
								{
									no_var_pull = 1;
									continue;
								}

								// remove no var pull flag ======================================================
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "no-var-pull-off") == 0)
								{
									no_var_pull = 0;
									continue;
								}							

								// set variable is immutable by default ========================================
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "variable-immutable") == 0)
								{
									var_immutable = 1;
									continue;
								}

								// set variable is immutable by default ========================================
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "variable-mutable") == 0)
								{
									var_immutable = 0;
									continue;
								}

								// set local variable ending check, to allow only access to local function variables or 'main' global variables
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "variable-local-on") == 0)
								{
									check_varname_end = 1;
									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "variable-local-off") == 0)
								{
									check_varname_end = 0;
									continue;
								}

								// set local variable ending check, to allow only access to local function variables or 'main' global variables
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "variable-local-only-on") == 0)
								{
									check_varname_end = 1;
									check_varname_end_local_only = 1;
									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "variable-local-only-off") == 0)
								{
									check_varname_end = 0;
									check_varname_end_local_only = 0;
									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "pure-off") == 0)
								{
									// set exception on pure function calls only!
									pure_function_override = 1;
									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "forbid-unsafe") == 0)
								{
									forbid_unsafe = 1;
									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "unsafe") == 0)
								{
									if (forbid_unsafe == 1)
									{
										printf ("error: line %lli: unsafe forbidden!\n", linenum);
										return (1);
									}

									// inside unsafe block
									inside_unsafe = 1;
									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "unsafe-end") == 0)
								{
									if (memory_bounds == 0)
									{
										printf ("error: line %lli: memory_bounds_on not called inside of unsafe code block!\n", linenum);
										return (1);
									}

									// unsafe block end
									inside_unsafe = 0;
									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "linter") == 0)
								{
									printf ("build needs linter!\n");
									do_check_linter = 1;
									continue;
								}

								// contracts
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "contracts-on") == 0)
								{
									contracts = 1;
									continue;
								}
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "contracts-off") == 0)
								{
									contracts = 0;
									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "precondition") == 0)
								{
									precondition = 1;
									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "precondition-end") == 0)
								{
									precondition_end = 1;
									precondition_code++;
									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "postcondition") == 0)
								{
									postcondition = 1;
									continue;
								}

								if (strcmp ((const char *) ast[level].expr[j][last_arg], "postcondition-end") == 0)
								{
									postcondition_end = 1;
									postcondition_code++;
									continue;
								}

								// pointer: store data address int int64 variable
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "pointer") == 0)
								{
									if (last_arg != 2)
									{
										printf ("error: line %lli: pointer arguments mismatch!\n", linenum);
										return (1);
									}

									// check if variables are defined 
									if (checkdef (ast[level].expr[j][last_arg - 2]) != 0)
									{
										return (1);
									}

									if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
									{
										return (1);
									}
									
									// check if source variable is const
									if (get_var_is_const (ast[level].expr[j][last_arg - 2]) == 1)
									{
										printf ("error: line %lli: pointer source variable is constant!\n", linenum);
										return (1);
									}

									if (get_ranges_index (ast[level].expr[j][last_arg - 2]) != -1)
									{
										printf ("error: line %lli: variable '%s' is range variable, use {var = } expression!\n", linenum, ast[level].expr[j][last_arg - 2]);
										return (1);
									}

									// check if variable name begins with uppercase P
									if (ast[level].expr[j][last_arg - 1][0] != 'P')
									{
										printf ("error: line %lli: pointer name must begin with uppercase P!\n", linenum);
										return (1);
									}

									// check if target variable is int64
									target_var_type = getvartype_real (ast[level].expr[j][last_arg - 1]);
									if (target_var_type != QUADWORD)
									{
										printf ("error: line %lli: pointer target variable not of int64 type!\n", linenum);
										return (1);
									}

									// get empty register for load opcode. loading variable address
									reg = get_regi (ast[level].expr[j][last_arg - 2]);
									if (reg == -1)
									{
										// variable is not in register, load it

										reg = get_free_regi ();
										set_regi (reg, ast[level].expr[j][last_arg - 2]);
									}

									code_line++;
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
									}

									strcpy ((char *) code[code_line], "load ");
									strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 2]);
									strcat ((char *) code[code_line], ", 0, ");
									sprintf ((char *) str, "%i", reg);
									strcat ((char *) code[code_line], (const char *) str);
									strcat ((char *) code[code_line], "\n");

									code_line++;
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
									}

									// get empty register for load opcode. loading target register
									target = get_regi (ast[level].expr[j][last_arg - 1]);
									if (target == -1)
									{
										// variable is not in register, load it

										target = get_free_regi ();
										set_regi (target, ast[level].expr[j][last_arg - 1]);
									}

									strcpy ((char *) code[code_line], "load ");
									strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 1]);
									strcat ((char *) code[code_line], ", 0, ");
									sprintf ((char *) str, "%i", target);
									strcat ((char *) code[code_line], (const char *) str);
									strcat ((char *) code[code_line], "\n");

									code_line++;
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
									}

									strcpy ((char *) code_temp, "pullqw ");
									sprintf ((char *) str, "%i", reg);
									strcat ((char *) code_temp, (const char *) str);
									strcat ((char *) code_temp, ", ");
									sprintf ((char *) str, "%i", target);
									strcat ((char *) code_temp, (const char *) str);
									strcat ((char *) code_temp, ", 0\n");

									code_line++;
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
									}

									strcpy ((char *) code[code_line], (const char *) code_temp);
									set_regi (target, (U1 *) "");
									continue;
								}

								// load opcode for loading variable into register. Use it before JIT-compiler blocks NEW CODE
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "load") == 0)
								{
									// new varaible scope for U1 type_ok
									{
									U1 type_ok = 0;

									if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
									{
										return (1);
									}

                                    // check if target variable is int64
									target_var_type = getvartype_real (ast[level].expr[j][last_arg - 1]);
                                    if (target_var_type == QUADWORD)
									{
										type_ok = 1;
										// get empty register for load opcode. loading target register
										target = get_regi (ast[level].expr[j][last_arg - 1]);
										if (target == -1)
										{
											// variable is not in register, load it

											target = get_free_regi ();
											set_regi (target, ast[level].expr[j][last_arg - 1]);
										}

										code_line++;
									    if (code_line >= code_max_lines)
									    {
										     printf ("error: line %lli: code list full!\n", linenum);
										     return (3);
									    }

	                                    strcpy ((char *) code[code_line], "loada ");
										strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 1]);
										strcat ((char *) code[code_line], ", 0, ");
										sprintf ((char *) str, "%i", target);
										strcat ((char *) code[code_line], (const char *) str);
										strcat ((char *) code[code_line], "\n");
									}

									if (target_var_type == DOUBLE)
									{
										type_ok = 1;
										// get empty register for load opcode. loading target register
										target = get_regd (ast[level].expr[j][last_arg - 1]);
										if (target == -1)
										{
											// variable is not in register, load it

											target = get_free_regd ();
											set_regd (target, ast[level].expr[j][last_arg - 1]);
										}

										code_line++;
									    if (code_line >= code_max_lines)
									    {
										     printf ("error: line %lli: code list full!\n", linenum);
										     return (3);
									    }

	                                    strcpy ((char *) code[code_line], "loadd ");
										strcat ((char *) code[code_line], (const char *) ast[level].expr[j][last_arg - 1]);
										strcat ((char *) code[code_line], ", 0, ");
										sprintf ((char *) str, "%i", target);
										strcat ((char *) code[code_line], (const char *) str);
										strcat ((char *) code[code_line], "\n");
									}
									if (type_ok == 0)
									{
										printf ("error: line %lli: load variable not int64 or double type!\n", linenum);
										return (1);
									}
									} // scope block end
									continue;
								}

								// range: get variable min max ranges ============================================================================
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "range") == 0)
								{
									// new variable scope
									{
									S2 target_type;
									S2 min_type;
									S2 max_type;

									if (last_arg != 3)
									{
										printf ("error: line %lli: range arguments mismatch!\n", linenum);
										return (1);
									}

									// check if variables are defined
									if (checkdef (ast[level].expr[j][last_arg - 3]) != 0)
									{
										return (1);
									}

									if (checkdef (ast[level].expr[j][last_arg - 2]) != 0)
									{
										return (1);
									}

									if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
									{
										return (1);
									}

									// check if ranges variables are constant
									if (get_var_is_const (ast[level].expr[j][last_arg - 2]) != 1)
									{
										printf ("error: line %lli: range argument min: %s is not constant!\n", linenum, ast[level].expr[j][last_arg - 2]);
										return (1);
									}

									if (get_var_is_const (ast[level].expr[j][last_arg - 1]) != 1)
									{
										printf ("error: line %lli: range argument max: %s is not constant!\n", linenum, ast[level].expr[j][last_arg - 1]);
										return (1);
									}

									// get variable types
								    target_type = getvartype (ast[level].expr[j][last_arg - 3]);
									min_type = getvartype (ast[level].expr[j][last_arg - 2]);
									max_type = getvartype (ast[level].expr[j][last_arg - 1]);

									// do check
									if (target_type == DOUBLE)
									{
										if (min_type != DOUBLE || max_type != DOUBLE)
										{
											printf ("error: line %lli: range arguments are not double type!\n", linenum);
											return (1);
										}
									}
									else
									{
										if (min_type != INTEGER || max_type != INTEGER)
										{
											printf ("error: line %lli: range arguments are not int type!\n", linenum);
											return (1);
										}
									}

									if (strlen_safe ((const char *) ast[level].expr[j][last_arg - 3], MAXSTRLEN) >= MAXSTRLEN)
									{
										printf ("error: line %lli: variable: %s name overflow!\n", linenum, ast[level].expr[j][last_arg - 3]);
										return (1);
									}

									if (strlen_safe ((const char *) ast[level].expr[j][last_arg - 2], MAXSTRLEN) >= MAXSTRLEN)
									{
										printf ("error: line %lli: variable: %s min overflow!\n", linenum, ast[level].expr[j][last_arg - 2]);
										return (1);
									}

									if (strlen_safe ((const char *) ast[level].expr[j][last_arg - 1], MAXSTRLEN) >= MAXSTRLEN)
									{
										printf ("error: line %lli: variable: %s max overflow!\n", linenum, ast[level].expr[j][last_arg - 1]);
										return (1);
									}

									if (ranges_ind >= MAXRANGESVAR)
									{
										printf ("error: line %lli: range index overflow!\n", linenum);
										return (1);
									}

									ranges_ind++;
                                    strcpy ((char *) ranges[ranges_ind].varname, (const char *) ast[level].expr[j][last_arg - 3]);
									strcpy ((char *) ranges[ranges_ind].min, (const char *) ast[level].expr[j][last_arg - 2]);
									strcpy ((char *) ranges[ranges_ind].max, (const char *) ast[level].expr[j][last_arg - 1]);

									continue;
									}
								}

                                // object flag
                                // object start  =======================================================================================
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "object") == 0)
								{
									if (last_arg != 1)
									{
										printf ("error: line %lli: no object name set!\n", linenum);
										return (1);
									}

									if (inside_object == 1)
									{
										printf ("error: line %lli: object start is nested!\n", linenum);
										return (1);
									}

									inside_object = 1;

									// save object name
									strcpy ((char *) object_name, (const char *) ast[level].expr[j][last_arg - 1]);

									// for variable ending check, set function name
								    strcpy ((char *) varname_end, (const char *) ast[level].expr[j][last_arg - 1]);

									continue;
								}

                                // object end  =======================================================================================
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "objectend") == 0)
								{
									if (inside_object != 1)
									{
										printf ("error: line %lli: object start is not set!\n", linenum);
										return (1);
									}

									inside_object = 0;

									continue;
								}

								// call =========================================================================
								if (strcmp ((const char *) ast[level].expr[j][last_arg], "call") == 0 || strcmp ((const char *) ast[level].expr[j][last_arg], "!") == 0)
								{
									// function call with arguments: "call" or "!" !!!

									if (inside_object == 0 && ast[level].expr[j][last_arg - 1][1] == 'P')
									{
										// found private function call outside object: ERROR!

										printf ("error: line %lli: private object function called outside object!\n", linenum);
										return (1);
									}

									if (pure_function == 1 && pure_function_override == 0)
									{
										if (check_pure_function (ast[level].expr[j][last_arg - 1]) != 0)
										{
											printf ("error line %lli: called function is not marked as pure: '%s'\n", linenum, line);
											return (1);
										}
									}

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
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
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
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
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
													if (code_line >= code_max_lines)
													{
														printf ("error: line %lli: code list full!\n", linenum);
														return (3);
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
															if (code_line >= code_max_lines)
															{
																printf ("error: line %lli: code list full!\n", linenum);
																return (3);
															}

															// set pushb opcode
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
															if (code_line >= code_max_lines)
															{
																printf ("error: line %lli: code list full!\n", linenum);
																return (3);
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
															if (code_line >= code_max_lines)
															{
																printf ("error: line %lli: code list full!\n", linenum);
																return (3);
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
													if (code_line >= code_max_lines)
													{
														printf ("error: line %lli: code list full!\n", linenum);
														return (3);
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
													if (code_line >= code_max_lines)
													{
														printf ("error: line %lli: code list full!\n", linenum);
														return (3);
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
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
											}

											strcpy ((char *) code[code_line], (const char *) code_temp);
											strcat ((char *) code[code_line], "\n");
										}
									}

									strcpy ((char *) code_temp, "jsr ");
									strcat ((char *) code_temp, (const char *) ast[level].expr[j][last_arg - 1]);
									strcat ((char *) code_temp, "\n");

									code_line++;
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
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
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
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
												if (code_line >= code_max_lines)
												{
													printf ("error: line %lli: code list full!\n", linenum);
													return (3);
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

							// stpush  =========================================================================
							if (strcmp ((const char *) ast[level].expr[j][last_arg], "stpush") == 0 || strcmp ((const char *) ast[level].expr[j][last_arg], "s>") == 0)
							{
								// exp: (num1 num2 stpush)
								// (num1 num2 s>)

								for (e = 0; e < last_arg; e++)
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
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
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
													if (code_line >= code_max_lines)
													{
														printf ("error: line %lli: code list full!\n", linenum);
														return (3);
													}

													// set pushb opcode
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
													if (code_line >= code_max_lines)
													{
														printf ("error: line %lli: code list full!\n", linenum);
														return (3);
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
													if (code_line >= code_max_lines)
													{
														printf ("error: line %lli: code list full!\n", linenum);
														return (3);
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
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
											}

											strcpy ((char *) code[code_line], "loadd ");
											strcat ((char *) code[code_line], (const char *) ast[level].expr[j][e]);
											strcat ((char *) code[code_line], ", 0, ");
											sprintf ((char *) str, "%i", reg);
											strcat ((char *) code[code_line], (const char *) str);
											strcat ((char *) code[code_line], "\n");

											strcat ((char *) code_temp, (const char *) str);
										}
										else
										{
											sprintf ((char *) str, "%i", reg);
											strcat ((char *) code_temp, (const char *) str);
										}
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
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
											}

											strcpy ((char *) code[code_line], "loada ");
											strcat ((char *) code[code_line], (const char *) str);
											strcat ((char *) code[code_line], ", 0, ");
											sprintf ((char *) str, "%i", reg);
											strcat ((char *) code[code_line], (const char *) str);
											strcat ((char *) code[code_line], "\n");

											strcat ((char *) code_temp, (const char *) str);
										}
										else
										{
											sprintf ((char *) str, "%i", reg);
											strcat ((char *) code_temp, (const char *) str);
										}
									}

									code_line++;
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
									}

									strcpy ((char *) code[code_line], (const char *) code_temp);
									strcat ((char *) code[code_line], "\n");
								}

								continue;
							}

							// stpop  =========================================================================
							if (strcmp ((const char *) ast[level].expr[j][last_arg], "stpop") == 0 || strcmp ((const char *) ast[level].expr[j][last_arg], "s<") == 0)
							{
								// exp: (num1 num2 stpull)
								// (num1 num2 s<)

								if (get_var_is_const (ast[level].expr[j][last_arg - 1]) == 1)
								{
									printf ("error: line %lli: variable '%s' is constant!\n", linenum, ast[level].expr[j][last_arg - 1]);
									return (1);
								}

								for (e = 0; e < last_arg; e++)
								{
									if (checkdef (ast[level].expr[j][e]) != 0)
									{
										return (1);
									}
									if (getvartype (ast[level].expr[j][e]) == INTEGER)
                                    {
										target = get_regi (ast[level].expr[j][e]);
										if (target == -1)
										{
											// variable is not in register, load it

											reg2 = get_free_regi ();
											set_regi (reg2, ast[level].expr[j][e]);
											target = reg2;
										}

										if (getvartype_real (ast[level].expr[j][e]) == BYTE)
										{
											strcpy ((char *) code_temp, "stpopb ");
										}
										else
										{
											strcpy ((char *) code_temp, "stpopi ");
										}

										sprintf ((char *) str, "%i", target);
										strcat ((char *) code_temp, (const char *) str);
										strcat ((char *) code_temp, "\n");

										code_line++;
										if (code_line >= code_max_lines)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (3);
										}

										strcpy ((char *) code[code_line], (const char *) code_temp);

										strcpy ((char *) code_temp, "load ");
										strcat ((char *) code_temp, (const char *) ast[level].expr[j][e]);
										strcat ((char *) code_temp, ", 0, ");

										reg = get_free_regi ();

										sprintf ((char *) str, "%i", reg);
										strcat ((char *) code_temp, (const char *) str);
										strcat ((char *) code_temp, "\n");

										code_line++;
										if (code_line >= code_max_lines)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (3);
										}

										strcpy ((char *) code[code_line], (const char *) code_temp);

										// create new scope for setting the needed pull opcode:
										{
											S8 vartype ALIGN;
											vartype = getvartype_real (ast[level].expr[j][e]);
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
										if (code_line >= code_max_lines)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (3);
										}

										strcpy ((char *) code[code_line], (const char *) code_temp);

										reg = get_regi (ast[level].expr[j][e]);
										if (reg != -1)
										{
											// set old value of reg as empty
											set_regi (reg, (U1 *) "");
										}

										set_regi (target, ast[level].expr[j][e]);
									}

									if (getvartype (ast[level].expr[j][e]) == DOUBLE)
                                    {
										target = get_regd (ast[level].expr[j][e]);
										if (target == -1)
										{
											// variable is not in register, load it

											reg2 = get_free_regd ();
											set_regd (reg2, ast[level].expr[j][e]);
											target = reg2;
										}

										strcpy ((char *) code_temp, "stpopd ");
										sprintf ((char *) str, "%i", target);
										strcat ((char *) code_temp, (const char *) str);
										strcat ((char *) code_temp, "\n");

										code_line++;
										if (code_line >= code_max_lines)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (3);
										}

										strcpy ((char *) code[code_line], (const char *) code_temp);

										strcpy ((char *) code_temp, "load ");
										strcat ((char *) code_temp, (const char *) ast[level].expr[j][e]);
										strcat ((char *) code_temp, ", 0, ");

										reg = get_free_regd ();

										sprintf ((char *) str, "%i", reg);
										strcat ((char *) code_temp, (const char *) str);
										strcat ((char *) code_temp, "\n");

										code_line++;
							            if (code_line >= code_max_lines)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (3);
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
										if (code_line >= code_max_lines)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (3);
										}

										strcpy ((char *) code[code_line], (const char *) code_temp);

										reg = get_regd (ast[level].expr[j][e]);
										if (reg != -1)
										{
											// set old value of reg as empty
											set_regd (reg, (U1 *) "");
										}

										set_regd (target, ast[level].expr[j][e]);
									}
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
								if (last_arg >= 2)
								{
									// get variable types 
									if (getvartype_real (ast[level].expr[j][last_arg - 2]) > expression_var_type_max)
									{
										expression_var_type_max = getvartype_real (ast[level].expr[j][last_arg - 2]);
									}

									if (getvartype_real (ast[level].expr[j][last_arg - 1]) > expression_var_type_max)
									{
										expression_var_type_max = getvartype_real (ast[level].expr[j][last_arg - 1]);
									}
								}

								// check if expression is if op or not
								if (translate[t].assemb_op <= DIVD)
								{
									expression_if_op = 0;
								}
								if (translate[t].assemb_op >= ANDI && translate[t].assemb_op <= LSEQD)
								{
									if (expression_if_op == -1)
									{
										expression_if_op = 1;
									}
								} 

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

									case LOADL:
										// set label
										if (set_call_label (ast[level].expr[j][last_arg - 2]) != 0)
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
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
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
									if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
									{
										return (1);
									}

									if (get_var_is_const (ast[level].expr[j][last_arg - 1]) == 1)
									{
										printf ("error: line %lli: variable '%s' is constant!\n", linenum, ast[level].expr[j][last_arg - 1]);
										return (1);
									}

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
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
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
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
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
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
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
									if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
									{
										return (1);
									}

									if (get_var_is_const (ast[level].expr[j][last_arg - 1]) == 1)
									{
										printf ("error: line %lli: variable '%s' is constant!\n", linenum, ast[level].expr[j][last_arg - 1]);
										return (1);
									}

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
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
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
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
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
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
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
									if (checkdef (ast[level].expr[j][last_arg - 1]) != 0)
									{
										return (1);
									}

									if (get_var_is_const (ast[level].expr[j][last_arg - 1]) == 1)
								    {
									     printf ("error: line %lli: variable '%s' is constant!\n", linenum, ast[level].expr[j][last_arg - 1]);
									     return (1);
							     	}

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
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
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
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
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
									if (code_line >= code_max_lines)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (3);
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
                                            if (code_line >= code_max_lines)
                                            {
                                                printf ("error: line %lli: code list full!\n", linenum);
                                                return (3);
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
											if (code_line >= code_max_lines)
											{
												printf ("error: line %lli: code list full!\n", linenum);
												return (3);
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

							// LOADL check
							if (translate[t].assemb_op == LOADL)
							{
								// printf ("DEBUG: loadl...\n");

								// correct the code in assembly listing
								{
									S8 ALIGN i;
									S8 ALIGN j = 0;
									U1 reg[MAXSTRLEN];
									U1 comma = ',';
									U1 comma_count = 0;
									S8 slen ALIGN;
									S8 pos ALIGN = 0;
									U1 ch;
									slen = strlen_safe ((const char *) code[code_line], MAXLINELEN);
									for (i = 0; i < slen; i++)
									{
										if (code[code_line][i] == comma)
										{
											comma_count++;
										}
										if (comma_count == 2)
										{
											pos = i + 2;
											break;
										}
									}
									// get register after the second comma
									for (i = pos; i < slen; i++)
									{
										ch = code[code_line][i];
										if (ch != ' ' && ch != 10 && ch != 13)
										{
											reg[j] = ch;
											j++;
										}
									}
									reg[j] = '\0';		// set end of register number string

									// printf ("register: '%s'\n", reg);

									// search for register number in current temp assembly line
									pos = searchstr (code_temp, (U1 *) reg, 0, 0, TRUE);
									if (pos > -1)
									{
										code_temp[pos] = ',';
										code_temp[pos + 1] = ' ';

										// insert register into current temp code line end
										slen = strlen_safe ((const char *) reg, MAXLINELEN);
										j = pos + 2;
										for (i = 0; i < slen; i++)
										{
											code_temp[j] = reg[i];
											j++;
										}
										code_temp[j] = '\0';
									}

									// printf ("code_temp: '%s'\n", code_temp);
								}
							}

							code_line++;
							if (code_line >= code_max_lines)
							{
								printf ("error: line %lli: code list full!\n", linenum);
								return (3);
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
	slen = strlen_safe ((const char *) name, MAXLINELEN);

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
    FILE *fptr = NULL;
    U1 asmname[512] = "";
    S4 slen = 0, pos = 0;
    U1 rbuf[MAXSTRLEN + 1] = "";                        /* read-buffer for one line */
	U1 rbuf_orig[MAXSTRLEN + 1] = "";                        /* read-buffer for one line */
    char *read = 0;
	S8 ret ALIGN = 0;
	S2 function_call = 0;

	S8 code_lines ALIGN = 0;

    slen = strlen_safe ((const char *) name, MAXLINELEN);
    U1 ok = 0, err = 0;

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
			if (file_inside == 0)
			{
				// inside of main file add line:
				linenum++;
			}
			else
			{
				files[file_index].linenum++;
			}

            slen = strlen_safe ((const char *) rbuf, MAXLINELEN);
			if (slen == 0)
			{
				printf ("ERROR: line: %lli: length is 0 or too long!\n", linenum);
				return (1);
			}

			convtabs (rbuf);  /* convert the funny tabs into spaces! */
			// printf ("[ %s ]\n", rbuf);

	        repair_multi_spaces (rbuf);

			// EDIT
			if (save_compiler_line == 1)
			{
				if (slen > MAXLINELEN - 5)
				{
					printf ("error: can't save Brackets code in comment line!\n");
				}
				else
				{
					code_line++;
					if (code_line >= code_max_lines)
					{
						printf ("error: line %lli: code list full!\n", linenum);
						return (1);
					}

					strcpy ((char *) code[code_line], "\n// ");
					strcat ((char *) code[code_line], (const char *) rbuf);
					strcat ((char *) code[code_line], "\n") ;
				}
			}

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

				pos = searchstr (rbuf, (U1 *) FILENAME_START_SB, 0, 0, TRUE);
				if (pos != -1)
				{
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
					continue;
				}

				pos = searchstr (rbuf, (U1 *) FILENAME_END_SB, 0, 0, TRUE);
				if (pos != -1)
				{
					if (file_index > 0)
					{
						file_index--;
					}
                    if (file_index == 0)
					{
						file_inside = 0;
					}
					continue;
				}

				function_call = -1;
				pos = searchstr (rbuf, (U1 *) "call", 0, 0, TRUE);
				if (pos != -1)
				{
					function_call = pos;
				}
				if (function_call == -1)
				{
					pos = searchstr (rbuf, (U1 *) "!", 0, 0, TRUE);
					if (pos != -1)
					{
						if (pos < slen - 1)
						{
							if (rbuf[pos + 1] != '=')
							{
								// not a: !=
								function_call = pos;
							}
						}
					}
				}

				if (function_call != -1)
				{
					pos = searchstr (rbuf, (U1 *) "set", 0, 0, TRUE);
					if (pos == -1)
					{
						if (rbuf[function_call - 1] != ' ')
						{
							printf ("error: function call syntax error!\n");
							printf ("%s\n", rbuf);
							return (1);
						}
					}
				}

				// search for "@," array variable assign in more than one line:
				// @, 32Q, 10, 5,
				// @, 1234567890, 4, 3, ;
				
				pos = searchstr (rbuf, (U1 *) "@,", 0, 0, TRUE);
				if (pos != -1)
				{
					data_line++;
					if (data_line >= code_max_lines)
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
					if (code_line >= code_max_lines)
					{
						printf ("error: line %lli: code list full!\n", linenum);
						return (3);
					}

					strcpy ((char *) code[code_line], (const char *) rbuf);
				}
				else
				{
					if (error_multi_spaces == 1)
					{
						if (check_spaces (rbuf) != 0)
						{
							if (file_inside == 1)
							{
								printf ("file: %s: line: %lli\n", files[file_index].name, files[file_index].linenum);
							}
							printf ("error: found double spaces!\n");
							printf ("> %s\n", rbuf);
							err = 1;
						}
					}
					strcpy ((char *) rbuf_orig, (const  char *) rbuf);  // save original line!
					ret = parse_line (rbuf);
 					if (ret != 0)
					{
						if (file_inside == 1)
						{
							printf ("file: %s: line: %lli\n", files[file_index].name, files[file_index].linenum);
						}

						printf ("> %s\n", rbuf_orig);
						err = 1;

						//if (ret == 2 || ret == 3)
						if (ret == 3)
						{
							// ERROR brackets don't match or code list full
							fclose (fptr);
							return (err);
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
	printf ("l1com <file> [-a] [-lines] [max linenumber] [-wsetundef] [-wvarunused] [-wall] [-werror]\n");
	printf ("\nCompiler for Brackets, a programming language with brackets ;-)\n");
	printf ("%s", VM_VERSION_STR);
	printf ("%s\n", COPYRIGHT_STR);
	printf ("-wsetundef : warn on set variables without value defined\n");
	printf ("-wvarunused : warn on defined but unused variables\n");
	printf ("-wall : all -w warn flags turned on!\n");
	printf ("-werror : handle all warnings as ERRORs!\n");
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

	init_data_info_var ();

	U1 syscallstr[256] = "l1asm ";		// system syscall for assembler
	S8 ret ALIGN = 0;					// return value of assembler
	S8 arglen ALIGN = 0;
	S8 i ALIGN = 0;
	S8 str_len_arg ALIGN = 0;
	S8 str_len_assembler_args ALIGN = 0;

	U1 assembler_args[MAXSTRLEN] = "";

	S2 warn_unused_vars = 0;

	// compile time vars
 	struct timeval timer_start, timer_end;
 	F8 timer_double ALIGN = 0.0;

	FILE *finptr = NULL;
    FILE *flint_status = NULL;
    U1 flint_status_file[MAXSTRLEN + 1];
    S4 file_name_len = 0;
	U1 linter_status_str[MAXLINELEN + 1];
    char *linter = NULL;

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
					str_len_assembler_args = strlen_safe ((const char *) assembler_args, MAXLINELEN);
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
			            code_max_lines = atoll (av[i + 1]);
						printf ("max line len set to: %lli lines\n", code_max_lines);
					}
				}

				if (strcmp (av[i], "-sizes") == 0)
				{
					if (ac > i + 1)
					{
						strcat ((char *) assembler_args, "-sizes ");
						str_len_arg = strlen_safe (av[i + 1], MAXLINELEN);
						if (str_len_arg < MAXLINELEN - 7)
						{
							strcat ((char *) assembler_args, av[i + 1]);
							strcat ((char *) assembler_args, " ");
							str_len_assembler_args = strlen_safe ((const char *) assembler_args, MAXLINELEN);
							if (str_len_assembler_args < MAXLINELEN - 1)
							{
								str_len_arg = strlen_safe (av[i + 2], MAXLINELEN);
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
			if (arglen == 7)
			{
				if (strcmp (av[i], "-werror") == 0)
				{
					warnings_as_errors = 1;
				}
			}

			if (arglen == 10)
			{
				if (strcmp (av[i], "-wsetundef") == 0)
				{
					warn_set_not_def = 1;
				}
			}

			if (arglen == 11)
			{
				if (strcmp (av[i], "-wvarunused") == 0)
				{
					warn_unused_variables = 1;
				}
			}

			if (arglen == 8)
			{
				if (strcmp (av[i], "-wspaces") == 0)
				{
					error_multi_spaces = 1;
				}
			}

			if (arglen == 9)
			{
				if (strcmp (av[i], "-savecode") == 0)
				{
					save_compiler_line = 1;
				}
			}

			// check if -wall is set
			if (arglen == 5)
			{
				if (strcmp (av[i], "-wall") == 0)
				{
					warn_set_not_def = 1;
					warn_unused_variables = 1;
				}
			}
		}
	}

	file_name_len = strlen_safe (av[1], MAXSTRLEN);
    if (file_name_len >= MAXSTRLEN - 14)
    {
       // error can't open input file
		printf ("ERROR: input file name overflow: '%s' !\n", av[1]);
		exit (1);
	}

	gettimeofday (&timer_start, NULL);

	data = alloc_array_U1 (code_max_lines, MAXLINELEN);
	if (data == NULL)
	{
		printf ("\033[31merror: can't allocate %lli lines for data!\n", code_max_lines);
		printf ("[!] %s\033[0m\n\n", av[1]);
		cleanup ();
		exit (1);
	}

	code = alloc_array_U1 (code_max_lines, MAXLINELEN);
	if (code == NULL)
	{
		printf ("\033[31merror: can't allocate %lli lines for code!\n", code_max_lines);
		printf ("[!] %s\033[0m\n\n", av[1]);
		cleanup ();
		exit (1);
	}

	printf ("Allocated %lli lines for code and for data.\n", code_max_lines);

	// switch to red text
	printf ("\033[31m");

	init_data_info_var ();

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

	// check for main function label
	if (search_label ((U1 *) ":main") == 0)
	{
		printf ("\033[31mERROR! no main function defined!\n");
		cleanup ();
		exit (1);
	}

	if (warn_unused_variables == 1)
	{
		warn_unused_vars = get_unused_var ();
		if (warn_unused_vars == 1 && warnings_as_errors == 1)
		{
			printf ("\033[31mERROR: found defined but unused variables!\n");
			printf ("\033[0m\n");
			cleanup ();
			exit (1);
		}
	}

	if (do_check_linter == 1)
	{
		printf ("checking linter output...\n");

		// flint status file
		strcpy ((char *) flint_status_file, av[1]);
		strcat ((char *) flint_status_file, ".l1com.l1lint");

		flint_status = fopen ((const char *) flint_status_file, "r");
		if (flint_status == NULL)
		{
			// error can't open lint status file
			printf ("ERROR: can't open linter status file: '%s' !\n", flint_status_file);
		    cleanup ();
			exit (1);
		}

		linter = fgets_uni ((char *) linter_status_str, MAXLINELEN, flint_status);
		if (linter != NULL)
		{
			if (strcmp ((const char *) linter_status_str, "linter OK!\n") != 0)
			{
				printf ("\033[31merror: linter check not OK!\n");
				printf ("[!] %s\033[0m\n\n", av[1]);

				fclose (flint_status);
				cleanup ();
				exit (1);
			}
		}
		else
		{
			printf ("\033[31merror: can't open linter check file!\n");
			printf ("[!] %s\033[0m\n\n", av[1]);

			fclose (flint_status);
			cleanup ();
			exit (1);
		}
		fclose (flint_status);
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

	// get build time end
	gettimeofday (&timer_end, NULL);

	timer_double = (double) (timer_end.tv_usec - timer_start.tv_usec) / 1000000 + (double) (timer_end.tv_sec - timer_start.tv_sec);
	// timer_double = timer_double; 	// get seconds
	printf ("build in %.4lf seconds\n", timer_double);

	exit (ret);
}
