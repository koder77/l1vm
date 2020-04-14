/*
 * This file mem.c is part of L1vm.
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

// allocate array memory and access it
// for dynamic allocation of memory


#include "../../../include/global.h"
#include "../../../include/stack.h"


#define MEMBYTE		0
#define MEMINT16	1
#define MEMINT32	2
#define MEMINT64	3
#define MEMDOUBLE	4


// structures

// allocated memory pointers union
union memptr 
{
	U1 *byteptr;
	S2 *int16ptr;
	S4 *int32ptr;
	S8 *int64ptr;
	F8 *doubleptr;
};

// the memory info structure
struct mem
{
	U1 used;
	U1 type;
	S8 memsize ALIGN;
	union memptr memptr;	// the memory pointers to save the allocations
};

static struct mem *mem = NULL;
static S8 memmax ALIGN = 0;


// mem functions ==============================================================

U1 *init_mem (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
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

U1 *free_mem (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	if (mem) free (mem);
	return (sp);
}
	
S8 get_free_mem (void)
{
	S8 i ALIGN;
	
	for (i = 0; i < memmax; i++)
	{
		if (mem[i].used == 0)
		{
			return (i);
		}
	}
	
	// no free memory found
	return (-1);
}

U1 *alloc_byte (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 memsize ALIGN = 0;
	U1 *memaddr = NULL;
	
	// memory struct index
	S8 memind ALIGN;
	
	sp = stpopi ((U1 *) &memsize, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("alloc_byte: ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	memind = get_free_mem ();
	if (memind == -1)
	{
		printf ("alloc_byte: no more memory slot free!\n");
		return (NULL);
	}
	
	memaddr = calloc (memsize, sizeof (S8));
	if (memaddr == NULL)
	{
		printf ("alloc_byte: out of memory: %lli of size int64!\n", memsize);
		
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("alloc_byte: ERROR: stack corrupt!\n");
			return (NULL);
		}
	}
	
	// save memory pointer
	mem[memind].memptr.byteptr = memaddr;
	mem[memind].used = 1;
	mem[memind].type = MEMBYTE;
	mem[memind].memsize = memsize;
	
	// push memory structure handle index
	sp = stpushi (memind, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("alloc_byte: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *alloc_int16 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 memsize ALIGN = 0;
	S2 *memaddr = NULL;
	
	// memory struct index
	S8 memind ALIGN;
	
	sp = stpopi ((U1 *) &memsize, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("alloc_int16: ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	memind = get_free_mem ();
	if (memind == -1)
	{
		printf ("alloc_int16: no more memory slot free!\n");
		return (NULL);
	}
	
	memaddr = calloc (memsize, sizeof (S8));
	if (memaddr == NULL)
	{
		printf ("alloc_int16: out of memory: %lli of size int64!\n", memsize);
		
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("alloc_int16: ERROR: stack corrupt!\n");
			return (NULL);
		}
	}
	
	// save memory pointer
	mem[memind].memptr.int16ptr = memaddr;
	mem[memind].used = 1;
	mem[memind].type = MEMINT16;
	mem[memind].memsize = memsize;
	
	// push memory structure handle index
	sp = stpushi (memind, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("alloc_int16: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *alloc_int32 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 memsize ALIGN = 0;
	S4 *memaddr = NULL;
	
	// memory struct index
	S8 memind ALIGN;
	
	sp = stpopi ((U1 *) &memsize, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("alloc_int32: ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	memind = get_free_mem ();
	if (memind == -1)
	{
		printf ("alloc_int32: no more memory slot free!\n");
		return (NULL);
	}
	
	memaddr = calloc (memsize, sizeof (S8));
	if (memaddr == NULL)
	{
		printf ("alloc_int32: out of memory: %lli of size int64!\n", memsize);
		
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("alloc_int32: ERROR: stack corrupt!\n");
			return (NULL);
		}
	}
	
	// save memory pointer
	mem[memind].memptr.int32ptr = memaddr;
	mem[memind].used = 1;
	mem[memind].type = MEMINT32;
	mem[memind].memsize = memsize;
	
	// push memory structure handle index
	sp = stpushi (memind, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("alloc_int32: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *alloc_int64 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 memsize ALIGN = 0;
	S8 *memaddr ALIGN = NULL;
	
	// memory struct index
	S8 memind ALIGN;
	
	sp = stpopi ((U1 *) &memsize, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("alloc_int64: ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	memind = get_free_mem ();
	if (memind == -1)
	{
		printf ("alloc_int64: no more memory slot free!\n");
		return (NULL);
	}
	
	memaddr = calloc (memsize, sizeof (S8));
	if (memaddr == NULL)
	{
		printf ("alloc_int64: out of memory: %lli of size int64!\n", memsize);
		
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("alloc_int64: ERROR: stack corrupt!\n");
			return (NULL);
		}
	}
	
	// save memory pointer
	mem[memind].memptr.int64ptr = memaddr;
	mem[memind].used = 1;
	mem[memind].type = MEMINT64;
	mem[memind].memsize = memsize;
	
	// push memory structure handle index
	sp = stpushi (memind, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("alloc_int64: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *alloc_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 memsize ALIGN = 0;
	F8 *memaddr ALIGN = NULL;
	
	// memory struct index
	S8 memind ALIGN;
	
	sp = stpopi ((U1 *) &memsize, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("alloc_double: ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	memind = get_free_mem ();
	if (memind == -1)
	{
		printf ("alloc_double: no more memory slot free!\n");
		return (NULL);
	}
	
	memaddr = calloc (memsize, sizeof (S8));
	if (memaddr == NULL)
	{
		printf ("alloc_double: out of memory: %lli of size int64!\n", memsize);
		
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("alloc_double: ERROR: stack corrupt!\n");
			return (NULL);
		}
	}
	
	// save memory pointer
	mem[memind].memptr.doubleptr = memaddr;
	mem[memind].used = 1;
	mem[memind].type = MEMDOUBLE;
	mem[memind].memsize = memsize;
	
	// push memory structure handle index
	sp = stpushi (memind, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("alloc_double: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

// dealloc mem ----------------------------------------------------------------

U1 *dealloc_mem (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 memind ALIGN = 0;
	
	sp = stpopi ((U1 *) &memind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("dealloc_mem: ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	// do sane check
	if (memind < 0 || memind >= memmax)
	{
		printf ("dealloc_mem: ERROR: memory index out of range!\n");
		return (NULL);
	}
	
	// free memory if allocated and set to used == 1
	switch (mem[memind].type)
	{
		case MEMBYTE:
			if (mem[memind].memptr.byteptr && mem[memind].used == 1)
			{
				free (mem[memind].memptr.byteptr);
			}
			else
			{
				printf ("dealloc_mem: ERROR byte mem not allocated!\n");
				return (NULL);
			}
			break;
			
		case MEMINT16:
			if (mem[memind].memptr.int16ptr && mem[memind].used == 1)
			{
				free (mem[memind].memptr.int16ptr);
			}
			else
			{
				printf ("dealloc_mem: ERROR int16 mem not allocated!\n");
				return (NULL);
			}
			break;
			
		case MEMINT32:
			if (mem[memind].memptr.int32ptr && mem[memind].used == 1)
			{
				free (mem[memind].memptr.int32ptr);
			}
			else
			{
				printf ("dealloc_mem: ERROR int32 mem not allocated!\n");
				return (NULL);
			}
			break;
			
		case MEMINT64:
			if (mem[memind].memptr.int64ptr && mem[memind].used == 1)
			{
				free (mem[memind].memptr.int64ptr);
			}
			else
			{
				printf ("dealloc_mem: ERROR int64 mem not allocated!\n");
				return (NULL);
			}
			break;
			
		case MEMDOUBLE:
			if (mem[memind].memptr.doubleptr && mem[memind].used == 1)
			{
				free (mem[memind].memptr.doubleptr);
			}
			else
			{
				printf ("dealloc_mem: ERROR double mem not allocated!\n");
				return (NULL);
			}
			break;
	}
	mem[memind].used = 0;
	return (sp);
}


// access array functions int =================================================

U1 *int_to_array (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// assign int to array
	
	S8 memind ALIGN = 0;
	S8 arrayind ALIGN = 0;
	S8 value ALIGN;
	
	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("int_to_array: ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	sp = stpopi ((U1 *) &arrayind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("int_to_array: ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	sp = stpopi ((U1 *) &memind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("int_to_array: ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	#if BOUNDSCHECK
	if (memind < 0 || memind >= memmax)
	{
		printf ("int_to_array: ERROR: memory index out of range!\n");
		return (NULL);
	}
	
	if (arrayind < 0 || arrayind >= mem[memind].memsize)
	{
		printf ("int_to_array: ERROR: array index out of range!\n");
		return (NULL);
	}
	#endif
	
	// assign to array 
	switch (mem[memind].type)
	{
		case MEMBYTE:
			mem[memind].memptr.byteptr[arrayind] = value;
			break;
			
		case MEMINT16:
			mem[memind].memptr.int16ptr[arrayind] = value;
			break;
			
		case MEMINT32:
			mem[memind].memptr.int32ptr[arrayind] = value;
			break;
			
		case MEMINT64:
			mem[memind].memptr.int64ptr[arrayind] = value;
			break;
	}
	return (sp);
}

U1 *array_to_int (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// assign int array to int 
	
	S8 memind ALIGN = 0;
	S8 arrayind ALIGN = 0;
	S8 value ALIGN;
	
	sp = stpopi ((U1 *) &arrayind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("array_to_int: ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	sp = stpopi ((U1 *) &memind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("array_to_int: ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	#if BOUNDSCHECK
	if (memind < 0 || memind >= memmax)
	{
		printf ("array_to_int: ERROR: memory index out of range!\n");
		return (NULL);
	}
	
	if (arrayind < 0 || arrayind >= mem[memind].memsize)
	{
		printf ("array_to_int: ERROR: array index out of range!\n");
		return (NULL);
	}
	#endif
	
	// assign to variable
	switch (mem[memind].type)
	{
		case MEMBYTE:
			value = mem[memind].memptr.byteptr[arrayind];
			break;
			
		case MEMINT16:
			value = mem[memind].memptr.int16ptr[arrayind];
			break;
			
		case MEMINT32:
			value = mem[memind].memptr.int32ptr[arrayind];
			break;
			
		case MEMINT64:
			value = mem[memind].memptr.int64ptr[arrayind];
			break;
	}
	
	sp = stpushi (value, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("array_to_int: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

// access array functions double ==============================================

U1 *double_to_array (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// assign double to double array
	
	S8 memind ALIGN = 0;
	S8 arrayind ALIGN = 0;
	F8 value ALIGN;
	
	sp = stpopd ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("double_to_array: ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	sp = stpopi ((U1 *) &arrayind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("double_to_array: ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	sp = stpopi ((U1 *) &memind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("double_to_array: ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	#if BOUNDSCHECK
	if (memind < 0 || memind >= memmax)
	{
		printf ("double_to_array: ERROR: memory index out of range!\n");
		return (NULL);
	}
	
	if (arrayind < 0 || arrayind >= mem[memind].memsize)
	{
		printf ("double_to_array: ERROR: array index out of range!\n");
		return (NULL);
	}
	#endif
	
	// assign to array 
	mem[memind].memptr.doubleptr[arrayind] = value;
	return (sp);
}

U1 *array_to_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// assign double array to double
	
	S8 memind ALIGN = 0;
	S8 arrayind ALIGN = 0;
	F8 value ALIGN;
	
	sp = stpopi ((U1 *) &arrayind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("array_to_double: ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	sp = stpopi ((U1 *) &memind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("array_to_double: ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	#if BOUNDSCHECK
	if (memind < 0 || memind >= memmax)
	{
		printf ("array_to_double: ERROR: memory index out of range!\n");
		return (NULL);
	}
	
	if (arrayind < 0 || arrayind >= mem[memind].memsize)
	{
		printf ("array_to_double: ERROR: array index out of range!\n");
		return (NULL);
	}
	#endif
	
	// assign to variable
	value = mem[memind].memptr.doubleptr[arrayind];
	
	sp = stpushd (value, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("array_to_double: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}
