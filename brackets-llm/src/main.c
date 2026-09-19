/*
 * This file main.c is part of L1vm.
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
 * brackets-llm - an opencode-like Brackets (L1VM) code environment.
 *
 * Speaks to a llama.cpp server (llama-server) via its OpenAI-compatible
 * /v1/chat/completions endpoint, feeds the L1VM system prompt, then lets
 * the user chat. The model can autonomously use file tools (read_file,
 * write_file, edit_file, list_files) like opencode, and Brackets (.l1com)
 * code in the answer is checked/auto-corrected with the l1vm-lsp language
 * server, then can be built and run.
 */

#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "config.h"
#include "http.h"
#include "inputline.h"
#include "intr.h"
#include "json.h"
#include "lspclient.h"
#include "sb.h"
#include "tools.h"

/* ANSI terminal colors. The "You> " prompt and everything the user types
 * are shown in a different color than the Assistant> side. */
#define COLOR_USER    "\033[1;36m"   /* bright cyan */
#define COLOR_RESET   "\033[0m"

/* ================== conversation ================== */

typedef struct {
    char *role;     /* "system" | "user" | "assistant" */
    char *content;
} Msg;

typedef struct {
    Msg *items;
    size_t len, cap;
} Hist;

static void hist_push(Hist *h, const char *role, const char *content)
{
    if (h->len == h->cap) {
        size_t nc = h->cap ? h->cap * 2 : 16;
        h->items = realloc(h->items, nc * sizeof(Msg));
        h->cap = nc;
    }
    h->items[h->len].role = strdup(role);
    h->items[h->len].content = strdup(content);
    h->len++;
}

static void hist_free(Hist *h)
{
    size_t i;
    for (i = 0; i < h->len; i++) {
        free(h->items[i].role);
        free(h->items[i].content);
    }
    free(h->items);
    memset(h, 0, sizeof(*h));
}

/* ================== file helpers ================== */

/* Read up to `limit` lines of a file starting at 1-based line `offset`.
 * limit < 0 reads everything. Returns a heap-allocated NUL-terminated
 * string, or NULL if the file cannot be opened. */
static char *read_file_part(const char *path, long limit, long offset)
{
    FILE *f = fopen(path, "rb");
    SB b;
    long lineno = 0, emitted = 0;
    char buf[32768];
    char *r;

    if (!f)
        return NULL;
    sb_init(&b);
    while (fgets(buf, sizeof(buf), f) != NULL) {
        lineno++;
        if (offset > 1 && lineno < offset)
            continue;
        if (limit >= 0 && emitted >= limit)
            break;
        sb_add(&b, buf);
        emitted++;
    }
    fclose(f);
    r = strdup(sb_cstr(&b));
    sb_free(&b);
    return r;
}

/* Read a whole file (private: whole-file form of read_file_part). */
static char *read_file(const char *path)
{
    return read_file_part(path, -1, 1);
}

/* Return a heap-allocated copy of lines `offset`..`offset+limit-1`
 * (1-based) of `text`. limit < 0 keeps everything from offsets on.
 * Preserves the original line endings. */
static char *slice_lines(const char *text, long limit, long offset)
{
    SB b;
    long lineno = 0, emitted = 0;
    const char *p = text;

    sb_init(&b);
    while (*p) {
        const char *e = strchr(p, '\n');
        size_t n = e ? (size_t)(e - p) + 1 : strlen(p);
        lineno++;
        if (offset > 1 && lineno < offset) {
            p += n;
            continue;
        }
        if (limit >= 0 && emitted >= limit)
            break;
        sb_addn(&b, p, n);
        emitted++;
        if (!e)
            break;
        p = e + 1;
    }
    {
        char *r = strdup(sb_cstr(&b));
        sb_free(&b);
        return r;
    }
}

/* Parse a signed integer after optional whitespace/'+'/'-'. */
static long parse_read_int(const char *s)
{
    long v = 0;
    int neg = 0;
    while (*s == ' ' || *s == '\t')
        s++;
    if (*s == '-') { neg = 1; s++; }
    else if (*s == '+') s++;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        s++;
    }
    return neg ? -v : v;
}

/* Parse an optional trailing "[limit=N, offset=M]" spec appended to a
 * /read argument. Writes the file path (without the spec) into `path`.
 * limit is stored non-negative when given (default -1 = unlimited);
 * offset is 1-based (default 1). */
static void parse_read_spec(const char *arg, char *path, size_t path_sz,
                            long *limit, long *offset)
{
    const char *b = strchr(arg, '[');
    size_t plen = b ? (size_t)(b - arg) : strlen(arg);
    const char *q;

    *limit = -1;
    *offset = 1;
    while (plen > 0 && (arg[plen - 1] == ' ' || arg[plen - 1] == '\t'))
        plen--;
    if (plen >= path_sz)
        plen = path_sz - 1;
    memcpy(path, arg, plen);
    path[plen] = '\0';
    if (!b)
        return;

    q = b + 1;
    while (q && *q && *q != ']') {
        const char *eq;
        size_t keylen;
        while (*q == ' ' || *q == '\t')
            q++;
        eq = strchr(q, '=');
        if (!eq)
            break;
        keylen = (size_t)(eq - q);
        if (keylen == 5 && strncmp(q, "limit", 5) == 0)
            *limit = parse_read_int(eq + 1);
        else if (keylen == 6 && strncmp(q, "offset", 6) == 0)
            *offset = parse_read_int(eq + 1);
        q = strchr(eq, ',');
        if (q)
            q++;
    }
}

static int write_file(const char *path, const char *content)
{
    FILE *f = fopen(path, "wb");
    size_t n = strlen(content);
    if (!f)
        return -1;
    if (fwrite(content, 1, n, f) != n) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

/* ================== llama-server interaction ================== */

/* rough budget in characters for history beyond the (large) system prompt.
   Server context is 65536 tokens; system prompt ~18k tokens stays under that,
   leaving room for a bounded conversation + the generated response. */
#define LLM_HISTORY_CHAR_BUDGET 150000

/* Send a chat request to the LLM. If `tools_json` (a JSON "tools" array or
 * NULL) is given it is included so the server may answer with tool_calls.
 * On success (0) the RAW JSON response body is stored in *out_raw (heap,
 * caller frees) and parsed and examined by the caller. */
static int llm_call(const Hist *hist, const char *model, const char *url,
                    const char *tools_json, char **out_raw,
                    long *out_in_tok, long *out_out_tok)
{
    JVal *payload = j_obj_new();
    JVal *msgs = j_arr_new();
    SB b;
    char *body;
    char *resp = NULL;
    long resplen = 0;
    int status = 0;
    size_t i;
    long budget_rem = LLM_HISTORY_CHAR_BUDGET;
    int ret = -1;

    *out_raw = NULL;

    j_obj_set(payload, "model", j_str_new(model));

    /* Walk the history backwards, keeping the system prompt plus the most
     * recent messages within the budget so we never overflow the server's
     * context (the system prompt alone is ~18k tokens). */
    {
        size_t sys_i = 0;
        int drop_tail = 0;
        long *acc = malloc((hist->len ? hist->len : 1) * sizeof(long));
        if (!acc)
            acc = NULL;
        for (i = 0; i < hist->len; i++) {
            if (hist->items[i].role && strcmp(hist->items[i].role, "system") == 0)
                sys_i = i;
            acc[i] = (long)strlen(hist->items[i].content);
        }
        /* compute, from the end, how many messages fit in budget */
        for (i = hist->len; i-- > 0;) {
            if ((long)i == (long)sys_i)
                continue;
            if (acc[i] <= budget_rem) {
                budget_rem -= acc[i];
            } else {
                drop_tail = (int)i + 1;
                break;
            }
        }
        for (i = 0; i < hist->len; i++) {
            if ((long)i < (long)drop_tail && (long)i != (long)sys_i)
                continue;
            {
                JVal *m = j_obj_new();
                j_obj_set(m, "role", j_str_new(hist->items[i].role));
                j_obj_set(m, "content", j_str_new(hist->items[i].content));
                j_arr_push(msgs, m);
            }
        }
        free(acc);
    }

    j_obj_set(payload, "messages", msgs);
    j_obj_set(payload, "stream", j_bool_new(0));
    j_obj_set(payload, "temperature", j_num_new(0.7));
    if (tools_json) {
        JVal *t = j_parse(tools_json);
        if (t)
            j_obj_set(payload, "tools", t);
    }
    {
        JVal *ctk = j_obj_new();
        j_obj_set(ctk, "enable_thinking", j_bool_new(0));
        j_obj_set(payload, "chat_template_kwargs", ctk);
    }

    sb_init(&b);
    j_emit(payload, &b);
    body = strdup(sb_cstr(&b));
    sb_free(&b);
    j_free(payload);

    if (http_post_json(url, "/v1/chat/completions",
                       body, (long)strlen(body), &resp, &resplen,
                       &status) == 0) {
        if (resp && *resp) {
            *out_raw = resp;
            resp = NULL;
            if (out_in_tok)
                *out_in_tok = -1;
            if (out_out_tok)
                *out_out_tok = -1;
            if (out_in_tok || out_out_tok) {
                JVal *ru = j_parse(*out_raw);
                const JVal *usage = ru ? j_get(ru, "usage") : NULL;
                if (usage) {
                    const JVal *pn = j_get(usage, "prompt_tokens");
                    const JVal *cn = j_get(usage, "completion_tokens");
                    if (pn && j_is(pn, J_NUM) && out_in_tok)
                        *out_in_tok = (long)j_num(pn);
                    if (cn && j_is(cn, J_NUM) && out_out_tok)
                        *out_out_tok = (long)j_num(cn);
                }
                j_free(ru);
            }
            ret = 0;
        } else {
            ret = -1;
        }
    }
    free(resp);
    free(body);
    return ret;
}

/*
 * Convenience wrapper used by the auto-correction loop: returns just the
 * assistant's text reply (first choice), not tool calls. If `tools_json` is
 * NULL the server may not emit tool calls; otherwise any tool_calls are
 * ignored here (the caller wants plain text).
 */
static int llm_text(const Hist *hist, const char *model, const char *url,
                    const char *tools_json, char **out_text,
                    long *out_in_tok, long *out_out_tok)
{
    char *raw = NULL;
    int rc = llm_call(hist, model, url, tools_json, &raw,
                      out_in_tok, out_out_tok);
    JVal *r;
    const JVal *choices, *msg, *content;

    *out_text = NULL;
    if (rc != 0 || !raw)
        return rc;
    r = j_parse(raw);
    choices = r ? j_get(r, "choices") : NULL;
    msg = (choices && j_len(choices) > 0) ? j_at(choices, 0) : NULL;
    msg = msg ? j_get(msg, "message") : NULL;
    content = msg ? j_get(msg, "content") : NULL;
    if (content && j_is(content, J_STR) && j_str(content))
        *out_text = strdup(j_str(content));
    if (r)
        j_free(r);
    free(raw);
    return rc;
}

/* Show token usage when the server reports it (OpenAI-compatible "usage"). */
static void print_token_usage(long in_tok, long out_tok)
{
    if (in_tok < 0 && out_tok < 0)
        return;
    printf("[tokens in: %ld, out: %ld]\n",
           in_tok < 0 ? 0 : in_tok, out_tok < 0 ? 0 : out_tok);
}

#define COMPACT_KEEP_RECENT 8

/* Fold the older part of the conversation into a single summary message so
 * fewer context tokens are sent to the server. Keeps the system prompt and
 * the COMPACT_KEEP_RECENT most recent messages. Returns the number of
 * messages removed, or -1 when there was nothing to compact. */
static int compact_history(Hist *h, const char *model, const char *url)
{
    size_t lsys = 0;
    size_t i, old_start, old_end, keep;
    SB transcript, p;
    char *summary = NULL;
    long tin = -1, tout = -1;
    int rc;

    for (i = 0; i < h->len; i++)
        if (h->items[i].role && strcmp(h->items[i].role, "system") == 0)
            lsys = i;

    keep = COMPACT_KEEP_RECENT;
    old_start = lsys + 1;
    if (h->len - old_start <= keep) {
        printf("not enough conversation to compact.\n");
        return -1;
    }
    old_end = h->len - keep;

    sb_init(&transcript);
    for (i = old_start; i < old_end; i++)
        sb_printf(&transcript, "%s: %s\n",
                  h->items[i].role ? h->items[i].role : "?",
                  h->items[i].content);

    sb_init(&p);
    sb_printf(&p,
        "This is a Brackets (L1VM) coding session. Summarize the whole "
        "conversation below into a concise summary that keeps every file name, "
        "code decision, error that was found and fixed, and the current task. "
        "Output only the summary.\n\n%s", sb_cstr(&transcript));
    sb_free(&transcript);

    {
        Hist tmp = {0};
        hist_push(&tmp, "system", h->items[lsys].content);
        hist_push(&tmp, "user", sb_cstr(&p));
        rc = llm_text(&tmp, model, url, NULL, &summary, &tin, &tout);
        hist_free(&tmp);
    }
    print_token_usage(tin, tout);
    if (rc != 0 || !summary || !*summary) {
        free(summary);
        printf("[compact] model summary failed; keeping history.\n");
        return -1;
    }

    /* rebuild history: system prompt(s) + summary + most recent messages */
    {
        size_t n = (lsys + 1) + 1 + keep;
        Msg *ni = calloc(n, sizeof(Msg));
        SB s;
        if (!ni) {
            free(summary);
            return -1;
        }
        for (i = 0; i <= lsys; i++) {
            ni[i].role = h->items[i].role;
            ni[i].content = h->items[i].content;
        }
        for (i = old_start; i < old_end; i++) {
            free(h->items[i].role);
            free(h->items[i].content);
        }
        for (i = old_end; i < h->len; i++) {
            ni[lsys + 2 + (i - old_end)].role = h->items[i].role;
            ni[lsys + 2 + (i - old_end)].content = h->items[i].content;
        }
        ni[lsys + 1].role = strdup("user");
        sb_init(&s);
        sb_printf(&s, "Summary of the previous conversation:\n%s", summary);
        ni[lsys + 1].content = strdup(sb_cstr(&s));
        sb_free(&s);
        free(h->items);
        h->items = ni;
        h->len = n;
        h->cap = n;
    }
    free(summary);
    printf("[compact] collapsed %zu messages; history is now %zu messages.\n",
           old_end - old_start, h->len);
    return (int)(old_end - old_start);
}

/* ================== code extraction ================== */

/*
 * Derive "name.l1com" (without .l1com) from a leading "// name.l1com"
 * comment near the start of `text`. Stores into out_name (may stay empty).
 */
static void derive_name(const char *text, char *out_name, size_t outname_sz)
{
    const char *dot = strstr(text, ".l1com");
    const char *ln, *fn;
    size_t n;
    out_name[0] = '\0';
    if (!dot)
        return;
    /* walk back to the beginning of the line that holds ".l1com" */
    ln = dot;
    while (ln > text && ln > dot - 512 && *ln != '\n')
        ln--;
    if (*ln == '\n')
        ln++;
    /* strip comment/indent markers */
    fn = ln;
    while (*fn == '/' || *fn == '*' || *fn == ' ' || *fn == '\t')
        fn++;
    n = (size_t)(dot - fn);
    if (n > 0 && n < outname_sz) {
        memcpy(out_name, fn, n);
        out_name[n] = '\0';
    }
}

/*
 * Extract the Brackets program from the model's response text.
 * Recognizes a fenced block ```l1com ... ``` or ```...``` that contains
 * "(main func)", or a "// filename.l1com" comment, or just a block whose
 * trimmed body starts with "(main func)". Returns a heap string or NULL.
 * If out_name != NULL, "name.l1com" is derived from a leading
 * "// name.l1com" comment if present.
 */
static char *extract_code(const char *text, char *out_name, size_t outname_sz)
{
    const char *p;

    if (out_name)
        out_name[0] = '\0';

    /* try to capture a fenced code block */
    if ((p = strstr(text, "```")) != NULL) {
        const char *start = p + 3;
        const char *nl = strchr(start, '\n');
        const char *end;
        if (nl) {
            /* fence info line ends at newline */
            start = nl + 1;
        }
        end = strstr(start, "```");
        if (end) {
            size_t n = (size_t)(end - start);
            char *block = malloc(n + 1);
            if (block) {
                memcpy(block, start, n);
                block[n] = '\0';
                if (strstr(block, "(main func)") || strstr(block, "(main function")) {
                    if (out_name)
                        derive_name(block, out_name, outname_sz);
                    return block;
                }
                free(block);
            }
        }
    }

    /* fallback: find "(main func)" and take to end of file (best-effort) */
    p = strstr(text, "(main func)");
    if (p) {
        size_t n = strlen(p);
        char *block = strdup(p);
        if (block) {
            if (out_name)
                derive_name(text, out_name, outname_sz);
            (void)n;
            return block;
        }
    }
    return NULL;
}

/* ================== diagnostics formatting ================== */

static char *format_diags(const LspDiagVec *v, const char *label)
{
    SB b;
    size_t i;
    char *out;
    sb_init(&b);
    sb_printf(&b, "%s (%zu):\n", label, v->len);
    for (i = 0; i < v->len; i++) {
        sb_printf(&b, "  line %d [%s]: %s\n",
                  v->items[i].line + 1,
                  v->items[i].source ? v->items[i].source : "lsp",
                  v->items[i].message);
    }
    out = strdup(sb_cstr(&b));
    sb_free(&b);
    return out;
}

/* ================== build / run ================== */

static int run_cmd_capture(const char *cmd, char **out)
{
    FILE *fp = popen(cmd, "r");
    SB b;
    char line[4096];
    size_t total;
    *out = NULL;
    if (!fp)
        return -1;
    sb_init(&b);
    while (fgets(line, sizeof(line), fp))
        sb_add(&b, line);
    total = sb_len(&b);
    *out = total ? strdup(sb_cstr(&b)) : strdup("");
    sb_free(&b);
    return pclose(fp);
}

/* ================== LSP check + auto-correct ================== */

#define MAX_FIX_ITERS 5

/*
 * Save `code` to path `name.l1com`, check with the LSP. If errors are found,
 * iterate: present diagnostics to the LLM and ask for a full corrected
 * program until clean or MAX_FIX_ITERS. Returns final diagnostics (may be
 * non-empty if not fixed). On success (0 errors) returns the last code.
 */
/* session-wide counter so every LSP check opens a genuinely fresh document:
 * l1vm-lsp ignores didOpen for an already-open URI (publishing nothing). The
 * current server also runs the real l1com compiler on didChange, but a fresh
 * URI per check keeps each document's state independent and cheap. */
static unsigned long g_check_seq = 0;

static int is_l1com_file(const char *p)
{
    size_t n = p ? strlen(p) : 0;
    return (n >= 6 && strcmp(p + n - 6, ".l1com") == 0);
}

/*
 * After the model writes a .l1com via the write_file tool, verify the saved
 * file with l1vm-lsp. Returns a heap-allocated one-line (or few-line)
 * summary, or NULL when there is nothing to check.
 */
static char *lsp_summary_for_write(LspClient *lsp, const char *path)
{
    char *exp;
    FILE *f;
    long sz;
    char *buf, *summary;
    LspDiagVec diags = {0};
    char checkpath[131072];
    char uri[262144];
    SB b;
    int ck;
    size_t i;
    int nerr = 0;

    if (!lsp || !path || !is_l1com_file(path))
        return NULL;
    exp = tool_expand_path(path);
    if (!exp)
        return NULL;
    f = fopen(exp, "rb");
    if (!f) {
        free(exp);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > (1024L * 1024L)) {
        fclose(f);
        free(exp);
        return NULL;
    }
    buf = malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        free(exp);
        return NULL;
    }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        free(exp);
        return NULL;
    }
    fclose(f);
    buf[sz] = '\0';

    sb_init(&b);
    {
        char cwd[4096];
        if (!getcwd(cwd, sizeof(cwd)))
            strcpy(cwd, ".");
        snprintf(checkpath, sizeof(checkpath),
                 "%s/.brackets-check-%d-%lu.l1com", cwd, (int)getpid(),
                 ++g_check_seq);
    }
    snprintf(uri, sizeof(uri), "file://%s", checkpath);
    write_file(checkpath, buf);
    ck = lsp_check(lsp, uri, checkpath, buf, &diags);
    unlink(checkpath);

    if (ck != 0) {
        sb_add(&b, "LSP check: FAILED (l1vm-lsp not available or timed out)");
    } else {
        for (i = 0; i < diags.len; i++)
            if (diags.items[i].severity == 1)
                nerr++;
        if (nerr == 0) {
            sb_printf(&b, "LSP check: OK (%zu diagnostic%s, no errors)",
                      diags.len, diags.len == 1 ? "" : "s");
        } else {
            sb_printf(&b, "LSP check: %d error%s:\n",
                      nerr, nerr == 1 ? "" : "s");
            for (i = 0; i < diags.len; i++) {
                if (diags.items[i].severity != 1)
                    continue;
                if (i >= 6) {
                    sb_add(&b, "  ... (more)\n");
                    break;
                }
                sb_printf(&b, "  line %d [%s]: %s\n",
                          diags.items[i].line + 1,
                          diags.items[i].source ? diags.items[i].source : "lsp",
                          diags.items[i].message);
            }
        }
    }
    free(buf);
    free(exp);
    lsp_diagvec_free(&diags);
    summary = strdup(sb_cstr(&b));
    sb_free(&b);
    return summary;
}

/* ask the user whether to continue the auto-fix loop. Returns 1 to
 * continue, 0 to abort. Non-interactive sessions are never interrupted. */
static int ask_continue(void)
{
    return input_confirm("Continue? [y/N] ", 1);
}

/* check + auto-correct a program. Returns 0 when the saved file is clean,
 * 1 when it was saved but still has errors (max_iters reached),
 * 2 when the LSP could not verify it, -2 when the user aborted the
 * auto-fix loop (the file is NOT saved), -1 on early check failure. On every
 * return >= -1 the caller owns *final_code; *final_diags (if len > 0)
 * describes the last state.
 *
 * `max_iters` > 0 bounds the check/fix iterations and asks "Continue?" after
 * each failed iteration (the /lsp behaviour); `max_iters` <= 0 loops
 * repeatedly until the code is clean, prompting nothing and aborting
 * immediately on Ctrl+C (the /loop command). */
static int code_workflow(LspClient *lsp, Hist *hist, const char *model,
                         const char *url, const char *filename,
                         const char *initial_code, int max_iters,
                         char **final_code, LspDiagVec *final_diags)
{
    char *code = strdup(initial_code);
    char cwd[4096];
    int iter;

    getcwd(cwd, sizeof(cwd));

    *final_code = NULL;
    *final_diags = (LspDiagVec){0};

    /* no language server available (start/restart failed): never crash on a
     * NULL client. Save the code unverified and let the caller report it. */
    if (!lsp) {
        *final_code = code;
        write_file(filename, code);
        return 2;
    }

    for (iter = 0; max_iters <= 0 || iter < max_iters; iter++) {
        LspDiagVec diags = {0};
        char checkpath[131072];
        char uri[262144];
        /* a unique temp file + uri per check so each didOpen is a fresh
         * document (and the compiler always runs) */
        snprintf(checkpath, sizeof(checkpath),
                 "%s/.brackets-check-%d-%lu.l1com", cwd, (int)getpid(),
                 ++g_check_seq);
        snprintf(uri, sizeof(uri), "file://%s", checkpath);
        write_file(checkpath, code);
        printf("\n[checking with l1vm-lsp...]\n");
        fflush(stdout);
        {
            char *diskcode = read_file_part(checkpath, -1, 1);
            int ck = lsp_check(lsp, uri, checkpath,
                               diskcode ? diskcode : code, &diags);
            free(diskcode);
            unlink(checkpath);
            if (ck != 0) {
                fprintf(stderr, "LSP check failed (is l1vm-lsp built/installed?)\n");
                lsp_diagvec_free(&diags);
                *final_code = code;
                return -1;
            }
        }

        {
            size_t i;
            int errors = 0;
            for (i = 0; i < diags.len; i++)
                if (diags.items[i].severity == 1)
                    errors++;
            if (errors == 0) {
                char *d = format_diags(&diags, "no errors, diagnostics");
                printf("%s", d);
                free(d);
                lsp_diagvec_free(&diags);
                write_file(filename, code);
                *final_code = code;
                return 0;
            }
        }

        /* report errors and ask the model to correct */
        {
            char *dtext = format_diags(&diags, "LSP errors");
            if (max_iters > 0)
                printf("Found errors; asking the model to fix (iteration %d/%d)...\n",
                       iter + 1, max_iters);
            else
                printf("Found errors; asking the model to fix (iteration %d, "
                       "unlimited loop; Ctrl+C to stop)...\n", iter + 1);
            printf("%s", dtext);
            SB fix;
            sb_init(&fix);
            sb_printf(&fix,
                "The Brackets program above has the following errors. "
                "Please output the COMPLETE corrected .l1com program as a "
                "single fenced block ```l1com ... ```, fixing every error. "
                "Do not describe anything; just output the corrected code.\n\n"
                "%s", dtext);
            hist_push(hist, "user", sb_cstr(&fix));
            sb_free(&fix);
            free(dtext);

            {
                char *reply = NULL;
                    long tin = -1, tout = -1;
                    printf("[asking model to fix...]\n");
                    fflush(stdout);
                    if (llm_text(hist, model, url, NULL, &reply,
                                 &tin, &tout) == 0 && reply) {
                        print_token_usage(tin, tout);
                    /* strip the code from the reply */
                    char fixedname[256];
                    char *newcode = extract_code(reply, fixedname, sizeof(fixedname));
                    if (newcode) {
                        /* keep file name if unchanged or not provided */
                        free(code);
                        code = newcode;
                    } else {
                        /* model didn't emit code; keep last code */
                    }
                    free(reply);
                }
            }
        }
        lsp_diagvec_free(&diags);
        if (max_iters > 0) {
            if (!ask_continue()) {
                printf("\nAuto-fix aborted by user; %s was NOT saved.\n", filename);
                *final_code = code;   /* caller still owns the last code */
                return -2;
            }
        } else if (g_intr_request) {
            /* /loop mode: a Ctrl+C stops the loop without asking */
            printf("\nAuto-fix aborted by user; %s was NOT saved.\n", filename);
            *final_code = code;   /* caller still owns the last code */
            return -2;
        }
    }

    /* give up after max iterations: report last state */
    {
        LspDiagVec diags = {0};
        char checkpath[131072];
        char uri[262144];
        int ck;
        size_t i;
        int nerr = 0;
        snprintf(checkpath, sizeof(checkpath),
                 "%s/.brackets-check-%d-%lu.l1com", cwd, (int)getpid(),
                 ++g_check_seq);
        snprintf(uri, sizeof(uri), "file://%s", checkpath);
        write_file(checkpath, code);
        {
            char *diskcode = read_file_part(checkpath, -1, 1);
            ck = lsp_check(lsp, uri, checkpath, diskcode ? diskcode : code,
                           &diags);
            free(diskcode);
        }
        unlink(checkpath);
        write_file(filename, code);
        *final_code = code;   /* caller owns */
        if (ck != 0) {
            *final_diags = diags;
            fprintf(stderr, "\n[WARNING] LSP check failed on the final "
                            "attempt; %s was saved without verification.\n",
                    filename);
            return 2;
        }
        *final_diags = diags;
        for (i = 0; i < diags.len; i++)
            if (diags.items[i].severity == 1)
                nerr++;
        return nerr > 0 ? 1 : 0;
    }
}

/* ================== code diff display ================== */

/* Count the lines (newlines + optional final unterminated line) in `s`. */
static long line_count(const char *s)
{
    long n = 0;
    if (!s || !*s)
        return 0;
    for (; *s; s++)
        if (*s == '\n')
            n++;
    if (s[-1] != '\n')
        n++;
    return n;
}

/* Number width needed to right-align `n` (>= 1). */
static int num_width(long n)
{
    int w = 1;
    while (n >= 10) {
        n /= 10;
        w++;
    }
    return w;
}

/* Print a NUL-terminated string as a numbered listing, one line at a time
 * with a "+" added marker, like opencode shows a newly created file. */
static void print_added_numbered(const char *content, int width)
{
    long ln = 0;
    const char *q = content ? content : "";
    while (*q) {
        const char *nl = strchr(q, '\n');
        size_t n = nl ? (size_t)(nl - q) : strlen(q);
        ln++;
        printf("%*ld +%.*s\n", width, ln, (int)n, q);
        if (!nl)
            break;
        q = nl + 1;
    }
}

/* Parse a unified diff hunk header "  @@ -old[,cnt] +new[,cnt] @@" and set
 * the old/new start line numbers. Returns 0 on success, -1 otherwise. */
static int parse_hunk_header(const char *s, long *o_start, long *n_start)
{
    long o = 0, n = 0;
    const char *p = s;
    while (*p == ' ' || *p == '\t')
        p++;
    if (strncmp(p, "@@ -", 4) != 0)
        return -1;
    p += 4;
    if (*p < '0' || *p > '9')
        return -1;
    o = 0;
    while (*p >= '0' && *p <= '9') o = o * 10 + (*p++ - '0');
    if (*p == ',') {
        p++;
        while (*p >= '0' && *p <= '9') p++;
    }
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p != '+')
        return -1;
    p++;
    if (*p < '0' || *p > '9')
        return -1;
    n = 0;
    while (*p >= '0' && *p <= '9') n = n * 10 + (*p++ - '0');
    *o_start = o;
    *n_start = n;
    return 0;
}

/* Show a generated/changed file in the shell with line numbers and "+"/"-"
 * marks, like opencode does. When old_content is NULL the file is new/created
 * and every line is shown as added ("+"). Otherwise a unified diff (removed
 * lines "-", added lines "+") is produced with diff(1). Never crashes: on any
 * temp-file or diff(1) failure it falls back to printing the new content as
 * added. */
static void print_file_diff(const char *path, const char *old_content,
                            const char *new_content)
{
    char new_f[64] = "/tmp/brackets-new-XXXXXX";
    int fd;
    FILE *f;
    char cmd[4096];
    char *out = NULL;
    char *p;
    int width;

    if (!new_content || !*new_content)
        return;

    /* number width so all line numbers right-align */
    width = num_width(line_count(new_content));
    if (old_content && line_count(old_content) > width)
        width = num_width(line_count(old_content));
    if (width < 5)
        width = 5;

    /* new file: nothing to compare against, show everything as added */
    if (!old_content) {
        printf("--- %s\n", path);
        printf("+++ %s\n", path);
        print_added_numbered(new_content, width);
        return;
    }

    fd = mkstemp(new_f);
    if (fd < 0)
        goto fallback;
    f = fdopen(fd, "w");
    if (!f) {
        close(fd);
        unlink(new_f);
        goto fallback;
    }
    fputs(new_content, f);
    fclose(f);

    {
        char old_f[64] = "/tmp/brackets-old-XXXXXX";
        fd = mkstemp(old_f);
        if (fd < 0) {
            unlink(new_f);
            goto fallback;
        }
        f = fdopen(fd, "w");
        if (!f) {
            close(fd);
            unlink(old_f);
            unlink(new_f);
            goto fallback;
        }
        fputs(old_content, f);
        fclose(f);

        snprintf(cmd, sizeof(cmd), "diff -u \"%s\" \"%s\"", old_f, new_f);
        run_cmd_capture(cmd, &out);

        printf("--- %s\n", path);
        printf("+++ %s\n", path);

        if (out && *out) {
            /* walk the diff body line by line and add line numbers:
             * skip the two temp-file header lines first, then track the
             * old/new line numbers via the @@ hunk headers */
            long oln = 0, nln = 0;
            p = out;
            {
                int skipped = 0;
                while (skipped < 2 && *p) {
                    char *nl = strchr(p, '\n');
                    if (!nl)
                        break;
                    p = nl + 1;
                    skipped++;
                }
            }
            while (*p) {
                char *nl = strchr(p, '\n');
                size_t len = nl ? (size_t)(nl - p) : strlen(p);
                if (parse_hunk_header(p, &oln, &nln) == 0 ||
                    strncmp(p, "@@ ", 3) == 0) {
                    printf("%*s %.*s\n", width, "", (int)len, p);
                } else if (p[0] == '-') {
                    printf("%*ld -%.*s\n", width, oln, (int)(len - 1), p + 1);
                    oln++;
                } else if (p[0] == '+') {
                    printf("%*ld +%.*s\n", width, nln, (int)(len - 1), p + 1);
                    nln++;
                } else if (p[0] == '\\') {
                    /* "no newline at end of file" marker */
                    printf("%*s %.*s\n", width, "", (int)len, p);
                } else {
                    /* context line: keep the leading space of the diff */
                    printf("%*ld %.*s\n", width, nln, (int)len, p);
                    oln++;
                    nln++;
                }
                if (!nl)
                    break;
                p = nl + 1;
            }
            free(out);
        } else {
            /* diff(1) missing or failed: show the new content as added */
            free(out);
            print_added_numbered(new_content, width);
        }
        unlink(new_f);
        unlink(old_f);
        return;
    }

fallback:
    /* could not diff: show the new content as added lines */
    if (strlen(new_f) > 0 && access(new_f, R_OK) == 0)
        unlink(new_f);
    {
        int w = num_width(line_count(new_content));
        if (w < 5)
            w = 5;
        printf("--- %s\n", path);
        printf("+++ %s\n", path);
        print_added_numbered(new_content, w);
    }
}

/* ================== agentic tool use ================== */

#define MAX_TOOL_ITERS 8

static int tool_calls_emitted(const JVal *r)
{
    const JVal *choices, *msg, *tc;
    if (!r)
        return 0;
    choices = j_get(r, "choices");
    if (!choices || j_len(choices) == 0)
        return 0;
    msg = j_at(choices, 0);
    if (!msg)
        return 0;
    msg = j_get(msg, "message");
    if (!msg)
        return 0;
    tc = j_get(msg, "tool_calls");
    return (tc && j_is(tc, J_ARR) && j_len(tc) > 0) ? 1 : 0;
}

/*
 * Process a list of tool_calls from the model: run each tool, keep the
 * assistant's function-call message in the conversation, and append one
 * "tool" result message per call. Returns 0 if at least one call was handled.
 */
static int handle_tool_calls(Hist *hist, const char *model, const char *url,
                             const JVal *r, LspClient *lsp,
                             char **last_code, char *last_name,
                             size_t last_name_sz)
{
    const JVal *choices = j_get(r, "choices");
    const JVal *msg = (choices && j_len(choices) > 0)
                          ? j_at(choices, 0) : NULL;
    const JVal *msgv = msg ? j_get(msg, "message") : NULL;
    const JVal *tc = msgv ? j_get(msgv, "tool_calls") : NULL;
    const char *c = msgv ? j_str(j_get(msgv, "content")) : NULL;
    size_t i;
    int count = 0;

    (void)model;
    (void)url;
    if (!tc || !j_is(tc, J_ARR))
        return 0;

    /* keep the assistant's natural-language text (often explains the plan) */
    if (c && *c) {
        printf("%s\n", c);
        hist_push(hist, "assistant", c);
    }

    for (i = 0; i < j_len(tc); i++) {
        const JVal *call = j_at(tc, i);
        const JVal *fn, *fnv, *argv;
        const char *name, *args;
        char *result = NULL;
        char *lspnote = NULL;

        fn = call ? j_get(call, "function") : NULL;
        fnv = fn ? j_get(fn, "name") : NULL;
        argv = fn ? j_get(fn, "arguments") : NULL;
        name = fnv ? j_str(fnv) : NULL;
        args = argv ? j_str(argv) : NULL;

        if (name) {
            int is_fwrite = (strcmp(name, "write_file") == 0 ||
                             strcmp(name, "edit_file") == 0);
            char *old_content = NULL;
            char *path = NULL;   /* expanded path of the file being written */

            /* For write_file / edit_file, capture the old file content before
             * the tool runs so the change can be shown as a diff. */
            if (is_fwrite && args) {
                JVal *aj = j_parse(args);
                const JVal *pv = aj ? j_get(aj, "path") : NULL;
                const char *pp = pv ? j_str(pv) : NULL;
                char *exp = pp ? tool_expand_path(pp) : NULL;
                if (exp) {
                    path = exp;
                    old_content = read_file(path);
                }
                if (aj)
                    j_free(aj);
            }

            tool_run(name, args, &result);
            if (!result)
                result = strdup("tool execution produced no result");

            /* after a successful write/edit, show the code change with
             * "+"/"-" marks in the shell, like opencode does */
            if (is_fwrite && path && result &&
                strncmp(result, "ok:", 3) == 0) {
                char *new_content = read_file(path);
                if (new_content) {
                    print_file_diff(path, old_content, new_content);
                    free(new_content);
                }
            }
            free(old_content);

            /* verify a freshly written .l1com file with l1vm-lsp and keep it
             * as the "last code" so that /lsp and /code work on it */
            if (lsp && path && strstr(path, ".l1com") &&
                result && strncmp(result, "ok:", 3) == 0) {
                char *fc = read_file(path);
                if (fc) {
                    free(*last_code);
                    *last_code = fc;
                    snprintf(last_name, last_name_sz, "%s", path);
                }
                lspnote = lsp_summary_for_write(lsp, path);
            }
            free(path);
        }
        {
            SB t;
            sb_init(&t);
            if (name) {
                sb_printf(&t, "Tool result for %s(%s):\n%s",
                          name, args ? args : "{}",
                          result ? result : "(no result)");
            } else {
                sb_add(&t, "Tool call had no function name");
            }
            if (lspnote) {
                sb_printf(&t, "\n%s", lspnote);
                printf("%s\n", lspnote);
                fflush(stdout);
                free(lspnote);
            }
            sb_add(&t, "\n");
            /* Inject as a user turn: portable across servers (no strict
             * ordering of an OpenAI "tool" role or tool_call_id needed). */
            hist_push(hist, "user", sb_cstr(&t));
            sb_free(&t);
        }
        free(result);
        count++;
    }
    return count > 0 ? 0 : -1;
}

/*
 * Extract any Brackets program from a final text reply, save it, check it
 * with the LSP and auto-correct it. Preserves the newest code + its name.
 */
static void process_code_reply(const char *text, LspClient *lsp, Hist *hist,
                               const char *model, const char *url,
                               char **last_code, char *last_name,
                               size_t last_name_sz)
{
    char fname[256] = "";
    char *code = extract_code(text, fname, sizeof(fname));
    if (!code)
        return;
    if (fname[0] == '\0')
        snprintf(fname, sizeof(fname), "program-%lu.l1com",
                 (unsigned long)hist->len);
    else if (strstr(fname, ".l1com") == NULL) {
        size_t fl = strlen(fname);
        if (fl + 7 < sizeof(fname))
            snprintf(fname + fl, sizeof(fname) - fl, ".l1com");
    }
    free(*last_code);
    *last_code = NULL;
    if (last_name[0])
        last_name[0] = '\0';

    printf("\n--- code artifact detected (%s) ---\n", fname);
    print_file_diff(fname, NULL, code);
    {
        char *final = NULL;
        LspDiagVec diags = {0};
        int rc = code_workflow(lsp, hist, model, url, fname, code,
                               MAX_FIX_ITERS, &final, &diags);
        if (final) {
            *last_code = strdup(final);
            free(final);
            snprintf(last_name, last_name_sz, "%s", fname);
        }
        if (rc == 0) {
            printf("saved %s (LSP check OK)\n", fname);
        } else if (rc == 1) {
            size_t i;
            int nerr = 0;
            for (i = 0; i < diags.len; i++)
                if (diags.items[i].severity == 1)
                    nerr++;
            fprintf(stderr,
                    "\n========================================================\n"
                    "WARNING: %s was SAVED but still contains %d error(s) "
                    "after %d fix attempts.\n",
                    fname, nerr, MAX_FIX_ITERS);
            if (diags.len > 0) {
                char *d = format_diags(&diags, "remaining errors");
                fprintf(stderr, "%s", d);
                free(d);
            }
            fprintf(stderr,
                    "========================================================\n");
        } else if (rc == 2) {
            fprintf(stderr,
                    "\n[WARNING] %s was saved but could not be verified by "
                    "the LSP; build it with /build to check for errors.\n",
                    fname);
        }
        lsp_diagvec_free(&diags);
    }
    free(code);
}

/* ================== slash commands ================== */

static int run_build(const char *progname)
{
    char cmd[131072];
    char *out = NULL;
    char *home = getenv("HOME");
    snprintf(cmd, sizeof(cmd), "%s/l1vm/bin/l1vm-build.sh %s >build.txt 2>&1",
             home ? home : "", progname);
    printf("[building %s...]\n", progname);
    fflush(stdout);
    run_cmd_capture(cmd, &out);
    printf("\n----- build output -----\n");
    if (out) {
        printf("%s", out);
        free(out);
    }
    printf("------------------------\n");
    {
        char *bt = read_file("build.txt");
        if (bt) {
            if (strstr(bt, "error") || strstr(bt, "Error") ||
                strstr(bt, "failed") || strstr(bt, "failed!")) {
                free(bt);
                return -1;
            }
            free(bt);
        }
    }
    return 0;
}

static int run_program(const char *progname)
{
    char cmd[131072];
    char *home = getenv("HOME");
    snprintf(cmd, sizeof(cmd), "%s/l1vm/bin/l1vm %s; echo; echo \"[exit:] $?\"",
             home ? home : "", progname);
    printf("[running %s...]\n", progname);
    fflush(stdout);
    system(cmd);
    return 0;
}

static void print_help(void)
{
    printf(
        "\n"
        "brackets-llm - opencode-like Brackets (L1VM) environment\n"
        "\n"
        "  Just type a request. The model can use tools (read_file,\n"
        "  write_file, edit_file, list_files) to create and modify files on\n"
        "  its own, like opencode. Brackets code in the answer is checked\n"
        "  with l1vm-lsp and auto-corrected before it is saved to disk.\n"
        "\n"
        "Commands:\n"
        "  /read <file> [limit=N, offset=M]\n"
        "                 show file contents (optional line range)\n"
        "  /write <file> [limit=N, offset=M]\n"
        "                 write the last generated code (or a line range) to <file>\n"
        "  /list            list files in the working directory\n"
        "  /lsp             re-run the LSP on the last code; any errors are\n"
        "                  sent to the model to fix (auto-correct loop)\n"
        "  /loop            check + auto-fix the last code repeatedly until\n"
        "                  all errors are fixed (unlimited; Ctrl+C stops)\n"
        "  /build <name>    build <name.l1com> with l1vm-build.sh\n"
        "  /run <name>      run <name> with l1vm\n"
        "  /code            re-extract/re-check the last code block\n"
        "  /save <name> [limit=N, offset=M]\n"
        "                 save the last code block (or a line range) to <name>.l1com\n"
        "  /compact         summarize + collapse old messages in the context\n"
        "  /new             reset the conversation\n"
        "  /help            this help\n"
        "  /quit            exit\n"
        "\n"
        "When the model (or /read, /write, /save) wants to access a path\n"
        "OUTSIDE the current directory, you are asked for permission first.\n"
        "\n");
}

/* ================== main ================== */

/* ================== interrupt (Ctrl+C) support ================== */

/* Ctrl+C: request a graceful abort of whatever is running (model call, LSP
 * check, tool call). Blocking calls get EINTR, notice the flag and unwind
 * normally (all heap state stays consistent); the REPL then prints
 * "(interrupted)", restarts the LSP and continues. We never siglongjmp from
 * a signal handler: jumping out of a malloc/realloc would corrupt glibc's
 * heap ("double free or corruption"). */
static void sigint_handler(int sig)
{
    (void)sig;
    g_intr_request = 1;
}

/* Restart the LSP after an interrupt: an aborted LSP exchange can leave the
 * server out of sync, so a fresh child process is the safe state. */
static void lsp_restart(LspClient **lsp, const char *lsp_path,
                        const char *l1com_path, const char *include_dir)
{
    if (*lsp) {
        lsp_stop(*lsp);
        *lsp = NULL;
    }
    *lsp = lsp_start(lsp_path, 1, l1com_path, include_dir);
    if (!*lsp)
        fprintf(stderr, "warning: could not restart l1vm-lsp (%s); code "
                        "checking disabled.\n", lsp_path);
}

/* Remove leftover .brackets-check-*.l1com temp files from an interrupted
 * LSP check. */
static void cleanup_check_files(void)
{
    DIR *d = opendir(".");
    struct dirent *e;
    if (!d)
        return;
    while ((e = readdir(d)) != NULL) {
        if (strncmp(e->d_name, ".brackets-check-", 16) == 0 &&
            strstr(e->d_name, ".l1com") != NULL)
            unlink(e->d_name);
    }
    closedir(d);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    const char *volatile url = cfg_server_url();
    const char *volatile model = cfg_model();
    const char *volatile lsp_path = cfg_lsp_path();
    const char *volatile l1com_path = cfg_l1com_path();
    const char *volatile sysprompt_path = cfg_system_prompt_path();
    char *sysprompt = read_file(sysprompt_path);
    char *last_code = NULL;
    char last_name[256] = "";
    Hist hist = {0};
    LspClient *lsp;
    char line[65536];
    int running = 1;

    if (!sysprompt) {
        fprintf(stderr, "brackets-llm: cannot read system prompt: %s\n",
                sysprompt_path);
        return 1;
    }
    hist_push(&hist, "system", sysprompt);
    printf("brackets-llm v%s\n", BRACKETS_LLM_VERSION);
    printf("  server : %s\n", url);
    printf("  model  : %s\n", model);
    printf("  lsp    : %s\n", lsp_path);
    printf("  prompt : %s (%zu bytes)\n\n", sysprompt_path, strlen(sysprompt));

    lsp = lsp_start(lsp_path, 1, l1com_path, cfg_include_dir());
    if (!lsp) {
        fprintf(stderr, "warning: could not start l1vm-lsp (%s); code "
                "checking disabled.\n"
                "(is it built/installed? point L1VM_LSP at it, or rebuild "
                "from ~/develop/l1vm/l1vm-lsp)\n",
                lsp_path);
        lsp = NULL;
    }

    print_help();

    /* Ctrl+C never kills the app: it aborts whatever is running and jumps
     * back to the "You> " prompt. Installed with no SA_RESTART so blocking
     * calls are interruptible. */
    {
        struct sigaction sa;
        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = sigint_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, NULL);
    }

    /* A Ctrl+C is delivered to the whole foreground process group, so a
     * child (l1vm-lsp) may be killed by it. Writing the next LSP message to
     * that child's pipe would then raise SIGPIPE and silently terminate the
     * program (the "kicked back to your shell" symptom). Writes to pipes and
     * sockets must instead fail with EPIPE so the error paths underneath
     * (lsp_restart etc.) can recover. */
    signal(SIGPIPE, SIG_IGN);

    while (running) {
        if (g_intr_request) {
            /* an operation was aborted by Ctrl+C: the flag was set, the
             * blocking calls returned early and unwound normally */
            g_intr_request = 0;
            printf("\n(interrupted)\n");
            lsp_restart(&lsp, lsp_path, l1com_path, cfg_include_dir());
            cleanup_check_files();
            continue;   /* back to the "You> " prompt */
        }
        {
            char youprom[128];
            char *ln;
            int tty = isatty(STDIN_FILENO) && isatty(STDOUT_FILENO);
            if (tty)
                /* \001/\002 wrap the invisible escape so readline counts
                 * the prompt width correctly; the trailing color (no reset)
                 * also colors everything the user types */
                snprintf(youprom, sizeof(youprom),
                         "\n\001" COLOR_USER "\002You> ");
            else
                snprintf(youprom, sizeof(youprom), "\nYou> ");
            g_intr_request = 0;   /* a stray signal between operations must
                                   * not abort the next model call */
            ln = input_line(youprom);
            if (tty)
                printf(COLOR_RESET);   /* back to plain for Assistant> */
            if (!ln) {
                /* readline returns NULL only on EOF (Ctrl+D). A Ctrl+C at
                 * the prompt is consumed by readline itself, and during any
                 * blocking I/O our handler sets g_intr_request so the loop
                 * above reprompts. Never exit on a signal: even if a
                 * readline/stdio build fails to report the interrupt through
                 * g_intr_request, an EINTR-driven NULL is not end of input
                 * (stdin is not at EOF), so keep the program running. */
                if (g_intr_request || (errno == EINTR && !feof(stdin))) {
                    errno = 0;   /* a stale EINTR must not trap a later
                                  * genuine Ctrl+D (EOF) into re-prompting */
                    clearerr(stdin);
                    continue;   /* Ctrl+C during the prompt read: the top of
                                 * the loop sees g_intr_request and prints
                                 * "(interrupted)" if a signal was involved */
                }
                break;          /* EOF (Ctrl+D or closed pipe) */
            }
            /* A Ctrl+C while the prompt was being edited is consumed by the
             * line editor itself; readline re-raises it into our handler, so
             * g_intr_request is set even though nothing was running. Clear it
             * here, otherwise the very next message the user submits would be
             * silently discarded as "(interrupted)". */
            g_intr_request = 0;
            snprintf(line, sizeof(line), "%s", ln);
            free(ln);
        }

        if (line[0] == '\0')
            continue;

        /* slash commands */
        if (line[0] == '/') {
            if (strncmp(line, "/quit", 5) == 0 ||
                strncmp(line, "/exit", 5) == 0) {
                running = 0;
                break;
            } else if (strncmp(line, "/help", 5) == 0) {
                print_help();
                continue;
            } else if (strncmp(line, "/new", 4) == 0) {
                hist_free(&hist);
                hist_push(&hist, "system", sysprompt);
                free(last_code);
                last_code = NULL;
                last_name[0] = '\0';
                printf("new session started.\n");
                continue;
            } else if (strncmp(line, "/list", 5) == 0) {
                DIR *d = opendir(".");
                struct dirent *e;
                if (d) {
                    while ((e = readdir(d)) != NULL)
                        printf("  %s\n", e->d_name);
                    closedir(d);
                }
                continue;
            } else if (strncmp(line, "/read ", 6) == 0) {
                {
                    char path[1024];
                    long limit, offset;
                    parse_read_spec(line + 6, path, sizeof(path), &limit, &offset);
                    if (!tool_path_inside_cwd(path)) {
                        if (!tool_ask_permission(path)) {
                            printf("denied: %s is outside the current "
                                   "directory.\n", path);
                            continue;
                        }
                    }
                    {
                        char *f = read_file_part(path, limit, offset);
                        if (f) {
                            if (limit >= 0)
                                printf("----- %s (lines %ld-%ld) -----\n%s\n",
                                       path, offset, offset + limit - 1, f);
                            else
                                printf("----- %s -----\n%s\n", path, f);
                            free(f);
                        } else {
                            printf("cannot read: %s\n", path);
                        }
                    }
                }
                continue;
            } else if (strncmp(line, "/write ", 7) == 0) {
                if (!last_code) {
                    printf("no code to write yet.\n");
                    continue;
                }
                {
                    char path[1024];
                    long limit, offset;
                    char *part = NULL;
                    parse_read_spec(line + 6, path, sizeof(path),
                                    &limit, &offset);
                    if (!tool_path_inside_cwd(path)) {
                        if (!tool_ask_permission(path)) {
                            printf("denied: %s is outside the current "
                                   "directory.\n", path);
                            continue;
                        }
                    }
                    if (limit >= 0 || offset > 1)
                        part = slice_lines(last_code, limit, offset);
                    if (write_file(path, part ? part : last_code) == 0) {
                        if (part)
                            printf("wrote %s (lines %ld-%ld)\n",
                                   path, offset, offset + limit - 1);
                        else
                            printf("wrote %s\n", path);
                        if (lsp && strstr(path, ".l1com")) {
                            char *note = lsp_summary_for_write(lsp, path);
                            if (note) {
                                if (strstr(note, "LSP check: OK") == NULL)
                                    printf("WARNING: %s has syntax errors (saved anyway):\n%s\n",
                                           path, note);
                                else
                                    printf("%s\n", note);
                                free(note);
                            }
                        }
                    } else {
                        printf("write failed.\n");
                    }
                    free(part);
                }
                continue;
            } else if (strncmp(line, "/save ", 6) == 0) {
                if (!last_code) {
                    printf("no code to save yet.\n");
                    continue;
                }
                {
                    char fname[131072];
                    char path[1024];
                    long limit, offset;
                    char *part = NULL;
                    parse_read_spec(line + 6, path, sizeof(path),
                                    &limit, &offset);
                    snprintf(fname, sizeof(fname), "%s.l1com", path);
                    if (!tool_path_inside_cwd(path)) {
                        if (!tool_ask_permission(path)) {
                            printf("denied: %s is outside the current "
                                   "directory.\n", path);
                            continue;
                        }
                    }
                    if (limit >= 0 || offset > 1)
                        part = slice_lines(last_code, limit, offset);
                    if (write_file(fname, part ? part : last_code) == 0) {
                        if (part)
                            printf("saved %s (lines %ld-%ld)\n",
                                   fname, offset, offset + limit - 1);
                        else
                            printf("saved %s\n", fname);
                        if (lsp) {
                            char *note = lsp_summary_for_write(lsp, fname);
                            if (note) {
                                if (strstr(note, "LSP check: OK") == NULL)
                                    printf("WARNING: %s has syntax errors (saved anyway):\n%s\n",
                                           fname, note);
                                else
                                    printf("%s\n", note);
                                free(note);
                            }
                        }
                    } else {
                        printf("save failed.\n");
                    }
                    free(part);
                }
                continue;
            } else if (strcmp(line, "/lsp") == 0 || strncmp(line, "/lsp ", 5) == 0) {
                if (!lsp) {
                    printf("l1vm-lsp is not available (code checking "
                           "disabled).\n");
                    continue;
                }
                if (!last_code) {
                    printf("no code to check yet.\n");
                    continue;
                }
                {
                    char fname[256];
                    char *final = NULL;
                    LspDiagVec diags = {0};
                    int rc;
                    if (last_name[0])
                        snprintf(fname, sizeof(fname), "%s", last_name);
                    else
                        snprintf(fname, sizeof(fname), "program-%lu.l1com",
                                 (unsigned long)hist.len);
                    rc = code_workflow(lsp, &hist, model, url, fname,
                                       last_code, MAX_FIX_ITERS, &final, &diags);
                    if (final) {
                        free(last_code);
                        last_code = strdup(final);
                        free(final);
                        snprintf(last_name, sizeof(last_name), "%s", fname);
                    }
                    if (rc == 0) {
                        printf("saved %s (LSP check OK)\n", fname);
                    } else if (rc == 1) {
                        size_t i;
                        int nerr = 0;
                        for (i = 0; i < diags.len; i++)
                            if (diags.items[i].severity == 1)
                                nerr++;
                        fprintf(stderr,
                                "WARNING: %s still contains %d error(s) after "
                                "%d fix attempts.\n",
                                fname, nerr, MAX_FIX_ITERS);
                        if (diags.len > 0) {
                            char *d = format_diags(&diags, "remaining errors");
                            fprintf(stderr, "%s", d);
                            free(d);
                        }
                    } else if (rc == 2) {
                        fprintf(stderr,
                                "[WARNING] %s could not be verified by the "
                                "LSP; build it with /build to check for "
                                "errors.\n",
                                fname);
                    }
                    lsp_diagvec_free(&diags);
                }
                continue;
            } else if (strcmp(line, "/loop") == 0 || strncmp(line, "/loop ", 6) == 0) {
                /* same as /lsp but unlimited: check + auto-fix again and
                 * again until the LSP reports no errors (or Ctrl+C aborts) */
                if (!lsp) {
                    printf("l1vm-lsp is not available (code checking "
                           "disabled).\n");
                    continue;
                }
                if (!last_code) {
                    printf("no code to check yet.\n");
                    continue;
                }
                {
                    char fname[256];
                    char *final = NULL;
                    LspDiagVec diags = {0};
                    int rc;
                    if (last_name[0])
                        snprintf(fname, sizeof(fname), "%s", last_name);
                    else
                        snprintf(fname, sizeof(fname), "program-%lu.l1com",
                                 (unsigned long)hist.len);
                    rc = code_workflow(lsp, &hist, model, url, fname,
                                       last_code, -1, &final, &diags);
                    if (final) {
                        free(last_code);
                        last_code = strdup(final);
                        free(final);
                        snprintf(last_name, sizeof(last_name), "%s", fname);
                    }
                    if (rc == 0) {
                        printf("saved %s (LSP check OK)\n", fname);
                    } else if (rc == -1) {
                        fprintf(stderr, "/loop aborted (LSP check failed).\n");
                    }
                    lsp_diagvec_free(&diags);
                }
                continue;
            } else if (strncmp(line, "/build ", 7) == 0) {
                run_build(line + 7);
                continue;
            } else if (strncmp(line, "/run ", 5) == 0) {
                run_program(line + 5);
                continue;
            } else if (strncmp(line, "/code", 5) == 0) {
                if (last_code) {
                    printf("----- last code -----\n%s\n", last_code);
                } else {
                    printf("no code yet.\n");
                }
                continue;
            } else if (strncmp(line, "/compact", 8) == 0) {
                compact_history(&hist, model, url);
                continue;
            } else {
                printf("unknown command: %s (try /help)\n", line);
                continue;
            }
        }

        /* ordinary chat message */
        hist_push(&hist, "user", line);

        printf("Assistant> ");
        fflush(stdout);
        {
            char *tools = tools_definitions_json();
            int iter;

            for (iter = 0; iter <= MAX_TOOL_ITERS; iter++) {
                char *reply = NULL;
                long tin = -1, tout = -1;
                int rc;
                if (g_intr_request)
                    break;   /* Ctrl+C during a tool call */
                rc = llm_call(&hist, model, url, tools, &reply,
                              &tin, &tout);
                print_token_usage(tin, tout);
                if (g_intr_request) {
                    free(reply);
                    break;   /* Ctrl+C during the model call */
                }
                if (rc != 0 || !reply) {
                    printf("<error: could not reach llama-server at %s%s>\n",
                           url, " - is it running?");
                    break;
                }
                {
                    JVal *r = j_parse(reply);
                    if (r && tool_calls_emitted(r)) {
                        if (iter >= MAX_TOOL_ITERS) {
                            printf("(tool call limit reached, giving up)\n");
                            j_free(r);
                            free(reply);
                            break;
                        }
                        printf("[tool] ");
                        if (handle_tool_calls(&hist, model, url, r, lsp,
                                              &last_code, last_name,
                                              sizeof(last_name)) == 0) {
                            j_free(r);
                            free(reply);
                            continue;   /* loop: run the model again */
                        }
                        j_free(r);
                        free(reply);
                        break;
                    }
                    {
                        const JVal *c;
                        const char *text;
                        c = r ? j_get(j_at(j_get(r, "choices"), 0), "message") : NULL;
                        text = c ? j_str(j_get(c, "content")) : NULL;
                        if (text && *text) {
                            printf("%s\n", text);
                            hist_push(&hist, "assistant", text);
                            process_code_reply(text, lsp, &hist, model, url,
                                               &last_code, last_name,
                                               sizeof(last_name));
                        }
                        j_free(r);
                    }
                    free(reply);
                    break;
                }
            }
            free(tools);
        }
    }

    hist_free(&hist);
    free(last_code);
    free(sysprompt);
    input_shutdown();
    if (lsp)
        lsp_stop(lsp);
    return 0;
}
