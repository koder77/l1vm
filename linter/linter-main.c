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
};

struct vars *vars;
S8 vars_ind ALIGN = -1;

// current function name
U1 function_name_current[MAXSTRLEN + 1];
U1 function_check_stpop = 0;    // set to 1 by: // (func args stpop)

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
        if (strcmp ((const char  *) function_args[function_args_ind].name, (const char *)  name) == 0)
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
        printf ("get_return_index: %lli: name: '%s'\n", i, return_args[return_args_ind].name);
        #endif

        if (strcmp ((const char  *) return_args[return_args_ind].name, (const char *)  name) == 0)
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
            // found variable, return variable type
            return (vars[i].type);
        }
    }
    // variable not found
    printf ("error: variable '%s' not defined!\n", name);
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



void cleanup (void)
{
    if (vars != NULL) free (vars);
    if (function_args != NULL) free (function_args);
    if (return_args != NULL) free (return_args);
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
    S2 spaces = 0;
    S2 args_ind = -1;
    U1 do_parse_vars = 1;
    S2 function_call = 0;
    S8 function_index ALIGN = 0;
    S2 func_pos = 0;
    S2 func_args = 0;
    S2 stpop_pos = 0;
    S2 return_pos = 0;
    S2 func_stpop_pos = 0;


    U1 type[MAXSTRLEN + 1];
    U1 name[MAXSTRLEN + 1];

    line_len = strlen_safe ((const char *) line, MAXSTRLEN);

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

        spaces = 0;
        i = name_pos;
        while (get_name == 1)
        {
            if (line[i] != ' ')
            {
                spaces++;
            }

            if (spaces == 2)
            {
                name_pos = i;
                get_name = 0;
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
            vars_ind++;
            strcpy ((char *) vars[vars_ind].name, (char *) name);
            vars[vars_ind].type = vartype;

            #if DEBUG
            printf ("variable: index: %lli, name: '%s', type: %lli\n", vars_ind, vars[vars_ind].name, vars[vars_ind].type);
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
        strict_pos = searchstr (line, (U1 *) "// (func args S ", 0, 0, TRUE);
        if (strict_pos != -1)
        {
            // handle function as strict. Use variables with R at the ending for return values
            function_args[function_index].strict = 1;
            name_pos = pos + 16;
        }
        else
        {
            function_args[function_index].strict = 0;
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

        if (function_args_ind < MAXFUNC - 1)
        {
            function_args_ind++;
            strcpy ((char *) function_args[function_args_ind].name, (char *) name);

            #if DEBUG
            printf ("function: index: %lli, name: '%s'\n", function_args_ind, function_args[function_args_ind].name);
            #endif
        }
        else
        {
            printf ("parse_line: set function overflow!\n");
            return (1);
        }

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
    function_call = searchstr (line, (U1 *) "set", 0, 0, TRUE);
    if (function_call != -1)
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
            printf ("parse_line: set call function not set: '%s'!\n", name);
            return (1);
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

                    if (function_args[function_index].strict == 1)
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

            if (function_args[function_args_ind].strict == 1)
            {
                {
                    // EDIT neu2
                    S2 type_len;

                    type_len = strlen_safe ((const char *) type, MAXSTRLEN);
                    if (type_len > 1)
                    {
                        if (type[type_len - 1] != 'R')
                        {
                            printf ("parse_line: call function: variable ends with 'R'! It is a return value!\n");
                            return (1);
                        }
                    }
                }
            }

            vartype = get_varname_type (type);
            if (vartype == -1)
            {
                // error no valid vartype
                printf ("parse-line: call function: invalid variable: '%s' ! \n", type);
                return (1);
            }

            if (args_ind <= function_args[function_args_ind].args)
            {
                args_ind++;

                if (function_args[function_args_ind].arg_type[args_ind] == VARTYPE_VARIABLE)
                {
                    // number of variables can change
                    // break
                    do_parse_vars = 0;
                    continue;
                }

                if (function_args[function_args_ind].arg_type[args_ind] != VARTYPE_ANY)
                {
                    // not an any var type, check:
                    if (function_args[function_args_ind].arg_type[args_ind] != vartype)
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
        }

        if (args_ind != function_args[function_args_ind].args - 1)
        {
            // error args number doesn't match
            printf ("parse-line: call function: args num mismatch! got %i args, need %i args!\n", args_ind + 1, function_args[function_args_ind].args);
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

            return_args_ind = get_return_index (name);
            if (return_args_ind == -1)
            {
                printf ("parse_line: return variables, set return function not set: '%s'!\n", name);
                return (1);
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

                        if (function_args[function_index].strict == 1)
                        {
                            if (type_ind >= 2)
                            {
                                if (type[type_ind - 1] != 'R')
                                {
                                    printf ("parse-line: variable '%s' is not return type variable!\n", type);
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

                if (args_ind <= return_args[return_args_ind].args)
                {
                    args_ind++;

                    if (function_args[function_args_ind].arg_type[args_ind] == VARTYPE_VARIABLE)
                    {
                        // number of variables can change
                        // break
                        do_parse_vars = 0;
                        continue;
                    }

                    if (return_args[return_args_ind].arg_type[args_ind] != VARTYPE_ANY)
                    {
                        if (return_args[return_args_ind].arg_type[args_ind] != vartype)
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
            printf ("parse-line: args_ind: %i, return_args[return_args_ind].args: %i\n", args_ind, return_args[return_args_ind].args);
            #endif

            if (args_ind != return_args[return_args_ind].args - 1)
            {
                // error args number doesn't match
                printf ("parse-line: return function: args num mismatch! got %i args, need %i args!\n", args_ind + 1, return_args[return_args_ind].args);
                return (1);
            }
        }
     }

    // check if function start
    func_pos = searchstr (line, (U1 *) "func)", 0, 0, TRUE);
    if (func_pos != -1)
    {
        get_current_function_name (line);
    }

    // check if return line
    //printf ("parse-line: line: '%s'\n", line);

    return_pos = searchstr (line, (U1 *) "(return)", 0, 0, TRUE);
    if (return_pos != -1)
    {
        return_args_ind = get_return_index (function_name_current);
        if (return_args_ind == -1)
        {
            // no return variables
            return (0);
        }

        { // new scope
        S2 n = 0;
        S2 var_ind = return_args[return_args_ind].args -1;
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
                    if (function_args[function_args_ind].arg_type[var_ind] == VARTYPE_VARIABLE)
                    {
                        // number of variables can change
                        // break
                        do_parse_vars = 0;
                        continue;
                    }

                    if (return_args[return_args_ind].arg_type[var_ind] != VARTYPE_ANY)
                    {
                        // not an any type variable, check type:
                        if (vartype != return_args[return_args_ind].arg_type[var_ind])
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

    if (function_check_stpop == 1)
    {
        func_stpop_pos = searchstr (line, (U1 *) "stpop)", 0, 0, TRUE);
        if (func_stpop_pos != -1)
        {
            function_args_ind = get_function_index (function_name_current);
            if (function_args_ind == -1)
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
                S2 var_ind = function_args[function_args_ind].args -1 ;
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

                        if (function_args[function_args_ind].arg_type[args_ind] == VARTYPE_VARIABLE)
                        {
                            // number of variables can change
                            // break
                            do_parse_vars = 0;
                            continue;
                        }

                        if (vartype != function_args[function_args_ind].arg_type[var_ind])
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
                        n = 0;
                    }
                    i++;
                }
            } // scope close
        }
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
    S2 pos = 0, pos2 = 0, linton = 0, lintoff = 0, function_args_check = 0;
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

            pos = searchstr (buf, (U1 *) "//", 0, 0, TRUE);
            if (pos != -1)
            {
                 pos = searchstr (buf, (U1 *) "// (func args ", 0, 0, TRUE);
                 pos2 = searchstr (buf, (U1 *) "// (return args ", 0, 0, TRUE);

                 linton = searchstr (buf, (U1 *) "// lint-on", 0, 0, TRUE);
                 lintoff = searchstr (buf, (U1 *) "// lint-off", 0, 0, TRUE);

                 function_args_check = searchstr (buf, (U1 *) "// (func args stpop)", 0, 0, TRUE);

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

                     printf ("linting: OFF : line: %lli\n", linenum);

                     linenum++;
                     continue;
                 }

                 if (function_args_check != -1)
                 {
                     function_check_stpop = 1;
                     linenum++;
                     continue;
                 }

                 if (pos == -1 && pos2 == -1)
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
    cleanup ();

    if (ret == 0) printf ("\033[0ml1vm-linter %s, no errors found!\n", VM_VERSION_STR );

    exit (ret);
}
