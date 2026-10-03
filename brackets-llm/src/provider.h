/*
 * This file provider.h is part of L1vm.
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
 * brackets-llm - model backend abstraction ("provider")
 *
 * The rest of the program never talks to a model server directly: it hands
 * a provider-neutral LlmChat to a provider and gets an LlmReply back. That
 * makes the prompt hand-over independent of the actual model executor.
 *
 *   LlmConfig    where and how to talk (base_url, model, api_key, ...)
 *   LlmChat      the conversation + optional tool definitions
 *   LlmReply     assistant text + raw OpenAI shaped JSON + token usage
 *   LlmProvider  the backend itself (complete/close + its name)
 *
 * Backends:
 *   "openai"   the universal standard POST {base_url}/chat/completions.
 *              Works with Ollama, vLLM, LM Studio, llama-server and a local
 *              opencode model server. Sends Authorization: Bearer <api_key>,
 *              supports Server-Sent Events streaming. (default)
 *   "llamacpp" the historical llama.cpp behaviour: same endpoint, but it
 *              also sends chat_template_kwargs.enable_thinking=false for
 *              Qwen3-style thinking models and never streams.
 *   "auto"     probe {base_url}/models and pick "openai" when the server
 *              answers, otherwise fall back to "llamacpp".
 */

#ifndef BRACKETS_PROVIDER_H
#define BRACKETS_PROVIDER_H

#include <stddef.h>

/* ---------------------------------------------------------------- */
/* provider neutral value types                                      */
/* ---------------------------------------------------------------- */

/* One conversation message. `role` is "system", "user" or "assistant". */
typedef struct {
    const char *role;
    const char *content;
} LlmMsg;

/*
 * A request. `msgs` is owned by the LlmChat (llm_chat_free releases it),
 * the strings themselves are borrowed from the caller. `on_delta`, when
 * given, is called with every content fragment as it arrives (streaming);
 * the caller decides what to do with it (print, log, discard).
 */
typedef struct {
    LlmMsg     *msgs;
    size_t      nmsgs;
    const char *tools_json;    /* OpenAI "tools" array as JSON text, or NULL */
    void      (*on_delta)(const char *text, void *ud);
    void       *on_delta_ud;
} LlmChat;

typedef struct {
    char *text;               /* assistant content, never NULL (may be "") */
    char *raw;                /* OpenAI shaped response object, never NULL */
    char *finish_reason;      /* "stop", "tool_calls", ... or NULL */
    long  in_tokens;          /* -1 when the server did not report it */
    long  out_tokens;         /* -1 when the server did not report it */
    int   has_tool_calls;
} LlmReply;

void llm_reply_init(LlmReply *r);
void llm_reply_clear(LlmReply *r);

typedef enum {
    LLM_OK = 0,
    LLM_ERR_ARGS = -1,        /* caller passed something impossible */
    LLM_ERR_CONFIG = -2,      /* base_url/model missing or malformed */
    LLM_ERR_CONNECT = -3,     /* no connection to the server */
    LLM_ERR_HTTP = -4,        /* 4xx / 5xx */
    LLM_ERR_PARSE = -5,       /* answer is not valid JSON */
    LLM_ERR_PROTOCOL = -6,    /* valid JSON, but no usable answer in it */
    LLM_ERR_INTERRUPTED = -7, /* Ctrl+C */
    LLM_ERR_IO = -8           /* transport problem */
} LlmStatus;

const char *llm_status_str(LlmStatus s);

/* ---------------------------------------------------------------- */
/* configuration                                                     */
/* ---------------------------------------------------------------- */

typedef struct {
    char   base_url[512];     /* "http://localhost:11434/v1"            */
    char   model[256];        /* "big-pickle", "qwen2.5-coder", ...     */
    char   api_key[256];      /* "" -> no Authorization header           */
    double temperature;       /* < 0 -> leave it to the server          */
    double top_p;             /* < 0 -> leave it to the server          */
    long   max_tokens;        /* <= 0 -> not sent                        */
    int    stream;            /* 1 -> request SSE streaming              */
    int    timeout_sec;       /* 0 -> wait as long as the model needs    */
    int    debug;             /* protocol logging on stderr              */
    long   hist_budget;       /* max characters of history per request  */
} LlmConfig;

void llm_config_defaults(LlmConfig *c);

/*
 * Normalize a user supplied base URL:
 *   "host:port", "http://host:port"          -> .../v1
 *   "http://host:port/"                      -> .../v1
 *   "http://host:8080/v1"                    -> unchanged
 *   "http://host:11434/v1/chat/completions"  -> .../v1
 * Returns 0 on success, -1 when `in` is not a usable http URL.
 */
int llm_normalize_base_url(const char *in, char *out, size_t outsz);

/* ---------------------------------------------------------------- */
/* chat assembly                                                     */
/* ---------------------------------------------------------------- */

void llm_chat_init(LlmChat *c, const char *tools_json);
void llm_chat_free(LlmChat *c);

/*
 * Append a message history, keeping the newest messages that fit into
 * `budget_chars`. Every "system" message is always kept (the L1VM system
 * prompt must never be dropped), older chat turns are dropped first.
 * Returns the number of messages kept, or 0 on error.
 */
size_t llm_chat_add_history(LlmChat *c, const LlmMsg *msgs, size_t nmsgs,
                            long budget_chars);

/* ---------------------------------------------------------------- */
/* the provider itself                                               */
/* ---------------------------------------------------------------- */

#define LLM_BACKEND_AUTO     "auto"
#define LLM_BACKEND_OPENAI   "openai"
#define LLM_BACKEND_LLAMACPP "llamacpp"

typedef struct LlmProvider LlmProvider;

/*
 * The wire-format flavour of a REST backend. The OpenAI-compatible core
 * implements exactly one protocol; a backend differs only in these flags.
 */
typedef struct {
    const char *name;        /* backend id for diagnostics */
    int send_auth;           /* send "Authorization: Bearer <api_key>" */
    int allow_stream;        /* honour LlmConfig.stream */
    int add_thinking_ctl;    /* send chat_template_kwargs.enable_thinking */
} LlmRestFlavor;

struct LlmProvider {
    const char    *name;             /* backend id                          */
    const char    *endpoint;         /* full URL of the chat endpoint       */
    const LlmConfig *cfg;
    LlmRestFlavor flavor;            /* protocol flags of this backend      */

    /* Run one chat completion. On success (LLM_OK) *out holds a reply that
     * the caller releases with llm_reply_clear(). On failure *out stays
     * empty and *errmsg (when given) receives a heap string that explains
     * what went wrong - always safe to print. */
    LlmStatus (*complete)(LlmProvider *self, const LlmChat *chat,
                          LlmReply *out, char **errmsg);
    void      (*close)(LlmProvider *self);
};

/*
 * Create a provider for `backend` ("openai", "llamacpp" or "auto"; NULL
 * means LLM_BACKEND_OPENAI). Returns NULL and sets *errmsg on a bad
 * backend id or an unusable base_url. Release with llm_provider_free().
 */
LlmProvider *llm_provider_create(LlmConfig *cfg, const char *backend,
                                 char **errmsg);
void llm_provider_free(LlmProvider *p);

/* Model ids the server reports ({"data":[{"id":...}]}). Returns a heap
 * string with one id per line, or NULL when the server is unreachable. */
char *llm_provider_list_models(const LlmProvider *p);

/* Convenience for the paths that only want the answer text. */
LlmStatus llm_complete_text(LlmProvider *p, const LlmChat *chat, char **text,
                            long *in_tok, long *out_tok, char **errmsg);

/* ---------------------------------------------------------------- */
/* shared REST core (provider_openai.c), also used by the llama.cpp  */
/* backend, so both speak exactly the same wire format                */
/* ---------------------------------------------------------------- */

LlmProvider *llm_rest_provider_new(LlmConfig *cfg,
                                   const LlmRestFlavor *flavor);

LlmProvider *llm_openai_provider_new(LlmConfig *cfg);
LlmProvider *llm_llamacpp_provider_new(LlmConfig *cfg);

/* Build the chat endpoint URL of a normalized base_url (".../chat/completions"). */
int llm_chat_endpoint(const char *base_url, char *out, size_t outsz);

#endif
