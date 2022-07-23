/*
 * This file parse-rpolish.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2021
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

// parse reverse polish math expressions
// based on a code by: https://www.tutorialspoint.com/data_structures_algorithms/expression_parsing_using_statck.htm


#include "../include/global.h"

size_t strlen_safe (const char * str, int maxlen);

// protos var.c
S2 get_var_is_const (U1 *name);

// translate.h
#define MAXTRANSLATE 39

//int stack
#define MAX_STACK 256
// #define MAXLINELEN 256

U1 stack_ob[MAX_STACK][MAXSTRLEN];
S2 stack_ob_int = -1;

S2 stack_reg[MAX_STACK];
int stack_reg_int = -1;

//int stack
int stack[MAXSTRLEN];
int top_int = -1;

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


// string.c
size_t strlen_safe (const char * str, int maxlen);
S2 searchstr (U1 *str, U1 *srchstr, S2 start, S2 end, U1 case_sens);

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

// don't use pull opcodes to optimize chained math expressions in "parse-rpolish.c" math expressions
extern U1 no_var_pull;


S4 load_variable_int (U1 *var)
{
	S4 target;
	S4 reg2, reg3;
	U1 str[MAXSTRLEN];
	U1 code_temp[MAXSTRLEN];

	if (checkdef (var) != 0)
	{
		return (-1);
	}
	if (getvartype_real (var) == QUADWORD)
	{
		// load quadword

		target = get_regi (var);
		if (target == -1)
		{
			// variable is not in register, load it

			reg2 = get_free_regi ();
			set_regi (reg2, var);
			// write code loada

			code_line++;
			if (code_line >= line_len)
			{
				printf ("error: line %lli: code list full!\n", linenum);
				return (-1);
			}

			strcpy ((char *) code[code_line], "loada ");
			strcat ((char *) code[code_line], (const char *) var);
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
		strcat ((char *) code_temp, (const char *) var);
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

		if (checkdef (var) != 0)
		{
			return (1);
		}

		if (getvartype_real (var) == BYTE)
		{
			strcpy ((char *) code_temp, "pushb ");
		}

		if (getvartype_real (var) == WORD)
		{
			strcpy ((char *) code_temp, "pushw ");
		}

		if (getvartype_real (var) == DOUBLEWORD)
		{
			strcpy ((char *) code_temp, "pushdw ");
		}

		if (getvartype_real (var) == QUADWORD)
		{
			strcpy ((char *) code_temp, "pushqw ");
		}

		reg3 = get_free_regi ();
		set_regi (reg3, var);

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

S4 load_variable_double (U1 *var)
{
	S4 target;
	S4 reg2;
	U1 str[MAXSTRLEN];

	if (getvartype_real (var) == DOUBLE)
	{
		target = get_regd (var);
		if (target == -1)
		{
			// variable is not in register, load it

			reg2 = get_free_regd ();
			set_regd (reg2, var);
			// write code loada

			code_line++;
			if (code_line >= line_len)
			{
				printf ("error: line %lli: code list full!\n", linenum);
				return (-1);
			}

			strcpy ((char *) code[code_line], "loadd ");
			strcat ((char *) code[code_line], (const char *) var);
			strcat ((char *) code[code_line], ", 0, ");
			sprintf ((char *) str, "%i", reg2);
			strcat ((char *) code[code_line], (const char *) str);
			strcat ((char *) code[code_line], "\n");
			target = reg2;
		}
	}
	else
	{
		printf ("load_variable_double: error: not double type: '%s' line: %lli\n", var, linenum);
		return (-1);
	}
	return (target);
}


// variable stack handling
S2 push_stack (U1 *item)
{
	if (stack_ob_int < MAX_STACK - 1)
	{
   		strcpy ((char *) stack_ob[++stack_ob_int], (const char *) item);
		return (0);
	}
	else
	{
		printf ("error: line %lli: max variable stack overflow!\n", linenum);
		return (1);
	}
}

U1 *pop_stack ()
{
	// printf ("pop_stack: stack_ob_int: %i\n", stack_ob_int);
	if (stack_ob_int > -1)
	{
		// printf ("stack OK!\n");
   		return (stack_ob[stack_ob_int--]);
	}
	else
	{
		printf ("error: line %lli: no variable on stack, stack empty!\n", linenum);
		return (NULL);
	}
}

// register stack handling
S2 push_reg_stack (S2 reg)
{
	if (stack_reg_int < MAX_STACK - 1)
	{
		stack_reg[++stack_reg_int] = reg;
		return (0);
	}
	else
	{
		printf ("error: line %lli: max stack reg overflow!\n", linenum);
		return (1);
	}
}

S2 pop_reg_stack ()
{
	if (stack_reg_int > -1)
	{
		return (stack_reg[stack_reg_int--]);
	}
	else
	{
		printf ("error: line %lli: no reg on stack, stack empty!\n", linenum);
		return (-1);
	}
}

S2 get_target_var (U1 *line, U1 *varname, S2 len)
{
	S2 j;
	size_t i = 0;
	S2 calc_begin;	// start of math expression
	S2 ok = 0;
	U1 ch;

	j = 0;
	while (ok == 0)
	{
		ch = line[i];
		// printf ("get_target_var: ch: '%c'\n", ch);
		if (ch != ' ' && ch != '=' && ch != '{')
		{
			varname[j] = ch;
			// printf ("get_target_var: varname ch: '%c'\n", ch);
			if (j < len - 1)
			{
				j++;
			}
			else
			{
				// return variable overflow!!!
				return (-1);
			}
		}
		else
		{
			varname[j] = '\0';
		}
		if (ch == '=')
		{
			// got begin of calculation
			// printf ("got: =\n");
			calc_begin = i + 1;
			ok = 1;
		}
		if (i < strlen_safe ((const char *) line, MAXLINELEN) - 1)
		{
			i++;
		}
		else
		{
			// error line end
			return (-1);
		}
	}
	return (calc_begin);
}

//check whether the symbol is operator?
S2 isOperator (char symbol)
{
   switch (symbol)
   {
      case '+':
      case '-':
      case '*':
      case '/':
	  case '%':
	  case '<':
	  case 'L':
	  case '>':
	  case 'G':
	  case 'E':
	  case 'N':
	  case '&':
      case '^':
	  case '|':
	  case 'A':
	  case 'O':
	  case '$':
	  case '@':
      case '(':
      case ')':
	  case '{':
	  case '}':
         return 1;
      break;
         default:
         return 0;
   }
}


//returns precedence of operators
S2 precedence (char symbol)
{
   switch (symbol) {
      	case '+':
      	case '-':
        	return 2;
         	break;

      	case '*':
     	case '/':
		case '%':
     	  	return 3;
       		break;

		case '$':
		case '@':
			return 4;
			break;

		case '<':
		case 'L':
		case '>':
		case 'G':
			return 5;
			break;

		case 'E':
		case 'N':
			return 6;

     	 case '&':
       		return 7;
         	break;

		case '^':
			return 8;
			break;

		case '|':
			return 9;
			break;

		case 'A':
			return 10;
			break;

		case 'O':
			return 11;
			break;

      	case '(':
      	case ')':
      	case '#':
        	return 1;
         	break;
   }
   return (0);
}

void replace_symbols (U1 *linestr)
{
	U1 run_loop = 1;
	S2 i, pos;
	S2 maxsymb = 8;

	U1 symbstr[8][3] = { "==", "!=", "<=", ">=", "&&", "||", "<|", ">|" };
	U1 repstr[8] = { 'E', 'N', 'L', 'G', 'A', 'O', '$', '@' };

	for (i = 0; i < maxsymb; i++)
	{
		run_loop = 1;
		while (run_loop)
		{
			pos = searchstr (linestr, symbstr[i], 0, 0, 0);
			if (pos >= 0)
			{
				// found symbol on position pos!
				// overwrite it with single char in repstr 

				linestr[pos] = repstr[i];
				linestr[pos + 1] = ' ';
			}
			else 
			{
				// search symbol not found, set run_loop = 0;
				run_loop = 0;
			}
		}
	}
}

//converts infix expression to postfix
S2 convert (U1 infix[], U1 postfix[])
{
	S2 symbol, j = 0, startexp = 0;
	size_t i, pos = 0;
	U1 *buf;
	U1 buf_push[3];

	stack_ob[++stack_ob_int][0] = '#';

	// printf ("convert: '%s'\n", infix);

	// call replace symbols 
	replace_symbols (infix);

	// printf ("convert: replaced: '%s'\n", infix);

	for (i = 0; i < strlen_safe ((const char *) infix, MAXLINELEN); i++)
	{
		postfix[j] = infix[i];
		if (infix[i] == '=')
		{
			startexp = i + 1;
		}
		if (infix[i] == '}')
		{
			// found end of expression, remove it
			infix[i] = '\0';
			break;
		}
		j++;
	}
	if (startexp == 0)
	{
		// error no assign = symbol found!!
		return (1);
	}
	j = startexp;

	if (infix[startexp] == ' ')
	{
		startexp++;
	}
	// printf ("convert: start: %lli\n", startexp);
	// printf ("convert: char: '%c'\n", infix[startexp]);

	for (i = startexp; i < strlen_safe ((const char *) infix, MAXLINELEN); i++)
	{
		symbol = infix[i];
		// printf ("DEBUG: symbol: %c\n", symbol);
		if (isOperator (symbol) == 0)
		{
			 postfix[j] = symbol;
			 j++;
		 }
		 else
		 {
         	if (symbol == '(')
		 	{
				buf_push[0] = symbol;
				buf_push[1] = '\0';
        		if (push_stack (buf_push) == 1)
				{
					// error
					return (1);
				}
         	}
		 	else
		 	{
            	if (symbol == ')')
				{
               		while (stack_ob[stack_ob_int][0] != '(')
			   		{
						buf = (U1 *) pop_stack ();
						if (buf == NULL)
						{
							// error
							return (1);
						}

						// printf ("DEBUG: buf: '%s'\n", buf);

						postfix[j] = ' ';
						j++;
						for (pos = 0; pos < strlen_safe ((const char *) buf, MAXLINELEN); pos++)
						{
							postfix[j] = buf[pos];
							j++;
						}
						postfix[j] = ' ';
						j++;
	               	}
	               if (pop_stack () == NULL)	// pop out (
				   {
					   // error
					   return (1);
				   }
            	}
				else
				{
					// printf ("DEBUG: stack_ob_int: %i\n", stack_ob_int);
               		if (precedence (symbol) > precedence (stack_ob[stack_ob_int][0]))
				   	{
						buf_push[0] = symbol;
						buf_push[1] = '\0';
						if (push_stack (buf_push) == 1)
						{
							// error
							return (1);
						}
               		}
				   	else
			   		{
						while (precedence (symbol) <= precedence (stack_ob[stack_ob_int][0]))
						{
							buf = (U1 *) pop_stack ();
							if (buf == NULL)
							{
								// error
								return (1);
							}
							for (pos = 0; pos < strlen_safe ((const char *) buf, MAXLINELEN); pos++)
							{
								postfix[j] = buf[pos];
								j++;
							}
                  		}
						buf_push[0] = symbol;
						buf_push[1] = '\0';
						push_stack (buf_push);
					}
            	}
         	}
      	}
	}
   while (stack_ob[stack_ob_int][0] != '#')
   {
	  buf = (U1 *) pop_stack ();
 	  if (buf == NULL)
 	  {
 		  // error
 		  return (1);
 	  }
	  postfix[j] = ' ';
	  j++;
	  for (pos = 0; pos < strlen_safe ((const char *) buf, MAXLINELEN); pos++)
	  {
		  postfix[j] = buf[pos];
		  j++;
	  }
   }

   postfix[j] = '\0'; //null terminate string.
   return (0);
}

S2 check_rpn_expression (U1 *postfix)
{
	S2 i;
	U1 ch;
	S2 len;

	U1 found_equal = 0;
	U1 found_op = 0;
	U1 found_var = 0;
	S2 ret = 0;

	len = strlen_safe ((const char *) postfix, MAXLINELEN);
	for (i = 0; i < len; i++)
	{
		ch = postfix[i];

		if (ch == '=')
		{
			found_equal = 1;
		}
		else
		{
			if (isOperator (ch) == 1)
			{
				found_op = 1;
			}
			if (ch != ' ' && isOperator (ch) == 0)
			{
				found_var = 1;
			}
		}
	}
	if (found_equal == 0)
	{
		printf ("error: line %lli: no equal sign found in expression!\n", linenum);
		ret = 1;
	}
	if (found_var == 0)
	{
		printf ("error: line %lli: no variable found in expression!\n", linenum);
		ret = 1;
	}
	return (0);
}

// evaluates reverse polish postfix expression
S2 parse_rpolish (U1 *postfix)
{
	U1 ch;
	S2 i = 0;
	S2 pos = 0;
	S2 get_var = 0;
	S2 parse = 1;
	U1 buf[MAXSTRLEN];

   	S2 reg = 0;
	S2 reg_int = 0;

	S2 target, target_reg;
	S2 reg1, reg2;
	U1 str[MAXSTRLEN];
	U1 code_temp[MAXSTRLEN];

	U1 target_var[MAXSTRLEN];
	S2 target_var_len = MAXLINELEN - 1;
	U1 target_var_type = UNKNOWN;
	U1 expression_var_type_max = UNKNOWN;

	S2 math_exp_begin;
	U1 found_op = 0;			// set to true if found operator

	// get target var name
	math_exp_begin = get_target_var (postfix, target_var, target_var_len);
	if (math_exp_begin == -1)
	{
		// printf ("error: line %lli: no '=' variable assign found!\n", linenum);
		return (1);
	}

	target_var_type = getvartype_real (target_var);

	i = math_exp_begin;

	// call replace symbols 
	replace_symbols (postfix);

	// check expression
	if (check_rpn_expression (postfix) == 1)
	{
		return (1);
	}

	// printf ("postfix: '%s'\n", postfix);

	while (parse == 1)
	{
       ch = postfix[i];
       // printf ("ch new inp: '%c'\n", ch);
       if (ch == '\0')
       {
           // end of string
           parse = 0;
		   break;
       }

	   if (ch == '}')
	   {
		   // end of string
		   parse = 0;
		   break;
	   }

       if (ch != ' ' && isOperator (ch) == 0)
       {
           // ch is not space or operator, get variable name
           pos = 0;
           // printf ("ch start: '%c'\n", ch);
           get_var = 0;
           while (get_var == 0)
           {
			   // printf ("input char: '%c'\n", ch);
               if (ch == ' ' || ch == '}')
               {
				   // printf ("adding zero end\n");
                   buf[pos] = '\0';
                   get_var = 1;
				   if (ch == '}')
				   {
					   parse = 0;
				   }
               }
               else
               {
                   buf[pos] = ch;
                   pos++;

                   // printf ("ch var: '%c'\n", ch);
               }
               i++;
               ch = postfix[i];
           }
           if (push_stack (buf) != 0)
		   {
			   return (1);
		   }
           // printf ("variable: '%s'\n", buf);
		   // printf ("stack_ob_int: %i\n", stack_ob_int);

		   if (checkdef (buf) != 0)
		   {
			   return (1);
		   }

			// set to higher bits, if variable has more bytes
			if (getvartype_real (buf) > expression_var_type_max)
			{
				expression_var_type_max = getvartype_real (buf);
	   		}

		   if (getvartype_real (buf) != DOUBLE)
		   {
			   target = load_variable_int (buf);
			   if (target == -1)
			   {
				   return (1);
			   }
			   reg_int = 1;
		   }
		   else
		   {
			   target = load_variable_double (buf);
			   if (target == -1)
			   {
				   return (1);
			   }
			   reg_int = 0;
		   }

		   if (push_reg_stack (target) != 0)
		   {
			   return (1);
		   }

           i--;
       }
       else
       {
           if (isOperator (ch) == 1)
           {
			   // printf ("got operator: %c\n", ch);
               // got operator
			   /*
			   strptr = (U1 *) pop_stack ();
			   if (strptr == NULL)
			   {
				   return (1);
			   }
			   strcpy ((char *) operand1, (const char *) strptr);
			   */

			  found_op = 1;

			   reg2 = pop_reg_stack ();
			   if (reg2 == -1)
			   {
				   return (1);
			   }

			   /*
			   strptr = (U1 *) pop_stack ();
			   if (strptr == NULL)
			   {
				   return (1);
			   }
			   strcpy ((char *) operand2, (const char *) strptr);
			   */

			   reg1 = pop_reg_stack ();
			   if (reg1 == -1)
			   {
				   return (1);
			   }

			   // printf ("reg stack popped!\n");

               switch (ch)
			   {
				   case '+':
				   		if (reg_int == 1)
						{
							// write opcode name to code_temp
							strcpy ((char *) code_temp, (const char *) opcode[ADDI].op);
						}
						else
						{
							// write opcode name to code_temp
							strcpy ((char *) code_temp, (const char *) opcode[ADDD].op);
						}
						break;

					case '-':
	 				   	if (reg_int == 1)
	 					{
	 						// write opcode name to code_temp
	 						strcpy ((char *) code_temp, (const char *) opcode[SUBI].op);
	 					}
	 					else
	 					{
	 						// write opcode name to code_temp
	 						strcpy ((char *) code_temp, (const char *) opcode[SUBD].op);
	 					}
	 					break;

					case '*':
	 				   	if (reg_int == 1)
	 					{
	 						// write opcode name to code_temp
	 						strcpy ((char *) code_temp, (const char *) opcode[MULI].op);
	 					}
	 					else
	 					{
	 						// write opcode name to code_temp
	 						strcpy ((char *) code_temp, (const char *) opcode[MULD].op);
	 					}
	 					break;

					case '/':
	 				   	if (reg_int == 1)
	 					{
	 						// write opcode name to code_temp
	 						strcpy ((char *) code_temp, (const char *) opcode[DIVI].op);
	 					}
	 					else
	 					{
	 						// write opcode name to code_temp
	 						strcpy ((char *) code_temp, (const char *) opcode[DIVD].op);
	 					}
	 					break;

					case 'E':
						if (reg_int == 1)
	 					{
	 						// write opcode name to code_temp
	 						strcpy ((char *) code_temp, (const char *) opcode[EQI].op);
	 					}
	 					else
	 					{
	 						// write opcode name to code_temp
	 						strcpy ((char *) code_temp, (const char *) opcode[EQD].op);
	 					}
	 					break;

					case 'N':
						if (reg_int == 1)
	 					{
	 						// write opcode name to code_temp
	 						strcpy ((char *) code_temp, (const char *) opcode[NEQI].op);
	 					}
	 					else
	 					{
	 						// write opcode name to code_temp
	 						strcpy ((char *) code_temp, (const char *) opcode[NEQD].op);
	 					}
	 					break;

					case '<':
						if (reg_int == 1)
	 					{
	 						// write opcode name to code_temp
	 						strcpy ((char *) code_temp, (const char *) opcode[LSI].op);
	 					}
	 					else
	 					{
	 						// write opcode name to code_temp
	 						strcpy ((char *) code_temp, (const char *) opcode[LSD].op);
	 					}
	 					break;
					
					case '>':
						if (reg_int == 1)
	 					{
	 						// write opcode name to code_temp
	 						strcpy ((char *) code_temp, (const char *) opcode[GRI].op);
	 					}
	 					else
	 					{
	 						// write opcode name to code_temp
	 						strcpy ((char *) code_temp, (const char *) opcode[GRD].op);
	 					}
	 					break;

					case 'L':
						if (reg_int == 1)
	 					{
	 						// write opcode name to code_temp
	 						strcpy ((char *) code_temp, (const char *) opcode[LSEQI].op);
	 					}
	 					else
	 					{
	 						// write opcode name to code_temp
	 						strcpy ((char *) code_temp, (const char *) opcode[LSEQD].op);
	 					}
	 					break;

					case 'G':
						if (reg_int == 1)
	 					{
	 						// write opcode name to code_temp
	 						strcpy ((char *) code_temp, (const char *) opcode[GREQI].op);
	 					}
	 					else
	 					{
	 						// write opcode name to code_temp
	 						strcpy ((char *) code_temp, (const char *) opcode[GREQD].op);
	 					}
	 					break;

					case 'A':
	 					// write opcode name to code_temp
	 					strcpy ((char *) code_temp, (const char *) opcode[ANDI].op);
	 					break;

					case 'O':
	 					// write opcode name to code_temp
	 					strcpy ((char *) code_temp, (const char *) opcode[ORI].op);
	 					break;

					case '&':
	 					// write opcode name to code_temp
	 					strcpy ((char *) code_temp, (const char *) opcode[BANDI].op);
	 					break;

					case '|':
	 					// write opcode name to code_temp
	 					strcpy ((char *) code_temp, (const char *) opcode[BORI].op);
	 					break;

					case '^':
	 					// write opcode name to code_temp
	 					strcpy ((char *) code_temp, (const char *) opcode[BXORI].op);
	 					break;

					case '%':
	 					// write opcode name to code_temp
	 					strcpy ((char *) code_temp, (const char *) opcode[MODI].op);
	 					break;

					case '$':
	 					// write opcode name to code_temp
	 					strcpy ((char *) code_temp, (const char *) opcode[SMULI].op);
	 					break;

					case '@':
	 					// write opcode name to code_temp
	 					strcpy ((char *) code_temp, (const char *) opcode[SDIVI].op);
	 					break;
				}

				strcat ((char *) code_temp, " ");

				sprintf ((char *) str, "%i", reg1);
				strcat ((char *) code_temp, (const char *) str);
				strcat ((char *) code_temp, ", ");
				sprintf ((char *) str, "%i", reg2);
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

				if (push_reg_stack (target_reg) != 0)
	 		   	{
	 				return (1);
	 		   	}
           }
       }
       i++;
   }

	if (found_op == 0)
	{
		// no operator found, was assign expression:
		// {ret = foo }

		target_reg = target;
		// printf ("no OP: target= %lli\n", target_reg);		
	}

	// assign target reg to target variable

	// assign to normal variable ==============

	if (checkdef (target_var) != 0)
	{
		return (1);
	}

	// check if variable is constant
	if (get_var_is_const (target_var) == 1)
	{
		printf ("error: line %lli: variable '%s' is constant!\n", linenum, target_var);
		return (1);
	}

	{
		U1 run_unset_loop = 1;
		S8 reg ALIGN;
		if (no_var_pull == 1)
		{
			// use no variable pull set target var into register
			if (getvartype (target_var) == DOUBLE)
			{
				while (run_unset_loop)
				{
					reg = get_regd (target_var);
					if (reg != -1)
					{
						// unset old register entry of target var
						set_regd (reg, (U1 *) "");
					}
					else 
					{
						run_unset_loop = 0;
					}
				}
				// set target variable register
				set_regd (target_reg, (U1 *) target_var);
			}
			
			if (getvartype (target_var) == INTEGER)
			{
				while (run_unset_loop)
				{
					reg = get_regi (target_var);
					if (reg != -1)
					{
						// unset old register entry of target var
						set_regi (reg, (U1 *) "");
					}
					else 
					{
						run_unset_loop = 0;
					}
				}
				// set target variable register
				set_regi (target_reg, (U1 *) target_var);
			}
			return (0);
		}
	}

	if (getvartype (target_var) == DOUBLE)
	{
		strcpy ((char *) code_temp, "load ");
		strcat ((char *) code_temp, (const char *) target_var);
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
				reg2 = get_regd (target_var);
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
		set_regd (target_reg, (U1 *) "");
		set_regd (target, (U1 *) "");
	}

	if (checkdef (target_var) != 0)
	{
		return (1);
	}
	if (getvartype (target_var) == INTEGER)
	{
		strcpy ((char *) code_temp, "load ");
		strcat ((char *) code_temp, (const char *) target_var);
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

		if (checkdef (target_var) != 0)
		{
			return (1);
		}

		if (getvartype_real (target_var) == BYTE)
		{
			strcpy ((char *) code_temp, "pullb ");
		}

		if (getvartype_real (target_var) == WORD)
		{
			strcpy ((char *) code_temp, "pullw ");
		}

		if (getvartype_real (target_var) == DOUBLEWORD)
		{
			strcpy ((char *) code_temp, "pulldw ");
		}

		if (getvartype_real (target_var) == QUADWORD)
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
				reg2 = get_regi (target_var);
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
		set_regi (target_reg, (U1 *) "");
		set_regi (target, (U1 *) "");
	}

	// check if for examnple an int64 is assigned to int32, loosing precision
	if (expression_var_type_max > target_var_type)
	{
		printf ("error: line %lli: expression assigns to target of lower precision!\n", linenum);
		return (1);
	}

   return (0);
}
