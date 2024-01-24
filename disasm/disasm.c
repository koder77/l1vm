/*
 * This file disasm.c is part of L1vm.
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

//  l1vm RISC VM disassembler
//
//

#include "../include/global.h"
#include "../include/stack.h"
#include "main.h"

S8 data_size ALIGN;
S8 code_size ALIGN;

// see global.h user settings on top
S8 max_code_size ALIGN = MAX_CODE_SIZE;
S8 max_data_size ALIGN = MAX_DATA_SIZE;

S8 data_mem_size ALIGN;
S8 stack_size ALIGN = STACKSIZE;		// stack size added to data size when dumped to object file

// code
U1 *code = NULL;

// data
U1 *data_global = NULL;

U1 silent_run = 1;

S8 code_ind ALIGN;
S8 data_ind ALIGN;
S8 modules_ind ALIGN = -1;    // no module loaded = -1

S8 cpu_ind ALIGN = 0;

S8 max_cpu ALIGN = MAXCPUCORES;    // number of threads that can be runned

struct data_info data_info[MAXDATAINFO];
S8 data_info_ind ALIGN = -1;

int main (int ac, char *av[])
{
    U1 byte_char = 0;

    printf ("L1VM disassembler\n\n");

    if (ac > 1)
    {
        if (ac == 3)
        {
            if (strcmp (av[2], "-byte-char") == 0)
            {
                byte_char = 1;
            }
            else
            {
                printf ("disasm <object-file> -byte-char\n");
                exit (1);
            }
        }
        load_object ((U1 *) av[1], byte_char);
    }
    else
    {
        printf ("disasm <object-file> -byte-char\n");
        exit (1);
    }

    if (code) free (code);
    if (data_global) free (data_global);
    exit (0);
}
