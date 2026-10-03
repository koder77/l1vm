/*
 * This file provider_openai.c is part of L1vm.
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
 * brackets-llm - the OpenAI-compatible REST provider
 *
 * POST {base_url}/chat/completions with
 *   Content-Type: application/json
 *   Authorization: Bearer <api_key>        (when an api_key is configured)
 * and the OpenAI request body
 *   {"model":..., "messages":[{"role","content"}...], "temperature", ...}
 *
 * Two response modes:
 *   non-streaming: read choices[0].message.content (and .tool_calls)
 *   streaming:     consume "data: {json}" Server-Sent Events line by line
 *                  and take choices[0].delta.content until "data: [DONE]"
 *
 * Every mode ends in the same LlmReply, and `raw` always holds a complete
 * OpenAI shaped response object - the streaming path rebuilds it from the
 * deltas - so the rest of the program (tool handling, token usage) does not
 * need to know whether the answer arrived in one piece or in a hundred.
 *
 * This implementation is also the shared REST core of the llama.cpp backend
 * (provider_llamacpp.c): only the flavour flags differ.
 */

#define _POSIX_C_SOURCE 200809L

#include "provider.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"
#include "intr.h"
#include "json.h"
#include "sb.h"

#define REST_PATH "/chat/completions"

/* ---------------------------------------------------------------- */
/* small helpers                                                     */
/* ---------------------------------------------------------------- */

static void dbg(const LlmProvider *p, const char *fmt, ...)
{
    va_list ap;

    if (!p || !p->cfg || !p->cfg->debug)
        return;
    fprintf(stderr, "[%s] ", p->name);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
}

static void set_err(char **errmsg, const char *fmt, ...)
{
    SB b;
    va_list ap;

    if (!errmsg || *errmsg)
        return;                  /* keep the first, most specific message */
    sb_init(&b);
    va_start(ap, fmt);
    {
        char tmp[1024];
        vsnprintf(tmp, sizeof(tmp), fmt, ap);
        sb_add(&b, tmp);
    }
    va_end(ap);
    *errmsg = strdup(sb_cstr(&b));
    sb_free(&b);
}

/* Extract the assistant text of a "content" field: a string, an array of
 * {"type":"text","text":...} parts, or null (tool calls only). */
static char *content_to_text(const JVal *content)
{
    SB b;
    size_t i;
    char *r;

    if (!content)
        return strdup("");
    if (j_is(content, J_STR))
        return strdup(j_str(content) ? j_str(content) : "");
    if (!j_is(content, J_ARR))
        return strdup("");

    sb_init(&b);
    for (i = 0; i < j_len(content); i++) {
        const JVal *part = j_at(content, i);
        const JVal *txt;
        if (!part || !j_is(part, J_OBJ))
            continue;
        txt = j_get(part, "text");
        if (txt && j_is(txt, J_STR) && j_str(txt))
            sb_add(&b, j_str(txt));
    }
    r = strdup(sb_cstr(&b));
    sb_free(&b);
    return r;
}

/* A JSON "error" member may be an object or a bare string. */
static char *error_member_text(const JVal *e)
{
    if (!e)
        return NULL;
    if (j_is(e, J_STR))
        return strdup(j_str(e) ? j_str(e) : "error");
    if (j_is(e, J_OBJ)) {
        const JVal *m = j_get(e, "message");
        if (m && j_is(m, J_STR) && j_str(m))
            return strdup(j_str(m));
    }
    return NULL;
}

static long usage_token(const JVal *usage, const char *key)
{
    const JVal *v;

    if (!usage)
        return -1;
    v = j_get(usage, key);
    if (!v || !j_is(v, J_NUM))
        return -1;
    return (long)j_num(v);
}

static char *dup_member(const JVal *v)
{
    if (v && j_is(v, J_STR) && j_str(v))
        return strdup(j_str(v));
    return NULL;
}

/* ---------------------------------------------------------------- */
/* request body                                                      */
/* ---------------------------------------------------------------- */

/*
 * Build the OpenAI request object. `*streaming` reports whether the answer
 * is requested as SSE - the caller uses it to pick the read mode.
 */
static JVal *build_payload(const LlmConfig *cfg, const LlmChat *chat,
                           const LlmRestFlavor *fl, int *streaming)
{
    JVal *payload = j_obj_new();
    JVal *msgs = j_arr_new();
    size_t i;

    j_obj_set(payload, "model", j_str_new(cfg->model));
    for (i = 0; i < chat->nmsgs; i++) {
        JVal *m = j_obj_new();
        j_obj_set(m, "role",
                  j_str_new(chat->msgs[i].role ? chat->msgs[i].role : "user"));
        j_obj_set(m, "content",
                  j_str_new(chat->msgs[i].content ? chat->msgs[i].content : ""));
        j_arr_push(msgs, m);
    }
    j_obj_set(payload, "messages", msgs);

    if (cfg->temperature >= 0.0)
        j_obj_set(payload, "temperature", j_num_new(cfg->temperature));
    if (cfg->top_p >= 0.0)
        j_obj_set(payload, "top_p", j_num_new(cfg->top_p));
    if (cfg->max_tokens > 0)
        j_obj_set(payload, "max_tokens", j_num_new((double)cfg->max_tokens));

    if (chat->tools_json) {
        JVal *t = j_parse(chat->tools_json);
        if (t && j_is(t, J_ARR))
            j_obj_set(payload, "tools", t);
        else
            j_free(t);            /* malformed tool list: just do without */
    }

    *streaming = (cfg->stream && fl->allow_stream) ? 1 : 0;
    j_obj_set(payload, "stream", j_bool_new(*streaming));
    if (*streaming) {
        /* asks the server for a final usage chunk (OpenAI, vLLM, LM Studio;
         * servers that ignore it simply send no usage) */
        JVal *so = j_obj_new();
        j_obj_set(so, "include_usage", j_bool_new(1));
        j_obj_set(payload, "stream_options", so);
    }

    if (fl->add_thinking_ctl) {
        /* Qwen3-style reasoning models: keep the answer free of thinking */
        JVal *ctk = j_obj_new();
        j_obj_set(ctk, "enable_thinking", j_bool_new(0));
        j_obj_set(payload, "chat_template_kwargs", ctk);
    }
    return payload;
}

static char *payload_to_json(JVal *payload, long *len)
{
    SB b;
    char *s;

    sb_init(&b);
    j_emit(payload, &b);
    s = strdup(sb_cstr(&b));
    if (len)
        *len = (long)sb_len(&b);
    sb_free(&b);
    j_free(payload);
    return s;
}

/* Request headers: authentication plus the Accept of the chosen mode. */
static size_t build_headers(const LlmConfig *cfg, const LlmRestFlavor *fl,
                            int streaming, const char **hdrs, size_t max,
                            char *authbuf, size_t authsz)
{
    size_t n = 0;

    if (fl->send_auth && cfg->api_key[0]) {
        snprintf(authbuf, authsz, "Authorization: Bearer %s", cfg->api_key);
        hdrs[n++] = authbuf;
    }
    if (streaming && n + 1 <= max)
        hdrs[n++] = "Accept: text/event-stream";
    return n;
}

/* ---------------------------------------------------------------- */
/* non-streaming answer                                              */
/* ---------------------------------------------------------------- */

/*
 * Turn one complete response body into an LlmReply. `raw_json` is kept as the
 * reply's raw JSON so callers that want tool_calls or usage can read it
 * exactly as they would from the server.
 */
static LlmStatus reply_from_json(LlmProvider *p, const char *raw_json,
                                 LlmReply *out, char **errmsg)
{
    JVal *r = j_parse(raw_json);
    const JVal *choices, *ch, *msg, *content, *tc, *err;
    char *etext;

    if (!r || !j_is(r, J_OBJ)) {
        j_free(r);
        set_err(errmsg, "%s: the server did not return a JSON object",
                p->endpoint);
        return LLM_ERR_PARSE;
    }

    /* a JSON error member can arrive with status 200 as well */
    err = j_get(r, "error");
    if (err) {
        etext = error_member_text(err);
        set_err(errmsg, "%s: server error: %s", p->endpoint,
                etext ? etext : "(no message)");
        free(etext);
        j_free(r);
        return LLM_ERR_PROTOCOL;
    }

    choices = j_get(r, "choices");
    ch = (choices && j_len(choices) > 0) ? j_at(choices, 0) : NULL;
    if (!ch) {
        /* Ollama's native shape only exists on /api/chat, but be tolerant */
        const JVal *m = j_get(r, "message");
        if (m) {
            content = j_get(m, "content");
            out->text = content_to_text(content);
            out->finish_reason = strdup("stop");
            out->raw = strdup(raw_json);
            j_free(r);
            return LLM_OK;
        }
        set_err(errmsg, "%s: the answer has no \"choices\" array",
                p->endpoint);
        j_free(r);
        return LLM_ERR_PROTOCOL;
    }

    msg = j_get(ch, "message");
    if (!msg)
        msg = j_get(ch, "delta");        /* some servers answer a delta shape */
    content = msg ? j_get(msg, "content") : NULL;
    out->text = content_to_text(content);
    out->finish_reason = dup_member(j_get(ch, "finish_reason"));
    tc = msg ? j_get(msg, "tool_calls") : NULL;
    out->has_tool_calls = (tc && j_is(tc, J_ARR) && j_len(tc) > 0) ? 1 : 0;
    out->raw = strdup(raw_json);

    {
        const JVal *usage = j_get(r, "usage");
        out->in_tokens = usage_token(usage, "prompt_tokens");
        out->out_tokens = usage_token(usage, "completion_tokens");
    }
    j_free(r);
    return LLM_OK;
}

/* ---------------------------------------------------------------- */
/* streaming answer (Server-Sent Events)                             */
/* ---------------------------------------------------------------- */

/* One tool call, reassembled from the fragments of a streamed delta.
 * Servers may split both the function name and its arguments, so both are
 * accumulated piece by piece. */
typedef struct {
    long   index;
    char  *id;
    SB     name;
    SB     args;
} TcPart;

typedef struct {
    const LlmChat *chat;
    SB      text;
    TcPart *tc;
    size_t  ntc, captc;
    char   *model;
    char   *id;
    char   *finish;
    long    in_tokens, out_tokens;
    int     done;          /* "data: [DONE]" seen */
    int     events;        /* events understood */
    int     skipped;       /* events that could not be used */
} StreamAcc;

static TcPart *tc_get(StreamAcc *a, long index)
{
    size_t i;
    TcPart *t;

    for (i = 0; i < a->ntc; i++)
        if (a->tc[i].index == index)
            return &a->tc[i];
    if (a->ntc == a->captc) {
        size_t nc = a->captc ? a->captc * 2 : 4;
        TcPart *nt = realloc(a->tc, nc * sizeof(TcPart));
        if (!nt)
            return NULL;
        a->tc = nt;
        a->captc = nc;
    }
    t = &a->tc[a->ntc++];
    memset(t, 0, sizeof(*t));
    t->index = index;
    sb_init(&t->name);
    sb_init(&t->args);
    return t;
}

static void acc_free(StreamAcc *a)
{
    size_t i;

    sb_free(&a->text);
    for (i = 0; i < a->ntc; i++) {
        free(a->tc[i].id);
        sb_free(&a->tc[i].name);
        sb_free(&a->tc[i].args);
    }
    free(a->tc);
    free(a->model);
    free(a->id);
    free(a->finish);
    memset(a, 0, sizeof(*a));
}

/* Append one streamed tool call fragment to the accumulator. */
static void acc_tool_delta(StreamAcc *a, const JVal *tc)
{
    size_t i;
    const JVal *idx, *id, *fn, *nm, *ar;

    if (!tc || !j_is(tc, J_ARR))
        return;
    for (i = 0; i < j_len(tc); i++) {
        const JVal *call = j_at(tc, i);
        TcPart *t;
        long index;

        if (!call || !j_is(call, J_OBJ))
            continue;
        idx = j_get(call, "index");
        index = (idx && j_is(idx, J_NUM)) ? (long)j_num(idx) : (long)i;
        t = tc_get(a, index);
        if (!t)
            continue;
        id = j_get(call, "id");
        if (id && j_is(id, J_STR) && j_str(id) && !t->id)
            t->id = strdup(j_str(id));
        fn = j_get(call, "function");
        if (fn) {
            nm = j_get(fn, "name");
            if (nm && j_is(nm, J_STR) && j_str(nm))
                sb_add(&t->name, j_str(nm));
            ar = j_get(fn, "arguments");
            if (ar && j_is(ar, J_STR) && j_str(ar))
                sb_add(&t->args, j_str(ar));
        }
    }
}

/* Read the usage of a final streaming chunk. */
static void acc_usage(StreamAcc *a, const JVal *usage)
{
    long v;

    if (!usage)
        return;
    v = usage_token(usage, "prompt_tokens");
    if (v >= 0)
        a->in_tokens = v;
    v = usage_token(usage, "completion_tokens");
    if (v >= 0)
        a->out_tokens = v;
}

/*
 * One streamed event. Accepts the OpenAI delta shape as well as Ollama's
 * native NDJSON shape ({"message":{"content":...},"done":false}) for the
 * servers that expose it under the /v1 prefix.
 */
static void acc_event(StreamAcc *a, const char *json)
{
    JVal *ev = j_parse(json);
    const JVal *choices, *ch, *delta, *content, *fr;

    if (!ev || !j_is(ev, J_OBJ)) {
        j_free(ev);
        a->skipped++;
        return;
    }
    if (!a->id)
        a->id = dup_member(j_get(ev, "id"));
    if (!a->model)
        a->model = dup_member(j_get(ev, "model"));

    choices = j_get(ev, "choices");
    if (choices && j_is(choices, J_ARR) && j_len(choices) > 0) {
        ch = j_at(choices, 0);
        delta = ch ? j_get(ch, "delta") : NULL;
        if (!delta)
            delta = ch ? j_get(ch, "message") : NULL;
        content = delta ? j_get(delta, "content") : NULL;
        if (content && j_is(content, J_STR) && j_str(content) && *j_str(content)) {
            sb_add(&a->text, j_str(content));
            if (a->chat->on_delta)
                a->chat->on_delta(j_str(content), a->chat->on_delta_ud);
        }
        acc_tool_delta(a, delta ? j_get(delta, "tool_calls") : NULL);
        fr = ch ? j_get(ch, "finish_reason") : NULL;
        if (fr && j_is(fr, J_STR) && j_str(fr) && *j_str(fr)) {
            free(a->finish);
            a->finish = strdup(j_str(fr));
        }
        acc_usage(a, j_get(ev, "usage"));
        a->events++;
        j_free(ev);
        return;
    }

    /* Ollama native shape */
    {
        const JVal *m = j_get(ev, "message");
        content = m ? j_get(m, "content") : NULL;
        if (!content)
            content = j_get(ev, "response");    /* /api/generate */
        if (content && j_is(content, J_STR) && j_str(content) && *j_str(content)) {
            sb_add(&a->text, j_str(content));
            if (a->chat->on_delta)
                a->chat->on_delta(j_str(content), a->chat->on_delta_ud);
        }
        acc_tool_delta(a, m ? j_get(m, "tool_calls") : NULL);
        fr = j_get(ev, "done_reason");
        if (fr && j_is(fr, J_STR) && j_str(fr) && !a->finish)
            a->finish = strdup(j_str(fr));
        acc_usage(a, j_get(ev, "usage"));
        if (j_get(ev, "done")) {
            a->done = 1;
            if (!a->finish)
                a->finish = strdup("stop");
        }
        a->events++;
    }
    j_free(ev);
}

/* Rebuild the OpenAI response object the non-streaming path would have
 * returned, so every consumer sees one shape. */
static char *acc_to_raw_json(StreamAcc *a)
{
    JVal *root = j_obj_new();
    JVal *choices = j_arr_new();
    JVal *ch = j_obj_new();
    JVal *msg = j_obj_new();
    size_t i;

    j_obj_set(root, "id", j_str_new(a->id ? a->id : "chatcmpl-brackets"));
    j_obj_set(root, "object", j_str_new("chat.completion"));
    j_obj_set(root, "model", j_str_new(a->model ? a->model : ""));
    j_obj_set(ch, "index", j_num_new(0));
    j_obj_set(msg, "role", j_str_new("assistant"));
    j_obj_set(msg, "content", j_str_new(sb_cstr(&a->text)));

    if (a->ntc) {
        JVal *arr = j_arr_new();
        for (i = 0; i < a->ntc; i++) {
            JVal *call = j_obj_new();
            JVal *fn = j_obj_new();
            char idbuf[64];
            const char *s = sb_cstr(&a->tc[i].args);

            if (!a->tc[i].id) {
                snprintf(idbuf, sizeof(idbuf), "call_%ld", (long)i);
                j_obj_set(call, "id", j_str_new(idbuf));
            } else {
                j_obj_set(call, "id", j_str_new(a->tc[i].id));
            }
            j_obj_set(call, "type", j_str_new("function"));
            j_obj_set(fn, "name", j_str_new(sb_cstr(&a->tc[i].name)));
            j_obj_set(fn, "arguments", j_str_new(s));
            j_obj_set(call, "function", fn);
            j_arr_push(arr, call);
        }
        j_obj_set(msg, "tool_calls", arr);
    }
    j_obj_set(ch, "message", msg);
    j_obj_set(ch, "finish_reason",
              j_str_new(a->finish ? a->finish
                                  : (a->ntc ? "tool_calls" : "stop")));
    j_arr_push(choices, ch);
    j_obj_set(root, "choices", choices);

    if (a->in_tokens >= 0 || a->out_tokens >= 0) {
        JVal *usage = j_obj_new();
        j_obj_set(usage, "prompt_tokens",
                  j_num_new(a->in_tokens > 0 ? (double)a->in_tokens : 0.0));
        j_obj_set(usage, "completion_tokens",
                  j_num_new(a->out_tokens > 0 ? (double)a->out_tokens : 0.0));
        j_obj_set(usage, "total_tokens",
                  j_num_new((double)((a->in_tokens > 0 ? a->in_tokens : 0) +
                                    (a->out_tokens > 0 ? a->out_tokens : 0))));
        j_obj_set(root, "usage", usage);
    }
    {
        SB b;
        char *s;
        sb_init(&b);
        j_emit(root, &b);
        s = strdup(sb_cstr(&b));
        sb_free(&b);
        j_free(root);
        return s;
    }
}

/* http_post_stream callback: one response line at a time. */
static int on_stream_line(const char *line, size_t n, void *ud)
{
    StreamAcc *a = ud;
    const char *p = line;
    size_t len = n;

    while (len && (*p == ' ' || *p == '\t')) {
        p++;
        len--;
    }
    while (len && (p[len - 1] == ' ' || p[len - 1] == '\t'))
        len--;
    if (!len)
        return 0;                   /* event separator: nothing to do */

    if (p[0] == ':')
        return 0;                   /* SSE comment / keep-alive */

    if (len >= 5 && strncmp(p, "data:", 5) == 0) {
        const char *j = p + 5;
        size_t jl = len - 5;
        while (jl && (*j == ' ' || *j == '\t')) {
            j++;
            jl--;
        }
        if (jl == 6 && strncmp(j, "[DONE]", 6) == 0) {
            a->done = 1;
            return -1;              /* stop the transfer, this is success */
        }
        if (!jl)
            return 0;
        {
            char *tmp = malloc(jl + 1);
            int rc;
            if (!tmp)
                return -1;
            memcpy(tmp, j, jl);
            tmp[jl] = '\0';
            acc_event(a, tmp);
            rc = a->done ? -1 : 0;
            free(tmp);
            return rc;
        }
    }

    if (len >= 6 && strncmp(p, "event:", 6) == 0)
        return 0;                   /* only the event name, ignore it */
    if (len >= 3 && strncmp(p, "id:", 3) == 0)
        return 0;
    if (len >= 6 && strncmp(p, "retry:", 6) == 0)
        return 0;

    if (p[0] == '{') {              /* newline delimited JSON (Ollama) */
        char *tmp = malloc(len + 1);
        int rc;
        if (!tmp)
            return -1;
        memcpy(tmp, p, len);
        tmp[len] = '\0';
        acc_event(a, tmp);
        rc = a->done ? -1 : 0;
        free(tmp);
        return rc;
    }

    a->skipped++;
    return 0;
}

/* ---------------------------------------------------------------- */
/* the provider entry point                                          */
/* ---------------------------------------------------------------- */

/* post_core() says why a transfer failed; map that onto our status enum so
 * callers can tell "server is down" from "server said no". */
static LlmStatus post_status(int post_rc, int http_status)
{
    switch (post_rc) {
    case -2: return LLM_ERR_CONNECT;      /* nothing is listening */
    case -3: return LLM_ERR_HTTP;         /* 4xx / 5xx */
    case -4: return LLM_ERR_INTERRUPTED;  /* Ctrl+C */
    default:  return (http_status >= 400) ? LLM_ERR_HTTP : LLM_ERR_IO;
    }
}

static LlmStatus rest_complete(LlmProvider *self, const LlmChat *chat,
                               LlmReply *out, char **errmsg)
{
    const LlmConfig *cfg = self->cfg;
    const LlmRestFlavor *fl = &self->flavor;
    JVal *payload;
    char *body;
    long bodylen = 0;
    int streaming = 0;
    int status = 0;
    const char *hdrs[8];
    char authbuf[320];
    size_t nhdrs;
    LlmStatus rc;

    if (!chat || chat->nmsgs == 0) {
        set_err(errmsg, "no messages to send");
        return LLM_ERR_ARGS;
    }
    if (!cfg->model[0]) {
        set_err(errmsg, "no model configured (BRACKETS_LLM_MODEL)");
        return LLM_ERR_CONFIG;
    }
    if (g_intr_request) {
        set_err(errmsg, "interrupted");
        return LLM_ERR_INTERRUPTED;
    }

    payload = build_payload(cfg, chat, fl, &streaming);
    body = payload_to_json(payload, &bodylen);
    if (!body) {
        set_err(errmsg, "out of memory while building the request");
        return LLM_ERR_IO;
    }
    nhdrs = build_headers(cfg, fl, streaming, hdrs, 8, authbuf, sizeof(authbuf));

    dbg(self, "POST %s (model=%s, %zu messages, stream=%d, temp=%.2f)",
        self->endpoint, cfg->model, chat->nmsgs, streaming, cfg->temperature);

    if (!streaming) {
        char *resp = NULL;
        long resplen = 0;
        int prc;

        prc = http_post_json_ex(cfg->base_url, REST_PATH, hdrs, nhdrs,
                                body, bodylen, &resp, &resplen, &status,
                                cfg->timeout_sec, errmsg);
        if (prc != 0) {
            free(body);
            return post_status(prc, status);
        }
        free(body);
        dbg(self, "HTTP %d, %ld bytes", status, resplen);
        if (!resp || !*resp) {
            free(resp);
            set_err(errmsg, "%s: the server returned an empty answer",
                    self->endpoint);
            return LLM_ERR_PARSE;
        }
        rc = reply_from_json(self, resp, out, errmsg);
        free(resp);
        return rc;
    }

    {
        StreamAcc acc;
        char *serr = NULL;
        int prc;

        memset(&acc, 0, sizeof(acc));
        sb_init(&acc.text);
        acc.chat = chat;
        acc.in_tokens = acc.out_tokens = -1;

        prc = http_post_stream(cfg->base_url, REST_PATH, hdrs, nhdrs,
                               body, bodylen, on_stream_line, &acc,
                               cfg->timeout_sec, &status, &serr);
        if (prc != 0) {
            LlmStatus frc = post_status(prc, status);

            free(body);
            if (frc == LLM_ERR_INTERRUPTED || frc == LLM_ERR_HTTP) {
                set_err(errmsg, "%s", serr ? serr : "the request failed");
                free(serr);
                acc_free(&acc);
                return frc;
            }
            /* the transfer broke, but partial text is still worth showing */
            if (!acc.text.len) {
                set_err(errmsg, "%s", serr ? serr : "the stream failed");
                free(serr);
                acc_free(&acc);
                return frc;
            }
            dbg(self, "stream aborted after %d bytes: %s",
                (int)sb_len(&acc.text), serr ? serr : "?");
            if (serr)
                free(serr);
        } else {
            free(serr);
        }
        free(body);

        dbg(self, "stream finished: %d events, %d skipped, %d bytes, done=%d",
            acc.events, acc.skipped, (int)sb_len(&acc.text), acc.done);

        if (!acc.events && !acc.text.len) {
            set_err(errmsg,
                    "%s: no usable event in the stream (the server may not "
                    "support streaming; try BRACKETS_LLM_STREAM=0)",
                    self->endpoint);
            acc_free(&acc);
            return LLM_ERR_PROTOCOL;
        }
        if (acc.skipped)
            dbg(self, "%d stream line(s) could not be used", acc.skipped);

        /* servers that do not repeat the model id in every chunk */
        if (!acc.model)
            acc.model = strdup(cfg->model);

        out->text = strdup(sb_cstr(&acc.text));
        out->raw = acc_to_raw_json(&acc);
        out->finish_reason = acc.finish ? strdup(acc.finish) : NULL;
        out->in_tokens = acc.in_tokens;
        out->out_tokens = acc.out_tokens;
        out->has_tool_calls = acc.ntc > 0;
        acc_free(&acc);
        return LLM_OK;
    }
}

static void rest_close(LlmProvider *self)
{
    free((char *)self->name);
    free((char *)self->endpoint);
    free(self);
}

LlmProvider *llm_rest_provider_new(LlmConfig *cfg,
                                   const LlmRestFlavor *flavor)
{
    LlmProvider *p;
    const LlmRestFlavor *fl = flavor ? flavor : NULL;
    static const LlmRestFlavor default_flavor = {
        LLM_BACKEND_OPENAI, 1, 1, 0
    };
    char *name, *ep;

    if (!cfg || !cfg->base_url[0] || !cfg->model[0])
        return NULL;
    if (!fl)
        fl = &default_flavor;

    ep = malloc(768);
    if (!ep)
        return NULL;
    if (llm_chat_endpoint(cfg->base_url, ep, 768) != 0) {
        free(ep);
        return NULL;
    }
    name = strdup(fl->name ? fl->name : LLM_BACKEND_OPENAI);
    if (!name) {
        free(ep);
        return NULL;
    }
    p = calloc(1, sizeof(*p));
    if (!p) {
        free(name);
        free(ep);
        return NULL;
    }
    p->name = name;
    p->endpoint = ep;
    p->cfg = cfg;
    p->flavor = *fl;
    p->complete = rest_complete;
    p->close = rest_close;
    return p;
}

LlmProvider *llm_openai_provider_new(LlmConfig *cfg)
{
    static const LlmRestFlavor fl = {
        LLM_BACKEND_OPENAI, 1 /* send_auth */, 1 /* allow_stream */, 0
    };
    return llm_rest_provider_new(cfg, &fl);
}
