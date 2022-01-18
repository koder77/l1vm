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
	#if STACK_CHECK
	if (sp + 1 >= sp_bottom)
	{
		sp--;
		*sp = data;

		sp--;
		*sp-- = BYTE;

		return (sp);		// success
	}
	else
	{
		// fatal ERROR: stack pointer can't go below address ZERO!
		return (NULL);		// FAIL
	}
	#else
	if (sp >= sp_bottom)
	{
		sp--;

		*sp = data;

		return (sp);		// success
	}
	else
	{
		// fatal ERROR: stack pointer can't go below address ZERO!
		return (NULL);		// FAIL
	}
	#endif
}

U1 *stpopb (U1 *data, U1 *sp, U1 *sp_top)
{
	#if STACK_CHECK
	if (sp + 1 > sp_top)
	{
		printf ("ERROR: nothing on stack!!!\n");
		// nothing on stack!! can't pop!!
		return (NULL);		// FAIL
	}

	sp++;
	if (*sp != BYTE)
	{
		printf ("stpopb: FATAL ERROR! stack element not byte!\n");
		return (NULL);
	}
	sp++;

	*data = *sp;

	sp++;
	return (sp);			// success
	#else
	if (sp == sp_top)
	{
		// nothing on stack!! can't pop!!
		return (NULL);		// FAIL
	}

	*data = *sp;

	sp++;
	return (sp);			// success
	#endif
}


// quadword

U1 *stpushi (S8 data, U1 *sp, U1 *sp_bottom)
{
	U1 *bptr;

	#if STACK_CHECK
	if (sp >= sp_bottom + 9)
	{
		// set stack pointer to lower address

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
		*sp-- = QUADWORD;

		return (sp);			// success
	}
	else
	{
		// fatal ERROR: stack pointer can't go below address ZERO!
		return (NULL);			// FAIL
	}
	#else
	if (sp >= sp_bottom + 8)
	{
		// set stack pointer to lower address

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
		*sp = *bptr;

		return (sp);			// success
	}
	else
	{
		// fatal ERROR: stack pointer can't go below address ZERO!
		return (NULL);			// FAIL
	}
	#endif
}

U1 *stpopi (U1 *data, U1 *sp, U1 *sp_top)
{
	#if STACK_CHECK 
	if (sp >= sp_top - 8)
	{
		// nothing on stack!! can't pop!!
		printf ("ERROR: nothing on stack!!!\n");
		return (NULL);			// FAIL
	}

	sp++;
	if (*sp != QUADWORD)
	{
		printf ("stpopi: FATAL ERROR! stack element not int64!\n");
		return (NULL);
	}
	sp++;

	data[7] = *sp++;
	data[6] = *sp++;
	data[5] = *sp++;
	data[4] = *sp++;
	data[3] = *sp++;
	data[2] = *sp++;
	data[1] = *sp++;
	data[0] = *sp++;

	return (sp);			// success
	#else
	if (sp >= sp_top - 7)
	{
		// nothing on stack!! can't pop!!
		return (NULL);			// FAIL
	}

	data[7] = *sp++;
	data[6] = *sp++;
	data[5] = *sp++;
	data[4] = *sp++;
	data[3] = *sp++;
	data[2] = *sp++;
	data[1] = *sp++;
	data[0] = *sp++;

	return (sp);			// success
	#endif
}

U1 *stpushd (F8 data, U1 *sp, U1 *sp_bottom)
{
	U1 *bptr;

	#if STACK_CHECK 
	if (sp >= sp_bottom + 9)
	{
		// set stack pointer to lower address

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
		*sp-- = DOUBLEFLOAT;

		return (sp);			// success
	}
	else
	{
		// fatal ERROR: stack pointer can't go below address ZERO!
		return (NULL);			// FAIL
	}
	#else 
	if (sp >= sp_bottom + 8)
	{
		// set stack pointer to lower address

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
		*sp = *bptr;

		return (sp);			// success
	}
	else
	{
		// fatal ERROR: stack pointer can't go below address ZERO!
		return (NULL);			// FAIL
	}
	#endif
}

U1 *stpopd (U1 *data, U1 *sp, U1 *sp_top)
{
	#if STACK_CHECK
	if (sp >= sp_top - 8)
	{
		printf ("ERROR: nothing on stack!!!\n");
		// nothing on stack!! can't pop!!
		return (NULL);			// FAIL
	}

	sp++;
	if (*sp != DOUBLEFLOAT)
	{
		printf ("stpopd: FATAL ERROR! stack element not double!\n");
		return (NULL);
	}
	sp++;

	data[7] = *sp++;
	data[6] = *sp++;
	data[5] = *sp++;
	data[4] = *sp++;
	data[3] = *sp++;
	data[2] = *sp++;
	data[1] = *sp++;
	data[0] = *sp++;

	return (sp);			// success
	#else
	if (sp >= sp_top - 7)
	{
		// nothing on stack!! can't pop!!
		return (NULL);			// FAIL
	}

	data[7] = *sp++;
	data[6] = *sp++;
	data[5] = *sp++;
	data[4] = *sp++;
	data[3] = *sp++;
	data[2] = *sp++;
	data[1] = *sp++;
	data[0] = *sp++;

	return (sp);			// success
	#endif
}
