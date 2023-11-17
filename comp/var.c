/*
 * This file var.c is part of L1vm.
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

#include "../include/global.h"
#include "../include/opcodes-types.h"

extern struct data_info data_info[MAXDATAINFO];
extern struct data_info_var data_info_var[MAXDATAINFO];
extern S8 data_ind;
extern S8 linenum;

// globals for local variable ending check only allow local and global (main) ending if set!
extern U1 check_varname_end;
extern U1 varname_end[MAXLINELEN];

// protos
U1 checkdigit (U1 *str);
size_t strlen_safe (const char *str, int maxlen);
S2 searchstr (U1 *str, U1 *srchstr, S2 start, S2 end, U1 case_sens);

S2 check_varname_ending (U1 *varname, U1 *ending)
{
	// check if variable has ending set local function name or 'main' for global variables

	S2 pos, varname_len, ending_len;

	varname_len = strlen_safe ((const char *) varname, MAXLINELEN);
	ending_len = strlen_safe ((const char *) ending, MAXLINELEN);

	pos = searchstr (varname, ending, 0, 0, 0);
	if (pos < 0)
	{
		// check for main ending
	    ending_len = strlen_safe ("main", MAXLINELEN);

		pos = searchstr (varname, (U1 *) "main", 0, 0, 0);
		if (pos < 0)
		{
			printf ("check_ending: no variable ending 'main' or '%s' found!\n", ending);
			return (1);
		}

		// check if ending is on end
		if (varname_len - ending_len != pos)
		{
			printf ("check_ending: no variable ending 'main' or '%s' found!\n", ending);
			return (1);
		}
	}

	// check if ending is on end
	if (varname_len - ending_len != pos)
	{
		printf ("check_ending: no variable ending '%s' found!\n", ending);
		return (1);
	}

	return (0);
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

	if (check_varname_end == 1)
	{
		if (check_varname_ending (name, varname_end) != 0)
		{
			return (1);
		}
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

S2 checkset (U1 *name)
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
			// variable already defined!
			return (0);
		}
	}
	// variable not defined, check ok!
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
			// set variable as used:
			data_info_var[i].used = 1;

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

S2 get_var_is_const (U1 *name)
{
	S4 i;

	if (name[0] == ':')
	{
		// it is a label, beginning with a colon char
		return (LABEL);
	}

	for (i = 0; i <= data_ind; i++)
	{
		if (strcmp ((const char *) name, (const char *) data_info[i].name) == 0)
		{
			if (data_info[i].constant == 0)
			{
				return (0);
			}
			else
			{
				return (1);
			}
		}
	}
	return (-1);	// ERROR
}

// data info var
// init data_info_var
void init_data_info_var (void)
{
	S4 i;

	for (i = 0; i < MAXDATAINFO; i++)
	{
		data_info_var[i].used = 0;
	}
}

// check if defined but unused variables are in code
S2 get_unused_var (void)
{
	S4 i;
	S2 ret = 0;
	U1 is_string_addr = 0;
	S2 pos;
	S2 slen;

	for (i = 0; i <= data_ind; i++)
	{
		if (data_info_var[i].used == 0)
		{
			is_string_addr = 0;
			slen = strlen_safe ((const char *) data_info[i].name, MAXLINELEN);
		    if (slen >= 5)
			{
				if (data_info[i].name[slen - 1] == 'r' && data_info[i].name[slen - 2] == 'd' &&  data_info[i].name[slen - 3] == 'd' && data_info[i].name[slen - 4] == 'a')
				{
					is_string_addr = 1;
				}
			}
			if (is_string_addr == 0)
			{
				// ptrint in red text color
				printf ("\033[31mwarning: set but unused variable: %s\n", data_info[i].name);
				ret = 1; // found unused variable
			}
		}
	}
	// set normal bash text color
	printf ("\033[0m");
	return (ret);
}
