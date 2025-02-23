/*
 * This file debugger.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (info@midnight-coding.de), 2024
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
// stack
U1 *stpushb (U1 data, U1 *sp, U1 *sp_bottom);
U1 *stpopb (U1 *data, U1 *sp, U1 *sp_top);
U1 *stpushi (S8 data, U1 *sp, U1 *sp_bottom);
U1 *stpopi (U1 *data, U1 *sp, U1 *sp_top);
U1 *stpushd (F8 data, U1 *sp, U1 *sp_bottom);
U1 *stpopd (U1 *data, U1 *sp, U1 *sp_top);
U1 *stack_type (U1 *data, U1 *sp, U1 *sp_top);

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
    U1 *sp_save = sp;
    const U1 *dsp_top = sp_top;
    U1 stack_loop = 1;
    U1 command_ok = 0;

    S8 regi ALIGN = 0;
    F8 regd ALIGN = 0.0;
    U1 st_type = 0;

    debugger_help (cpu_core, epos);

    while (run_loop == 1)
    {
        command_ok = 0;   // set to 1 if user input matches a command

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

            command_ok = 1;
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

            command_ok = 1;
        }

        if (strcmp (EXIT_SB, (const char *) command) == 0)
        {
            ret = 0;
            return (ret);
        }

        if (strcmp (CONT_SB, (const char *) command) == 0)
        {
            ret = 1;
            return (ret);
        }

        if (strcmp (STACK_SB, (const char *) command) == 0)
        {
            printf ("stack top:\n");
            dsp = sp_save;

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

                dsp = stack_type ((U1 *) &st_type, dsp, dsp_top);
                if (dsp == NULL)
                {
                    printf ("ERROR: stack pointer error!\n");
                    stack_loop = 0;
                }

                switch (st_type)
                {
                    case STACK_BYTE:
                        dsp = stpopb ((U1 *) &regi, dsp, dsp_top);
                        if (dsp == NULL)
                        {
                            printf ("ERROR: stack pointer error!\n");
                            stack_loop = 0;
                        }

                        printf ("stack: byte: %lli\n", regi);
                        break;

                    case STACK_QUADWORD:
                        dsp = stpopi ((U1 *) &regi, dsp, dsp_top);
                        if (dsp == NULL)
                        {
                            printf ("ERROR: stack pointer error!\n");
                            stack_loop = 0;
                        }

                        printf ("stack: int64: %lli\n", regi);
                        break;

                   case STACK_DOUBLEFLOAT:
                        dsp = stpopd ((U1 *) &regd, dsp, dsp_top);
                        if (dsp == NULL)
                        {
                            printf ("ERROR: stack pointer error!\n");
                            stack_loop = 0;
                        }

                        printf ("stack: double: %.10lf\n", regd);
                        break;
                }
            }
            command_ok = 1;
        }

        if (strcmp (HELP_SB, (const char *) command) == 0)
        {
            debugger_help (cpu_core, epos);
            command_ok = 1;
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

            command_ok = 1;
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

            command_ok = 1;
        }

        if (command_ok == 0)
        {
            printf ("error: unknown command!\n");
        }
    }
    sp = sp_save;
    return (ret);
}
