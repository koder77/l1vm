/*
 * This file lsp.c is part of L1vm.
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
 * l1vm-lsp - LSP protocol layer
 * JSON-RPC over stdio (Content-Length framed), LSP method dispatch
 */

#define _POSIX_C_SOURCE 200809L

#include "l1lang.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

/* copy a request id into a new owned JVal */
static JVal *id_copy(const JVal *id)
{
    if (!id || id->type == J_NULL)
        return j_new(J_NULL);
    if (id->type == J_NUM)
        return j_num_new(id->u.num);
    if (id->type == J_STR)
        return j_str_new(id->u.str);
    return j_new(J_NULL);
}

/* ==================== open documents ==================== */

typedef struct {
    char *uri;
    L1Doc *doc;
} OpenDoc;

static OpenDoc *docs = NULL;
static size_t n_docs = 0;
static size_t cap_docs = 0;

static int g_exit_requested = 0;

static L1Doc *find_doc(const char *uri)
{
    size_t i;
    for (i = 0; i < n_docs; i++)
        if (strcmp(docs[i].uri, uri) == 0)
            return docs[i].doc;
    return NULL;
}

static L1Doc *add_doc(const char *uri, const char *text, int version)
{
    L1Doc *d;
    if (n_docs == cap_docs) {
        size_t nc = cap_docs ? cap_docs * 2 : 8;
        docs = realloc(docs, nc * sizeof(OpenDoc));
        if (!docs)
            return NULL;
        cap_docs = nc;
    }
    d = l1_doc_new(uri, NULL, text, version);
    if (!d)
        return NULL;
    docs[n_docs].uri = strdup(uri);
    docs[n_docs].doc = d;
    n_docs++;
    return d;
}

static void remove_doc(const char *uri)
{
    size_t i;
    for (i = 0; i < n_docs; i++) {
        if (strcmp(docs[i].uri, uri) == 0) {
            l1_doc_free(docs[i].doc);
            free(docs[i].uri);
            memmove(&docs[i], &docs[i + 1],
                    (n_docs - i - 1) * sizeof(OpenDoc));
            n_docs--;
            return;
        }
    }
}

/* ==================== framing ==================== */

static void write_all(const char *s, size_t n)
{
    size_t off = 0;
    while (off < n) {
        ssize_t w = write(STDOUT_FILENO, s + off, n - off);
        if (w < 0)
            return;
        off += (size_t)w;
    }
}

static void send_body(const char *body, size_t n)
{
    char hdr[64];
    int h = snprintf(hdr, sizeof(hdr),
                     "Content-Length: %zu\r\n\r\n", n);
    write_all(hdr, (size_t)h);
    write_all(body, n);
    fflush(stdout);
}

static void send_response(const JVal *id, JVal *result)
{
    JVal *msg = j_obj_new();
    SB b;
    j_obj_set(msg, "jsonrpc", j_str_new("2.0"));
    j_obj_set(msg, "id", id_copy(id));
    j_obj_set(msg, "result", result ? result : j_new(J_NULL));
    sb_init(&b);
    j_emit(msg, &b);
    send_body(sb_cstr(&b), sb_len(&b));
    sb_free(&b);
    j_free(msg);
}

static void send_error(const JVal *id, long code, const char *msg_txt)
{
    JVal *msg = j_obj_new();
    JVal *err = j_obj_new();
    SB b;
    j_obj_set(err, "code", j_num_new((double)code));
    j_obj_set(err, "message", j_str_new(msg_txt));
    j_obj_set(msg, "jsonrpc", j_str_new("2.0"));
    j_obj_set(msg, "id", id_copy(id));
    j_obj_set(msg, "error", err);
    sb_init(&b);
    j_emit(msg, &b);
    send_body(sb_cstr(&b), sb_len(&b));
    sb_free(&b);
    j_free(msg);
}

static void send_notification(const char *method, JVal *params)
{
    JVal *msg = j_obj_new();
    SB b;
    j_obj_set(msg, "jsonrpc", j_str_new("2.0"));
    j_obj_set(msg, "method", j_str_new(method));
    j_obj_set(msg, "params", params ? params : j_new(J_NULL));
    sb_init(&b);
    j_emit(msg, &b);
    send_body(sb_cstr(&b), sb_len(&b));
    sb_free(&b);
    j_free(msg);
}

/* ==================== diagnostics ==================== */

static void publish_diagnostics(L1Doc *d)
{
    JVal *params = j_obj_new();
    JVal *diags = j_arr_new();
    int i;

    for (i = 0; i < d->diags.len; i++) {
        const L1Diag *g = &d->diags.data[i];
        JVal *diag = j_obj_new();
        JVal *range = j_obj_new();
        JVal *start = j_obj_new();
        JVal *end = j_obj_new();
        j_obj_set(start, "line", j_num_new(g->line));
        j_obj_set(start, "character", j_num_new(g->col));
        j_obj_set(end, "line", j_num_new(g->end_line));
        j_obj_set(end, "character", j_num_new(g->end_col));
        j_obj_set(range, "start", start);
        j_obj_set(range, "end", end);
        j_obj_set(diag, "range", range);
        j_obj_set(diag, "severity", j_num_new(g->sev));
        if (g->source)
            j_obj_set(diag, "source", j_str_new(g->source));
        j_obj_set(diag, "message", j_str_new(g->msg));
        j_arr_push(diags, diag);
    }
    j_obj_set(params, "uri", j_str_new(d->uri));
    j_obj_set(params, "version", j_num_new(d->version));
    j_obj_set(params, "diagnostics", diags);
    send_notification("textDocument/publishDiagnostics", params);
}

static void analyze_and_publish(L1Doc *d)
{
    l1_doc_analyze(d);
    l1_doc_diagnostics(d);
    l1_doc_run_compiler(d);
    publish_diagnostics(d);
}

/* ==================== settings ==================== */

static void apply_settings(const JVal *cfg)
{
    const JVal *v;
    if (!cfg)
        return;
    v = j_get(cfg, "l1comEnabled");
    if (v) {
        if (j_is(v, J_STR)) {
            const char *s = j_str(v);
            if (strcmp(s, "off") == 0 || strcmp(s, "false") == 0)
                l1_settings.l1com_enabled = L1_L1COM_OFF;
            else if (strcmp(s, "on") == 0 || strcmp(s, "true") == 0)
                l1_settings.l1com_enabled = L1_L1COM_ON;
            else
                l1_settings.l1com_enabled = L1_L1COM_AUTO;
        } else if (j_is(v, J_NUM)) {
            int n = (int)j_num(v);
            l1_settings.l1com_enabled =
                (n == 1) ? L1_L1COM_ON :
                (n == 2) ? L1_L1COM_OFF : L1_L1COM_AUTO;
        }
    }
    v = j_get(cfg, "l1comPath");
    if (v && j_is(v, J_STR)) {
        free(l1_settings.l1com_path);
        l1_settings.l1com_path = strdup(j_str(v));
    }
    v = j_get(cfg, "includeDirs");
    if (v && j_is(v, J_ARR)) {
        SB b;
        size_t i;
        free(l1_settings.include_dirs);
        sb_init(&b);
        for (i = 0; i < j_len(v); i++) {
            const JVal *it = j_at(v, i);
            if (i > 0)
                sb_addc(&b, '\n');
            sb_add(&b, j_str(it));
        }
        l1_settings.include_dirs = strdup(sb_cstr(&b));
        sb_free(&b);
    }
    v = j_get(cfg, "staticDiag");
    if (v && j_is(v, J_BOOL))
        l1_settings.static_diag = j_bool(v);
    v = j_get(cfg, "missingTildeHint");
    if (v && j_is(v, J_BOOL))
        l1_settings.missing_tilde_hint = j_bool(v);
}

/* ==================== request params helpers ==================== */

static const char *param_uri(const JVal *params)
{
    const JVal *td = j_get(params, "textDocument");
    if (!td)
        return NULL;
    return j_str(j_get(td, "uri"));
}

static void param_position(const JVal *params, int *line, int *col)
{
    const JVal *pos = j_get(params, "position");
    *line = pos ? (int)j_num(j_get(pos, "line")) : 0;
    *col = pos ? (int)j_num(j_get(pos, "character")) : 0;
}

/* ==================== handlers ==================== */

/* LSP position (line, UTF-16 column) -> byte offset in whole text */
static long text_offset_at(const char *text, int line, int char16)
{
    const char *p = text;
    int ln = 0, i, c;
    if (!text)
        return 0;
    while (*p && ln < line) {
        if (*p == '\n')
            ln++;
        p++;
    }
    if (ln < line)
        return (long)strlen(text);
    i = 0;
    c = 0;
    while (c < char16 && p[i] && p[i] != '\n') {
        unsigned char ch = (unsigned char)p[i];
        if (ch < 0x80)
            i += 1;
        else if ((ch & 0xE0) == 0xC0)
            i += 2;
        else if ((ch & 0xF0) == 0xE0)
            i += 3;
        else if ((ch & 0xF8) == 0xF0) {
            i += 4;
            c++;            /* 2 UTF-16 units */
        } else
            i += 1;
        c++;
    }
    return (long)(p - text) + i;
}

/* apply one incremental (range-based) text edit, returning a new string */
static char *apply_text_edit(const char *text, const JVal *range,
                             const char *repl)
{
    long start, end, len, rl, outlen;
    char *out;
    const JVal *s = range ? j_get(range, "start") : NULL;
    const JVal *e = range ? j_get(range, "end") : NULL;
    if (!text)
        text = "";
    len = (long)strlen(text);
    if (range) {
        start = text_offset_at(
            text, (int)j_num(j_get(s, "line")),
            (int)j_num(j_get(s, "character")));
        end = text_offset_at(
            text, (int)j_num(j_get(e, "line")),
            (int)j_num(j_get(e, "character")));
    } else {
        start = 0;
        end = len;
    }
    if (start < 0)
        start = 0;
    if (start > len)
        start = len;
    if (end < start)
        end = start;
    if (end > len)
        end = len;
    rl = repl ? (long)strlen(repl) : 0;
    outlen = len - (end - start) + rl;
    out = malloc((size_t)outlen + 1);
    if (!out)
        return strdup(text);
    memcpy(out, text, (size_t)start);
    if (rl)
        memcpy(out + start, repl, (size_t)rl);
    memcpy(out + start + rl, text + end, (size_t)(len - end));
    out[outlen] = '\0';
    return out;
}

static void handle_initialize(const JVal *id, const JVal *params)
{
    JVal *result = j_obj_new();
    JVal *caps = j_obj_new();
    JVal *sync = j_obj_new();
    JVal *completion = j_obj_new();
    JVal *completion_trig = j_arr_new();
    JVal *sig = j_obj_new();
    JVal *sig_trig = j_arr_new();
    JVal *sem = j_obj_new();
    JVal *sem_full = j_obj_new();
    JVal *legend = j_obj_new();
    JVal *types = j_arr_new();
    JVal *mods = j_arr_new();
    static const char *const tnames[] = {
        "keyword", "type", "function", "variable", "constant", "label",
        "macro", "comment", "string", "number", "operator",
        "preprocessor", "namespace", "class"
    };
    static const char *const mnames[] = {
        "declaration", "readonly", "defaultLibrary"
    };
    int i;

    (void)params;
    j_obj_set(sync, "openClose", j_bool_new(1));
    j_obj_set(sync, "change", j_num_new(2));   /* incremental sync */
    j_obj_set(caps, "textDocumentSync", sync);

    j_arr_push(completion_trig, j_str_new(":"));
    j_arr_push(completion_trig, j_str_new("#"));
    j_arr_push(completion_trig, j_str_new("."));
    j_obj_set(completion, "triggerCharacters", completion_trig);
    j_obj_set(completion, "resolveProvider", j_bool_new(0));
    j_obj_set(caps, "completionProvider", completion);

    j_obj_set(caps, "hoverProvider", j_bool_new(1));
    j_obj_set(caps, "definitionProvider", j_bool_new(1));
    j_obj_set(caps, "referencesProvider", j_bool_new(1));
    j_obj_set(caps, "documentSymbolProvider", j_bool_new(1));
    j_obj_set(caps, "foldingRangeProvider", j_bool_new(1));
    j_obj_set(caps, "documentHighlightProvider", j_bool_new(1));

    j_arr_push(sig_trig, j_str_new("("));
    j_arr_push(sig_trig, j_str_new("!"));
    j_obj_set(sig, "triggerCharacters", sig_trig);
    j_obj_set(caps, "signatureHelpProvider", sig);

    for (i = 0; i < 14; i++)
        j_arr_push(types, j_str_new(tnames[i]));
    for (i = 0; i < 3; i++)
        j_arr_push(mods, j_str_new(mnames[i]));
    j_obj_set(legend, "tokenTypes", types);
    j_obj_set(legend, "tokenModifiers", mods);
    j_obj_set(sem_full, "delta", j_bool_new(0));
    j_obj_set(sem, "legend", legend);
    j_obj_set(sem, "full", sem_full);
    j_obj_set(caps, "semanticTokensProvider", sem);

    j_obj_set(result, "capabilities", caps);
    {
        JVal *info = j_obj_new();
        j_obj_set(info, "name", j_str_new("l1vm-lsp"));
        j_obj_set(info, "version", j_str_new("0.1.0"));
        j_obj_set(result, "serverInfo", info);
    }

    /* settings may arrive via initializationOptions */
    apply_settings(j_get(params, "initializationOptions"));

    send_response(id, result);
}

static void handle_did_open(const JVal *params)
{
    const char *uri = param_uri(params);
    const JVal *td = j_get(params, "textDocument");
    const char *text;
    int version;
    L1Doc *d;

    if (!uri)
        return;
    text = td ? j_str(j_get(td, "text")) : "";
    version = td ? (int)j_num(j_get(td, "version")) : 0;
    if (find_doc(uri))
        return;
    d = add_doc(uri, text, version);
    if (d)
        analyze_and_publish(d);
}

static void handle_did_change(const JVal *params)
{
    const char *uri = param_uri(params);
    const JVal *changes = j_get(params, "contentChanges");
    int version = 0;
    size_t i;
    char *cur;
    L1Doc *d;

    if (!uri)
        return;
    d = find_doc(uri);
    if (!d)
        return;
    version = (int)j_num(j_get(params, "version"));
    cur = strdup(d->text ? d->text : "");
    for (i = 0; i < j_len(changes); i++) {
        const JVal *c = j_at(changes, i);
        const JVal *t = c ? j_get(c, "text") : NULL;
        const JVal *range = c ? j_get(c, "range") : NULL;
        char *n;
        if (!t)
            continue;
        n = apply_text_edit(cur, range, j_str(t));
        free(cur);
        cur = n;
    }
    l1_doc_set_text(d, cur, version);
    free(cur);
    l1_doc_analyze(d);
    l1_doc_diagnostics(d);
    publish_diagnostics(d);
}

static void handle_did_close(const JVal *params)
{
    const char *uri = param_uri(params);
    if (!uri)
        return;
    /* clear diagnostics for the closed document */
    {
        JVal *p = j_obj_new();
        j_obj_set(p, "uri", j_str_new(uri));
        j_obj_set(p, "diagnostics", j_arr_new());
        send_notification("textDocument/publishDiagnostics", p);
    }
    remove_doc(uri);
}

static void handle_position_request(const JVal *id, const JVal *params,
                                    JVal *(*fn)(L1Doc *, int, int))
{
    const char *uri = param_uri(params);
    int line, col;
    L1Doc *d;
    JVal *res;
    if (!uri)
        return send_error(id, -32602, "missing textDocument/uri");
    d = find_doc(uri);
    if (!d)
        return send_error(id, -32602, "document not open");
    param_position(params, &line, &col);
    res = fn(d, line, col);
    send_response(id, res);
}

/* ==================== dispatch ==================== */

static void handle_message(const JVal *req)
{
    const JVal *id = j_get(req, "id");
    const char *method = j_str(j_get(req, "method"));
    const JVal *params = j_get(req, "params");
    JVal *res = NULL;

    if (!method) {
        if (id)
            send_error(id, -32600, "missing method");
        return;
    }

    if (strcmp(method, "initialize") == 0) {
        handle_initialize(id, params);
        return;
    }
    if (strcmp(method, "initialized") == 0) {
        /* nothing to do */
        return;
    }
    if (strcmp(method, "shutdown") == 0) {
        send_response(id, j_new(J_NULL));
        return;
    }
    if (strcmp(method, "exit") == 0) {
        g_exit_requested = 1;
        return;
    }
    if (strcmp(method, "$/cancelRequest") == 0) {
        return;
    }
    if (strcmp(method, "workspace/didChangeConfiguration") == 0) {
        const JVal *settings = j_get(params, "settings");
        apply_settings(settings ? settings : params);
        return;
    }

    if (strcmp(method, "textDocument/didOpen") == 0) {
        handle_did_open(params);
        return;
    }
    if (strcmp(method, "textDocument/didChange") == 0) {
        handle_did_change(params);
        return;
    }
    if (strcmp(method, "textDocument/didClose") == 0) {
        handle_did_close(params);
        return;
    }

    if (strcmp(method, "textDocument/completion") == 0) {
        const char *uri = param_uri(params);
        int line, col;
        JVal *items;
        L1Doc *d;
        if (!uri)
            return send_error(id, -32602, "missing textDocument/uri");
        d = find_doc(uri);
        if (!d)
            return send_error(id, -32602, "document not open");
        param_position(params, &line, &col);
        items = l1_completion(d, line, col);
        {
            JVal *wrap = j_obj_new();
            j_obj_set(wrap, "isIncomplete", j_bool_new(0));
            j_obj_set(wrap, "items", items ? items : j_arr_new());
            send_response(id, wrap);
        }
        return;
    }
    if (strcmp(method, "textDocument/hover") == 0) {
        handle_position_request(id, params, l1_hover);
        return;
    }
    if (strcmp(method, "textDocument/definition") == 0) {
        handle_position_request(id, params, l1_definition);
        return;
    }
    if (strcmp(method, "textDocument/references") == 0) {
        handle_position_request(id, params, l1_references);
        return;
    }
    if (strcmp(method, "textDocument/documentHighlight") == 0) {
        handle_position_request(id, params, l1_highlight);
        return;
    }
    if (strcmp(method, "textDocument/signatureHelp") == 0) {
        handle_position_request(id, params, l1_signature);
        return;
    }

    /* document-level requests */
    if (strcmp(method, "textDocument/documentSymbol") == 0 ||
        strcmp(method, "textDocument/foldingRange") == 0 ||
        strcmp(method, "textDocument/semanticTokens/full") == 0) {
        const char *uri = param_uri(params);
        L1Doc *d;
        if (!uri)
            return send_error(id, -32602, "missing textDocument/uri");
        d = find_doc(uri);
        if (!d)
            return send_error(id, -32602, "document not open");
        if (strcmp(method, "textDocument/documentSymbol") == 0)
            res = l1_document_symbols(d);
        else if (strcmp(method, "textDocument/foldingRange") == 0)
            res = l1_folding(d);
        else {
            JVal *wrap = j_obj_new();
            j_obj_set(wrap, "data", l1_semantic_tokens(d));
            res = wrap;
        }
        send_response(id, res);
        return;
    }

    /* unknown request: respond with method-not-found */
    if (id)
        send_error(id, -32601, "method not found");
}

/* ==================== reader ==================== */

static int read_line(char *buf, size_t sz)
{
    size_t i = 0;
    int c;
    while (i + 1 < sz && (c = getchar()) != EOF) {
        if (c == '\n')
            break;
        buf[i++] = (char)c;
    }
    if (i > 0 && buf[i - 1] == '\r')
        buf[--i] = '\0';
    buf[i] = '\0';
    if (i == 0 && c == EOF)
        return 0;
    return 1;
}

static int read_message(SB *body)
{
    long len = -1;
    char hdr[4096];
    char *p;

    for (;;) {
        if (!read_line(hdr, sizeof(hdr)))
            return 0;
        p = hdr;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '\0')
            break;   /* end of headers */
        if (strncasecmp(p, "Content-Length:", 15) == 0) {
            len = strtol(p + 15, NULL, 10);
        }
    }
    if (len < 0)
        return 0;

    sb_reset(body);
    {
        long i;
        for (i = 0; i < len; i++) {
            int c = getchar();
            if (c == EOF)
                return 0;
            sb_addc(body, (char)c);
        }
    }
    return 1;
}

void lsp_main_loop(void)
{
    SB body;

    g_exit_requested = 0;
    l1_settings.l1com_enabled = L1_L1COM_AUTO;
    l1_settings.l1com_path = NULL;
    l1_settings.include_dirs = NULL;
    l1_settings.static_diag = 1;
    l1_settings.missing_tilde_hint = 1;

    sb_init(&body);
    while (read_message(&body)) {
        JVal *req = j_parse(sb_cstr(&body));
        if (req) {
            handle_message(req);
            j_free(req);
        }
        if (g_exit_requested)
            break;
    }
    sb_free(&body);
}
