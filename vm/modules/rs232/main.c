/*
 * This file rs232.c is part of L1vm.
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

#include "rs232.h"

S2 init_memory_bounds (struct data_info *data_info_orig, S8 data_info_ind_orig)
{
	return (0);
}

U1 *rs232_GetPortNumber  (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 portnumber ALIGN;
    S8 portnameaddr ALIGN;

    sp = stpopi ((U1 *) &portnameaddr, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: rs232_GetPortNumber: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

    portnumber =  RS232_GetPortnr ((const char *) &data[portnameaddr]);

    sp = stpushi (portnumber, sp, sp_bottom);
    if (sp == NULL)
    {
      // error
      printf ("rs232_GetPortNumber: ERROR: stack corrupt!\n");
      return (NULL);
    }

    return (sp);
}

U1 *rs232_OpenComport (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 portnumber ALIGN;
    S8 baudrate ALIGN;
    S8 modeaddr ALIGN;
    S8 ret ALIGN;

	if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: rs232_OpenComport: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &modeaddr, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: rs232_OpenComport: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

    sp = stpopi ((U1 *) &baudrate, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_OpenComport: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &portnumber, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_OpenComport: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if DEBUG
	printf ("rs232_open_comport: port: %lli, baud: %lli, mode: '%s'\n", portnumber, baudrate, &data[modeaddr]);
	#endif

    ret = RS232_OpenComport (portnumber, baudrate, (const char *) &data[modeaddr]);

	// ret = RS232_OpenComport (0, 38400, (const char *) mode);

	#if DEBUG
	printf ("rs232_open_comport: return value: %lli\n", ret);
	#endif

    sp = stpushi (ret, sp, sp_bottom);
    if (sp == NULL)
    {
      // error
      printf ("rs232_OpenComport: ERROR: stack corrupt!\n");
      return (NULL);
    }

    return (sp);
}

U1 *rs232_PollComport (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 portnumber ALIGN;
    S8 size ALIGN;
	S8 dataptr ALIGN;
    S8 i ALIGN;
	S8 ret ALIGN;

	U1 buf[4096];

    sp = stpopi ((U1 *) &size, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_PollComport: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &dataptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_PollComport: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &portnumber, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_PollComport: ERROR: stack corrupt!\n");
		return (NULL);
	}

    ret = RS232_PollComport (portnumber, buf, size);
	if (ret > size)
	{
		// error
		printf ("rs232_PollComport: ERROR: data buffer overflow!\n");
		printf ("reading %lli bytes into buffer of: %lli failed!\n", ret, size);
		return (NULL);
	}

    for (i = 0; i < ret; i++)
	{
		data[dataptr + i] = buf[i];
	}

    // push number of received bytes on stack
    sp = stpushi (ret, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("rs232_PollComport: ERROR: stack corrupt!\n");
        return (NULL);
    }

    return (sp);
}

U1 *rs232_SendByte (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 portnumber ALIGN;
	S8 ret ALIGN;
	U1 byte;

    sp = stpopb (&byte, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_SendByte: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &portnumber, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_SendByte: ERROR: stack corrupt!\n");
		return (NULL);
	}

    ret = RS232_SendByte (portnumber, byte);

	#if DEBUG
	printf ("rs232_SendByte: %02X, port: %lli\n", byte, portnumber);
	#endif

    sp = stpushi (ret, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("rs232_SendByte: ERROR: stack corrupt!\n");
        return (NULL);
    }

    return (sp);
}

U1 *rs232_SendBuf (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 portnumber ALIGN;
    S8 size ALIGN;
	S8 dataptr ALIGN;
	S8 ret ALIGN;


    sp = stpopi ((U1 *) &size, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_SendBuf: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &dataptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_PollComport: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &portnumber, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_SendBuf: ERROR: stack corrupt!\n");
		return (NULL);
	}

    ret = RS232_SendBuf (portnumber, &data[dataptr], size);

    sp = stpushi (ret, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("rs232_SendBuf: ERROR: stack corrupt!\n");
        return (NULL);
    }

    return (sp);
}

U1 *rs232_CloseComport (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 portnumber ALIGN;

    sp = stpopi ((U1 *) &portnumber, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_CloseComport: ERROR: stack corrupt!\n");
		return (NULL);
	}

    RS232_CloseComport (portnumber);

    return (sp);
}
