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
    U1 value_known;             // 1 = value is known at compile time
    S8 value ALIGN;             // known compile time value
    S8 array_size ALIGN;        // number of elements, -1 = unknown
};

struct vars *vars;
S8 vars_ind ALIGN = -1;

// pointer safety state
U1 pointer_check_active = 0;
U1 pointer_check_name[MAXSTRLEN + 1] = "";
S8 pointer_check_line ALIGN = -1;

// number of warnings (warnings do not make the linter fail)
S8 linter_warnings ALIGN = 0;

// current function name
U1 function_name_current[MAXSTRLEN + 1];
U1 function_check_stpop = 0;    // set to 1 by: // (func args stpop)

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
    S8 i ALIGN = -1;

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
    S8 i ALIGN = -1;

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
    if (vars != NULL) free (vars);
    if (function_args != NULL) free (function_args);
    if (return_args != NULL) free (return_args);
}

// ---------------------------------------------------------------------------
// safety checks: tokens, pointers, initialization, array bounds, structure
// ---------------------------------------------------------------------------

#define MAXTOKENS 256

U1 safety_tokens[MAXTOKENS][MAXSTRLEN + 1];

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
    // typed operator suffixes: int64 (i), double (d), byte (b), int16 (w)
    return (U1) (c == 'i' || c == 'd' || c == 'b' || c == 'w');
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
            j = 0;
            tokens[n][j] = c;
            j++;
            i++;
            while (i < len && isdigit ((unsigned char) line[i]) && j < MAXSTRLEN)
            {
                tokens[n][j] = line[i];
                j++;
                i++;
            }
            tokens[n][j] = '\0';
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

        // identifier or digit sequence
        if (is_ident_start (c) || isdigit ((unsigned char) c))
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
        vars[vi].initialized = 1;
        vars[vi].pointer_set = 1;
        vars[vi].value_known = 0;
        vars[vi].warned_uninit = 0;
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
    printf ("linter warning: variable '%s' is used before it is initialized!\n", name);
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

// main safety check, called for every parsed line
S2 safety_check_line (U1 *line)
{
    S2 nt;
    S2 i;
    S2 op;
    S8 vi ALIGN;
    S8 vj ALIGN;
    U1 *base;
    U1 *idx;
    U1 *dest;
    U1 *src;
    U1 *name;
    S8 value ALIGN;
    U1 is_set = 0;
    U1 is_intr = 0;
    U1 is_pointer_instruction = 0;

    nt = tokenize_line (line, safety_tokens, MAXTOKENS);
    if (nt == 0)
    {
        return (0);
    }

    // preprocessor lines and file markers
    if (line[0] == '#')
    {
        return (0);
    }
    if (strcmp ((const char *) safety_tokens[0], "FILE:") == 0)
    {
        return (0);
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
                printf ("linter warning: variable '%s' is declared more than once (last declaration wins)!\n", name);
                printf (" > %s\n", line);
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

                vars[vj].initialized = 1;
                vars[vj].pointer_set = 1;
                vars[vj].is_pointer = 1;
                vars[vj].value_known = 0;
                vars[vj].warned_uninit = 0;

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

    // stpop: all variables before stpop are written
    for (i = 0; i < nt; i++)
    {
        if (strcmp ((const char *) safety_tokens[i], "stpop") == 0 ||
            strcmp ((const char *) safety_tokens[i], "stpopi") == 0 ||
            strcmp ((const char *) safety_tokens[i], "stpopd") == 0 ||
            strcmp ((const char *) safety_tokens[i], "stpopb") == 0)
        {
            S2 k;
            for (k = 0; k < i; k++)
            {
                if (is_ident_start (safety_tokens[k][0]))
                {
                    mark_var_written (safety_tokens[k]);
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
                        mark_var_written (safety_tokens[i - 1]);
                    }
                }
            }
        }
        // division / modulo by a known zero (also typed operators like /d, %i)
        if ((safety_tokens[i][0] == '/' || safety_tokens[i][0] == '%') &&
            (safety_tokens[i][1] == '\0' || is_operator_suffix (safety_tokens[i][1])) &&
            safety_tokens[i][2] == '\0')
        {
            if (i + 1 < nt)
            {
                idx = safety_tokens[i + 1];
                if (is_number_token (idx))
                {
                    value = (S8) strtoll ((const char *) idx, NULL, 10);
                    if (value == 0)
                    {
                        printf ("linter error: division by zero!\n");
                        printf (" > %s\n", line);
                        return (1);
                    }
                }
                else
                {
                    vi = find_var_index (idx);
                    if (vi >= 0)
                    {
                        if (vars[vi].value_known == 1 && vars[vi].value == 0)
                        {
                            printf ("linter error: division by zero (variable '%s' is zero)!\n", idx);
                            printf (" > %s\n", line);
                            return (1);
                        }
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
                vars[vi].is_pointer = 1;
            }

            // check compile time known indices of real arrays
            if (vars[vi].is_array == 1 && vars[vi].array_size > 1 && i + 1 < nt)
            {
                idx = safety_tokens[i + 1];
                value = 0;
                vj = -1;
                if (is_number_token (idx))
                {
                    value = (S8) strtoll ((const char *) idx, NULL, 10);
                }
                else
                {
                    vj = find_var_index (idx);
                    if (vj >= 0 && vars[vj].value_known == 1)
                    {
                        value = vars[vj].value;
                    }
                    else
                    {
                        vj = -1;
                    }
                }

                if (is_number_token (idx) || vj >= 0)
                {
                    S8 elem_size ALIGN = var_element_size (vars[vi].type);
                    S8 array_bytes ALIGN = vars[vi].array_size * elem_size;

                    if (value < 0 || value > array_bytes - elem_size)
                    {
                        printf ("linter error: array index out of bounds: '%s[%lli]', array size is %lli byte (element size %lli)!\n",
                                base, value, array_bytes, elem_size);
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
    U1 get_name = 1;
    S2 vartype = 0;
    S2 type_len = 0;
    S2 name_len = 0;
    S2 args_ind = -1;
    U1 do_parse_vars = 1;
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
            if (function_return == 0)
            {
                if (strcmp ((const char *) function_name_current, "main") != 0)
                {
                    if (get_return_index (function_name_current) != -1)
                    {
                        // return arguments in function
                        // ńo return found at function end, show message
                        printf ("parse-line function: %s, no unnested return found!\n", function_name_current);
                        return (1);
                    }
                }
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
            S8 array_size ALIGN = -1;
            S8 known_value ALIGN = 0;
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
            vars[vars_ind].value_known = value_known;
            vars[vars_ind].value = known_value;
            vars[vars_ind].array_size = array_size;

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
                            printf ("DEBUG: parse_line: call function: variable ends with 'R'! It is a return value!\n");
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
                                printf ("DEBUG: parse_line: call function: variable ends with 'R'! It is a return value!\n");
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
                                if (type[type_len - 1] == 'R')
                                {
                                    printf ("DEBUG: parse_line: call function: variable ends with 'R'! It is a return value!\n");
                                    return (1);
                                }
                                else
                                {
                                    S2 ending_len;
                                    U1 endingstr[MAXSTRLEN] = "R";
                                    S2 pos;

                                    strcat ((char *) endingstr, (const char*) function_name_current);
                                    ending_len = strlen_safe ((const char *) endingstr, MAXSTRLEN);

                                    pos = searchstr (type, endingstr, 0, 0, TRUE);
                                    if (pos == type_len - ending_len)
                                    {
                                        printf ("DEBUG: parse_line: call function: variable ends with 'R'! It is a return value!\n");
                                        return (1);
                                    }
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

                type_len = strlen_safe ((const char *) type, MAXSTRLEN);
                type_pos = type_pos + type_len + 1 ;

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

int main (int ac, char *av[])
{
    FILE *finptr = NULL;
    FILE *flint_status = NULL;
    U1 flint_status_file[MAXSTRLEN + 1];
    S4 file_name_len = 0;

    char *read = NULL;
    S4 slen = 0;
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
			slen = strlen_safe ((const char *) buf, MAXLINELEN);

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

                     linenum++;
                     continue;
                 }
                 if (lintoff != -1)
                 {
                     do_lint = 0;

                     printf ("linting: OFF: line: %lli\n", linenum);

                     linenum++;
                     continue;
                 }

                 if (function_args_check != -1)
                 {
                     function_check_stpop = 1;
                     linenum++;
                     continue;
                 }

                 // pointer strict check, to allow only const type pointers and no pointer copy to another!
                 if (pointer_strict_on != -1)
                 {
                     pointer_strict = 1;
                     linenum++;
                     continue;
                 }

                 if (pointer_strict_off != -1)
                 {
                     pointer_strict = 0;
                     linenum++;
                     continue;
                 }

                 if (func_stricton != -1)
                 {
                     func_strict = 1;
                 }

                 if (pos == -1 && pos2 == -1 && pos3 == -1)
                 {
                     linenum++;
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
        printf ("l1vm-linter: %lli warning(s) found!\n", linter_warnings);
    }

    if (ret == 0) printf ("\033[0ml1vm-linter %s, no errors found!\n", VM_VERSION_STR );

    exit (ret);
}
