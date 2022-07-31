/*
 * This file main.h is part of L1vm.
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

 #ifndef _COMP_MAIN_H_
 #define _COMP_MAIN_H_

 struct ast
 {
 	U1 expr[MAXEXPRESSION][MAXARGS][MAXSTRLEN];
 	S4 expr_max;
 	S4 expr_args[MAXEXPRESSION]; 						// number of arguments in expression
 	S4 expr_reg[MAXEXPRESSION];							// registers of expression calculations = target registers
 	U1 expr_type[MAXEXPRESSION];						// type of register (INTEGER or DOUBLE)
 };

 // forward declarations
 U1 checkdigit (U1 *str);
 S8 get_temp_int (void);
 F8 get_temp_double (void);
 char *fgets_uni (char *str, int len, FILE *fptr);

 // proto parse_rpolish.c
 S2 parse_rpolish (U1 *postfix);
 //converts infix expression to postfix
 S2 convert (U1 infix[], U1 postfix[]);

 // mem.c
 void dealloc_array_U1 (U1** array, size_t n_rows);
 U1** alloc_array_U1 (size_t n_rows, size_t n_columns);

 // labels
 void init_labels (void);
 void init_call_labels (void);
 S2 set_call_label (U1 *label);
 S2 check_labels (void);
 S2 search_label (U1 *label);

 // string functions ===============================================================================
 size_t strlen_safe (const char * str, int maxlen);
 S2 searchstr (U1 *str, U1 *srchstr, S2 start, S2 end, U1 case_sens);
 void convtabs (U1 *str);

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
 S2 get_var_is_const (U1 *name);

 #endif
