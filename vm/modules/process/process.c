/*
 * This file process.c is part of L1vm.
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

#include "../../../include/global.h"
#include "../../../include/stack.h"


U1 *run_shell (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 commandaddr ALIGN;
	S8 ret ALIGN;

	if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: run_shell: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &commandaddr, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: genann_read_ann: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

	ret = system ((const char *) &data[commandaddr]);
    if (ret != 0)
    {
        perror ("run_shell:\n");
    }

    sp = stpushi (ret, sp, sp_bottom);
    if (sp == NULL)
    {
      // error
      printf ("run_shell: ERROR: stack corrupt!\n");
      return (NULL);
    }

    return (sp);
}
