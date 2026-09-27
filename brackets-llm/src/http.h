/*
 * This file http.h is part of L1vm.
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
 */

#ifndef BRACKETS_HTTP_H
#define BRACKETS_HTTP_H

#include <stddef.h>

/* Parse "http://host:port" into host and port. Returns 0 on success.
 * Returns -1 for https:// URLs: this project targets a local llama-server. */
int http_parse_url(const char *url, char *host, size_t hostsz, int *port);

/* A URL split into its parts. `path` is heap-allocated (path + query). */
typedef struct {
    char host[256];
    int  port;        /* default filled in from the scheme */
    int  https;       /* 1 for https:// */
    char *path;       /* request target, never NULL after a successful split */
} HttpUrl;

/* Split "http(s)://host[:port]/path?query". Returns 0 on success, -1 when
 * the URL is not a usable http/https URL. Frees u->path internally on
 * failure; on success the caller releases it with http_url_free(). */
int http_split_url(const char *url, HttpUrl *u);
void http_url_free(HttpUrl *u);

/*
 * GET a document (used by the web_fetch tool). Fetches at most maxbytes
 * bytes of the body, follows redirects and understands "chunked" transfer
 * encoding. On success returns 0 and stores:
 *   *body        - the body, heap-allocated and NUL-terminated (may be ""),
 *   *bodylen     - the number of body bytes received (borrowed or NULL),
 *   *http_status - the status of the last response (e.g. 200), or NULL,
 *   *content_type- the Content-Type header without parameters (or NULL),
 *   *final_url   - the URL after redirects, heap-allocated (or NULL).
 * On error returns -1 and stores a short description in *errmsg
 * (heap-allocated) when errmsg != NULL. Every output pointer is optional.
 *
 * https:// is only available when the program is built with libcurl
 * (HAVE_LIBCURL); otherwise the "curl" program is used as a helper, and
 * when neither is available the call fails with a clear message.
 */
int http_get(const char *url, long maxbytes,
             char **body, long *bodylen, int *http_status,
             char **content_type, char **final_url, char **errmsg);

/*
 * POST a JSON body to /v1/chat/completions. On success returns 0 and stores
 * the response body (JSON) into `resp` (heap-allocated, NUL-terminated).
 * Returns -1 on connection/HTTP error. If http_status != NULL it receives
 * the HTTP status code (e.g. 200).
 */
int http_post_json(const char *url, const char *path,
                   const char *body, long bodylen,
                   char **resp, long *resplen, int *http_status);

#endif
