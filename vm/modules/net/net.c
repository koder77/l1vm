/*
 * This file net.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2020
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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

// protos

extern S2 memory_bounds (S8 start, S8 offset_access);

#define SOCKADDRESSLEN      16      /* max dotted ip len */
#define SOCKBUFSIZE         10240    /* socket data buffer len */

#define SOCKETOPEN 1              // state flags
#define SOCKETCLOSED 0

#define SOCKSERVER          0
#define SOCKCLIENT          1


typedef int NINT;

struct socket
{
    S2 socket;                                /* socket handle */
    S2 serv_conn;                           /* server connection */
    struct addrinfo *servinfo;
    U1 type;                                /* server / client */
    U1 state;                              /* open, closed */
    U1 client_ip[SOCKADDRESSLEN];           /* client ip */
    U1 buf[SOCKBUFSIZE];                    /* socket data buffer */
};

static struct socket *sockets = NULL;
static S8 socketmax ALIGN = 0;

size_t strlen_safe (const char * str, int maxlen);

U1 get_sandbox_filename (U1 *filename, U1 *sandbox_filename, S2 max_name_len);


/* helper function (taken from bgnet_socket_programming) */
char *get_ip_str (const struct sockaddr *sa, char *s, size_t maxlen)
{
    switch (sa->sa_family)
    {
        case AF_INET:
            inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr), s, maxlen);
            break;

        case AF_INET6:
            inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr), s, maxlen);
            break;

        default:
            strncpy(s, "Unknown AF", maxlen);
            return NULL;
    }
    return s;
}


U1 *init_sockets (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 i ALIGN;
	S8 maxind ALIGN;

	sp = stpopi ((U1 *) &maxind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("init_sockets: ERROR: stack corrupt!\n");
		return (NULL);
	}

	// allocate gobal mem structure
	sockets = (struct socket*) calloc (maxind, sizeof (struct socket));
	if (sockets == NULL)
	{
		printf ("init_sockets: ERROR can't allocate %lli memory indexes!\n", maxind);

		sp = stpushi (1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("init_sockets: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}

	socketmax = maxind;	// save to global var

	// set all socket handles as CLOSED
    for (i = 0; i < socketmax; i++)
    {
        sockets[i].state = SOCKETCLOSED;
    }

    // error code ok
    sp = stpushi (0, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("init_sockets: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *free_mem (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	if (sockets) free (sockets);
	return (sp);
}


S8 get_free_socket_handle (void)
{
	S8 i ALIGN;

	for (i = 0; i < socketmax; i++)
	{
		if (sockets[i].state == SOCKETCLOSED)
		{
			return (i);
		}
	}

	// no free file handle found
	return (-1);
}

U1 *open_server_socket (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S2 handle = -1;
    U1 *hostname;
    S8 hostname_addr ALIGN;
    S8 port ALIGN;

    NINT yes = 1;
    S2 server, error;
    S2 status;
    U1 port_string[256];
    struct addrinfo hints;
    struct addrinfo *servinfo;
    // will point to the results
    memset (&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;
    // don't care IPv4 or IPv6

    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

    handle = get_free_socket_handle ();
    if (handle == -1)
    {
        /* error socket list full */
        sp = stpushi (ERR_FILE_OPEN, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_server_socket: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: open_server_socket: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &port, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("open_server_socket: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &hostname_addr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("open_server_socket: ERROR: stack corrupt!\n");
		return (NULL);
	}

	hostname = &data[hostname_addr];

    if (handle < 0 || handle >= socketmax)
    {
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_server_socket: ERROR: stack corrupt!\n");
			return (NULL);
		}

        printf ("ERROR: open_server_socket: handle out of range!\n");
        sp = stpushi (ERR_FILE_OPEN, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_server_socket: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    if (sockets[handle].state == SOCKETOPEN)
    {
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_server_socket: ERROR: stack corrupt!\n");
			return (NULL);
		}

        printf ("ERROR: open_server_socket: handle %i already open!\n", handle);
        sp = stpushi (ERR_FILE_OPEN, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_server_socket: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    snprintf ((char *) port_string, 256, "%lld", port);
    // convert integer port number to a string

    // printf ("open_server_socket: hostname: %s, port: %s, port (num): %lld\n", hostname, port_string, port);

    if ((status = getaddrinfo ((const char *) hostname, (const char *) port_string, &hints, &servinfo)) != 0)
    {
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_server_socket: ERROR: stack corrupt!\n");
			return (NULL);
		}

        sp = stpushi (status, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_server_socket: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);    /* return error code */
    }

    server = socket (servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (server == -1)
    {
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_server_socket: ERROR: stack corrupt!\n");
			return (NULL);
		}

        sp = stpushi (errno, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_server_socket: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    /* avoiding socket already in use error */

    error = (setsockopt (server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof (NINT)));
    if (error == -1)
    {
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_server_socket: ERROR: stack corrupt!\n");
			return (NULL);
		}

        sp = stpushi (errno, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_server_socket: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    error = bind (server, servinfo->ai_addr, servinfo->ai_addrlen);
    if (error == -1)
    {
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_server_socket: ERROR: stack corrupt!\n");
			return (NULL);
		}

        sp = stpushi (errno, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_server_socket: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    error = listen (server, SOMAXCONN);
    if (error == -1)
    {
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_server_socket: ERROR: stack corrupt!\n");
			return (NULL);
		}

        sp = stpushi (errno, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_server_socket: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    sp = stpushi (handle, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("open_server_socket: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("open_server_socket: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sockets[handle].socket = server;
    sockets[handle].servinfo = servinfo;
    sockets[handle].type = SOCKSERVER;
    sockets[handle].state = SOCKETOPEN;
    strcpy ((char *) sockets[handle].client_ip, "");
    return (sp);
}

U1 *open_accept_server (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S2 handle;
    S2 connection;
    struct sockaddr_storage client;
    socklen_t addr_size;
    S8 new_handle ALIGN = -1;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: open_accept_server: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("open_server_socket: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (handle < 0 || handle >= socketmax)
    {
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_server_socket: ERROR: stack corrupt!\n");
			return (NULL);
		}

		printf ("ERROR: open_accept_server: handle out of range!\n");
        sp = stpushi (ERR_FILE_OPEN, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_accept_server: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    if (sockets[handle].state != SOCKETOPEN || sockets[handle].type != SOCKSERVER)
    {
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_server_socket: ERROR: stack corrupt!\n");
			return (NULL);
		}

        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_accept_server: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    addr_size = sizeof client;
    connection = accept (sockets[handle].socket, (struct sockaddr *) &client, &addr_size);
    if (connection == -1)
    {
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_server_socket: ERROR: stack corrupt!\n");
			return (NULL);
		}

        sp = stpushi (errno, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_accept_server: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    new_handle = get_free_socket_handle ();
    if (new_handle == -1)
    {
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_server_socket: ERROR: stack corrupt!\n");
			return (NULL);
		}

        /* error socket list full */
        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_accept_server: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    if (get_ip_str ((struct sockaddr *) &client, (char *) sockets[new_handle].client_ip, SOCKADDRESSLEN) == NULL)
    {
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_server_socket: ERROR: stack corrupt!\n");
			return (NULL);
		}

        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_accept_server: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    sockets[new_handle].serv_conn = connection;
    sockets[new_handle].state = SOCKETOPEN;
    sockets[new_handle].type = SOCKSERVER;

    sp = stpushi (new_handle, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("open_accept_server: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("open_accept_server: ERROR: stack corrupt!\n");
        return (NULL);
    }

    return (sp);
}

U1 *open_client_socket (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S2 client, error;
    S2 status;
    U1 port_string[256];
    U1 *hostname;
    S8 hostname_addr ALIGN;
    S2 handle = -1, port;

    struct addrinfo hints;
    struct addrinfo *servinfo;
    // will point to the results
    memset (&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;
    // don't care IPv4 or IPv6

    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

    handle = get_free_socket_handle ();
    if (handle == -1)
    {
        /* error socket list full */
        sp = stpushi (ERR_FILE_OPEN, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("open_client_socket: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: open_client_socket: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &port, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("open_client_socket: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &hostname_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("open_client_socket: ERROR: stack corrupt!\n");
        return (NULL);
    }

    hostname = &data[hostname_addr];

    if (handle < 0 || handle >= socketmax)
    {
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_client_socket: ERROR: stack corrupt!\n");
			return (NULL);
		}

        printf ("ERROR: open_client_socket: handle out of range!\n");
        sp = stpushi (ERR_FILE_OPEN, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_client_socket: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    if (sockets[handle].state == SOCKETOPEN)
    {
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_client_socket: ERROR: stack corrupt!\n");
			return (NULL);
		}

        printf ("ERROR: open_client_socket: handle %i already open!\n", handle);
        sp = stpushi (ERR_FILE_OPEN, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_client_socket: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    snprintf ((char *) port_string, 256, "%d", port);
    // convert integer port number to a string

    if ((status = getaddrinfo ((const char *) hostname, (const char *) port_string, &hints, &servinfo)) != 0)
    {
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_client_socket: ERROR: stack corrupt!\n");
			return (NULL);
		}

        sp = stpushi (status, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_client_socket: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    client = socket (servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (client == -1)
    {
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_client_socket: ERROR: stack corrupt!\n");
			return (NULL);
		}

        sp = stpushi (errno, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_client_socket: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    error = connect (client, servinfo->ai_addr, servinfo->ai_addrlen);
    if (error == -1)
    {
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_client_socket#: ERROR: stack corrupt!\n");
			return (NULL);
		}

        sp = stpushi (errno, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_client_socket: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    sp = stpushi (handle, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("open_client_socket: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("open_client_socket: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sockets[handle].socket = client;
    sockets[handle].type = SOCKCLIENT;
    sockets[handle].state = SOCKETOPEN;

    return (sp);
}

U1 *close_server_socket (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S2 handle;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: close_server_socket: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("close_server_socket: ERROR: stack corrupt!\n");
        return (NULL);
    }

    if (handle < 0 || handle > socketmax - 1)
    {
        sp = stpushi (ERR_FILE_NUMBER, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_server_socket: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    if (sockets[handle].state == SOCKETCLOSED || sockets[handle].type != SOCKSERVER)
    {
        sp = stpushi (ERR_FILE_CLOSE, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_server_socket: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    if (shutdown (sockets[handle].socket, SHUT_RDWR) == -1)   /* 0x02*/
    {
        sp = stpushi (ERR_FILE_CLOSE, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_server_socket: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    if (close (sockets[handle].socket) == -1)
    {
        sp = stpushi (ERR_FILE_CLOSE, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_server_socket: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }
    else
    {
        sockets[handle].state = SOCKETCLOSED;

        // servinfo now points to a linked list of 1 or more struct addrinfos
        // ... do everything until you don't need servinfo anymore ....
        freeaddrinfo (sockets[handle].servinfo); // free the linked-list

        sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_server_socket: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }
}

U1 *close_accept_server (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S2 handle;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: close_accept_server: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("close_accept_server: ERROR: stack corrupt!\n");
        return (NULL);
    }

    if (handle < 0 || handle > socketmax - 1)
    {
        sp = stpushi (ERR_FILE_NUMBER, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_accept_server: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    if (sockets[handle].state == SOCKETCLOSED || sockets[handle].type != SOCKSERVER)
    {
        sp = stpushi (ERR_FILE_CLOSE, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_accept_server: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    if (shutdown (sockets[handle].serv_conn, SHUT_RDWR) == -1)
    {
        sp = stpushi (ERR_FILE_CLOSE, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_accept_server: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    if (close (sockets[handle].serv_conn) == -1)
    {
        sp = stpushi (ERR_FILE_CLOSE, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_accept_server: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }
    else
    {
        strcpy ((char *) sockets[handle].client_ip, "");

        sockets[handle].state = SOCKETCLOSED;

        sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_accept_server: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }
}

U1 *close_client_socket (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S2 handle;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: close_client_socket: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("close_client_socket: ERROR: stack corrupt!\n");
        return (NULL);
    }

    if (handle < 0 || handle > socketmax - 1)
    {
        sp = stpushi (ERR_FILE_NUMBER, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_client_socket: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    if (sockets[handle].state == SOCKETCLOSED || sockets[handle].type != SOCKCLIENT)
    {
        sp = stpushi (ERR_FILE_CLOSE, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_client_socket: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    if (shutdown (sockets[handle].socket, SHUT_RDWR) == -1)
    {
        sp = stpushi (ERR_FILE_CLOSE, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_client_socket: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    if (close (sockets[handle].socket) == -1)
    {
        sp = stpushi (ERR_FILE_CLOSE, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_client_socket: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }
    else
    {
        sockets[handle].state = SOCKETCLOSED;

        sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_client_socket: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }
}

U1 *get_clientaddr (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S2 handle;
    S2 client_len;
    S8 ret_addr ALIGN;
    S8 i ALIGN;
    U1 *ret_data = data;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: getclientaddr: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &ret_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("getclientaddr: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("getclientaddr: ERROR: stack corrupt!\n");
        return (NULL);
    }

    if (handle < 0 || handle > socketmax - 1)
    {
        sp = stpushi (ERR_FILE_NUMBER, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("getclientaddr: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    if (sockets[handle].state == SOCKETCLOSED || sockets[handle].type != SOCKSERVER)
    {
        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("getclientaddr: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    client_len = strlen_safe ((const char *) sockets[handle].client_ip, MAXLINELEN) + 1;
    for (i = 0; i <= client_len; i++)
    {
        ret_data[ret_addr + i] = sockets[handle].client_ip[i];
    }

    sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("getclientaddr: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}

U1 *get_hostname (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    U1 hostname[256];
    S2 hostname_len;
    S8 ret_addr ALIGN;
    S8 i ALIGN;
    U1 *ret_data = data;

    sp = stpopi ((U1 *) &ret_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("get_hostname: ERROR: stack corrupt!\n");
        return (NULL);
    }

    if (gethostname ((char *) hostname, 255) == -1)
    {
        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("get_hostname: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    hostname_len = strlen_safe ((char *) hostname, MAXLINELEN) + 1;

    #if BOUNDSCHECK
    if (memory_bounds (ret_addr, hostname_len - 1) != 0)
    {
        printf ("get_hostname: string oveflow!\n");
        return (NULL);
    }
    #endif

    for (i = 0; i <= hostname_len; i++)
    {
        ret_data[ret_addr + i] = hostname[i];
    }

    sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("get_hostname: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}

U1 *get_hostbyname (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    U1 *hostname;
    S8 hostname_addr ALIGN;
    S8 ret_addr ALIGN;
    U1 ret[256];
    S2 ret_len;
    S8 i ALIGN;
    U1 *ret_data = data;

    struct hostent *hp = NULL;
    struct in_addr addr;

    sp = stpopi ((U1 *) &ret_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("getclientaddr: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &hostname_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("getclientaddr: ERROR: stack corrupt!\n");
        return (NULL);
    }

    hostname = &data[hostname_addr];

    hp = (struct hostent *) gethostbyname ((const char *) hostname);
    if (hp == NULL)
    {
        sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("get_hostbyname: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    if (hp->h_addr_list[0] != NULL)
    {
        memcpy (&addr, hp->h_addr_list[0], sizeof (struct in_addr));
        if (strlen_safe (inet_ntoa (addr), MAXLINELEN) <= 256)
        {
            strcpy ((char *) ret, inet_ntoa (addr));

            ret_len = strlen_safe ((char *) ret, MAXLINELEN) + 1;

            #if BOUNDSCHECK
            if (memory_bounds (ret_addr, ret_len - 1) != 0)
            {
                printf ("get_hostbyname: string oveflow!\n");
                return (NULL);
            }
            #endif

            for (i = 0; i <= ret_len; i++)
            {
                ret_data[ret_addr + i] = ret[i];
            }

            sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
            if (sp == NULL)
            {
                // error
                printf ("get_hostbyname: ERROR: stack corrupt!\n");
                return (NULL);
            }
            return (sp);
        }
        else
        {
            sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
            if (sp == NULL)
            {
                // error
                printf ("get_hostbyname: ERROR: stack corrupt!\n");
                return (NULL);
            }
            return (sp);
        }
    }
    else
    {
        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("get_hostbyname: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }
}

U1 *get_hostbyaddr (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    U1 *addr;
    S8 addr_addr ALIGN;
    U1 hostname[256];
    S2 hostname_len;
    S8 ret_addr ALIGN;
    S8 i ALIGN;
    U1 *ret_data = data;

    struct hostent *hp;
    struct in_addr hostaddr;

    sp = stpopi ((U1 *) &ret_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("getclientaddr: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &addr_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("getclientaddr: ERROR: stack corrupt!\n");
        return (NULL);
    }

    addr = &data[addr_addr];

    hostaddr.s_addr = inet_addr ((const char *) addr);
    hp = (struct hostent *) gethostbyaddr ((U1 *) &hostaddr, sizeof (struct in_addr), AF_INET);
    if (hp == NULL)
    {
        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("get_hostbyaddr: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    if (strlen_safe (hp->h_name, MAXLINELEN) <= 255)
    {
        strcpy ((char *) hostname, hp->h_name);

        hostname_len = strlen_safe ((const char *) hostname, MAXLINELEN) + 1;

        #if BOUNDSCHECK
        if (memory_bounds (ret_addr, hostname_len - 1) != 0)
        {
            printf ("get_hostbyaddr: string oveflow!\n");
            return (NULL);
        }
        #endif


        for (i = 0; i <= hostname_len; i++)
        {
            ret_data[ret_addr + i] = hostname[i];
        }

        sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("get_hostbyaddr: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }
    else
    {
        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("get_hostbyaddr: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }
}

// helper functions endianess

#if ! MACHINE_BIG_ENDIAN
S8 htonq (S8 num)
{
    U1 *num_ptr, *new_ptr;
    S8 newv ALIGN;

    num_ptr = (U1 *) &num;
    new_ptr = (U1 *) &newv;

    new_ptr[0] = num_ptr[7];
    new_ptr[1] = num_ptr[6];
    new_ptr[2] = num_ptr[5];
    new_ptr[3] = num_ptr[4];
    new_ptr[4] = num_ptr[3];
    new_ptr[5] = num_ptr[2];
    new_ptr[6] = num_ptr[1];
    new_ptr[7] = num_ptr[0];

    return (newv);
}

S8 ntohq (S8 num)
{
    U1 *num_ptr, *new_ptr;
    S8 newv ALIGN;

    num_ptr = (U1 *) &num;
    new_ptr = (U1 *) &newv;

    new_ptr[0] = num_ptr[7];
    new_ptr[1] = num_ptr[6];
    new_ptr[2] = num_ptr[5];
    new_ptr[3] = num_ptr[4];
    new_ptr[4] = num_ptr[3];
    new_ptr[5] = num_ptr[2];
    new_ptr[6] = num_ptr[1];
    new_ptr[7] = num_ptr[0];

    return (newv);
}

#else
S8 htonq (S8 num)
{
	return (num);
}

S8 ntohq (S8 num)
{
	return (num);
}
#endif

F8 htond (F8 hostd)
{
    U1 *netdptr;
    U1 *hostdptr;
    S2 i;
    F8 netd ALIGN;

    netdptr = (U1 *) &netd;

    hostdptr = (U1 *) &hostd;
    hostdptr += sizeof (F8) - 1;

    #if ! MACHINE_BIG_ENDIAN
        for (i = 0; i <= sizeof (F8) - 1; i++)
        {
            *netdptr++ = *hostdptr--;
        }
    #else
        netd = hostd;
    #endif

    return (netd);
}

F8 ntohd (F8 netd)
{
    U1 *netdptr;
    U1 *hostdptr;
    S2 i;
    F8 hostd ALIGN;

    hostdptr = (U1 *) &hostd;

    netdptr = (U1 *) &netd;
    netdptr += sizeof (F8) - 1;

    #if ! MACHINE_BIG_ENDIAN
        for (i = 0; i <= sizeof (F8) - 1; i++)
        {
            *hostdptr++ = *netdptr--;
        }
    #else
        hostd = netd;
    #endif

    return (hostd);
}


// socket read/write functions

U1 exe_sread (S4 slist_ind, S4 len)
{
    U1 *buf;
    S2 sockh = 0, ret;
    S4 todo, buf_ind = 0;

    if (slist_ind < 0 || slist_ind > socketmax - 1)
    {
        return (ERR_FILE_NUMBER);
    }

    if (sockets[slist_ind].state != SOCKETOPEN)
    {
        return (ERR_FILE_OPEN);
    }

    if (len < 0 || len > SOCKBUFSIZE)
    {
        return (ERR_FILE_READ);
    }

    switch (sockets[slist_ind].type)
    {
        case SOCKSERVER:
            sockh = sockets[slist_ind].serv_conn;
            break;

        case SOCKCLIENT:
            sockh = sockets[slist_ind].socket;
            break;
    }

    todo = len;
    buf = sockets[slist_ind].buf;

    while (todo > 0)
    {
        ret = recv (sockh, &(buf[buf_ind]), todo, MSG_NOSIGNAL);
        if (ret == -1)
        {
            return (errno);
        }

        todo = todo - ret;
        buf_ind = buf_ind + ret;
    }

    return (ERR_FILE_OK);
}

U1 exe_swrite (S4 slist_ind, S4 len)
{
    U1 *buf;
    S2 sockh = 0, ret;
    S4 todo, buf_ind = 0;

    if (slist_ind < 0 || slist_ind > socketmax - 1)
    {
        return (ERR_FILE_NUMBER);
    }

    if (sockets[slist_ind].state != SOCKETOPEN)
    {
        return (ERR_FILE_OPEN);
    }

    if (len < 0 || len > SOCKBUFSIZE)
    {
        return (ERR_FILE_WRITE);
    }

    switch (sockets[slist_ind].type)
    {
        case SOCKSERVER:
            sockh = sockets[slist_ind].serv_conn;
            break;

        case SOCKCLIENT:
            sockh = sockets[slist_ind].socket;
            break;
    }

    todo = len;
    buf = sockets[slist_ind].buf;

    while (todo > 0)
    {
        ret = send (sockh, &(buf[buf_ind]), todo, MSG_NOSIGNAL);
        if (ret == -1)
        {
            return (errno);
        }

        todo = todo - ret;
        buf_ind = buf_ind + ret;
    }

    return (ERR_FILE_OK);
}

/* read ------------------------------------------------------------- */

U1 *socket_read_byte (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    U1 ret;
    S2 handle;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: socket_read_byte: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_read_byte: ERROR: stack corrupt!\n");
        return (NULL);
    }

    ret = exe_sread (handle, sizeof (U1));
    if (ret != ERR_FILE_OK)
    {
        // push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_read_byte: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_read_byte: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    // printf ("socket_read_byte: '%c'\n", sockets[handle].buf[0]);

    sp = stpushi (sockets[handle].buf[0], sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("socket_read_byte: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("socket_read_byte: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}

U1 *socket_read_int64 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 ret ALIGN;
    S2 handle;
    S8 n ALIGN;
    U1 *ptr;
    S8 value ALIGN;
    S8 i ALIGN;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: socket_read_int64: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_read_int64: ERROR: stack corrupt!\n");
        return (NULL);
    }

    ret = exe_sread (handle, sizeof (S8));
    if (ret != ERR_FILE_OK)
    {
        // push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_read_int64: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_read_int64: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    ptr = (U1 *) &n;

    for (i = 0; i <= sizeof (S8) - 1; i++)
    {
        *ptr++ = sockets[handle].buf[i];
    }

    value = ntohq (n);

    sp = stpushi (value, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("socket_read_int64: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("socket_read_int64: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}

U1 *socket_read_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 ret ALIGN;
    S2 handle;
    F8 n ALIGN;
    U1 *ptr;
    F8 value ALIGN;
    S8 i ALIGN;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: socket_read_double: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_read_double: ERROR: stack corrupt!\n");
        return (NULL);
    }

    ret = exe_sread (handle, sizeof (F8));
    if (ret != ERR_FILE_OK)
    {
        // push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_read_double: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_read_double: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    ptr = (U1 *) &n;

    for (i = 0; i <= sizeof (F8) - 1; i++)
    {
        *ptr++ = sockets[handle].buf[i];
    }

    value = ntohd (n);

    sp = stpushd (value, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("socket_read_double: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("socket_read_double: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}

U1 *socket_read_string (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
     /* read CRLF or LF terminated line */

    S8 ret ALIGN;
    S8 ret_addr ALIGN;
    S2 handle;
    U1 ch;
    S8 slen ALIGN;
    U1 end = FALSE;
    U1 error = FALSE;
    S8 i ALIGN = 0;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: socket_read_string: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &slen, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_read_string: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &ret_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_read_string: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_read_string: ERROR: stack corrupt!\n");
        return (NULL);
    }

    while (! end)
    {
        ret = exe_sread (handle, sizeof (U1));
        if (ret != ERR_FILE_OK)
        {
            error = TRUE;
            end = TRUE;

			break;
        }

        ch = sockets[handle].buf[0];

        if (ch != '\n')
        {
            if (i <= slen)
            {
                data[ret_addr + i] = ch;
                i++;
            }
            else
            {
               error = TRUE; end = TRUE;
            }
        }
        else
        {
            /* line end */
            data[ret_addr + i] = '\0';
            end = TRUE;
        }
    }

    if (error == FALSE)
    {
        sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_read_string: ERROR: stack corrupt!\n");
            return (NULL);
        }
    }
    else
    {
        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_read_string: ERROR: stack corrupt!\n");
            return (NULL);
        }
    }
    return (sp);
}


// write ------------------------------------------------------------

U1 *socket_write_byte (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 send_byte ALIGN;
    U1 ret;
    S2 handle;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: socket_write_byte: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &send_byte, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_write_byte: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_write_byte: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sockets[handle].buf[0] = (U1) send_byte;

    ret = exe_swrite (handle, sizeof (U1));
    sp = stpushi (ret, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("socket_write_byte: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}

U1 *socket_write_int64 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 ret ALIGN;
    S2 handle;
    U1 *ptr;
    S8 n ALIGN;
    S8 send_int64 ALIGN;
    S8 i ALIGN;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: socket_write_int64: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &send_int64, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_write_int64: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_write_int64: ERROR: stack corrupt!\n");
        return (NULL);
    }

    n = htonq (send_int64);
    ptr = (U1 *) &n;

    for (i = 0; i <= sizeof (S8) - 1; i++)
    {
        sockets[handle].buf[i] = *ptr++;
    }

    // ret = exe_swrite (handle, sizeof (U1)); ERROR???
	ret = exe_swrite (handle, sizeof (S8));
    sp = stpushi (ret, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("socket_write_int64: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}

U1 *socket_write_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 ret ALIGN;
    S2 handle;
    U1 *ptr;
    F8 n ALIGN;
    F8 send_double ALIGN;
    S8 i ALIGN;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: socket_write_double: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &send_double, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_write_double: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_write_double: ERROR: stack corrupt!\n");
        return (NULL);
    }

    n = htond (send_double);
    ptr = (U1 *) &n;

    for (i = 0; i <= sizeof (S8) - 1; i++)
    {
        sockets[handle].buf[i] = *ptr++;
    }

    ret = exe_swrite (handle, sizeof (U1));
    sp = stpushi (ret, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("socket_write_double: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}

U1 *socket_write_string (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
     /* write string + LF terminated line */

    S8 ret ALIGN;
    S8 send_addr ALIGN;
    S2 handle;
    U1 end = FALSE;
    S8 i ALIGN = -1;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: socket_write_string: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &send_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_write_string: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_write_string: ERROR: stack corrupt!\n");
        return (NULL);
    }

    while (! end)
    {
		i++;
        sockets[handle].buf[i] = data[send_addr + i];
        if (sockets[handle].buf[i] == '\0')
        {
            sockets[handle].buf[i] = '\n';
            end = TRUE;
        }
    }

	ret = exe_swrite (handle, i + 1);
	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("socket_write_string: ERROR: stack corrupt!\n");
		return (NULL);
	}
    return (sp);
}

// get mimetype from file suffix: .* ==========================================

U1 *get_mimetype_from_filename (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	U1 endingstr[256];
	U1 dotch = '.';
	S8 slen ALIGN;
	S8 mime_len ALIGN;
	S8 i ALIGN;
	S8 j ALIGN;
	S8 dotpos ALIGN;
	S8 filename_addr ALIGN;
	S8 mimetype_addr ALIGN;
	S8 mimetype_len ALIGN;

	S8 mimetypes ALIGN = 31; // from 0 - x
	U1 found_mimetype = 0;

	U1 mimetype_octet_stream[] = "application/octet-stream";

	struct mimetype
	{
		U1 ending[256];
		U1 mimetype[256];
	};

	struct mimetype mimetype[] =
	{
 		{ "HTML", "text/html" },
		{ "HTM", "text/html" },
		{ "TXT", "text/plain" },
		{ "DOC", "text/plain" },
		{ "GUIDE", "text/aguide" },
		{ "GIF", "image/gif" },
		{ "BMP", "image/bmp" },
		{ "PNG", "image/png" },
		{ "JPEG", "image/jpeg" },
		{ "JPG", "image/jpeg" },
		{ "WAV", "audio/* "},
		{ "AU", "audio/*" },
		{ "8SVX", "audio/*" },
		{ "MP3", "audio/*" },
		{ "MIDI", "audio/*" },
		{ "MID", "audio/*" },
		{ "ZIP", "application/octet-stream" },
		{ "BZ2", "application/octet-stream" },
		{ "GZ", "application/octet-stream" },
		{ "LHA", "application/octet-stream" },
		{ "LZH", "application/octet-stream" },
		{ "LZX", "application/octet-stream" },
		{ "TAR", "application/octet-stream" },
		{ "EXE", "application/octet-stream" },
		{ "APK", "application/octet-stream" },
		{ "PS", "application/postscript" },
		{ "PDF", "application/pdf" },
		{ "MOV", "application/quicktime" },
		{ "MOOV", "application/quicktime" },
		{ "MP4", "video/mp4" },
		{ "AVI", "video/avi" },
		{ "ISO", "application/octet-stream" },
        { "L1COM", "text/plain" }, 
        { "L1H", "text/plain" },
        { "L1ASM", "text/plain" },
        { "L1OBJ", "application/octet-stream" },
        { "L1DBG", "text/plain" }
	};

	sp = stpopi ((U1 *) &mimetype_len, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("get_mimetype_from_filename: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &mimetype_addr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("get_mimetype_from_filename: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &filename_addr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("get_mimetype_from_filename: ERROR: stack corrupt!\n");
		return (NULL);
	}


	// get .* part of filename

	dotpos = -1;
	slen = strlen_safe ((const char *) &data[filename_addr], MAXLINELEN);
	for (i = slen - 1; i >= 0; i--)
	{
		if (data[filename_addr + i] == dotch)
		{
			dotpos = i;
			break;
		}
	}

	if (dotpos == -1)
	{
		// no file ending: .* found
		strcpy ((char *) &data[mimetype_addr], "application/octet-stream");
		sp = stpushi (0, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("get_mimetype_from_filename: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}

	j = 0;
	for (i = dotpos + 1; i < slen; i++)
	{
		endingstr[j] = toupper (data[filename_addr + i]);
		j++;
	}
	endingstr[j] = '\0';	// set end of string

	// printf ("get_mimetype_from_filename: ending: '%s'\n", endingstr);

	// get mimetype from file ending: -----------------------------------------

	for (i = 0; i <= mimetypes; i++)
	{
		if (strcmp ((const char *) endingstr, (const char *) mimetype[i].ending) == 0)
		{
			// found mimetype
			mime_len = strlen_safe ((const char *) mimetype[i].mimetype, MAXLINELEN);
			if (mime_len > mimetype_len - 1)
			{
				printf ("get_mimetype_from_filename: ERROR mimetype string overflow!\n");

				strcpy ((char *) &data[mimetype_addr], "");

				sp = stpushi (1, sp, sp_bottom);
				if (sp == NULL)
				{
					// error
					printf ("get_mimetype_from_filename: ERROR: stack corrupt!\n");
					return (NULL);
				}
				return (sp);		// exit
			}
			else
			{
				strcpy ((char *) &data[mimetype_addr], (const char *) mimetype[i].mimetype);
				found_mimetype = 1;
				break;
			}
		}
	}

	if (found_mimetype == 0)
	{
		// no mimetype found, use octet/stream

		if (strlen_safe ((const char *) mimetype_octet_stream, MAXLINELEN) > mimetype_len - 1)
		{
			printf ("get_mimetype_from_filename: ERROR mimetype string overflow!\n");

			strcpy ((char *) &data[mimetype_addr], "");

			sp = stpushi (1, sp, sp_bottom);
			if (sp == NULL)
			{
				// error
				printf ("get_mimetype_from_filename: ERROR: stack corrupt!\n");
				return (NULL);
			}
		}
		else
		{
			strcpy ((char *) &data[mimetype_addr], (const char *) mimetype_octet_stream);

			sp = stpushi (0, sp, sp_bottom);
			if (sp == NULL)
			{
				// error
				printf ("get_mimetype_from_filename: ERROR: stack corrupt!\n");
				return (NULL);
			}
		}
	}
	else
	{
		sp = stpushi (0, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("get_mimetype_from_filename: ERROR: stack corrupt!\n");
			return (NULL);
		}
	}

	// printf ("get_mimetype_from_filename: mimetype: '%s'\n", &data[mimetype_addr]);

	return (sp);
}

U1 *socket_send_file (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// send file requested by "GET" HTTP command
	// for building a simple webserver in brackets

	S8 i ALIGN;
	S8 nameaddr ALIGN;;
	S8 mimetype_addr ALIGN;
	S2 handle;

	FILE *file;
	U1 file_name[256];
	U1 file_size_str[256];
	S8 file_name_len ALIGN;
	S8 file_size ALIGN;
	U1 ch;

	S8 header_len ALIGN;
	S8 ret ALIGN;

	U1 not_found = 0;	// 1 if 404 HTTP error

	sp = stpopi ((U1 *) &mimetype_addr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("socket_send_file: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &nameaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("socket_send_file: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("socket_send_file: ERROR: stack corrupt!\n");
		return (NULL);
	}

	file_name_len = strlen_safe ((char *) &data[nameaddr], 255);
	if (file_name_len > 255)
	{
		printf ("ERROR: socket_send_file: file name: '%s' too long!\n", (char *) &data[nameaddr]);
		return (NULL);
	}

	#if SANDBOX
	if (get_sandbox_filename (&data[nameaddr], file_name, 255) != 0)
	{
		printf ("ERROR: socket_send_file: illegal filename: %s\n", &data[nameaddr]);
		return (NULL);
	}
	#else
	if (strlen_safe (&data[nameaddr], MAXLINELEN) < 255)
	{
		strcpy ((char *) file_name, (char *) &data[nameaddr]);
	}
	else
	{
		printf ("ERROR: socket_send_file: filename too long!\n", &data[nameaddr]);
		return (NULL);
	}
	#endif

	file = fopen ((char *) file_name, "r");
	if (file == NULL)
	{
		// send 404 not found
		not_found = 1;

		printf ("socket_send_file: 404 not found: '%s'\n", file_name);

		strcpy ((char *) sockets[handle].buf, "HTTP/1.1 404 Not Found");
		strcat ((char *) sockets[handle].buf, "\n");

		header_len = strlen_safe ((const char *) sockets[handle].buf, MAXLINELEN);
		ret = exe_swrite (handle, header_len);
		if (ret != ERR_FILE_OK)
		{
			printf ("socket_send_file: ERROR: sending data!\n");

			// push ERROR code ERROR
			sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
			if (sp == NULL)
			{
				// error
				printf ("socket_send_file: ERROR: stack corrupt!\n");
				return (NULL);
			}
			return (sp);
		}

		// set 404.html as filename
		#if SANDBOX
		if (get_sandbox_filename ((U1 *) "web/404.html", file_name, 255) != 0)
		{
			printf ("ERROR: socket_send_file: illegal filename: %s\n", &data[nameaddr]);
			return (NULL);
		}
		#else
		if (strlen_safe (&data[nameaddr], MAXLINELEN) < 255)
		{
			strcpy ((char *) file_name, (char *) "web/404.html");
		}
		else
		{
			printf ("ERROR: socket_send_file: filename too long!\n", &data[nameaddr]);
			return (NULL);
		}
		#endif

		// printf ("socket_send_file: DEBUG: 404 filename: %s\n", file_name);

		file = fopen ((char *) file_name, "r");
		if (file == NULL)
		{
			// push ERROR code ERROR
			printf ("socket_send_file: ERROR: can't open file: 404.html !\n");

			sp = stpushi (ERR_FILE_OPEN, sp, sp_bottom);
			if (sp == NULL)
			{
				// error
				printf ("socket_send_file: ERROR: stack corrupt!\n");
				return (NULL);
			}
			return (sp);
		}

		printf ("socket_send_file: sending 404.html file\n");

		strcpy ((char *) sockets[handle].buf, "HTTP/1.1 200 OK");
		strcat ((char *) sockets[handle].buf, "\n");
	}
	else
	{
		// file found

		// send 200 OK, file found

		strcpy ((char *) sockets[handle].buf, "HTTP/1.1 200 OK");
		strcat ((char *) sockets[handle].buf, "\n");

		header_len = strlen_safe ((const char *) sockets[handle].buf, MAXLINELEN);
		ret = exe_swrite (handle, header_len);
		if (ret != ERR_FILE_OK)
		{
			printf ("socket_send_file: ERROR: sending data!\n");

			// push ERROR code ERROR
			sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
			if (sp == NULL)
			{
				// error
				printf ("socket_send_file: ERROR: stack corrupt!\n");
				fclose (file);
				return (NULL);
			}
			fclose (file);
			return (sp);
		}
	}

	// get file size
	fseek (file, 0, SEEK_END);
	file_size = ftell (file);
	// rewind
	fseek (file, 0, SEEK_SET);

	// send server name
	strcpy ((char *) sockets[handle].buf, "Server: L1VM webserver/1.0");
	strcat ((char *) sockets[handle].buf, "\n");

	header_len = strlen_safe ((const char *) sockets[handle].buf, MAXLINELEN);
	ret = exe_swrite (handle, header_len);
	if (ret != ERR_FILE_OK)
	{
		printf ("socket_send_file: ERROR: sending data!\n");

		// push ERROR code ERROR
		sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("socket_send_file: ERROR: stack corrupt!\n");
			fclose (file);
			return (NULL);
		}
		fclose (file);
		return (sp);
	}

	sprintf ((char *) file_size_str, "%lli", file_size);

	// send content length
	strcpy ((char *) sockets[handle].buf, "Content-Length: ");

	header_len = strlen_safe ((const char *) sockets[handle].buf, MAXLINELEN);
	ret = exe_swrite (handle, header_len);
	if (ret != ERR_FILE_OK)
	{
		printf ("socket_send_file: ERROR: sending data!\n");

		// push ERROR code ERROR
		sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("socket_send_file: ERROR: stack corrupt!\n");
			fclose (file);
			return (NULL);
		}
		fclose (file);
		return (sp);
	}

	strcpy ((char *) sockets[handle].buf, (const char *) file_size_str);
	strcat ((char *) sockets[handle].buf, "\n");

	header_len = strlen_safe ((const char *) sockets[handle].buf, MAXLINELEN);
	ret = exe_swrite (handle, header_len);
	if (ret != ERR_FILE_OK)
	{
		printf ("socket_send_file: ERROR: sending data!\n");

		// push ERROR code ERROR
		sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("socket_send_file: ERROR: stack corrupt!\n");
			fclose (file);
			return (NULL);
		}
		fclose (file);
		return (sp);
	}

	// send connection close
	strcpy ((char *) sockets[handle].buf, "Connection: close");
	strcat ((char *) sockets[handle].buf, "\n");

	header_len = strlen_safe ((const char *) sockets[handle].buf, MAXLINELEN);
	ret = exe_swrite (handle, header_len);
	if (ret != ERR_FILE_OK)
	{
		printf ("socket_send_file: ERROR: sending data!\n");

		// push ERROR code ERROR
		sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("socket_send_file: ERROR: stack corrupt!\n");
			fclose (file);
			return (NULL);
		}
		fclose (file);
		return (sp);
	}

	// send content type and mimetype
	strcpy ((char *) sockets[handle].buf, "Content-Type: ");
	strcat ((char *) sockets[handle].buf, (const char *) &data[mimetype_addr]);
	strcat ((char *) sockets[handle].buf, "\n\n");

	header_len = strlen_safe ((const char *) sockets[handle].buf, MAXLINELEN);
	ret = exe_swrite (handle, header_len);
	if (ret != ERR_FILE_OK)
	{
		printf ("socket_send_file: ERROR: sending data!\n");

		// push ERROR code ERROR
		sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("socket_send_file: ERROR: stack corrupt!\n");
			fclose (file);
			return (NULL);
		}
		fclose (file);
		return (sp);
	}


	// send file
	for (i = 0; i < file_size; i++)
	{
		ch = fgetc (file);
		if (feof (file)) break;

		sockets[handle].buf[0] = (U1) ch;

		ret = exe_swrite (handle, sizeof (U1));
		if (ret != ERR_FILE_OK)
		{
			printf ("socket_send_file: ERROR: sending data!\n");

			// push ERROR code ERROR
			sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
			if (sp == NULL)
			{
				// error
				printf ("socket_send_file: ERROR: stack corrupt!\n");
				return (NULL);
			}
			fclose (file);
			return (sp);
		}
	}
	fclose (file);

	// push ERROR code OK
	sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("socket_send_file: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *socket_handle_get (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 get_stringaddr ALIGN;
	S8 filenameaddr ALIGN;
	S8 filenamelen ALIGN;
	U1 ch;
	S8 i ALIGN;
	S8 j ALIGN = 0;
	S8 slen ALIGN;
	S8 filestrlen ALIGN;
	U1 found_dot = 0;

	sp = stpopi ((U1 *) &filenamelen, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("socket_handle_get: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &filenameaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("socket_handle_get: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &get_stringaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("socket_handle_get: ERROR: stack corrupt!\n");
		return (NULL);
	}

	// printf ("socket_handle_get: GET: '%s'\n", &data[get_stringaddr]);

	slen = strlen_safe ((const char *) &data[get_stringaddr], 255);
	if (slen == 0)
	{
		// error empty string
		printf ("socket_handle_get: got empty input string!\n");

		sp = stpushi (1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("socket_handle_get: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}

	// printf ("socket_handle_get: slen: %lli\n", slen);

	if (slen > 3)
	{
		if (data[get_stringaddr] == 'G' && data[get_stringaddr + 1] == 'E' && data[get_stringaddr + 2] == 'T')
  		{
			// printf ("socket_handle_get: get filename from input...\n");

			j = 0;
	  		for (i = 4; i < slen; i++)
			{
				ch = data[get_stringaddr + i];

				if (ch != ' ')
				{
					data[filenameaddr + j] = ch;
					j++;
				}
				else
				{
					data[filenameaddr + j] = '\0';	// end of string
					break;
				}
			}
		}
	}

	if (j == 0)
	{
		// input was: GET / HTTP/1.1
		// set index.html as filename

		// printf ("filename len: %lli\n", filenamelen);

		if (filenamelen < 10)
		{
			// error filename string to small!
			printf ("socket_handle_get: filename string to short!\n");

			sp = stpushi (1, sp, sp_bottom);
			if (sp == NULL)
			{
				// error
				printf ("socket_handle_get: ERROR: stack corrupt!\n");
				return (NULL);
			}
			return (sp);
		}

		strcpy ((char *) &data[filenameaddr], "index.html");
	}
	// printf ("socket_handle_get: filename: '%s'\n", &data[filenameaddr]);

	filestrlen = strlen_safe ((const char *) &data[filenameaddr], MAXLINELEN);
	for (i = 0; i < filestrlen; i++)
	{
		// check if "." is in filename
		if (data[filenameaddr + i] == '.')
		{
			found_dot = 1;
			break;
		}
	}
	if (found_dot == 0)
	{
		// tackon .html file extension
		if (filestrlen + 5 < filenamelen)
		{
			strcat ((char *) &data[filenameaddr], ".html");
		}
	}

	sp = stpushi (0, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("socket_handle_get: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

// data client socket code for connection with l1vm-data server ===============
S2 socket_data_read_string (S2 handle, U1 *string, S8 string_len)
{
     /* read CRLF or LF terminated line */

    S8 ret ALIGN;
    U1 ch;
    U1 end = FALSE;
    U1 error = FALSE;
    S8 i ALIGN = 0;

	S2 sockh = sockets[handle].socket;

	while (! end)
 	{
		ret = recv (sockh, &ch, sizeof (U1), MSG_NOSIGNAL);
    	if (ret == -1)
    	{
        	error = TRUE;
        	end = TRUE;

        	break;
    	}

		// printf ("socket_data_read_string: ch: %i\n", ch);

        if (ch != '\n')
        {
            if (i <= string_len)
            {
                string[i] = ch;
                i++;
            }
            else
            {
               error = TRUE; end = TRUE;
            }
        }
        else
        {
            string[i] = '\0';
            end = TRUE;
        }
    }

	// printf ("socket_data_read_string: s: '%s'\n", string);

    if (error == FALSE)
    {
        return (0);
    }
    else
    {
        return (1);
    }
}

S2 socket_data_write_string (S2 handle, U1 *string)
{
     /* write string + LF terminated line */

    S8 ret ALIGN;
    U1 end = FALSE;
    S8 i ALIGN = -1;

    while (! end)
    {
		i++;
        sockets[handle].buf[i] = string[i];
        if (sockets[handle].buf[i] == '\0')
        {
            sockets[handle].buf[i] = '\n';
            end = TRUE;
        }
    }

	ret = exe_swrite (handle, i + 1);
	return (ret);
}


// get file code ==============================================================
U1 *socket_get_file (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// get file requested by "GET" HTTP command

	S8 i ALIGN;
	S8 j ALIGN;
	S8 n ALIGN;
	S8 requestaddr ALIGN;
	S2 handle;

	FILE *file;
	U1 file_name[256];
	U1 path_name[256];
	U1 file_size_str[256];
	S8 file_size ALIGN;
	U1 read_buf[256];
	U1 content_length[] = "Content-Length: ";
	U1 http[] = " HTTP/1.1";

	S8 header_len ALIGN;
	S8 ret ALIGN;

	U1 loop = 1;

	sp = stpopi ((U1 *) &requestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("socket_get_file: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("socket_get_file: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (strlen_safe ((char *) &data[requestaddr], MAXLINELEN) > MAXLINELEN - 5)
	{
		// error string too long
		printf ("socket_get_file: ERROR request size overflow!");
		return (NULL);
	}

	// send GET request to server
	strcpy ((char *) sockets[handle].buf, "GET ");
	strcat ((char *) sockets[handle].buf, (const char *) &data[requestaddr]);
	// add http version
	strcat ((char *) sockets[handle].buf, (const char *) http);
	strcat ((char *) sockets[handle].buf, "\n");

	// printf ("DEBUG: socket_get_file: '%s'\n", sockets[handle].buf);

	header_len = strlen_safe ((const char *) sockets[handle].buf, MAXLINELEN);
	ret = exe_swrite (handle, header_len);
	if (ret != ERR_FILE_OK)
	{
		printf ("socket_get_file: ERROR: sending data!\n");

		// push ERROR code ERROR
		sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("socket_get_file: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}

	// get response, wait for "Content_Length:"
	while (loop == 1)
	{
		if (socket_data_read_string (handle, read_buf, 255) != 0)
		{
			printf ("socket_get_file: ERROR reading data!\n");

			// push ERROR code ERROR
			sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
			if (sp == NULL)
			{
				// error
				printf ("socket_get_file: ERROR: stack corrupt!\n");
				return (NULL);
			}
			return (sp);
		}

		// printf ("DEBUG: socket_get_file: answ: '%s'\n", read_buf);

		if (strlen_safe ((const char *) read_buf, MAXLINELEN) > strlen_safe ((const char *) content_length, MAXLINELEN))
		{
			// check if content length was sent
			ret = 1;
			for (i = 0; i < strlen_safe ((const char *) content_length, MAXLINELEN); i++)
			{
				if (read_buf[i] != content_length[i])
				{
					ret = 0;
				}
			}
			if (ret == 1)
			{
				// found string, get file size
				n = 0;
				for (j = i; j < strlen_safe ((const char *) read_buf, MAXLINELEN); j++)
				{
					file_size_str[n] = read_buf[j];
					n++;
				}
				// printf ("DEBUG: socket_get_file: filesize: %s\n", file_size_str);
			}
		}
		if (strlen_safe ((const char *) read_buf, MAXLINELEN) == 0)
		{
			// got end of header
			loop = 0;
		}
	}

	// get number of file size string
	file_size = atoi ((const char *) file_size_str);

	// get file name of request
	for (i = strlen_safe ((const char *) &data[requestaddr], MAXLINELEN) - 1; i > 0; i--)
	{
		if (data[requestaddr + i] == '/')
		{
			n = 0;
			for (j = i + 1; j < strlen_safe ((const char *) &data[requestaddr], MAXLINELEN); j++)
			{
				file_name[n] = data[requestaddr + j];
				n++;
			}
			file_name[n] = '\0';
		}
	}

	// open file
	#if SANDBOX
	if (get_sandbox_filename (file_name, path_name, 255) != 0)
	{
		printf ("ERROR: socket_get_file: illegal filename: %s\n", file_name);
		return (NULL);
	}
	#else
	strcpy (path_name, file_name);
	#endif

	// printf ("DEBUG: socket_get_file: filename: '%s'\n", path_name);

	file = fopen ((char *) path_name, "w");
	if (file == NULL)
	{
		// push ERROR code ERROR
		printf ("socket_get_file: ERROR: can't open file: 404.html !\n");

		sp = stpushi (ERR_FILE_OPEN, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("socket_get_file: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}

	// get file
	for (i = 0; i < file_size; i++)
	{
		if (exe_sread (handle, 1) != ERR_FILE_OK)
		{
			printf ("socket_get_file: ERROR reading file data!");

			fclose (file);

			// push ERROR code ERROR
			sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
			if (sp == NULL)
			{
				// error
				printf ("socket_get_file: ERROR: stack corrupt!\n");
				return (NULL);
			}
			return (sp);
		}
		else
		{
			if (fputc (sockets[handle].buf[0], file) == EOF)
			{
				fclose (file);

				// push ERROR code ERROR
				sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
				if (sp == NULL)
				{
					// error
					printf ("socket_get_file: ERROR: stack corrupt!\n");
					return (NULL);
				}
				return (sp);
			}
		}
	}
	fclose (file);

	sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("socket_get_file: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}


// data send/receive ==========================================================

U1 *socket_store_int64 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 ret ALIGN;
    S2 handle;
	S8 name_addr ALIGN;
	S8 value ALIGN;

	U1 comm[] = "STORE INT64";
	U1 buf[512];
	S8 buf_len ALIGN = 511;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: socket_store_int64: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("socket_store_int64: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &name_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_store_int64: ERROR: stack corrupt!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_store_int64: ERROR: stack corrupt!\n");
        return (NULL);
    }

	// send command
	ret = socket_data_write_string (handle, comm);
    if (ret != ERR_FILE_OK)
    {
        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_store_int64: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	// send data element name
	ret = socket_data_write_string (handle, &data[name_addr]);
    if (ret != ERR_FILE_OK)
    {
        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_store_int64: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	// send data element value
	if (snprintf ((char *) &buf, buf_len, "%lli", value) <= 0)
	{
		// conversion failed
        sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_store_int64: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
	}

	ret = socket_data_write_string (handle, (U1 *) &buf);
    if (ret != ERR_FILE_OK)
    {
        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_store_int64: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	// get answer string
	ret = socket_data_read_string (handle, buf, buf_len);
	if (ret != ERR_FILE_OK)
    {
        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_store_int64: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	if (strcmp ((const char *) buf, "ERROR") == 0)
	{
		printf ("socket_store_int64: ERROR: storing data!\n");

		sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_store_int64: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
		return (sp);
	}
	else
	{
		sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
	    if (sp == NULL)
	    {
	        // error
	        printf ("socket_store_int64: ERROR: stack corrupt!\n");
	        return (NULL);
	    }
	    return (sp);
	}
}

U1 *socket_store_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 ret ALIGN;
    S2 handle;
	S8 name_addr ALIGN;
	F8 value ALIGN;

	U1 comm[] = "STORE DOUBLE";
	U1 buf[512];
	S8 buf_len ALIGN = 511;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: socket_store_double: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

	sp = stpopd ((U1 *) &value, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("socket_store_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &name_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_store_double: ERROR: stack corrupt!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_store_double: ERROR: stack corrupt!\n");
        return (NULL);
    }

	// send command
	ret = socket_data_write_string (handle, comm);
    if (ret != ERR_FILE_OK)
    {
        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_store_double: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	// send data element name
	ret = socket_data_write_string (handle, &data[name_addr]);
    if (ret != ERR_FILE_OK)
    {
        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_store_double: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	// send data element value
	if (snprintf ((char *) &buf, buf_len, "%10.10f", value) <= 0)
	{
		// conversion failed
        sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_store_double: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
	}

	ret = socket_data_write_string (handle, (U1 *) &buf);
    if (ret != ERR_FILE_OK)
    {
        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_store_double: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	// get answer string
	ret = socket_data_read_string (handle, buf, buf_len);
	if (ret != ERR_FILE_OK)
    {
        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_store_double: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	if (strcmp ((const char *) buf, "ERROR") == 0)
	{
		printf ("socket_store_double: ERROR: storing data!\n");

		sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_store_double: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
		return (sp);
	}
	else
	{
		sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
	    if (sp == NULL)
	    {
	        // error
	        printf ("socket_store_double: ERROR: stack corrupt!\n");
	        return (NULL);
	    }
	    return (sp);
	}
}

// string =====================================================================
U1 *socket_store_string (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 ret ALIGN;
    S2 handle;
	S8 name_addr ALIGN;
	S8 value_addr ALIGN;

	U1 comm[] = "STORE STRING";
	U1 buf[512];
	S8 buf_len ALIGN = 511;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: socket_store_string: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &value_addr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("socket_store_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &name_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_store_string: ERROR: stack corrupt!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_store_string: ERROR: stack corrupt!\n");
        return (NULL);
    }

	// send command
	ret = socket_data_write_string (handle, comm);
    if (ret != ERR_FILE_OK)
    {
        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_store_string: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	// send data element name
	ret = socket_data_write_string (handle, &data[name_addr]);
    if (ret != ERR_FILE_OK)
    {
        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_store_string: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	// send data element value
    ret = socket_data_write_string (handle, &data[value_addr]);
    if (ret != ERR_FILE_OK)
    {
        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_store_string: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	// get answer string
	ret = socket_data_read_string (handle, buf, buf_len);
	if (ret != ERR_FILE_OK)
    {
        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_store_string: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	if (strcmp ((const char *) buf, "ERROR") == 0)
	{
		printf ("socket_store_string: ERROR: storing data!\n");

		sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_store_string: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
		return (sp);
	}
	else
	{
		sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
	    if (sp == NULL)
	    {
	        // error
	        printf ("socket_store_string: ERROR: stack corrupt!\n");
	        return (NULL);
	    }
	    return (sp);
	}
}

// get ========================================================================
U1 *socket_get_int64 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 ret ALIGN;
	S8 value ALIGN;
    S2 handle;
	S8 name_addr ALIGN;

	U1 comm[] = "GET INT64";
	U1 buf[512];
	S8 buf_len ALIGN = 511;
	char *endp;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: socket_get_int64: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &name_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_get_int64: ERROR: stack corrupt!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_get_int64: ERROR: stack corrupt!\n");
        return (NULL);
    }

	// send command
	ret = socket_data_write_string (handle, comm);
    if (ret != ERR_FILE_OK)
    {
        // push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_get_int64: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_get_int64: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	// send data element name
	ret = socket_data_write_string (handle, &data[name_addr]);
    if (ret != ERR_FILE_OK)
    {
        // push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_get_int64: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_get_int64: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	// get answer string
	ret = socket_data_read_string (handle, buf, buf_len);
	if (ret != ERR_FILE_OK)
    {
        // push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_get_int64: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_get_int64: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	if (strcmp ((const char *) buf, "ERROR") == 0)
	{
		// push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_get_int64: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_get_int64: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
	}

	value = strtoll ((const char *) buf, &endp, 10);

	// DEBUG
	// printf ("socket_get_int64: string: '%s'\n", buf);
	// printf ("socket_get_int64: value: %lli\n", value);

    sp = stpushi (value, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("socket_get_int64: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("socket_get_int64: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}

U1 *socket_get_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 ret ALIGN;
	F8 value ALIGN;
    S2 handle;
	S8 name_addr ALIGN;

	U1 comm[] = "GET DOUBLE";
	U1 buf[512];
	S8 buf_len ALIGN = 511;
	char *endp;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: socket_get_double: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &name_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_get_double: ERROR: stack corrupt!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_get_double: ERROR: stack corrupt!\n");
        return (NULL);
    }

	// send command
	ret = socket_data_write_string (handle, comm);
    if (ret != ERR_FILE_OK)
    {
        // push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_get_double: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_get_double: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	// send data element name
	ret = socket_data_write_string (handle, &data[name_addr]);
    if (ret != ERR_FILE_OK)
    {
        // push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_get_double: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_get_double: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	// get answer string
	ret = socket_data_read_string (handle, buf, buf_len);
	if (ret != ERR_FILE_OK)
    {
        // push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_get_double: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_get_double: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	if (strcmp ((const char *) buf, "ERROR") == 0)
	{
		// push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_get_double: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_get_double: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
	}

	value = strtod ((const char *) buf, &endp);

	// DEBUG
	// printf ("socket_get_double: string: '%s'\n", buf);
	// printf ("socket_get_double: value: %lli\n", value);

    sp = stpushd (value, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("socket_get_double: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("socket_get_double: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}

// string =====================================================================
U1 *socket_get_string (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 ret ALIGN;
    S2 handle;
	S8 name_addr ALIGN;
    S8 return_str_addr ALIGN;

	U1 comm[] = "GET STRING";
	U1 buf[512];
	S8 buf_len ALIGN = 511;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: socket_get_string: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &return_str_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_get_string: ERROR: stack corrupt!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &name_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_get_string: ERROR: stack corrupt!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_get_string: ERROR: stack corrupt!\n");
        return (NULL);
    }

	// send command
	ret = socket_data_write_string (handle, comm);
    if (ret != ERR_FILE_OK)
    {
        // push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_get_string: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_get_string: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	// send data element name
	ret = socket_data_write_string (handle, &data[name_addr]);
    if (ret != ERR_FILE_OK)
    {
        // push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_get_string: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_get_string: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	// get answer string
	ret = socket_data_read_string (handle, buf, buf_len);
	if (ret != ERR_FILE_OK)
    {
        // push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_get_string: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_get_string: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	if (strcmp ((const char *) buf, "ERROR") == 0)
	{
		// push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_get_string: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_get_string: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
	}

    // copy buffer to return string
    { 
        S2 buf_len;
        buf_len = strlen_safe ((const char *) buf, MAXLINELEN);

        #if BOUNDSCHECK
        if (memory_bounds (return_str_addr, buf_len - 1) != 0)
        {
            printf ("socket_get_string: string oveflow!\n");
            return (NULL);
        }
        #endif

        strcpy ((char *) &data[return_str_addr], (const char *) buf);
    }
    
    sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("socket_get_string: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}


// remove =====================================================================

U1 *socket_remove_int64 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 ret ALIGN;
	S8 value ALIGN;
    S2 handle;
	S8 name_addr ALIGN;

	U1 comm[] = "REMOVE INT64";
	U1 buf[512];
	S8 buf_len ALIGN = 511;
	char *endp;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: socket_remove_int64: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &name_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_remove_int64: ERROR: stack corrupt!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_remove_int64: ERROR: stack corrupt!\n");
        return (NULL);
    }

	// send command
	ret = socket_data_write_string (handle, comm);
    if (ret != ERR_FILE_OK)
    {
        // push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_remove_int64: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_remove_int64: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	// send data element name
	ret = socket_data_write_string (handle, &data[name_addr]);
    if (ret != ERR_FILE_OK)
    {
        // push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_remove_int64: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_remove_int64: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	// get answer string
	ret = socket_data_read_string (handle, buf, buf_len);
	if (ret != ERR_FILE_OK)
    {
        // push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_remove_int64: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_remove_int64: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	if (strcmp ((const char *) buf, "ERROR") == 0)
	{
		// push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_remove_int64: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_remove_int64: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
	}

	value = strtoll ((const char *) buf, &endp, 10);

	// DEBUG
	// printf ("socket_remove_int64: string: '%s'\n", buf);
	// printf ("socket_remove_int64: value: %lli\n", value);

    sp = stpushi (value, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("socket_remove_int64: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("socket_remove_int64: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}

U1 *socket_remove_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 ret ALIGN;
	F8 value ALIGN;
    S2 handle;
	S8 name_addr ALIGN;

	U1 comm[] = "REMOVE DOUBLE";
	U1 buf[512];
	S8 buf_len ALIGN = 511;
	char *endp;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: socket_remove_double: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &name_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_remove_double: ERROR: stack corrupt!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_remove_double: ERROR: stack corrupt!\n");
        return (NULL);
    }

	// send command
	ret = socket_data_write_string (handle, comm);
    if (ret != ERR_FILE_OK)
    {
        // push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_remove_double: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_remove_double: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	// send data element name
	ret = socket_data_write_string (handle, &data[name_addr]);
    if (ret != ERR_FILE_OK)
    {
        // push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_remove_double: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_remove_double: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	// get answer string
	ret = socket_data_read_string (handle, buf, buf_len);
	if (ret != ERR_FILE_OK)
    {
        // push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_remove_double: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_remove_double: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	if (strcmp ((const char *) buf, "ERROR") == 0)
	{
		// push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_remove_double: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_remove_double: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
	}

	value = strtod ((const char *) buf, &endp);

	// DEBUG
	// printf ("socket_remove_double: string: '%s'\n", buf);
	// printf ("socket_remove_double: value: %lli\n", value);

    sp = stpushd (value, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("socket_remove_double: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("socket_remove_double: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}

// string =====================================================================
U1 *socket_remove_string (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 ret ALIGN;
    S2 handle;
	S8 name_addr ALIGN;
    S8 return_str_addr ALIGN;

	U1 comm[] = "REMOVE STRING";
	U1 buf[512];
	S8 buf_len ALIGN = 511;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: socket_remove_string: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &return_str_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_remove_string: ERROR: stack corrupt!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &name_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_remove_string: ERROR: stack corrupt!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_remove_string: ERROR: stack corrupt!\n");
        return (NULL);
    }

	// send command
	ret = socket_data_write_string (handle, comm);
    if (ret != ERR_FILE_OK)
    {
        // push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_remove_string: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_remove_string: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	// send data element name
	ret = socket_data_write_string (handle, &data[name_addr]);
    if (ret != ERR_FILE_OK)
    {
        // push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_remove_string: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_remove_string: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	// get answer string
	ret = socket_data_read_string (handle, buf, buf_len);
	if (ret != ERR_FILE_OK)
    {
        // push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_remove_string: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_remove_string: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	if (strcmp ((const char *) buf, "ERROR") == 0)
	{
		// push ZERO to stack, as empty read
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("socket_remove_string: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_remove_string: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
	}

    // copy buffer to return string
    { 
        S2 buf_len;
        buf_len = strlen_safe ((const char *) buf, MAXLINELEN);

        #if BOUNDSCHECK
        if (memory_bounds (return_str_addr, buf_len - 1) != 0)
        {
            printf ("socket_remove_string: string oveflow!\n");
            return (NULL);
        }
        #endif

        strcpy ((char *) &data[return_str_addr], (const char *) buf);
    }

    sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("socket_remove_string: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}

// get data info ==============================================================
U1 *socket_get_info (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 ret ALIGN;
    S2 handle;
	S8 name_addr ALIGN;
    S8 return_var_addr ALIGN;
	S8 return_type_addr ALIGN;

	U1 comm[] = "GET INFO";
	U1 buf[512];
	S8 buf_len ALIGN = 511;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: socket_get_info: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &return_type_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_get_info: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &return_var_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_get_info: ERROR: stack corrupt!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &name_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_get_info: ERROR: stack corrupt!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("socket_get_info: ERROR: stack corrupt!\n");
        return (NULL);
    }

	// send command
	ret = socket_data_write_string (handle, comm);
    if (ret != ERR_FILE_OK)
    {
        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_get_info: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	// send data element name
	ret = socket_data_write_string (handle, &data[name_addr]);
    if (ret != ERR_FILE_OK)
    {
        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_get_info: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	// get answer var name string
	ret = socket_data_read_string (handle, buf, buf_len);
	if (ret != ERR_FILE_OK)
    {
        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_get_info: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	if (strcmp ((const char *) buf, "ERROR") == 0)
	{
        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_get_info: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
	}

    // copy buffer to return string
    { 
        S2 buf_len;
        buf_len = strlen_safe ((const char *) buf, MAXLINELEN);

        #if BOUNDSCHECK
        if (memory_bounds (return_var_addr, buf_len - 1) != 0)
        {
            printf ("socket_get_info: string oveflow!\n");
            return (NULL);
        }
        #endif

        strcpy ((char *) &data[return_var_addr], (const char *) buf);
    }

	// get type
	ret = socket_data_read_string (handle, buf, buf_len);
	if (ret != ERR_FILE_OK)
    {
        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_get_info: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    // copy buffer to return string
    { 
        S2 buf_len;
        buf_len = strlen_safe ((const char *) buf, MAXLINELEN);

        #if BOUNDSCHECK
        if (memory_bounds (return_type_addr, buf_len - 1) != 0)
        {
            printf ("socket_get_info: string oveflow!\n");
            return (NULL);
        }
        #endif

        strcpy ((char *) &data[return_type_addr], (const char *) buf);
    }

	// get OK
	ret = socket_data_read_string (handle, buf, buf_len);
	if (ret != ERR_FILE_OK)
    {
        sp = stpushi (ret, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_get_info: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

	if (strcmp ((const char *) buf, "OK") != 0)
	{
        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("socket_get_info: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
	}

    sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("socket_get_info: ERROR: stack corrupt!\n");
        return (NULL);
    }
    return (sp);
}
