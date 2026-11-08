/*
 * This file linter-main.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (info@midnight-coding.de), 2024
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

// Optional Z3 based bounded model checker. The build script defines HAVE_Z3
// when the Z3 headers and library are available. Without Z3 the linter keeps
// working exactly as before.
#ifdef HAVE_Z3
#include <z3.h>
#endif

// struct for function argument type definitions
// function arguments to check with function call arguments
#define MAXFUNC 4096        // maximum of function definitions
#define MAXFUNCARGS 256

#define MAXVARS 4096

#define BOOL 13
#define VARTYPE_NONE 14
#define VARTYPE_ANY 15
#define VARTYPE_VARIABLE 16

#define DEBUG 0

// set to one if the exit function call was found
// will result in an error if it is not found in a program!
U1 found_exit = 0;

// set to one if set by flag: to switch strict check for function protos!
// and show an error if no protos are found
U1 func_strict = 0;

// string functions ===============================================================================
// protos
char *fgets_uni (char *str, int len, FILE *fptr);
size_t strlen_safe (const char * str, S8  maxlen);
S2 searchstr (U1 *str, U1 *srchstr, S2 start, S2 end, U1 case_sens);
void convtabs (U1 *str);

// for function call arguments
struct function_args
{
    U1 name[MAXSTRLEN + 1];
    S2 arg_type[MAXFUNCARGS];
    S2 args;
    U1 strict;
};

struct function_args *function_args;
S8 function_args_ind ALIGN = -1;

// for function return variables
struct return_args
{
    U1 name[MAXSTRLEN + 1];
    S2 arg_type[MAXFUNCARGS];
    S2 args;
};

struct return_args *return_args;
S8 return_args_ind ALIGN = -1;

// for all variables
struct vars
{
    U1 name[MAXSTRLEN + 1];
    U1 type;
    U1 strict_arg;

    // safety / initialization tracking
    U1 initialized;             // 1 = has a value (set with value, assigned, stpop, ...)
    U1 pointer_set;             // 1 = can be used as pointer (set by pointer/assignment/stpop)
    U1 is_pointer;              // 1 = has been used as pointer destination
    U1 is_array;                // 1 = array (size > 1)
    U1 is_string;               // 1 = string variable
    U1 is_const;                // 1 = const-* variable
    U1 warned_uninit;           // warning about uninitialized use already printed
    U1 pointer_null;            // 1 = pointer was set from a known zero value
    U1 freed;                   // 1 = pointer was passed to dealloc/free
    U1 warned_null;             // null pointer warning already printed
    U1 warned_freed;            // use after free warning already printed
    U1 value_known;             // 1 = integer value is known at compile time
    S8 value ALIGN;             // known compile time integer value
    U1 dvalue_known;            // 1 = double value is known at compile time
    double dvalue;              // known compile time double value
    U1 aliased;                 // 1 = a pointer to this variable may write it
    S8 array_size ALIGN;        // number of elements, -1 = unknown
    S8 capacity ALIGN;          // bytes for arrays, characters (incl. NUL) for strings, -1 = unknown
    U1 str_len_known;           // 1 = string length is known at compile time
    S8 str_len ALIGN;           // current known string length (without NUL)
    U1 range_known;             // 1 = integer value range is known
    S8 range_min ALIGN;         // lower bound of known value range
    S8 range_max ALIGN;         // upper bound of known value range
    U1 may_zero;                // 1 = value may be zero on at least one path
    U1 heap_state;              // 0 = not heap, 1 = allocated, 2 = freed
    U1 heap_size_known;         // 1 = heap allocation size is known
    S8 heap_size ALIGN;         // heap allocation size in bytes, -1 = unknown
    U1 heap_origin;             // 0 = unknown, 1 = points to object/stack, 2 = heap memory
    U1 ptr_off_known;           // 1 = pointer offset is known at compile time
    S8 ptr_off ALIGN;           // known pointer offset from the start of the object
};

struct vars *vars;
S8 vars_ind ALIGN = -1;

// pointer safety state
U1 pointer_check_active = 0;
U1 pointer_check_name[MAXSTRLEN + 1] = "";
S8 pointer_check_line ALIGN = -1;

// number of warning-level diagnostics (all are treated as errors at exit)
S8 linter_warnings ALIGN = 0;

// optional Z3-backed bounded model checker (see linter-solver.c)
void linter_solver_file (const char *fname);

// lines which already produced a logical/assertion diagnostic. The solver
// pass runs after the interval pass and uses this table to avoid printing the
// same source line twice.
#define WARN_LINE_MAX 8192
S8 warn_lines[WARN_LINE_MAX] ALIGN;
S2 warn_lines_count = 0;

extern U1 file_inside;

void warn_line_mark (S8 line)
{
    S2 i;

    if (file_inside != 0)
    {
        return;
    }
    for (i = 0; i < warn_lines_count; i++)
    {
        if (warn_lines[i] == line)
        {
            return;
        }
    }
    if (warn_lines_count < WARN_LINE_MAX)
    {
        warn_lines[warn_lines_count] = line;
        warn_lines_count++;
    }
}

U1 warn_line_marked (S8 line)
{
    S2 i;

    for (i = 0; i < warn_lines_count; i++)
    {
        if (warn_lines[i] == line)
        {
            return (1);
        }
    }
    return (0);
}


// current function name
U1 function_name_current[MAXSTRLEN + 1];
U1 function_check_stpop = 0;    // set to 1 by: // (func args stpop)

// ---------------------------------------------------------------------------
// interprocedural data-flow analysis
//
// The linter is a single pass over the file. To analyse a call with the
// caller's actual value ranges a pre-pass collects all function bodies and
// their annotations. At a call site the callee body is then executed in a
// sandbox: a private copy of the variable table, a private control flow
// state and a private value stack. The diagnostics produced in the sandbox
// are attributed to the call site, the effects on globals and the return
// value ranges are fed back into the caller.
// ---------------------------------------------------------------------------

#define IP_MAX_FUNCS        4096
#define IP_MAX_FUNCS_LINES  200000
#define IP_MAX_RETURNS      32
#define IP_MAX_CALL_ARGS    32
#define IP_MAX_DEPTH        12
#define IP_MAX_DECLS        256
#define IP_MAX_EFFECTS      4096
#define IP_VSTACK_MAX       128

struct ip_func
{
    U1 name[MAXSTRLEN + 1];
    U1 **lines;
    S2 line_count;
    S2 line_cap;
    S2 args;                    // number of arguments, -1 = unknown
    S2 returns;                 // number of return values, -1 = unknown
    U1 has_body;
};

struct ip_func *ip_funcs = NULL;
S2 ip_funcs_ind = -1;
U1 ip_enabled = 1;
S8 ip_total_lines ALIGN = 0;
U1 ip_sandbox_active = 0;
S2 ip_sandbox_depth = 0;
U1 ip_call_precise = 0;         // the last call was analysed by a sandbox
U1 ip_active_funcs[IP_MAX_DEPTH][MAXSTRLEN + 1];

U1 *ip_written = NULL;          // per sandbox variable written map
U1 ip_effect_mark[IP_MAX_EFFECTS]; // caller variables written by the last call
S8 ip_budget ALIGN = 0;

S2 ip_analyze_call (U1 *callee, S2 nargs, U1 *call_args[], U1 *line);
S2 ip_raw_callee_name (U1 *line, U1 *name);
void ip_capture_return (S2 nt, U1 tokens[][MAXSTRLEN + 1], U1 *line);
void ip_cleanup (void);
S2 ip_find_func (U1 *name);

// pointer only const type and not as a target to vallue copy
U1 pointer_strict = 0;

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


// for functions, to check if (return) was used in function
U1 function_inside = 0;
U1 function_return = 0;

U1 if_inside = 0;
U1 switch_inside = 0;
U1 for_inside = 0;
U1 do_inside = 0;


S2 init_function_args (void)
{
	function_args = (struct function_args*) calloc (MAXFUNC, sizeof (struct function_args));
	if (function_args == NULL)
	{
		printf ("error: can't allocate function call argumnents list!\n");
		return (1);
	}
	return (0);
}

S2 init_return_args (void)
{
	return_args = (struct return_args*) calloc (MAXFUNC, sizeof (struct return_args));
	if (return_args == NULL)
	{
		printf ("error: can't allocate function return argumnents list!\n");
		return (1);
	}
	return (0);
}

S2 init_vars (void)
{
	vars = (struct vars*) calloc (MAXVARS, sizeof (struct vars));
	if (vars == NULL)
	{
		printf ("error: can't allocate vars list!\n");
		return (1);
	}
	return (0);
}


S2 set_function_name (U1 *name)
{
    S8 name_len ALIGN;

    name_len = strlen_safe ((const char *) name, MAXSTRLEN);
    if (name_len > MAXSTRLEN)
    {
        printf ("set_function_name: error: function name overflow!\n");
        return (1);
    }

    if (function_args_ind >= MAXFUNC)
    {
        printf ("set_function_name: error: functions overflow!\n");
        return (1);
    }
    function_args_ind++;

    // copy function name
    strcpy ((char *) function_args[function_args_ind].name, (char *) name);
    return (0);
}

S2 get_function_index (U1 *name)
{
    S8 name_len ALIGN;
    S8 i ALIGN;

    name_len = strlen_safe ((const char *) name, MAXSTRLEN);
    if (name_len > MAXSTRLEN)
    {
        printf ("get_function_index: error: function name overflow!\n");
        return (1);
    }

    for (i = 0; i <= function_args_ind; i++)
    {
        // EDIT
        //printf ("get_function_index: %lli stored name: '%s', search name: '%s'\n", i, function_args[i].name, name);

        if (strcmp ((const char  *) function_args[i].name, (const char *) name) == 0)
        {
            return (i);  // function name matches! return index
        }
    }

    return (-1);  // function name not found
}

S2 get_return_index (U1 *name)
{
    S8 name_len ALIGN;
    S8 i ALIGN;

    #if DEBUG
    printf ("get_return_index...\n");
    #endif

    name_len = strlen_safe ((const char *) name, MAXSTRLEN);
    if (name_len > MAXSTRLEN)
    {
        printf ("get_return_index: error: function name overflow!\n");
        return (1);
    }

    for (i = 0; i <= return_args_ind; i++)
    {
        #if DEBUG
        printf ("get_return_index: %lli: name: '%s'\n", i, return_args[i].name);
        #endif

        if (strcmp ((const char  *) return_args[i].name, (const char *)  name) == 0)
        {
            return (i);  // function return name matches! return index
        }
    }

    return (-1);  // function return name not found
}

S2 get_varname_type (U1 *name)
{
    S8 i ALIGN = 0;

    for (i = 0; i <= vars_ind; i++)
    {
        if (strcmp ((const char *) vars[i].name, (const char *) name) == 0)
        {
            if (vars[i].strict_arg == 1)
            {
                printf ("error: variable was used in strict function call.\nAnd is not valid in the later code!\n");
                return (-1);
            }
            else
            {
                // found variable, return variable type
                return (vars[i].type);
            }
        }
    }
    // variable not found
    printf ("error: variable '%s' not defined!\n", name);
    return (-1);
}

U1 checkpointer (U1 *type)
{
    if (strcmp ((const char *) type, "const-int64") == 0)
    {
        // is pointer const
        return (0);
    }
    else
    {
        // is not const int64
        return (1);
    }
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
        vartype = BYTE;
    }
    if (strcmp ((const char *) type, "const-string") == 0)
    {
        vartype = BYTE;
    }
    if (strcmp ((const char *) type, "mut-string") == 0)
    {
        vartype = BYTE;
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

    if (strcmp ((const char *) type, "any") == 0)
    {
        vartype = VARTYPE_ANY;
    }

    if (strcmp ((const char *) type, "variable") == 0)
    {
        // number of variables variable, not checked if set
        vartype = VARTYPE_VARIABLE;
    }

    return (vartype);
}

S2 get_var_strict (U1 *name)
{
    S8 i ALIGN = 0;

    for (i = 0; i <= vars_ind; i++)
    {
        if (strcmp ((const char *) vars[i].name, (const char *) name) == 0)
        {
            // found variable, return variable type
            if (vars[i].strict_arg > 0)
            {
                if (vars[i].strict_arg == 1)
                {
                    vars[i].strict_arg = 2;
                    return (1);
                }
                else
                {
                    if (vars[i].strict_arg == 2)
                    {
                        printf ("> error: variable '%s', which was used in 'strict' function call or removed, is not valid!\n", name);
                        return (vars[i].strict_arg);
                    }
                }
            }
            return (vars[i].strict_arg);
        }
    }

    return (0);
}

S2 set_var_strict (U1 *name)
{
    S8 i ALIGN = 0;

    for (i = 0; i <= vars_ind; i++)
    {
        if (strcmp ((const char *) vars[i].name, (const char *) name) == 0)
        {
            // found variable, return variable type
            vars[i].strict_arg = 1;
            return (0);
        }
    }

    // variable not found
    printf ("set_var_strict: error: variable '%s' not defined!\n", name);
    return (-1);
}

S2 set_var_strict_two (U1 *name)
{
    S8 i ALIGN = 0;

    for (i = 0; i <= vars_ind; i++)
    {
        if (strcmp ((const char *) vars[i].name, (const char *) name) == 0)
        {
            // found variable, return variable type
            vars[i].strict_arg = 2;
            return (0);
        }
    }

    // variable not found
    printf ("set_var_strict_two: error: variable '%s' not defined!\n", name);
    return (-1);
}

S2 check_line_strict (U1 *line)
{
    // check if strict variable, wich was used in function call is used later
    // This is not alowed!

    S8 i ALIGN = 0;
    S8 j ALIGN = 0;
    S8 k ALIGN = 0;
    S8 line_len ALIGN;
    U1 var[MAXSTRLEN + 1] = "";
    S8 varind ALIGN = -1;
    U1 parse = 1;
    S8 comm_pos = 0;
    // EDIT WRITE

    U1 pointer_var[MAXSTRLEN + 1] = "";

    line_len = strlen_safe ((const char *) line, MAXLINELEN);
    if (line_len == 0)
    {
        // empty line: return all ok!
        return (0);
    }

    if (line_len >= 2)
    {
        comm_pos = searchstr (line, (U1 *) "//", 0, 0, TRUE);
        if (comm_pos != -1)
        {
            return (0);   // comment line
        }
    }

    while (parse == 1)
    {
        if (line[i] != ' ' && line[i] != '(' && line[i] != ')' && line[i] != '=' && line[i] != ':')
        {
            if (varind < MAXSTRLEN)
            {
                varind++;
            }
            else
            {
                printf ("check_line_strict: variable name overflow!\n");
                return (1);
            }
            var[varind] = line[i];
        }

        if (line[i] == '(' || line[i] == ' ' || line[i] == ')' || line [i] == ':')
        {
            if (varind < MAXSTRLEN)
            {
                varind++;
            }
            else
            {
                printf ("check_line_strict: variable name overflow!\n");
                return (1);
            }
            var[varind] = '\0';

            // EDIT POINTER
            if (varind >= 2)
            {
                //printf ("check_line_strict: POINTER: '%c'\n", var[0]);

                if (var[0] == 'P')
                {
                  //  printf ("check_line_strict: POINTER START\n");

                    k = 0;
                    for (j = 1; j <= varind; j++)
                    {
                        pointer_var[k] = var[j];
                        k++;
                    }

                    if (get_var_strict (pointer_var) == 2)
                    {
                        // variable is strict variable, not allowed to use any more!
                        return (1);
                    }
                }
            }

            if (get_var_strict (var) == 2)
            {
                // variable is strict variable, not allowed to use any more!
                return (1);
            }

            if (get_var_strict (pointer_var) == 2)
            {
                // variable is strict variable, not allowed to use any more!
                return (1);
            }

            // check if :=) line end:
            if (line[i] == ':')
            {
                if (i < line_len - 2)
                {
                    if (line[i + 1] == '=')
                    {
                        i++;
                    }
                }
            }

            // reset varind:
            varind = -1;
        }
        if (i < line_len - 1)
        {
            i++;
        }
        else
        {
            parse = 0;
        }
    }
    return (0);
}

void cleanup (void)
{
    ip_cleanup ();
    if (vars != NULL) free (vars);
    if (function_args != NULL) free (function_args);
    if (return_args != NULL) free (return_args);
}

// ---------------------------------------------------------------------------
// safety checks: tokens, pointers, initialization, array bounds, structure
// ---------------------------------------------------------------------------

#define MAXTOKENS 256

U1 safety_tokens[MAXTOKENS][MAXSTRLEN + 1];
U1 annotation_tokens[MAXTOKENS][MAXSTRLEN + 1];

U1 is_ident_start (U1 c)
{
    return (U1) (isalpha ((unsigned char) c) || c == '_' || c == '\\');
}

U1 is_ident_char (U1 c)
{
    return (U1) (isalnum ((unsigned char) c) || c == '_' || c == '.' ||
                 c == '~' || c == '@' || c == '\\');
}

U1 is_operator_suffix (U1 c)
{
    // typed operator suffixes: int64 (i), double (d), byte (b), int16 (w), int32 (l)
    return (U1) (c == 'i' || c == 'd' || c == 'b' || c == 'w' || c == 'l');
}

U1 operator_takes_suffix (U1 c)
{
    return (U1) (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
                 c == '<' || c == '>');
}

U1 is_number_token (U1 *tok)
{
    S2 i = 0;

    if (tok[0] == '-' || tok[0] == '+')
    {
        i = 1;
    }
    if (tok[i] == '\0')
    {
        return (0);
    }
    while (tok[i] != '\0')
    {
        if (! isdigit ((unsigned char) tok[i]))
        {
            return (0);
        }
        i++;
    }
    return (1);
}

// simple tokenizer for one source line. Strings are kept as one token.
// scan an integer or floating point number; returns the next input position
S2 scan_number (S2 i, U1 *line, S2 len, U1 *token)
{
    S2 j = 0;

    if (i < len && (line[i] == '-' || line[i] == '+'))
    {
        if (j < MAXSTRLEN)
        {
            token[j] = line[i];
            j++;
        }
        i++;
    }
    while (i < len && isdigit ((unsigned char) line[i]))
    {
        if (j < MAXSTRLEN)
        {
            token[j] = line[i];
            j++;
        }
        i++;
    }
    if (i < len && line[i] == '.')
    {
        if (j < MAXSTRLEN)
        {
            token[j] = line[i];
            j++;
        }
        i++;
        while (i < len && isdigit ((unsigned char) line[i]))
        {
            if (j < MAXSTRLEN)
            {
                token[j] = line[i];
                j++;
            }
            i++;
        }
    }
    if (i < len && (line[i] == 'e' || line[i] == 'E'))
    {
        S2 exp_start = i;
        S2 exp_pos = j;
        if (j < MAXSTRLEN)
        {
            token[j] = line[i];
            j++;
        }
        i++;
        if (i < len && (line[i] == '-' || line[i] == '+'))
        {
            if (j < MAXSTRLEN)
            {
                token[j] = line[i];
                j++;
            }
            i++;
        }
        if (i < len && isdigit ((unsigned char) line[i]))
        {
            while (i < len && isdigit ((unsigned char) line[i]))
            {
                if (j < MAXSTRLEN)
                {
                    token[j] = line[i];
                    j++;
                }
                i++;
            }
        }
        else
        {
            // not an exponent, keep the number without the 'e'
            i = exp_start;
            j = exp_pos;
        }
    }
    token[j] = '\0';
    return (i);
}

S2 tokenize_line (U1 *line, U1 tokens[][MAXSTRLEN + 1], S2 max_tokens)
{
    S2 i = 0;
    S2 n = 0;
    S2 j;
    S2 len;
    U1 c;

    len = strlen_safe ((const char *) line, MAXLINELEN);

    while (i < len)
    {
        c = line[i];

        if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
        {
            i++;
            continue;
        }
        if (n >= max_tokens)
        {
            break;
        }

        // string literal: keep everything up to the closing quote
        if (c == '"')
        {
            j = 0;
            tokens[n][j] = c;
            j++;
            i++;
            while (i < len && line[i] != '"')
            {
                if (j < MAXSTRLEN)
                {
                    tokens[n][j] = line[i];
                    j++;
                }
                i++;
            }
            if (i < len)
            {
                if (j < MAXSTRLEN)
                {
                    tokens[n][j] = line[i];
                    j++;
                }
                i++;
            }
            tokens[n][j] = '\0';
            n++;
            continue;
        }

        // multi character operators
        if (c == ':' && i + 1 < len && line[i + 1] == '=')
        {
            strcpy ((char *) tokens[n], ":=");
            n++;
            i += 2;
            continue;
        }
        if ((c == '=' || c == '!' || c == '<' || c == '>') && i + 1 < len && line[i + 1] == '=')
        {
            S2 tlen = 2;
            tokens[n][0] = c;
            tokens[n][1] = line[i + 1];
            if (i + 2 < len && is_operator_suffix (line[i + 2]))
            {
                tokens[n][2] = line[i + 2];
                tlen = 3;
            }
            tokens[n][tlen] = '\0';
            n++;
            i += tlen;
            continue;
        }
        if ((c == '&' || c == '|') && i + 1 < len && line[i + 1] == c)
        {
            tokens[n][0] = c;
            tokens[n][1] = c;
            tokens[n][2] = '\0';
            n++;
            i += 2;
            continue;
        }

        // negative number
        if ((c == '-' || c == '+') && i + 1 < len && isdigit ((unsigned char) line[i + 1]))
        {
            i = scan_number (i, line, len, tokens[n]);
            n++;
            continue;
        }

        // punctuation
        if (c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' ||
            c == ':' || c == '=' || c == '<' || c == '>' || c == '+' || c == '-' ||
            c == '*' || c == '/' || c == '%' || c == '?' || c == ',' || c == ';' ||
            c == '&' || c == '|' || c == '!')
        {
            S2 tlen = 1;
            tokens[n][0] = c;
            if (operator_takes_suffix (c) && i + 1 < len && is_operator_suffix (line[i + 1]))
            {
                tokens[n][1] = line[i + 1];
                tlen = 2;
            }
            tokens[n][tlen] = '\0';
            n++;
            i += tlen;
            continue;
        }

        // number
        if (isdigit ((unsigned char) c))
        {
            i = scan_number (i, line, len, tokens[n]);
            n++;
            continue;
        }

        // identifier
        if (is_ident_start (c))
        {
            j = 0;
            while (i < len && (is_ident_char (line[i]) || isdigit ((unsigned char) line[i])))
            {
                if (j < MAXSTRLEN)
                {
                    tokens[n][j] = line[i];
                    j++;
                }
                i++;
            }
            tokens[n][j] = '\0';
            n++;
            continue;
        }

        // unknown character: skip
        i++;
    }

    return (n);
}

// find a variable, but do not print an error if it is unknown
S8 find_var_index (U1 *name)
{
    S8 i ALIGN;

    for (i = 0; i <= vars_ind; i++)
    {
        if (strcmp ((const char *) vars[i].name, (const char *) name) == 0)
        {
            return (i);
        }
    }
    return (-1);
}

U1 var_is_pointer_candidate (U1 *name)
{
    S8 i ALIGN;
    const char *dot;
    U1 rest[MAXSTRLEN + 1];

    if (name[0] != 'P' || name[1] == '\0')
    {
        return (0);
    }

    // P + variable name
    if (find_var_index (name + 1) != -1)
    {
        return (1);
    }

    // P + last component of a struct like variable name: person.x -> Px
    strcpy ((char *) rest, (const char *) (name + 1));
    for (i = 0; i <= vars_ind; i++)
    {
        dot = strrchr ((const char *) vars[i].name, '.');
        if (dot != NULL)
        {
            if (strcmp (dot + 1, (const char *) rest) == 0)
            {
                return (1);
            }
        }
    }
    return (0);
}

void mark_var_written (U1 *name)
{
    S8 vi ALIGN;

    vi = find_var_index (name);
    if (vi >= 0)
    {
        if (ip_written != NULL && vi < MAXVARS)
        {
            ip_written[vi] = 1;
        }
        vars[vi].initialized = 1;
        vars[vi].pointer_set = 1;
        vars[vi].value_known = 0;
        vars[vi].dvalue_known = 0;
        vars[vi].range_known = 0;
        vars[vi].may_zero = 0;
        vars[vi].str_len_known = 0;
        vars[vi].warned_uninit = 0;
        vars[vi].pointer_null = 0;
        vars[vi].freed = 0;
        vars[vi].warned_null = 0;
        vars[vi].warned_freed = 0;
    }
}

void warn_uninitialized (U1 *name, U1 *line)
{
    S8 vi ALIGN;

    vi = find_var_index (name);
    if (vi < 0)
    {
        return;
    }
    if (vars[vi].initialized == 1)
    {
        return;
    }
    if (vars[vi].warned_uninit == 1)
    {
        return;
    }

    vars[vi].warned_uninit = 1;
    linter_warnings++;
    printf ("linter error: variable '%s' is used before it is initialized!\n", name);
    printf (" > %s\n", line);
}

U1 token_is_keyword (U1 *tok)
{
    static const char *keywords[] =
    {
        "set", "func", "funcend", "object", "objectend", "pointer",
        "stpop", "stpopi", "stpopd", "stpopb",
        "stpush", "stpushi", "stpushd", "stpushb",
        "stack_type", "pull_int64_var", "pull_double_var", "pull_byte_var",
        "if", "if+", "else", "endif", "for", "for-loop", "next",
        "do", "while", "switch", "switchend", "break", "return",
        "cast", "range", "jmp", "jmpi", "intr0", "intr1", "call",
        "loada", "load", "loadl", "rts",
        "addi", "subi", "muli", "divi", "lsi", "lseqi", "neqi",
        "ASM", "ASM_END", "memory_bounds_on", "memory_bounds_off",
        "check_pointer", "pointer_type", "get_size", "string_immutable",
        "none", "any", "variable", "true", "false", "s",
        "byte", "int16", "int32", "int64", "double", "bool", "string",
        "const-byte", "const-int16", "const-int32", "const-int64",
        "const-double", "const-bool", "const-string",
        "mut-byte", "mut-int16", "mut-int32", "mut-int64",
        "mut-double", "mut-bool", "mut-string",
        NULL
    };
    S2 i = 0;

    while (keywords[i] != NULL)
    {
        if (strcmp ((const char *) tok, keywords[i]) == 0)
        {
            return (1);
        }
        i++;
    }
    return (0);
}

// get interrupt number of a preprocessed "(OP ... intr0/intr1)" line, -1 if none
S2 get_intr_op (S2 nt, U1 tokens[][MAXSTRLEN + 1])
{
    S2 i;

    for (i = 2; i < nt; i++)
    {
        if (strcmp ((const char *) tokens[i], "intr0") == 0 ||
            strcmp ((const char *) tokens[i], "intr1") == 0)
        {
            if (is_number_token (tokens[1]))
            {
                return ((S2) atoi ((const char *) tokens[1]));
            }
            return (-1);
        }
    }
    return (-1);
}

// mark variables which are written by common intr0 functions
void mark_intr_writes (S2 op, S2 nt, U1 tokens[][MAXSTRLEN + 1])
{
    S2 idx[4];
    S2 n = 0;
    S2 i;

    switch (op)
    {
        case 8:     // detime
        case 9:     // input_i
        case 10:    // input_d
        case 12:    // shell_args
        case 15:    // cpu_core
        case 16:    // get_host_endianess
        case 19:    // weekday
        case 25:    // stop_timer
        case 32:    // stack_type
        case 35:    // get_host_cpu
        case 36:    // get_host_os
        case 41:    // epochms
        case 252:   // get_overflow
            idx[n] = 2;
            n++;
            break;

        case 11:    // input_s: length, string
        case 13:    // get_shell_arg: index, string
            idx[n] = 3;
            n++;
            break;

        case 17:    // time: hour, min, sec
        case 18:    // date: year, month, day
            idx[n] = 2;
            n++;
            idx[n] = 3;
            n++;
            idx[n] = 4;
            n++;
            break;

        case 31:    // pointer_type: pointer, type
        case 33:    // get_size: pointer, size
            idx[n] = 3;
            n++;
            break;

        default:
            break;
    }

    for (i = 0; i < n; i++)
    {
        if (idx[i] < nt)
        {
            if (is_ident_start (tokens[idx[i]][0]))
            {
                mark_var_written (tokens[idx[i]]);
            }
        }
    }
}

U1 line_has_pointer_check (S2 nt, U1 tokens[][MAXSTRLEN + 1], U1 *name)
{
    S2 i;
    U1 check_found = 0;
    U1 name_found = 0;
    S2 op;

    op = get_intr_op (nt, tokens);
    if (op == 30 || op == 31)
    {
        check_found = 1;
    }
    for (i = 0; i < nt; i++)
    {
        if (strcmp ((const char *) tokens[i], "check_pointer") == 0 ||
            strcmp ((const char *) tokens[i], "pointer_type") == 0)
        {
            check_found = 1;
        }
        if (strcmp ((const char *) tokens[i], (const char *) name) == 0)
        {
            name_found = 1;
        }
    }
    return (U1) (check_found && name_found);
}

// element size in bytes for array bounds checking
S8 var_element_size (U1 type)
{
    switch (type)
    {
        case BYTE:
            return (1);
        case WORD:
            return (2);
        case DOUBLEWORD:
            return (4);
        case QUADWORD:
            return (8);
        case DOUBLEFLOAT:
            return (8);
        default:
            return (1);
    }
}

// find the closing ')' for an opening '('
S2 find_close_paren (S2 nt, U1 tokens[][MAXSTRLEN + 1], S2 open)
{
    S2 i;
    S2 depth = 0;

    for (i = open; i < nt; i++)
    {
        if (strcmp ((const char *) tokens[i], "(") == 0)
        {
            depth++;
        }
        else if (strcmp ((const char *) tokens[i], ")") == 0)
        {
            depth--;
            if (depth == 0)
            {
                return (i);
            }
        }
    }
    return (-1);
}

// find the opening '(' for a closing ')'
S2 find_open_paren (S2 nt, U1 tokens[][MAXSTRLEN + 1], S2 close)
{
    S2 i;
    S2 depth = 0;

    (void) nt;

    for (i = close; i >= 0; i--)
    {
        if (strcmp ((const char *) tokens[i], ")") == 0)
        {
            depth++;
        }
        else if (strcmp ((const char *) tokens[i], "(") == 0)
        {
            depth--;
            if (depth == 0)
            {
                return (i);
            }
        }
    }
    return (-1);
}

// find the ']' for an opening '['
S2 find_close_bracket (S2 nt, U1 tokens[][MAXSTRLEN + 1], S2 open)
{
    S2 i;
    S2 depth = 0;

    for (i = open; i < nt; i++)
    {
        if (strcmp ((const char *) tokens[i], "[") == 0)
        {
            depth++;
        }
        else if (strcmp ((const char *) tokens[i], "]") == 0)
        {
            depth--;
            if (depth == 0)
            {
                return (i);
            }
        }
    }
    return (-1);
}

// find the innermost '(' ... ')' group containing pos
U1 find_enclosing_paren (S2 nt, U1 tokens[][MAXSTRLEN + 1], S2 pos, S2 *open, S2 *close)
{
    S2 i;
    S2 depth = 0;

    *open = -1;
    *close = -1;

    for (i = pos; i >= 0; i--)
    {
        if (strcmp ((const char *) tokens[i], ")") == 0)
        {
            depth++;
        }
        else if (strcmp ((const char *) tokens[i], "(") == 0)
        {
            if (depth == 0)
            {
                *open = i;
                break;
            }
            depth--;
        }
    }
    if (*open < 0)
    {
        return (0);
    }
    *close = find_close_paren (nt, tokens, *open);
    if (*close < 0)
    {
        *open = -1;
        return (0);
    }
    return (1);
}

U1 is_operator_like (U1 *tok)
{
    if (tok[0] == '\0')
    {
        return (0);
    }
    if (is_number_token (tok) == 1)
    {
        return (0);
    }
    return (U1) (strchr ("+-*/%&|^<>=!", tok[0]) != NULL);
}

// apply a constant integer operator. Returns 1 on success.
U1 eval_apply (U1 *tok, S8 a, S8 b, S8 *result, U1 *overflow)
{
    S2 len;
    U1 suffix = 0;

    len = (S2) strlen_safe ((const char *) tok, MAXSTRLEN);
    if (len == 2 && is_operator_suffix (tok[1]) == 1)
    {
        suffix = tok[1];
    }

    // floating point and narrower integer types are not evaluated here
    if (suffix == 'd' || suffix == 'b' || suffix == 'w' || suffix == 'l')
    {
        return (0);
    }

    if (strcmp ((const char *) tok, "==") == 0)
    {
        *result = (S8) (a == b);
        return (1);
    }
    if (strcmp ((const char *) tok, "!=") == 0)
    {
        *result = (S8) (a != b);
        return (1);
    }
    if (strcmp ((const char *) tok, "<") == 0 || strcmp ((const char *) tok, "<i") == 0)
    {
        *result = (S8) (a < b);
        return (1);
    }
    if (strcmp ((const char *) tok, ">") == 0 || strcmp ((const char *) tok, ">i") == 0)
    {
        *result = (S8) (a > b);
        return (1);
    }
    if (strcmp ((const char *) tok, "<=") == 0 || strcmp ((const char *) tok, "<=i") == 0)
    {
        *result = (S8) (a <= b);
        return (1);
    }
    if (strcmp ((const char *) tok, ">=") == 0 || strcmp ((const char *) tok, ">=i") == 0)
    {
        *result = (S8) (a >= b);
        return (1);
    }
    if (strcmp ((const char *) tok, "&&") == 0)
    {
        *result = (S8) (a != 0 && b != 0);
        return (1);
    }
    if (strcmp ((const char *) tok, "||") == 0)
    {
        *result = (S8) (a != 0 || b != 0);
        return (1);
    }

    switch (tok[0])
    {
        case '+':
            if (__builtin_add_overflow (a, b, result) != 0)
            {
                *overflow = 1;
                return (0);
            }
            return (1);

        case '-':
            if (__builtin_sub_overflow (a, b, result) != 0)
            {
                *overflow = 1;
                return (0);
            }
            return (1);

        case '*':
            if (__builtin_mul_overflow (a, b, result) != 0)
            {
                *overflow = 1;
                return (0);
            }
            return (1);

        case '/':
            if (b == 0)
            {
                return (0);
            }
            if (a == LLONG_MIN && b == -1)
            {
                *overflow = 1;
                return (0);
            }
            *result = a / b;
            return (1);

        case '%':
            if (b == 0)
            {
                return (0);
            }
            if (a == LLONG_MIN && b == -1)
            {
                *overflow = 1;
                return (0);
            }
            *result = a % b;
            return (1);

        case '&':
            *result = a & b;
            return (1);

        case '|':
            *result = a | b;
            return (1);

        case '^':
            *result = a ^ b;
            return (1);

        default:
            return (0);
    }
}

// evaluate a constant integer expression. Supports nested parentheses and both
// L1VM notations: infix "(a + b)" and reverse Polish "((a b +) c =)".
// Returns 1 if a value is known, 0 otherwise.
U1 eval_expr (S2 nt, U1 tokens[][MAXSTRLEN + 1], S2 start, S2 end, S8 *value, U1 *overflow)
{
    S2 i;
    S2 close;
    S2 first_op = -1;
    S2 last_op = -1;
    S2 op_count = 0;
    S8 left_value ALIGN;
    S8 right_value ALIGN;
    U1 left_overflow = 0;
    U1 right_overflow = 0;
    U1 left_known;
    U1 right_known;
    S2 right_start;

    if (start < 0 || end < 0 || start > end || end >= nt)
    {
        return (0);
    }

    // single literal, variable or parenthesized group
    if (start == end)
    {
        if (is_number_token (tokens[start]) == 1)
        {
            char *endptr = NULL;
            S8 v ALIGN;

            errno = 0;
            v = (S8) strtoll ((const char *) tokens[start], &endptr, 10);
            if (errno == ERANGE)
            {
                *overflow = 1;
                return (0);
            }
            *value = v;
            return (1);
        }
        else
        {
            S8 vi ALIGN = find_var_index (tokens[start]);
            if (vi >= 0 && vars[vi].value_known == 1)
            {
                *value = vars[vi].value;
                return (1);
            }
            return (0);
        }
    }

    if (strcmp ((const char *) tokens[start], "(") == 0)
    {
        close = find_close_paren (nt, tokens, start);
        if (close == end)
        {
            return (eval_expr (nt, tokens, start + 1, end - 1, value, overflow));
        }
    }

    // find top level operators, skipping nested groups and array subscripts
    for (i = start; i <= end; i++)
    {
        if (strcmp ((const char *) tokens[i], "(") == 0)
        {
            close = find_close_paren (nt, tokens, i);
            if (close < 0 || close > end)
            {
                return (0);
            }
            i = close;
            continue;
        }
        if (strcmp ((const char *) tokens[i], "[") == 0)
        {
            close = find_close_bracket (nt, tokens, i);
            if (close < 0 || close > end)
            {
                return (0);
            }
            i = close;
            continue;
        }
        if (is_operator_like (tokens[i]) == 1)
        {
            if (op_count == 0)
            {
                first_op = i;
            }
            last_op = i;
            op_count++;
        }
    }

    // reverse Polish: the last token is the operator
    if (op_count >= 1 && last_op == end)
    {
        if (end - 1 < start)
        {
            return (0);
        }
        if (strcmp ((const char *) tokens[end - 1], ")") == 0)
        {
            right_start = find_open_paren (nt, tokens, end - 1);
            if (right_start < start)
            {
                return (0);
            }
        }
        else
        {
            right_start = end - 1;
        }
        if (right_start <= start)
        {
            return (0);
        }

        left_known = eval_expr (nt, tokens, start, right_start - 1, &left_value, &left_overflow);
        right_known = eval_expr (nt, tokens, right_start, end - 1, &right_value, &right_overflow);
        if (left_overflow == 1 || right_overflow == 1)
        {
            *overflow = 1;
            return (0);
        }
        if (left_known == 1 && right_known == 1)
        {
            return (eval_apply (tokens[last_op], left_value, right_value, value, overflow));
        }
        return (0);
    }

    // infix: exactly one top level operator, between two operands
    if (op_count == 1 && first_op > start && first_op < end)
    {
        left_known = eval_expr (nt, tokens, start, first_op - 1, &left_value, &left_overflow);
        right_known = eval_expr (nt, tokens, first_op + 1, end, &right_value, &right_overflow);
        if (left_overflow == 1 || right_overflow == 1)
        {
            *overflow = 1;
            return (0);
        }
        if (left_known == 1 && right_known == 1)
        {
            return (eval_apply (tokens[first_op], left_value, right_value, value, overflow));
        }
        return (0);
    }

    return (0);
}

// evaluate the operand starting at pos (literal, variable or "( ... )")
U1 eval_operand_after (S2 nt, U1 tokens[][MAXSTRLEN + 1], S2 pos, S8 *value, U1 *overflow)
{
    S2 close;

    if (pos < 0 || pos >= nt)
    {
        return (0);
    }
    if (strcmp ((const char *) tokens[pos], "(") == 0)
    {
        close = find_close_paren (nt, tokens, pos);
        if (close < 0)
        {
            return (0);
        }
        return (eval_expr (nt, tokens, pos, close, value, overflow));
    }
    return (eval_expr (nt, tokens, pos, pos, value, overflow));
}

// evaluate the operand ending at pos (literal, variable or "( ... )")
U1 eval_operand_before (S2 nt, U1 tokens[][MAXSTRLEN + 1], S2 pos, S8 *value, U1 *overflow)
{
    S2 open;

    if (pos < 0 || pos >= nt)
    {
        return (0);
    }
    if (strcmp ((const char *) tokens[pos], ")") == 0)
    {
        open = find_open_paren (nt, tokens, pos);
        if (open < 0)
        {
            return (0);
        }
        return (eval_expr (nt, tokens, open, pos, value, overflow));
    }
    return (eval_expr (nt, tokens, pos, pos, value, overflow));
}

U1 is_float_token (U1 *tok)
{
    char *endptr = NULL;

    if (tok[0] == '\0')
    {
        return (0);
    }
    if (! (isdigit ((unsigned char) tok[0]) ||
           ((tok[0] == '-' || tok[0] == '+') &&
            (isdigit ((unsigned char) tok[1]) || tok[1] == '.'))))
    {
        return (0);
    }
    errno = 0;
    (void) strtod ((const char *) tok, &endptr);
    if (errno == ERANGE)
    {
        return (0);
    }
    return (U1) (endptr != (char *) tok && endptr != NULL && *endptr == '\0');
}

// evaluate a simple double operand: literal or variable with known value
U1 eval_operand_double_before (S2 nt, U1 tokens[][MAXSTRLEN + 1], S2 pos, double *value)
{
    S8 vi ALIGN;

    if (pos < 0 || pos >= nt)
    {
        return (0);
    }
    if (is_float_token (tokens[pos]) == 1)
    {
        *value = strtod ((const char *) tokens[pos], NULL);
        return (1);
    }
    vi = find_var_index (tokens[pos]);
    if (vi >= 0 && vars[vi].dvalue_known == 1)
    {
        *value = vars[vi].dvalue;
        return (1);
    }
    return (0);
}

// decide if an expression group uses reverse Polish notation
U1 group_is_postfix (S2 nt, U1 tokens[][MAXSTRLEN + 1], S2 open, S2 close)
{
    S2 i;
    S2 start = open + 1;
    S2 end = close - 1;
    S2 operands = 0;
    S2 assign_op = -1;

    if (start > end)
    {
        return (0);
    }

    // remove a trailing assignment target and operator
    for (i = start; i <= end; i++)
    {
        if (strcmp ((const char *) tokens[i], "(") == 0)
        {
            S2 c = find_close_paren (nt, tokens, i);
            if (c < 0 || c > end)
            {
                return (0);
            }
            i = c;
        }
        else if (strcmp ((const char *) tokens[i], "=") == 0 ||
                 strcmp ((const char *) tokens[i], ":=") == 0)
        {
            assign_op = i;
        }
    }
    if (assign_op >= 0)
    {
        if (assign_op - 1 < start)
        {
            return (0);
        }
        end = assign_op - 2;
    }
    if (start > end)
    {
        return (0);
    }

    for (i = start; i <= end; i++)
    {
        if (strcmp ((const char *) tokens[i], "(") == 0)
        {
            S2 c = find_close_paren (nt, tokens, i);
            if (c < 0 || c > end)
            {
                return (0);
            }
            operands++;
            i = c;
            continue;
        }
        if (strcmp ((const char *) tokens[i], "[") == 0)
        {
            S2 c = find_close_bracket (nt, tokens, i);
            if (c < 0 || c > end)
            {
                return (0);
            }
            i = c;
            continue;
        }
        if (is_operator_like (tokens[i]) == 1)
        {
            break;
        }
        operands++;
    }
    return (U1) (operands >= 2);
}

// ---------------------------------------------------------------------------
// CBMC-style abstract interpretation support
//
// The checks below go beyond the purely local checks of the original linter:
//  - integer value ranges (intervals) and overflow detection
//  - path sensitive initialization (if/else, for, do/while, switch)
//  - pointer provenance and heap allocation state (use after free, double
//    free, invalid free, leaks)
//  - string buffer capacities and known string lengths
//  - stack balance (push/pop) to catch stack underflow
// ---------------------------------------------------------------------------

#define FLOW_STACK_MAX      1024
#define LABEL_STATE_MAX     8192

#define OP_ANY              0

#define HEAP_NONE           0
#define HEAP_ALLOCATED      1
#define HEAP_FREED          2

#define ORIGIN_UNKNOWN      0
#define ORIGIN_OBJECT       1
#define ORIGIN_HEAP         2

struct int_range
{
    U1 known;
    S8 lo ALIGN;
    S8 hi ALIGN;
};

// interprocedural value stack: used to feed caller arguments into a callee
// and callee return values back to the caller
struct int_range ip_vstack[IP_VSTACK_MAX];
S2 ip_vstack_ind = -1;
S2 ip_vstack_floor = -1;

// return values captured from the current (return) line in a sandbox
struct int_range ip_last_return[IP_MAX_RETURNS];
S2 ip_last_return_count = 0;
U1 ip_last_return_seen = 0;

struct flow_var
{
    U1 initialized;
    U1 pointer_set;
    U1 is_pointer;
    U1 pointer_null;
    U1 warned_null;
    U1 freed;
    U1 heap_state;
    U1 heap_size_known;
    U1 heap_origin;
    U1 range_known;
    U1 value_known;
    U1 dvalue_known;
    U1 may_zero;
    U1 str_len_known;
    U1 ptr_off_known;
    S8 heap_size ALIGN;
    S8 range_min ALIGN;
    S8 range_max ALIGN;
    S8 value ALIGN;
    double dvalue;
    S8 str_len ALIGN;
    S8 ptr_off ALIGN;
};

struct flow_state
{
    S8 n;
    struct flow_var *v;
    S8 stack_depth ALIGN;
    U1 stack_known;
};

#define FRAME_IF        1
#define FRAME_FOR       2
#define FRAME_DO        3
#define FRAME_SWITCH    4

#define FLOW_MAX_CASES  256

struct flow_frame
{
    S2 kind;
    U1 has_else;
    U1 has_then;
    U1 then_reachable;
    S2 cond_truth;              // 1 = always true, 0 = always false, -1 = unknown
    U1 has_else_pre;            // state of the false branch of the condition
    S2 case_count;              // number of switch cases seen so far
    S8 case_values[FLOW_MAX_CASES] ALIGN; // switch case constants
    S8 case_subject_hash[FLOW_MAX_CASES] ALIGN; // switch subject identifier hash
    struct flow_state pre;
    struct flow_state then_state;
    struct flow_state else_pre;
};

struct label_state
{
    U1 name[MAXSTRLEN + 1];
    U1 used;
    struct flow_state state;
};

struct flow_frame flow_frames[FLOW_STACK_MAX];
S2 flow_frame_ind = -1;

struct label_state *label_states = NULL;
S2 label_states_ind = -1;

U1 flow_reachable = 1;
S8 flow_stack_depth ALIGN = 0;
U1 flow_stack_known = 1;
U1 flow_loop_widen = 0;

// pending result of a library / allocation call, consumed by the next stpop
U1 pending_alloc = 0;
U1 pending_alloc_known = 0;
S8 pending_alloc_size ALIGN = -1;
S8 pending_alloc_elem_size ALIGN = 1;
U1 pending_result = 0;      // 1 = known library result
S8 pending_result_value ALIGN = 0;
S8 pending_result_min ALIGN = 0;
S8 pending_result_max ALIGN = -1;

// ---------------------------------------------------------------------------
// CBMC-style logical flow diagnostics
//
// The interval analysis above can prove that a condition is always true or
// always false at a given program point. The helpers below turn that
// information into user visible diagnostics about dead branches, unreachable
// code, missing return paths and related logical bugs.
// ---------------------------------------------------------------------------

S2 flow_cond_truth = -1;        // last analysed relation: -1 unknown, 1 true, 0 false
U1 flow_cond_stable = 0;        // relation only depends on constants / same value
U1 flow_logic_strict = 0;       // // (logic-strict): also use inferred ranges
U1 flow_unreachable_warned = 0; // suppress repeated warnings in one dead region
U1 flow_dead_branch = 0;        // unreachability was caused by a dead condition
U1 flow_line_unreachable = 0;   // current line is an ordinary statement in dead code
U1 flow_case_dead = 0;          // previous switch case ended before this case
U1 flow_assert_strict = 0;      // // (assert-strict): require assertions to be proven
U1 flow_missing_return = 0;     // function can reach (funcend) without a return
S8 flow_assert_count ALIGN = 0; // number of assertions checked

// evaluate a relation between two known intervals.
// returns -1 if the result is unknown, 1 if it is always true, 0 if always false.
S2 range_relation_truth (const struct int_range *l, U1 *op, const struct int_range *r)
{
    if (l->known == 0 || r->known == 0)
    {
        return (-1);
    }

    if (strcmp ((const char *) op, "==") == 0)
    {
        if (l->hi < r->lo || r->hi < l->lo)
        {
            return (0);     // disjoint intervals can never be equal
        }
        if (l->lo == l->hi && r->lo == r->hi && l->lo == r->lo)
        {
            return (1);
        }
        return (-1);
    }
    if (strcmp ((const char *) op, "!=") == 0)
    {
        if (l->hi < r->lo || r->hi < l->lo)
        {
            return (1);
        }
        if (l->lo == l->hi && r->lo == r->hi && l->lo == r->lo)
        {
            return (0);
        }
        return (-1);
    }
    if (strcmp ((const char *) op, "<") == 0)
    {
        if (l->hi < r->lo)
        {
            return (1);
        }
        if (l->lo >= r->hi)
        {
            return (0);
        }
        return (-1);
    }
    if (strcmp ((const char *) op, "<=") == 0)
    {
        if (l->hi <= r->lo)
        {
            return (1);
        }
        if (l->lo > r->hi)
        {
            return (0);
        }
        return (-1);
    }
    if (strcmp ((const char *) op, ">") == 0)
    {
        if (l->lo > r->hi)
        {
            return (1);
        }
        if (l->hi <= r->lo)
        {
            return (0);
        }
        return (-1);
    }
    if (strcmp ((const char *) op, ">=") == 0)
    {
        if (l->lo >= r->hi)
        {
            return (1);
        }
        if (l->hi < r->lo)
        {
            return (0);
        }
        return (-1);
    }
    return (-1);
}

// a literal or a const-* variable never changes at run time
U1 token_is_stable_constant (U1 *tok)
{
    S8 vi ALIGN;

    if (is_number_token (tok) == 1)
    {
        return (1);
    }
    vi = find_var_index (tok);
    if (vi >= 0 && vars[vi].is_const == 1)
    {
        return (1);
    }
    return (0);
}

// a comparison of a value with itself is a tautology / contradiction
S2 same_operand_truth (U1 *op)
{
    if (strcmp ((const char *) op, "==") == 0 ||
        strcmp ((const char *) op, "<=") == 0 ||
        strcmp ((const char *) op, ">=") == 0)
    {
        return (1);
    }
    if (strcmp ((const char *) op, "!=") == 0 ||
        strcmp ((const char *) op, "<") == 0 ||
        strcmp ((const char *) op, ">") == 0)
    {
        return (0);
    }
    return (-1);
}

void warn_condition_truth (S2 truth, U1 *line)
{
    linter_warnings++;
    warn_line_mark (linenum);
    if (truth == 1)
    {
        printf ("linter error: logical bug: condition is always true, the else branch is dead code!\n");
    }
    else
    {
        printf ("linter error: logical bug: condition is always false, the then branch is dead code!\n");
    }
    printf (" > %s\n", line);
}

void warn_condition_contradiction (U1 *line)
{
    linter_warnings++;
    warn_line_mark (linenum);
    printf ("linter error: logical bug: condition can never be true here (it contradicts the preceding checks)!\n");
    printf (" > %s\n", line);
}

void warn_dead_loop (U1 *line)
{
    linter_warnings++;
    warn_line_mark (linenum);
    printf ("linter error: logical bug: loop condition is always false, the loop body is dead code!\n");
    printf (" > %s\n", line);
}

void warn_duplicate_case (S8 value, U1 *line)
{
    linter_warnings++;
    printf ("linter error: logical bug: duplicate switch case value %lli!\n", value);
    printf (" > %s\n", line);
}

// stable 64 bit hash of an identifier, used to remember switch subjects
S8 flow_token_hash (U1 *tok)
{
    S8 h ALIGN = 1469598103934665603LL;
    S2 i;

    for (i = 0; tok[i] != '\0'; i++)
    {
        h ^= (S8) tok[i];
        h *= 1099511628211LL;
    }
    return (h);
}

// The interval state inside a loop body describes the first iteration only.
// Conditions that depend on values modified by the loop can therefore look
// constant without being a real logical bug. Suppress those diagnostics while
// at least one loop is open.
U1 flow_inside_loop (void)
{
    S2 k;

    for (k = 0; k <= flow_frame_ind; k++)
    {
        if (flow_frames[k].kind == FRAME_FOR || flow_frames[k].kind == FRAME_DO)
        {
            return (1);
        }
    }
    return (0);
}

// Epilogue and terminator instructions are emitted by the compiler after an
// explicit exit or an unconditional jump. They are technically unreachable but
// carry no intent, unlike a skipped assignment or call. Do not report them.
U1 flow_line_is_ignorable (S2 nt, U1 tokens[][MAXSTRLEN + 1])
{
    S2 i;
    U1 *op = NULL;
    U1 is_intr = 0;

    if (nt < 2)
    {
        return (0);
    }
    op = tokens[1];
    if (strcmp ((const char *) op, "loadreg") == 0 ||
        strcmp ((const char *) op, "break") == 0 ||
        strcmp ((const char *) op, "rts") == 0)
    {
        return (1);
    }
    // "unsafe-end" is split into "unsafe", "-", "end" by the tokenizer
    if (strcmp ((const char *) op, "unsafe") == 0 && nt > 3 &&
        strcmp ((const char *) tokens[2], "-") == 0 &&
        strcmp ((const char *) tokens[3], "end") == 0)
    {
        return (1);
    }
    for (i = 0; i < nt; i++)
    {
        if (strcmp ((const char *) tokens[i], "intr0") == 0 ||
            strcmp ((const char *) tokens[i], "intr1") == 0)
        {
            is_intr = 1;
            break;
        }
    }
    if (is_intr == 1)
    {
        // 39/40: memory bounds on/off, 255: program exit. These are the
        // usual tail markers after the last real instruction of a function.
        if (strcmp ((const char *) op, "39") == 0 ||
            strcmp ((const char *) op, "40") == 0 ||
            strcmp ((const char *) op, "255") == 0)
        {
            return (1);
        }
    }
    return (0);
}

void warn_unreachable_code (S2 nt, U1 tokens[][MAXSTRLEN + 1], U1 *line)
{
    S2 k;

    // loop bodies are only analyzed for one iteration; do not turn a
    // first-iteration reachability into a permanent claim.
    if (flow_unreachable_warned == 1 || flow_inside_loop () == 1)
    {
        return;
    }
    // a branch which was already reported as dead by a constant condition
    // does not need a second unreachable-code warning
    for (k = 0; k <= flow_frame_ind; k++)
    {
        if ((flow_frames[k].kind == FRAME_IF ||
             flow_frames[k].kind == FRAME_FOR) &&
            flow_frames[k].cond_truth != -1)
        {
            return;
        }
    }
    if (flow_line_is_ignorable (nt, tokens) == 1)
    {
        return;
    }
    flow_unreachable_warned = 1;
    linter_warnings++;
    warn_line_mark (linenum);
    printf ("linter error: logical bug: unreachable code (control flow ends before this line)!\n");
    printf (" > %s\n", line);
}

// one-time initialization of label state table
S2 init_label_states (void)
{
    if (label_states == NULL)
    {
        label_states = (struct label_state *) calloc (LABEL_STATE_MAX, sizeof (struct label_state));
        if (label_states == NULL)
        {
            printf ("error: can't allocate label state table!\n");
            return (1);
        }
        label_states_ind = -1;
    }
    return (0);
}

void free_flow_state (struct flow_state *s)
{
    if (s->v != NULL)
    {
        free (s->v);
        s->v = NULL;
    }
    s->n = 0;
    s->stack_depth = 0;
    s->stack_known = 1;
}

S2 capture_flow_state (struct flow_state *s)
{
    S8 i ALIGN;

    s->n = vars_ind + 1;
    s->stack_depth = flow_stack_depth;
    s->stack_known = flow_stack_known;

    if (s->n <= 0)
    {
        s->v = NULL;
        return (0);
    }
    s->v = (struct flow_var *) calloc ((size_t) s->n, sizeof (struct flow_var));
    if (s->v == NULL)
    {
        printf ("error: can't allocate flow state!\n");
        s->n = 0;
        return (1);
    }
    for (i = 0; i <= vars_ind; i++)
    {
        s->v[i].initialized = vars[i].initialized;
        s->v[i].pointer_set = vars[i].pointer_set;
        s->v[i].is_pointer = vars[i].is_pointer;
        s->v[i].pointer_null = vars[i].pointer_null;
        s->v[i].warned_null = vars[i].warned_null;
        s->v[i].freed = vars[i].freed;
        s->v[i].heap_state = vars[i].heap_state;
        s->v[i].heap_size_known = vars[i].heap_size_known;
        s->v[i].heap_origin = vars[i].heap_origin;
        s->v[i].range_known = vars[i].range_known;
        s->v[i].value_known = vars[i].value_known;
        s->v[i].dvalue_known = vars[i].dvalue_known;
        s->v[i].may_zero = vars[i].may_zero;
        s->v[i].str_len_known = vars[i].str_len_known;
        s->v[i].ptr_off_known = vars[i].ptr_off_known;
        s->v[i].heap_size = vars[i].heap_size;
        s->v[i].range_min = vars[i].range_min;
        s->v[i].range_max = vars[i].range_max;
        s->v[i].value = vars[i].value;
        s->v[i].dvalue = vars[i].dvalue;
        s->v[i].str_len = vars[i].str_len;
        s->v[i].ptr_off = vars[i].ptr_off;
    }
    return (0);
}

void restore_flow_state (const struct flow_state *s)
{
    S8 i ALIGN;

    for (i = 0; i <= vars_ind && i < s->n; i++)
    {
        vars[i].initialized = s->v[i].initialized;
        vars[i].pointer_set = s->v[i].pointer_set;
        vars[i].is_pointer = s->v[i].is_pointer;
        vars[i].pointer_null = s->v[i].pointer_null;
        vars[i].warned_null = s->v[i].warned_null;
        vars[i].freed = s->v[i].freed;
        vars[i].heap_state = s->v[i].heap_state;
        vars[i].heap_size_known = s->v[i].heap_size_known;
        vars[i].heap_origin = s->v[i].heap_origin;
        vars[i].range_known = s->v[i].range_known;
        vars[i].value_known = s->v[i].value_known;
        vars[i].dvalue_known = s->v[i].dvalue_known;
        vars[i].may_zero = s->v[i].may_zero;
        vars[i].str_len_known = s->v[i].str_len_known;
        vars[i].ptr_off_known = s->v[i].ptr_off_known;
        vars[i].heap_size = s->v[i].heap_size;
        vars[i].range_min = s->v[i].range_min;
        vars[i].range_max = s->v[i].range_max;
        vars[i].value = s->v[i].value;
        vars[i].dvalue = s->v[i].dvalue;
        vars[i].str_len = s->v[i].str_len;
        vars[i].ptr_off = s->v[i].ptr_off;
    }
    flow_stack_depth = s->stack_depth;
    flow_stack_known = s->stack_known;
}

// merge the state "b" into the state "a". a and b are not modified.
void merge_flow_states_to_current (const struct flow_state *a, const struct flow_state *b)
{
    S8 i ALIGN;
    const struct flow_var *fa;
    const struct flow_var *fb;

    for (i = 0; i <= vars_ind; i++)
    {
        fa = (i < a->n ? &a->v[i] : NULL);
        fb = (i < b->n ? &b->v[i] : NULL);

        if (fa == NULL)
        {
            if (fb == NULL)
            {
                continue;
            }
            vars[i].initialized = fb->initialized;
            vars[i].range_known = fb->range_known;
            vars[i].range_min = fb->range_min;
            vars[i].range_max = fb->range_max;
            vars[i].may_zero = fb->may_zero;
            continue;
        }
        if (fb == NULL)
        {
            vars[i].initialized = fa->initialized;
            vars[i].range_known = fa->range_known;
            vars[i].range_min = fa->range_min;
            vars[i].range_max = fa->range_max;
            vars[i].may_zero = fa->may_zero;
            continue;
        }

        // definite initialization: initialized on both paths
        vars[i].initialized = (U1) (fa->initialized && fb->initialized);

        // a value is only known if it is identical on both paths
        vars[i].value_known = (U1) (fa->value_known && fb->value_known &&
                                    fa->value == fb->value);
        if (vars[i].value_known == 1)
        {
            vars[i].value = fa->value;
        }
        vars[i].dvalue_known = (U1) (fa->dvalue_known && fb->dvalue_known &&
                                     fa->dvalue == fb->dvalue);
        if (vars[i].dvalue_known == 1)
        {
            vars[i].dvalue = fa->dvalue;
        }

        // range: union (hull) of both paths
        if (fa->range_known == 1 && fb->range_known == 1)
        {
            vars[i].range_known = 1;
            vars[i].range_min = (fa->range_min < fb->range_min ? fa->range_min : fb->range_min);
            vars[i].range_max = (fa->range_max > fb->range_max ? fa->range_max : fb->range_max);
            if (vars[i].value_known == 1)
            {
                vars[i].range_min = vars[i].value;
                vars[i].range_max = vars[i].value;
            }
        }
        else
        {
            vars[i].range_known = 0;
        }
        vars[i].may_zero = (U1) (fa->may_zero || fb->may_zero);

        // pointer state: be conservative, a pointer may be null/freed on one path
        vars[i].pointer_set = (U1) (fa->pointer_set || fb->pointer_set);
        vars[i].is_pointer = (U1) (fa->is_pointer || fb->is_pointer);
        vars[i].pointer_null = (U1) (fa->pointer_null || fb->pointer_null);
        vars[i].freed = (U1) (fa->freed || fb->freed);
        vars[i].warned_null = (U1) (fa->warned_null && fb->warned_null);
        if (fa->heap_state == HEAP_FREED || fb->heap_state == HEAP_FREED)
        {
            vars[i].heap_state = HEAP_FREED;
        }
        else if (fa->heap_state == HEAP_ALLOCATED && fb->heap_state == HEAP_ALLOCATED)
        {
            vars[i].heap_state = HEAP_ALLOCATED;
        }
        else
        {
            vars[i].heap_state = HEAP_NONE;
        }

        if (fa->heap_size_known == 1 && fb->heap_size_known == 1 &&
            fa->heap_size == fb->heap_size)
        {
            vars[i].heap_size_known = 1;
            vars[i].heap_size = fa->heap_size;
        }
        else
        {
            vars[i].heap_size_known = 0;
            vars[i].heap_size = -1;
        }

        if (fa->heap_origin == fb->heap_origin)
        {
            vars[i].heap_origin = fa->heap_origin;
        }
        else
        {
            vars[i].heap_origin = ORIGIN_UNKNOWN;
        }

        if (fa->str_len_known == 1 && fb->str_len_known == 1 &&
            fa->str_len == fb->str_len)
        {
            vars[i].str_len_known = 1;
            vars[i].str_len = fa->str_len;
        }
        else
        {
            vars[i].str_len_known = 0;
        }

        if (fa->ptr_off_known == 1 && fb->ptr_off_known == 1 &&
            fa->ptr_off == fb->ptr_off)
        {
            vars[i].ptr_off_known = 1;
            vars[i].ptr_off = fa->ptr_off;
        }
        else
        {
            vars[i].ptr_off_known = 0;
        }
    }

    // stack depth is only known if both paths agree
    if (a->stack_known == 1 && b->stack_known == 1 &&
        a->stack_depth == b->stack_depth)
    {
        flow_stack_depth = a->stack_depth;
        flow_stack_known = 1;
    }
    else
    {
        flow_stack_known = 0;
    }
}

// merge a second state into an already captured state (used for label joins)
void merge_into_captured_state (struct flow_state *a, const struct flow_state *b)
{
    S8 i ALIGN;
    S8 n ALIGN = (a->n > b->n ? a->n : b->n);
    struct flow_var *nv;

    if (n <= 0)
    {
        return;
    }
    if (a->n < n)
    {
        nv = (struct flow_var *) realloc (a->v, (size_t) n * sizeof (struct flow_var));
        if (nv == NULL)
        {
            return;
        }
        memset (&nv[a->n], 0, (size_t) (n - a->n) * sizeof (struct flow_var));
        a->v = nv;
        a->n = n;
    }

    for (i = 0; i < n; i++)
    {
        if (i >= b->n)
        {
            continue;
        }
        a->v[i].initialized = (U1) (a->v[i].initialized && b->v[i].initialized);
        a->v[i].pointer_set = (U1) (a->v[i].pointer_set || b->v[i].pointer_set);
        a->v[i].is_pointer = (U1) (a->v[i].is_pointer || b->v[i].is_pointer);
        a->v[i].pointer_null = (U1) (a->v[i].pointer_null || b->v[i].pointer_null);
        a->v[i].freed = (U1) (a->v[i].freed || b->v[i].freed);
        if (b->v[i].heap_state == HEAP_FREED || a->v[i].heap_state == HEAP_FREED)
        {
            a->v[i].heap_state = HEAP_FREED;
        }
        else if (b->v[i].heap_state == HEAP_ALLOCATED || a->v[i].heap_state == HEAP_ALLOCATED)
        {
            a->v[i].heap_state = HEAP_ALLOCATED;
        }
        if (a->v[i].range_known == 1 && b->v[i].range_known == 1)
        {
            a->v[i].range_min = (a->v[i].range_min < b->v[i].range_min ? a->v[i].range_min : b->v[i].range_min);
            a->v[i].range_max = (a->v[i].range_max > b->v[i].range_max ? a->v[i].range_max : b->v[i].range_max);
        }
        else
        {
            a->v[i].range_known = 0;
        }
        a->v[i].may_zero = (U1) (a->v[i].may_zero || b->v[i].may_zero);
        if (a->v[i].str_len_known == 1 && b->v[i].str_len_known == 1 &&
            a->v[i].str_len != b->v[i].str_len)
        {
            a->v[i].str_len_known = 0;
        }
    }
}

struct int_range range_unknown (void)
{
    struct int_range r;

    r.known = 0;
    r.lo = 0;
    r.hi = 0;
    return r;
}

struct int_range range_exact (S8 v)
{
    struct int_range r;

    r.known = 1;
    r.lo = v;
    r.hi = v;
    return r;
}

struct int_range range_of_var (U1 *name)
{
    S8 vi ALIGN;

    vi = find_var_index (name);
    if (vi >= 0)
    {
        if (vars[vi].range_known == 1)
        {
            return (struct int_range) { 1, vars[vi].range_min, vars[vi].range_max };
        }
        if (vars[vi].value_known == 1)
        {
            return range_exact (vars[vi].value);
        }
    }
    return range_unknown ();
}

S2 operator_precedence (U1 *tok)
{
    S2 len;

    if (tok[0] == '\0')
    {
        return (-1);
    }
    len = (S2) strlen_safe ((const char *) tok, MAXSTRLEN);
    if (len >= 2 && is_operator_suffix (tok[len - 1]))
    {
        // typed operator suffix, ignore suffix for precedence
    }

    if (strcmp ((const char *) tok, "||") == 0) return (1);
    if (strcmp ((const char *) tok, "&&") == 0) return (2);
    if (strcmp ((const char *) tok, "|") == 0) return (3);
    if (strcmp ((const char *) tok, "^") == 0) return (4);
    if (strcmp ((const char *) tok, "&") == 0) return (5);
    if (strcmp ((const char *) tok, "==") == 0 || strcmp ((const char *) tok, "!=") == 0) return (6);
    if (tok[0] == '<' && tok[1] == '=') return (7);
    if (tok[0] == '>' && tok[1] == '=') return (7);
    if (tok[0] == '<' && tok[1] == '|') return (8);
    if (tok[0] == '>' && tok[1] == '|') return (8);
    if (tok[0] == '<' && tok[1] != '|') return (7);
    if (tok[0] == '>' && tok[1] != '|') return (7);
    if (tok[0] == '+' || tok[0] == '-') return (9);
    if (tok[0] == '*' || tok[0] == '/' || tok[0] == '%') return (10);
    return (-1);
}

void range_clamp (__int128 v, struct int_range *r, U1 *overflow)
{
    if (v < (__int128) LLONG_MIN)
    {
        r->lo = LLONG_MIN;
        *overflow = 1;
    }
    else if (v > (__int128) LLONG_MAX)
    {
        r->hi = LLONG_MAX;
        *overflow = 1;
    }
}

U1 range_apply (U1 *op, struct int_range a, struct int_range b, struct int_range *r, U1 *overflow)
{
    __int128 lo;
    __int128 hi;
    __int128 v1, v2, v3, v4;
    U1 first;

    r->known = 0;
    if (a.known == 0 || b.known == 0)
    {
        return (0);
    }

    first = op[0];

    if (strcmp ((const char *) op, "==") == 0 || strcmp ((const char *) op, "!=") == 0 ||
        (first == '<' && op[1] != '|') || (first == '>' && op[1] != '|') ||
        strcmp ((const char *) op, "&&") == 0 || strcmp ((const char *) op, "||") == 0)
    {
        // comparison / logic: result is in {0,1}. If both operands are exact
        // the exact result can be computed.
        S8 res ALIGN;

        if (a.lo == a.hi && b.lo == b.hi)
        {
            U1 ov = 0;
            if (eval_apply (op, a.lo, b.lo, &res, &ov) == 1)
            {
                *r = range_exact (res);
                return (1);
            }
        }
        r->known = 1;
        r->lo = 0;
        r->hi = 1;
        return (1);
    }

    lo = 0;
    hi = 0;

    switch (first)
    {
        case '+':
            lo = (__int128) a.lo + (__int128) b.lo;
            hi = (__int128) a.hi + (__int128) b.hi;
            break;

        case '-':
            lo = (__int128) a.lo - (__int128) b.hi;
            hi = (__int128) a.hi - (__int128) b.lo;
            break;

        case '*':
            v1 = (__int128) a.lo * (__int128) b.lo;
            v2 = (__int128) a.lo * (__int128) b.hi;
            v3 = (__int128) a.hi * (__int128) b.lo;
            v4 = (__int128) a.hi * (__int128) b.hi;
            lo = v1; hi = v1;
            if (v2 < lo) lo = v2;
            if (v2 > hi) hi = v2;
            if (v3 < lo) lo = v3;
            if (v3 > hi) hi = v3;
            if (v4 < lo) lo = v4;
            if (v4 > hi) hi = v4;
            break;

        case '/':
        case '%':
            if (b.lo <= 0 && b.hi >= 0)
            {
                return (0);     // divisor can be zero: result unknown
            }
            if (first == '/')
            {
                v1 = (__int128) a.lo / (__int128) b.lo;
                v2 = (__int128) a.lo / (__int128) b.hi;
                v3 = (__int128) a.hi / (__int128) b.lo;
                v4 = (__int128) a.hi / (__int128) b.hi;
            }
            else
            {
                v1 = (__int128) a.lo % (__int128) b.lo;
                v2 = (__int128) a.lo % (__int128) b.hi;
                v3 = (__int128) a.hi % (__int128) b.lo;
                v4 = (__int128) a.hi % (__int128) b.hi;
            }
            lo = v1; hi = v1;
            if (v2 < lo) lo = v2;
            if (v2 > hi) hi = v2;
            if (v3 < lo) lo = v3;
            if (v3 > hi) hi = v3;
            if (v4 < lo) lo = v4;
            if (v4 > hi) hi = v4;
            break;

        case '&':
            if (a.lo == a.hi && b.lo == b.hi)
            {
                *r = range_exact (a.lo & b.lo);
                return (1);
            }
            r->known = 1;
            r->lo = 0;
            r->hi = (a.hi > b.hi ? a.hi : b.hi);
            if (r->hi < 0)
            {
                return (0);
            }
            return (1);

        case '|':
        case '^':
            if (a.lo == a.hi && b.lo == b.hi)
            {
                *r = range_exact (first == '|' ? (a.lo | b.lo) : (a.lo ^ b.lo));
                return (1);
            }
            return (0);

        case '<':
            if (op[1] == '|')
            {
                if (b.lo < 0 || b.hi > 63)
                {
                    return (0);
                }
                lo = (__int128) a.lo << (S2) b.lo;
                hi = (__int128) a.hi << (S2) b.hi;
                if (b.lo != b.hi)
                {
                    // conservative: take the widest hull
                    __int128 t1 = (__int128) a.lo << (S2) b.hi;
                    __int128 t2 = (__int128) a.hi << (S2) b.lo;
                    if (t1 < lo) lo = t1;
                    if (t1 > hi) hi = t1;
                    if (t2 < lo) lo = t2;
                    if (t2 > hi) hi = t2;
                }
            }
            else
            {
                return (0);
            }
            break;

        case '>':
            if (op[1] == '|')
            {
                if (b.lo < 0 || b.hi > 63)
                {
                    return (0);
                }
                lo = (__int128) a.lo >> (S2) b.lo;
                hi = (__int128) a.hi >> (S2) b.hi;
                if (b.lo != b.hi)
                {
                    __int128 t1 = (__int128) a.lo >> (S2) b.hi;
                    __int128 t2 = (__int128) a.hi >> (S2) b.lo;
                    if (t1 < lo) lo = t1;
                    if (t1 > hi) hi = t1;
                    if (t2 < lo) lo = t2;
                    if (t2 > hi) hi = t2;
                }
            }
            else
            {
                return (0);
            }
            break;

        default:
            return (0);
    }

    r->known = 1;
    r->lo = (lo < (__int128) LLONG_MIN ? LLONG_MIN : (lo > (__int128) LLONG_MAX ? LLONG_MAX : (S8) lo));
    r->hi = (hi < (__int128) LLONG_MIN ? LLONG_MIN : (hi > (__int128) LLONG_MAX ? LLONG_MAX : (S8) hi));
    if (lo < (__int128) LLONG_MIN || hi > (__int128) LLONG_MAX)
    {
        *overflow = 1;
    }
    (void) range_clamp;
    return (1);
}

// evaluate an integer expression to an interval
U1 range_eval (S2 nt, U1 tokens[][MAXSTRLEN + 1], S2 start, S2 end, struct int_range *r, U1 *overflow)
{
    S2 i;
    S2 close;
    S2 op_pos = -1;
    S2 op_prec_best = 1000;
    S2 prec;
    struct int_range a;
    struct int_range b;
    U1 aov = 0;
    U1 bov = 0;

    r->known = 0;
    if (start < 0 || end < 0 || start > end || end >= nt)
    {
        return (0);
    }

    // strip enclosing parentheses
    if (strcmp ((const char *) tokens[start], "(") == 0)
    {
        close = find_close_paren (nt, tokens, start);
        if (close == end)
        {
            return (range_eval (nt, tokens, start + 1, end - 1, r, overflow));
        }
    }

    if (start == end)
    {
        if (is_number_token (tokens[start]) == 1)
        {
            char *endptr = NULL;
            S8 v ALIGN;

            errno = 0;
            v = (S8) strtoll ((const char *) tokens[start], &endptr, 10);
            if (errno == ERANGE)
            {
                *overflow = 1;
                return (0);
            }
            *r = range_exact (v);
            return (1);
        }
        if (is_ident_start (tokens[start][0]) == 1 &&
            token_is_keyword (tokens[start]) == 0)
        {
            *r = range_of_var (tokens[start]);
            return (r->known);
        }
        return (0);
    }

    // array element access: value unknown
    for (i = start; i <= end; i++)
    {
        if (strcmp ((const char *) tokens[i], "[") == 0)
        {
            return (0);
        }
    }

    // find the lowest precedence top level operator. For left associative
    // infix and for postfix (RPN) expressions the right most operator of the
    // lowest precedence is the root.
    for (i = start; i <= end; i++)
    {
        if (strcmp ((const char *) tokens[i], "(") == 0)
        {
            close = find_close_paren (nt, tokens, i);
            if (close < 0 || close > end)
            {
                return (0);
            }
            i = close;
            continue;
        }
        if (strcmp ((const char *) tokens[i], "[") == 0)
        {
            close = find_close_bracket (nt, tokens, i);
            if (close < 0 || close > end)
            {
                return (0);
            }
            i = close;
            continue;
        }
        prec = operator_precedence (tokens[i]);
        if (prec > 0)
        {
            if (prec <= op_prec_best)
            {
                op_prec_best = prec;
                op_pos = i;
            }
        }
    }

    if (op_pos > start && op_pos < end)
    {
        if (range_eval (nt, tokens, start, op_pos - 1, &a, &aov) == 0)
        {
            return (0);
        }
        if (range_eval (nt, tokens, op_pos + 1, end, &b, &bov) == 0)
        {
            return (0);
        }
        if (aov == 1 || bov == 1)
        {
            *overflow = 1;
        }
        return (range_apply (tokens[op_pos], a, b, r, overflow));
    }

    return (0);
}

void range_intersect_lo (S8 *lo, S8 *hi, S8 nlo, S8 nhi)
{
    if (nlo > *lo)
    {
        *lo = nlo;
    }
    if (nhi < *hi)
    {
        *hi = nhi;
    }
}

U1 range_refine_compare (U1 *var, U1 *op, S8 other_lo, S8 other_hi, U1 truth)
{
    S8 vi ALIGN;
    S8 lo ALIGN;
    S8 hi ALIGN;
    S8 nlo ALIGN;
    S8 nhi ALIGN;
    U1 first = op[0];
    U1 is_le = (U1) (op[1] == '=');
    U1 is_shift = (U1) (op[1] == '|');

    if (is_shift == 1)
    {
        return (0);
    }
    vi = find_var_index (var);
    if (vi < 0)
    {
        return (0);
    }
    if (vars[vi].range_known == 1)
    {
        lo = vars[vi].range_min;
        hi = vars[vi].range_max;
    }
    else if (vars[vi].value_known == 1)
    {
        lo = vars[vi].value;
        hi = vars[vi].value;
    }
    else
    {
        lo = LLONG_MIN;
        hi = LLONG_MAX;
    }

    nlo = lo;
    nhi = hi;

    if (truth == 1)
    {
        if (first == '<')
        {
            if (is_le == 1)
            {
                nhi = other_hi;
            }
            else
            {
                nhi = other_hi - 1;
            }
        }
        else if (first == '>')
        {
            if (is_le == 1)
            {
                nlo = other_lo;
            }
            else
            {
                nlo = other_lo + 1;
            }
        }
        else if (strcmp ((const char *) op, "==") == 0)
        {
            nlo = other_lo;
            nhi = other_hi;
        }
        else if (strcmp ((const char *) op, "!=") == 0)
        {
            // cannot represent a hole in the middle of an interval, but
            // excluding a boundary value is easy and very useful.
            if (other_lo == other_hi)
            {
                S8 c ALIGN = other_lo;

                if (c == lo && c == hi)
                {
                    nhi = lo - 1;       // contradiction
                }
                else if (c == lo)
                {
                    nlo = c + 1;
                }
                else if (c == hi)
                {
                    nhi = c - 1;
                }
                else
                {
                    if (c == 0)
                    {
                        // excluding zero proves the value is not zero
                        vars[vi].may_zero = 0;
                    }
                    return (0);
                }
            }
            else
            {
                return (0);
            }
        }
        else
        {
            return (0);
        }
    }
    else
    {
        if (first == '<')
        {
            if (is_le == 1)
            {
                nlo = other_lo + 1;
            }
            else
            {
                nlo = other_lo;
            }
        }
        else if (first == '>')
        {
            if (is_le == 1)
            {
                nhi = other_hi - 1;
            }
            else
            {
                nhi = other_hi;
            }
        }
        else if (strcmp ((const char *) op, "==") == 0)
        {
            // not equal: exclude a boundary value if possible
            if (other_lo == other_hi)
            {
                S8 c ALIGN = other_lo;

                if (c == lo && c == hi)
                {
                    nhi = lo - 1;       // contradiction
                }
                else if (c == lo)
                {
                    nlo = c + 1;
                }
                else if (c == hi)
                {
                    nhi = c - 1;
                }
                else
                {
                    if (c == 0)
                    {
                        // excluding zero proves the value is not zero
                        vars[vi].may_zero = 0;
                    }
                    return (0);
                }
            }
            else
            {
                return (0);
            }
        }
        else if (strcmp ((const char *) op, "!=") == 0)
        {
            nlo = other_lo;
            nhi = other_hi;
        }
        else
        {
            return (0);
        }
    }

    if (flow_loop_widen == 1 && truth == 1 && first == '<')
    {
        lo = (vars[vi].range_known == 1 ? vars[vi].range_min :
              (vars[vi].value_known == 1 ? vars[vi].value : LLONG_MIN));
        hi = (is_le == 1 ? other_hi : other_hi - 1);
        if (lo > hi)
        {
            return (2);
        }
    }
    else if (flow_loop_widen == 1 && truth == 1 && first == '>')
    {
        lo = (is_le == 1 ? other_lo : other_lo + 1);
        hi = (vars[vi].range_known == 1 ? vars[vi].range_max :
              (vars[vi].value_known == 1 ? vars[vi].value : LLONG_MAX));
        if (lo > hi)
        {
            return (2);
        }
    }
    else
    {
        range_intersect_lo (&lo, &hi, nlo, nhi);
    }
    if (lo > hi)
    {
        return (2);     // contradiction: branch is impossible
    }
    vars[vi].range_known = 1;
    vars[vi].range_min = lo;
    vars[vi].range_max = hi;
    vars[vi].may_zero = (U1) (lo <= 0 && hi >= 0);
    if (vars[vi].is_const == 0)
    {
        // keep exact value information consistent with the refined range:
        // a stale exact value would defeat the interval based checks.
        if (lo == hi)
        {
            vars[vi].value_known = 1;
            vars[vi].value = lo;
            vars[vi].dvalue_known = 0;
        }
        else
        {
            vars[vi].value_known = 0;
            vars[vi].dvalue_known = 0;
        }
    }
    return (1);
}

// refine variable ranges from a comparison in an if / for / while condition.
// returns 2 if the condition is impossible, 1 if refined, 0 if untouched.
S2 refine_condition (S2 nt, U1 tokens[][MAXSTRLEN + 1], U1 truth)
{
    S2 i;
    S2 op = -1;
    S2 left;
    S2 right;
    S8 left_start;
    S8 right_end;
    S2 lend;
    S2 rstart;
    S2 rend;
    S2 lop_pos;
    S2 rop_pos;
    struct int_range lr;
    struct int_range rr;
    U1 lov = 0;
    U1 rov = 0;
    S2 ret = 0;
    S2 rr_ret;
    U1 *lop;
    U1 *rop;

    flow_cond_truth = -1;
    flow_cond_stable = 0;

    for (i = 0; i < nt; i++)
    {
        if (strcmp ((const char *) tokens[i], "(") == 0)
        {
            S2 c = find_close_paren (nt, tokens, i);
            if (c > i)
            {
                // only scan inside condition groups, not skip them completely
            }
        }
        if (strcmp ((const char *) tokens[i], "==") == 0 ||
            strcmp ((const char *) tokens[i], "!=") == 0 ||
            strcmp ((const char *) tokens[i], "<") == 0 ||
            strcmp ((const char *) tokens[i], "<=") == 0 ||
            strcmp ((const char *) tokens[i], ">") == 0 ||
            strcmp ((const char *) tokens[i], ">=") == 0)
        {
            op = i;
            break;
        }
    }
    if (op < 0)
    {
        return (0);
    }

    left = op - 1;
    right = op + 1;
    if (left < 0)
    {
        return (0);
    }
    if (right < nt && strcmp ((const char *) tokens[right], ")") == 0)
    {
        // postfix comparison: (left right op)
        S2 r_end = op - 1;
        S2 r_start;
        S2 l_end;
        S2 l_start;

        if (r_end < 0)
        {
            return (0);
        }
        if (strcmp ((const char *) tokens[r_end], ")") == 0)
        {
            r_start = find_open_paren (nt, tokens, r_end);
            if (r_start < 0)
            {
                return (0);
            }
        }
        else
        {
            r_start = r_end;
        }
        l_end = r_start - 1;
        if (l_end < 0)
        {
            return (0);
        }
        if (strcmp ((const char *) tokens[l_end], ")") == 0)
        {
            l_start = find_open_paren (nt, tokens, l_end);
            if (l_start < 0)
            {
                return (0);
            }
        }
        else if (is_number_token (tokens[l_end]) == 0 &&
                 is_ident_start (tokens[l_end][0]) == 0 &&
                 l_end >= 2)
        {
            // left operand is a postfix expression ending at l_end
            S2 le2 = l_end - 1;
            S2 ls2;

            if (strcmp ((const char *) tokens[le2], ")") == 0)
            {
                ls2 = find_open_paren (nt, tokens, le2);
                if (ls2 < 0)
                {
                    return (0);
                }
            }
            else
            {
                ls2 = le2;
            }
            l_start = ls2 - 1;
            if (l_start < 0)
            {
                return (0);
            }
            if (strcmp ((const char *) tokens[l_start], ")") == 0)
            {
                l_start = find_open_paren (nt, tokens, l_start);
                if (l_start < 0)
                {
                    return (0);
                }
            }
        }
        else
        {
            l_start = l_end;
        }
        left_start = l_start;
        lend = l_end;
        rstart = r_start;
        rend = r_end;
    }
    else
    {
        if (right >= nt)
        {
            return (0);
        }
        // unwrap a group before the operator
        if (strcmp ((const char *) tokens[left], ")") == 0)
        {
            left = find_open_paren (nt, tokens, left);
            if (left < 0)
            {
                return (0);
            }
        }
        // unwrap a group after the operator
        if (strcmp ((const char *) tokens[right], "(") == 0)
        {
            S2 c = find_close_paren (nt, tokens, right);
            if (c < 0)
            {
                return (0);
            }
            right = c;
        }

        left_start = left;
        right_end = right;

        if (strcmp ((const char *) tokens[left], "(") == 0)
        {
            left_start = left + 1;
        }
        if (strcmp ((const char *) tokens[right], ")") == 0)
        {
            right_end = right - 1;
        }
        lend = op - 1;
        rstart = op + 1;
        rend = right_end;
    }
    if (left_start > op - 1 || rstart > rend)
    {
        return (0);
    }

    lr = range_unknown ();
    rr = range_unknown ();
    (void) range_eval (nt, tokens, left_start, lend, &lr, &lov);
    (void) range_eval (nt, tokens, rstart, rend, &rr, &rov);
    (void) lov;
    (void) rov;

    lop_pos = (S2) left_start;
    rop_pos = rstart;
    while (lop_pos < lend && strcmp ((const char *) tokens[lop_pos], "(") == 0)
    {
        lop_pos++;
    }
    while (rop_pos < rend && strcmp ((const char *) tokens[rop_pos], "(") == 0)
    {
        rop_pos++;
    }
    lop = tokens[lop_pos];
    rop = tokens[rop_pos];

    // CBMC-style feasibility: if the compiler can prove the relation from the
    // current interval state, remember whether the condition is a tautology or
    // a contradiction. This is independent of the branch being refined.
    flow_cond_truth = range_relation_truth (&lr, tokens[op], &rr);
    if (left_start == lend && rstart == rend &&
        token_is_stable_constant (lop) == 1 &&
        token_is_stable_constant (rop) == 1)
    {
        // both operands are compile time constants: the comparison is stable
        flow_cond_stable = 1;
    }
    if (left_start == lend && rstart == rend &&
        strcmp ((const char *) lop, (const char *) rop) == 0 &&
        (is_ident_start (lop[0]) == 1 || is_number_token (lop) == 1) &&
        token_is_keyword (lop) == 0)
    {
        // (x x ==), (x x !=), ... : compare the same value with itself.
        // This holds regardless of the current interval. Floating point
        // values can be NaN, where x != x is true, so skip doubles.
        S2 self_truth = same_operand_truth (tokens[op]);
        S2 self_vi = find_var_index (lop);
        U1 self_float = 0;

        if (self_vi >= 0 && vars[self_vi].type == DOUBLEFLOAT)
        {
            self_float = 1;
        }
        if (self_truth != -1 && self_float == 0)
        {
            flow_cond_truth = self_truth;
            flow_cond_stable = 1;
        }
    }

    if (lr.known == 1 && is_ident_start (rop[0]) == 1 &&
        token_is_keyword (rop) == 0 && rr.known == 0)
    {
        // refine the right variable using the left range, inverted operator
        U1 inv[MAXSTRLEN + 1];

        if (strcmp ((const char *) tokens[op], "<") == 0) strcpy ((char *) inv, ">");
        else if (strcmp ((const char *) tokens[op], "<=") == 0) strcpy ((char *) inv, ">=");
        else if (strcmp ((const char *) tokens[op], ">") == 0) strcpy ((char *) inv, "<");
        else if (strcmp ((const char *) tokens[op], ">=") == 0) strcpy ((char *) inv, "<=");
        else strcpy ((char *) inv, (const char *) tokens[op]);

        rr_ret = range_refine_compare (rop, inv, lr.lo, lr.hi, truth);
        if (rr_ret == 2)
        {
            return (2);
        }
        if (rr_ret == 1)
        {
            ret = 1;
        }
    }

    if ((lr.known == 1 || rr.known == 1) && is_ident_start (lop[0]) == 1 &&
        token_is_keyword (lop) == 0)
    {
        if (rr.known == 1)
        {
            rr_ret = range_refine_compare (lop, tokens[op], rr.lo, rr.hi, truth);
        }
        else
        {
            rr_ret = range_refine_compare (lop, tokens[op], lr.lo, lr.hi, truth);
        }
        if (rr_ret == 2)
        {
            return (2);
        }
        if (rr_ret == 1)
        {
            ret = 1;
        }
    }

    return (ret);
}

// check for an allocation call and remember its size for the next stpop
void note_allocation_call (U1 *name, S2 nt, U1 tokens[][MAXSTRLEN + 1])
{
    S8 elem ALIGN = 1;
    U1 is_alloc = 0;
    U1 is_vect = 0;
    S2 arg_pos = -1;
    S2 i;
    struct int_range r;

    pending_alloc = 0;
    pending_alloc_known = 0;
    pending_alloc_size = -1;
    pending_alloc_elem_size = 1;

    if (strcmp ((const char *) name, "alloc_byte") == 0) { is_alloc = 1; elem = 1; }
    else if (strcmp ((const char *) name, "alloc_int16") == 0) { is_alloc = 1; elem = 2; }
    else if (strcmp ((const char *) name, "alloc_int32") == 0) { is_alloc = 1; elem = 4; }
    else if (strcmp ((const char *) name, "alloc_int64") == 0) { is_alloc = 1; elem = 8; }
    else if (strcmp ((const char *) name, "alloc_double") == 0) { is_alloc = 1; elem = 8; }
    else if (strcmp ((const char *) name, "alloc_byte_vect") == 0) { is_alloc = 1; is_vect = 1; elem = 1; }
    else if (strcmp ((const char *) name, "alloc_int16_vect") == 0) { is_alloc = 1; is_vect = 1; elem = 2; }
    else if (strcmp ((const char *) name, "alloc_int32_vect") == 0) { is_alloc = 1; is_vect = 1; elem = 4; }
    else if (strcmp ((const char *) name, "alloc_int64_vect") == 0) { is_alloc = 1; is_vect = 1; elem = 8; }
    else if (strcmp ((const char *) name, "alloc_double_vect") == 0) { is_alloc = 1; is_vect = 1; elem = 8; }

    if (is_alloc == 0)
    {
        return;
    }

    // first argument of the call: token after the opening '(' group name
    for (i = 0; i < nt; i++)
    {
        if (strcmp ((const char *) tokens[i], ":") == 0 ||
            strcmp ((const char *) tokens[i], "!") == 0)
        {
            break;
        }
        if (is_ident_start (tokens[i][0]) == 1 &&
            strcmp ((const char *) tokens[i], (const char *) name) != 0 &&
            token_is_keyword (tokens[i]) == 0)
        {
            arg_pos = i;
        }
    }
    if (arg_pos < 0)
    {
        return;
    }

    r = range_of_var (tokens[arg_pos]);
    if (r.known == 1)
    {
        pending_alloc_known = 1;
        if (r.lo < 1)
        {
            printf ("linter error: allocation size '%s' can be zero or negative (%lli..%lli)!\n",
                    tokens[arg_pos], r.lo, r.hi);
            linter_warnings++;
        }
        pending_alloc_elem_size = elem;
        if ((__int128) r.hi * (__int128) elem > (__int128) LLONG_MAX)
        {
            printf ("linter error: allocation size overflow: %lli elements * %lli bytes!\n",
                    r.hi, elem);
            linter_warnings++;
            pending_alloc_known = 0;
        }
        else
        {
            pending_alloc_size = (is_vect == 1 ? r.hi * elem : r.hi * elem);
        }
    }
    pending_alloc = 1;
    pending_alloc_elem_size = elem;
}

// check that a string destination has room for "len" characters plus NUL
U1 check_string_capacity (U1 *name, S8 len, U1 *line)
{
    S8 vi ALIGN;

    vi = find_var_index (name);
    if (vi < 0)
    {
        return (0);
    }
    if (vars[vi].is_string == 0)
    {
        return (0);
    }
    if (vars[vi].capacity >= 0 && len >= 0 && len + 1 > vars[vi].capacity)
    {
        linter_warnings++;
        printf ("linter error: string buffer overflow: '%s' needs %lli bytes (including NUL) but has capacity %lli!\n",
                name, len + 1, vars[vi].capacity);
        printf (" > %s\n", line);
        return (1);
    }
    return (0);
}

// update the known length of a string after a library call
void update_string_length (U1 *name, S8 len)
{
    S8 vi ALIGN;

    vi = find_var_index (name);
    if (vi < 0 || vars[vi].is_string == 0 || len < 0)
    {
        return;
    }
    vars[vi].str_len_known = 1;
    vars[vi].str_len = len;
}

S8 get_string_length (U1 *name)
{
    S8 vi ALIGN;

    vi = find_var_index (name);
    if (vi < 0 || vars[vi].is_string == 0 || vars[vi].str_len_known == 0)
    {
        return (-1);
    }
    return (vars[vi].str_len);
}

// ---------------------------------------------------------------------------
// control flow state machine used by the abstract interpreter
// ---------------------------------------------------------------------------

S2 push_flow_frame (S2 kind)
{
    if (flow_frame_ind >= FLOW_STACK_MAX - 1)
    {
        printf ("linter error: control flow nesting too deep!\n");
        return (1);
    }
    flow_frame_ind++;
    memset (&flow_frames[flow_frame_ind], 0, sizeof (struct flow_frame));
    flow_frames[flow_frame_ind].kind = kind;
    flow_frames[flow_frame_ind].cond_truth = -1;
    if (capture_flow_state (&flow_frames[flow_frame_ind].pre) != 0)
    {
        return (1);
    }
    return (0);
}

void pop_flow_frame (void)
{
    if (flow_frame_ind >= 0)
    {
        free_flow_state (&flow_frames[flow_frame_ind].pre);
        free_flow_state (&flow_frames[flow_frame_ind].then_state);
        free_flow_state (&flow_frames[flow_frame_ind].else_pre);
        flow_frame_ind--;
    }
}

S2 save_label_state (U1 *name)
{
    S2 i;
    struct flow_state *dst = NULL;

    if (init_label_states () != 0)
    {
        return (1);
    }

    for (i = 0; i <= label_states_ind; i++)
    {
        if (label_states[i].used == 1 &&
            strcmp ((const char *) label_states[i].name, (const char *) name) == 0)
        {
            dst = &label_states[i].state;
            break;
        }
    }
    if (dst == NULL)
    {
        if (label_states_ind >= LABEL_STATE_MAX - 1)
        {
            printf ("linter error: too many jump labels!\n");
            return (1);
        }
        label_states_ind++;
        label_states[label_states_ind].used = 1;
        strcpy ((char *) label_states[label_states_ind].name, (const char *) name);
        dst = &label_states[label_states_ind].state;
        dst->n = 0;
        dst->v = NULL;
        // "or" merge needs a complete state: capture the current one
        if (capture_flow_state (dst) != 0)
        {
            return (1);
        }
    }
    else
    {
        struct flow_state cur;

        if (capture_flow_state (&cur) != 0)
        {
            return (1);
        }
        merge_into_captured_state (dst, &cur);
        free_flow_state (&cur);
    }
    return (0);
}

S2 restore_label_state (U1 *name)
{
    S2 i;
    struct flow_state *src = NULL;

    if (label_states == NULL)
    {
        return (1);
    }
    for (i = 0; i <= label_states_ind; i++)
    {
        if (label_states[i].used == 1 &&
            strcmp ((const char *) label_states[i].name, (const char *) name) == 0)
        {
            src = &label_states[i].state;
            break;
        }
    }
    if (src == NULL)
    {
        return (1);
    }
    if (flow_reachable == 1)
    {
        struct flow_state cur;

        if (capture_flow_state (&cur) != 0)
        {
            return (1);
        }
        merge_into_captured_state (src, &cur);
        free_flow_state (&cur);
    }
    restore_flow_state (src);
    free_flow_state (src);
    label_states[i].used = 0;
    return (0);
}

void reset_flow_analysis (void)
{
    S2 i;

    for (i = 0; i <= flow_frame_ind; i++)
    {
        free_flow_state (&flow_frames[i].pre);
        free_flow_state (&flow_frames[i].then_state);
        free_flow_state (&flow_frames[i].else_pre);
    }
    flow_frame_ind = -1;
    if (label_states != NULL)
    {
        for (i = 0; i <= label_states_ind; i++)
        {
            if (label_states[i].used == 1)
            {
                free_flow_state (&label_states[i].state);
                label_states[i].used = 0;
            }
        }
    }
    label_states_ind = -1;
    flow_reachable = 1;
    flow_case_dead = 0;
    flow_dead_branch = 0;
    flow_stack_depth = 0;
    flow_stack_known = 1;
    pending_alloc = 0;
    pending_alloc_known = 0;
    pending_alloc_size = -1;
    pending_result = 0;
    pending_result_min = 0;
    pending_result_max = -1;
    flow_loop_widen = 0;
}

U1 line_has_token (S2 nt, U1 tokens[][MAXSTRLEN + 1], const char *what)
{
    S2 i;

    for (i = 0; i < nt; i++)
    {
        if (strcmp ((const char *) tokens[i], what) == 0)
        {
            return (1);
        }
    }
    return (0);
}

U1 line_has_comparison (S2 nt, U1 tokens[][MAXSTRLEN + 1])
{
    S2 i;

    for (i = 0; i < nt; i++)
    {
        if (strcmp ((const char *) tokens[i], "==") == 0 ||
            strcmp ((const char *) tokens[i], "!=") == 0 ||
            strcmp ((const char *) tokens[i], "<") == 0 ||
            strcmp ((const char *) tokens[i], "<=") == 0 ||
            strcmp ((const char *) tokens[i], ">") == 0 ||
            strcmp ((const char *) tokens[i], ">=") == 0)
        {
            return (1);
        }
    }
    return (0);
}

// truth value of a simple constant boolean variable used as a condition,
// e.g. "(f if)". The line must not assign to or call anything, otherwise the
// variable may just have been changed by the condition expression itself.
S2 condition_variable_truth (S2 nt, U1 tokens[][MAXSTRLEN + 1], const char *keyword)
{
    S2 i;
    S2 vi;
    S2 ident_count = 0;

    for (i = 1; i < nt; i++)
    {
        if (strcmp ((const char *) tokens[i], keyword) == 0)
        {
            S2 k;

            for (k = 0; k < i; k++)
            {
                if (strcmp ((const char *) tokens[k], "=") == 0 ||
                    strcmp ((const char *) tokens[k], ":=") == 0 ||
                    strcmp ((const char *) tokens[k], "!") == 0 ||
                    strcmp ((const char *) tokens[k], "call") == 0 ||
                    strcmp ((const char *) tokens[k], "stpop") == 0 ||
                    strcmp ((const char *) tokens[k], "stpopi") == 0 ||
                    strcmp ((const char *) tokens[k], "stpopd") == 0 ||
                    strcmp ((const char *) tokens[k], "stpopb") == 0)
                {
                    return (-1);
                }
                if (is_ident_start (tokens[k][0]) == 1 &&
                    token_is_keyword (tokens[k]) == 0)
                {
                    ident_count++;
                }
            }
            if (ident_count != 1)
            {
                return (-1);
            }
            if (is_ident_start (tokens[i - 1][0]) == 0 ||
                token_is_keyword (tokens[i - 1]) == 1)
            {
                return (-1);
            }
            vi = find_var_index (tokens[i - 1]);
            if (vi < 0 || vars[vi].is_const == 0)
            {
                return (-1);
            }
            if (vars[vi].value_known == 1)
            {
                return (vars[vi].value != 0 ? 1 : 0);
            }
            return (-1);
        }
    }
    return (-1);
}

// Handle structured control flow and label joins. Returns 1 if the line is a
// pure control line and should not be analyzed further, 0 otherwise.
U1 handle_flow_control (S2 nt, U1 tokens[][MAXSTRLEN + 1], U1 *line)
{
    S2 i;

    U1 has_func = 0;
    U1 has_funcend = 0;
    U1 has_else = 0;
    U1 has_endif = 0;
    U1 has_break = 0;
    U1 has_next = 0;
    U1 has_do = 0;
    U1 has_switch = 0;
    U1 has_switchend = 0;
    U1 has_forloop = 0;
    U1 has_for = 0;
    U1 has_while = 0;
    U1 has_return = 0;
    U1 has_case = 0;
    S2 jmp_label = -1;
    U1 is_jmp = 0;
    U1 is_jmpi = 0;
    S2 label_pos = -1;
    S2 cond_truth;

    for (i = 0; i < nt; i++)
    {
        if (strcmp ((const char *) tokens[i], "func") == 0) has_func = 1;
        if (strcmp ((const char *) tokens[i], "funcend") == 0) has_funcend = 1;
        if (strcmp ((const char *) tokens[i], "else") == 0) has_else = 1;
        if (strcmp ((const char *) tokens[i], "endif") == 0) has_endif = 1;
        if (strcmp ((const char *) tokens[i], "break") == 0) has_break = 1;
        if (strcmp ((const char *) tokens[i], "next") == 0) has_next = 1;
        if (strcmp ((const char *) tokens[i], "do") == 0) has_do = 1;
        if (strcmp ((const char *) tokens[i], "switch") == 0) has_switch = 1;
        if (strcmp ((const char *) tokens[i], "switchend") == 0) has_switchend = 1;
        if (strcmp ((const char *) tokens[i], "for") == 0) has_for = 1;
        if (strcmp ((const char *) tokens[i], "for-loop") == 0) has_forloop = 1;
        if (strcmp ((const char *) tokens[i], "while") == 0) has_while = 1;
        if (strcmp ((const char *) tokens[i], "return") == 0) has_return = 1;
        if (strcmp ((const char *) tokens[i], "jmp") == 0) is_jmp = 1;
        if (strcmp ((const char *) tokens[i], "jmpi") == 0) is_jmpi = 1;
        if (strcmp ((const char *) tokens[i], "?") == 0) has_case = 1;
    }

    // function boundaries reset the intra-procedural analysis
    if (has_func == 1 && has_funcend == 0)
    {
        U1 fname[MAXSTRLEN + 1];
        S2 fn_pos = -1;

        fname[0] = '\0';
        for (i = 1; i < nt; i++)
        {
            if (strcmp ((const char *) tokens[i], "func") == 0)
            {
                fn_pos = i - 1;
                break;
            }
        }
        if (fn_pos >= 0 && is_ident_start (tokens[fn_pos][0]) == 1)
        {
            strcpy ((char *) fname, (const char *) tokens[fn_pos]);
        }

        reset_flow_analysis ();
        ip_vstack_ind = -1;
        ip_vstack_floor = -1;
        if (strcmp ((const char *) fname, "main") == 0)
        {
            // main has no arguments on the stack
            flow_stack_depth = 0;
            flow_stack_known = 1;
        }
        else
        {
            S2 fi = get_function_index (fname);

            if (fi >= 0 && function_args[fi].args >= 0)
            {
                // the caller has pushed the function arguments
                flow_stack_depth = function_args[fi].args;
                flow_stack_known = 1;
            }
            else
            {
                // unannotated function: argument/return stack effect unknown
                flow_stack_known = 0;
            }
        }
        return (0);
    }
    if (has_funcend == 1)
    {
        // CBMC-style return path check: if the function can still fall through
        // to its end and it declares return arguments, a return value is
        // missing on at least one path.
        flow_missing_return = 0;
        if (flow_reachable == 1 &&
            strcmp ((const char *) function_name_current, "main") != 0 &&
            get_return_index (function_name_current) != -1)
        {
            flow_missing_return = 1;
        }
        // report leaks of allocations made in main
        reset_flow_analysis ();
        ip_vstack_ind = -1;
        ip_vstack_floor = -1;
        return (0);
    }

    if (flow_reachable == 0)
    {
        if (has_case == 1 && flow_frame_ind >= 0 &&
            flow_frames[flow_frame_ind].kind == FRAME_SWITCH)
        {
            // a case label is a new entry point: the previous case ended
            // with break, but the switch value may select this case.
            restore_flow_state (&flow_frames[flow_frame_ind].pre);
            flow_reachable = 1;
            flow_case_dead = 1;
        }
        else
        {
        // only labels and block delimiters can make code reachable again
        if (is_jmp == 1 || is_jmpi == 1)
        {
            return (1);
        }
        if (has_else == 1 && flow_frame_ind >= 0 && flow_frames[flow_frame_ind].kind == FRAME_IF)
        {
            struct flow_frame *f = &flow_frames[flow_frame_ind];

            free_flow_state (&f->then_state);
            f->has_then = 0;
            f->then_reachable = 0;
            if (f->has_else_pre == 1)
            {
                restore_flow_state (&f->else_pre);
                flow_reachable = 1;
            }
            else
            {
                restore_flow_state (&f->pre);
                flow_reachable = 0;
            }
            f->has_else = 1;
            return (1);
        }
        if (has_endif == 1 && flow_frame_ind >= 0 && flow_frames[flow_frame_ind].kind == FRAME_IF)
        {
            struct flow_frame *f = &flow_frames[flow_frame_ind];

            if (f->has_else == 1)
            {
                if (f->then_reachable == 1)
                {
                    restore_flow_state (&f->then_state);
                    flow_reachable = 1;
                }
                else
                {
                    // both branches ended unreachable
                    flow_reachable = 0;
                }
            }
            else
            {
                if (f->has_else_pre == 1)
                {
                    restore_flow_state (&f->else_pre);
                    flow_reachable = 1;
                }
                else
                {
                    flow_reachable = 0;
                }
            }
            pop_flow_frame ();
            return (1);
        }
        if (has_next == 1 && flow_frame_ind >= 0 && flow_frames[flow_frame_ind].kind == FRAME_FOR)
        {
            struct flow_frame *f = &flow_frames[flow_frame_ind];

            restore_flow_state (&f->pre);
            flow_reachable = 1;
            pop_flow_frame ();
            return (1);
        }
        if (has_switchend == 1 && flow_frame_ind >= 0 && flow_frames[flow_frame_ind].kind == FRAME_SWITCH)
        {
            struct flow_frame *f = &flow_frames[flow_frame_ind];

            restore_flow_state (&f->pre);
            flow_reachable = 1;
            pop_flow_frame ();
            return (1);
        }
        if (has_while == 1 && flow_frame_ind >= 0 && flow_frames[flow_frame_ind].kind == FRAME_DO)
        {
            struct flow_frame *f = &flow_frames[flow_frame_ind];

            restore_flow_state (&f->pre);
            flow_reachable = 1;
            pop_flow_frame ();
            return (1);
        }

        // label definition is a join point
        for (i = 1; i + 1 < nt; i++)
        {
            if (strcmp ((const char *) tokens[i - 1], ":") == 0 &&
                strcmp ((const char *) tokens[i + 1], ")") == 0 &&
                is_ident_start (tokens[i][0]) == 1 &&
                token_is_keyword (tokens[i]) == 0)
            {
                // a label can be an independent entry point (thread start,
                // function pointer target), so it makes the code reachable
                (void) restore_label_state (tokens[i]);
                flow_reachable = 1;
                return (1);
            }
        }
        // ordinary statement inside dead code
        flow_line_unreachable = 1;
        return (1);
        }
    }

    // if / if+ : start of a conditional. Capture the state before refinement.
    if ((line_has_token (nt, tokens, "if") == 1 ||
         line_has_token (nt, tokens, "if+") == 1) &&
        has_for == 0 && has_while == 0 && has_case == 0)
    {
        if (push_flow_frame (FRAME_IF) != 0)
        {
            return (1);
        }
        {
            struct flow_frame *f = &flow_frames[flow_frame_ind];
            struct flow_state then_cur;
            U1 have_then_cur = 0;
            S2 rc = refine_condition (nt, tokens, 1);
            S2 cond = flow_cond_truth;
            U1 cond_stable = flow_cond_stable;
            U1 trust;

            if (cond == -1 && line_has_comparison (nt, tokens) == 0)
            {
                cond = condition_variable_truth (nt, tokens, "if");
                if (cond == -1)
                {
                    cond = condition_variable_truth (nt, tokens, "if+");
                }
                if (cond != -1)
                {
                    cond_stable = 1;
                }
            }
            // Inferred ranges are not a loop invariant and unstructured jumps
            // can invalidate them. Only trust them in strict logic mode or if
            // the comparison only uses compile time constants.
            trust = (U1) (cond_stable == 1 || flow_logic_strict == 1);

            if (trust == 1 && cond == 1)
            {
                f->cond_truth = 1;
                if (flow_inside_loop () == 0)
                {
                    warn_condition_truth (1, line);
                }
            }
            else if (trust == 1 && (cond == 0 || rc == 2))
            {
                f->cond_truth = 0;
                flow_reachable = 0;
                flow_dead_branch = 1;
                if (flow_inside_loop () == 0)
                {
                    warn_condition_truth (0, line);
                }
            }
            else
            {
                f->cond_truth = -1;
            }

            if (rc == 2 && trust == 0)
            {
                // do not discard the then branch based on an unstable range
                restore_flow_state (&f->pre);
                flow_reachable = 1;
            }

            if (flow_reachable == 1)
            {
                if (capture_flow_state (&then_cur) == 0)
                {
                    have_then_cur = 1;
                }
            }

            // compute the state of the false branch from the pre-if state
            restore_flow_state (&f->pre);
            flow_reachable = 1;
            rc = refine_condition (nt, tokens, 0);
            if (rc == 2 && trust == 0)
            {
                // the false branch may still be reachable
                restore_flow_state (&f->pre);
            }
            if (rc != 2 || trust == 0)
            {
                if (capture_flow_state (&f->else_pre) == 0)
                {
                    f->has_else_pre = 1;
                }
            }

            if (have_then_cur == 1)
            {
                restore_flow_state (&then_cur);
                free_flow_state (&then_cur);
                flow_reachable = 1;
            }
            else
            {
                restore_flow_state (&f->pre);
                flow_reachable = 0;
            }
            f->has_else = (U1) (line_has_token (nt, tokens, "if+") == 1);
        }
        // the condition itself is normal code and must still be checked
        return (0);
    }

    // for-loop marker
    if (has_forloop == 1)
    {
        if (push_flow_frame (FRAME_FOR) != 0)
        {
            return (1);
        }
        return (1);
    }

    // for condition line
    if (has_for == 1 && has_forloop == 0)
    {
        U1 trust;

        flow_loop_widen = 1;
        cond_truth = refine_condition (nt, tokens, 1);
        flow_loop_widen = 0;
        trust = (U1) (flow_cond_stable == 1 || flow_logic_strict == 1);

        if (cond_truth == 2 && trust == 1)
        {
            // only report a top level loop here: inside an outer loop the
            // current ranges describe just one iteration.
            if (flow_frame_ind >= 0 && flow_frames[flow_frame_ind].kind == FRAME_FOR)
            {
                flow_frames[flow_frame_ind].cond_truth = 0;
            }
            if (flow_frame_ind == 0 && flow_frames[0].kind == FRAME_FOR)
            {
                warn_dead_loop (line);
            }
            flow_reachable = 0;
            flow_dead_branch = 1;
        }
        return (0);
    }

    // (do) marker
    if (has_do == 1)
    {
        if (push_flow_frame (FRAME_DO) != 0)
        {
            return (1);
        }
        return (1);
    }

    // while condition line: merge body state with the pre-loop state
    if (has_while == 1)
    {
        struct flow_frame *f = NULL;

        if (flow_frame_ind >= 0 && flow_frames[flow_frame_ind].kind == FRAME_DO)
        {
            f = &flow_frames[flow_frame_ind];
        }
        // exit condition for the code after the loop
        cond_truth = refine_condition (nt, tokens, 0);
        if (f != NULL)
        {
            struct flow_state body;

            if (capture_flow_state (&body) != 0)
            {
                return (1);
            }
            // code after the loop sees the union of pre and body state
            merge_flow_states_to_current (&f->pre, &body);
            free_flow_state (&body);
            pop_flow_frame ();
            flow_reachable = 1;
        }
        if (cond_truth == 2)
        {
            // the exit condition contradicts the current state only in weird
            // cases; keep the code reachable
        }
        return (0);
    }

    // switch marker
    if (has_switch == 1)
    {
        if (push_flow_frame (FRAME_SWITCH) != 0)
        {
            return (1);
        }
        return (1);
    }

    // switch case: (value constant ?)
    if (has_case == 1 && flow_frame_ind >= 0 && flow_frames[flow_frame_ind].kind == FRAME_SWITCH)
    {
        struct flow_frame *f = &flow_frames[flow_frame_ind];

        // CBMC-style switch check: the same case value must not appear twice.
        if (nt >= 3)
        {
            S8 case_value ALIGN;
            U1 case_known = 0;

            case_value = 0;
            if (is_number_token (tokens[2]))
            {
                case_value = (S8) strtoll ((const char *) tokens[2], NULL, 10);
                case_known = 1;
            }
            else
            {
                S2 cvi = find_var_index (tokens[2]);
                if (cvi >= 0 && vars[cvi].is_const == 1 && vars[cvi].value_known == 1)
                {
                    case_value = vars[cvi].value;
                    case_known = 1;
                }
            }
            if (case_known == 1)
            {
                S2 k;
                S8 subject_hash ALIGN = flow_token_hash (tokens[1]);

                for (k = 0; k < f->case_count; k++)
                {
                    if (f->case_values[k] == case_value &&
                        f->case_subject_hash[k] == subject_hash)
                    {
                        warn_duplicate_case (case_value, line);
                        break;
                    }
                }
                if (f->case_count < FLOW_MAX_CASES)
                {
                    f->case_values[f->case_count] = case_value;
                    f->case_subject_hash[f->case_count] = subject_hash;
                    f->case_count++;
                }
            }
        }

        // the previous case may fall through into this case. A case which is
        // entered directly after a break/return must not contribute the dead
        // state of the previous case.
        if (flow_reachable == 1 && flow_case_dead == 0)
        {
            struct flow_state cur;

            if (f->has_then == 0)
            {
                if (capture_flow_state (&f->then_state) != 0)
                {
                    return (1);
                }
                f->has_then = 1;
            }
            else
            {
                if (capture_flow_state (&cur) != 0)
                {
                    return (1);
                }
                merge_into_captured_state (&f->then_state, &cur);
                free_flow_state (&cur);
            }
        }
        flow_case_dead = 0;
        restore_flow_state (&f->pre);
        flow_reachable = 1;
        // refine the switch value to the case constant
        if (nt >= 4)
        {
            S2 vi = find_var_index (tokens[1]);
            struct int_range r = range_of_var (tokens[2]);
            if (vi >= 0 && r.known == 1 && r.lo == r.hi)
            {
                vars[vi].range_known = 1;
                vars[vi].range_min = r.lo;
                vars[vi].range_max = r.hi;
            }
        }
        return (1);
    }

    if (has_break == 1)
    {
        if (flow_frame_ind >= 0 && flow_frames[flow_frame_ind].kind == FRAME_SWITCH)
        {
            struct flow_frame *f = &flow_frames[flow_frame_ind];
            struct flow_state cur;

            if (flow_reachable == 1)
            {
                if (capture_flow_state (&cur) != 0)
                {
                    return (1);
                }
                if (f->has_then == 0)
                {
                    f->then_state = cur;
                    f->has_then = 1;
                }
                else
                {
                    merge_into_captured_state (&f->then_state, &cur);
                    free_flow_state (&cur);
                }
            }
            flow_reachable = 0;
        }
        return (1);
    }

    if (has_switchend == 1)
    {
        struct flow_frame *f = NULL;

        if (flow_frame_ind >= 0 && flow_frames[flow_frame_ind].kind == FRAME_SWITCH)
        {
            f = &flow_frames[flow_frame_ind];
        }
        if (f != NULL)
        {
            struct flow_state cur;

            if (capture_flow_state (&cur) != 0)
            {
                return (1);
            }
            if (f->has_then == 0)
            {
                merge_flow_states_to_current (&f->pre, &cur);
            }
            else
            {
                struct flow_state merged;

                // merge accumulated cases and the current fall-through case
                merged.n = 0;
                merged.v = NULL;
                merged.stack_depth = cur.stack_depth;
                merged.stack_known = cur.stack_known;
                merge_into_captured_state (&f->then_state, &cur);
                merged.v = f->then_state.v;
                merged.n = f->then_state.n;
                merged.stack_depth = f->then_state.stack_depth;
                merged.stack_known = f->then_state.stack_known;
                merge_flow_states_to_current (&f->pre, &merged);
            }
            free_flow_state (&cur);
            pop_flow_frame ();
        }
        flow_reachable = 1;
        return (1);
    }

    if (has_else == 1)
    {
        if (flow_frame_ind >= 0 && flow_frames[flow_frame_ind].kind == FRAME_IF)
        {
            struct flow_frame *f = &flow_frames[flow_frame_ind];

            free_flow_state (&f->then_state);
            if (flow_reachable == 1)
            {
                if (capture_flow_state (&f->then_state) != 0)
                {
                    return (1);
                }
                f->has_then = 1;
                f->then_reachable = 1;
            }
            else
            {
                f->has_then = 0;
                f->then_reachable = 0;
            }
            if (f->has_else_pre == 1)
            {
                restore_flow_state (&f->else_pre);
                flow_reachable = 1;
            }
            else
            {
                restore_flow_state (&f->pre);
                flow_reachable = 0;
            }
            f->has_else = 1;
        }
        return (1);
    }

    if (has_endif == 1)
    {
        if (flow_frame_ind >= 0 && flow_frames[flow_frame_ind].kind == FRAME_IF)
        {
            struct flow_frame *f = &flow_frames[flow_frame_ind];
            struct flow_state then_state;
            U1 have_then = 0;

            if (f->has_else == 1 && f->has_then == 1)
            {
                then_state = f->then_state;
                f->then_state.v = NULL;
                f->then_state.n = 0;
                have_then = 1;
            }
            else if (f->has_else == 0 && f->cond_truth != 0)
            {
                // then-branch state is the current one
                if (flow_reachable == 1)
                {
                    if (capture_flow_state (&then_state) != 0)
                    {
                        return (1);
                    }
                    have_then = 1;
                }
            }

            if (f->has_else == 1)
            {
                struct flow_state else_state;
                U1 have_else = 0;

                if (flow_reachable == 1)
                {
                    if (capture_flow_state (&else_state) != 0)
                    {
                        return (1);
                    }
                    have_else = 1;
                }
                if (have_then == 1 && have_else == 1)
                {
                    merge_flow_states_to_current (&then_state, &else_state);
                    flow_reachable = 1;
                }
                else if (have_then == 1)
                {
                    restore_flow_state (&then_state);
                    flow_reachable = 1;
                }
                else if (have_else == 1)
                {
                    restore_flow_state (&else_state);
                    flow_reachable = 1;
                }
                else
                {
                    flow_reachable = 0;
                }
                if (have_then == 1) free_flow_state (&then_state);
                if (have_else == 1) free_flow_state (&else_state);
            }
            else
            {
                // if without else: merge the then branch with the
                // (condition false) fall-through branch
                if (have_then == 1 && f->has_else_pre == 1)
                {
                    merge_flow_states_to_current (&f->else_pre, &then_state);
                    flow_reachable = 1;
                    free_flow_state (&then_state);
                }
                else if (have_then == 1)
                {
                    restore_flow_state (&then_state);
                    flow_reachable = 1;
                    free_flow_state (&then_state);
                }
                else if (f->has_else_pre == 1)
                {
                    restore_flow_state (&f->else_pre);
                    flow_reachable = 1;
                }
                else
                {
                    restore_flow_state (&f->pre);
                    flow_reachable = 0;
                }
            }
            pop_flow_frame ();
        }
        return (1);
    }

    if (has_next == 1)
    {
        if (flow_frame_ind >= 0 && flow_frames[flow_frame_ind].kind == FRAME_FOR)
        {
            struct flow_frame *f = &flow_frames[flow_frame_ind];
            struct flow_state body;

            if (capture_flow_state (&body) != 0)
            {
                return (1);
            }
            if (flow_reachable == 1)
            {
                merge_flow_states_to_current (&f->pre, &body);
            }
            else
            {
                restore_flow_state (&f->pre);
            }
            free_flow_state (&body);
            pop_flow_frame ();
            flow_reachable = 1;
        }
        return (1);
    }

    // labels and jumps
    for (i = 1; i + 1 < nt; i++)
    {
        if (strcmp ((const char *) tokens[i - 1], ":") == 0 &&
            strcmp ((const char *) tokens[i + 1], ")") == 0 &&
            is_ident_start (tokens[i][0]) == 1 &&
            token_is_keyword (tokens[i]) == 0)
        {
            label_pos = i;
            break;
        }
    }
    if (label_pos >= 0)
    {
        if (restore_label_state (tokens[label_pos]) == 0)
        {
            flow_reachable = 1;
        }
        return (1);
    }

    if (is_jmp == 1 || is_jmpi == 1)
    {
        // find label after ':'
        for (i = 1; i < nt; i++)
        {
            if (strcmp ((const char *) tokens[i - 1], ":") == 0)
            {
                jmp_label = i;
                break;
            }
        }
        if (jmp_label >= 0)
        {
            (void) save_label_state (tokens[jmp_label]);
        }
        if (is_jmp == 1 && is_jmpi == 0)
        {
            flow_reachable = 0;
        }
        return (0);
    }

    if (has_return == 1)
    {
        flow_reachable = 0;
        return (0);
    }

    return (0);
}

void type_range (U1 type, S8 *lo, S8 *hi)
{
    switch (type)
    {
        case BYTE:
            *lo = 0;
            *hi = 255;
            break;
        case WORD:
            *lo = -32768;
            *hi = 32767;
            break;
        case DOUBLEWORD:
            *lo = -2147483648LL;
            *hi = 2147483647LL;
            break;
        default:
            *lo = LLONG_MIN;
            *hi = LLONG_MAX;
            break;
    }
}

void set_var_range (U1 *name, S8 lo, S8 hi)
{
    S8 vi ALIGN;

    vi = find_var_index (name);
    if (vi < 0 || vars[vi].is_array == 1 || vars[vi].is_string == 1)
    {
        return;
    }
    vars[vi].range_known = 1;
    vars[vi].range_min = lo;
    vars[vi].range_max = hi;
    vars[vi].may_zero = (U1) (lo <= 0 && hi >= 0);
}

// report heap blocks which are still allocated. Only allocations made in main
// are reported: library functions may intentionally return allocated memory.
void check_heap_leaks (U1 *line)
{
    S8 i ALIGN;
    U1 reported = 0;

    if (strcmp ((const char *) function_name_current, "main") != 0)
    {
        return;
    }
    for (i = 0; i <= vars_ind; i++)
    {
        if (vars[i].heap_state == HEAP_ALLOCATED && vars[i].heap_origin == ORIGIN_HEAP)
        {
            if (reported == 0)
            {
                printf ("linter error: memory leak: allocated heap memory is not freed before exit!\n");
                printf (" > %s\n", line);
                reported = 1;
            }
            printf ("linter error: leaked allocation in variable '%s' (%lli bytes)!\n",
                    vars[i].name, vars[i].heap_size_known == 1 ? vars[i].heap_size : -1);
            linter_warnings++;
        }
    }
}

// collect simple call arguments (identifiers / numbers / string literals)
S2 collect_call_args (S2 nt, U1 tokens[][MAXSTRLEN + 1], S2 colon, U1 *args[], S2 max_args)
{
    S2 i;
    S2 count = 0;
    S2 first = 0;

    (void) nt;
    for (i = 0; i < colon; i++)
    {
        if (strcmp ((const char *) tokens[i], "(") == 0)
        {
            first = i + 1;
        }
    }
    for (i = first; i < colon && count < max_args; i++)
    {
        if (tokens[i][0] == '"')
        {
            args[count++] = tokens[i];
        }
        else if (is_number_token (tokens[i]) == 1)
        {
            args[count++] = tokens[i];
        }
        else if (is_ident_start (tokens[i][0]) == 1 &&
                 token_is_keyword (tokens[i]) == 0)
        {
            args[count++] = tokens[i];
        }
    }
    return (count);
}

S8 token_string_length (U1 *tok)
{
    S8 len ALIGN;

    if (tok[0] != '"')
    {
        return (-1);
    }
    len = (S8) strlen_safe ((const char *) tok, MAXSTRLEN);
    if (len < 2)
    {
        return (0);
    }
    return (len - 2);
}

S8 call_arg_length (U1 *arg)
{
    S8 len ALIGN;

    len = token_string_length (arg);
    if (len >= 0)
    {
        return (len);
    }
    return (get_string_length (arg));
}

S8 call_arg_count (U1 *arg)
{
    S8 vi;

    if (is_number_token (arg) == 1)
    {
        return ((S8) strtoll ((const char *) arg, NULL, 10));
    }
    vi = find_var_index (arg);
    if (vi >= 0)
    {
        if (vars[vi].range_known == 1)
        {
            // the upper bound is the safe value for capacity checks
            return (vars[vi].range_max);
        }
        if (vars[vi].value_known == 1)
        {
            return (vars[vi].value);
        }
    }
    return (-1);
}

// check library calls which write into string buffers
void check_string_call (U1 *callee, S2 nargs, U1 *args[], U1 *line)
{
    S8 src_len ALIGN;
    S8 dst_len ALIGN;
    S8 cap ALIGN;
    S8 vi ALIGN;
    S8 count ALIGN;

    if (strcmp ((const char *) callee, "string_copy") == 0 && nargs >= 2)
    {
        // first argument is the destination, second the source
        src_len = call_arg_length (args[1]);
        vi = find_var_index (args[0]);
        cap = (vi >= 0 ? vars[vi].capacity : -1);
        if (src_len >= 0 && cap >= 0 && src_len + 1 > cap)
        {
            linter_warnings++;
            printf ("linter error: string_copy overflows destination '%s': %lli bytes needed, capacity %lli!\n",
                    args[0], src_len + 1, cap);
            printf (" > %s\n", line);
        }
        update_string_length (args[0], src_len);
        return;
    }

    if (strcmp ((const char *) callee, "string_cat") == 0 && nargs >= 2)
    {
        // first argument is the destination, second the source
        src_len = call_arg_length (args[1]);
        dst_len = call_arg_length (args[0]);
        vi = find_var_index (args[0]);
        cap = (vi >= 0 ? vars[vi].capacity : -1);
        if (src_len >= 0 && dst_len >= 0 && cap >= 0 &&
            dst_len + src_len + 1 > cap)
        {
            linter_warnings++;
            printf ("linter error: string_cat overflows destination '%s': %lli bytes needed, capacity %lli!\n",
                    args[0], dst_len + src_len + 1, cap);
            printf (" > %s\n", line);
        }
        else if (src_len >= 0 && cap >= 0 && dst_len < 0 && src_len + 1 > cap)
        {
            linter_warnings++;
            printf ("linter error: string_cat may overflow destination '%s' (capacity %lli)!\n",
                    args[0], cap);
            printf (" > %s\n", line);
        }
        if (src_len >= 0 && dst_len >= 0)
        {
            update_string_length (args[0], dst_len + src_len);
        }
        else
        {
            update_string_length (args[0], -1);
        }
        return;
    }

    if ((strcmp ((const char *) callee, "string_left") == 0 ||
         strcmp ((const char *) callee, "string_right") == 0) && nargs >= 3)
    {
        src_len = call_arg_length (args[0]);
        count = call_arg_count (args[2]);
        vi = find_var_index (args[1]);
        cap = (vi >= 0 ? vars[vi].capacity : -1);
        if (count >= 0 && cap >= 0 && count + 1 > cap)
        {
            linter_warnings++;
            printf ("linter error: %s overflows destination '%s': %lli bytes needed, capacity %lli!\n",
                    callee, args[1], count + 1, cap);
            printf (" > %s\n", line);
        }
        if (count >= 0)
        {
            if (src_len >= 0 && src_len < count)
            {
                count = src_len;
            }
            update_string_length (args[1], count);
        }
        else
        {
            update_string_length (args[1], -1);
        }
        return;
    }

    if ((strcmp ((const char *) callee, "string_int64tostring") == 0 ||
         strcmp ((const char *) callee, "string_doubletostring") == 0 ||
         strcmp ((const char *) callee, "string_bytetohexstring") == 0 ||
         strcmp ((const char *) callee, "string_bytenumtostring") == 0) && nargs >= 3)
    {
        count = call_arg_count (args[2]);
        vi = find_var_index (args[1]);
        cap = (vi >= 0 ? vars[vi].capacity : -1);
        if (count >= 0 && cap >= 0 && count > cap)
        {
            linter_warnings++;
            printf ("linter error: %s needs %lli bytes but destination '%s' has capacity %lli!\n",
                    callee, count, args[1], cap);
            printf (" > %s\n", line);
        }
        if (count >= 0)
        {
            // upper bound for the formatted result length (conservative)
            if (strcmp ((const char *) callee, "string_int64tostring") == 0)
            {
                update_string_length (args[1], 20);
            }
            else if (strcmp ((const char *) callee, "string_doubletostring") == 0)
            {
                update_string_length (args[1], 32);
            }
            else if (strcmp ((const char *) callee, "string_bytetohexstring") == 0)
            {
                update_string_length (args[1], 2);
            }
            else
            {
                update_string_length (args[1], 3);
            }
        }
        else
        {
            update_string_length (args[1], -1);
        }
        return;
    }

    if ((strcmp ((const char *) callee, "file_get_string") == 0 ||
         strcmp ((const char *) callee, "socket_read_string") == 0) && nargs >= 3)
    {
        count = call_arg_count (args[2]);
        vi = find_var_index (args[1]);
        cap = (vi >= 0 ? vars[vi].capacity : -1);
        if (count >= 0 && cap >= 0 && count + 1 > cap)
        {
            linter_warnings++;
            printf ("linter error: %s can write %lli bytes into '%s' (capacity %lli)!\n",
                    callee, count + 1, args[1], cap);
            printf (" > %s\n", line);
        }
        update_string_length (args[1], -1);
        return;
    }

    if (strcmp ((const char *) callee, "string_len") == 0 && nargs >= 1)
    {
        S8 vi_len = find_var_index (args[0]);

        pending_result = 1;
        pending_result_value = get_string_length (args[0]);
        pending_result_min = 0;
        pending_result_max = (vi_len >= 0 && vars[vi_len].capacity >= 0 ?
                              vars[vi_len].capacity - 1 : -1);
        return;
    }

    if (strcmp ((const char *) callee, "string_search") == 0 && nargs >= 1)
    {
        S8 vi_len = find_var_index (args[0]);

        pending_result = 1;
        pending_result_value = -2;      // not an exact known value
        pending_result_min = -1;        // -1 = not found
        pending_result_max = (vi_len >= 0 && vars[vi_len].capacity >= 0 ?
                              vars[vi_len].capacity - 1 : -1);
        return;
    }
}

// evaluate the operand which starts/ends at token position pos to a range
U1 operand_range_at (S2 nt, U1 tokens[][MAXSTRLEN + 1], S2 pos, struct int_range *r, U1 *overflow)
{
    S2 start = pos;
    S2 end = pos;

    if (pos < 0 || pos >= nt)
    {
        return (0);
    }
    if (strcmp ((const char *) tokens[pos], ")") == 0)
    {
        start = find_open_paren (nt, tokens, pos);
        if (start < 0)
        {
            return (0);
        }
    }
    else if (strcmp ((const char *) tokens[pos], "(") == 0)
    {
        end = find_close_paren (nt, tokens, pos);
        if (end < 0)
        {
            return (0);
        }
    }
    return (range_eval (nt, tokens, start, end, r, overflow));
}

// evaluate a (mostly postfix) expression into a list of pushed values.
// This is used for stpush and for the value list of a (return) line.
S2 ip_eval_rpn (S2 nt, U1 tokens[][MAXSTRLEN + 1], S2 start, S2 end, struct int_range *out, S2 max_out)
{
    struct int_range stack[IP_VSTACK_MAX];
    S2 sp = -1;
    S2 i;
    S2 count;

    if (max_out <= 0)
    {
        return (0);
    }

    for (i = start; i <= end && i < nt; i++)
    {
        U1 *tok = tokens[i];

        if (strcmp ((const char *) tok, "(") == 0)
        {
            S2 close = find_close_paren (nt, tokens, i);
            S2 sub_count;

            if (close < 0 || close > end)
            {
                continue;
            }
            sub_count = ip_eval_rpn (nt, tokens, i + 1, close - 1, stack + sp + 1,
                                     (S2) (IP_VSTACK_MAX - 1 - sp));
            if (sub_count > 0)
            {
                sp += sub_count;
            }
            i = close;
            continue;
        }
        if (strcmp ((const char *) tok, ")") == 0)
        {
            continue;
        }
        if (is_number_token (tok) == 1)
        {
            if (sp < IP_VSTACK_MAX - 1)
            {
                sp++;
                stack[sp] = range_exact ((S8) strtoll ((const char *) tok, NULL, 10));
            }
            continue;
        }
        if (strcmp ((const char *) tok, "true") == 0)
        {
            if (sp < IP_VSTACK_MAX - 1)
            {
                sp++;
                stack[sp] = range_exact (1);
            }
            continue;
        }
        if (strcmp ((const char *) tok, "false") == 0)
        {
            if (sp < IP_VSTACK_MAX - 1)
            {
                sp++;
                stack[sp] = range_exact (0);
            }
            continue;
        }
        if (operator_precedence (tok) > 0)
        {
            if (sp >= 1)
            {
                struct int_range b = stack[sp];
                struct int_range a = stack[sp - 1];
                struct int_range r = range_unknown ();
                U1 ov = 0;

                sp -= 2;
                (void) range_apply (tok, a, b, &r, &ov);
                sp++;
                stack[sp] = r;
            }
            continue;
        }
        if (is_ident_start (tok[0]) == 1 && token_is_keyword (tok) == 0)
        {
            if (sp < IP_VSTACK_MAX - 1)
            {
                sp++;
                stack[sp] = range_of_var (tok);
            }
            continue;
        }
        if (tok[0] == '"')
        {
            if (sp < IP_VSTACK_MAX - 1)
            {
                sp++;
                stack[sp] = range_unknown ();
            }
            continue;
        }
    }

    count = sp + 1;
    if (count > max_out)
    {
        count = max_out;
    }
    for (i = 0; i < count; i++)
    {
        out[i] = stack[i];
    }
    return (count);
}

void ip_push_range (struct int_range r)
{
    if (ip_vstack_ind < IP_VSTACK_MAX - 1)
    {
        ip_vstack_ind++;
        ip_vstack[ip_vstack_ind] = r;
    }
}

struct int_range ip_pop_range (void)
{
    if (ip_vstack_ind > ip_vstack_floor)
    {
        struct int_range r = ip_vstack[ip_vstack_ind];

        ip_vstack_ind--;
        return (r);
    }
    return (range_unknown ());
}

// capture the return values of a (return) line. The return line often looks
// like "(ret~ stpush) (return)", so the pushed values are evaluated from the
// line. If the values were pushed on an earlier line they are taken from the
// value stack.
void ip_capture_return (S2 nt, U1 tokens[][MAXSTRLEN + 1], U1 *line)
{
    S2 i;
    S2 stpush_pos = -1;
    S2 go = -1;
    S2 gc = -1;

    (void) line;

    ip_last_return_seen = 1;
    ip_last_return_count = 0;

    for (i = 0; i < nt; i++)
    {
        if (strncmp ((const char *) tokens[i], "stpush", 6) == 0)
        {
            stpush_pos = i;
        }
    }

    if (stpush_pos >= 0)
    {
        if (find_enclosing_paren (nt, tokens, stpush_pos, &go, &gc) == 1)
        {
            ip_last_return_count = ip_eval_rpn (nt, tokens, go + 1, stpush_pos - 1,
                                                ip_last_return, IP_MAX_RETURNS);
        }
        else
        {
            ip_last_return_count = ip_eval_rpn (nt, tokens, 0, stpush_pos - 1,
                                                ip_last_return, IP_MAX_RETURNS);
        }
    }
    else
    {
        S2 k;
        S2 n = 0;

        for (k = ip_vstack_floor + 1; k <= ip_vstack_ind && n < IP_MAX_RETURNS; k++)
        {
            ip_last_return[n] = ip_vstack[k];
            n++;
        }
        ip_last_return_count = n;
    }
}

// main safety check, called for every parsed line
S2 safety_check_line (U1 *line)
{
    S2 nt;
    S2 i;
    S2 op;
    S8 vi ALIGN;
    S8 vj ALIGN;
    U1 *base;
    U1 *dest;
    U1 *src;
    U1 *name;
    U1 is_set = 0;
    U1 is_intr = 0;
    U1 is_pointer_instruction = 0;

    nt = tokenize_line (line, safety_tokens, MAXTOKENS);
    if (nt == 0)
    {
        return (0);
    }
    ip_call_precise = 0;
    // preprocessor lines, comments and file markers
    if (line[0] == '#')
    {
        return (0);
    }
    {
        S2 cpos = 0;

        while (line[cpos] == ' ' || line[cpos] == '\t')
        {
            cpos++;
        }
        if (line[cpos] == '/' && line[cpos + 1] == '/')
        {
            return (0);
        }
    }
    if (strcmp ((const char *) safety_tokens[0], "FILE:") == 0)
    {
        return (0);
    }

    // interprocedural analysis: remember the values of a (return) line before
    // the flow analysis marks the following code unreachable
    if (ip_sandbox_active == 1 && flow_reachable == 1 &&
        line_has_token (nt, safety_tokens, "return") == 1)
    {
        ip_capture_return (nt, safety_tokens, line);
    }

    // structured control flow / path sensitive state
    flow_line_unreachable = 0;
    if (handle_flow_control (nt, safety_tokens, line) == 1)
    {
        if (flow_line_unreachable == 1)
        {
            warn_unreachable_code (nt, safety_tokens, line);
            flow_line_unreachable = 0;
        }
        if (flow_reachable == 1)
        {
            flow_unreachable_warned = 0;
            flow_dead_branch = 0;
        }
        return (0);
    }
    if (flow_reachable == 1)
    {
        flow_unreachable_warned = 0;
        flow_dead_branch = 0;
    }
    if (flow_reachable == 0)
    {
        return (0);
    }

    // do not carry deallocation state across function boundaries
    for (i = 0; i < nt; i++)
    {
        if (strcmp ((const char *) safety_tokens[i], "func") == 0 ||
            strcmp ((const char *) safety_tokens[i], "funcend") == 0)
        {
            S8 k;
            for (k = 0; k <= vars_ind; k++)
            {
                vars[k].freed = 0;
                vars[k].warned_freed = 0;
                // do not carry compile time values across function boundaries
                if (vars[k].is_const == 0)
                {
                    vars[k].value_known = 0;
                    vars[k].dvalue_known = 0;
                    vars[k].range_known = 0;
                    vars[k].may_zero = 0;
                    vars[k].str_len_known = 0;
                }
                vars[k].heap_state = HEAP_NONE;
                vars[k].heap_size_known = 0;
                vars[k].heap_size = -1;
                vars[k].heap_origin = ORIGIN_UNKNOWN;
                vars[k].ptr_off_known = 0;
            }
            break;
        }
    }

    // check the pointer check requirement (strict pointer mode)
    if (pointer_check_active == 1)
    {
        if (line_has_pointer_check (nt, safety_tokens, pointer_check_name) == 0)
        {
            if (pointer_strict == 1)
            {
                printf ("linter error: pointer '%s' set on line %lli, but no following ':check_pointer !' found!\n",
                        pointer_check_name, pointer_check_line);
                pointer_check_active = 0;
                return (1);
            }
        }
        pointer_check_active = 0;
    }

    // variable declaration
    if (nt > 1)
    {
        if (strcmp ((const char *) safety_tokens[0], "(") == 0 &&
            strcmp ((const char *) safety_tokens[1], "set") == 0)
        {
            is_set = 1;
        }
    }

    if (is_set == 1)
    {
        if (nt > 4)
        {
            name = safety_tokens[4];
            if (find_var_index (name) != -1)
            {
                linter_warnings++;
                printf ("linter error: variable '%s' is declared more than once (last declaration wins)!\n", name);
                printf (" > %s\n", line);
            }
        }

        // check array initializer count against the declared array size
        if (nt > 5 && is_number_token (safety_tokens[3]))
        {
            S8 decl_size ALIGN = (S8) strtoll ((const char *) safety_tokens[3], NULL, 10);
            if (decl_size > 1)
            {
                S2 vcount = 0;
                S2 k = 5;
                while (k < nt)
                {
                    if (strcmp ((const char *) safety_tokens[k], ")") == 0)
                    {
                        break;
                    }
                    // multiline initializer markers
                    if (strcmp ((const char *) safety_tokens[k], "/") == 0 ||
                        strcmp ((const char *) safety_tokens[k], ",") == 0 ||
                        strcmp ((const char *) safety_tokens[k], ";") == 0 ||
                        strcmp ((const char *) safety_tokens[k], "\\") == 0)
                    {
                        k++;
                        continue;
                    }
                    vcount++;
                    k++;
                }
                if (vcount > decl_size)
                {
                    printf ("linter error: array '%s' has %i initializer values but size is %lli!\n",
                            (const char *) safety_tokens[4], vcount, decl_size);
                    printf (" > %s\n", line);
                    return (1);
                }
            }
        }

        // declarations are handled in parse_line
        return (0);
    }

    op = get_intr_op (nt, safety_tokens);
    if (op != -1)
    {
        is_intr = 1;
    }

    if (is_intr == 1)
    {
        // pointer type checks need an initialized pointer
        if ((op == 30 || op == 31 || op == 33 || op == 34) && nt > 2)
        {
            name = safety_tokens[2];
            vi = find_var_index (name);
            if (vi >= 0)
            {
                if (vars[vi].is_array == 0 && vars[vi].is_string == 0)
                {
                    if (vars[vi].pointer_set == 0)
                    {
                        printf ("linter error: pointer '%s' is used before it was set by a 'pointer' instruction!\n", name);
                        printf (" > %s\n", line);
                        return (1);
                    }
                    vars[vi].is_pointer = 1;
                }
            }
        }

        if (op == 255)
        {
            check_heap_leaks (line);
            // the exit call terminates the program: code after it is unreachable
            flow_reachable = 0;
        }
        mark_intr_writes (op, nt, safety_tokens);
        return (0);
    }

    // pointer instruction: (source Pdest pointer)
    for (i = 0; i + 1 < nt; i++)
    {
        if (strcmp ((const char *) safety_tokens[i], "pointer") == 0 &&
            strcmp ((const char *) safety_tokens[i + 1], ")") == 0)
        {
            is_pointer_instruction = 1;
            if (i >= 2)
            {
                dest = safety_tokens[i - 1];
                src = safety_tokens[i - 2];

                if (strcmp ((const char *) src, "(") == 0)
                {
                    printf ("linter error: pointer instruction without source variable!\n");
                    printf (" > %s\n", line);
                    return (1);
                }

                vi = find_var_index (src);
                if (vi == -1)
                {
                    printf ("linter error: pointer source variable '%s' is not defined!\n", src);
                    printf (" > %s\n", line);
                    return (1);
                }

                vj = find_var_index (dest);
                if (vj == -1)
                {
                    printf ("linter error: pointer variable '%s' is not defined!\n", dest);
                    printf (" > %s\n", line);
                    return (1);
                }

                if (dest[0] != 'P')
                {
                    printf ("linter error: pointer variable '%s' must begin with an uppercase 'P'!\n", dest);
                    printf (" > %s\n", line);
                    return (1);
                }

                if (vars[vj].type != QUADWORD)
                {
                    printf ("linter error: pointer variable '%s' must be an int64 variable!\n", dest);
                    printf (" > %s\n", line);
                    return (1);
                }

                if (pointer_strict == 1)
                {
                    U1 expected[MAXSTRLEN + 1];
                    const char *dot;

                    if (vars[vj].is_const == 0)
                    {
                        printf ("linter error: pointer variable '%s' must be a const-int64 variable in strict mode!\n", dest);
                        printf (" > %s\n", line);
                        return (1);
                    }

                    strcpy ((char *) expected, "P");
                    strcat ((char *) expected, (const char *) src);
                    dot = strrchr ((const char *) src, '.');
                    if (strcmp ((const char *) dest, (const char *) expected) != 0)
                    {
                        if (dot == NULL)
                        {
                            printf ("linter error: pointer name '%s' is not 'P%s'!\n", dest, src);
                            printf (" > %s\n", line);
                            return (1);
                        }
                        strcpy ((char *) expected, "P");
                        strcat ((char *) expected, dot + 1);
                        if (strcmp ((const char *) dest, (const char *) expected) != 0)
                        {
                            printf ("linter error: pointer name '%s' is not 'P%s'!\n", dest, src);
                            printf (" > %s\n", line);
                            return (1);
                        }
                    }
                }

                // the source variable can now be modified through the pointer
                U1 src_is_null = 0;

                if ((vars[vi].value_known == 1 && vars[vi].value == 0) ||
                    vars[vi].pointer_null == 1)
                {
                    src_is_null = 1;
                }

                vars[vi].aliased = 1;
                vars[vi].value_known = 0;
                vars[vi].dvalue_known = 0;

                vars[vj].initialized = 1;
                vars[vj].pointer_set = 1;
                vars[vj].is_pointer = 1;
                vars[vj].value_known = 0;
                vars[vj].dvalue_known = 0;
                vars[vj].range_known = 0;
                vars[vj].may_zero = 0;
                vars[vj].warned_uninit = 0;
                vars[vj].pointer_null = src_is_null;
                vars[vj].freed = vars[vi].freed;
                vars[vj].heap_state = HEAP_NONE;
                vars[vj].heap_size_known = 0;
                vars[vj].heap_size = -1;
                vars[vj].heap_origin = (vars[vi].heap_state == HEAP_ALLOCATED ?
                                        ORIGIN_HEAP : ORIGIN_OBJECT);
                vars[vj].ptr_off_known = 1;
                vars[vj].ptr_off = 0;
                vars[vj].warned_null = 0;
                vars[vj].warned_freed = 0;

                if (pointer_strict == 1)
                {
                    strcpy ((char *) pointer_check_name, (const char *) dest);
                    pointer_check_active = 1;
                    pointer_check_line = linenum;
                }
            }
            break;
        }
    }

    if (is_pointer_instruction == 1)
    {
        return (0);
    }

    // function / library call: allocation results, string buffer writes, ...
    {
        S2 colon = -1;
        U1 *callee = NULL;
        U1 *call_args[16];
        S2 nargs = 0;
        U1 is_call = 0;

        for (i = 0; i < nt; i++)
        {
            if (strcmp ((const char *) safety_tokens[i], "!") == 0 ||
                strcmp ((const char *) safety_tokens[i], "call") == 0)
            {
                is_call = 1;
            }
        }
        if (is_call == 1)
        {
            for (i = 1; i + 1 < nt; i++)
            {
                if (strcmp ((const char *) safety_tokens[i], ":") == 0 &&
                    is_ident_start (safety_tokens[i + 1][0]) == 1)
                {
                    colon = i;
                    callee = safety_tokens[i + 1];
                    break;
                }
            }
            if (callee != NULL)
            {
                U1 raw_callee[MAXSTRLEN + 1];
                U1 *ip_callee = callee;

                if (ip_raw_callee_name (line, raw_callee) == 1)
                {
                    ip_callee = raw_callee;
                }
                nargs = collect_call_args (nt, safety_tokens, colon, call_args, 16);
                note_allocation_call (callee, nt, safety_tokens);
                check_string_call (callee, nargs, call_args, line);

                // full interprocedural analysis of user defined functions
                if (ip_enabled == 1 && ip_find_func (ip_callee) != -1)
                {
                    if (ip_analyze_call (ip_callee, nargs, call_args, line) == 1)
                    {
                        ip_call_precise = 1;
                    }
                }

                // without a full signature of the callee the stack effect is
                // unknown. This never produces false positives: checks are
                // only emitted while the stack depth is known.
                if (strcmp ((const char *) callee, "stpush") != 0)
                {
                    flow_stack_known = 0;
                }
            }
        }
    }

    // a function call can change non-const global variables; drop their values.
    // Variables written by the interprocedural callee summary are kept: the
    // sandbox already computed their new state.
    {
        U1 has_function_call = 0;

        for (i = 0; i < nt; i++)
        {
            if (strcmp ((const char *) safety_tokens[i], "call") == 0)
            {
                has_function_call = 1;
                break;
            }
            if (strcmp ((const char *) safety_tokens[i], "!") == 0)
            {
                S2 k;
                for (k = 0; k < nt; k++)
                {
                    if (strcmp ((const char *) safety_tokens[k], ":") == 0)
                    {
                        has_function_call = 1;
                        break;
                    }
                }
                if (has_function_call == 1)
                {
                    break;
                }
            }
        }
        if (has_function_call == 1)
        {
            if (ip_call_precise == 1)
            {
                // the sandbox already copied the callee's precise effects into
                // the caller variable table. In particular, arguments which the
                // callee did not write keep their known values.
                memset (ip_effect_mark, 0, sizeof (ip_effect_mark));
            }
            else
            {
                S8 k;
                for (k = 0; k <= vars_ind; k++)
                {
                    if (ip_effect_mark[k] == 1)
                    {
                        ip_effect_mark[k] = 0;
                        continue;
                    }
                    if (vars[k].is_const == 0)
                    {
                        vars[k].value_known = 0;
                        vars[k].dvalue_known = 0;
                        vars[k].range_known = 0;
                        vars[k].may_zero = 0;
                    }
                }
            }
        }
        else
        {
            // no call on this line: there is no effect to keep
            memset (ip_effect_mark, 0, sizeof (ip_effect_mark));
        }
    }

    // jsr / jsrind call a function through the stack: the stack effect is
    // unknown here.
    for (i = 0; i < nt; i++)
    {
        if (strcmp ((const char *) safety_tokens[i], "jsr") == 0 ||
            strcmp ((const char *) safety_tokens[i], "jsrind") == 0)
        {
            flow_stack_known = 0;
            break;
        }
    }

    // stpop: all variables before stpop are written
    for (i = 0; i < nt; i++)
    {
        if (strcmp ((const char *) safety_tokens[i], "stpop") == 0 ||
            strcmp ((const char *) safety_tokens[i], "stpopi") == 0 ||
            strcmp ((const char *) safety_tokens[i], "stpopd") == 0 ||
            strcmp ((const char *) safety_tokens[i], "stpopb") == 0)
        {
            S2 k;
            S2 target_count = 0;
            U1 pop_type = 0;
            U1 used_pending = 0;

            if (strcmp ((const char *) safety_tokens[i], "stpopi") == 0)
            {
                pop_type = QUADWORD;
            }
            else if (strcmp ((const char *) safety_tokens[i], "stpopd") == 0)
            {
                pop_type = DOUBLEFLOAT;
            }
            else if (strcmp ((const char *) safety_tokens[i], "stpopb") == 0)
            {
                pop_type = BYTE;
            }

            for (k = 0; k < i; k++)
            {
                if (is_ident_start (safety_tokens[k][0]) == 1 &&
                    token_is_keyword (safety_tokens[k]) == 0)
                {
                    target_count++;
                }
            }

            if (flow_stack_known == 1 && flow_stack_depth < target_count)
            {
                linter_warnings++;
                printf ("linter error: stack underflow: stpop needs %i value(s) but only %lli are on the stack!\n",
                        target_count, flow_stack_depth);
                printf (" > %s\n", line);
                flow_stack_known = 0;
            }

            for (k = 0; k < i; k++)
            {
                if (is_ident_start (safety_tokens[k][0]) == 1 &&
                    token_is_keyword (safety_tokens[k]) == 0)
                {
                    S8 tv ALIGN;
                    U1 used_vstack = 0;

                    mark_var_written (safety_tokens[k]);
                    tv = find_var_index (safety_tokens[k]);

                    // value propagated from a callee / caller through the
                    // interprocedural value stack
                    if (tv >= 0 && ip_vstack_ind > ip_vstack_floor)
                    {
                        struct int_range pr = ip_pop_range ();

                        used_vstack = 1;
                        vars[tv].initialized = 1;
                        vars[tv].pointer_set = 1;
                        vars[tv].warned_uninit = 0;
                        vars[tv].range_known = pr.known;
                        if (pr.known == 1)
                        {
                            vars[tv].range_min = pr.lo;
                            vars[tv].range_max = pr.hi;
                            vars[tv].may_zero = (U1) (pr.lo <= 0 && pr.hi >= 0);
                            if (pr.lo == pr.hi)
                            {
                                vars[tv].value_known = 1;
                                vars[tv].value = pr.lo;
                            }
                            else
                            {
                                vars[tv].value_known = 0;
                            }
                            if (vars[tv].is_pointer == 1 || safety_tokens[k][0] == 'P')
                            {
                                vars[tv].pointer_null = (U1) (pr.lo == 0 && pr.hi == 0);
                            }
                        }
                        else
                        {
                            vars[tv].range_known = 0;
                            vars[tv].value_known = 0;
                            vars[tv].may_zero = 0;
                        }
                    }

                    if (tv >= 0 && pop_type != 0 && vars[tv].type != pop_type &&
                        vars[tv].is_string == 0 && vars[tv].is_array == 0 &&
                        !(pop_type == QUADWORD &&
                          (vars[tv].type == QUADWORD || vars[tv].type == DOUBLEWORD ||
                           vars[tv].type == WORD || vars[tv].type == BYTE ||
                           vars[tv].type == BOOL)) &&
                        !(pop_type == BYTE &&
                          (vars[tv].type == BYTE || vars[tv].type == BOOL)))
                    {
                        linter_warnings++;
                        printf ("linter error: %s pops type %s but variable '%s' has a different type!\n",
                                safety_tokens[i],
                                pop_type == QUADWORD ? "int64" :
                                (pop_type == DOUBLEFLOAT ? "double" : "byte"),
                                safety_tokens[k]);
                        printf (" > %s\n", line);
                    }

                    if (used_vstack == 1)
                    {
                        // value already applied from the interprocedural stack
                    }
                    else if (tv >= 0 && used_pending == 0 && pending_alloc == 1)
                    {
                        vars[tv].initialized = 1;
                        vars[tv].pointer_set = 1;
                        vars[tv].is_pointer = 1;
                        vars[tv].pointer_null = 0;
                        vars[tv].freed = 0;
                        vars[tv].warned_freed = 0;
                        vars[tv].heap_state = HEAP_ALLOCATED;
                        vars[tv].heap_origin = ORIGIN_HEAP;
                        vars[tv].heap_size_known = pending_alloc_known;
                        vars[tv].heap_size = (pending_alloc_known ? pending_alloc_size : -1);
                        vars[tv].ptr_off_known = 1;
                        vars[tv].ptr_off = 0;
                        vars[tv].range_known = 0;
                        vars[tv].may_zero = 0;
                        used_pending = 1;
                    }
                    else if (tv >= 0 && used_pending == 0 && pending_result == 1)
                    {
                        if (pending_result_value >= 0)
                        {
                            vars[tv].range_known = 1;
                            vars[tv].range_min = pending_result_value;
                            vars[tv].range_max = pending_result_value;
                            vars[tv].value_known = 1;
                            vars[tv].value = pending_result_value;
                            vars[tv].may_zero = (U1) (pending_result_value == 0);
                        }
                        else if (vars[tv].is_string == 0 && pending_result_max >= 0)
                        {
                            vars[tv].range_known = 1;
                            vars[tv].range_min = pending_result_min;
                            vars[tv].range_max = pending_result_max;
                            vars[tv].value_known = 0;
                            vars[tv].may_zero = (U1) (pending_result_min <= 0 &&
                                                       pending_result_max >= 0);
                        }
                        else if (vars[tv].is_string == 0)
                        {
                            vars[tv].range_known = 1;
                            vars[tv].range_min = 0;
                            vars[tv].range_max = LLONG_MAX;
                            vars[tv].may_zero = 1;
                        }
                        used_pending = 1;
                    }
                    else if (tv >= 0 && vars[tv].is_pointer == 1)
                    {
                        // unknown pointer returned from a function
                        vars[tv].heap_state = HEAP_NONE;
                        vars[tv].heap_size_known = 0;
                        vars[tv].heap_size = -1;
                    }
                }
            }
            pending_alloc = 0;
            pending_alloc_known = 0;
            pending_alloc_size = -1;
            pending_result = 0;
            pending_result_min = 0;
            pending_result_max = -1;
            if (flow_stack_known == 1)
            {
                flow_stack_depth -= target_count;
            }
            break;
        }
    }

    // stpush: track the number of pushed values
    for (i = 0; i < nt; i++)
    {
        if (strcmp ((const char *) safety_tokens[i], "stpush") == 0 ||
            strcmp ((const char *) safety_tokens[i], "stpushi") == 0 ||
            strcmp ((const char *) safety_tokens[i], "stpushd") == 0 ||
            strcmp ((const char *) safety_tokens[i], "stpushb") == 0)
        {
            S2 k;
            S2 push_count = 0;
            U1 has_operator = 0;

            for (k = 0; k < i; k++)
            {
                S2 prec = operator_precedence (safety_tokens[k]);

                if (strcmp ((const char *) safety_tokens[k], "(") == 0 ||
                    strcmp ((const char *) safety_tokens[k], ")") == 0)
                {
                    continue;
                }
                if (prec > 0)
                {
                    has_operator = 1;
                    break;
                }
                if (is_ident_start (safety_tokens[k][0]) == 1 ||
                    is_number_token (safety_tokens[k]) == 1)
                {
                    push_count++;
                }
            }
            if (has_operator == 1)
            {
                flow_stack_known = 0;
            }
            else if (push_count > 0)
            {
                if (flow_stack_known == 1)
                {
                    flow_stack_depth += push_count;
                }
            }

            // remember the pushed value ranges for later stpop / return
            {
                S2 go = -1;
                S2 gc = -1;

                if (find_enclosing_paren (nt, safety_tokens, i, &go, &gc) == 1)
                {
                    struct int_range pushes[IP_MAX_RETURNS];
                    S2 pc = ip_eval_rpn (nt, safety_tokens, go + 1, i - 1,
                                         pushes, IP_MAX_RETURNS);
                    S2 pi;

                    for (pi = 0; pi < pc; pi++)
                    {
                        ip_push_range (pushes[pi]);
                    }
                }
            }
            break;
        }
    }

    // assignments: the token before ':=' or '=' is the target
    for (i = 0; i < nt; i++)
    {
        if (strcmp ((const char *) safety_tokens[i], ":=") == 0 ||
            strcmp ((const char *) safety_tokens[i], "=") == 0)
        {
            if (i > 0)
            {
                if (strcmp ((const char *) safety_tokens[i - 1], "]") != 0)
                {
                    if (is_ident_start (safety_tokens[i - 1][0]))
                    {
                        S8 target_vi ALIGN = find_var_index (safety_tokens[i - 1]);

                        if (target_vi >= 0 && vars[target_vi].is_const == 1)
                        {
                            linter_warnings++;
                            printf ("linter error: logical bug: assignment to constant variable '%s'!\n",
                                    (const char *) safety_tokens[i - 1]);
                            printf (" > %s\n", line);
                        }
                        // (x x =) / (x x :=) is a no-op, usually a typo
                        if (i >= 2 &&
                            strcmp ((const char *) safety_tokens[i - 2],
                                    (const char *) safety_tokens[i - 1]) == 0 &&
                            is_ident_start (safety_tokens[i - 2][0]) == 1 &&
                            token_is_keyword (safety_tokens[i - 2]) == 0)
                        {
                            linter_warnings++;
                            printf ("linter error: logical bug: self-assignment of variable '%s' (no-op)!\n",
                                    (const char *) safety_tokens[i - 1]);
                            printf (" > %s\n", line);
                        }
                        mark_var_written (safety_tokens[i - 1]);
                    }
                }
            }
        }
    }

    // division / modulo by a known zero
    for (i = 1; i + 1 < nt; i++)
    {
        S2 op_len = (S2) strlen_safe ((const char *) safety_tokens[i], MAXSTRLEN);

        if ((safety_tokens[i][0] == '/' || safety_tokens[i][0] == '%') &&
            (op_len == 1 || (op_len == 2 && is_operator_suffix (safety_tokens[i][1]) == 1)))
        {
            S2 group_open;
            S2 group_close;
            S2 divisor_pos;
            S8 divisor_value ALIGN;
            U1 divisor_overflow = 0;
            U1 divisor_known;
            U1 postfix;

            // floating point division by zero is not a hard error
            if (safety_tokens[i][1] == 'd')
            {
                continue;
            }

            if (find_enclosing_paren (nt, safety_tokens, i, &group_open, &group_close) == 0)
            {
                continue;
            }
            postfix = group_is_postfix (nt, safety_tokens, group_open, group_close);
            divisor_pos = postfix == 1 ? i - 1 : i + 1;
            if (divisor_pos < 0 || divisor_pos >= nt)
            {
                continue;
            }

            if (postfix == 1)
            {
                divisor_known = eval_operand_before (nt, safety_tokens, divisor_pos, &divisor_value, &divisor_overflow);
            }
            else
            {
                divisor_known = eval_operand_after (nt, safety_tokens, divisor_pos, &divisor_value, &divisor_overflow);
            }
            if (divisor_overflow == 1)
            {
                printf ("linter error: integer overflow in constant expression!\n");
                printf (" > %s\n", line);
                return (1);
            }
            if (divisor_known == 1 && divisor_value == 0)
            {
                warn_line_mark (linenum);
                if (safety_tokens[i][0] == '%')
                {
                    printf ("linter error: modulo by zero!\n");
                }
                else
                {
                    printf ("linter error: division by zero!\n");
                }
                printf (" > %s\n", line);
                return (1);
            }
        }
    }

    // division / modulo by a variable whose range can contain zero
    for (i = 1; i + 1 < nt; i++)
    {
        S2 op_len = (S2) strlen_safe ((const char *) safety_tokens[i], MAXSTRLEN);

        if ((safety_tokens[i][0] == '/' || safety_tokens[i][0] == '%') &&
            (op_len == 1 || (op_len == 2 && is_operator_suffix (safety_tokens[i][1]) == 1)))
        {
            S2 group_open;
            S2 group_close;
            S2 divisor_pos;
            U1 postfix;
            struct int_range dr;
            U1 dov = 0;

            if (safety_tokens[i][1] == 'd')
            {
                continue;
            }
            if (find_enclosing_paren (nt, safety_tokens, i, &group_open, &group_close) == 0)
            {
                continue;
            }
            postfix = group_is_postfix (nt, safety_tokens, group_open, group_close);
            divisor_pos = (postfix == 1 ? i - 1 : i + 1);
            (void) operand_range_at (nt, safety_tokens, divisor_pos, &dr, &dov);
            if (dov == 1)
            {
                printf ("linter error: integer overflow in divisor expression!\n");
                printf (" > %s\n", line);
                return (1);
            }
            if (dr.known == 1 && dr.lo <= 0 && dr.hi >= 0)
            {
                linter_warnings++;
                warn_line_mark (linenum);
                printf ("linter error: %s by a value which can be zero (range [%lli, %lli])!\n",
                        safety_tokens[i][0] == '%' ? "modulo" : "division", dr.lo, dr.hi);
                printf (" > %s\n", line);
            }
            else if (dr.known == 0 && divisor_pos >= 0 && divisor_pos < nt &&
                     is_ident_start (safety_tokens[divisor_pos][0]) == 1 &&
                     token_is_keyword (safety_tokens[divisor_pos]) == 0)
            {
                S8 dv ALIGN = find_var_index (safety_tokens[divisor_pos]);

                if (dv >= 0 && vars[dv].may_zero == 1)
                {
                    linter_warnings++;
                    warn_line_mark (linenum);
                    printf ("linter error: %s by a value which may be zero!\n",
                            safety_tokens[i][0] == '%' ? "modulo" : "division");
                    printf (" > %s\n", line);
                }
            }
            if ((safety_tokens[i][0] == '/' || safety_tokens[i][0] == '%') &&
                dr.known == 1 && dr.lo == -1 && dr.hi == -1)
            {
                struct int_range nr;
                U1 nov = 0;
                S2 left_pos = (postfix == 1 ? i - 2 : group_open + 1);

                if (operand_range_at (nt, safety_tokens, left_pos, &nr, &nov) == 1 &&
                    nr.known == 1 && nr.lo <= LLONG_MIN)
                {
                    linter_warnings++;
                    warn_line_mark (linenum);
                    printf ("linter error: INT64_MIN / -1 or %% -1 overflow possible!\n");
                    printf (" > %s\n", line);
                }
            }
        }
    }

    // shift amount out of range (undefined behaviour in C, and rejected by
    // the L1VM for amounts outside 0..63). The tokenizer splits '<|' into
    // '<' and '|', so check adjacent tokens.
    for (i = 1; i + 1 < nt; i++)
    {
        if ((strcmp ((const char *) safety_tokens[i], "<") == 0 ||
             strcmp ((const char *) safety_tokens[i], ">") == 0) &&
            strcmp ((const char *) safety_tokens[i + 1], "|") == 0)
        {
            S2 group_open;
            S2 group_close;
            S2 amount_pos;
            U1 postfix;
            struct int_range sr;
            U1 sov = 0;

            if (find_enclosing_paren (nt, safety_tokens, i, &group_open, &group_close) == 0)
            {
                continue;
            }
            postfix = group_is_postfix (nt, safety_tokens, group_open, group_close);
            amount_pos = (postfix == 1 ? i - 1 : i + 2);
            if (operand_range_at (nt, safety_tokens, amount_pos, &sr, &sov) == 0)
            {
                continue;
            }
            (void) sov;
            if (sr.known == 1 && (sr.lo < 0 || sr.hi > 63))
            {
                linter_warnings++;
                warn_line_mark (linenum);
                printf ("linter error: shift amount out of range [0, 63]: [%lli, %lli]!\n",
                        sr.lo, sr.hi);
                printf (" > %s\n", line);
            }
        }
    }

    // constant propagation for assignments outside conditional blocks
    for (i = 0; i < nt; i++)
    {
        if (strcmp ((const char *) safety_tokens[i], ":=") == 0 ||
            strcmp ((const char *) safety_tokens[i], "=") == 0)
        {
            if (i > 0 && strcmp ((const char *) safety_tokens[i - 1], "]") != 0 &&
                is_ident_start (safety_tokens[i - 1][0]))
            {
                S2 group_open;
                S2 group_close;
                S2 expr_start;
                S2 expr_end;
                S8 const_value ALIGN;
                U1 const_overflow = 0;
                U1 const_known;
                struct int_range rr;
                U1 range_overflow = 0;

                if (find_enclosing_paren (nt, safety_tokens, i, &group_open, &group_close) == 0)
                {
                    continue;
                }
                expr_start = group_open + 1;
                expr_end = i - 2;
                if (expr_end < expr_start)
                {
                    continue;
                }

                const_known = eval_expr (nt, safety_tokens, expr_start, expr_end, &const_value, &const_overflow);
                if (const_overflow == 1)
                {
                    printf ("linter error: integer overflow in constant expression!\n");
                    printf (" > %s\n", line);
                    return (1);
                }

                rr = range_unknown ();
                (void) range_eval (nt, safety_tokens, expr_start, expr_end, &rr, &range_overflow);
                if (range_overflow == 1 && const_known == 0)
                {
                    if (rr.known == 1 && (rr.lo == LLONG_MIN || rr.hi == LLONG_MAX))
                    {
                        // saturated: only report if the range clearly leaves int64
                    }
                    linter_warnings++;
                    printf ("linter error: integer overflow possible in assignment!\n");
                    printf (" > %s\n", line);
                }

                vj = find_var_index (safety_tokens[i - 1]);
                if (vj >= 0 && vars[vj].is_array == 0 && vars[vj].is_string == 0)
                {
                    if (rr.known == 1)
                    {
                        S8 type_lo ALIGN;
                        S8 type_hi ALIGN;

                        vars[vj].range_known = 1;
                        vars[vj].range_min = rr.lo;
                        vars[vj].range_max = rr.hi;
                        vars[vj].may_zero = (U1) (rr.lo <= 0 && rr.hi >= 0);
                        type_range (vars[vj].type, &type_lo, &type_hi);
                        if (rr.lo < type_lo || rr.hi > type_hi)
                        {
                            linter_warnings++;
                            printf ("linter error: value range [%lli, %lli] does not fit into type of '%s'!\n",
                                    rr.lo, rr.hi, safety_tokens[i - 1]);
                            printf (" > %s\n", line);
                        }
                    }
                    else
                    {
                        vars[vj].range_known = 0;
                        vars[vj].may_zero = 0;
                    }
                    if (expr_start == expr_end &&
                        is_ident_start (safety_tokens[expr_start][0]) == 1)
                    {
                        S8 src_zero_vi ALIGN = find_var_index (safety_tokens[expr_start]);

                        if (src_zero_vi >= 0 && src_zero_vi != vj)
                        {
                            vars[vj].may_zero = vars[src_zero_vi].may_zero;
                        }
                    }

                    if (const_known == 1 &&
                        if_inside == 0 && switch_inside == 0 && for_inside == 0 && do_inside == 0 &&
                        vars[vj].aliased == 0)
                    {
                        vars[vj].value_known = 1;
                        vars[vj].value = const_value;
                    }

                    if (safety_tokens[i - 1][0] == 'P' || vars[vj].is_pointer == 1)
                    {
                        vars[vj].pointer_null = (U1) (rr.known == 1 && rr.lo == 0 && rr.hi == 0);
                        if (expr_start == expr_end && is_ident_start (safety_tokens[expr_start][0]) == 1)
                        {
                            S8 src_vi ALIGN = find_var_index (safety_tokens[expr_start]);

                            if (src_vi >= 0 && src_vi != vj)
                            {
                                vars[vj].heap_state = vars[src_vi].heap_state;
                                vars[vj].heap_size_known = vars[src_vi].heap_size_known;
                                vars[vj].heap_size = vars[src_vi].heap_size;
                                vars[vj].heap_origin = vars[src_vi].heap_origin;
                                vars[vj].ptr_off_known = vars[src_vi].ptr_off_known;
                                vars[vj].ptr_off = vars[src_vi].ptr_off;
                                vars[vj].freed = vars[src_vi].freed;
                                vars[vj].pointer_null = vars[src_vi].pointer_null;
                            }
                        }
                        else if (rr.known == 1)
                        {
                            vars[vj].heap_state = HEAP_NONE;
                            vars[vj].heap_origin = ORIGIN_UNKNOWN;
                        }
                    }
                }
                else if (vj >= 0 && vars[vj].is_string == 1)
                {
                    S8 lit_len ALIGN = token_string_length (safety_tokens[expr_start]);

                    if (expr_start == expr_end && lit_len >= 0)
                    {
                        if (check_string_capacity (safety_tokens[i - 1], lit_len, line) == 1)
                        {
                            // capacity violation already reported
                        }
                        update_string_length (safety_tokens[i - 1], lit_len);
                    }
                    else
                    {
                        update_string_length (safety_tokens[i - 1], -1);
                    }
                }
            }
        }
    }

    // array / pointer access
    for (i = 0; i < nt; i++)
    {
        if (strcmp ((const char *) safety_tokens[i], "[") == 0)
        {
            if (i == 0)
            {
                continue;
            }
            base = safety_tokens[i - 1];
            if (! is_ident_start (base[0]))
            {
                continue;
            }

            vi = find_var_index (base);
            if (vi == -1)
            {
                continue;
            }

            if (vars[vi].is_array == 0 && vars[vi].is_string == 0)
            {
                if (vars[vi].pointer_set == 0)
                {
                    printf ("linter error: '%s' is used as array/pointer base, but it is not an array and no 'pointer' instruction was found!\n", base);
                    printf (" > %s\n", line);
                    return (1);
                }
                if (vars[vi].pointer_null == 1 && vars[vi].warned_null == 0)
                {
                    vars[vi].warned_null = 1;
                    linter_warnings++;
                    printf ("linter error: pointer '%s' is known to be null and is dereferenced!\n", base);
                    printf (" > %s\n", line);
                }
                if (vars[vi].freed == 1 && vars[vi].warned_freed == 0)
                {
                    vars[vi].warned_freed = 1;
                    linter_warnings++;
                    printf ("linter error: pointer '%s' is used after it was freed!\n", base);
                    printf (" > %s\n", line);
                }
                vars[vi].is_pointer = 1;
            }

            // check compile time known indices of real arrays
            if (vars[vi].is_array == 1 && vars[vi].array_size > 1 && i + 1 < nt)
            {
                S8 index_value ALIGN;
                U1 index_overflow = 0;
                U1 index_known;

                index_known = eval_operand_after (nt, safety_tokens, i + 1, &index_value, &index_overflow);
                if (index_overflow == 1)
                {
                    printf ("linter error: integer overflow in array index expression!\n");
                    printf (" > %s\n", line);
                    return (1);
                }

                if (index_known == 1)
                {
                    S8 elem_size ALIGN = var_element_size (vars[vi].type);
                    S8 array_bytes ALIGN = vars[vi].array_size * elem_size;

                    if (index_value < 0 || index_value > array_bytes - elem_size)
                    {
                        printf ("linter error: array index out of bounds: '%s[%lli]', array size is %lli byte (element size %lli)!\n",
                                base, index_value, array_bytes, elem_size);
                        printf (" > %s\n", line);
                        return (1);
                    }
                }
            }

            // interval analysis of the index expression
            {
                S2 close_idx = find_close_bracket (nt, safety_tokens, i);
                struct int_range ir;
                U1 iov = 0;

                if (close_idx > i + 1 &&
                    range_eval (nt, safety_tokens, i + 1, close_idx - 1, &ir, &iov) == 1 &&
                    iov == 0 && ir.known == 1)
                {
                    if (vars[vi].is_array == 1 && vars[vi].array_size > 1)
                    {
                        S8 elem_size ALIGN = var_element_size (vars[vi].type);
                        S8 max_off ALIGN = vars[vi].array_size * elem_size - elem_size;

                        if (ir.hi < 0 || ir.lo > max_off)
                        {
                            linter_warnings++;
                            printf ("linter error: array index out of bounds: '%s' index range [%lli, %lli], valid byte range [0, %lli]!\n",
                                    base, ir.lo, ir.hi, max_off);
                            printf (" > %s\n", line);
                        }
                        else if (ir.hi > max_off ||
                                 (ir.lo < 0 && ir.lo != LLONG_MIN))
                        {
                            linter_warnings++;
                            printf ("linter error: array index may be out of bounds: '%s' index range [%lli, %lli], valid byte range [0, %lli]!\n",
                                    base, ir.lo, ir.hi, max_off);
                            printf (" > %s\n", line);
                        }
                    }
                    else if (vars[vi].is_string == 1 && vars[vi].capacity >= 0)
                    {
                        if (ir.hi < 0 || ir.lo > vars[vi].capacity - 1)
                        {
                            linter_warnings++;
                            printf ("linter error: string index out of bounds: '%s' index range [%lli, %lli], capacity %lli!\n",
                                    base, ir.lo, ir.hi, vars[vi].capacity);
                            printf (" > %s\n", line);
                        }
                        else if (ir.hi > vars[vi].capacity - 1 ||
                                 (ir.lo < 0 && ir.lo != LLONG_MIN))
                        {
                            linter_warnings++;
                            printf ("linter error: string index may be out of bounds: '%s' index range [%lli, %lli], capacity %lli!\n",
                                    base, ir.lo, ir.hi, vars[vi].capacity);
                            printf (" > %s\n", line);
                        }
                    }
                    else if (vars[vi].heap_state == HEAP_ALLOCATED &&
                             vars[vi].heap_size_known == 1 &&
                             vars[vi].ptr_off_known == 1)
                    {
                        S8 off ALIGN = vars[vi].ptr_off;
                        S8 max_off ALIGN = vars[vi].heap_size - 1 - off;

                        if (ir.hi < -off || ir.lo > max_off)
                        {
                            linter_warnings++;
                            printf ("linter error: heap access out of bounds: '%s' pointer offset range [%lli, %lli], allocation size %lli!\n",
                                    base, ir.lo + off, ir.hi + off, vars[vi].heap_size);
                            printf (" > %s\n", line);
                        }
                        else if (ir.hi > max_off ||
                                 (ir.lo < -off && ir.lo != LLONG_MIN))
                        {
                            linter_warnings++;
                            printf ("linter error: heap access may be out of bounds: '%s' pointer offset range [%lli, %lli], allocation size %lli!\n",
                                    base, ir.lo + off, ir.hi + off, vars[vi].heap_size);
                            printf (" > %s\n", line);
                        }
                    }
                }
            }
        }
    }

    // compile-time range checks: (value min max range)
    for (i = 4; i + 1 < nt; i++)
    {
        if (strcmp ((const char *) safety_tokens[i], "range") == 0 &&
            strcmp ((const char *) safety_tokens[i + 1], ")") == 0)
        {
            S8 range_var ALIGN = find_var_index (safety_tokens[i - 3]);
            U1 is_double_range = (U1) ((range_var >= 0 && vars[range_var].type == DOUBLEFLOAT) ||
                                        is_float_token (safety_tokens[i - 3]));

            if (is_double_range == 1)
            {
                double range_dvalue;
                double range_dmin;
                double range_dmax;
                U1 dvalue_known = eval_operand_double_before (nt, safety_tokens, i - 3, &range_dvalue);
                U1 dmin_known = eval_operand_double_before (nt, safety_tokens, i - 2, &range_dmin);
                U1 dmax_known = eval_operand_double_before (nt, safety_tokens, i - 1, &range_dmax);

                if (dmin_known == 1 && dmax_known == 1)
                {
                    if (range_dmin > range_dmax)
                    {
                        printf ("linter error: invalid range: minimum %g is greater than maximum %g!\n",
                                range_dmin, range_dmax);
                        printf (" > %s\n", line);
                        return (1);
                    }
                    if (dvalue_known == 1 && (range_dvalue < range_dmin || range_dvalue > range_dmax))
                    {
                        printf ("linter error: value %g is outside compile-time range [%g, %g]!\n",
                                range_dvalue, range_dmin, range_dmax);
                        printf (" > %s\n", line);
                        return (1);
                    }
                }
            }
            else
            {
                S8 range_value ALIGN;
                S8 range_min ALIGN;
                S8 range_max ALIGN;
                U1 value_known;
                U1 min_known;
                U1 max_known;
                U1 value_overflow = 0;
                U1 min_overflow = 0;
                U1 max_overflow = 0;

                max_known = eval_operand_before (nt, safety_tokens, i - 1, &range_max, &max_overflow);
                min_known = eval_operand_before (nt, safety_tokens, i - 2, &range_min, &min_overflow);
                value_known = eval_operand_before (nt, safety_tokens, i - 3, &range_value, &value_overflow);
                if (value_overflow == 1 || min_overflow == 1 || max_overflow == 1)
                {
                    printf ("linter error: integer overflow in range check expression!\n");
                    printf (" > %s\n", line);
                    return (1);
                }
                if (min_known == 1 && max_known == 1)
                {
                    if (range_min > range_max)
                    {
                        printf ("linter error: invalid range: minimum %lli is greater than maximum %lli!\n",
                                range_min, range_max);
                        printf (" > %s\n", line);
                        return (1);
                    }
                    if (value_known == 1 && (range_value < range_min || range_value > range_max))
                    {
                        printf ("linter error: value %lli is outside compile-time range [%lli, %lli]!\n",
                                range_value, range_min, range_max);
                        printf (" > %s\n", line);
                        return (1);
                    }
                }
            }
        }
    }

    // warn about reads of uninitialized variables
    for (i = 0; i < nt; i++)
    {
        name = safety_tokens[i];

        if (name[0] == '\0' || name[0] == '"')
        {
            continue;
        }
        if (! is_ident_start (name[0]))
        {
            continue;
        }
        if (token_is_keyword (name) == 1)
        {
            continue;
        }
        // function names and labels after ':'
        if (i > 0 && strcmp ((const char *) safety_tokens[i - 1], ":") == 0)
        {
            continue;
        }
        // array base variables are checked above
        if (i + 1 < nt && strcmp ((const char *) safety_tokens[i + 1], "[") == 0)
        {
            continue;
        }
        warn_uninitialized (name, line);

        vi = find_var_index (name);
        if (vi >= 0 && vars[vi].freed == 1 && vars[vi].warned_freed == 0)
        {
            vars[vi].warned_freed = 1;
            linter_warnings++;
            printf ("linter error: pointer '%s' is used after it was freed!\n", name);
            printf (" > %s\n", line);
        }
    }

    // deallocation: invalid free, double free, use after free
    for (i = 1; i + 1 < nt; i++)
    {
        if (strcmp ((const char *) safety_tokens[i - 1], ":") == 0 &&
            (strcmp ((const char *) safety_tokens[i], "dealloc") == 0 ||
             strcmp ((const char *) safety_tokens[i], "dealloc_vect") == 0 ||
             strcmp ((const char *) safety_tokens[i], "free_obj_memobj") == 0))
        {
            U1 builtin = (U1) (strcmp ((const char *) safety_tokens[i], "free_obj_memobj") != 0);

            if (i >= 2 && is_ident_start (safety_tokens[i - 2][0]))
            {
                vj = find_var_index (safety_tokens[i - 2]);
                if (vj >= 0)
                {
                    if (vars[vj].heap_state == HEAP_FREED || vars[vj].freed == 1)
                    {
                        linter_warnings++;
                        printf ("linter error: double free: pointer '%s' was already freed!\n",
                                safety_tokens[i - 2]);
                        printf (" > %s\n", line);
                    }
                    else if (builtin == 1 && vars[vj].is_array == 1)
                    {
                        linter_warnings++;
                        printf ("linter error: invalid free: '%s' is a static array/object, not heap memory!\n",
                                safety_tokens[i - 2]);
                        printf (" > %s\n", line);
                    }
                    else if (builtin == 1 && vars[vj].heap_origin == ORIGIN_OBJECT)
                    {
                        linter_warnings++;
                        printf ("linter error: invalid free: pointer '%s' points to a static array/object!\n",
                                safety_tokens[i - 2]);
                        printf (" > %s\n", line);
                    }
                    else if (builtin == 1 && vars[vj].heap_state == HEAP_NONE &&
                             vars[vj].heap_origin == ORIGIN_UNKNOWN &&
                             vars[vj].pointer_set == 1 &&
                             vars[vj].is_pointer == 1)
                    {
                        // This can be a pointer returned by a library function.
                        // Do not reject it, but remember it was freed.
                    }

                    if (vars[vj].pointer_set == 1 || vars[vj].is_array == 1 ||
                        vars[vj].heap_state == HEAP_ALLOCATED)
                    {
                        vars[vj].freed = 1;
                        vars[vj].heap_state = HEAP_FREED;
                        vars[vj].warned_freed = 0;
                    }
                }
            }
        }
    }

    return (0);
}

void get_current_function_name (U1 *line)
{
    S2 i = 0, n = 0;
    U1 ok = 1;
    U1 name_start = 0;
    U1 name[MAXSTRLEN + 1];

    while (ok)
    {
        if (line[i] == '(')
        {
            name_start = 1;
            i++;
        }
        if (name_start == 1)
        {
            if (line[i] != ' ')
            {
                name[n] = line[i];
                n++;
            }
            else
            {
                name[n] = '\0';
                ok = 0;
            }
        }
        i++;
    }
    // printf ("get_current_function_name: '%s'\n", name);
    strcpy ((char *) function_name_current, (const char *) name);
}

S2 check_exit_call (U1 *line)
{
    S2 i, j;
    S2 start_pos;
    S2 end_pos;
    S2 var_start;
    U1 varname[MAXSTRLEN];
    S2 var_type;

    start_pos = searchstr (line, (U1 *) "(255", 0, 0, TRUE);
    if (start_pos == -1)
    {
        // error, no valid exit call interrupt start
        return (1);
    }

    end_pos = searchstr (line, (U1 *) "intr0)", 0, 0, TRUE);
    if (end_pos == -1)
    {
        // error, no valid exit call interrupt end
        return (1);
    }

    // sane check:
    if (end_pos <= start_pos)
    {
        printf ("error: exit interrupt syntax error!\n");
        return (1);
    }

    var_start = start_pos + 5;
    j = 0;
    for (i = var_start; i < end_pos; i++)
    {
        if (line[i] != ' ')
        {
            varname[j] = line[i];
            if (j < MAXSTRLEN - 1)
            {
                j++;
            }
            else
            {
                // error variable overflow
                printf ("error: exit interrupt: variable name overflow!\n");
                return (1);
            }
        }
        else
        {
            varname[j] = '\0';
            break;
        }
    }

    // check vartype
    var_type = get_varname_type (varname);
    if (var_type == -1)
    {
        printf ("error: exit interrupt: variable '%s' not set!\n", varname);
        return (1);
    }

    if (var_type == DOUBLEFLOAT)
    {
        printf ("error: exit interrupt: variable '%s' is not int type!\n", varname);
        return (1);
    }

    return (0);
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

S2 parse_line (U1 *line)
{
    S2 pos, type_pos, type_ind = 0, i, ret_start, strict_pos;
    S2 name_pos, name_ind = 0;
    S2 line_len = 0;
    U1 get_type = 1;
    U1 get_name;
    S2 vartype = 0;
    S2 type_len = 0;
    S2 name_len = 0;
    S2 args_ind = -1;
    U1 do_parse_vars;
    S2 function_call = 0;
    S8 function_index ALIGN = 0;
    S2 func_pos = 0;
    S2 funcend_pos = 0;
    S2 func_args = 0;
    S2 stpop_pos = 0;
    S2 return_pos = 0;
    S2 func_stpop_pos = 0;
    S2 if_pos = 0;
    S2 endif_pos = 0;
    S2 switch_pos = 0;
    S2 switchend_pos = 0;
    S2 for_pos  = 0;
    S2 forend_pos = 0;
    S2 do_pos = 0;
    S2 doend_pos = 0;

    U1 type[MAXSTRLEN + 1];
    U1 name[MAXSTRLEN + 1];

    S8 return_args_i = 0;
    U1 line_is_set = 0;
    S2 set_start = 0;

    line_len = strlen_safe ((const char *) line, MAXSTRLEN);

    // enhanced safety checks: uninitialized pointers, array bounds, ...
    if (safety_check_line (line) != 0)
    {
        return (1);
    }

    // a declaration line can contain "func)" or "(funcend)" inside a string
    // value, so it must not be treated as a function start or end
    set_start = 0;
    while (line[set_start] == ' ' || line[set_start] == '\t')
    {
        set_start++;
    }
    if (strncmp ((const char *) &line[set_start], "(set", 4) == 0 &&
        (line[set_start + 4] == ' ' || line[set_start + 4] == '\t' ||
         line[set_start + 4] == '\0'))
    {
        line_is_set = 1;
    }

    // check if function start / end. This must be done before any early return
    // because a function name can contain keywords like "set".
    if (line_is_set == 0)
    {
    func_pos = searchstr (line, (U1 *) "func)", 0, 0, TRUE);
    if (func_pos != -1)
    {
        if (function_inside == 1)
        {
            printf ("linter error: function '%s' is not closed by '(funcend)' before the next function starts!\n", function_name_current);
            printf (" > %s\n", line);
            return (1);
        }
        get_current_function_name (line);
        function_inside = 1;  // start of functiuon
        function_return = 0;  // reset
        if_inside = 0;
        switch_inside = 0;
        for_inside = 0;
        do_inside = 0;
    }

    funcend_pos = searchstr (line, (U1 *) "(funcend)", 0, 0, TRUE);
    if (funcend_pos != -1)
    {
        if (function_inside == 1)
        {
            if (if_inside != 0 || switch_inside != 0 || for_inside != 0 || do_inside != 0)
            {
                printf ("linter error: function '%s' has unclosed control blocks (if: %i, switch: %i, for: %i, do: %i)!\n",
                        function_name_current, if_inside, switch_inside, for_inside, do_inside);
                printf (" > %s\n", line);
                function_inside = 0;
                if_inside = 0;
                switch_inside = 0;
                for_inside = 0;
                do_inside = 0;
                return (1);
            }

            function_inside = 0;
            if (flow_missing_return == 1)
            {
                // the path sensitive analysis found a path which falls through
                // the end of a function which has to return a value
                printf ("linter error: logical bug: function '%s' can reach (funcend) without a return value!\n",
                        function_name_current);
                return (1);
            }
            function_return = 0;  // reset
        }
        else
        {
            printf ("linter error: '(funcend)' without an open function!\n");
            printf (" > %s\n", line);
            return (1);
        }
    }
    } // line_is_set == 0

    if_pos = searchstr (line, (U1 *) " if)", 0, 0, TRUE);
    if (if_pos != -1)
    {
        if_inside++;
    }

    if_pos = searchstr (line, (U1 *) " if+)", 0, 0, TRUE);
    if (if_pos != -1)
    {
        if_inside++;
    }

    // (value constant ?) is a switch case, but outside a switch it is used
    // like an if block closed by (endif)
    if (switch_inside == 0 && searchstr (line, (U1 *) " ?)", 0, 0, TRUE) != -1)
    {
        if_inside++;
    }

    if (searchstr (line, (U1 *) "(else)", 0, 0, TRUE) != -1)
    {
        if (if_inside == 0)
        {
            printf ("linter error: '(else)' without an open 'if'!\n");
            printf (" > %s\n", line);
            return (1);
        }
    }

    endif_pos = searchstr (line, (U1 *) "(endif)", 0, 0, TRUE);
    if (endif_pos != -1)
    {
        if (if_inside > 0)
        {
            if_inside--;
        }
        else
        {
            printf ("linter error: '(endif)' without an open 'if'!\n");
            printf (" > %s\n", line);
            return (1);
        }
    }

    switch_pos = searchstr (line, (U1 *) "(switch)", 0, 0, TRUE);
    if (switch_pos != -1)
    {
        switch_inside++;
    }

    switchend_pos = searchstr (line, (U1 *) "(switchend)", 0, 0, TRUE);
    if (switchend_pos != -1)
    {
        if (switch_inside > 0)
        {
            switch_inside--;
        }
        else
        {
            printf ("linter error: '(switchend)' without an open 'switch'!\n");
            printf (" > %s\n", line);
            return (1);
        }
    }

    if (searchstr (line, (U1 *) "(break)", 0, 0, TRUE) != -1)
    {
        if (switch_inside == 0)
        {
            printf ("linter error: '(break)' outside of a 'switch'!\n");
            printf (" > %s\n", line);
            return (1);
        }
    }

    for_pos = searchstr (line, (U1 *) " for)", 0, 0, TRUE);
    if (for_pos != -1)
    {
        for_inside++;
    }

    forend_pos = searchstr (line, (U1 *) "(next)", 0, 0, TRUE);
    if (forend_pos != -1)
    {
        if (for_inside > 0)
        {
            for_inside--;
        }
        else
        {
            printf ("linter error: '(next)' without an open 'for' loop!\n");
            printf (" > %s\n", line);
            return (1);
        }
    }

    do_pos = searchstr (line, (U1 *) "(do)", 0, 0, TRUE);
    if (do_pos != -1)
    {
        do_inside++;
    }

    doend_pos = searchstr (line, (U1 *) " while)", 0, 0, TRUE);
    if (doend_pos == -1)
    {
        doend_pos = searchstr (line, (U1 *) "(while)", 0, 0, TRUE);
    }
    if (doend_pos != -1)
    {
        if (do_inside > 0)
        {
            do_inside--;
        }
        else
        {
            printf ("linter error: 'while' without an open 'do' loop!\n");
            printf (" > %s\n", line);
            return (1);
        }
    }

    // check if set found
    pos = searchstr (line, (U1 *) "(set", 0, 0, TRUE);
    if (pos != -1)
    {
        func_pos = searchstr (line, (U1 *) "func)", 0, 0, TRUE);
        if (func_pos != -1)
        {
            return (0);   // function define
        }

        if (vars_ind >= MAXVARS)
        {
            printf ("parse_line: variables overflow!\n");
            return (1);
        }

        type_pos = pos + 5;
        i = type_pos;

        #if DEBUG
        printf ("parse_line: set: type pos: %i, char: '%c'\n", type_pos, line[i]);
        #endif

        while (get_type == 1)
        {
            if (line[i] != ' ')
            {
                if (type_ind >= MAXSTRLEN)
                {
                    printf ("parse_line: set type overflow!\n");
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
                printf ("parse_line: line overflow!\n");
                return (1);
            }
        }

        // get vartype
        vartype = getvartype (type);
        if (vartype == 0)
        {
            // error no valid vartype
            printf ("parse-line: set: invalid variable type: %s ! \n", type);
            return (1);
        }

        type_len = strlen_safe ((const char *) type, MAXSTRLEN);
        name_pos = type_pos + type_len;

        // skip the size field: (set TYPE SIZE NAME ...)
        while (name_pos < line_len && line[name_pos] == ' ')
        {
            name_pos++;
        }
        while (name_pos < line_len && line[name_pos] != ' ' && line[name_pos] != ')')
        {
            name_pos++;
        }
        while (name_pos < line_len && line[name_pos] == ' ')
        {
            name_pos++;
        }
        if (name_pos >= line_len || line[name_pos] == ')')
        {
            printf ("parse_line: set: no variable name found!\n");
            return (1);
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
                    printf ("parse_line: set name overflow!\n");
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

        if (vars_ind < MAXVARS - 1)
        {
            U1 size_str[MAXSTRLEN + 1] = "";
            U1 is_string = 0;
            U1 is_const = 0;
            U1 initialized = 0;
            U1 value_known = 0;
            U1 dvalue_known = 0;
            S8 array_size ALIGN = -1;
            S8 known_value ALIGN = 0;
            double known_dvalue = 0.0;
            S2 ss ALIGN;
            S2 se ALIGN;
            S2 vp ALIGN;
            S2 size_len ALIGN;

            if (pointer_strict == 1)
            {
                // EDIT CHECK POINTER
                if (name[0] == 'P')
                {
                    if (checkpointer (type) != 0)
                    {
                        printf ("parse_line: pointer: '%s' not const-int64 type!\n", name);
                        return (1);
                    }
                }
            }

            // size field between type and name
            ss = type_pos + type_len + 1;
            while (ss < name_pos && line[ss] == ' ')
            {
                ss++;
            }
            se = name_pos;
            while (se > ss && line[se - 1] == ' ')
            {
                se--;
            }
            size_len = se - ss;
            if (size_len < 0)
            {
                size_len = 0;
            }
            if (size_len > MAXSTRLEN)
            {
                size_len = MAXSTRLEN;
            }
            if (size_len > 0)
            {
                memcpy (size_str, &line[ss], (size_t) size_len);
            }
            size_str[size_len] = '\0';

            if (strcmp ((const char *) type, "string") == 0 ||
                strcmp ((const char *) type, "const-string") == 0 ||
                strcmp ((const char *) type, "mut-string") == 0)
            {
                is_string = 1;
            }
            if (strncmp ((const char *) type, "const-", 6) == 0)
            {
                is_const = 1;
            }

            if (is_string == 0 && size_str[0] != '\0' && size_str[0] != 's')
            {
                array_size = (S8) strtoll ((const char *) size_str, NULL, 10);
            }

            // has the declaration an initialization value?
            vp = name_pos + (S2) strlen_safe ((const char *) name, MAXSTRLEN);
            while (vp < line_len && line[vp] == ' ')
            {
                vp++;
            }
            if (vp < line_len && line[vp] != ')')
            {
                initialized = 1;
            }

            // remember simple compile time values (constants, zero, ...)
            if (is_string == 0 && array_size <= 1 && initialized == 1)
            {
                U1 value_str[64] = "";
                S2 vk = 0;
                char *endptr = NULL;

                while (vp < line_len && line[vp] != ' ' && line[vp] != ')' && vk < 63)
                {
                    value_str[vk] = line[vp];
                    vk++;
                    vp++;
                }
                value_str[vk] = '\0';

                if (value_str[0] != '\0')
                {
                    known_value = (S8) strtoll ((const char *) value_str, &endptr, 10);
                    if (endptr != NULL && endptr != (char *) value_str && *endptr == '\0')
                    {
                        value_known = 1;
                    }

                    if (vartype == DOUBLEFLOAT)
                    {
                        char *dendptr = NULL;
                        double dval;

                        errno = 0;
                        dval = strtod ((const char *) value_str, &dendptr);
                        if (errno != ERANGE && dendptr != NULL &&
                            dendptr != (char *) value_str && *dendptr == '\0')
                        {
                            known_dvalue = dval;
                            dvalue_known = 1;
                        }
                    }
                }
            }

            vars_ind++;
            strcpy ((char *) vars[vars_ind].name, (char *) name);
            vars[vars_ind].type = vartype;
            vars[vars_ind].strict_arg = 0;

            vars[vars_ind].initialized = initialized;
            vars[vars_ind].pointer_set = 0;
            vars[vars_ind].is_pointer = 0;
            vars[vars_ind].is_array = (U1) (array_size > 1);
            vars[vars_ind].is_string = is_string;
            vars[vars_ind].is_const = is_const;
            vars[vars_ind].warned_uninit = 0;
            vars[vars_ind].pointer_null = 0;
            vars[vars_ind].freed = 0;
            vars[vars_ind].warned_null = 0;
            vars[vars_ind].warned_freed = 0;
            vars[vars_ind].aliased = 0;
            vars[vars_ind].value_known = value_known;
            vars[vars_ind].value = known_value;
            vars[vars_ind].dvalue_known = dvalue_known;
            vars[vars_ind].dvalue = known_dvalue;
            vars[vars_ind].array_size = array_size;
            vars[vars_ind].str_len_known = 0;
            vars[vars_ind].str_len = 0;
            vars[vars_ind].range_known = 0;
            vars[vars_ind].range_min = 0;
            vars[vars_ind].range_max = 0;
            vars[vars_ind].may_zero = 0;
            vars[vars_ind].heap_state = 0;
            vars[vars_ind].heap_size_known = 0;
            vars[vars_ind].heap_size = -1;
            vars[vars_ind].heap_origin = 0;

            // capacity and initializer range/string length
            if (is_string == 1)
            {
                // explicit string length: (set string N name "text")
                if (size_str[0] != '\0' && size_str[0] != 's' && is_number_token (size_str))
                {
                    vars[vars_ind].capacity = (S8) strtoll ((const char *) size_str, NULL, 10);
                }
                else
                {
                    // auto length: size of the literal including NUL
                    S8 slen ALIGN = -1;
                    S2 qpos ALIGN;
                    S2 qend ALIGN;

                    qpos = searchstr (line, (U1 *) "\"", 0, 0, TRUE);
                    if (qpos != -1)
                    {
                        qend = searchstr (line, (U1 *) "\"", qpos + 1, 0, TRUE);
                        if (qend != -1)
                        {
                            slen = qend - qpos - 1;
                        }
                    }
                    vars[vars_ind].capacity = (slen >= 0 ? slen + 1 : -1);
                }
                if (initialized == 1)
                {
                    S2 qpos ALIGN;
                    S2 qend ALIGN;

                    qpos = searchstr (line, (U1 *) "\"", 0, 0, TRUE);
                    if (qpos != -1)
                    {
                        qend = searchstr (line, (U1 *) "\"", qpos + 1, 0, TRUE);
                        if (qend != -1)
                        {
                            vars[vars_ind].str_len_known = 1;
                            vars[vars_ind].str_len = (S8) (qend - qpos - 1);
                        }
                    }
                }
            }
            else if (array_size > 1)
            {
                vars[vars_ind].capacity = array_size * var_element_size (vartype);
            }
            else
            {
                vars[vars_ind].capacity = var_element_size (vartype);
            }

            // known integer / double range of the initial value
            if (is_string == 0 && array_size <= 1 && initialized == 1 && value_known == 1)
            {
                vars[vars_ind].range_known = 1;
                vars[vars_ind].range_min = known_value;
                vars[vars_ind].range_max = known_value;
                vars[vars_ind].may_zero = (U1) (known_value == 0);
            }
            if (is_string == 0 && array_size <= 1 && initialized == 1 && dvalue_known == 1)
            {
                vars[vars_ind].range_known = 0;
            }

            // a string literal is not a numeric range, but constant numeric
            // declarations are tracked above

            #if DEBUG
            printf ("variable: index: %lli, name: '%s', type: %lli, size: %lli, init: %i\n", vars_ind, vars[vars_ind].name, vars[vars_ind].type, vars[vars_ind].array_size, vars[vars_ind].initialized);
            #endif
        }
        else
        {
            printf ("parse_line: set variable overflow!\n");
            return (1);
        }
    }

    // EDIT neu
    // check if function args found
    pos = searchstr (line, (U1 *) "// (func args ", 0, 0, TRUE);
    if (pos != -1)
    {
        if (function_args_ind < MAXFUNC - 1)
        {
            function_args_ind++;
        }
        else
        {
            printf ("parse_line: set function overflow!\n");
            return (1);
        }

        strict_pos = searchstr (line, (U1 *) "// (func args S ", 0, 0, TRUE);
        if (strict_pos != -1)
        {
            // handle function as strict. Use variables with R at the ending for return values
            function_args[function_args_ind].strict = 1;
            name_pos = pos + 16;
        }
        else
        {
            function_args[function_args_ind].strict = 0;
            name_pos = pos + 14;
        }

        // check for function name
        get_name = 1;
        i = name_pos;
        args_ind = -1;
        func_args = 0;

        while (get_name == 1)
        {
            if (line[i] != ' ')
            {
                if (name_ind >= MAXSTRLEN)
                {
                    printf ("parse_line: set name overflow!\n");
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

        strcpy ((char *) function_args[function_args_ind].name, (char *) name);

        #if DEBUG
        printf ("function: index: %lli, name: '%s'\n", function_args_ind, function_args[function_args_ind].name);
        #endif

        #if DEBUG
        printf ("parse_line: set function name: '%s'\n", name);
        #endif

        name_len = strlen_safe ((const char *) name, MAXSTRLEN);
        type_pos = name_pos + name_len + 1;
        do_parse_vars = 1;

        while (do_parse_vars == 1)
        {
            i = type_pos;
            get_type = 1;
            type_ind = 0;

            #if DEBUG
            printf ("parse_line: i (parse_start): %i\n", i);
            #endif

            while (get_type == 1)
            {
                #if DEBUG
                printf ("parse_line: i: %i, char: %c\n", i, line[i]);
                #endif

                if (line[i] != ' ' && line[i] != ')')
                {
                    if (type_ind >= MAXSTRLEN)
                    {
                        printf ("parse_line: set function overflow!\n");
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
                    func_args++;
                }

                if (i < line_len - 1)
                {
                    i++;
                }
                else
                {
                    get_type = 0;
                }
            }

            // get vartype
            #if DEBUG
            printf ("parse-line: set function type: '%s'\n", type);
            #endif

            vartype = getvartype (type);
            if (vartype == 0)
            {
                // error no valid vartype
                printf ("parse-line: set function: invalid variable type: '%s' ! \n", type);
                return (1);
            }

            if (vartype == VARTYPE_NONE)
            {
                // no function return value
                return_args[return_args_ind].args = 0;
                get_type = 0;
                do_parse_vars = 0;
                continue;
            }

            if (args_ind < MAXFUNCARGS - 1)
            {
                args_ind++;
                function_args[function_args_ind].arg_type[args_ind] = vartype;
                function_args[function_args_ind].args = func_args;
            }

            #if DEBUG
            printf ("parse-line: set function type %i: %i\n", args_ind, vartype);
            #endif

            type_len = strlen_safe ((const char *) type, MAXSTRLEN);
            type_pos = type_pos + type_len + 1 ;

            if (i == line_len - 1)
            {
                do_parse_vars = 0;
            }
        }
    }

    // set function return values
    pos = searchstr (line, (U1 *) "// (return args ", 0, 0, TRUE);
    if (pos != -1)
    {
        // check for function name
        name_pos = pos + 16;
        get_name = 1;
        i = name_pos;
        args_ind = -1;
        func_args = 0;
        name_ind = 0;

        #if DEBUG
        printf ("found return args!\n");
        printf ("line: '%s'\n", line);
        #endif

        while (get_name == 1)
        {
            if (line[i] != ' ')
            {
                if (name_ind >= MAXSTRLEN)
                {
                    printf ("parse_line: set name overflow!\n");
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

        #if DEBUG
        printf ("return args got name: '%s'\n", name);
        #endif

        // return
        if (return_args_ind < MAXFUNC - 1)
        {
            return_args_ind++;
            strcpy ((char *) return_args[return_args_ind].name, (char *) name);

            #if DEBUG
            printf ("function: return index: %lli, name: '%s'\n", return_args_ind, return_args[return_args_ind].name);
            #endif
        }
        else
        {
            printf ("parse_line: set return overflow!\n");
            return (1);
        }

        #if DEBUG
        printf ("parse_line: set return function name: '%s'\n", name);
        #endif

        name_len = strlen_safe ((const char *) name, MAXSTRLEN);
        type_pos = name_pos + name_len + 1;
        do_parse_vars = 1;

        while (do_parse_vars == 1)
        {
            i = type_pos;
            get_type = 1;
            type_ind = 0;

            #if DEBUG
            printf ("parse_line: i (parse_start): %i\n", i);
            #endif

            while (get_type == 1)
            {
                #if DEBUG
                printf ("parse_line: i: %i, char: %c\n", i, line[i]);
                #endif

                if (line[i] != ' ' && line[i] != ')')
                {
                    if (type_ind >= MAXSTRLEN)
                    {
                        printf ("parse_line: set return function overflow!\n");
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
                    func_args++;
                }
                if (i < line_len - 1)
                {
                    i++;
                }
                else
                {
                    get_type = 0;
                }
            }

            // get vartype
            #if DEBUG
            printf ("parse-line: set return function type: '%s'\n", type);
            #endif

            vartype = getvartype (type);
            if (vartype == 0)
            {
                // error no valid vartype
                printf ("parse-line: set return function: invalid variable type: '%s' !\n", type);
                return (1);
            }

            if (vartype == VARTYPE_NONE)
            {
                // no function return value
                return_args[return_args_ind].args = 0;
                get_type = 0;
                do_parse_vars = 0;
                continue;
            }

            if (args_ind < MAXFUNCARGS - 1)
            {
                args_ind++;
                return_args[return_args_ind].arg_type[args_ind] = vartype;
                return_args[return_args_ind].args = func_args;
            }

            #if DEBUG
            printf ("parse-line: set return function return type %i: %i\n", args_ind, vartype);
            #endif

            type_len = strlen_safe ((const char *) type, MAXSTRLEN);
            type_pos = type_pos + type_len + 1 ;

            if (i == line_len - 1)
            {
                do_parse_vars = 0;
            }
        }
    }

    //printf ("line: '%s'\n", line);

    // manual set strict variable remove, mark variable as invalid
    pos = searchstr (line, (U1 *) "// (remove ", 0, 0, TRUE);
    if (pos != -1)
    {
        // new scope
        {
           S2 line_len = strlen_safe ((const char *) line, MAXSTRLEN);
           // EDIT REMOVE
           S2 i, j = 0;
           U1 varname[MAXSTRLEN + 1];

           pos = pos + 11;

           for (i = pos; i < line_len; i++)
           {
               if (line[i] != ')')
               {
                   varname[j] = line[i];
                   j++;
               }
               else
               {
                   varname[j] = '\0';
                   break;
               }
           }

           if (set_var_strict_two (varname) != 0)
           {
               printf ("parse-line: can't set variable as removed!\n");
               printf ("'%s'\n", line);
               return (1);
           }
        }
    }

    pos = searchstr (line, (U1 *) "intr0)", 0, 0, TRUE);
    if (pos != -1)
    {
        pos = searchstr (line, (U1 *) "(255", 0, 0, TRUE);
        if (pos != -1)
        {
            if (check_exit_call (line) == 0)
            {
                found_exit = 1;
            }
        }
    }

    // check if function call
    function_call = 0;
    pos = 0;

    // reset name
    strcpy ((char *) name, "");

    function_call = searchstr (line, (U1 *) "call", 0, 0, TRUE);
    if (function_call != -1)
    {
        pos = function_call;
    }
    function_call = searchstr (line, (U1 *) "!", 0, 0, TRUE);
    if (function_call != -1)
    {
        pos = function_call;
    }
    if (line_is_set == 1)
    {
        // set variable declaration: return
        return (0);
    }
    function_call = searchstr (line, (U1 *) "!=", 0, 0, TRUE);
    if (function_call != -1)
    {
        // set variable declaration: return
        return (0);
    }

    if (pos != 0)
    {
       // parse function call
       // get fuction name
       // sane check:
       if (pos > 1)
       {
           if (line[pos - 1] != ' ')
           {
               printf ("parse-line: function call syntax error! No space before call!\n");
               printf ("'%s'\n", line);
               return (1);
           }
       }
       else
       {
           printf ("parse-line: function call syntax error!\n");
           printf ("'%s'\n", line);
           return (1);
       }

       name_pos = searchstr (line, (U1 *) ":", 0, 0, TRUE);

       // do sane check:
       if (name_pos > pos)
       {
           printf ("parse-line: function call syntax error!\n");
           printf ("'%s'\n", line);
           return (1);
       }

       // get name

        get_name = 1;
        i = name_pos + 1;
        name_ind = 0;

        #if DEBUG
        printf ("parse-line: function call name start pos: %i\n", i);
        #endif

        while (get_name == 1)
        {
            if (line[i] != ' ' && line[i] != ')')
            {
                if (name_ind >= MAXSTRLEN)
                {
                    printf ("parse_line: set call name overflow!\n");
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

        #if DEBUG
        printf ("parse_line: set call function name: '%s'\n", name);
        #endif

        function_index = get_function_index (name);
        if (function_index == -1)
        {
            if (func_strict == 1)
            {
                printf ("parse_line: set call function not set: '%s'!\n", name);
                return (1);
            }
        }

        i = searchstr (line, (U1 *) "(", 0, 0, TRUE);
        if (i == -1)
        {
            printf ("parse_line: set call function syntax error! no ( found!\n");
            return (1);
        }

        i++;
        do_parse_vars = 1;

        while (do_parse_vars == 1)
        {
            get_type = 1;
            type_ind = 0;

            #if DEBUG
            printf ("parse_line: i (parse_start): %i\n", i);
            #endif

            while (get_type == 1)
            {
                #if DEBUG
                printf ("parse_line: i: %i, char: %c\n", i, line[i]);
                #endif

                if (line[i] == ':')
                {
                    do_parse_vars = 0;
                    break;
                }

                if (line[i] != ' ' && line[i] != ')' && line[i] != ':')
                {
                    if (type_ind >= MAXSTRLEN)
                    {
                        printf ("parse_line: set call overflow!\n");
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

                    if (function_index >= 0 && function_args[function_index].strict == 1)
                    {
                        if (type_ind >= 2)
                        {
                            if (type[type_ind - 1] == 'R')
                            {
                                printf ("parse-line: variable '%s' is return type variable!\n", type);
                                return (1);
                            }
                        }
                    }
                }
                if (i < line_len - 1)
                {
                    i++;
                }
                else
                {
                    get_type = 0;
                }
            }

            if (do_parse_vars == 0)
            {
                break;
            }

            // get vartype
            #if DEBUG
            printf ("parse-line: call function type: '%s'\n", type);
            #endif

            vartype = get_varname_type (type);
            if (vartype == -1)
            {
                // error no valid vartype
                printf ("parse-line: call function: invalid variable: '%s' ! \n", type);
                return (1);
            }

            if (function_index >= 0 && function_args[function_index].strict == 1)
            {
                {
                    // EDIT neu2
                    S2 type_len;

                    type_len = strlen_safe ((const char *) type, MAXSTRLEN);
                    if (type_len > 1)
                    {
                        {
                        if (type[type_len - 1] == 'R')
                        {
                            printf ("parse_line: call function: variable ends with 'R'! It is a return value!\n");
                            return (1);
                        }
                        else
                        {   
                            S2 ending_len;
                            U1 endingstr[MAXSTRLEN] = "R";
                            S2 pos;

                            strcat ((char *) endingstr, (const char*) function_name_current);
                            ending_len = strlen_safe ((const char *) endingstr, MAXSTRLEN);

                            // EDITNEU
                            pos = searchstr (type, endingstr, 0, 0, TRUE);
                            if (pos == type_len - ending_len)
                            {
                                printf ("parse_line: call function: variable ends with 'R'! It is a return value!\n");
                                return (1);
                            }
                        }
                        }
                    }
                }
            }

            if (function_index >= 0 && args_ind <= function_args[function_index].args)
            {
                args_ind++;

                if (function_args[function_index].arg_type[args_ind] == VARTYPE_VARIABLE)
                {
                    // number of variables can change
                    // break
                    do_parse_vars = 0;
                    continue;
                }

                if (function_args[function_index].arg_type[args_ind] != VARTYPE_ANY)
                {
                    // not an any var type, check:
                    if (function_args[function_index].arg_type[args_ind] != vartype)
                    {
                        // error no valid vartype
                        printf ("parse-line: call function: type mismatch: '%s' ! \n", type);
                        return (1);
                    }
                }
            }

            #if DEBUG
            printf ("parse-line: set function type %i: %i\n", args_ind, vartype);
            #endif

            type_len = strlen_safe ((const char *) type, MAXSTRLEN);
            type_pos = type_pos + type_len + 1 ;

            if (i == line_len - 1)
            {
                do_parse_vars = 0;
            }

            if (function_index >= 0 && function_args[function_index].strict == 1)
            {
                // EDIT neu
                set_var_strict (type);
            }
        }

        if (function_index >= 0 && args_ind != function_args[function_index].args - 1)
        {
            // error args number doesn't match
            printf ("parse-line: call function: args num mismatch! got %i args, need %i args!\n", args_ind + 1, function_args[function_index].args);
            return (1);
        }

        // check if "stpop" is in the same line after the function call
        pos = searchstr (line, (U1 *) "stpop)", 0, 0, TRUE);
        if (pos != -1)
        {
            stpop_pos = pos;

            #if DEBUG
            printf ("found 'stpop)' after function call:\n,'%s'\n", line);
            #endif

            if (strcmp ((const char *) name, "") == 0)
            {
                printf ("parse_line: set return values not after function call: '%s'!\n", name);
                return (1);
            }

            // EDIT FIX
            return_args_i = get_return_index (name);
            if (return_args_i == -1)
            {
                if (func_strict == 1)
                {
                    printf ("parse_line: return variables, set return function not set: '%s'!\n", name);
                    return (1);
                }
                else
                {
                    return (0);
                }
            }

            #if DEBUG
            printf ("parse_line: stpop pos: %i\n", pos);
            #endif

            ret_start = 0;
            // sane check for "(x y stpop)" argument
            for (i = 0; i < line_len; i++)
            {
                if (line[i] == '(') {
                    ret_start++;
                }
                if (ret_start == 2)
                {
                    break;
                }
            }
            if (ret_start == 0)
            {
                // error on stpop no start ( found!
                printf ("parse_line: stpop syntax error!\n");
                return (1);
            }

            #if DEBUG
            printf ("parse_line: ret_start: %i\n", ret_start);
            #endif

            do_parse_vars = 1;
            i++;
            args_ind = -1;

            while (do_parse_vars == 1)
            {
                get_type = 1;
                type_ind = 0;

                #if DEBUG
                printf ("parse_line: i (parse_start): %i\n", i);
                #endif

                while (get_type == 1)
                {
                    #if DEBUG
                    printf ("parse_line: i: %i, char: %c\n", i, line[i]);
                    #endif

                    if (line[i] != ' ' && line[i] != ')' && line[i] != ':')
                    {
                        if (type_ind >= MAXSTRLEN)
                        {
                            printf ("parse_line: set call overflow!\n");
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

                        if (function_index >= 0 && function_args[function_index].strict == 1)
                        {
                            if (type_ind == 1)
                            {
                                // variable name is single char. No 'R' end possible!
                                printf ("parse-line: variable '%s' is not return type variable!\n", type);
                                return (1);
                            }

                            // EDIT NEU
                            {
                                S2 ending_len;
                                U1 endingstr[MAXSTRLEN] = "R";
                                S2 pos;
                                S2 fpos;
                                S2 function_call = 0;

                                strcat ((char *) endingstr, (const char*) function_name_current);
                                ending_len = strlen_safe ((const char *) endingstr, MAXSTRLEN);

                                pos = searchstr (type, endingstr, 0, 0, TRUE);
                                // printf ("DEBUG: pos: %i, type_len: %i, ending_len: %i\n\n", pos, type_ind - 1, ending_len);
                                if (pos == (type_ind - 1) - ending_len)
                                {
                                    printf ("parse_line: call function: variable ends not with 'R': '%s'!\n", type);
                                    return (1);
                                }

                                // check if function call before stpop:
                                fpos = searchstr (line, (U1 *) "call", 0, 0, TRUE);
                                if (fpos >= 0)
                                {
                                     function_call = 1;
                                }

                                fpos = searchstr (line, (U1 *) "!", 0, 0, TRUE);
                                if (fpos >= 0)
                                {
                                     function_call = 1;
                                }

                                if (function_call == 0)
                                {
                                    printf ("parse_line: no function call in line before 'stpop' !\n");
                                    return (1);
                                }
                            }
                        }
                    }
                    if (i < stpop_pos - 1 )
                    {
                        i++;
                    }
                    else
                    {
                        get_type = 0;
                        do_parse_vars = 0;
                    }
                }

                // get vartype
                #if DEBUG
                printf ("parse-line: return function type: '%s'\n", type);
                #endif

                vartype = get_varname_type (type);
                if (vartype == -1)
                {
                    // error no valid vartype
                    printf ("parse-line: return function: invalid variable: '%s' ! \n", type);
                    return (1);
                }

                if (args_ind <= return_args[return_args_i].args)
                {
                    args_ind++;

                    if (return_args[return_args_i].arg_type[args_ind] == VARTYPE_VARIABLE)
                    {
                        // number of variables can change
                        // break
                        do_parse_vars = 0;
                        continue;
                    }

                    if (return_args[return_args_i].arg_type[args_ind] != VARTYPE_ANY)
                    {
                        if (return_args[return_args_i].arg_type[args_ind] != vartype)
                        {
                            // error no valid vartype
                            printf ("parse-line: return function: type mismatch: '%s' ! \n", type);
                            return (1);
                        }
                    }
                }

                #if DEBUG
                printf ("parse-line: set return function type %i: %i\n", args_ind, vartype);
                #endif

                if (i == line_len - 1)
                {
                    do_parse_vars = 0;
                }
            }

            #if DEBUG
            printf ("parse-line: args_ind: %i, return_args[return_args_ind].args: %i\n", args_ind, return_args[return_args_i].args);
            #endif

            if (args_ind != return_args[return_args_i].args - 1)
            {
                // error args number doesn't match
                printf ("parse-line: return function: args num mismatch! got %i args, need %i args!\n", args_ind + 1, return_args[return_args_i].args);
                return (1);
            }
        }
    }

    // check if return line
    //printf ("parse-line: line: '%s'\n", line);

    return_pos = searchstr (line, (U1 *) "(return)", 0, 0, TRUE);
    if (return_pos != -1)
    {
        if (if_inside == 0 && switch_inside == 0 && for_inside == 0 && do_inside == 0)
        {
            function_return = 1;
        }

        return_args_i = get_return_index (function_name_current);
        if (return_args_i == -1)
        {
            // no return variables
            return (0);
        }

        { // new scope
        S2 n = 0;
        S2 var_ind = return_args[return_args_i].args -1;
        S2 stpush_pos;
        S2 stpush_found = 0;
        do_parse_vars = 1;

        pos = searchstr (line, (U1 *) "(", 0, 0, TRUE);
        if (pos >= return_pos)
        {
            printf ("parse-line: error: no return variables found!\n");
            return (1);
        }

        stpush_pos = searchstr (line, (U1 *) "stpush)", 0, 0, TRUE);
        if (stpush_pos == -1)
        {
            stpush_pos = searchstr (line, (U1 *) "stpushi)", 0, 0, TRUE);
            if (stpush_pos == -1)
            {
                stpush_pos = searchstr (line, (U1 *) "stpushd)", 0, 0, TRUE);
                if (stpush_pos == -1)
                {
                    stpush_pos = searchstr (line, (U1 *) "stpushb)", 0, 0, TRUE);
                    if (stpush_pos != -1)
                    {
                        stpush_found = 1;
                    }
                }
                else
                {
                    stpush_found = 1;
                }
            }
            else
            {
                stpush_found = 1;
            }
        }
        else
        {
            stpush_found = 1;
        }

        if (stpush_found == 0)
        {
            printf ("parse-line: error: no return variables found!\n");
            return (1);
        }

        i = pos + 1;
        if (i >= stpush_pos)
        {
            printf ("parse-line: error: no return variables found!\n");
            return (1);
        }
        while (do_parse_vars)
        {
            //printf ("parse-line: char %i: %c\n", i, line[i]);

            if (i == stpush_pos)
            {
                break;
            }

            if (line[i] != ' ')
            {
                type[n] = line[i];
                n++;
            }
            else
            {
                if (var_ind >= 0)
                {
                    type[n] = '\0';

                    //printf ("parse-line: return type name '%s'\n", type);

                    vartype = get_varname_type (type);

                    //printf ("parse-line: return variable type: %i\n", vartype);
                    //printf ("parse-line: return variable defined type: %i\n", return_args[return_args_ind].arg_type[var_ind]);

                    //printf ("var_ind: %lli\n", var_ind);
                    //
                    //
                    if (return_args[return_args_i].arg_type[var_ind] == VARTYPE_VARIABLE)
                    {
                        // number of variables can change
                        // break
                        do_parse_vars = 0;
                        continue;
                    }

                    if (return_args[return_args_i].arg_type[var_ind] != VARTYPE_ANY)
                    {
                        // not an any type variable, check type:
                        if (vartype != return_args[return_args_i].arg_type[var_ind])
                        {
                            printf ("parse-line: error: return variable type mismatch!\n");
                            return (1);
                        }
                    }
                }

                var_ind--;

                if (var_ind < -1)
                {
                    printf ("parse-line: error: to many return variables!\n");
                    return (1);
                }
                n = 0;
            }
            i++;
        }
        }
    }

    // pointer checks and tracking are done in safety_check_line() (uninitialized
    // pointers, pointer names, pointer target checks and check_pointer in strict mode)
    if (function_check_stpop == 1)
    {
        func_stpop_pos = searchstr (line, (U1 *) "stpop)", 0, 0, TRUE);
        if (func_stpop_pos != -1)
        {
            function_index = get_function_index (function_name_current);
            if (function_index == -1)
            {
                printf ("parse-line: error: no function args set!\n");
                return (1);
            }

            i = searchstr (line, (U1 *) "(", 0, 0, TRUE);
            if (i == -1)
            {
                // parse error
                printf ("parse-line: error: no '(' in stpop!\n");
                return (1);
            }

            if (i >= func_stpop_pos)
            {
                printf ("parse-line: error: '(' behind stpop!\n");
                return (1);
            }
            i++;

            // reset flag:
            function_check_stpop = 0;

            { // new scope
                S2 n = 0;
                S2 var_ind = function_args[function_index].args -1 ;
                do_parse_vars = 1;

                while (do_parse_vars)
                {
                    if (i == func_stpop_pos)
                    {
                        break;
                    }

                    if (line[i] != ' ')
                    {
                        type[n] = line[i];
                        n++;
                    }
                    else
                    {
                        type[n] = '\0';

                        vartype = get_varname_type (type);

                        // printf ("parse-line: return variable type: %i\n", vartype);
                        // printf ("parse-line: return variable defined type: %i\n", return_args[return_args_ind].arg_type[var_ind]);

                        if (var_ind >= 0)
                        {
                            if (function_args[function_index].arg_type[var_ind] == VARTYPE_VARIABLE)
                            {
                                // number of variables can change
                                // break
                                do_parse_vars = 0;
                                continue;
                            }

                            if (vartype != function_args[function_index].arg_type[var_ind])
                            {
                                printf ("parse-line: error: function arg variable type mismatch!\n");
                                return (1);
                            }

                            if (var_ind > 0)
                            {
                                var_ind--;
                            }
                            else
                            {
                                break;
                            }
                        }
                        else
                        {
                            printf ("parse-line: error: to many function arg variables in stpop!\n");
                            return (1);
                        }
                        n = 0;
                    }
                    i++;
                }
            } // scope close
        }
    }

    // check if strict variable is reused after function call.
    if (check_line_strict (line) != 0)
    {
        return (1);
    }

    return (0);
}

// CBMC-style annotations in comments:
//   // (assert (x 0 >))   the condition must hold at this program point
//   // (assume (x 0 >))   the condition is assumed to hold from here on
//   // (assert-strict)     also report assertions which cannot be proven
//   // (assert-lenient)    only report assertions which are provably false
// Returns 1 if the comment was consumed as an annotation.
U1 handle_annotation_comment (U1 *line)
{
    U1 *p;
    U1 text[MAXSTRLEN + 1];
    S2 nt;
    S2 rc;
    S2 truth;
    U1 is_assert = 0;
    U1 is_assume = 0;
    struct flow_state saved;

    p = (U1 *) strstr ((char *) line, "//");
    if (p == NULL)
    {
        return (0);
    }
    p += 2;
    while (*p == ' ' || *p == '\t')
    {
        p++;
    }

    if (strncmp ((char *) p, "(assert-strict)", 15) == 0)
    {
        flow_assert_strict = 1;
        return (1);
    }
    if (strncmp ((char *) p, "(assert-lenient)", 16) == 0)
    {
        flow_assert_strict = 0;
        return (1);
    }
    if (strncmp ((char *) p, "(logic-strict)", 14) == 0)
    {
        flow_logic_strict = 1;
        return (1);
    }
    if (strncmp ((char *) p, "(logic-lenient)", 15) == 0)
    {
        flow_logic_strict = 0;
        return (1);
    }
    if (strncmp ((char *) p, "(assert ", 8) == 0)
    {
        is_assert = 1;
    }
    else if (strncmp ((char *) p, "(assume ", 8) == 0)
    {
        is_assume = 1;
    }
    else
    {
        return (0);
    }

    strncpy ((char *) text, (const char *) p, MAXSTRLEN);
    text[MAXSTRLEN] = '\0';

    nt = tokenize_line (text, annotation_tokens, MAXTOKENS);
    if (nt == 0)
    {
        return (1);
    }

    if (is_assert == 1)
    {
        if (capture_flow_state (&saved) != 0)
        {
            return (1);
        }
        flow_cond_truth = -1;
        rc = refine_condition (nt, annotation_tokens, 1);
        truth = flow_cond_truth;
        restore_flow_state (&saved);
        free_flow_state (&saved);

        flow_assert_count++;
        if (truth == 0)
        {
            linter_warnings++;
            warn_line_mark (linenum);
            printf ("linter error: logical bug: assertion is always false at this point!\n");
            printf (" > %s\n", line);
        }
        else if (truth != 1 && (rc == 2 || flow_assert_strict == 1))
        {
            linter_warnings++;
            warn_line_mark (linenum);
            printf ("linter error: logical bug: assertion could not be proven (assert-strict)!\n");
            printf (" > %s\n", line);
        }
        return (1);
    }

    if (is_assume == 1)
    {
        rc = refine_condition (nt, annotation_tokens, 1);
        if (rc == 2)
        {
            linter_warnings++;
            warn_line_mark (linenum);
            printf ("linter error: logical bug: assumption can never hold at this point!\n");
            printf (" > %s\n", line);
        }
        return (1);
    }

    return (0);
}

// ---------------------------------------------------------------------------
// interprocedural data-flow analysis: collection pre-pass and call sandbox
// ---------------------------------------------------------------------------

S8 ip_find_var_in (struct vars *arr, S8 ind, U1 *name)
{
    S8 i ALIGN;

    for (i = 0; i <= ind; i++)
    {
        if (strcmp ((const char *) arr[i].name, (const char *) name) == 0)
        {
            return (i);
        }
    }
    return (-1);
}

// extract the raw function name of a call from the line. The tokenizer
// splits names like "alloc->circle", so the name is read from the raw text
// after the call colon. Labels ("label:") and ":=" are skipped.
S2 ip_raw_callee_name (U1 *line, U1 *name)
{
    S2 i = 0;
    S2 start;
    S2 n;

    while (line[i] != '\0')
    {
        if (line[i] == '"')
        {
            i++;
            while (line[i] != '\0' && line[i] != '"')
            {
                i++;
            }
            if (line[i] == '"')
            {
                i++;
            }
            continue;
        }
        if (line[i] == ':')
        {
            S2 p;

            if (line[i + 1] == '=')
            {
                i += 2;
                continue;
            }
            p = i - 1;
            if (p >= 0 &&
                (is_ident_char (line[p]) == 1 || isdigit ((unsigned char) line[p])))
            {
                // label, not a call
                i++;
                continue;
            }
            start = i + 1;
            while (line[start] == ' ' || line[start] == '\t')
            {
                start++;
            }
            n = start;
            while (line[n] != '\0' && line[n] != ' ' && line[n] != '\t' &&
                   line[n] != '!' && line[n] != ')' && line[n] != '(' &&
                   line[n] != '\n' && line[n] != '\r')
            {
                n++;
            }
            if (n == start)
            {
                return (0);
            }
            if (n - start > MAXSTRLEN)
            {
                n = start + MAXSTRLEN;
            }
            memcpy (name, &line[start], (size_t) (n - start));
            name[n - start] = '\0';
            return (1);
        }
        i++;
    }
    return (0);
}

// extract the variable name from a "(set TYPE SIZE NAME ...)" declaration.
// The name is parsed from the raw line because names may contain characters
// which the tokenizer treats as operators.
S2 ip_decl_name (U1 *line, U1 *name)
{
    S2 p = 0;
    S2 start;

    while (line[p] == ' ' || line[p] == '\t')
    {
        p++;
    }
    if (strncmp ((const char *) &line[p], "(set", 4) != 0)
    {
        return (0);
    }
    if (line[p + 4] != ' ' && line[p + 4] != '\t')
    {
        return (0);
    }
    p += 5;

    // skip type
    while (line[p] != '\0' && line[p] != ' ' && line[p] != '\t' &&
           line[p] != ')' && line[p] != '\n' && line[p] != '\r')
    {
        p++;
    }
    while (line[p] == ' ' || line[p] == '\t')
    {
        p++;
    }
    // skip size
    while (line[p] != '\0' && line[p] != ' ' && line[p] != '\t' &&
           line[p] != ')' && line[p] != '\n' && line[p] != '\r')
    {
        p++;
    }
    while (line[p] == ' ' || line[p] == '\t')
    {
        p++;
    }

    start = p;
    while (line[p] != '\0' && line[p] != ' ' && line[p] != '\t' &&
           line[p] != ')' && line[p] != '\n' && line[p] != '\r')
    {
        p++;
    }
    if (p == start)
    {
        return (0);
    }
    if (p - start > MAXSTRLEN)
    {
        p = start + MAXSTRLEN;
    }
    memcpy (name, &line[start], (size_t) (p - start));
    name[p - start] = '\0';
    return (1);
}

// parse a raw function definition line "(name func)". Function names may
// contain operator characters ("alloc->circle"), so the tokenizer is not used.
S2 ip_extract_func_start (U1 *line, U1 *name)
{
    S2 p = 0;
    S2 q;
    S2 start;
    S2 end;
    S2 r;
    S2 len;

    while (line[p] == ' ' || line[p] == '\t')
    {
        p++;
    }
    if (line[p] != '(')
    {
        return (0);
    }
    if (strncmp ((const char *) &line[p], "(set", 4) == 0 &&
        (line[p + 4] == ' ' || line[p + 4] == '\t' || line[p + 4] == '\0'))
    {
        return (0);
    }

    q = searchstr (line, (U1 *) " func)", p, 0, TRUE);
    if (q < 0)
    {
        return (0);
    }
    r = q + 6;
    while (line[r] == ' ' || line[r] == '\t' ||
           line[r] == '\n' || line[r] == '\r')
    {
        r++;
    }
    if (line[r] != '\0' && !(line[r] == '/' && line[r + 1] == '/'))
    {
        return (0);
    }

    start = p + 1;
    end = q;
    if (end <= start)
    {
        return (0);
    }
    len = end - start;
    if (len > MAXSTRLEN)
    {
        len = MAXSTRLEN;
    }
    memcpy (name, &line[start], (size_t) len);
    name[len] = '\0';
    return (1);
}

S2 ip_is_funcend (U1 *line)
{
    S2 p = 0;

    while (line[p] == ' ' || line[p] == '\t')
    {
        p++;
    }
    if (strncmp ((const char *) &line[p], "(funcend)", 9) == 0)
    {
        return (1);
    }
    return (0);
}

S2 ip_new_func (U1 *name)
{
    S2 fi;

    fi = ip_find_func (name);
    if (fi >= 0)
    {
        return (fi);
    }
    if (ip_funcs_ind >= IP_MAX_FUNCS - 1)
    {
        return (-1);
    }
    ip_funcs_ind++;
    memset (&ip_funcs[ip_funcs_ind], 0, sizeof (struct ip_func));
    strcpy ((char *) ip_funcs[ip_funcs_ind].name, (const char *) name);
    ip_funcs[ip_funcs_ind].args = -1;
    ip_funcs[ip_funcs_ind].returns = -1;
    return (ip_funcs_ind);
}

S2 ip_find_func (U1 *name)
{
    S2 i;

    if (ip_funcs == NULL)
    {
        return (-1);
    }
    for (i = 0; i <= ip_funcs_ind; i++)
    {
        if (ip_funcs[i].name[0] != '\0' &&
            strcmp ((const char *) ip_funcs[i].name, (const char *) name) == 0)
        {
            return (i);
        }
    }
    return (-1);
}

// parse "// (func args [S] name type ...)" and "// (return args name type ...)"
S2 ip_parse_annotation_raw (U1 *line, const char *prefix, S2 is_func)
{
    S2 p;
    S2 prefix_len;
    S2 name_start;
    S2 count = 0;
    S2 fi;
    U1 name[MAXSTRLEN + 1];

    p = searchstr (line, (U1 *) prefix, 0, 0, TRUE);
    if (p < 0)
    {
        return (0);
    }
    prefix_len = (S2) strlen (prefix);
    p += prefix_len;

    while (line[p] == ' ' || line[p] == '\t')
    {
        p++;
    }
    if (is_func == 1 && line[p] == 'S' &&
        (line[p + 1] == ' ' || line[p + 1] == '\t'))
    {
        p++;
        while (line[p] == ' ' || line[p] == '\t')
        {
            p++;
        }
    }

    name_start = p;
    while (line[p] != '\0' && line[p] != ' ' && line[p] != '\t' &&
           line[p] != ')' && line[p] != '\n' && line[p] != '\r')
    {
        p++;
    }
    if (p == name_start)
    {
        return (0);
    }
    if (p - name_start > MAXSTRLEN)
    {
        p = name_start + MAXSTRLEN;
    }
    memcpy (name, &line[name_start], (size_t) (p - name_start));
    name[p - name_start] = '\0';

    while (line[p] == ' ' || line[p] == '\t')
    {
        p++;
    }
    while (line[p] != '\0' && line[p] != ')' &&
           line[p] != '\n' && line[p] != '\r')
    {
        while (line[p] == ' ' || line[p] == '\t')
        {
            p++;
        }
        if (line[p] == ')' || line[p] == '\0' ||
            line[p] == '\n' || line[p] == '\r')
        {
            break;
        }
        while (line[p] != '\0' && line[p] != ' ' && line[p] != '\t' &&
               line[p] != ')' && line[p] != '\n' && line[p] != '\r')
        {
            p++;
        }
        count++;
    }

    // "// (func args stpop)" is the stack check marker, not a function
    if (is_func == 1 && strcmp ((const char *) name, "stpop") == 0 && count == 0)
    {
        return (0);
    }

    fi = ip_new_func (name);
    if (fi < 0)
    {
        return (0);
    }
    if (is_func == 1)
    {
        ip_funcs[fi].args = count;
    }
    else
    {
        ip_funcs[fi].returns = count;
    }
    return (1);
}

void ip_free_funcs (void)
{
    S2 i;
    S2 j;

    if (ip_funcs == NULL)
    {
        return;
    }
    for (i = 0; i <= ip_funcs_ind; i++)
    {
        for (j = 0; j < ip_funcs[i].line_count; j++)
        {
            free (ip_funcs[i].lines[j]);
        }
        free (ip_funcs[i].lines);
    }
    free (ip_funcs);
    ip_funcs = NULL;
    ip_funcs_ind = -1;
    ip_total_lines = 0;
}

void ip_cleanup (void)
{
    ip_free_funcs ();
}

S2 ip_collect_functions (FILE *finptr, const char *fname)
{
    U1 rbuf[MAXSTRLEN + 1];
    U1 buf[MAXSTRLEN + 1];
    U1 cur[MAXSTRLEN + 1];
    U1 name[MAXSTRLEN + 1];
    char *read;
    S2 cur_i = -1;
    U1 in_body = 0;
    S2 cpos;
    S2 i;

    (void) fname;

    ip_funcs = (struct ip_func *) calloc (IP_MAX_FUNCS, sizeof (struct ip_func));
    if (ip_funcs == NULL)
    {
        ip_enabled = 0;
        return (1);
    }
    ip_funcs_ind = -1;
    ip_total_lines = 0;
    cur[0] = '\0';

    rewind (finptr);
    while (1)
    {
        read = fgets_uni ((char *) rbuf, MAXLINELEN, finptr);
        if (read == NULL)
        {
            break;
        }
        strcpy ((char *) buf, (const char *) rbuf);
        convtabs (buf);

        // annotations
        if (searchstr (buf, (U1 *) "// (func args ", 0, 0, TRUE) >= 0)
        {
            (void) ip_parse_annotation_raw (buf, "// (func args ", 1);
        }
        if (searchstr (buf, (U1 *) "// (return args ", 0, 0, TRUE) >= 0)
        {
            (void) ip_parse_annotation_raw (buf, "// (return args ", 0);
        }

        cpos = 0;
        while (buf[cpos] == ' ' || buf[cpos] == '\t')
        {
            cpos++;
        }
        if (buf[cpos] == '\0' || buf[cpos] == '\n' || buf[cpos] == '\r')
        {
            continue;
        }
        if (buf[cpos] == '#')
        {
            continue;
        }
        if (buf[cpos] == '/' && buf[cpos + 1] == '/')
        {
            continue;
        }
        if (strncmp ((const char *) &buf[cpos], "FILE:", 5) == 0 ||
            strncmp ((const char *) &buf[cpos], "FILE END", 8) == 0)
        {
            continue;
        }

        if (ip_is_funcend (buf) == 1)
        {
            in_body = 0;
            cur_i = -1;
            cur[0] = '\0';
            continue;
        }

        if (ip_extract_func_start (buf, name) == 1)
        {
            strcpy ((char *) cur, (const char *) name);
            cur_i = ip_new_func (cur);
            if (cur_i >= 0 && ip_funcs[cur_i].has_body == 0)
            {
                ip_funcs[cur_i].has_body = 1;
                in_body = 1;
            }
            else
            {
                in_body = 0;
            }
            continue;
        }

        if (in_body == 1 && cur_i >= 0)
        {
            U1 **new_lines;
            S2 new_cap;

            if (ip_total_lines >= IP_MAX_FUNCS_LINES)
            {
                ip_enabled = 0;
                break;
            }
            if (ip_funcs[cur_i].line_count >= ip_funcs[cur_i].line_cap)
            {
                new_cap = ip_funcs[cur_i].line_cap == 0 ? 64 :
                          ip_funcs[cur_i].line_cap * 2;
                new_lines = (U1 **) realloc (ip_funcs[cur_i].lines,
                                             (size_t) new_cap * sizeof (U1 *));
                if (new_lines == NULL)
                {
                    ip_enabled = 0;
                    break;
                }
                ip_funcs[cur_i].lines = new_lines;
                ip_funcs[cur_i].line_cap = new_cap;
            }
            i = ip_funcs[cur_i].line_count;
            ip_funcs[cur_i].lines[i] = (U1 *) strdup ((const char *) buf);
            if (ip_funcs[cur_i].lines[i] == NULL)
            {
                ip_enabled = 0;
                break;
            }
            ip_funcs[cur_i].line_count++;
            ip_total_lines++;
        }
    }

    rewind (finptr);
    clearerr (finptr);

    if (ip_enabled == 0)
    {
        ip_free_funcs ();
    }
    return (0);
}

// merge one return path into the summary of a function
void ip_merge_return (struct int_range *summary, U1 *seen, S2 *count,
                      struct int_range *last, S2 last_count)
{
    S2 old = *count;
    S2 n;
    S2 j;

    n = old > last_count ? old : last_count;
    if (n > IP_MAX_RETURNS)
    {
        n = IP_MAX_RETURNS;
    }
    for (j = old; j < n; j++)
    {
        summary[j] = range_unknown ();
        seen[j] = 0;
    }
    for (j = 0; j < n; j++)
    {
        if (j < last_count)
        {
            if (seen[j] == 1)
            {
                struct int_range a = summary[j];
                struct int_range b = last[j];
                struct int_range r = range_unknown ();

                if (a.known == 1 && b.known == 1)
                {
                    r.known = 1;
                    r.lo = a.lo < b.lo ? a.lo : b.lo;
                    r.hi = a.hi > b.hi ? a.hi : b.hi;
                }
                summary[j] = r;
            }
            else
            {
                summary[j] = last[j];
                seen[j] = 1;
            }
        }
        else
        {
            // this return path does not provide a value for this position
            summary[j] = range_unknown ();
            seen[j] = 1;
        }
    }
    *count = n;
}

// only forward data-flow findings of the callee. Structural findings (parse
// errors, duplicate declarations, ...) are already reported by the main pass.
U1 ip_keep_message (const char *msg)
{
    static const char *skip[] = {
        "declared more than once", "unclosed", "without an open",
        "not closed", "no exit call", "function args", "return args",
        "type mismatch", "not defined", "unknown variable",
        "stack underflow", "logical bug:", "unreachable code",
        "duplicate switch", "self-assignment", "assignment to constant",
        "loop condition", "condition is always", "condition can never",
        "condition is never", "used before it is initialized", NULL
    };
    static const char *keep[] = {
        "division by", "modulo by", "array index", "array bounds",
        "string index", "string buffer", "heap access", "out of bounds",
        "used before it is initialized", "pointer", "null", "freed",
        "allocation", "memory leak", "overflow", "shift amount",
        "value range", "does not fit", "too small", "capacity",
        "negative size", "invalid free", "double free", NULL
    };
    S2 i;

    for (i = 0; skip[i] != NULL; i++)
    {
        if (strstr (msg, skip[i]) != NULL)
        {
            return (0);
        }
    }
    for (i = 0; keep[i] != NULL; i++)
    {
        if (strstr (msg, keep[i]) != NULL)
        {
            return (1);
        }
    }
    return (0);
}

// print the findings of one sandbox. At the top level the message is
// attributed to the call site. In a nested sandbox the text is written to the
// capture of the outer sandbox, so it is reported once at the real call site.
void ip_report_capture (char *capture, U1 *callee, U1 *caller, U1 nested)
{
    char *p;
    S8 emitted = 0;
    S8 seen[256];
    S2 i;
    U1 caller_name[MAXSTRLEN + 1];

    if (capture == NULL)
    {
        return;
    }
    for (i = 0; i < 256; i++)
    {
        seen[i] = 0;
    }
    if (caller[0] == '\0')
    {
        strcpy ((char *) caller_name, "unknown");
    }
    else
    {
        strcpy ((char *) caller_name, (const char *) caller);
    }

    p = capture;
    while (p != NULL && *p != '\0')
    {
        char *nl = strchr (p, '\n');
        char *after;

        if (nl != NULL)
        {
            *nl = '\0';
            after = nl + 1;
        }
        else
        {
            after = p + strlen (p);
        }

        if (strncmp (p, "linter error: ", 14) == 0)
        {
            char *msg = p + 14;
            char *src = NULL;
            U1 keep;
            S8 hash = 0;
            S2 slot;

            if (*after == ' ' && *(after + 1) == '>')
            {
                char *snl = strchr (after, '\n');

                src = after;
                if (snl != NULL)
                {
                    *snl = '\0';
                    after = snl + 1;
                }
                else
                {
                    after = after + strlen (after);
                }
            }

            if (strncmp (msg, "interprocedural: ", 16) == 0)
            {
                keep = 1;
            }
            else
            {
                keep = ip_keep_message (msg);
            }

            if (keep == 1)
            {
                char *q;

                for (q = msg; *q != '\0'; q++)
                {
                    hash = hash * 31 + (unsigned char) *q;
                }
                if (src != NULL)
                {
                    for (q = src; *q != '\0'; q++)
                    {
                        hash = hash * 31 + (unsigned char) *q;
                    }
                }
                slot = (S2) (hash & 255);
                if (seen[slot] != hash)
                {
                    seen[slot] = hash;
                    if (strncmp (msg, "interprocedural: ", 16) == 0)
                    {
                        printf ("linter error: %s\n", msg);
                    }
                    else if (nested == 1)
                    {
                        printf ("linter error: interprocedural: in function '%s': %s\n",
                                callee, msg);
                    }
                    else
                    {
                        printf ("linter error: interprocedural: in function '%s' called from '%s': %s\n",
                                callee, caller_name, msg);
                    }
                    if (src != NULL)
                    {
                        printf ("%s\n", src);
                    }
                    emitted++;
                }
            }
        }
        p = after;
    }
    if (nested == 0)
    {
        linter_warnings += emitted;
    }
}

// execute the body of "callee" with the actual argument ranges. Returns 1 if
// the callee was analysed. The caller's state is fully restored; only the
// computed effects on caller variables and the return value ranges survive.
S2 ip_analyze_call (U1 *callee, S2 nargs, U1 *call_args[], U1 *line)
{
    struct ip_func *f;
    S2 fi;
    S2 i;
    S2 j;
    S2 nargs_used = 0;
    S2 formal;
    struct int_range argr[IP_MAX_CALL_ARGS];
    struct int_range summary[IP_MAX_RETURNS];
    U1 summary_seen[IP_MAX_RETURNS];
    S2 summary_count;
    U1 callee_name[MAXSTRLEN + 1];

    struct vars *saved_vars;
    S8 saved_vars_ind;
    U1 saved_funcname[MAXSTRLEN + 1];
    U1 s_function_check_stpop;
    U1 s_function_inside;
    U1 s_function_return;
    U1 s_if_inside;
    U1 s_switch_inside;
    U1 s_for_inside;
    U1 s_do_inside;
    U1 s_pointer_strict;
    U1 s_pointer_check_active;
    U1 s_pointer_check_name[MAXSTRLEN + 1];
    S8 s_pointer_check_line;
    S8 s_linenum;
    U1 s_file_inside;
    S8 s_file_index;
    U1 s_found_exit;
    U1 s_func_strict;
    S8 s_warnings;
    S2 s_flow_frame_ind;
    struct flow_frame *saved_frames = NULL;
    struct label_state *saved_label_states;
    S2 saved_label_states_ind;
    U1 s_flow_reachable;
    S8 s_flow_stack_depth;
    U1 s_flow_stack_known;
    U1 s_flow_loop_widen;
    S2 s_flow_cond_truth;
    U1 s_flow_cond_stable;
    U1 s_flow_logic_strict;
    U1 s_flow_unreachable_warned;
    U1 s_flow_dead_branch;
    U1 s_flow_line_unreachable;
    U1 s_flow_case_dead;
    U1 s_flow_assert_strict;
    U1 s_flow_missing_return;
    S8 s_flow_assert_count;
    U1 s_pending_alloc;
    U1 s_pending_alloc_known;
    S8 s_pending_alloc_size;
    S8 s_pending_alloc_elem_size;
    U1 s_pending_result;
    S8 s_pending_result_value;
    S8 s_pending_result_min;
    S8 s_pending_result_max;
    struct int_range s_vstack[IP_VSTACK_MAX];
    S2 s_vstack_ind;
    S2 s_vstack_floor;
    struct int_range s_last_return[IP_MAX_RETURNS];
    S2 s_last_return_count;
    U1 s_last_return_seen;
    U1 *s_written;
    U1 *s_safety_tokens = NULL;
    struct vars *sandbox = NULL;
    U1 *written = NULL;
    U1 *removed = NULL;
    S2 removed_count = 0;
    FILE *capfile = NULL;
    int saved_stdout;
    char *capture = NULL;
    U1 nested;

    (void) line;

    if (ip_enabled == 0 || ip_budget <= 0)
    {
        return (0);
    }
    strcpy ((char *) callee_name, (const char *) callee);
    if (ip_sandbox_depth >= IP_MAX_DEPTH)
    {
        return (0);
    }
    fi = ip_find_func (callee_name);
    if (fi < 0)
    {
        return (0);
    }
    f = &ip_funcs[fi];
    if (f->has_body == 0 || f->line_count == 0)
    {
        return (0);
    }
    for (i = 0; i < ip_sandbox_depth; i++)
    {
        if (strcmp ((const char *) ip_active_funcs[i], (const char *) callee_name) == 0)
        {
            // recursion: leave the return values unknown
            return (0);
        }
    }

    for (i = 0; i < nargs && i < IP_MAX_CALL_ARGS; i++)
    {
        U1 *a = call_args[i];

        if (a == NULL)
        {
            argr[i] = range_unknown ();
        }
        else if (is_number_token (a) == 1)
        {
            argr[i] = range_exact ((S8) strtoll ((const char *) a, NULL, 10));
        }
        else if (a[0] == '"')
        {
            argr[i] = range_unknown ();
        }
        else
        {
            argr[i] = range_of_var (a);
        }
        nargs_used++;
    }

    sandbox = (struct vars *) malloc ((size_t) MAXVARS * sizeof (struct vars));
    written = (U1 *) calloc ((size_t) MAXVARS, 1);
    removed = (U1 *) calloc ((size_t) IP_MAX_DECLS, (size_t) MAXSTRLEN + 1);
    s_safety_tokens = (U1 *) malloc ((size_t) MAXTOKENS * ((size_t) MAXSTRLEN + 1));
    if (sandbox == NULL || written == NULL || removed == NULL ||
        s_safety_tokens == NULL)
    {
        free (sandbox);
        free (written);
        free (removed);
        free (s_safety_tokens);
        return (0);
    }

    s_flow_frame_ind = flow_frame_ind;
    if (s_flow_frame_ind >= 0)
    {
        saved_frames = (struct flow_frame *) malloc ((size_t) (s_flow_frame_ind + 1) *
                                                     sizeof (struct flow_frame));
        if (saved_frames == NULL)
        {
            free (sandbox);
            free (written);
            free (removed);
            free (s_safety_tokens);
            return (0);
        }
        memcpy (saved_frames, flow_frames,
                (size_t) (s_flow_frame_ind + 1) * sizeof (struct flow_frame));
    }

    // capture stdout before any state is modified
    fflush (stdout);
    saved_stdout = dup (1);
    if (saved_stdout < 0)
    {
        free (saved_frames);
        free (sandbox);
        free (written);
        free (removed);
        free (s_safety_tokens);
        return (0);
    }
    capfile = tmpfile ();
    if (capfile == NULL)
    {
        close (saved_stdout);
        free (saved_frames);
        free (sandbox);
        free (written);
        free (removed);
        free (s_safety_tokens);
        return (0);
    }

    // save the caller state
    saved_vars = vars;
    saved_vars_ind = vars_ind;
    strcpy ((char *) saved_funcname, (const char *) function_name_current);
    s_function_check_stpop = function_check_stpop;
    s_function_inside = function_inside;
    s_function_return = function_return;
    s_if_inside = if_inside;
    s_switch_inside = switch_inside;
    s_for_inside = for_inside;
    s_do_inside = do_inside;
    s_pointer_strict = pointer_strict;
    s_pointer_check_active = pointer_check_active;
    strcpy ((char *) s_pointer_check_name, (const char *) pointer_check_name);
    s_pointer_check_line = pointer_check_line;
    s_linenum = linenum;
    s_file_inside = file_inside;
    s_file_index = file_index;
    s_found_exit = found_exit;
    s_func_strict = func_strict;
    s_warnings = linter_warnings;
    saved_label_states = label_states;
    saved_label_states_ind = label_states_ind;

    s_flow_reachable = flow_reachable;
    s_flow_stack_depth = flow_stack_depth;
    s_flow_stack_known = flow_stack_known;
    s_flow_loop_widen = flow_loop_widen;
    s_flow_cond_truth = flow_cond_truth;
    s_flow_cond_stable = flow_cond_stable;
    s_flow_logic_strict = flow_logic_strict;
    s_flow_unreachable_warned = flow_unreachable_warned;
    s_flow_dead_branch = flow_dead_branch;
    s_flow_line_unreachable = flow_line_unreachable;
    s_flow_case_dead = flow_case_dead;
    s_flow_assert_strict = flow_assert_strict;
    s_flow_missing_return = flow_missing_return;
    s_flow_assert_count = flow_assert_count;

    s_pending_alloc = pending_alloc;
    s_pending_alloc_known = pending_alloc_known;
    s_pending_alloc_size = pending_alloc_size;
    s_pending_alloc_elem_size = pending_alloc_elem_size;
    s_pending_result = pending_result;
    s_pending_result_value = pending_result_value;
    s_pending_result_min = pending_result_min;
    s_pending_result_max = pending_result_max;

    memcpy (s_vstack, ip_vstack, sizeof (ip_vstack));
    s_vstack_ind = ip_vstack_ind;
    s_vstack_floor = ip_vstack_floor;
    memcpy (s_last_return, ip_last_return, sizeof (ip_last_return));
    s_last_return_count = ip_last_return_count;
    s_last_return_seen = ip_last_return_seen;
    s_written = ip_written;
    memcpy (s_safety_tokens, safety_tokens,
            (size_t) MAXTOKENS * ((size_t) MAXSTRLEN + 1));

    // detach the caller control flow
    flow_frame_ind = -1;
    label_states = NULL;
    label_states_ind = -1;

    // sandbox variable table
    for (i = 0; i <= saved_vars_ind; i++)
    {
        sandbox[i] = saved_vars[i];
    }
    vars = sandbox;
    vars_ind = saved_vars_ind;
    ip_written = written;

    // remove the callee's own declarations from the copy so parse_line
    // declares them fresh with their initial values
    for (i = 0; i < f->line_count; i++)
    {
        U1 dname[MAXSTRLEN + 1];
        U1 duplicate = 0;

        if (ip_decl_name (f->lines[i], dname) == 1)
        {
            for (j = 0; j < removed_count; j++)
            {
                if (strcmp ((const char *) (removed + (size_t) j * ((size_t) MAXSTRLEN + 1)),
                            (const char *) dname) == 0)
                {
                    duplicate = 1;
                    break;
                }
            }
            if (duplicate == 0 && removed_count < IP_MAX_DECLS)
            {
                strcpy ((char *) (removed + (size_t) removed_count * ((size_t) MAXSTRLEN + 1)),
                        (const char *) dname);
                removed_count++;
            }
        }
    }
    for (i = 0; i < removed_count; i++)
    {
        U1 *rname = removed + (size_t) i * ((size_t) MAXSTRLEN + 1);
        S8 ri;

        while ((ri = ip_find_var_in (vars, vars_ind, rname)) >= 0)
        {
            for (j = ri; j < vars_ind; j++)
            {
                vars[j] = vars[j + 1];
            }
            vars_ind--;
        }
    }

    // sandbox function environment
    strcpy ((char *) function_name_current, (const char *) callee_name);
    function_inside = 1;
    function_return = 0;
    function_check_stpop = 0;
    if_inside = 0;
    switch_inside = 0;
    for_inside = 0;
    do_inside = 0;
    pointer_check_active = 0;
    pointer_check_name[0] = '\0';
    pointer_check_line = -1;
    linenum = 1;
    found_exit = 0;
    linter_warnings = 0;

    reset_flow_analysis ();
    flow_cond_truth = -1;
    flow_cond_stable = 0;
    flow_logic_strict = 0;
    flow_unreachable_warned = 0;
    flow_dead_branch = 0;
    flow_line_unreachable = 0;
    flow_case_dead = 0;
    flow_assert_count = 0;
    flow_missing_return = 0;

    // seed the value stack with the actual arguments
    ip_vstack_ind = s_vstack_ind;
    ip_vstack_floor = s_vstack_ind;
    formal = (f->args >= 0) ? f->args : nargs_used;
    if (formal > IP_VSTACK_MAX - 1 - ip_vstack_ind)
    {
        formal = IP_VSTACK_MAX - 1 - ip_vstack_ind;
    }
    if (formal < 0)
    {
        formal = 0;
    }
    for (i = 0; i < formal; i++)
    {
        ip_push_range (i < nargs_used ? argr[i] : range_unknown ());
    }
    // the callee's formal arguments are on the stack
    flow_stack_depth = formal;
    flow_stack_known = 1;
    ip_last_return_seen = 0;
    ip_last_return_count = 0;

    summary_count = (f->returns >= 0) ? f->returns : 0;
    if (summary_count > IP_MAX_RETURNS)
    {
        summary_count = IP_MAX_RETURNS;
    }
    for (i = 0; i < IP_MAX_RETURNS; i++)
    {
        summary[i] = range_unknown ();
        summary_seen[i] = 0;
    }

    strcpy ((char *) ip_active_funcs[ip_sandbox_depth], (const char *) callee_name);
    ip_sandbox_depth++;
    ip_sandbox_active = 1;

    fflush (stdout);
    if (dup2 (fileno (capfile), 1) < 0)
    {
        // should not happen; run without capture
    }

    for (i = 0; i < f->line_count; i++)
    {
        if (ip_budget <= 0)
        {
            break;
        }
        ip_budget--;
        ip_last_return_seen = 0;
        ip_last_return_count = 0;
        (void) parse_line (f->lines[i]);
        if (ip_last_return_seen == 1)
        {
            ip_merge_return (summary, summary_seen, &summary_count,
                             ip_last_return, ip_last_return_count);
        }
    }

    fflush (stdout);
    if (saved_stdout >= 0)
    {
        dup2 (saved_stdout, 1);
        close (saved_stdout);
    }

    // read the captured diagnostics
    if (capfile != NULL)
    {
        long sz;

        fseek (capfile, 0, SEEK_END);
        sz = ftell (capfile);
        if (sz > 0)
        {
            capture = (char *) malloc ((size_t) sz + 1);
            if (capture != NULL)
            {
                size_t rd;

                fseek (capfile, 0, SEEK_SET);
                rd = fread (capture, 1, (size_t) sz, capfile);
                capture[rd] = '\0';
            }
        }
        fclose (capfile);
        capfile = NULL;
    }

    ip_sandbox_depth--;
    ip_sandbox_active = (ip_sandbox_depth > 0) ? 1 : 0;
    nested = (ip_sandbox_depth > 0) ? 1 : 0;

    // copy the state of all pre-existing caller variables back by name. The
    // sandbox started as a copy of the caller table, so unchanged variables
    // keep their state and written/invalidated variables carry the precise
    // effect of the callee.
    for (i = 0; i <= vars_ind; i++)
    {
        U1 is_removed = 0;
        S8 ci;

        for (j = 0; j < removed_count; j++)
        {
            if (strcmp ((const char *) (removed + (size_t) j * ((size_t) MAXSTRLEN + 1)),
                        (const char *) vars[i].name) == 0)
            {
                is_removed = 1;
                break;
            }
        }
        if (is_removed == 1)
        {
            continue;
        }
        ci = ip_find_var_in (saved_vars, saved_vars_ind, vars[i].name);
        if (ci >= 0)
        {
            saved_vars[ci] = vars[i];
            if (ci < IP_MAX_EFFECTS)
            {
                ip_effect_mark[ci] = 1;
            }
        }
    }

    // free the sandbox and restore the caller state
    free (sandbox);
    vars = saved_vars;
    vars_ind = saved_vars_ind;

    reset_flow_analysis ();
    if (label_states != NULL)
    {
        free (label_states);
    }
    label_states = saved_label_states;
    label_states_ind = saved_label_states_ind;
    if (saved_frames != NULL)
    {
        memcpy (flow_frames, saved_frames,
                (size_t) (s_flow_frame_ind + 1) * sizeof (struct flow_frame));
    }
    flow_frame_ind = s_flow_frame_ind;
    free (saved_frames);

    flow_reachable = s_flow_reachable;
    flow_stack_depth = s_flow_stack_depth;
    flow_stack_known = s_flow_stack_known;
    flow_loop_widen = s_flow_loop_widen;
    flow_cond_truth = s_flow_cond_truth;
    flow_cond_stable = s_flow_cond_stable;
    flow_logic_strict = s_flow_logic_strict;
    flow_unreachable_warned = s_flow_unreachable_warned;
    flow_dead_branch = s_flow_dead_branch;
    flow_line_unreachable = s_flow_line_unreachable;
    flow_case_dead = s_flow_case_dead;
    flow_assert_strict = s_flow_assert_strict;
    flow_missing_return = s_flow_missing_return;
    flow_assert_count = s_flow_assert_count;

    pending_alloc = s_pending_alloc;
    pending_alloc_known = s_pending_alloc_known;
    pending_alloc_size = s_pending_alloc_size;
    pending_alloc_elem_size = s_pending_alloc_elem_size;
    pending_result = s_pending_result;
    pending_result_value = s_pending_result_value;
    pending_result_min = s_pending_result_min;
    pending_result_max = s_pending_result_max;

    function_check_stpop = s_function_check_stpop;
    function_inside = s_function_inside;
    function_return = s_function_return;
    if_inside = s_if_inside;
    switch_inside = s_switch_inside;
    for_inside = s_for_inside;
    do_inside = s_do_inside;
    pointer_strict = s_pointer_strict;
    pointer_check_active = s_pointer_check_active;
    strcpy ((char *) pointer_check_name, (const char *) s_pointer_check_name);
    pointer_check_line = s_pointer_check_line;
    linenum = s_linenum;
    file_inside = s_file_inside;
    file_index = s_file_index;
    found_exit = s_found_exit;
    func_strict = s_func_strict;
    linter_warnings = s_warnings;
    strcpy ((char *) function_name_current, (const char *) saved_funcname);

    ip_written = s_written;
    memcpy (safety_tokens, s_safety_tokens,
            (size_t) MAXTOKENS * ((size_t) MAXSTRLEN + 1));
    memcpy (ip_last_return, s_last_return, sizeof (ip_last_return));
    ip_last_return_count = s_last_return_count;
    ip_last_return_seen = s_last_return_seen;

    // restore the caller value stack and put the return values on top
    memcpy (ip_vstack, s_vstack, sizeof (ip_vstack));
    ip_vstack_ind = s_vstack_ind;
    ip_vstack_floor = s_vstack_floor;
    for (i = 0; i < summary_count && i < IP_MAX_RETURNS; i++)
    {
        ip_push_range (summary[i]);
    }

    if (capture != NULL)
    {
        ip_report_capture (capture, callee_name, saved_funcname, nested);
        free (capture);
    }

    free (removed);
    free (written);
    free (s_safety_tokens);
    return (1);
}

int main (int ac, char *av[])
{
    FILE *finptr = NULL;
    FILE *flint_status = NULL;
    U1 flint_status_file[MAXSTRLEN + 1];
    S4 file_name_len = 0;

    char *read = NULL;
    U1 ok = 0;
    S2 ret = 0;
    U1 rbuf[MAXSTRLEN + 1] = "";                        /* read-buffer for one line */
	U1 buf[MAXSTRLEN + 1] = "";
    S2 pos = 0, pos2 = 0, pos3 = 0, linton = 0, lintoff = 0, function_args_check = 0, pointer_strict_on = 0, pointer_strict_off = 0;
    S2 func_stricton = 0;
    U1 do_lint = 1; // switch linting on and off

    if (ac < 2)
    {
         printf("l1vm-linter %s\n", VM_VERSION_STR);
         printf ("l1vm-linter <program>\n");
         cleanup ();
         exit (1);
    }

    file_name_len = strlen_safe (av[1], MAXSTRLEN);
    if (file_name_len >= MAXSTRLEN - 14)
    {
       // error can't open input file
		printf ("ERROR: input file name overflow: '%s' !\n", av[1]);
		exit (1);
	}

    // flint status file
    strcpy ((char *) flint_status_file, av[1]);
    strcat ((char *) flint_status_file, ".l1lint");

    flint_status = fopen ((const char *) flint_status_file, "w");
    if (flint_status == NULL)
    {
        // error can't open lint status file
		printf ("ERROR: can't open lint status file: '%s' !\n", flint_status_file);
		exit (1);
	}

    if (fputs("linter run\n", flint_status) < 0)
    {
        // error can't open lint status file
		printf ("ERROR: can't create lint status file: '%s' !\n", flint_status_file);
		exit (1);
    }
    fclose (flint_status);

    // open input file
	finptr = fopen (av[1], "r");
	if (finptr == NULL)
	{
		// error can't open input file
		printf ("ERROR: can't open input file: '%s' !\n", av[1]);
		exit (1);
	}

     if (init_function_args () != 0)
     {
         fclose (finptr);

         cleanup ();
         exit (1);
     }

     if (init_return_args () != 0)
     {
         fclose (finptr);

         cleanup ();
         exit (1);
     }

     if (init_vars () != 0)
     {
         fclose (finptr);

         cleanup ();
         exit (1);
     }

     // collect all function bodies for the interprocedural analysis. The
     // function rewinds the input file when it is done.
     ip_budget = 2000000;
     (void) ip_collect_functions (finptr, av[1]);

     // dummys
     /*
     parse_line ("(set int64 1 zero 0)");
     parse_line ("(set int64 1 x 12)");
     parse_line ("(set int64 1 y 10)");
     parse_line ("// (func args square int64 int64)");
     parse_line ("x y :square !");
     */

    printf ("\033[31m");  // switch color to red

    // read input line loop
	ok = TRUE;
	while (ok)
	{
		read = fgets_uni ((char *) rbuf, MAXLINELEN, finptr);
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

			strcpy ((char *) buf, (const char *) rbuf);
			convtabs (buf);

            // EDIT MAIN FLAGS
            // DEBUG:
            //printf ("> '%s'", buf);

            pos = searchstr (buf, (U1 *) "//", 0, 0, TRUE);
            if (pos != -1)
            {
                 pos = searchstr (buf, (U1 *) "// (func args ", 0, 0, TRUE);
                 pos2 = searchstr (buf, (U1 *) "// (return args ", 0, 0, TRUE);

                 pos3 = searchstr (buf, (U1 *) "// (remove ", 0, 0, TRUE);

                 linton = searchstr (buf, (U1 *) "// lint-on", 0, 0, TRUE);
                 lintoff = searchstr (buf, (U1 *) "// lint-off", 0, 0, TRUE);

                 function_args_check = searchstr (buf, (U1 *) "// (func args stpop)", 0, 0, TRUE);

                 pointer_strict_on = searchstr (buf, (U1 *) "// (pointer strict on)", 0, 0, TRUE);
                 pointer_strict_off = searchstr (buf, (U1 *) "// (pointer strict off)", 0, 0, TRUE);

                 func_stricton = searchstr (buf, (U1 *) "// (func-strict)", 0, 0, TRUE);

                 if (linton != -1)
                 {
                     do_lint = 1;

                     printf ("linting: ON: line: %lli\n", linenum);

                     continue;
                 }
                 if (lintoff != -1)
                 {
                     do_lint = 0;

                     printf ("linting: OFF: line: %lli\n", linenum);

                     continue;
                 }

                 if (function_args_check != -1)
                 {
                     function_check_stpop = 1;
                     continue;
                 }

                 // pointer strict check, to allow only const type pointers and no pointer copy to another!
                 if (pointer_strict_on != -1)
                 {
                     pointer_strict = 1;
                     continue;
                 }

                 if (pointer_strict_off != -1)
                 {
                     pointer_strict = 0;
                     continue;
                 }

                 if (func_stricton != -1)
                 {
                     func_strict = 1;
                 }

                 // CBMC-style assertions and assumptions in comments
                 if (do_lint == 1 && handle_annotation_comment (buf) == 1)
                 {
                     continue;
                 }

                 if (pos == -1 && pos2 == -1 && pos3 == -1)
                 {
                     continue;
                 }
            }

            pos = searchstr (buf, (U1 *) FILENAME_START_SB, 0, 0, TRUE);
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

			pos = searchstr (buf, (U1 *) FILENAME_END_SB, 0, 0, TRUE);
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

            if (do_lint == 1)
            {
                if (parse_line (buf) != 0)
                {
                    ret = 1; // error

                    if (file_inside == 0)
                    {
                        printf ("linter error: line: %lli\n", linenum);
                    }
                    else
                    {
						printf ("linter error file: %s: line: %lli\n", files[file_index].name, files[file_index].linenum);
                    }

					printf ("> %s\n", buf);
                }
            }
        }
        else
		{
			ok = FALSE;
		}
    }

    flint_status = fopen ((const char *) flint_status_file, "w");
    if (flint_status == NULL)
    {
        // error can't open lint status file
		printf ("ERROR: can't open lint status file: '%s' !\n", flint_status_file);
		exit (1);
	}

    if (fputs("linter OK!\n", flint_status) < 0)
    {
        // error can't open lint status file
		printf ("ERROR: can't create lint status file: '%s' !\n", flint_status_file);
		exit (1);
    }
    fclose (flint_status);

    fclose (finptr);

    // Optional path-sensitive constraint solving with exact 64 bit L1VM
    // semantics. It complements the interval analysis and only reports
    // findings which are proven from the collected path constraints.
    linter_solver_file (av[1]);

    // check if exit interrupt was found
    if (found_exit == 0)
    {
        printf ("linter error: no exit call in program found!\n");
        ret = 1; // error
    }

    // check for functions / control blocks which are not closed at end of file
    if (function_inside == 1)
    {
        printf ("linter error: function '%s' is not closed by '(funcend)' at end of file!\n", function_name_current);
        ret = 1;
    }
    if (if_inside != 0 || switch_inside != 0 || for_inside != 0 || do_inside != 0)
    {
        printf ("linter error: unclosed control blocks at end of file (if: %i, switch: %i, for: %i, do: %i)!\n",
                if_inside, switch_inside, for_inside, do_inside);
        ret = 1;
    }

    cleanup ();

    if (linter_warnings > 0)
    {
        printf ("l1vm-linter: %lli warning(s) treated as errors!\n", linter_warnings);
        ret = 1;
    }

    if (ret == 0) printf ("\033[0ml1vm-linter %s, no errors found!\n", VM_VERSION_STR );

    exit (ret);
}
