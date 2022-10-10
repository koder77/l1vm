/*
 * This file memobj.cpp is part of L1vm.
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

// store int64 and double variables into a memory object
// see lib/mem-obj-lib.l1com

#include <cstdlib>
#include <iostream>

using namespace std;

#include "../../../include/global.h"
#include "../../../include/stack.h"


// protos

extern "C" S2 memory_bounds (S8 start, S8 offset_access);


// arrays memory codes
#define EMPTY 		0
#define MEMINT64	1
#define MEMDOUBLE   2
#define MEMSTRING	3

// structures

// allocated memory pointers union
union memptr
{
	S8 int64v ALIGN;
	F8 doublev ALIGN;
	U1 *straddr;
};

struct obj
{
	union memptr memptr;
	U1 type;
	S8 strlen ALIGN;
};

// the memory info structure
struct mem
{
	U1 used;
	S8 memsize ALIGN;
	struct obj *objptr;	// the memory pointers to save the allocations
};

static struct mem *mem = NULL;
static S8 memmax ALIGN = 0;

size_t strlen_safe (const char * str, int maxlen)
{
	 long long int i = 0;

	 while (1)
	 {
	 	if (str[i] != '\0')
		{
			i++;
		}
		else
		{
			return (i);
		}
		if (i > maxlen)
		{
			return (0);
		}
	}
}

// mem functions ==============================================================

extern "C" U1 *init_mem (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 i ALIGN;
	S8 maxind ALIGN;

	sp = stpopi ((U1 *) &maxind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("init_mem: ERROR: stack corrupt!\n");
		return (NULL);
	}

	// printf ("memobj: init_mem: maxind: %lli\n", maxind);

	// allocate gobal mem structure
	mem = (struct mem *) calloc (maxind, sizeof (struct mem));
	if (mem == NULL)
	{
		printf ("init_mem: ERROR can't allocate %lli memory indexes!\n", maxind);

		sp = stpushi (1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("init_mem: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}

	memmax = maxind;	// save to global var

	// init mem structure
	for (i = 0; i < memmax; i++)
	{
		mem[i].used = 0;
		mem[i].memsize = 0;
		mem[i].objptr = NULL;
	}

	// printf ("init_mem: allocated %lli memory spaces\n", maxind);

	// error code ok
	sp = stpushi (0, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("init_mem: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

extern "C" U1 *free_mem (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// free global mem
	if (mem) free (mem);
	return (sp);
}

extern "C" S8 get_free_mem (void)
{
	S8 i ALIGN;

	for (i = 0; i < memmax; i++)
	{
		if (mem[i].used == 0)
		{
			// return index of free mem
			return (i);
		}
	}

	// no free memory found
	return (-1);
}

extern "C" U1 *alloc_obj_mem (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 memind ALIGN;
	S8 memsize ALIGN;
	S8 variables ALIGN;
	S8 i ALIGN;

	sp = stpopi ((U1 *) &memsize, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("alloc_obj_mem: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &variables, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("alloc_obj_mem: ERROR: stack corrupt!\n");
		return (NULL);
	}

	memind = get_free_mem ();
	if (memind == -1)
	{
		printf ("alloc_obj_mem: no more memory slot free!\n");
		return (NULL);
	}

	mem[memind].objptr = (obj *) calloc (memsize * variables, sizeof (struct obj));
	if (mem[memind].objptr == NULL)
	{
		printf ("alloc_obj_mem: out of memory: %lli of size  byte\n", memsize);

		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("alloc_obj_mem: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}

	// save memory pointer
	mem[memind].used = 1;
	mem[memind].memsize = memsize * variables;

	// set string lengths to zero, to mark them as empty
	for (i = 0; i < memsize; i++)
	{
		mem[memind].objptr[i].type = EMPTY;
		mem[memind].objptr[i].strlen = 0;
	}

	// return memory index
	sp = stpushi (memind, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("alloc_obj_mem: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

extern "C" U1 *free_obj_mem (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 memind ALIGN;
	S8 i ALIGN;

	sp = stpopi ((U1 *) &memind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("free_obj_mem: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (memind < 0 || memind >= mem[memind].memsize)
	{
		// error
		printf ("free_obj_mem: ERROR: object index out of range!\n");
		return (NULL);
	}

	for (i = 0; i < mem[memind].memsize; i++)
	{
		if (mem[memind].objptr[i].strlen != 0)
		{
			free (mem[memind].objptr[i].memptr.straddr);
			mem[memind].objptr[i].strlen = 0;
		}
	}

	free (mem[memind].objptr);
	return (sp);
}

extern "C" U1 *save_obj_mem (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 memind ALIGN;
	S8 ind ALIGN;
	S8 variables ALIGN;
	S8 type ALIGN;
	S8 i ALIGN;
	S8 var_i ALIGN;
	F8 var_d ALIGN;
	S8 var_saddr ALIGN;
	S8 string_len ALIGN;

	// get object array index to write the variables into
	sp = stpopi ((U1 *) &memind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("save_obj_mem: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (memind < 0 || memind >= memmax)
	{
		// error
		printf ("save_obj_mem: ERROR: memind index out of range!\n");
		return (NULL);
	}

	// get mem obj variable index start
	sp = stpopi ((U1 *) &ind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("save_obj_mem: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (ind < 0 || ind >= mem[memind].memsize)
	{
		// error
		printf ("save_obj_mem: ERROR: ind index out of range!\n");
		return (NULL);
	}

	// get number of variables to store in the array
	sp = stpopi ((U1 *) &variables, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("save_obj_mem: ERROR: stack corrupt!\n");
		return (NULL);
	}

	// get the variable types and the variables from stack
	for (i = 0; i < variables; i++)
	{
		// get variable type from stack
		sp = stpopi ((U1 *) &type, sp, sp_top);
		if (sp == NULL)
		{
			// error
			printf ("save_obj_mem: ERROR: stack corrupt!\n");
			return (NULL);
		}

		// printf ("DEBUG save_obj_mem: variable type: %lli\n", type);

		switch (type)
		{
			case MEMINT64:
				// get int64 variable
				sp = stpopi ((U1 *) &var_i, sp, sp_top);
				if (sp == NULL)
				{
					// error
					printf ("save_obj_mem: ERROR: stack corrupt!\n");
					return (NULL);
				}

				if (ind < mem[memind].memsize)
				{
					mem[memind].objptr[ind].type = MEMINT64;
					mem[memind].objptr[ind].memptr.int64v = var_i;
				}
				else
				{
					// variable index overflow
					sp = stpushi (1, sp, sp_bottom);
					if (sp == NULL)
					{
						// error
						printf ("save_obj_mem: ERROR: stack corrupt!\n");
						return (NULL);
					}
					return (sp);
				}
				ind++;
				break;

			case MEMDOUBLE:
				// get double variable
				sp = stpopd ((U1 *) &var_d, sp, sp_top);
				if (sp == NULL)
				{
					// error
					printf ("save_obj_mem: ERROR: stack corrupt!\n");
					return (NULL);
				}

				if (ind < mem[memind].memsize)
				{
					mem[memind].objptr[ind].type = MEMDOUBLE;
					mem[memind].objptr[ind].memptr.doublev = var_d;
				}
				else
				{
					// variable index overflow
					printf ("save_obj_mem: error obj variable index overflow: %lli\n", memind);

					sp = stpushi (1, sp, sp_bottom);
					if (sp == NULL)
					{
						// error
						printf ("save_obj_mem: ERROR: stack corrupt!\n");
						return (NULL);
					}
					return (sp);
				}
				ind++;
				break;

			case MEMSTRING:
				// create a new string in memory and copy the string argument to it
				// printf ("DEBUG: save_obj_mem: save string...\n");

				sp = stpopi ((U1 *) &var_saddr, sp, sp_top);
				if (sp == NULL)
				{
					// error
					printf ("save_obj_mem: ERROR: stack corrupt!\n");
					return (NULL);
				}

				if (mem[memind].objptr[ind].strlen != 0)
				{
					// found allocated string in memory, free it now
					free (mem[memind].objptr[ind].memptr.straddr);
				 	mem[memind].objptr[ind].strlen = 0;
				}

				if (ind < mem[memind].memsize)
				{
					mem[memind].objptr[ind].type = MEMSTRING;
					string_len = strlen_safe ((char *) &data[var_saddr], MAXLINELEN);
					if (string_len > 0)
					{
						// string not empty, create mem string
						mem[memind].objptr[ind].strlen = string_len + 1;
						mem[memind].objptr[ind].memptr.straddr = (U1 *) calloc (string_len + 1, sizeof (U1));
						if (mem[memind].objptr[ind].memptr.straddr == NULL)
						{
							// error out of memory!
							printf ("save_obj_mem: out of memory: %lli of size string\n", string_len + 1);
							return (NULL);
						}

						// copy string to new memory

						strcpy ((char *) mem[memind].objptr[ind].memptr.straddr, (const char *) &data[var_saddr]);
						// printf ("DEBUG: save_obj_mem: string: '%s' copied into mem obj: %lli\n", &data[var_saddr], ind);
					}
				}
				ind++;
				break;
		}
	}

	// all ok
	sp = stpushi (0, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("save_obj_mem: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

extern "C" U1 *load_obj_mem (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 memind ALIGN;
	S8 ind ALIGN;
	S8 variables ALIGN;
	S8 i ALIGN;

	// get object array index to write the variables into
	sp = stpopi ((U1 *) &memind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("load_obj_mem: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (memind < 0 || memind >= memmax)
	{
		// error
		printf ("load_obj_mem: ERROR: memind index out of range!\n");
		return (NULL);
	}

	// get mem obj variable index start
	sp = stpopi ((U1 *) &ind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("load_obj_mem: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (ind < 0 || ind >= mem[memind].memsize)
	{
		// error
		printf ("load_obj_mem: ERROR: ind index out of range!\n");
		return (NULL);
	}

	// get number of variables to load from the array
	sp = stpopi ((U1 *) &variables, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("load_obj_mem: ERROR: stack corrupt!\n");
		return (NULL);
	}

	// get the variable types ans the variables from stack
	for (i = 0; i < variables; i++)
	{
		if (ind < mem[memind].memsize)
		{
			switch (mem[memind].objptr[ind].type)
			{
				case MEMINT64:
					// get int64 variable from object array
					sp = stpushi (mem[memind].objptr[ind].memptr.int64v, sp, sp_bottom);
					if (sp == NULL)
					{
						// error
						printf ("load_obj_mem: ERROR: stack corrupt!\n");
						return (NULL);
					}
					break;

				case MEMDOUBLE:
					// get double variable from object array
					sp = stpushd (mem[memind].objptr[ind].memptr.doublev, sp, sp_bottom);
					if (sp == NULL)
					{
						// error
						printf ("load_obj_mem: ERROR: stack corrupt!\n");
						return (NULL);
					}
					break;
			}
			ind++;
		}
		else
		{
			// variable index overflow
			printf ("load_obj_mem: error obj variable index overflow: %lli\n", memind);

			// error exit
			sp = stpushi (1, sp, sp_bottom);
			if (sp == NULL)
			{
				// error
				printf ("load_obj_mem: ERROR: stack corrupt!\n");
				return (NULL);
			}
			return (sp);
		}
	}

	// ok exit
	sp = stpushi (0, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("load_obj_mem: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

// load string from memory ====================================================
extern "C" U1 *load_string_obj_mem (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 memind ALIGN;
	S8 ind ALIGN;
	S8 var_saddr ALIGN;
	S8 offset ALIGN;

	// address of target string
	sp = stpopi ((U1 *) &var_saddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("load_string_obj_mem: ERROR: stack corrupt!\n");
		return (NULL);
	}

	// get object array index to write the variables into
	sp = stpopi ((U1 *) &memind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("load_string_obj_mem: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (memind < 0 || memind >= memmax)
	{
		// error
		printf ("load_string_obj_mem: ERROR: memind index out of range!\n");
		return (NULL);
	}

	// get mem obj variable index start
	sp = stpopi ((U1 *) &ind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("load_string_obj_mem: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (ind < 0 || ind >= mem[memind].memsize)
	{
		// error
		printf ("load_string_obj_mem: ERROR: ind index out of range!\n");
		return (NULL);
	}

	if (mem[memind].objptr[ind].type != MEMSTRING)
	{
		printf ("load_string_obj_mem: ERROR: variable type: %i is not string: %i!\n", mem[memind].objptr[ind].type, MEMSTRING);
		return (NULL);
	}

	offset = strlen_safe ((char *) mem[memind].objptr[ind].memptr.straddr, MAXLINELEN);

	if (offset >= 0)
	{
		#if BOUNDSCHECK
		if (memory_bounds (var_saddr, offset) != 0)
		{
			printf ("string_copy: ERROR: dest string overflow!\n");
			return (NULL);
		}
		#endif

		strcpy ((char *) &data[var_saddr], (const char *)  mem[memind].objptr[ind].memptr.straddr);

		// ok exit
		sp = stpushi (0, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("load_string_obj_mem: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}
	else
	{
		// error no string set, ERROR exit
		sp = stpushi (1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("load_string_obj_mem: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}
}

extern "C" U1 *load_obj_mem_array (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 memind ALIGN;
	S8 ind ALIGN;
	S8 variables ALIGN;
	S8 i ALIGN;
	S8 address ALIGN;
	S8 offset ALIGN;
	S8 *int_ptr;
	F8 *double_ptr;

	// get object array index to write the variables into
	sp = stpopi ((U1 *) &memind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("load_obj_mem_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (memind < 0 || memind >= memmax)
	{
		// error
		printf ("load_obj_mem_array: ERROR: memind index out of range!\n");
		return (NULL);
	}

	// get mem obj variable index start
	sp = stpopi ((U1 *) &ind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("load_obj_mem_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (ind < 0 || ind >= mem[memind].memsize)
	{
		// error
		printf ("load_obj_mem_array: ERROR: ind index out of range!\n");
		return (NULL);
	}

	// get number of variables to load from the array
	sp = stpopi ((U1 *) &variables, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("load_obj_mem_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	// get the variable types ans the variables from stack
	for (i = 0; i < variables; i++)
	{
		if (ind < mem[memind].memsize)
		{
			sp = stpopi ((U1 *) &address, sp, sp_top);
			if (sp == NULL)
			{
				// error
				printf ("load_obj_mem_array: ERROR: stack corrupt!\n");
				return (NULL);
			}

			#if BOUNDSCHECK
			if (memory_bounds (address, 0) != 0)
			{
				printf ("load_obj_mem_array: address: %lli overflow!\n", address);
				return (NULL);
			}
			#endif

			switch (mem[memind].objptr[ind].type)
			{
				case MEMINT64:
					// get int64 variable from object array
					// write to variable address on stack
					int_ptr = (S8 *) &data[address];
					*int_ptr = (S8) mem[memind].objptr[ind].memptr.int64v;
					break;

				case MEMDOUBLE:
					// get double variable from object array
					// write to variable address on stack
					double_ptr = (F8 *) &data[address];
					*double_ptr = (F8) mem[memind].objptr[ind].memptr.doublev;
					break;

				case MEMSTRING:
					// get double variable from object array
					// write to variable address on stack
					offset = strlen_safe ((char *) mem[memind].objptr[ind].memptr.straddr, MAXLINELEN);

					if (offset >= 0)
					{
						#if BOUNDSCHECK
						if (memory_bounds (address, offset) != 0)
						{
							printf ("load_obj_mem_array: string_copy: ERROR: dest string overflow: address: %lli\n", address);
							return (NULL);
						}
						#endif
					}

					strcpy ((char *) &data[address], (const char *)  mem[memind].objptr[ind].memptr.straddr);
					break;
			}
			ind++;
		}
		else
		{
			// variable index overflow
			printf ("load_obj_mem_array: error obj variable index overflow: %lli\n", memind);

			// error exit
			sp = stpushi (1, sp, sp_bottom);
			if (sp == NULL)
			{
				// error
				printf ("load_obj_mem_array: ERROR: stack corrupt!\n");
				return (NULL);
			}
			return (sp);
		}
	}
	// ok exit
		sp = stpushi (0, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("load_string_obj_mem_array: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
}

extern "C" U1 *get_obj_mem_type (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 memind ALIGN;
	S8 ind ALIGN;
	S8 mem_type ALIGN = -1;

	// get object array index to write the variables into
	sp = stpopi ((U1 *) &memind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("get_obj_mem_type: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (memind < 0 || memind >= memmax)
	{
		// error
		printf ("get_obj_mem_type: ERROR: memind index out of range!\n");
		return (NULL);
	}

	// get mem obj variable index start
	sp = stpopi ((U1 *) &ind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("get_obj_mem_type: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (ind < 0 || ind >= mem[memind].memsize)
	{
		// error
		printf ("get_obj_mem_type: ERROR: ind index out of range!\n");
		return (NULL);
	}

	mem_type = mem[memind].objptr[ind].type;

	sp = stpushi (mem_type, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("load_string_obj_mem_array: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}
