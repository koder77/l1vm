/*
 * This file debugger.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2017
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

//  l1vm VM debugger
//
//

#include "../include/global.h"

#define REGI_SB "regi"
#define REGD_SB "regd"
#define REGI_EDIT_SB "regi ed"
#define REGD_EDIT_SB "regd ed"
#define EXIT_SB "exit"
#define CONT_SB "continue"
#define STACK_SB "stack"
#define HELP_SB "help"

// protos
size_t strlen_safe (const char *str, S8 maxlen);

U1 *db_stpopb (U1 *data, U1 *sp, const U1 *sp_top)
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

U1 *db_stpopi (U1 *data, U1 *sp, const U1 *sp_top)
{
	#if STACK_CHECK
	if (sp >= sp_top - 8)
	{
		// nothing on stack!! can't pop!!
		printf ("ERROR: nothing on stack!!!\n");
		return (NULL);			// FAIL
	}

	if (*sp != STACK_QUADWORD)
	{
		printf ("stpopi: FATAL ERROR! stack element not int64!\n");
		printf ("FOUND: %i\n", *sp);
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

U1 *db_stpopd (U1 *data, U1 *sp, const U1 *sp_top)
{
	#if STACK_CHECK
	if (sp >= sp_top - 8)
	{
		printf ("ERROR: nothing on stack!!!\n");
		// nothing on stack!! can't pop!!
		return (NULL);			// FAIL
	}

	if (*sp != STACK_DOUBLEFLOAT)
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

U1 *db_stack_type (U1 *data, U1 *sp, const U1 *sp_top)
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

void debugger_help (S2 cpu_core, S8 epos)
{
    printf ("\n\ncpu: %i, epos: %lli\n", cpu_core, epos);
    printf ("debugger\ncommands: 'regi': show int register, 'regd': show double register\n'stack': show stack\n");
    printf ("'regi ed': edit int register, 'regd ed': edit double register\nexit': exit program, 'continue': continue program\n\'help': show this help\n");
}

void read_string (U1 *str, S8 len)
{
    S2 ch;
	S8 i ALIGN = 0;

	while (1)
	{
        ch = getc (stdin);
        if (ch == 10)
        {
            if (i == 0)
            {
				str[0] = '\0';
			}
            break;
        }

		if (i < len - 1)
		{
			str[i] = ch;
			i++;
		}
	}
    str[i] = '\0';
}


S2 debugger (S8 *reg_int, F8 *reg_double, S8 epos, U1 *sp, U1 *sp_bottom, U1 *sp_top, S2 cpu_core)
{
    U1 run_loop = 1;
    S2 ret = 0;
    U1 command[80];
    S2 command_maxlen = 79;
    U1 reg_inp[4];
    S2 reg_maxlen = 4;
    S2 reg_num = 0;

    S2 numi_inputlen = 4095;
    S2 numd_inputlen = 4095;
    U1 numi[4096];
    U1 numd[4096];

    S2 slen = 0;
    U1 *dsp = sp;
    const U1 *dsp_top = sp_top;
    U1 stack_loop = 1;
    U1 stack_debug = 0;

    S8 regi ALIGN = 0;
    F8 regd ALIGN = 0.0;
    U1 stack_type = 0;

    debugger_help (cpu_core, epos);

    while (run_loop == 1)
    {
        // print prompt
        printf ("> ");

        // get input
        read_string (command, command_maxlen);

        if (strcmp (REGI_SB, (const char *) command) == 0)
        {
            printf ("register number? ");

            // get input
            read_string (reg_inp, reg_maxlen);

            reg_num = strtol ((char *) reg_inp, NULL, 10);
            if (reg_num < 0 || reg_num > 255)
            {
                printf ("error: register must be in range 0 - 255 !\n");
            }
            else
            {
                printf ("regi %i: %lli\n", reg_num, reg_int[reg_num]);
            }
        }

        if (strcmp (REGD_SB, (const char *) command) == 0)
        {
            printf ("register number? ");

            // get input
            read_string (reg_inp, reg_maxlen);

            reg_num = strtol ((char *) reg_inp, NULL, 10);
            if (reg_num < 0 || reg_num > 255)
            {
                printf ("error: register must be in range 0 - 255 !\n");
            }
            else
            {
                printf ("regd %i: %.10lf\n", reg_num, reg_double[reg_num]);
            }
        }

        if (strcmp (EXIT_SB, (const char *) command) == 0)
        {
            ret = 0;
            return (ret);
        }

        if (strcmp (CONT_SB, (const char *) command) == 0)
        {
            if (stack_debug == 1)
            {
                printf ("error: stack debugged: can't continue program!\n");
            }
            else
            {
                ret = 1;
                return (ret);
            }
        }

        if (strcmp (STACK_SB, (const char *) command) == 0)
        {
            printf ("stack top:\n");
            stack_loop = 1;

            while (stack_loop == 1)
            {
                // check if something is on stack
                if (dsp == dsp_top)
                {
                    printf ("stack at end!\n");
                    stack_loop = 0;
                    continue;
                }

                dsp = db_stack_type ((U1 *) &stack_type, dsp, dsp_top);
                if (dsp == NULL)
                {
                    printf ("ERROR: stack pointer error!\n");
                    stack_loop = 0;
                }

                switch (stack_type)
                {
                    case STACK_BYTE:
                        dsp = db_stpopb ((U1 *) &regi, dsp, dsp_top);
                        if (dsp == NULL)
                        {
                            printf ("ERROR: stack pointer error!\n");
                            stack_loop = 0;
                        }

                        printf ("stack: byte: %lli\n", regi);
                        break;

                    case STACK_QUADWORD:
                        dsp = db_stpopi ((U1 *) &regi, dsp, dsp_top);
                        if (dsp == NULL)
                        {
                            printf ("ERROR: stack pointer error!\n");
                            stack_loop = 0;
                        }

                        printf ("stack: int64: %lli\n", regi);
                        break;

                   case STACK_DOUBLEFLOAT:
                        dsp = db_stpopd ((U1 *) &regd, dsp, dsp_top);
                        if (dsp == NULL)
                        {
                            printf ("ERROR: stack pointer error!\n");
                            stack_loop = 0;
                        }

                        printf ("stack: double: %.10lf\n", regd);
                        break;
                }
            }
            stack_debug = 1; // set flag, can't continue if set!!!
        }

        if (strcmp (HELP_SB, (const char *) command) == 0)
        {
            debugger_help (cpu_core, epos);
        }

        if (strcmp (REGI_EDIT_SB, (const char *) command) == 0)
        {
            printf ("register number? ");

            // get input
            read_string (reg_inp, reg_maxlen);

            reg_num = strtol ((char *) reg_inp, NULL, 10);
            if (reg_num < 0 || reg_num > 255)
            {
                printf ("error: register must be in range 0 - 255 !\n");
            }
            else
            {
                // get input
                printf ("value? ");
                read_string (numi, numi_inputlen);

                slen = strlen_safe ((const char *) numi, numi_inputlen);
                numi[slen - 1] = '\0';
                sscanf ((const char *) numi, "%lli", &regi);

                reg_int[reg_num] = regi;
                printf ("regi %i: %lli\n", reg_num, reg_int[reg_num]);
            }
        }

        if (strcmp (REGD_EDIT_SB, (const char *) command) == 0)
        {
            printf ("register number? ");

            // get input
            read_string (reg_inp, reg_maxlen);

            reg_num = strtol ((char *) reg_inp, NULL, 10);
            if (reg_num < 0 || reg_num > 255)
            {
                printf ("error: register must be in range 0 - 255 !\n");
            }
            else
            {
                // get input
                printf ("value? ");
                read_string (numd, numd_inputlen);
                sscanf ((const char *) numd, "%lf", &regd);

                reg_double[reg_num] = regd;
                printf ("regd %i: %.10lf\n", reg_num, reg_double[reg_num]);
            }
        }
    }
    return (ret);
}
