/*
 * This file register.c is part of L1vm.
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

extern U1 regi[MAXREG][MAXSTRLEN];
extern U1 regd[MAXREG][MAXSTRLEN];


// register tracking functions
void set_regi (S4 reg, U1 *name)
{
	strcpy ((char *) regi[reg], (const char *) name);
}

void set_regd (S4 reg, U1 *name)
{
	strcpy ((char *) regd[reg], (const char *) name);
}

S4 get_regi (U1 *name)
{
	S4 i;

	for (i = 0; i < MAXREG; i++)
	{
		if (strcmp ((const char *) regi[i], (const char *) name) == 0)
		{
			return (i);
		}
	}
	return (-1);
}

S4 get_regd (U1 *name)
{
	S4 i;

	for (i = 0; i < MAXREG; i++)
	{
		if (strcmp ((const char *) regd[i], (const char *) name) == 0)
		{
			return (i);
		}
	}
	return (-1);
}

void init_registers (void)
{
	S4 i;

	for (i = 0; i < MAXREG; i++)
	{
		set_regi (i, (U1 *) "");
	}

	for (i = 0; i < MAXREG; i++)
	{
		set_regd (i, (U1 *) "");
	}
}

S4 get_free_regi (void)
{
	S4 i;

	for (i = 1; i < MAXREG; i++)
	{
		if (strcmp ((const char *) regi[i], "") == 0)
		{
			return (i);
		}
	}

	return (-1);
}

S4 get_free_regd (void)
{
	S4 i;

	for (i = 1; i < MAXREG; i++)
	{
		if (strcmp ((const char *) regd[i], "") == 0)
		{
			return (i);
		}
	}

	return (-1);
}
