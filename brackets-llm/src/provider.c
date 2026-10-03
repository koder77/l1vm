/*
 * This file provider.c is part of L1vm.
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
 * brackets-llm - provider glue
 *
 * Backend independent parts only: status strings, configuration defaults,
 * base URL normalization, chat assembly with a history budget, and the
 * backend factory. The protocol itself lives in provider_openai.c
 * (OpenAI-compatible REST) and provider_llamacpp.c.
 */

#define _POSIX_C_SOURCE 200809L

#include "provider.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "http.h"
#include "json.h"
#include "sb.h"

/* history budget in characters: the server context is 65536 tokens and the
 * L1VM system prompt alone is ~18k tokens, so only a bounded conversation
 * plus the generated answer fit next to it */
#define HIST_BUDGET_DEFAULT 150000L

/* ---------------------------------------------------------------- */
/* status + reply helpers                                            */
/* ---------------------------------------------------------------- */

const char *llm_status_str(LlmStatus s)
{
    switch (s) {
    case LLM_OK:              return "ok";
    case LLM_ERR_ARGS:        return "invalid arguments";
    case LLM_ERR_CONFIG:      return "invalid configuration";
    case LLM_ERR_CONNECT:     return "cannot connect to the server";
    case LLM_ERR_HTTP:        return "the server rejected the request";
    case LLM_ERR_PARSE:       return "the answer is not valid JSON";
    case LLM_ERR_PROTOCOL:    return "unexpected answer format";
    case LLM_ERR_INTERRUPTED: return "interrupted";
    case LLM_ERR_IO:          return "transport error";
    }
    return "unknown error";
}

void llm_reply_init(LlmReply *r)
{
    memset(r, 0, sizeof(*r));
    r->in_tokens = -1;
    r->out_tokens = -1;
}

void llm_reply_clear(LlmReply *r)
{
    if (!r)
        return;
    free(r->text);
    free(r->raw);
    free(r->finish_reason);
    llm_reply_init(r);
}

/* ---------------------------------------------------------------- */
/* configuration                                                     */
/* ---------------------------------------------------------------- */

void llm_config_defaults(LlmConfig *c)
{
    if (!c)
        return;
    memset(c, 0, sizeof(*c));
    snprintf(c->base_url, sizeof(c->base_url), "http://127.0.0.1:8080/v1");
    c->model[0] = '\0';
    c->api_key[0] = '\0';
    c->temperature = -1.0;            /* -1: leave it to the server */
    c->top_p = -1.0;
    c->max_tokens = 0;                /* 0: not sent */
    c->stream = 0;
    c->timeout_sec = 0;               /* 0: wait for the model */
    c->debug = 0;
    c->hist_budget = HIST_BUDGET_DEFAULT;
}

int llm_chat_endpoint(const char *base_url, char *out, size_t outsz)
{
    size_t n;

    if (!base_url || !*base_url || !out || outsz < 2)
        return -1;
    n = strlen(base_url);
    if (n >= outsz)
        return -1;
    snprintf(out, outsz, "%s/chat/completions", base_url);
    return 0;
}

int llm_normalize_base_url(const char *in, char *out, size_t outsz)
{
    char tmp[512];
    size_t n;
    int has_scheme = 0;

    if (!in || !out || outsz < 2)
        return -1;

    while (*in == ' ' || *in == '\t')
        in++;
    n = strlen(in);
    while (n > 0 && (in[n - 1] == ' ' || in[n - 1] == '\t' ||
                     in[n - 1] == '\n' || in[n - 1] == '\r'))
        n--;
    if (n == 0 || n >= sizeof(tmp))
        return -1;
    memcpy(tmp, in, n);
    tmp[n] = '\0';

    /* a host without a scheme is understood as plain http:// */
    if (strstr(tmp, "://")) {
        has_scheme = 1;
        if (strncasecmp(tmp, "http://", 7) != 0 &&
            strncasecmp(tmp, "https://", 8) != 0)
            return -1;
    } else {
        if (strchr(tmp, ' '))
            return -1;
        memmove(tmp + 7, tmp, strlen(tmp) + 1);
        memcpy(tmp, "http://", 7);
    }

    /* drop trailing slashes */
    n = strlen(tmp);
    while (n > 1 && tmp[n - 1] == '/')
        tmp[--n] = '\0';

    /* a full endpoint was given: cut it back to its base */
    {
        static const char *suffixes[] = {
            "/chat/completions", "/completions", NULL
        };
        int i;
        for (i = 0; suffixes[i]; i++) {
            size_t sl = strlen(suffixes[i]);
            if (n > sl && strcasecmp(tmp + n - sl, suffixes[i]) == 0) {
                n -= sl;
                tmp[n] = '\0';
                while (n > 1 && tmp[n - 1] == '/')
                    tmp[--n] = '\0';
                break;
            }
        }
    }

    /* no path at all: the OpenAI endpoints live under /v1 */
    {
        const char *hostend = strstr(tmp, "://");
        size_t skip = 0;
        if (hostend)
            skip = (size_t)(hostend - tmp) + 3;
        if (!strchr(tmp + skip, '/'))
            strncat(tmp, "/v1", sizeof(tmp) - strlen(tmp) - 1);
    }

    if (strlen(tmp) >= outsz)
        return -1;
    (void)has_scheme;
    snprintf(out, outsz, "%s", tmp);
    return 0;
}

/* ---------------------------------------------------------------- */
/* chat assembly                                                     */
/* ---------------------------------------------------------------- */

void llm_chat_init(LlmChat *c, const char *tools_json)
{
    memset(c, 0, sizeof(*c));
    c->tools_json = tools_json;
}

void llm_chat_free(LlmChat *c)
{
    if (!c)
        return;
    free(c->msgs);
    c->msgs = NULL;
    c->nmsgs = 0;
}

static int chat_reserve(LlmChat *c, size_t n)
{
    LlmMsg *nm;

    if (n <= c->nmsgs)
        return 0;
    nm = realloc(c->msgs, n * sizeof(LlmMsg));
    if (!nm)
        return -1;
    c->msgs = nm;
    return 0;
}

static int msg_is_system(const LlmMsg *m)
{
    return m->role && strcmp(m->role, "system") == 0;
}

size_t llm_chat_add_history(LlmChat *c, const LlmMsg *msgs, size_t nmsgs,
                            long budget_chars)
{
    size_t i, first_kept = nmsgs, nsys_head = 0, nsel;
    long budget = budget_chars > 0 ? budget_chars : HIST_BUDGET_DEFAULT;

    if (!c || !msgs || nmsgs == 0)
        return 0;

    /* Keep the newest chat turns while they fit into the budget. System
     * prompts (the L1VM prompt is ~18k tokens on its own) never count
     * against it and are always sent. */
    for (i = nmsgs; i-- > 0;) {
        long len;
        if (msg_is_system(&msgs[i]))
            continue;
        len = msgs[i].content ? (long)strlen(msgs[i].content) : 0L;
        if (len > budget)
            break;
        budget -= len;
        first_kept = i;
    }
    /* the newest message must go out even when it alone is over budget */
    if (first_kept == nmsgs) {
        for (i = nmsgs; i-- > 0;) {
            if (!msg_is_system(&msgs[i])) {
                first_kept = i;
                break;
            }
        }
    }
    for (i = 0; i < first_kept; i++)
        if (msg_is_system(&msgs[i]))
            nsys_head++;
    nsel = nsys_head + (nmsgs - first_kept);

    if (chat_reserve(c, c->nmsgs + nsel) != 0)
        return 0;
    for (i = 0; i < nmsgs; i++) {
        if (i < first_kept && !msg_is_system(&msgs[i]))
            continue;
        c->msgs[c->nmsgs++] = msgs[i];
    }
    return nsel;
}

/* ---------------------------------------------------------------- */
/* backend factory                                                   */
/* ---------------------------------------------------------------- */

/* Probe {base_url}/models: 200 means "an OpenAI-compatible server". */
static int probe_openai(const LlmConfig *cfg, char **errmsg)
{
    char url[768];
    char *body = NULL, *err = NULL;
    int status = 0;
    int ok = 0;

    if (snprintf(url, sizeof(url), "%s/models", cfg->base_url) >=
        (int)sizeof(url)) {
        if (errmsg)
            *errmsg = strdup("the base URL is too long");
        return -1;
    }
    if (http_get(url, 262144, &body, NULL, &status, NULL, NULL, &err) == 0) {
        JVal *r = body ? j_parse(body) : NULL;
        const JVal *data = r ? j_get(r, "data") : NULL;
        if (status == 200 && data && j_is(data, J_ARR))
            ok = 1;
        else if (status == 200 && r)
            ok = 1;                 /* answered, just no model list */
        j_free(r);
    }
    free(body);
    free(err);
    return ok ? 1 : 0;
}

LlmProvider *llm_provider_create(LlmConfig *cfg, const char *backend,
                                 char **errmsg)
{
    LlmProvider *p = NULL;
    const char *want = backend && *backend ? backend : LLM_BACKEND_OPENAI;
    char norm[512];

    if (!cfg || !cfg->model[0]) {
        if (errmsg)
            *errmsg = strdup("no model configured (set BRACKETS_LLM_MODEL)");
        return NULL;
    }
    if (llm_normalize_base_url(cfg->base_url, norm, sizeof(norm)) != 0) {
        if (errmsg)
            *errmsg = strdup("base_url must be http://host:port[/path], "
                             "e.g. http://localhost:11434/v1");
        return NULL;
    }
    snprintf(cfg->base_url, sizeof(cfg->base_url), "%s", norm);

    if (strcasecmp(want, LLM_BACKEND_OPENAI) == 0) {
        p = llm_openai_provider_new(cfg);
    } else if (strcasecmp(want, LLM_BACKEND_LLAMACPP) == 0) {
        p = llm_llamacpp_provider_new(cfg);
    } else if (strcasecmp(want, LLM_BACKEND_AUTO) == 0) {
        if (probe_openai(cfg, NULL) == 1)
            p = llm_openai_provider_new(cfg);
        else {
            fprintf(stderr, "warning: %s/models is not answering; using the "
                            "llama.cpp backend\n", cfg->base_url);
            p = llm_llamacpp_provider_new(cfg);
        }
    } else {
        if (errmsg) {
            char b[256];
            snprintf(b, sizeof(b), "unknown backend '%s' (use openai, "
                                   "llamacpp or auto)", want);
            *errmsg = strdup(b);
        }
        return NULL;
    }

    if (!p) {
        if (errmsg)
            *errmsg = strdup("cannot create the backend "
                             "(invalid base_url?)");
        return NULL;
    }
    if (errmsg && *errmsg) {
        p->close(p);                /* report the first failure only */
        return NULL;
    }
    return p;
}

void llm_provider_free(LlmProvider *p)
{
    if (p && p->close)
        p->close(p);
}

char *llm_provider_list_models(const LlmProvider *p)
{
    char url[768];
    char *body = NULL, *msg = NULL, *r = NULL;
    JVal *root;
    const JVal *data;

    if (!p || !p->cfg)
        return NULL;
    if (snprintf(url, sizeof(url), "%s/models", p->cfg->base_url) >=
        (int)sizeof(url))
        return NULL;
    if (http_get(url, 262144, &body, NULL, NULL, NULL, NULL, &msg) != 0) {
        char b[1024];
        snprintf(b, sizeof(b), "cannot reach %s: %s", url,
                 msg ? msg : "no answer");
        free(msg);
        free(body);
        return strdup(b);
    }
    free(msg);
    root = j_parse(body ? body : "");
    free(body);
    data = root ? j_get(root, "data") : NULL;
    if (!data || !j_is(data, J_ARR)) {
        j_free(root);
        return strdup("the server did not return a model list "
                      "(expected {\"data\":[{\"id\":...}]})");
    }
    {
        SB b;
        size_t i;
        sb_init(&b);
        for (i = 0; i < j_len(data); i++) {
            const JVal *m = j_at(data, i);
            const JVal *id = m ? j_get(m, "id") : NULL;
            if (id && j_is(id, J_STR) && j_str(id))
                sb_printf(&b, "  %s\n", j_str(id));
            else
                sb_add(&b, "  (unnamed model)\n");
        }
        r = strdup(sb_cstr(&b));
        sb_free(&b);
    }
    j_free(root);
    return r;
}

LlmStatus llm_complete_text(LlmProvider *p, const LlmChat *chat, char **text,
                            long *in_tok, long *out_tok, char **errmsg)
{
    LlmReply rep;
    LlmStatus rc;

    if (text)
        *text = NULL;
    if (in_tok)
        *in_tok = -1;
    if (out_tok)
        *out_tok = -1;
    if (!p || !p->complete) {
        if (errmsg)
            *errmsg = strdup("no backend");
        return LLM_ERR_CONFIG;
    }
    llm_reply_init(&rep);
    rc = p->complete(p, chat, &rep, errmsg);
    if (rc != LLM_OK) {
        llm_reply_clear(&rep);
        return rc;
    }
    if (text)
        *text = rep.text ? rep.text : strdup("");
    if (in_tok)
        *in_tok = rep.in_tokens;
    if (out_tok)
        *out_tok = rep.out_tokens;
    rep.text = NULL;
    llm_reply_clear(&rep);
    return LLM_OK;
}
