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

#include <stdlib.h>
#include <string.h>

/* ---- default configuration with environment overrides ---- */

static const char *cfg_server_url(void) {
    const char *v = getenv("BRACKETS_LLM_URL");
    return v ? v : "http://127.0.0.1:8080";
}

static const char *cfg_model(void) {
    const char *v = getenv("BRACKETS_LLM_MODEL");
    return v ? v : "Qwen3.6-35B-A3B";
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

#define BRACKETS_LLM_VERSION "0.1.0"

#endif
