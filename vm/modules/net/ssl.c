/*
 * This file ssl.c is part of L1vm.
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

// This code is based on a tutorial at https://aticleworld.com/ssl-server-client-using-openssl-in-c/
// It was modified to fit and I did add error checking to make it safer!

#include "../../../include/global.h"

#if __linux__ || __HAIKU__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <resolv.h>
#endif

#include "openssl/ssl.h"
#include "openssl/err.h"

// protos
void bzero (void *s, size_t n);

#define SOCKADDRESSLEN      16      /* max dotted ip len */
#define SOCKBUFSIZE         10240    /* socket data buffer len */

#define SOCKETOPEN 1              // state flags
#define SOCKETCLOSED 0

#define SOCKSERVER          0
#define SOCKCLIENT          1

#if _WIN32
#define SHUT_RDWR 2
#define MSG_NOSIGNAL MSG_WAITALL
#endif

#define FAIL -1

struct socket
{
    S2 socket;                                /* socket handle */
    S2 serv_conn;                           /* server connection */
    struct addrinfo *servinfo;
    U1 type;                                /* server / client */
    U1 state;                              /* open, closed */
    U1 ssl_conn;                                 /* 1 = SSL, 0 = normal */
    U1 client_ip[SOCKADDRESSLEN];           /* client ip */
    U1 buf[SOCKBUFSIZE];                   /* socket data buffer */

    // ssl stuff:
    SSL_CTX *ctx;
    SSL *ssl;
};

extern struct socket *sockets;
extern S8 socketmax;


U1 get_sandbox_filename (U1 *filename, U1 *sandbox_filename, S2 max_name_len);

size_t strlen_safe (const char * str, S8 maxlen);
S8 get_free_socket_handle (void);

// stack
U1 *stpushb (U1 data, U1 *sp, U1 *sp_bottom);
U1 *stpopb (U1 *data, U1 *sp, U1 *sp_top);
U1 *stpushi (S8 data, U1 *sp, U1 *sp_bottom);
U1 *stpopi (U1 *data, U1 *sp, U1 *sp_top);
U1 *stpushd (F8 data, U1 *sp, U1 *sp_bottom);
U1 *stpopd (U1 *data, U1 *sp, U1 *sp_top);
U1 *stack_type (U1 *data, U1 *sp, U1 *sp_top);

// server functions ==========================================================00
// Create the SSL socket and intialize the socket address structure
int OpenListener(int port)
{
    int sd;
    struct sockaddr_in addr;
    sd = socket(PF_INET, SOCK_STREAM, 0);
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sd, (struct sockaddr*)&addr, sizeof(addr)) != 0 )
    {
        perror("OpenListener: can't bind port");
        return (-1);
    }
    if ( listen(sd, 10) != 0 )
    {
        perror("OpenListener: can't configure listening port");
        return (-1);
    }
    return sd;
}

SSL_CTX* InitServerCTX(void)
{
    SSL_METHOD *method;
    SSL_CTX *ctx;
    OpenSSL_add_all_algorithms();  /* load & register all cryptos, etc. */
    SSL_load_error_strings();   /* load all error messages */
    method = (SSL_METHOD *) TLS_server_method();  /* create new server-method instance */
    ctx = SSL_CTX_new(method);   /* create new context from method */
    if ( ctx == NULL )
    {
        ERR_print_errors_fp(stderr);
        return (NULL);
    }
    return ctx;
}

int LoadCertificates(SSL_CTX* ctx, char* CertFile, char* KeyFile)
{
    /* set the local certificate from CertFile */
    if ( SSL_CTX_use_certificate_file(ctx, CertFile, SSL_FILETYPE_PEM) <= 0 )
    {
        ERR_print_errors_fp(stderr);
        return (1);
    }
    /* set the private key from KeyFile (may be the same as CertFile) */
    if ( SSL_CTX_use_PrivateKey_file(ctx, KeyFile, SSL_FILETYPE_PEM) <= 0 )
    {
        ERR_print_errors_fp(stderr);
        return (1);
    }
    /* verify private key */
    if ( !SSL_CTX_check_private_key(ctx) )
    {
        fprintf(stderr, "Private key does not match the public certificate\n");
        return (1);
    }

    // all ok
    return (0);
}

void ShowCerts(SSL* ssl)
{
    X509 *cert;
    char *line;
    cert = SSL_get_peer_certificate(ssl); /* Get certificates (if available) */
    if ( cert != NULL )
    {
        printf("Server certificates:\n");
        line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
        printf("Subject: %s\n", line);
        free(line);
        line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
        printf("Issuer: %s\n", line);
        free(line);
        X509_free(cert);
    }
    else
        printf("No certificates.\n");
}

// client functions ============================================================
int OpenConnection(const char *hostname, int port)
{
    int sd;
    struct hostent *host;
    struct sockaddr_in addr;
    if ( (host = gethostbyname(hostname)) == NULL )
    {
        perror(hostname);
        return (-1);
    }

    sd = socket(PF_INET, SOCK_STREAM, 0);
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = *(long*)(host->h_addr);

    if ( connect(sd, (struct sockaddr*)&addr, sizeof(addr)) != 0 )
    {
        close(sd);
        perror(hostname);
        return (-1);
    }

    return sd;
}

SSL_CTX* InitCTX(void)
{
    SSL_METHOD *method;
    SSL_CTX *ctx;
    OpenSSL_add_all_algorithms();  /* Load cryptos, et.al. */
    SSL_load_error_strings();   /* Bring in and register error messages */
    method = (SSL_METHOD *) TLS_client_method();  /* Create new client-method instance */
    ctx = SSL_CTX_new(method);   /* Create new context */
    if ( ctx == NULL )
    {
        ERR_print_errors_fp(stderr);
        return (NULL);
    }
    return ctx;
}

// server ======================================================================
U1 *open_server_socket_ssl (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S2 handle = -1;
    S8 certificate_addr ALIGN;
    S8 port ALIGN;

    S2 server;

    S8 file_name_len ALIGN;
    U1 certificate_name[256];

    S2 ret = 0;

    handle = get_free_socket_handle ();
    if (handle == -1)
    {
        /* error socket list full */
        sp = stpushi (ERR_FILE_OPEN, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_server_socket_ssl: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: open_server_socket_ssl: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &port, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("open_server_socket_ssl: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &certificate_addr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("open_server_socket_ssl: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (handle < 0 || handle >= socketmax)
    {
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_server_socket_ssl: ERROR: stack corrupt!\n");
			return (NULL);
		}

        printf ("ERROR: open_server_socket_ssl: handle out of range!\n");
        sp = stpushi (ERR_FILE_OPEN, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_server_socket_ssl: ERROR: stack corrupt!\n");
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
			printf ("open_server_socket_ssl: ERROR: stack corrupt!\n");
			return (NULL);
		}

        printf ("ERROR: open_server_socket_ssl: handle %i already open!\n", handle);
        sp = stpushi (ERR_FILE_OPEN, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_server_socket_ssl: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    // get certificate full name
    file_name_len = strlen_safe ((char *) &data[certificate_addr], 255);
	if (file_name_len > 255)
	{
		printf ("ERROR: open_server_socket_ssl: file name: '%s' too long!\n", (char *) &data[certificate_addr]);
		return (NULL);
	}

	#if SANDBOX
	if (get_sandbox_filename (&data[certificate_addr], certificate_name, 255) != 0)
	{
		printf ("ERROR: open_server_socket_ssl: illegal filename: %s\n", &data[certificate_addr]);
		return (NULL);
	}
	#else
	if (strlen_safe ((char *) &data[certificate_addr], 255) < 255)
	{
    	strcpy ((char *) certificate_name, (char *) &data[certificate_addr]);
	}
	else
	{
		printf ("ERROR: open_server_socket_ssl: filename %s too long!\n", &data[certificate_addr]);
		return (NULL);
	}
	#endif

    sockets[handle].ctx = InitServerCTX ();        /* initialize SSL */
    if (sockets[handle].ctx == NULL)
    {
        ret = 1;
    }
    else
    {
        if (LoadCertificates (sockets[handle].ctx, (char *) certificate_name, (char *) certificate_name) != 0)
        {
            ret = 1;
        }
    }

    server = OpenListener (port);    /* create server socket */
    if (server < 0)
    {
        // error
        ret = 1;
    }

    if (ret == 0)
    {
        // all ok!
        sp = stpushi (handle, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("open_server_socket_ssl: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("open_server_socket_ssl: ERROR: stack corrupt!\n");
            return (NULL);
        }
    }
    else
    {
        // error
        sp = stpushi (-1, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("open_server_socket_ssl: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("open_server_socket_ssl: ERROR: stack corrupt!\n");
            return (NULL);
        }
    }

    sockets[handle].socket = server;
    sockets[handle].type = SOCKSERVER;
    sockets[handle].state = SOCKETOPEN;
    sockets[handle].ssl_conn = 1;                     // SSL connection type
    strcpy ((char *) sockets[handle].client_ip, "");
    return (sp);
}

U1 *open_accept_server_ssl (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S2 handle;
    S2 client;
    S8 new_handle ALIGN = -1;

    struct sockaddr_in addr;
    socklen_t len = sizeof (addr);
    SSL *ssl;

    U1 ret = 0;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: open_accept_server_ssl: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("open_accept_server_ssl: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (handle < 0 || handle >= socketmax)
    {
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_accept_server_ssl: ERROR: stack corrupt!\n");
			return (NULL);
		}

		printf ("ERROR:  open_accept_server_ssl: handle out of range!\n");
        sp = stpushi (ERR_FILE_OPEN, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_accept_server_ssl: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    // check if SSL server socket
    if (sockets[handle].ssl_conn != 1)
    {
        // error, no SSL socket
        sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_accept_server_ssl: ERROR: stack corrupt!\n");
			return (NULL);
		}

        printf ("ERROR: open_accept_server_ssl: handle not a SSL server socket!\n");
        sp = stpushi (ERR_FILE_OPEN, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_accept_server_ssl: ERROR: stack corrupt!\n");
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
			printf ("open_accept_server_ssl: ERROR: stack corrupt!\n");
			return (NULL);
		}

        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_accept_server_ssl: ERROR: stack corrupt!\n");
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
			printf ("open_accept_server_ssl: ERROR: stack corrupt!\n");
			return (NULL);
		}

        /* error socket list full */
        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_accept_server_ssl: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    client = accept (sockets[handle].socket, (struct sockaddr*) &addr, &len);  /* accept connection as usual */
    if (client == -1)
    {
        printf ("open_accept_server_ssl accept failed!\n");


		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_accept_server_ssl: ERROR: stack corrupt!\n");
			return (NULL);
		}

        sp = stpushi (errno, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_accept_server_ssl: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    // printf("Connection: %s:%d\n",inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
    ssl = SSL_new (sockets[handle].ctx);              /* get new SSL state with context */
    if (ssl == NULL)
    {
        ret = 1;
    }

    if (SSL_set_fd (ssl, client) == 0)
    {
        ret = 1;
    }

    {
         S8 ret_accept;
         ret_accept = SSL_accept (ssl);

         if (ret_accept == FAIL)
         {   /* do SSL-protocol accept */
             printf ("SSL_accept error!\n");
             ERR_print_errors_fp (stderr);
         }
         else
         {
             ShowCerts (ssl);        /* get any certificates */
         }
     }

    if (ret == 1)
    {
        // error happened before
        sp = stpushi (-1, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("open_accept_server_ssl: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("open_accept_server_ssl: ERROR: stack corrupt!\n");
            return (NULL);
        }
        // exit on error!
        return (sp);
    }

    sockets[new_handle].state = SOCKETOPEN;
    sockets[new_handle].type = SOCKSERVER;
    sockets[new_handle].ssl = ssl;
    sockets[new_handle].ssl_conn = 1;

    sp = stpushi (new_handle, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("open_accept_server_ssl: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("open_accept_server_ssl: ERROR: stack corrupt!\n");
        return (NULL);
    }

    return (sp);
}

U1 *close_accept_server_ssl (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S2 handle;
    S2 sd;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: close_accept_server_ssl: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("close_accept_server_ssl: ERROR: stack corrupt!\n");
        return (NULL);
    }

    if (handle < 0 || handle > socketmax - 1)
    {
        sp = stpushi (ERR_FILE_NUMBER, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_accept_server_ssl: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    // check if SSL server socket
    if (sockets[handle].ssl_conn != 1)
    {
        // error, no SSL socket
        printf ("ERROR: close_accept_server_ssl: handle not a SSL server socket!\n");
        sp = stpushi (ERR_FILE_OPEN, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_accept_server_ssl: ERROR: stack corrupt!\n");
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
    		printf ("close_accept_server_ssl: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    sd = SSL_get_fd (sockets[handle].ssl);       /* get socket connection */
    SSL_free (sockets[handle].ssl);               /* release SSL state */

    if (close (sd) == -1)
    {
        sp = stpushi (ERR_FILE_CLOSE, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_accept_server_ssl: ERROR: stack corrupt!\n");
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
    		printf ("close_accept_server_ssl: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }
}

U1 *close_server_socket_ssl (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S2 handle;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: close_server_socket_ssl: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("close_server_socket_ssl: ERROR: stack corrupt!\n");
        return (NULL);
    }

    if (handle < 0 || handle > socketmax - 1)
    {
        sp = stpushi (ERR_FILE_NUMBER, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_server_socket_ssl: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    // check if SSL server socket
    if (sockets[handle].ssl_conn != 1)
    {
        // error, no SSL socket
        printf ("ERROR: close_server_socket_ssl: handle not a SSL server socket!\n");
        sp = stpushi (ERR_FILE_OPEN, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_server_socket_ssl: ERROR: stack corrupt!\n");
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
    		printf ("close_server_socket_ssl: ERROR: stack corrupt!\n");
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
    		printf ("close_server_socket_ssl: ERROR: stack corrupt!\n");
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
    		printf ("close_server_socket_ssl: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }
    else
    {
        sockets[handle].state = SOCKETCLOSED;

        // servinfo now points to a linked list of 1 or more struct addrinfos
        // ... do everything until you don't need servinfo anymore ....
        //freeaddrinfo (sockets[handle].servinfo); // free the linked-list

        SSL_CTX_free (sockets[handle].ctx);

        sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_server_socket_ssl: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }
}

// client ======================================================================
U1 *open_client_socket_ssl (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    U1 *hostname;
    S8 hostname_addr ALIGN;
    S2 handle = -1, port;

    SSL_CTX *ctx;
    int server;
    SSL *ssl;

    handle = get_free_socket_handle ();
    if (handle == -1)
    {
        /* error socket list full */
        sp = stpushi (ERR_FILE_OPEN, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("open_client_socket_ssl: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: open_client_socket_ssl: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &port, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("open_client_socket_ssl: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &hostname_addr, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("open_client_socket_ssl: ERROR: stack corrupt!\n");
        return (NULL);
    }

    hostname = &data[hostname_addr];

    if (handle < 0 || handle >= socketmax)
    {
		sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_client_socket_ssl: ERROR: stack corrupt!\n");
			return (NULL);
		}

        printf ("ERROR: open_client_socket_ssl: handle out of range!\n");
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
			printf ("open_client_socket_ssl: ERROR: stack corrupt!\n");
			return (NULL);
		}

        printf ("ERROR: open_client_socket_ssl: handle %i already open!\n", handle);
        sp = stpushi (ERR_FILE_OPEN, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_client_socket_ssl: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    ctx = InitCTX ();
    if (ctx == NULL)
    {
        // error!
        sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_client_socket_ssl: ERROR: stack corrupt!\n");
			return (NULL);
		}

        sp = stpushi (errno, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_client_socket_ssl: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    server = OpenConnection ((const char *) &data[hostname_addr], port);
    if (server < 0)
    {
        // error!
        sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_client_socket_ssl: ERROR: stack corrupt!\n");
			return (NULL);
		}

        sp = stpushi (errno, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_client_socket_ssl: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    ssl = SSL_new (ctx);      /* create new SSL connection state */
    SSL_set_fd (ssl, server);    /* attach the socket descriptor */

    if ( SSL_connect (ssl) == FAIL )
    {
        // error!
        ERR_print_errors_fp (stderr);

        sp = stpushi (-1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("open_client_socket_ssl: ERROR: stack corrupt!\n");
			return (NULL);
		}

        sp = stpushi (errno, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("open_client_socket_ssl: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    ShowCerts (ssl);

    sp = stpushi (handle, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("open_client_socket_ssl: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    if (sp == NULL)
    {
        // error
        printf ("open_client_socket_ssl: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sockets[handle].socket = server;
    sockets[handle].ssl = ssl;
    sockets[handle].type = SOCKCLIENT;
    sockets[handle].state = SOCKETOPEN;
    sockets[handle].ssl_conn = 1;                     // SSL connection type
    return (sp);
}

U1 *close_client_socket_ssl (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S2 handle;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: close_client_socket_ssl: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("close_client_socket_ssl: ERROR: stack corrupt!\n");
        return (NULL);
    }

    if (handle < 0 || handle > socketmax - 1)
    {
        sp = stpushi (ERR_FILE_NUMBER, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_client_socket_ssl: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }

    // check if SSL client socket
    if (sockets[handle].ssl_conn != 1)
    {
        // error, no SSL socket
        printf ("ERROR: close_client_socket_ssl: handle not a SSL server socket!\n");
        sp = stpushi (ERR_FILE_OPEN, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_client_socket_ssl: ERROR: stack corrupt!\n");
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
    		printf ("close_client_socket_ssl: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }


    SSL_free (sockets[handle].ssl);        /* release connection state */

    if (close (sockets[handle].socket) == -1)
    {
        sp = stpushi (ERR_FILE_CLOSE, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_client_socket_ssl: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }
    else
    {
        sockets[handle].state = SOCKETCLOSED;
        SSL_CTX_free (sockets[handle].ctx);        /* release context */

        sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("close_client_socket_ssl: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
        return (sp);
    }
}
