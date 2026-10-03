/*
 * This file config.c is part of L1vm.
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
 * brackets-llm - configuration
 */

#ifndef BRACKETS_CONFIG_H
#define BRACKETS_CONFIG_H

#include "provider.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- default configuration with environment overrides ---- */

/* Read a boolean-ish environment variable ("1/yes/true/on" = on). */
static int cfg_bool(const char *v) {
    return v && (*v == '1' || *v == 'y' || *v == 'Y' || *v == 't' ||
                 *v == 'T' || *v == 'o' || *v == 'O');
}

/*
 * Model backend:
 *   openai    POST {base_url}/chat/completions with Authorization: Bearer
 *             (the universal standard: Ollama, vLLM, LM Studio, llama.cpp,
 *             a local opencode model server) - the default
 *   llamacpp  same protocol plus chat_template_kwargs.enable_thinking=false,
 *             no auth header, never streams
 *   auto      probe {base_url}/models, fall back to llamacpp
 */
static const char *cfg_backend(void) {
    const char *v = getenv("BRACKETS_LLM_BACKEND");
    return (v && *v) ? v : "openai";
}

/*
 * Base URL of the OpenAI-compatible API. A missing path is completed with
 * "/v1" by llm_normalize_base_url(), so all of these work:
 *   http://localhost:11434        (Ollama)
 *   http://localhost:8000         (vLLM)
 *   http://127.0.0.1:1234/v1      (LM Studio)
 *   http://127.0.0.1:8080         (llama-server)
 * BRACKETS_LLM_URL is still accepted as the older spelling.
 */
static const char *cfg_base_url(void) {
    const char *v = getenv("BRACKETS_LLM_BASE_URL");
    if (v && *v)
        return v;
    v = getenv("BRACKETS_LLM_URL");
    if (v && *v)
        return v;
    return "http://127.0.0.1:8080/v1";
}

/* Kept for the places that only print the address. */
__attribute__((unused))
static const char *cfg_server_url(void) {
    return cfg_base_url();
}

static const char *cfg_model(void) {
    const char *v = getenv("BRACKETS_LLM_MODEL");
    return v ? v : "Qwen3.6-35B-A3B";
}

/* Optional token; most local servers ignore it, a gateway may need it. */
static const char *cfg_api_key(void) {
    const char *v = getenv("BRACKETS_LLM_API_KEY");
    if (v && *v)
        return v;
    return getenv("OPENAI_API_KEY");
}

/* -1 means: do not send the field at all, the server decides. */
static double cfg_double(const char *name, double fallback) {
    const char *v = getenv(name);
    if (v && *v)
        return strtod(v, NULL);
    return fallback;
}

static long cfg_long(const char *name, long fallback) {
    const char *v = getenv(name);
    if (v && *v)
        return strtol(v, NULL, 10);
    return fallback;
}

/* Assemble the provider configuration from the environment. */
static void cfg_llm(LlmConfig *c) {
    llm_config_defaults(c);
    snprintf(c->base_url, sizeof(c->base_url), "%s", cfg_base_url());
    snprintf(c->model, sizeof(c->model), "%s", cfg_model());
    if (cfg_api_key())
        snprintf(c->api_key, sizeof(c->api_key), "%s", cfg_api_key());
    c->temperature = cfg_double("BRACKETS_LLM_TEMPERATURE", -1.0);
    c->top_p = cfg_double("BRACKETS_LLM_TOP_P", -1.0);
    c->max_tokens = cfg_long("BRACKETS_LLM_MAX_TOKENS", 0);
    c->stream = cfg_bool(getenv("BRACKETS_LLM_STREAM"));
    c->timeout_sec = (int)cfg_long("BRACKETS_LLM_TIMEOUT", 0);
    c->debug = cfg_bool(getenv("BRACKETS_LLM_DEBUG"));
    c->hist_budget = cfg_long("BRACKETS_LLM_HISTORY_BUDGET",
                              c->hist_budget);
}

static const char *cfg_lsp_path(void) {
    const char *v = getenv("L1VM_LSP");
    if (v && *v)
        return v;
    v = getenv("HOME");
    if (v) {
        static char buf[4096];
        snprintf(buf, sizeof(buf), "%s/l1vm/bin/l1vm-lsp", v);
        return buf;
    }
    return "l1vm-lsp";
}

static const char *cfg_l1com_path(void) {
    const char *v = getenv("L1VM_L1COM");
    if (v && *v)
        return v;
    v = getenv("HOME");
    if (v) {
        static char buf[4096];
        snprintf(buf, sizeof(buf), "%s/l1vm/bin/l1com", v);
        return buf;
    }
    return "l1com";
}

static const char *cfg_include_dir(void) {
    const char *v = getenv("L1VM_INCLUDE_DIR");
    if (v && *v)
        return v;
    v = getenv("HOME");
    if (v) {
        static char buf[8192];
        snprintf(buf, sizeof(buf), "%s/l1vm/include", v);
        return buf;
    }
    return "/home/l1vm/include";
}

/* system prompt file, default next to the project tree */
static const char *cfg_system_prompt_path(void) {
    const char *v = getenv("BRACKETS_LLM_SYSTEM_PROMPT");
    if (v && *v)
        return v;
    return "l1vm-system-prompt.txt";
}

#define BRACKETS_LLM_VERSION "0.2.0"

#endif
