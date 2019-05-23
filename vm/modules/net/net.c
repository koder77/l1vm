/*
 * This file net.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2018
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

#define MAXSOCKETS 32             // max number of open sockets

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

struct socket sockets[MAXSOCKETS];

size_t strlen_safe (const char * str, int maxlen);

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
    // set all socket handles as CLOSED
    S8 i ALIGN;

    for (i = 0; i < MAXSOCKETS; i++)
    {
        sockets[i].state = SOCKETCLOSED;
    }

    return (sp);
}


U1 *open_server_socket (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S2 handle = -1;
    U1 *hostname;
    S8 hostname_addr ALIGN;
    S8 port ALIGN;

    S8 i ALIGN = 0;

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

    for (i = 0; i < MAXSOCKETS; i++)
    {
        if (sockets[i].state == SOCKETCLOSED)
        {
            handle = i;
            break;
        }
    }

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

    if (handle < 0 || handle >= MAXSOCKETS)
    {
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
        printf ("open_accept_server: ERROR: stack corrupt!\n");
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
    S2 i;
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

    if (handle < 0 || handle >= MAXSOCKETS)
    {
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
        sp = stpushi (errno, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_accept_server: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    for (i = 0; i < MAXSOCKETS; i++)
    {
        if (sockets[i].state == SOCKETCLOSED)
        {
            new_handle = i;
            break;
        }
    }

    if (new_handle == -1)
    {
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
    S4 i;
    S2 handle = -1, port;

    struct addrinfo hints;
    struct addrinfo *servinfo;
    // will point to the results
    memset (&hints, 0, sizeof hints); // make sure the struct is empty
    hints.ai_family = AF_UNSPEC;
    // don't care IPv4 or IPv6

    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

    for (i = 0; i < MAXSOCKETS; i++)
    {
        if (sockets[i].state == SOCKETCLOSED)
        {
            handle = i;
            break;
        }
    }

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

    if (handle < 0 || handle >= MAXSOCKETS)
    {
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
        printf ("open_accept_server: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("open_server_socket: ERROR: stack corrupt!\n");
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

    if (handle < 0 || handle > MAXSOCKETS - 1)
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

    if (handle < 0 || handle > MAXSOCKETS - 1)
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

    if (handle < 0 || handle > MAXSOCKETS - 1)
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

    if (handle < 0 || handle > MAXSOCKETS - 1)
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
        printf ("getclientaddr: ERROR: stack corrupt!\n");
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

    hostname_len = strlen ((char *) hostname) + 1;
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
        if (strlen (inet_ntoa (addr)) <= 256)
        {
            strcpy ((char *) ret, inet_ntoa (addr));

            ret_len = strlen ((char *) ret) + 1;
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
S8 htonq (S8 num ALIGN)
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

S8 ntohq (S8 num ALIGN)
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
S8 htonq (S8 num ALIGN)
{
	return (num);
}

S8 ntohq (S8 num ALIGN)
{
	return (num);
}
#endif

F8 htond (F8 hostd ALIGN)
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

F8 ntohd (F8 netd ALIGN)
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

    if (slist_ind < 0 || slist_ind > MAXSOCKETS - 1)
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

    if (slist_ind < 0 || slist_ind > MAXSOCKETS - 1)
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

            if (i == 0)
            {
                /* error at first read, break while */
                break;
            }
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
            /* check if last char was a CR */

            if (data[ret_addr + i - 1] == '\r')
            {
                i--;
            }

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

    ret = exe_swrite (handle, sizeof (U1));
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
    S8 i ALIGN = 0;

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
        sockets[handle].buf[i] = data[send_addr + i];
        if (sockets[handle].buf[i] == '\0')
        {
            i++;
            sockets[handle].buf[i] = '\n';
            end = TRUE;
        }
        i++;
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
