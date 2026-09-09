/*
 * This file lspclient.c is part of L1vm.
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
 * brackets-llm - L1VM LSP client
 */

#define _POSIX_C_SOURCE 200809L

#include "lspclient.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "json.h"
#include "sb.h"

struct LspClient {
    pid_t pid;
    int in_fd;          /* write requests here */
    int out_fd;         /* read responses from here */
    int initialized;
    char *open_uri;     /* URI of the one document kept open for checks */
    int version;        /* last version sent for that document */
    LspDiagVec pending; /* diagnostics from the most recent check */
};

/* Never block on a silent LSP for more than this per read; a hung or
 * crashed server must fail the check instead of hanging the program. */
#define LSP_RECV_TIMEOUT_MS 60000

/* ---------- framing helper: read one full message ---------- */

static int wait_readable(int fd, int timeout_ms)
{
    struct pollfd p = { .fd = fd, .events = POLLIN };
    int r;
    do {
        r = poll(&p, 1, timeout_ms);
    } while (r < 0 && errno == EINTR);
    return r > 0;
}

static char *read_message(int fd, int timeout_ms)
{
    SB hdr;
    size_t len = 0;
    int have_len = 0;

    sb_init(&hdr);
    /* read headers until CRLF CRLF */
    for (;;) {
        char c;
        ssize_t n;
        if (!wait_readable(fd, timeout_ms)) {
            sb_free(&hdr);
            return NULL;
        }
        n = read(fd, &c, 1);
        if (n <= 0) {
            sb_free(&hdr);
            return NULL;
        }
        sb_addc(&hdr, c);
        if (sb_len(&hdr) >= 4 &&
            strcmp(sb_cstr(&hdr) + sb_len(&hdr) - 4, "\r\n\r\n") == 0)
            break;
    }
    /* parse Content-Length from the header block */
    {
        const char *p = sb_cstr(&hdr);
        const char *cl = strstr(p, "Content-Length:");
        if (cl) {
            cl += 15;
            while (*cl == ' ' || *cl == '\t')
                cl++;
            len = (size_t)atol(cl);
            have_len = 1;
        }
    }
    sb_free(&hdr);
    if (!have_len)
        return NULL;

    {
        char *body = malloc(len + 1);
        size_t got = 0;
        if (!body)
            return NULL;
        while (got < len) {
            ssize_t n;
            if (!wait_readable(fd, timeout_ms)) {
                free(body);
                return NULL;
            }
            n = read(fd, body + got, len - got);
            if (n <= 0) {
                free(body);
                return NULL;
            }
            got += (size_t)n;
        }
        body[len] = '\0';
        return body;
    }
}

/* ---------- sending ---------- */

static int send_raw(int fd, const char *body)
{
    size_t n = strlen(body);
    char hdr[64];
    int h = snprintf(hdr, sizeof(hdr), "Content-Length: %zu\r\n\r\n", n);
    size_t off = 0;
    if (write(fd, hdr, (size_t)h) < 0)
        return -1;
    while (off < n) {
        ssize_t w = write(fd, body + off, n - off);
        if (w < 0)
            return -1;
        off += (size_t)w;
    }
    return 0;
}

/* ---------- diagnostics parsing ---------- */

static void diagvec_push(LspDiagVec *v, LspDiag d)
{
    if (v->len == v->cap) {
        size_t nc = v->cap ? v->cap * 2 : 8;
        v->items = realloc(v->items, nc * sizeof(LspDiag));
        v->cap = nc;
    }
    v->items[v->len++] = d;
}

void lsp_diagvec_free(LspDiagVec *v)
{
    size_t i;
    for (i = 0; i < v->len; i++) {
        free(v->items[i].message);
        free(v->items[i].source);
    }
    free(v->items);
    v->items = NULL;
    v->len = v->cap = 0;
}

static void store_diagnostics(LspDiagVec *out, const JVal *diagarr)
{
    size_t i;
    if (!diagarr)
        return;
    for (i = 0; i < j_len(diagarr); i++) {
        const JVal *d = j_at(diagarr, i);
        const JVal *range, *start, *src, *msg, *sev;
        LspDiag g;
        memset(&g, 0, sizeof(g));
        range = j_get(d, "range");
        start = range ? j_get(range, "start") : NULL;
        if (start)
            g.line = (int)j_num(j_get(start, "line"));
        sev = j_get(d, "severity");
        g.severity = sev ? (int)j_num(sev) : 1;
        src = j_get(d, "source");
        g.source = (src && j_is(src, J_STR)) ? strdup(j_str(src)) : strdup("lsp");
        msg = j_get(d, "message");
        g.message = (msg && j_is(msg, J_STR)) ? strdup(j_str(msg)) : strdup("");
        diagvec_push(out, g);
    }
}

/* ---------- public API ---------- */

LspClient *lsp_start(const char *lsp_path, int l1com_enabled,
                     const char *l1com_path, const char *include_dir)
{
    LspClient *c = calloc(1, sizeof(LspClient));
    int in_pipe[2], out_pipe[2];

    if (!c)
        return NULL;
    if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0) {
        free(c);
        return NULL;
    }
    c->pid = fork();
    if (c->pid < 0) {
        free(c);
        return NULL;
    }
    if (c->pid == 0) {
        /* child: run the language server */
        dup2(in_pipe[0], 0);
        dup2(out_pipe[1], 1);
        close(in_pipe[0]);
        close(in_pipe[1]);
        close(out_pipe[0]);
        close(out_pipe[1]);
        execlp(lsp_path, lsp_path, (char *)NULL);
        _exit(127);
    }
    close(in_pipe[0]);
    close(out_pipe[1]);
    c->in_fd = in_pipe[1];
    c->out_fd = out_pipe[0];

    /* initialize */
    {
        JVal *params = j_obj_new();
        JVal *iopts = j_obj_new();
        JVal *incarr = j_arr_new();
        char *body, *resp;
        SB b;
        int init_ok = 0;

        if (l1com_enabled)
            j_obj_set(iopts, "l1comEnabled", j_str_new("on"));
        else
            j_obj_set(iopts, "l1comEnabled", j_str_new("off"));
        if (l1com_path && *l1com_path)
            j_obj_set(iopts, "l1comPath", j_str_new(l1com_path));
        if (include_dir && *include_dir)
            j_arr_push(incarr, j_str_new(include_dir));
        if (j_len(incarr) > 0)
            j_obj_set(iopts, "includeDirs", incarr);
        else
            j_free(incarr);

        j_obj_set(params, "processId", j_new(J_NULL));
        j_obj_set(params, "rootUri", j_new(J_NULL));
        j_obj_set(params, "capabilities", j_obj_new());
        j_obj_set(params, "initializationOptions", iopts);

        {
            JVal *msg = j_obj_new();
            j_obj_set(msg, "jsonrpc", j_str_new("2.0"));
            j_obj_set(msg, "id", j_num_new(1));
            j_obj_set(msg, "method", j_str_new("initialize"));
            j_obj_set(msg, "params", params);
            sb_init(&b);
            j_emit(msg, &b);
            body = strdup(sb_cstr(&b));
            sb_free(&b);
            j_free(msg);
        }
        if (send_raw(c->in_fd, body) == 0) {
            resp = read_message(c->out_fd, LSP_RECV_TIMEOUT_MS);
            if (resp)
                init_ok = 1;
            free(resp);
        }
        free(body);
        if (!init_ok) {
            lsp_stop(c);
            return NULL;
        }
        /* send "initialized" notification */
        {
            const char *n =
                "{\"jsonrpc\":\"2.0\",\"method\":\"initialized\",\"params\":{}}";
            (void)send_raw(c->in_fd, n);
        }
        c->initialized = 1;
    }
    return c;
}

void lsp_stop(LspClient *c)
{
    if (!c)
        return;
    if (c->initialized) {
        const char *n = "{\"jsonrpc\":\"2.0\",\"method\":\"exit\",\"params\":null}";
        (void)send_raw(c->in_fd, n);
    }
    close(c->in_fd);
    close(c->out_fd);
    if (c->pid > 0) {
        int st;
        waitpid(c->pid, &st, 0);
    }
    free(c->open_uri);
    free(c);
}

int lsp_check(LspClient *c, const char *uri, const char *path,
              const char *text, LspDiagVec *out)
{
    char *body = NULL;
    SB b;
    int ok = 0;

    (void)path;
    out->len = out->cap = 0;
    out->items = NULL;

    /* l1vm-lsp ignores didOpen for already-open URIs and publishes no
     * diagnostics in that case, so the first check opens the document and
     * every later check replaces its text wholesale via didChange. */
    {
        JVal *msg = j_obj_new();
        JVal *params = j_obj_new();
        JVal *td = j_obj_new();

        j_obj_set(msg, "jsonrpc", j_str_new("2.0"));
        j_obj_set(msg, "params", params);

        if (c->open_uri && strcmp(c->open_uri, uri) == 0) {
            JVal *changes = j_arr_new();
            JVal *item = j_obj_new();
            c->version++;
            j_obj_set(msg, "method", j_str_new("textDocument/didChange"));
            j_obj_set(td, "uri", j_str_new(uri));
            j_obj_set(td, "version", j_num_new(c->version));
            j_obj_set(params, "textDocument", td);
            j_obj_set(item, "text", j_str_new(text));
            j_arr_push(changes, item);
            j_obj_set(params, "contentChanges", changes);
        } else {
            free(c->open_uri);
            c->open_uri = strdup(uri);
            c->version = 1;
            j_obj_set(msg, "method", j_str_new("textDocument/didOpen"));
            j_obj_set(td, "uri", j_str_new(uri));
            j_obj_set(td, "languageId", j_str_new("l1com"));
            j_obj_set(td, "version", j_num_new(c->version));
            j_obj_set(td, "text", j_str_new(text));
            j_obj_set(params, "textDocument", td);
        }

        sb_init(&b);
        j_emit(msg, &b);
        body = strdup(sb_cstr(&b));
        sb_free(&b);
        j_free(msg);   /* msg owns params and everything below it */
    }

    if (send_raw(c->in_fd, body) != 0) {
        free(body);
        return -1;
    }
    free(body);

    /* wait for publishDiagnostics */
    for (;;) {
        char *resp = read_message(c->out_fd, LSP_RECV_TIMEOUT_MS);
        JVal *msg;
        if (!resp) {
            ok = -1;
            break;
        }
        msg = j_parse(resp);
        free(resp);
        if (!msg) {
            ok = -1;
            break;
        }
        {
            const char *method = j_str(j_get(msg, "method"));
            if (method && strcmp(method, "textDocument/publishDiagnostics") == 0) {
                const JVal *params = j_get(msg, "params");
                const JVal *darr = params ? j_get(params, "diagnostics") : NULL;
                store_diagnostics(out, darr);
                j_free(msg);
                break;
            }
        }
        j_free(msg);
    }
    return ok;
}
