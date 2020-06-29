/*
 * This file parse-cont.c is part of L1vm.
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

//  l1com RISC compiler
//

#include "../include/global.h"

// protos var.c
S2 get_var_is_const (U1 *name);

// translate.h
#define MAXTRANSLATE 39

// register tracking functions
void set_regi (S4 reg, U1 *name);
void set_regd (S4 reg, U1 *name);
S4 get_regi (U1 *name);
S4 get_regd (U1 *name);
void init_registers (void);
S4 get_free_regi (void);
S4 get_free_regd (void);


// var.c
S2 checkdef (U1 *name);
S2 getvartype (U1 *name);
S2 getvartype_real (U1 *name);
S8 get_variable_is_array (U1 *name);

// assembly text output
extern U1 **data;
extern U1 **code;


extern struct opcode opcode[];
extern struct translate translate[];

extern S8 code_line ALIGN;
extern S8 line_len ALIGN;
extern S8 linenum ALIGN;

extern S8 label_ind ALIGN;
extern S8 call_label_ind ALIGN;

struct ast
{
	U1 expr[MAXEXPRESSION][MAXARGS][MAXLINELEN];
	S4 expr_max;
	S4 expr_args[MAXEXPRESSION]; 						// number of arguments in expression
	S4 expr_reg[MAXEXPRESSION];							// registers of expression calculations = target registers
	U1 expr_type[MAXEXPRESSION];						// type of register (INTEGER or DOUBLE)
};

extern struct ast ast[MAXBRACKETLEVEL];
extern S8 ast_level ALIGN;

S4 load_variable_int (S4 level, S4 arg, S4 j)
{
	S4 target;
	S4 reg2, reg3;
	U1 str[MAXLINELEN];
	U1 code_temp[MAXLINELEN];

	if (checkdef (ast[level].expr[j][arg]) != 0)
	{
		return (-1);
	}
	if (getvartype_real (ast[level].expr[j][arg]) == QUADWORD)
	{
		// load quadword

		target = get_regi (ast[level].expr[j][arg]);
		if (target == -1)
		{
			// variable is not in register, load it

			reg2 = get_free_regi ();
			set_regi (reg2, ast[level].expr[j][arg]);
			// write code loada

			code_line++;
			if (code_line >= line_len)
			{
				printf ("error: line %lli: code list full!\n", linenum);
				return (-1);
			}

			strcpy ((char *) code[code_line], "loada ");
			strcat ((char *) code[code_line], (const char *) ast[level].expr[j][arg]);
			strcat ((char *) code[code_line], ", 0, ");
			sprintf ((char *) str, "%i", reg2);
			strcat ((char *) code[code_line], (const char *) str);
			strcat ((char *) code[code_line], "\n");
			target = reg2;
		}
	}
	else
	{
		// load other variables

		strcpy ((char *) code_temp, "load ");
		strcat ((char *) code_temp, (const char *) ast[level].expr[j][arg]);
		strcat ((char *) code_temp, ", 0, ");

		reg2 = get_free_regi ();
		// set_regd (reg, (U1 *) ast[level].expr[j][last_arg - 1]);

		sprintf ((char *) str, "%i", reg2);
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

		if (checkdef (ast[level].expr[j][arg]) != 0)
		{
			return (1);
		}

		if (getvartype_real (ast[level].expr[j][arg]) == BYTE)
		{
			strcpy ((char *) code_temp, "pushb ");
		}

		if (getvartype_real (ast[level].expr[j][arg]) == WORD)
		{
			strcpy ((char *) code_temp, "pushw ");
		}

		if (getvartype_real (ast[level].expr[j][arg]) == DOUBLEWORD)
		{
			strcpy ((char *) code_temp, "pushdw ");
		}

		if (getvartype_real (ast[level].expr[j][arg]) == QUADWORD)
		{
			strcpy ((char *) code_temp, "pushqw ");
		}

		reg3 = get_free_regi ();
		set_regi (reg3, ast[level].expr[j][arg]);

		sprintf ((char *) str, "%i", reg2);
		strcat ((char *) code_temp, (const char *) str);
		strcat ((char *) code_temp, ", 0");
		strcat ((char *) code_temp, ", ");
		sprintf ((char *) str, "%i", reg3);
		strcat ((char *) code_temp, (const char *) str);
		strcat ((char *) code_temp, "\n");

		code_line++;
		if (code_line >= line_len)
		{
			printf ("error: line %lli: code list full!\n", linenum);
			return (1);
		}

		strcpy ((char *) code[code_line], (const char *) code_temp);
		target = reg3;
	}
	return (target);
}

S4 load_variable_double (S4 level, S4 arg, S4 j)
{
	S4 target;
	S4 reg2;
	U1 str[MAXLINELEN];

	if (getvartype_real (ast[level].expr[j][arg]) == DOUBLE)
	{
		target = get_regd (ast[level].expr[j][arg]);
		if (target == -1)
		{
			// variable is not in register, load it

			reg2 = get_free_regd ();
			set_regd (reg2, ast[level].expr[j][arg]);
			// write code loada

			code_line++;
			if (code_line >= line_len)
			{
				printf ("error: line %lli: code list full!\n", linenum);
				return (-1);
			}

			strcpy ((char *) code[code_line], "loadd ");
			strcat ((char *) code[code_line], (const char *) ast[level].expr[j][arg]);
			strcat ((char *) code[code_line], ", 0, ");
			sprintf ((char *) str, "%i", reg2);
			strcat ((char *) code[code_line], (const char *) str);
			strcat ((char *) code[code_line], "\n");
			target = reg2;
		}
	}
	else
	{
		printf ("load_variable_double: error: not double type: '%s' line: %lli\n", ast[level].expr[j][arg], linenum);
		return (-1);
	}
	return (target);
}

S2 parse_continous (void)
{
	S4 level, j, last_arg,  t, reg, reg2, target, target1, target2;
	U1 ok;
	S4 arg;
	U1 str[MAXLINELEN];
	U1 code_temp[MAXLINELEN];

	U1 operator = 0;
	U1 finished = 0;
	U1 reg_int = 0;

	S4 target_reg = 0;

	// walking the continous AST
	for (level = ast_level; level >= 0; level--)
	{
		#if DEBUG
		printf ("level: %i, expr max: %i\n", level, ast[level].expr_max);
		#endif

		if (ast[level].expr_max > -1)
		{
			arg = 0;
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
					last_arg = ast[level].expr_args[j];
					if (last_arg >= 0)
					{
						// operator not found in list!

						// check if = operator

						if (strcmp ((const char *) ast[level].expr[j][last_arg], "=") == 0 && last_arg >= 4)
						{
							while (finished == 0)
							{
								// do variable assign
								// get variable

								if (arg == 0)
								{
									// first variable =========================

									if (checkdef (ast[level].expr[j][arg]) != 0)
									{
										return (1);
									}

									if (getvartype_real (ast[level].expr[j][arg]) != DOUBLE)
									{
										target1 = load_variable_int (level, arg, j);
										if (target1 == -1)
										{
											return (1);
										}
										reg_int = 1;
									}
									else
									{
										target1 = load_variable_double (level, arg, j);
										if (target1 == -1)
										{
											return (1);
										}
										reg_int = 0;
									}

									ok = 0;
									for (t = 0; t < MAXTRANSLATE; t++)
									{
										if (strcmp ((const char *) ast[level].expr[j][arg + 1], (const char *) translate[t].op) == 0)
										{
											ok = 1;
											break;
										}
									}

									if (ok == 0)
									{
										printf ("error: line %lli: unknown operator!\n", linenum);
										return (1);
									}
									operator = t;


									// second variable ========================

									if (checkdef (ast[level].expr[j][arg + 2]) != 0)
									{
										return (1);
									}

									if (getvartype_real (ast[level].expr[j][arg + 2]) != DOUBLE)
									{
										target2 = load_variable_int (level, arg + 2, j);
										if (target2 == -1)
										{
											return (1);
										}
									}
									else
									{
										target2 = load_variable_double (level, arg + 2, j);
										if (target2 == -1)
										{
											return (1);
										}
									}

									// write code
									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}

									// write assembler operator code

									// write opcode name to code_temp
									strcpy ((char *) code_temp, (const char *) opcode[translate[operator].assemb_op].op);
									strcat ((char *) code_temp, " ");

									sprintf ((char *) str, "%i", target1);
									strcat ((char *) code_temp, (const char *) str);
									strcat ((char *) code_temp, ", ");
									sprintf ((char *) str, "%i", target2);
									strcat ((char *) code_temp, (const char *) str);
									strcat ((char *) code_temp, ", ");

									// set temp variable as target for opcode

									if (reg_int)
									{
										target_reg = get_free_regi ();
										set_regi (target_reg, (U1 *) "cont_temp");
									}
									else
									{
										target_reg = get_free_regd ();
										set_regd (target_reg, (U1 *) "cont_temp");
									}

									sprintf ((char *) str, "%i", target_reg);
									strcat ((char *) code_temp, (const char *) str);
									strcat ((char *) code_temp, "\n");

									// write code
									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}
									strcpy ((char *) code[code_line], (const char *) code_temp);

									arg = 3;
								}
								else
								{
									// get next operator

									ok = 0;
									for (t = 0; t < MAXTRANSLATE; t++)
									{
										if (strcmp ((const char *) ast[level].expr[j][arg], (const char *) translate[t].op) == 0)
										{
											ok = 1;
											break;
										}
									}

									if (ok == 0)
									{
										printf ("error: line %lli: unknown operator!\n", linenum);
										return (1);
									}
									operator = t;

									// get next variable

									if (getvartype_real (ast[level].expr[j][arg + 1]) != DOUBLE)
									{
										target1 = load_variable_int (level, arg + 1, j);
										if (target1 == -1)
										{
											return (1);
										}
										reg_int = 1;
									}
									else
									{
										target1 = load_variable_double (level, arg + 1, j);
										if (target1 == -1)
										{
											return (1);
										}
										reg_int = 0;
									}

									// write opcode name to code_temp
									strcpy ((char *) code_temp, (const char *) opcode[translate[operator].assemb_op].op);
									strcat ((char *) code_temp, " ");

									sprintf ((char *) str, "%i", target1);
									strcat ((char *) code_temp, (const char*) str);
									strcat ((char *) code_temp, ", ");
									sprintf ((char *) str, "%i", target_reg);
									strcat ((char *) code_temp, (const char*) str);
									strcat ((char *) code_temp, ", ");

									// set temp variable as target for opcode

									sprintf ((char *) str, "%i", target_reg);
									strcat ((char *) code_temp, (const char*) str);
									strcat ((char *) code_temp, "\n");


									// write code
									code_line++;
									if (code_line >= line_len)
									{
										printf ("error: line %lli: code list full!\n", linenum);
										return (1);
									}
									strcpy ((char *) code[code_line], (const char *) code_temp);

									arg = arg + 2;
								}

								if (arg == last_arg - 1)
								{
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

										target = reg;

										code_line++;
										if (code_line >= line_len)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (1);
										}

										strcpy ((char *) code[code_line], (const char *) code_temp);

										strcpy ((char *) code_temp, "pulld ");
										sprintf ((char *) str, "%i", target_reg);
										strcat ((char *) code_temp, (const char *) str);
										strcat ((char *) code_temp, ", ");
										sprintf ((char *) str, "%i", target);
										strcat ((char *) code_temp, (const char *) str);
										strcat ((char *) code_temp, ", 0\n");

										code_line++;
										if (code_line >= line_len)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (1);
										}

										strcpy ((char *) code[code_line], (const char *) code_temp);

										{
											S4 reset_reg = 1;
											while (reset_reg)
											{
												reg2 = get_regd (ast[level].expr[j][last_arg - 1]);
												if (reg2 != -1)
												{
													// set old value of reg2 as empty
													set_regd (reg2, (U1 *) "");
												}
												else
												{
													reset_reg = 0;
												}
											}
										}
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

										target = reg;

										code_line++;
										if (code_line >= line_len)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (1);
										}

										strcpy ((char *) code[code_line], (const char *) code_temp);

										set_regi (reg, (U1 *) "temp");

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

										sprintf ((char *) str, "%i", target_reg);
										strcat ((char *) code_temp, (const char *) str);
										strcat ((char *) code_temp, ", ");
										sprintf ((char *) str, "%i", target);
										strcat ((char *) code_temp, (const char *) str);
										strcat ((char *) code_temp, ", 0\n");

										code_line++;
										if (code_line >= line_len)
										{
											printf ("error: line %lli: code list full!\n", linenum);
											return (1);
										}

										strcpy ((char *) code[code_line], (const char *) code_temp);

										{
											S4 reset_reg = 1;
											while (reset_reg)
											{
												reg2 = get_regi (ast[level].expr[j][last_arg - 1]);
												if (reg2 != -1)
												{
													// set old value of reg2 as empty
													set_regi (reg2, (U1 *) "");
												}
												else
												{
													reset_reg = 0;
												}
											}
										}
									}
									finished = 1;
								}
							}
						}
						else
						{
							printf ("error: line %lli: missing '=' in expression!\n", linenum);
							return (1);
						}
					}
				}
			}
		}
	}
	return (0);
}
