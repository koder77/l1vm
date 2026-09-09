/*
 * This file lspclient.h is part of L1vm.
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
 *
 * Spawns l1vm-lsp as a child process and speaks JSON-RPC over its stdio
 * (Content-Length framed). Capabilities used: didOpen/didChange for
 * diagnostics (static analysis + real l1com/l1pre compiler diagnostics).
 */

#ifndef BRACKETS_LSPCLIENT_H
#define BRACKETS_LSPCLIENT_H

#include <stddef.h>

/* A single diagnostic returned by the server. */
typedef struct {
    int severity;      /* 1=error, 2=warning, 3=info, 4=hint */
    int line;          /* 0-based (matches the source) */
    char *source;      /* "l1vm" (static) or "l1com" (compiler) */
    char *message;
} LspDiag;

typedef struct {
    LspDiag *items;
    size_t len;
    size_t cap;
} LspDiagVec;

typedef struct LspClient LspClient;

/*
 * Start the language server. Returns NULL on failure.
 * `l1com_enabled`: 1 => run real compiler diagnostics (needs l1com on PATH).
 * `include_dir`: directory for <...> includes (may be NULL).
 */
LspClient *lsp_start(const char *lsp_path, int l1com_enabled,
                     const char *l1com_path, const char *include_dir);

void lsp_stop(LspClient *c);

/* Open (or replace) a document and return diagnostics. */
int lsp_check(LspClient *c, const char *uri, const char *path,
              const char *text, LspDiagVec *out);

void lsp_diagvec_free(LspDiagVec *v);

#endif
