/*
 * This file main.c is part of L1vm.
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
#define EXIT_SB "exit"
#define CONT_SB "continue"

// protos
size_t strlen_safe (const char *str, S8 maxlen);

S2 debugger (S8 *reg_int, F8 *reg_double, S8 epos)
{
    U1 run_loop = 1;
    S2 ret = 0;
    U1 command[80];
    S2 command_maxlen = 79;
    U1 reg_inp[4];
    S2 reg_maxlen = 3;
    S2 reg_num = 0;
    S2 slen = 0;

    while (run_loop == 1)
    {
        printf ("\n\nepos: %lli\n", epos);
        printf ("debugger\ncommands: 'regi': show int register, 'regd': show double register\n'exit': exit program, 'continue': continue program\n");
        // get input
        if (fgets ((char *) command, command_maxlen, stdin) == NULL)
		{
            // error
            printf ("error: can't read input!\n");
            return (ret);
        }

        slen = strlen_safe ((const char *) command, command_maxlen);
        command[slen - 1] = '\0';

        if (strcmp (REGI_SB, (const char *) command) == 0)
        {
            printf ("register number? ");

            // get input
            if (fgets ((char *) reg_inp, command_maxlen, stdin) == NULL)
            {
                // error
                printf ("error: can't read input!\n");
                return (ret);
            }

            slen = strlen_safe ((const char *) reg_inp, reg_maxlen);
            reg_inp[slen - 1] = '\0';

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
            if (fgets ((char *) reg_inp, command_maxlen, stdin) == NULL)
            {
                // error
                printf ("error: can't read input!\n");
                return (ret);
            }

            slen = strlen_safe ((const char *) reg_inp, reg_maxlen);
            reg_inp[slen - 1] = '\0';

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
            run_loop = 0;
        }

        if (strcmp (CONT_SB, (const char *) command) == 0)
        {
            ret = 1;
            run_loop = 0;
        }
    }
    return (ret);
}
