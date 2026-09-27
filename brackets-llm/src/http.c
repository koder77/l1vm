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
 *
 * http_get() additionally fetches documents from the internet for the
 * web_fetch tool. Plain http:// is done here, always. https:// needs TLS and
 * is provided by libcurl when the build found it (-DHAVE_LIBCURL), otherwise
 * by the "curl" command line program used as a helper.
 */

#define _POSIX_C_SOURCE 200809L

#include "http.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "intr.h"
#include "sb.h"

#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif

#define HTTP_UA "brackets-llm (web_fetch tool)"
#define HTTP_MAX_REDIRS 5
#define HTTP_META_TAG "<<<BRACKETS_HTTP_META"
#define HTTP_HEAD_SLACK 65536L     /* room for response headers */

/* ---------------------------------------------------------------- */
/* small helpers                                                    */
/* ---------------------------------------------------------------- */

static const char *find_bytes(const char *hay, long hlen, const char *needle,
                              size_t nlen)
{
    long i;

    if (nlen == 0 || hlen < (long)nlen)
        return NULL;
    for (i = 0; i <= hlen - (long)nlen; i++) {
        if (hay[i] == needle[0] && memcmp(hay + i, needle, nlen) == 0)
            return hay + i;
    }
    return NULL;
}

static int ci_contains(const char *hay, const char *needle)
{
    size_t nl = strlen(needle);
    size_t i;

    if (nl == 0)
        return 1;
    for (i = 0; hay[i]; i++) {
        if (strncasecmp(hay + i, needle, nl) == 0)
            return 1;
    }
    return 0;
}

static char *trim_dup(const char *s, size_t n)
{
    char *r;

    while (n > 0 && (*s == ' ' || *s == '\t'))
        s++, n--;
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r'))
        n--;
    r = malloc(n + 1);
    if (!r)
        return NULL;
    memcpy(r, s, n);
    r[n] = '\0';
    return r;
}

#ifndef HAVE_LIBCURL
static void trim_in_place(char *s)
{
    size_t n = strlen(s);
    char *p = s;

    while (n > 0 && (*p == ' ' || *p == '\t'))
        p++, n--;
    while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t' || p[n - 1] == '\r'))
        n--;
    memmove(s, p, n);
    s[n] = '\0';
}
#endif

/* ---------------------------------------------------------------- */
/* URL handling                                                     */
/* ---------------------------------------------------------------- */

void http_url_free(HttpUrl *u)
{
    if (!u)
        return;
    free(u->path);
    u->path = NULL;
}

int http_split_url(const char *url, HttpUrl *u)
{
    const char *h, *slash, *at, *colon;
    size_t n;
    char *endp;
    long p;

    memset(u, 0, sizeof(*u));
    if (!url || !*url)
        return -1;

    /* no spaces, no control characters: the URL goes into a request line */
    for (const char *c = url; *c; c++) {
        unsigned char uc = (unsigned char)*c;
        if (uc <= 0x20 || uc == 0x7f)
            return -1;
    }

    if (strncasecmp(url, "http://", 7) == 0) {
        h = url + 7;
        u->port = 80;
    } else if (strncasecmp(url, "https://", 8) == 0) {
        h = url + 8;
        u->https = 1;
        u->port = 443;
    } else {
        return -1;   /* only http and https */
    }

    slash = strchr(h, '/');
    n = slash ? (size_t)(slash - h) : strlen(h);
    if (n == 0 || n >= sizeof(u->host))
        return -1;
    memcpy(u->host, h, n);
    u->host[n] = '\0';

    /* strip user info: user:password@host */
    at = strrchr(u->host, '@');
    if (at) {
        n = strlen(at + 1);
        if (n == 0 || n >= sizeof(u->host))
            return -1;
        memmove(u->host, at + 1, n + 1);
    }

    /* port (ignore the colons of an IPv6 literal "[::1]:8080") */
    colon = strrchr(u->host, ':');
    if (colon && strchr(u->host, ']') == NULL) {
        p = strtol(colon + 1, &endp, 10);
        if (endp == colon + 1 || *endp != '\0' || p <= 0 || p > 65535)
            return -1;
        *((char *)colon) = '\0';
        u->port = (int)p;
    }
    if (!u->host[0])
        return -1;
    for (const char *c = u->host; *c; c++) {
        if (!(isalnum((unsigned char)*c) || *c == '.' || *c == '-' ||
              *c == '_'))
            return -1;
    }

    u->path = strdup(slash ? slash : "/");
    if (!u->path)
        return -1;
    return 0;
}

int http_parse_url(const char *url, char *host, size_t hostsz, int *port)
{
    HttpUrl u;
    char tmp[2048];
    const char *in = url;
    size_t n;

    /* a bare "host:port" is accepted here, the way older configs used it */
    if (url && !strstr(url, "://")) {
        if (strlen(url) + 8 >= sizeof(tmp))
            return -1;
        snprintf(tmp, sizeof(tmp), "http://%s", url);
        in = tmp;
    }
    if (http_split_url(in, &u) != 0)
        return -1;
    if (u.https) {
        /* no TLS here: this call is only used for the local llama-server */
        http_url_free(&u);
        return -1;
    }
    n = strlen(u.host);
    if (n >= hostsz)
        n = hostsz - 1;
    memcpy(host, u.host, n);
    host[n] = '\0';
    *port = u.port;
    http_url_free(&u);
    return 0;
}

/* ---------------------------------------------------------------- */
/* response reading                                                 */
/* ---------------------------------------------------------------- */

typedef struct {
    int  status;
    long clen;        /* -1: not announced */
    int  chunked;
    char *ctype;      /* Content-Type without parameters (heap) */
    char *location;   /* Location header (heap) */
    long body_off;    /* offset of the body inside the raw buffer */
} RespInfo;

/* Read until EOF, until `cap` bytes were collected or Ctrl+C arrives.
 * Returns 1 when interrupted, 0 otherwise (a read error is handled like
 * EOF: whatever arrived is used). */
static int read_all(int fd, SB *out, long cap)
{
    char buf[32768];

    for (;;) {
        ssize_t n;
        size_t room;

        if (g_intr_request)
            return 1;
        n = read(fd, buf, sizeof(buf));
        if (n > 0) {
            room = (cap > 0 && (long)sb_len(out) + n > cap)
                       ? (size_t)(cap - (long)sb_len(out))
                       : (size_t)n;
            if (room)
                sb_addn(out, buf, room);
            if (cap > 0 && (long)sb_len(out) >= cap)
                return 0;   /* enough for our purposes */
        } else if (n < 0 && errno == EINTR) {
            continue;      /* unrelated signal: g_intr_request checked above */
        } else {
            break;         /* EOF or error */
        }
    }
    return 0;
}

static int write_all(int fd, const char *buf, size_t len)
{
    size_t off = 0;

    while (off < len) {
        ssize_t n = write(fd, buf + off, len - off);
        if (n > 0) {
            off += (size_t)n;
        } else if (n < 0 && errno == EINTR) {
            if (g_intr_request)
                return -1;
        } else {
            return -1;
        }
    }
    return 0;
}

/* Value of header `name` (case-insensitive) inside the header block, or NULL. */
static const char *find_header(const char *hdrs, long hlen, const char *name,
                               size_t *vlen)
{
    size_t nl = strlen(name);
    const char *p = hdrs;
    const char *end = hdrs + hlen;

    while (p < end) {
        const char *eol = memchr(p, '\n', (size_t)(end - p));
        size_t linelen = eol ? (size_t)(eol - p) : (size_t)(end - p);
        if (linelen > nl && strncasecmp(p, name, nl) == 0 && p[nl] == ':') {
            const char *v = p + nl + 1;
            size_t vl = linelen - nl - 1;
            if (vl && v[vl - 1] == '\r')
                vl--;
            *vlen = vl;
            return v;
        }
        if (!eol)
            break;
        p = eol + 1;
    }
    return NULL;
}

/* Decode a "chunked" transfer-encoded body. */
static void dechunk(const char *in, long len, SB *out)
{
    long i = 0;

    while (i < len) {
        long j = i, sz = 0;
        int any = 0;

        while (j < len && in[j] != '\n') {           /* chunk size line */
            char c = in[j];
            int d;
            if (c >= '0' && c <= '9')          d = c - '0';
            else if (c >= 'a' && c <= 'f')     d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F')     d = c - 'A' + 10;
            else { j++; continue; }                   /* extensions: skip */
            sz = sz * 16 + d;
            any = 1;
            j++;
        }
        if (!any || j >= len)
            break;
        i = j + 1;                                   /* skip the '\n' */
        if (sz <= 0)
            break;                                   /* last chunk */
        if (i + sz > len)
            sz = len - i;
        sb_addn(out, in + i, (size_t)sz);
        i += sz;
        while (i < len && in[i] != '\n')              /* skip the trailer */
            i++;
        if (i < len)
            i++;
    }
}

/* Split a raw response into status, interesting headers and body. */
static void parse_response(const char *raw, long len, RespInfo *ri, SB *body)
{
    const char *eol;
    long hdrlen;
    const char *v;
    size_t vl;

    memset(ri, 0, sizeof(*ri));
    ri->clen = -1;
    sb_init(body);

    eol = (len >= 4) ? find_bytes(raw, len, "\r\n\r\n", 4) : NULL;
    if (eol) {
        hdrlen = (long)(eol - raw) + 4;             /* skip "\r\n\r\n" */
    } else {
        eol = (len >= 2) ? find_bytes(raw, len, "\n\n", 2) : NULL;
        hdrlen = eol ? (long)(eol - raw) + 2 : len;
    }
    if (hdrlen > len)
        hdrlen = len;

    /* status line: "HTTP/1.1 200 OK" */
    if (len > 12 && strncmp(raw, "HTTP/", 5) == 0) {
        const char *sp = memchr(raw, ' ', (size_t)(len < 32 ? len : 32));
        if (sp) {
            char num[4];
            size_t i = 0;
            const char *q = sp + 1;
            while (*q >= '0' && *q <= '9' && i < 3) {
                num[i++] = *q++;
            }
            num[i] = '\0';
            if (i)
                ri->status = atoi(num);
        }
    }

    v = find_header(raw, hdrlen, "content-length", &vl);
    if (v) {
        char *num = trim_dup(v, vl);
        if (num) {
            ri->clen = strtol(num, NULL, 10);
            free(num);
        }
    }
    v = find_header(raw, hdrlen, "transfer-encoding", &vl);
    if (v) {
        char *val = trim_dup(v, vl);
        if (val) {
            if (ci_contains(val, "chunked"))
                ri->chunked = 1;
            free(val);
        }
    }
    v = find_header(raw, hdrlen, "content-type", &vl);
    if (v) {
        char *val = trim_dup(v, vl);
        if (val) {
            char *semi = strchr(val, ';');
            if (semi)
                *semi = '\0';
            ri->ctype = val;
        }
    }
    v = find_header(raw, hdrlen, "location", &vl);
    if (v)
        ri->location = trim_dup(v, vl);

    ri->body_off = hdrlen;
    if (ri->chunked) {
        dechunk(raw + hdrlen, len - hdrlen, body);
    } else {
        long blen = len - hdrlen;
        if (ri->clen >= 0 && ri->clen < blen)
            blen = ri->clen;
        if (blen > 0)
            sb_addn(body, raw + hdrlen, (size_t)blen);
    }
}

/* ---------------------------------------------------------------- */
/* plain HTTP GET (internal, no TLS)                                */
/* ---------------------------------------------------------------- */

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
        if (g_intr_request)
            break;
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

/* Resolve a Location header against the current URL into `out`. */
static int resolve_redirect(const char *base, const char *loc, char *out,
                            size_t outsz)
{
    HttpUrl u;
    char bpath[1024];

    if (!loc || !*loc)
        return -1;
    if (strncasecmp(loc, "http://", 7) == 0 ||
        strncasecmp(loc, "https://", 8) == 0) {
        snprintf(out, outsz, "%s", loc);
        return 0;
    }
    if (http_split_url(base, &u) != 0)
        return -1;
    snprintf(bpath, sizeof(bpath), "%s", u.path);
    if (loc[0] == '/') {
        snprintf(out, outsz, "http%s://%s:%d%s", u.https ? "s" : "", u.host,
                 u.port, loc);
    } else {
        char *last = strrchr(bpath, '/');
        if (last)
            last[1] = '\0';      /* relative to the current directory */
        snprintf(out, outsz, "http%s://%s:%d%s%s", u.https ? "s" : "", u.host,
                 u.port, bpath, loc);
    }
    http_url_free(&u);
    return 0;
}

static int fetch_plain(const char *url, long maxbytes, char **body,
                       long *bodylen, int *http_status, char **content_type,
                       char **final_url, SB *err)
{
    char cur[2048];
    int hop;

    snprintf(cur, sizeof(cur), "%s", url);
    for (hop = 0; hop <= HTTP_MAX_REDIRS; hop++) {
        HttpUrl u;
        char req[8192];
        int fd, intr;
        SB raw, bodysb;
        RespInfo ri;
        char *res;

        if (http_split_url(cur, &u) != 0) {
            sb_printf(err, "invalid URL '%s'", cur);
            return -1;
        }
        if (u.https) {
            http_url_free(&u);
            sb_add(err, "https is not available in this build");
            return -1;
        }
        fd = connect_host(u.host, u.port);
        if (fd < 0) {
            sb_printf(err, "cannot connect to %s:%d", u.host, u.port);
            http_url_free(&u);
            return -1;
        }

        snprintf(req, sizeof(req),
                 "GET %s HTTP/1.1\r\n"
                 "Host: %s\r\n"
                 "User-Agent: %s\r\n"
                 "Accept: */*\r\n"
                 "Accept-Encoding: identity\r\n"
                 "Connection: close\r\n"
                 "\r\n",
                 u.path, u.host, HTTP_UA);
        if (write_all(fd, req, strlen(req)) < 0) {
            sb_printf(err, "cannot send the request to %s", u.host);
            http_url_free(&u);
            close(fd);
            return -1;
        }
        http_url_free(&u);

        sb_init(&raw);
        intr = read_all(fd, &raw, maxbytes + HTTP_HEAD_SLACK);
        close(fd);
        if (intr) {
            sb_free(&raw);
            sb_add(err, "interrupted");
            return -1;
        }

        parse_response(sb_cstr(&raw), (long)sb_len(&raw), &ri, &bodysb);

        if ((ri.status == 301 || ri.status == 302 || ri.status == 303 ||
             ri.status == 307 || ri.status == 308) && ri.location) {
            char next[2048];
            int ok;

            if (hop >= HTTP_MAX_REDIRS) {
                sb_printf(err, "too many redirects (more than %d)",
                        HTTP_MAX_REDIRS);
                free(ri.ctype);
                free(ri.location);
                sb_free(&bodysb);
                sb_free(&raw);
                return -1;
            }
            ok = resolve_redirect(cur, ri.location, next, sizeof(next)) == 0;
            if (!ok)
                sb_printf(err, "cannot follow the redirect to '%s'", ri.location);
            free(ri.ctype);
            free(ri.location);
            sb_free(&bodysb);
            sb_free(&raw);
            if (!ok)
                return -1;
            snprintf(cur, sizeof(cur), "%s", next);
            continue;
        }

        res = strdup(sb_cstr(&bodysb));
        if (!res) {
            free(ri.ctype);
            free(ri.location);
            sb_free(&bodysb);
            sb_free(&raw);
            sb_add(err, "out of memory");
            return -1;
        }
        *body = res;
        if (bodylen)
            *bodylen = (long)sb_len(&bodysb);
        if (http_status)
            *http_status = ri.status;
        if (content_type) {
            *content_type = ri.ctype;
            ri.ctype = NULL;
        }
        if (final_url)
            *final_url = strdup(cur);
        free(ri.ctype);
        free(ri.location);
        sb_free(&bodysb);
        sb_free(&raw);
        return 0;
    }

    sb_printf(err, "too many redirects (more than %d)", HTTP_MAX_REDIRS);
    return -1;
}

/* ---------------------------------------------------------------- */
/* the llama-server POST (plain HTTP, no TLS)                        */
/* ---------------------------------------------------------------- */

int http_post_json(const char *url, const char *path,
                   const char *body, long bodylen,
                   char **resp, long *resplen, int *http_status)
{
    char host[256];
    int port;
    char req[4096];
    int fd;
    SB out;
    long total = 0;
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
    read_all(fd, &out, 0);
    close(fd);

    /* split the response: status line, headers, (de-chunked) body */
    {
        RespInfo ri;
        SB bodysb;

        parse_response(sb_cstr(&out), (long)sb_len(&out), &ri, &bodysb);
        status = ri.status;
        total = (long)sb_len(&bodysb);
        rbody = strdup(sb_cstr(&bodysb));
        free(ri.ctype);
        free(ri.location);
        sb_free(&bodysb);
    }

    if (http_status)
        *http_status = status;
    if (rbody) {
        *resp = rbody;
        if (resplen)
            *resplen = total;
    }
    sb_free(&out);
    return rbody ? 0 : -1;
}

/* ---------------------------------------------------------------- */
/* https: libcurl, or the curl program used as a helper             */
/* ---------------------------------------------------------------- */

#ifdef HAVE_LIBCURL

typedef struct {
    SB buf;
    long cap;
} Sink;

static size_t curl_sink(void *ptr, size_t size, size_t nmemb, void *ud)
{
    Sink *s = ud;
    size_t n = size * nmemb;
    size_t room;

    if (g_intr_request)
        return 0;   /* abort the transfer */
    room = ((long)sb_len(&s->buf) + (long)n > s->cap)
               ? (size_t)(s->cap - (long)sb_len(&s->buf))
               : n;
    if (room)
        sb_addn(&s->buf, ptr, room);
    return room ? n : 0;
}

static int curl_progress(void *ud, curl_off_t d, curl_off_t dn,
                         curl_off_t u, curl_off_t un)
{
    (void)ud; (void)d; (void)dn; (void)u; (void)un;
    return g_intr_request ? 1 : 0;
}

static int fetch_curl(const char *url, long maxbytes, char **body,
                      long *bodylen, int *http_status, char **content_type,
                      char **final_url, SB *err)
{
    CURL *c;
    CURLcode rc;
    Sink sink;
    long code = 0;
    char *ct = NULL;
    char *eff = NULL;
    char *res;
    int intr = 0;

    sb_init(&sink.buf);
    sink.cap = maxbytes;

    c = curl_easy_init();
    if (!c) {
        sb_free(&sink.buf);
        sb_add(err, "cannot initialize libcurl");
        return -1;
    }
    curl_easy_setopt(c, CURLOPT_URL, url);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_MAXREDIRS, (long)HTTP_MAX_REDIRS);
    curl_easy_setopt(c, CURLOPT_USERAGENT, HTTP_UA);
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, curl_sink);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &sink);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(c, CURLOPT_ACCEPT_ENCODING, "");  /* gzip/deflate/brotli */
    curl_easy_setopt(c, CURLOPT_XFERINFOFUNCTION, curl_progress);
#if LIBCURL_VERSION_NUM >= 0x075500
    curl_easy_setopt(c, CURLOPT_PROTOCOLS_STR, "http,https");
    curl_easy_setopt(c, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");
#else
    curl_easy_setopt(c, CURLOPT_PROTOCOLS,
                     (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
    curl_easy_setopt(c, CURLOPT_REDIR_PROTOCOLS,
                     (long)(CURLPROTO_HTTP | CURLPROTO_HTTPS));
#endif
    if (getenv("BRACKETS_LLM_NET_DEBUG"))
        curl_easy_setopt(c, CURLOPT_VERBOSE, 1L);

    rc = curl_easy_perform(c);
    curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_getinfo(c, CURLINFO_CONTENT_TYPE, &ct);
    curl_easy_getinfo(c, CURLINFO_EFFECTIVE_URL, &eff);
    /* the info strings live inside the easy handle: copy them out now */
    ct = ct ? strdup(ct) : NULL;
    eff = eff ? strdup(eff) : NULL;

    if (g_intr_request) {
        intr = 1;
    } else if (rc != CURLE_OK && (long)sb_len(&sink.buf) == 0) {
        sb_printf(err, "fetch failed: %s", curl_easy_strerror(rc));
        free(ct);
        free(eff);
        curl_easy_cleanup(c);
        sb_free(&sink.buf);
        return -1;
    }
    curl_easy_cleanup(c);

    if (intr) {
        free(ct);
        free(eff);
        sb_free(&sink.buf);
        sb_add(err, "interrupted");
        return -1;
    }

    res = strdup(sb_cstr(&sink.buf));
    if (!res) {
        free(ct);
        free(eff);
        sb_free(&sink.buf);
        sb_add(err, "out of memory");
        return -1;
    }
    *body = res;
    if (bodylen)
        *bodylen = (long)sb_len(&sink.buf);
    if (http_status)
        *http_status = (int)code;
    if (content_type && ct) {
        char *semi = strchr(ct, ';');
        if (semi)
            *semi = '\0';
        *content_type = ct;
        ct = NULL;
    }
    if (final_url && eff)
        *final_url = eff;
    else
        free(eff);
    free(ct);
    sb_free(&sink.buf);
    return 0;
}

#else   /* !HAVE_LIBCURL: use the curl command line program as a helper */

/* The body arrives on stdout, the status line trailer on stderr (curl
 * %{stderr} needs 7.63; an older curl simply reports no status then). */
static int fetch_curl(const char *url, long maxbytes, char **body,
                      long *bodylen, int *http_status, char **content_type,
                      char **final_url, SB *err)
{
    int outp[2], errp[2];
    pid_t pid;
    SB out, meta;
    char tmo[16];
    char *args[24];
    int n = 0, intr;
    char *res;

    if (pipe(outp) != 0) {
        sb_add(err, "cannot create a pipe for the curl helper");
        return -1;
    }
    if (pipe(errp) != 0) {
        close(outp[0]);
        close(outp[1]);
        sb_add(err, "cannot create a pipe for the curl helper");
        return -1;
    }
    snprintf(tmo, sizeof(tmo), "%d", 30);

    args[n++] = (char *)"curl";
    args[n++] = (char *)"-s";
    args[n++] = (char *)"-L";
    args[n++] = (char *)"--max-redirs";
    args[n++] = (char *)"5";
    args[n++] = (char *)"--proto";
    args[n++] = (char *)"=http,https";
    args[n++] = (char *)"--proto-redir";
    args[n++] = (char *)"=http,https";
    args[n++] = (char *)"--max-time";
    args[n++] = tmo;
    args[n++] = (char *)"-A";
    args[n++] = (char *)HTTP_UA;
    args[n++] = (char *)"-w";
    args[n++] = (char *)"%{stderr}" HTTP_META_TAG " %{http_code}\n"
                        "%{content_type}\n%{url_effective}\n>>>\n";
    args[n++] = (char *)url;
    args[n] = NULL;

    pid = fork();
    if (pid < 0) {
        close(outp[0]);
        close(outp[1]);
        close(errp[0]);
        close(errp[1]);
        sb_add(err, "cannot fork the curl helper");
        return -1;
    }
    if (pid == 0) {
        close(outp[0]);
        close(errp[0]);
        if (dup2(outp[1], STDOUT_FILENO) < 0)
            _exit(127);
        if (dup2(errp[1], STDERR_FILENO) < 0)
            _exit(127);
        close(outp[1]);
        close(errp[1]);
        execvp("curl", args);
        _exit(127);
    }
    close(outp[1]);
    close(errp[1]);

    sb_init(&out);
    intr = read_all(outp[0], &out, maxbytes);
    close(outp[0]);
    if (intr)
        kill(pid, SIGTERM);
    sb_init(&meta);
    read_all(errp[0], &meta, 8192);
    close(errp[0]);
    waitpid(pid, NULL, 0);

    if (intr) {
        sb_free(&out);
        sb_free(&meta);
        sb_add(err, "interrupted");
        return -1;
    }

    /* the trailer starts with the marker, then status, type and final URL */
    {
        const char *m = find_bytes(sb_cstr(&meta), (long)sb_len(&meta),
                                   HTTP_META_TAG, strlen(HTTP_META_TAG));
        if (m) {
            const char *eol, *p = m + strlen(HTTP_META_TAG);
            if (http_status) {
                while (*p == ' ')
                    p++;
                *http_status = atoi(p);
            }
            eol = strchr(p, '\n');
            if (eol) {
                p = eol + 1;
                eol = strchr(p, '\n');
                if (eol && content_type)
                    *content_type = trim_dup(p, (size_t)(eol - p));
                if (eol) {
                    const char *end = strstr(eol, ">>>");
                    if (final_url)
                        *final_url = trim_dup(eol + 1, end
                                              ? (size_t)(end - eol - 1)
                                              : strlen(eol + 1));
                }
            }
        }
    }
    if (content_type && *content_type) {
        char *semi = strchr(*content_type, ';');
        if (semi)
            *semi = '\0';
        trim_in_place(*content_type);
    } else if (content_type) {
        free(*content_type);
        *content_type = NULL;
    }
    if (final_url && *final_url) {
        trim_in_place(*final_url);
        if (!**final_url) {
            free(*final_url);
            *final_url = strdup(url);
        }
    } else if (final_url) {
        *final_url = strdup(url);
    }

    res = strdup(sb_cstr(&out));
    if (!res) {
        sb_free(&out);
        sb_free(&meta);
        sb_add(err, "out of memory");
        return -1;
    }
    *body = res;
    if (bodylen)
        *bodylen = (long)sb_len(&out);
    sb_free(&out);
    sb_free(&meta);
    return 0;
}

#endif  /* HAVE_LIBCURL */

int http_get(const char *url, long maxbytes,
             char **body, long *bodylen, int *http_status,
             char **content_type, char **final_url, char **errmsg)
{
    SB err;
    HttpUrl u;
    int rc;

    if (body)
        *body = NULL;
    if (bodylen)
        *bodylen = 0;
    if (http_status)
        *http_status = 0;
    if (content_type)
        *content_type = NULL;
    if (final_url)
        *final_url = NULL;
    sb_init(&err);

    if (!url || !*url) {
        sb_add(&err, "no URL given");
        goto fail;
    }
    if (http_split_url(url, &u) != 0) {
        sb_printf(&err, "invalid URL '%s': only http:// and https:// URLs "
                        "can be fetched", url);
        goto fail;
    }
    if (maxbytes <= 0)
        maxbytes = 65536;
    if (!u.https)
        rc = fetch_plain(url, maxbytes, body, bodylen, http_status,
                         content_type, final_url, &err);
    else
        rc = fetch_curl(url, maxbytes, body, bodylen, http_status,
                        content_type, final_url, &err);
    http_url_free(&u);

    if (rc != 0) {
        if (body && *body) {
            free(*body);
            *body = NULL;
        }
        goto fail;
    }
    sb_free(&err);
    return 0;

fail:
    if (errmsg)
        *errmsg = strdup(sb_cstr(&err));
    sb_free(&err);
    return -1;
}
