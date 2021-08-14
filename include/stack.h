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

#if STACK_CHECK
extern U1 stack_types[MAX_STACK_TYPES];
extern S8 stack_types_ind ALIGN;
#endi

U1 *stpushb (U1 data, U1 *sp, U1 *sp_bottom)
{
	if (sp >= sp_bottom)
	{
		sp--;

		*sp = data;

		#f STACK_CHECK
			if (stack_types_ind < MAX_STACK_TYPES - 1)
			{
				stack_types_ind++;
				stack_types[stack_types_ind] = BYTE;
			}
			else
			{
				// ERROR stack_types array full!
				printf ("FATAL ERROR: stack_types overflow: %lli too low!\n", stack_types_ind);
				PRINT_EPOS();
				free (jumpoffs);
				pthread_exit ((void *) 1);
			}
		#endif

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
	if (sp == sp_top)
	{
		// nothing on stack!! can't pop!!
		return (NULL);		// FAIL
	}

	*data = *sp;

	#if STACK_CHECK
		if (stack_type[stack_types_ind] != BYTE)
		{
			// ERROR stack_types array full!
			printf ("FATAL ERROR: stack type is not byte!\n");
			PRINT_EPOS();
			free (jumpoffs);
			pthread_exit ((void *) 1);
		}
		if (stack_types_ind >= 0)
		{
			stack_types_ind--;
		}
		else
		{
			// ERROR stack_types array full!
			printf ("FATAL ERROR: stack type: stack corrupt!\n");
			PRINT_EPOS();
			free (jumpoffs);
			pthread_exit ((void *) 1);
		}
	#endif

	sp++;
	return (sp);			// success
}


// quadword

U1 *stpushi (S8 data, U1 *sp, U1 *sp_bottom)
{
	U1 *bptr;

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

		#f STACK_CHECK
			if (stack_types_ind < MAX_STACK_TYPES - 1)
			{
				stack_types_ind++;
				stack_types[stack_types_ind] = QUADWORD;
			}
			else
			{
				// ERROR stack_types array full!
				printf ("FATAL ERROR: stack_types overflow: %lli too low!\n", stack_types_ind);
				PRINT_EPOS();
				free (jumpoffs);
				pthread_exit ((void *) 1);
			}
		#endif

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
	if (sp == sp_top)
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

	#if STACK_CHECK
		if (stack_type[stack_types_ind] != QUADWORD)
		{
			// ERROR stack_types array full!
			printf ("FATAL ERROR: stack type is not int64!\n");
			PRINT_EPOS();
			free (jumpoffs);
			pthread_exit ((void *) 1);
		}
		if (stack_types_ind >= 0)
		{
			stack_types_ind--;
		}
		else
		{
			// ERROR stack_types array full!
			printf ("FATAL ERROR: stack type: stack corrupt!\n");
			PRINT_EPOS();
			free (jumpoffs);
			pthread_exit ((void *) 1);
		}
	#endif

	return (sp);			// success
}

U1 *stpushd (F8 data, U1 *sp, U1 *sp_bottom)
{
	U1 *bptr;

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

		#f STACK_CHECK
			if (stack_types_ind < MAX_STACK_TYPES - 1)
			{
				stack_types_ind++;
				stack_types[stack_types_ind] = DOUBLEFLOAT;
			}
			else
			{
				// ERROR stack_types array full!
				printf ("FATAL ERROR: stack_types overflow: %lli too low!\n", stack_types_ind);
				PRINT_EPOS();
				free (jumpoffs);
				pthread_exit ((void *) 1);
			}
		#endif

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
	if (sp == sp_top)
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

	#if STACK_CHECK
		if (stack_type[stack_types_ind] != DOUBLEFLOAT)
		{
			// ERROR stack_types array full!
			printf ("FATAL ERROR: stack type is not double!\n");
			PRINT_EPOS();
			free (jumpoffs);
			pthread_exit ((void *) 1);
		}
		if (stack_types_ind >= 0)
		{
			stack_types_ind--;
		}
		else
		{
			// ERROR stack_types array full!
			printf ("FATAL ERROR: stack type: stack corrupt!\n");
			PRINT_EPOS();
			free (jumpoffs);
			pthread_exit ((void *) 1);
		}
	#endif

	return (sp);			// success
}
