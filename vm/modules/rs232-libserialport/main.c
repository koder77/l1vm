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

#include <libserialport.h>

#define MAXRS232 32
#define PORTCLOSED 0
#define PORTOPEN 1

struct rs232
{
	struct sp_port *port;
	U1 name[256];
	U1 state;
};

static struct rs232 rs232[MAXRS232];

U1 *rs232_init (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 i ALIGN;

	for (i = 0; i < MAXRS232; i++)
	{
		rs232[i].state = PORTCLOSED;
		rs232[i].name[0] = '\0';
		rs232[i].port = NULL;
	}

	return (sp);
}

U1 *rs232_OpenComport (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 portnameaddr ALIGN;
    S8 baudrate ALIGN;
    S8 handle ALIGN;
    S8 ret ALIGN;

	enum sp_return error;

	sp = stpopi ((U1 *) &baudrate, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_OpenComport: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &portnameaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_OpenComport: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_OpenComport: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (rs232[handle].state == PORTOPEN)
	{
		printf ("rs232_OpenComport: ERROR port: %s already open!\n", &data[portnameaddr]);

		// push ERROR code ERROR
        sp = stpushi (ERR_FILE_OPEN, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_fopen: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
	}

	#if DEBUG
	printf ("rs232_OpenComport: port: %s, baud: %lli\n", &data[portnameaddr], baudrate);
	#endif

	error = sp_get_port_by_name ((const char *) &data[portnameaddr] ,&rs232[handle].port);
    if (error == SP_OK)
	{
    	error = sp_open (rs232[handle].port, SP_MODE_READ_WRITE);
	    if (error == SP_OK)
		{
        	sp_set_baudrate(rs232[handle].port, baudrate);

			strncpy ((char *) rs232[handle].name, (const char *) &data[portnameaddr], 255);
			rs232[handle].state = PORTOPEN;
			ret = ERR_FILE_OK;

			printf ("rs232_OpenComport: port handle: %lli opened...\n", handle);
		}
		else
		{
			ret = ERR_FILE_OPEN;
		}
	}
	else
	{
		ret = ERR_FILE_OPEN;
	}

	// push ERROR code
	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("rs232_OpenComport: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *rs232_CloseComport (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 handle ALIGN;

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_CloseComport: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (rs232[handle].state != PORTOPEN)
	{
		printf ("rs232_CloseComport: ERROR port not open: %s !\n", rs232[handle].name);
	}

	sp_close (rs232[handle].port);
	rs232[handle].state = PORTCLOSED;
	rs232[handle].name[0] = '\0';
	rs232[handle].port = NULL;

    return (sp);
}

U1 *rs232_PollComport (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
	S8 bufaddr ALIGN;
	S8 bufsize ALIGN;

	enum sp_return error;

	sp = stpopi ((U1 *) &bufsize, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_PollComport: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &bufaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_PollComport: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_PollComport: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (rs232[handle].state == PORTCLOSED)
	{
		printf ("rs232_PollComport: ERROR: handle: %lli, port: '%s' closed!\n", handle, rs232[handle].name);
		// push ERROR code
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("rs232_PollComport: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}

	error = sp_nonblocking_read (rs232[handle].port, &data[bufaddr], bufsize);
	// push ERROR code
	sp = stpushi (error, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("rs232_PollComport: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *rs232_PollComport_Wait (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
	S8 bufaddr ALIGN;
	S8 bufsize ALIGN;

	S8  bytes_waiting ALIGN;
	U1 wait = 1;
	enum sp_return error;

	sp = stpopi ((U1 *) &bufsize, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_PollComport_Wait: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &bufaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_PollComport_Wait: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_PollComport_Wait: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (rs232[handle].state == PORTCLOSED)
	{
		printf ("rs232_PollComport_Wait: ERROR: handle: %lli, port: '%s' closed!\n", handle, rs232[handle].name);
		// push ERROR code
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("rs232_PollComport_Wait: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}

	// wait for incoming bytes on serial port...
	while (wait)
	{
		bytes_waiting = sp_input_waiting (rs232[handle].port);
		if (bytes_waiting > 0)
		{
			error = sp_nonblocking_read (rs232[handle].port, &data[bufaddr], bufsize);
			wait = 0;
		}
		usleep (50 * 1000);
	}

	// push ERROR code
	sp = stpushi (error, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("rs232_PollComport_Wait: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *rs232_SendByte (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
	S8 bufaddr ALIGN;

	enum sp_return error;

	sp = stpopi ((U1 *) &bufaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_SendByte: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_SendByte: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (rs232[handle].state == PORTCLOSED)
	{
		printf ("rs232_SendByte: ERROR: handle: %lli, port: '%s' closed!\n", handle, rs232[handle].name);
		// push ERROR code
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("rs232_SendByte: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}

	error = sp_nonblocking_write (rs232[handle].port, &data[bufaddr], 1);
	// push ERROR code
	sp = stpushi (error, sp, sp_bottom);
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
	S8 handle ALIGN;
	S8 bufaddr ALIGN;
	S8 bufsize ALIGN;

	enum sp_return error;

	sp = stpopi ((U1 *) &bufsize, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_send_buf: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &bufaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_send_buf: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("rs232_SendBuf: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (rs232[handle].state == PORTCLOSED)
	{
		printf ("rs232_SendBuf: ERROR: handle: %lli, port: '%s' closed!\n", handle, rs232[handle].name);
		// push ERROR code
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("rs232_SendBuf: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}

	error = sp_nonblocking_write (rs232[handle].port, &data[bufaddr], bufsize);
	// push ERROR code
	sp = stpushi (error, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("rs232_SendBuf: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}
