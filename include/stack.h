/*
 * This file stack.h is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (info@midnight-coding.de), 2019
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

#include <string.h>

// stack operations ---------------------------------------
// byte

U1 *stpushb (U1 data, U1 *sp, U1 *sp_bottom)
{
	#if STACK_CHECK
	if (sp + 1 >= sp_bottom)
	{
		sp--;
		*sp-- = data;

		*sp = STACK_BYTE;

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

	if (*sp != STACK_BYTE)
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
U1 *stpushi(S8 data, U1 *sp, U1 *sp_bottom)
{
#if STACK_CHECK
    if (sp < sp_bottom + 9) {
        return NULL; // fatal ERROR: stack overflow
    }

    sp -= 9;
    *sp = STACK_QUADWORD;        // Tag an die Basis schreiben
    memcpy(sp + 1, &data, 8);   // 8 Bytes als Block kopieren

    return sp;
#else
    if (sp < sp_bottom + 8) {
        return NULL;
    }

    sp -= 8;
    memcpy(sp, &data, 8);       // Inlined zu 1x SIMD/Register store

    return sp;
#endif
}

U1 *stpopi(U1 *data, U1 *sp, U1 *sp_top)
{
#if STACK_CHECK
    if (sp >= sp_top - 8) {
        printf("ERROR: nothing on stack!!!\n");
        return NULL;
    }

    if (*sp != STACK_QUADWORD) {
        printf("stpopi: FATAL ERROR! stack element not int64!\n");
        printf("FOUND: %i\n", *sp);
        return NULL;
    }

    sp++;
    memcpy(data, sp, 8);        // 8 Bytes als Block lesen

    return sp + 8;
#else
    if (sp >= sp_top - 7) {
        return NULL;
    }

    memcpy(data, sp, 8);

    return sp + 8;
#endif
}


// double
U1 *stpushd(F8 data, U1 *sp, U1 *sp_bottom)
{
#if STACK_CHECK
    if (sp < sp_bottom + 9) {
        return NULL; // fatal ERROR: stack overflow
    }

    sp -= 9;
    *sp = STACK_DOUBLEFLOAT;        // Tag an die Basis schreiben
    memcpy(sp + 1, &data, 8);   // 8 Bytes als Block kopieren

    return sp;
#else
    if (sp < sp_bottom + 8) {
        return NULL;
    }

    sp -= 8;
    memcpy(sp, &data, 8);       // Inlined zu 1x SIMD/Register store

    return sp;
#endif
}

U1 *stpopd(U1 *data, U1 *sp, U1 *sp_top)
{
#if STACK_CHECK
    if (sp >= sp_top - 8) {
        printf("ERROR: nothing on stack!!!\n");
        return NULL;
    }

    if (*sp != STACK_DOUBLEFLOAT) {
        printf("stpopi: FATAL ERROR! stack element not int64!\n");
        printf("FOUND: %i\n", *sp);
        return NULL;
    }

    sp++;
    memcpy(data, sp, 8);        // 8 Bytes als Block lesen

    return sp + 8;
#else
    if (sp >= sp_top - 7) {
        return NULL;
    }

    memcpy(data, sp, 8);

    return sp + 8;
#endif
}


U1 *stack_type (U1 *data, U1 *sp, U1 *sp_top)
{
	if (sp + 1 > sp_top)
	{
		printf ("ERROR: stack_type: nothing on stack!!!\n");
		// nothing on stack!! can't pop!!
		return (NULL);		// FAIL
	}

	*data = *sp;
	return (sp);
}
