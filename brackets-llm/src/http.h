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

/* Parse "http://host:port" into host and port. Returns 0 on success. */
int http_parse_url(const char *url, char *host, size_t hostsz, int *port);

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
