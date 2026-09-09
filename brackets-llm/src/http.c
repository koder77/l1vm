/*
 * This file http.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (info@midnight-coding.de), 2026
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

/*
 * brackets-llm - minimal HTTP client over raw POSIX sockets
 *
 * Speaks HTTP/1.1 to llama-server's OpenAI-compatible REST API.
 * No external dependencies beyond the C standard library + POSIX.
 */

#define _POSIX_C_SOURCE 200809L

#include "http.h"

#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "sb.h"

int http_parse_url(const char *url, char *host, size_t hostsz, int *port)
{
    const char *h = url;
    const char *colon;
    size_t n;

    if (strncmp(h, "http://", 7) == 0)
        h += 7;
    else if (strncmp(h, "https://", 8) == 0)
        return -1;   /* no TLS support; this project targets local llama-server */

    colon = strrchr(h, ':');
    if (colon && strchr(h, '/') == NULL) {
        n = (size_t)(colon - h);
        if (n >= hostsz)
            n = hostsz - 1;
        memcpy(host, h, n);
        host[n] = '\0';
        *port = atoi(colon + 1);
    } else {
        n = strlen(h);
        if (n >= hostsz)
            n = hostsz - 1;
        memcpy(host, h, n);
        host[n] = '\0';
        *port = 80;
    }
    if (*port <= 0 || *port > 65535)
        *port = 80;
    return 0;
}

static int connect_host(const char *host, int port)
{
    struct addrinfo hints, *res = NULL, *rp;
    char portstr[16];
    int fd = -1;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(portstr, sizeof(portstr), "%d", port);

    if (getaddrinfo(host, portstr, &hints, &res) != 0)
        return -1;

    for (rp = res; rp; rp = rp->ai_next) {
        fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (fd < 0)
            continue;
        if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0)
            break;
        close(fd);
        fd = -1;
    }
    freeaddrinfo(res);
    return fd;
}

int http_post_json(const char *url, const char *path,
                   const char *body, long bodylen,
                   char **resp, long *resplen, int *http_status)
{
    char host[256];
    int port;
    char req[4096];
    int fd;
    ssize_t n;
    SB out;
    long total = 0;
    long header_end = -1;
    int status = 0;
    char *rbody = NULL;

    *resp = NULL;
    if (resplen)
        *resplen = 0;
    if (http_status)
        *http_status = 0;

    if (http_parse_url(url, host, sizeof(host), &port) != 0)
        return -1;

    fd = connect_host(host, port);
    if (fd < 0)
        return -1;

    snprintf(req, sizeof(req),
             "POST %s HTTP/1.1\r\n"
             "Host: %s:%d\r\n"
             "Content-Type: application/json\r\n"
             "Accept: application/json\r\n"
             "Content-Length: %ld\r\n"
             "Connection: close\r\n"
             "\r\n",
             path, host, port, bodylen);

    if (write(fd, req, strlen(req)) < 0) {
        close(fd);
        return -1;
    }
    if (bodylen > 0 && write(fd, body, (size_t)bodylen) < 0) {
        close(fd);
        return -1;
    }

    sb_init(&out);
    {
        char buf[32768];
        for (;;) {
            n = read(fd, buf, sizeof(buf));
            if (n > 0)
                sb_addn(&out, buf, (size_t)n);
            else
                break;
        }
    }
    close(fd);

    /* scan headers */
    {
        const char *p = sb_cstr(&out);
        const char *e = strstr(p, "\r\n\r\n");
        if (e) {
            const char *sl = strchr(p, ' ');
            if (sl) {
                char num[8];
                size_t i;
                for (i = 0; i < sizeof(num) - 1 && sl[1 + i] >= '0' && sl[1 + i] <= '9'; i++)
                    num[i] = sl[1 + i];
                num[i] = '\0';
                status = atoi(num);
            }
            header_end = (long)(e - p);
            total = sb_len(&out);
            rbody = malloc((size_t)(total - header_end - 4) + 1);
            if (rbody) {
                memcpy(rbody, e + 4, (size_t)(total - header_end - 4));
                rbody[total - header_end - 4] = '\0';
            }
        }
    }

    if (http_status)
        *http_status = status;
    if (rbody) {
        *resp = rbody;
        if (resplen)
            *resplen = total - header_end - 4;
    }
    sb_free(&out);
    return rbody ? 0 : -1;
}
