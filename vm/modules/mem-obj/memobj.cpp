/*
 * This file mem.cpp is part of L1vm.
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


// arrays memory codes
#define MEMINT64	0
#define MEMDOUBLE   1

// structures

// allocated memory pointers union
union memptr
{
	S8 int64v;
	F8 doublev;
	S8 straddr;
};

struct obj
{
	union memptr memptr;
	U1 type;
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

	if (mem[memind].objptr != NULL)
	{
		free (mem[memind].objptr);
	}

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
		printf ("save_obj_mem: ERROR: object address index out of range!\n");
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
		printf ("save_obj_mem: ERROR: object index out of range!\n");
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
		printf ("load_obj_mem: ERROR: object index out of range!\n");
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
		printf ("load_obj_mem: ERROR: object index out of range!\n");
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
