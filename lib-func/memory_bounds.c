/*
 * This file memory_bounds.c is part of L1vm.
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

#include "../include/global.h"


extern struct data_info data_info[MAXDATAINFO];
extern S8 data_info_ind;


// memory bounds checking function
S2 memory_bounds (S8 start, S8 offset_access)
{
	S8 i ALIGN;

	if (start + offset_access < 0 || offset_access < 0)
	{
		// access ERROR!
		printf ("memory_bounds: FATAL ERROR: address: %lli, offset: %lli below zero!\n", start, offset_access);
		return (1);
	}

	for (i = 0; i <= data_info_ind; i++)
	{
		if ((start >= data_info[i].offset) && (start + offset_access <= data_info[i].end))
		{
			if (offset_access == 0)
			{
				// all ok, return 0
				return (0);
			}
			else
			{
				switch (data_info[i].type)
				{
					case BYTE:
						// range already checked on top if
						// all ok, return 0
						return (0);
						break;

					case WORD:
						if (offset_access % sizeof (S2) != 0)
						{
						printf ("memory_bounds: FATAL ERROR: variable access not on word bound, address: %lli, offset: %lli!\n", start, offset_access);
							return (1);
						}
						return (0);
						break;

					case DOUBLEWORD:
						if (offset_access % sizeof (S4) != 0)
						{
							printf ("memory_bounds: FATAL ERROR: variable access not on double word bound, address: %lli, offset: %lli!\n", start, offset_access);
							return (1);
						}
						return (0);
						break;

					case QUADWORD:
					case DOUBLEFLOAT:
						if (offset_access % sizeof (S8) != 0)
						{
							printf ("memory_bounds: FATAL ERROR: variable access not on quad word/double float bound, address: %lli, offset: %lli!\n", start, offset_access);
							return (1);
						}
						return (0);
						break;
				}
			}
		}
	}
	printf ("memory_bounds: FATAL ERROR: variable not found overflow address: %lli, offset: %lli!\n", start, offset_access);
	return (1);
}

// memory size get function
S2 memory_size (S8 start)
{
	S8 i ALIGN;
	S8 size ALIGN = 0;

	if (start < 0)
	{
		// access ERROR!
		printf ("memory_size: FATAL ERROR: address: %lli below zero!\n", start);
		return (size);
	}

	for (i = 0; i <= data_info_ind; i++)
	{
		if ((start >= data_info[i].offset) && (start <= data_info[i].end))
		{
			// calculate variable size in bytes
			size =  data_info[i].end - data_info[i].offset + 1;

			printf ("pointer address: %lli, start: %lli, end: %lli, size : %lli\n", start, data_info[i].offset, data_info[i].end, size);

			return (size); // all OK, variable pointer found!
		}
	}
	return (size);  // return 0, as ERROR variable not found!
}

// memory bounds checking function
S2 pointer_check (S8 start, S8 pointer_type)
{
	S8 i ALIGN;

	for (i = 0; i <= data_info_ind; i++)
	{
		if ((start >= data_info[i].offset) && (start <= data_info[i].end))
		{
			if (data_info[i].type != pointer_type)
			{
				printf ("pointer_check: FATAL ERROR: pointer address: %lli not of matching type!\n", start);
				return (1);
			}
			else
			{
				return (0); // all ok!
			}
		}
	}
	printf ("pointer_check: FATAL ERROR: variable not found overflow address: %lli !\n", start);
	return (1);
}

// get variable type from pointer var
S2 pointer_type (S8 start)
{
	S8 i ALIGN;

	for (i = 0; i <= data_info_ind; i++)
	{
		if ((start >= data_info[i].offset) && (start <= data_info[i].end))
		{
			return (data_info[i].type);
		}
	}
	return (0); // error pointer variable address not found!
}
