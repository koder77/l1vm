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

// You have to create a new user "l1vm" with no "sudo" or "su" rights, to make this safe!

#include "../../../include/global.h"
#include "../../../include/stack.h"

// proto
S2 searchstr (U1 *str, U1 *srchstr, S2 start, S2 end, U1 case_sens);
size_t strlen_safe (const char * str, int maxlen);

#if PROCESS_MODULE

U1 *run_shell (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 commandaddr ALIGN;
	S8 ret ALIGN;

	U1 command_str[4097];
    #if PROCCESS_L1VM_USER
    U1 *sudo_str = (U1 *) "sudo -u l1vm ";
    #else
    U1 *sudo_str = (U1 *) "";
    #endif
	S8 sudo_str_len = 0;
	S8 command_len = 0;
	
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
 	   printf ("ERROR: run_shell: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

    if (searchstr (&data[commandaddr], (U1 *) "sudo", 0, 0, 0) >= 0)
    {
        // ERROR: "sudo" not allowed!

        printf ("run_shell: ERROR command contains sudo!\n");

        sp = stpushi (1, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("run_shell: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    if (searchstr (&data[commandaddr], (U1 *) "su", 0, 0, 0) >= 0)
    {
        // ERROR: "su" not allowed!

        printf ("run_shell: ERROR command contains su!\n");

        sp = stpushi (1, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("run_shell: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    // get sudo_str len 
	sudo_str_len = strlen_safe ((const char *) &data[commandaddr], MAXLINELEN);
    
	// get command len 
	command_len = strlen_safe ((const char *) &data[commandaddr], MAXLINELEN);
	if (sudo_str_len + command_len > MAXLINELEN)
	{
		printf ("ERROR: run_shell: ERROR: command string overflow!\n");
 	   return (NULL);
    }
    
    // string sizes ok, build command_str for linux
    strcpy ((char *) command_str, (const char *) sudo_str);
    strcat ((char *) command_str, (const char *) &data[commandaddr]);

    #if _WIN32
    ret = system ((const char *) &data[commandaddr]);
    if (ret == -1)
    {
        perror ("run_shell:\n");
    }
    #else
    ret = system ((const char *) command_str);
    if (ret == -1)
    {
        perror ("run_shell:\n");
    }
    else
    {
        ret = WEXITSTATUS (ret);
    }
    #endif

    sp = stpushi (ret, sp, sp_bottom);
    if (sp == NULL)
    {
      // error
      printf ("run_shell: ERROR: stack corrupt!\n");
      return (NULL);
    }

    return (sp);
}

#else

U1 *run_shell (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// print error message!
	printf ("run_shell: process calling module switched off by default!\nSee 'include/settings.h' !!!\n");
	return (NULL);
}

#endif
