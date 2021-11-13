/*
 * This file stack.h is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2019
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


// stack operations ---------------------------------------
// byte

U1 *stpushb (U1 data, U1 *sp, U1 *sp_bottom)
{
	if (sp + 1 >= sp_bottom)
	{
		sp--;
		*sp = data;

		// push type
		sp--;
		*sp = STACK_BYTE;

		return (sp);		// success
	}
	else
	{
		// fatal ERROR: stack pointer can't go below address ZERO!
		return (NULL);		// FAIL
	}
}

U1 *stpopb (U1 *data, U1 *sp, U1 *sp_top)
{
	U1 type;

	if (sp - 1 <= sp_top)
	{
		// check if byte type
		type = *sp;
		if (type != STACK_BYTE)
		{
			printf ("ERROR: stack variable is not byte!\n");
			return (NULL);
		}
		else
		{
			sp++;

			*data = *sp;
			sp++;
		}
	}
	else
	{
		printf ("ERROR: stpopb stack corrupt!\n");
		return (NULL);
	}

	return (sp);			// success
}


// quadword

U1 *stpushi (S8 data, U1 *sp, U1 *sp_bottom)
{
	U1 *bptr;

	if (sp >= sp_bottom + 9)
	{
		bptr = (U1 *) &data;

		sp--;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;

		// push type
		*sp = STACK_INT;

		return (sp);			// success
	}
	else
	{
		// fatal ERROR: stack pointer can't go below address ZERO!
		return (NULL);			// FAIL
	}
}

U1 *stpopi (U1 *data, U1 *sp, U1 *sp_top)
{
	U1 type;

	if (sp >= sp_top - 8)
	{
		printf ("ERROR: stpopi stack corrupt!\n");
		return (NULL);
	}
	else
	{
		// check type
		type = *sp;
		if (type != STACK_INT)
		{
			printf ("ERROR: stack variable is not int64!\n");
			return (NULL);
		}
		else
		{
			sp++;

			data[7] = *sp++;
			data[6] = *sp++;
			data[5] = *sp++;
			data[4] = *sp++;
			data[3] = *sp++;
			data[2] = *sp++;
			data[1] = *sp++;
			data[0] = *sp++;
		}
	}

	return (sp);			// success
}


// double

U1 *stpushd (F8 data, U1 *sp, U1 *sp_bottom)
{
	U1 *bptr;

	if (sp >= sp_bottom + 9)
	{
		bptr = (U1 *) &data;

		sp--;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;

		// push type
		*sp = STACK_DOUBLE;

		return (sp);			// success
	}
	else
	{
		// fatal ERROR: stack pointer can't go below address ZERO!
		return (NULL);			// FAIL
	}
}

U1 *stpopd (U1 *data, U1 *sp, U1 *sp_top)
{
	U1 type;

	if (sp >= sp_top - 8)
	{
		printf ("ERROR: stpopd stack corrupt!\n");
		return (NULL);
	}
	else
	{
		// check type
		type = *sp;
		if (type != STACK_DOUBLE)
		{
			printf ("ERROR: stack variable is not double!\n");
			return (NULL);
		}
		else
		{
			sp++;

			data[7] = *sp++;
			data[6] = *sp++;
			data[5] = *sp++;
			data[4] = *sp++;
			data[3] = *sp++;
			data[2] = *sp++;
			data[1] = *sp++;
			data[0] = *sp++;
		}
	}

	return (sp);			// success
}
