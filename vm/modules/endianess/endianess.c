/*
 * This file endianess.c is part of L1vm.
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

 #include <sys/types.h>
 #if _WIN32
 	#include <winsock2.h>
 #else
 	#include <sys/socket.h>
 	#include <netinet/in.h>
 #endif

#include "../../../include/global.h"
#include "../../../include/stack.h"

// helper functions endianess
// htonq, ntohq, htond, ntohd

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

// library functions

// word

U1 *host_to_nw (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S2 host;
	S2 net;

	sp = stpopi ((U1 *) &host, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("host_to_nw: ERROR: stack corrupt!\n");
		return (NULL);
	}

	net = htons (host);

	sp = stpushi (net, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("host_to_nw: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *n_to_hostw (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S2 host;
	S2 net;

	sp = stpopi ((U1 *) &net, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("n_to_hostw: ERROR: stack corrupt!\n");
		return (NULL);
	}

	host = ntohs (net);

	sp = stpushi (host, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("n_to_hostw: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

// double word

U1 *host_to_ndw (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S4 host;
	S4 net;

	sp = stpopi ((U1 *) &host, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("host_to_ndw: ERROR: stack corrupt!\n");
		return (NULL);
	}

	net = htonl (host);

	sp = stpushi (net, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("host_to_ndw: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *n_to_hostdw (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S4 host;
	S4 net;

	sp = stpopi ((U1 *) &net, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("n_to_hostdw: ERROR: stack corrupt!\n");
		return (NULL);
	}

	host = ntohl (net);

	sp = stpushi (host, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("n_to_hostdw: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

// quad word (64 bit)

U1 *host_to_nqw (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 host ALIGN;
	S8 net ALIGN;

	sp = stpopi ((U1 *) &host, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("host_to_nqw: ERROR: stack corrupt!\n");
		return (NULL);
	}

	net = htonq (host);

	sp = stpushi (net, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("host_to_nqw: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *n_to_hostqw (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 host ALIGN;
	S8 net ALIGN;

	sp = stpopi ((U1 *) &net, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("n_to_hostqw: ERROR: stack corrupt!\n");
		return (NULL);
	}

	host = ntohq (net);

	sp = stpushi (host, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("n_to_hostqw: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}
