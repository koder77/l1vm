/*
 * This file tools.c is part of L1vm.
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
 * brackets-llm - agent tools: read_file / write_file / edit_file / list_files
 *
 * Executes tool calls requested by the LLM, opencode-style. File writes are
 * allowed automatically; set BRACKETS_LLM_CONFIRM=1 to be asked for
 * confirmation on every write (in interactive mode).
 */

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "tools.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "inputline.h"
#include "json.h"
#include "sb.h"

#define TOOL_READ_CAP 128000L
#define TOOL_EDIT_CAP (4 * 1024 * 1024L)

char *tool_expand_path(const char *p)
{
    if (p && p[0] == '~') {
        const char *h = getenv("HOME");
        if (h) {
            SB b;
            char *r;
            sb_init(&b);
            sb_add(&b, h);
            sb_add(&b, p + 1);
            r = strdup(sb_cstr(&b));
            sb_free(&b);
            return r;
        }
    }
    return strdup(p ? p : "");
}

/* Canonical absolute form of `path`: expands ~, resolves "." and ".." and
 * prepends the working directory for relative paths. Symlinks are resolved
 * with realpath() whenever the resulting path exists. Returns a heap-allocated
 * string (caller frees) or NULL on allocation failure. */
char *tool_canon_path(const char *path)
{
    char *exp = tool_expand_path(path);
    char abs[4096];
    char **stack = NULL;
    size_t n = 0, cap = 0;
    size_t i;
    char *out = NULL;
    SB b;
    const char *p;

    if (!exp)
        return NULL;
    if (exp[0] == '/') {
        snprintf(abs, sizeof(abs), "%s", exp);
    } else {
        char cwd[4096];
        if (!getcwd(cwd, sizeof(cwd)))
            strcpy(cwd, ".");
        snprintf(abs, sizeof(abs), "%s/%s", cwd, exp);
    }
    free(exp);

    p = abs;
    while (*p) {
        const char *e;
        size_t len;
        while (*p == '/')
            p++;
        if (!*p)
            break;
        e = strchr(p, '/');
        len = e ? (size_t)(e - p) : strlen(p);
        if (len == 1 && p[0] == '.') {
            /* current dir: skip */
        } else if (len == 2 && p[0] == '.' && p[1] == '.') {
            if (n > 0)
                n--;   /* pop one component (never above root) */
        } else {
            if (n == cap) {
                cap = cap ? cap * 2 : 8;
                stack = realloc(stack, cap * sizeof(char *));
                if (!stack)
                    goto done;
            }
            stack[n] = strndup(p, len);
            if (!stack[n]) {
                free(stack[n]);
                goto done;
            }
            n++;
        }
        p += len;
    }

    sb_init(&b);
    if (n == 0) {
        sb_add(&b, "/");
    } else {
        sb_add(&b, "/");
        for (i = 0; i < n; i++) {
            if (i > 0)
                sb_add(&b, "/");
            sb_add(&b, stack[i]);
        }
    }
    out = strdup(sb_cstr(&b));
    sb_free(&b);
    {
        char *rp = out ? realpath(out, NULL) : NULL;
        if (rp) {
            free(out);
            out = rp;
        }
    }

done:
    for (i = 0; i < n; i++)
        free(stack[i]);
    free(stack);
    return out;
}

/* 1 when `path` refers to the working directory itself or something inside
 * it; 0 when it clearly points outside (including ".." or absolute paths). */
int tool_path_inside_cwd(const char *path)
{
    char cwd[4096];
    char *t;
    size_t clen;
    int inside;

    if (!path || !*path)
        return 1;
    if (!getcwd(cwd, sizeof(cwd)))
        strcpy(cwd, ".");
    t = tool_canon_path(path);
    if (!t)
        return 1;      /* cannot resolve: be permissive */
    clen = strlen(cwd);
    inside = (strncmp(t, cwd, clen) == 0 &&
              (t[clen] == '\0' || t[clen] == '/'));
    free(t);
    return inside;
}

/* Warn + ask the user for permission to access a path outside the working
 * directory. Never prompts on piped/scripted input (always allows it then).
 * Returns 1 to allow, 0 to deny. */
int tool_ask_permission(const char *path)
{
    char prompt[1024];

    if (!isatty(STDIN_FILENO))
        return 1;      /* piped/scripted input: cannot prompt */
    snprintf(prompt, sizeof(prompt),
             "\n[WARNING] Access to '%s' is OUTSIDE the current working "
             "directory.\nAllow? [y/N] ", path ? path : "");
    return input_confirm(prompt, 0);
}

/* Shared guard: require user permission before any file tool touches a path
 * outside the working directory. Returns 0 after filling *out when denied,
 * 1 when allowed. */
static int tool_check_outside(const char *path, char **out)
{
    if (!tool_path_inside_cwd(path)) {
        if (!tool_ask_permission(path)) {
            SB m;
            sb_init(&m);
            sb_printf(&m, "denied: access to '%s' outside the working "
                          "directory was not allowed by the user",
                      path ? path : "");
            {
                char *r = strdup(sb_cstr(&m));
                sb_free(&m);
                *out = r;
                return 0;
            }
        }
    }
    return 1;
}

static int hasmsg_alloc(char **out, const char *msg)
{
    *out = strdup(msg ? msg : "error");
    return *out ? -1 : -1;
}

static int tool_read(const char *path, long limit, long offset, char **out)
{
    char *p;
    FILE *f;
    SB b;
    char *r;
    long lineno = 0, emitted = 0, total = 0;

    *out = NULL;
    if (!tool_check_outside(path, out))
        return -1;
    p = tool_expand_path(path);
    f = p ? fopen(p, "rb") : NULL;
    if (!f) {
        SB m;
        sb_init(&m);
        sb_printf(&m, "error: cannot read file '%s'", path ? path : "");
        free(p);
        return hasmsg_alloc(out, sb_cstr(&m));
    }
    sb_init(&b);
    {
        char buf[32768];
        while (fgets(buf, sizeof(buf), f) != NULL) {
            size_t n;
            lineno++;
            if (offset > 1 && lineno < offset)
                continue;
            if (limit >= 0 && emitted >= limit)
                break;
            n = strlen(buf);
            if (total + (long)n <= TOOL_READ_CAP) {
                sb_add(&b, buf);
                total += (long)n;
            } else {
                size_t keep = (size_t)(TOOL_READ_CAP - (total > TOOL_READ_CAP ?
                                                        TOOL_READ_CAP : total));
                sb_addn(&b, buf, keep);
                sb_add(&b, "\n... [file truncated, content is larger than 128 KB]");
                break;
            }
            emitted++;
        }
    }
    fclose(f);
    r = strdup(sb_cstr(&b));
    sb_free(&b);
    free(p);
    *out = r;
    return r ? 0 : -1;
}

/* opencode-style edit_file: replace every exact occurrence of old_str in the
 * file with new_str. Returns an "ok:" result with the replacement count, or
 * an error string when the oldString is not found. */
static int tool_edit(const char *path, const char *old_str, const char *new_str,
                     char **out)
{
    char *p;
    FILE *f;
    long n;
    char *buf = NULL;
    char *w;
    size_t olen = strlen(old_str ? old_str : "");
    size_t nlen = strlen(new_str ? new_str : "");
    size_t count = 0;
    size_t outlen;
    size_t i, j;
    SB res;

    if (!tool_check_outside(path, out))
        return -1;
    p = tool_expand_path(path);
    f = p ? fopen(p, "rb") : NULL;
    buf = NULL;
    w = NULL;

    if (!p)
        return hasmsg_alloc(out, "error: no path given");
    if (!f) {
        SB m;
        sb_init(&m);
        sb_printf(&m, "error: cannot read file '%s'", path ? path : "");
        free(p);
        return hasmsg_alloc(out, sb_cstr(&m));
    }
    if (olen == 0) {
        fclose(f);
        free(p);
        return hasmsg_alloc(out,
                            "error: oldString must not be empty");
    }
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0 || n > TOOL_EDIT_CAP) {
        fclose(f);
        free(p);
        return hasmsg_alloc(out,
                            "error: file too large to edit (limit 4 MB)");
    }
    buf = malloc((size_t)n + 1);
    if (!buf) {
        fclose(f);
        free(p);
        return hasmsg_alloc(out, "error: out of memory");
    }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        free(buf);
        fclose(f);
        free(p);
        return hasmsg_alloc(out, "error: read failed");
    }
    fclose(f);
    buf[n] = '\0';

    for (i = 0; i + olen <= (size_t)n;) {
        if (memcmp(buf + i, old_str, olen) == 0) {
            count++;
            i += olen;
        } else {
            i++;
        }
    }
    if (count == 0) {
        SB m;
        sb_init(&m);
        sb_printf(&m,
                  "error: oldString not found in '%s'. The oldString must "
                  "match the current file content exactly; read the file "
                  "first.",
                  path ? path : "");
        free(buf);
        free(p);
        return hasmsg_alloc(out, sb_cstr(&m));
    }

    outlen = (size_t)n + count * (nlen > olen ? nlen - olen : 0);
    w = malloc(outlen + 1);
    if (!w) {
        free(buf);
        free(p);
        return hasmsg_alloc(out, "error: out of memory");
    }
    j = 0;
    for (i = 0; i < (size_t)n;) {
        if (i + olen <= (size_t)n && memcmp(buf + i, old_str, olen) == 0) {
            memcpy(w + j, new_str, nlen);
            j += nlen;
            i += olen;
        } else {
            w[j++] = buf[i++];
        }
    }
    w[j] = '\0';

    f = fopen(p, "wb");
    if (!f) {
        free(w);
        free(buf);
        free(p);
        return hasmsg_alloc(out, "error: cannot open file for writing");
    }
    if (fwrite(w, 1, j, f) != j) {
        fclose(f);
        free(w);
        free(buf);
        free(p);
        return hasmsg_alloc(out, "error: short write");
    }
    fclose(f);
    free(buf);
    free(p);

    sb_init(&res);
    sb_printf(&res,
              "ok: replaced %zu occurrence%s of oldString in '%s' (wrote %zu "
              "bytes)",
              count, count == 1 ? "" : "s", path ? path : "", j);
    {
        char *r = strdup(sb_cstr(&res));
        sb_free(&res);
        *out = r;
        return r ? 0 : -1;
    }
}

static int confirm_write(const char *path)
{
    const char *e = getenv("BRACKETS_LLM_CONFIRM");
    char prompt[1024];
    if (!e || !*e)
        return 1;   /* automatic by default (opencode-like) */
    if (!(strcmp(e, "1") == 0 || strcmp(e, "yes") == 0 ||
          strcmp(e, "y") == 0 || strcmp(e, "true") == 0))
        return 1;
    if (!isatty(STDIN_FILENO))
        return 1;   /* piped/scripted input: cannot prompt */
    snprintf(prompt, sizeof(prompt),
             "The model wants to write '%s'. Allow? [y/N] ", path);
    return input_confirm(prompt, 0);
}

static int tool_write(const char *path, const char *content, char **out)
{
    char *p;
    SB m;
    size_t n;
    FILE *f;

    if (!tool_check_outside(path, out))
        return -1;
    p = tool_expand_path(path);
    if (!p)
        return hasmsg_alloc(out, "error: no path given");
    if (!confirm_write(p)) {
        SB t;
        sb_init(&t);
        sb_printf(&t, "denied: user did not allow writing '%s'", path);
        {
            char *r = strdup(sb_cstr(&t));
            sb_free(&t);
            free(p);
            return hasmsg_alloc(out, r);
        }
    }
    f = fopen(p, "wb");
    if (!f) {
        SB t;
        sb_init(&t);
        sb_printf(&t, "error: cannot open '%s' for writing", path);
        {
            char *r = strdup(sb_cstr(&t));
            sb_free(&t);
            free(p);
            return hasmsg_alloc(out, r);
        }
    }
    n = strlen(content ? content : "");
    if (fwrite(content ? content : "", 1, n, f) != n) {
        fclose(f);
        free(p);
        return hasmsg_alloc(out, "error: short write");
    }
    fclose(f);
    sb_init(&m);
    sb_printf(&m, "ok: wrote %zu bytes to %s", n, path);
    {
        char *r = strdup(sb_cstr(&m));
        sb_free(&m);
        free(p);
        *out = r;
        return r ? 0 : -1;
    }
}

static int tool_list(const char *dir, char **out)
{
    const char *d = dir && *dir ? dir : ".";
    char *p = tool_expand_path(d);
    DIR *dd = p ? opendir(p) : NULL;
    SB b;
    struct dirent *e;
    size_t count = 0;

    if (!tool_path_inside_cwd(d) && !tool_ask_permission(d)) {
        SB m;
        sb_init(&m);
        sb_printf(&m, "denied: access to '%s' outside the working directory "
                      "was not allowed by the user", d);
        free(p);
        return hasmsg_alloc(out, sb_cstr(&m));
    }

    if (!dd) {
        SB m;
        sb_init(&m);
        sb_printf(&m, "error: cannot list directory '%s'", d);
        free(p);
        return hasmsg_alloc(out, sb_cstr(&m));
    }
    sb_init(&b);
    sb_printf(&b, "directory %s:\n", p);
    while ((e = readdir(dd)) != NULL) {
        char fp[8192];
        struct stat st;
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        if (count++ >= 500) {
            sb_add(&b, "... (more entries omitted)\n");
            break;
        }
        if (strcmp(p, ".") == 0)
            snprintf(fp, sizeof(fp), "%s", e->d_name);
        else
            snprintf(fp, sizeof(fp), "%s/%s", p, e->d_name);
        if (stat(fp, &st) == 0 && S_ISDIR(st.st_mode))
            sb_printf(&b, "  d %s/\n", e->d_name);
        else
            sb_printf(&b, "  f %s\n", e->d_name);
    }
    closedir(dd);
    {
        char *r = strdup(sb_cstr(&b));
        sb_free(&b);
        free(p);
        *out = r;
        return r ? 0 : -1;
    }
}

/* ---------------------------------------------------------------- */
/* tools definitions                                                */
/* ---------------------------------------------------------------- */

static void add_param_string(JVal *props, const char *name, const char *desc)
{
    JVal *p = j_obj_new();
    j_obj_set(p, "type", j_str_new("string"));
    j_obj_set(p, "description", j_str_new(desc));
    j_obj_set(props, name, p);
}

static void add_param_integer(JVal *props, const char *name, const char *desc)
{
    JVal *p = j_obj_new();
    j_obj_set(p, "type", j_str_new("integer"));
    j_obj_set(p, "description", j_str_new(desc));
    j_obj_set(props, name, p);
}

static JVal *build_function(const char *name, const char *desc,
                            const char *prop_names[], const char *prop_types[],
                            const char *prop_descs[], int nprops, int nrequired)
{
    JVal *f = j_obj_new();
    JVal *params = j_obj_new();
    JVal *props = j_obj_new();
    JVal *required;
    int i;

    j_obj_set(f, "name", j_str_new(name));
    j_obj_set(f, "description", j_str_new(desc));
    for (i = 0; i < nprops; i++) {
        if (strcmp(prop_types[i], "integer") == 0)
            add_param_integer(props, prop_names[i], prop_descs[i]);
        else
            add_param_string(props, prop_names[i], prop_descs[i]);
    }
    j_obj_set(params, "type", j_str_new("object"));
    j_obj_set(params, "properties", props);
    if (nrequired > 0) {
        required = j_arr_new();
        for (i = 0; i < nrequired; i++)
            j_arr_push(required, j_str_new(prop_names[i]));
        j_obj_set(params, "required", required);
    }
    j_obj_set(f, "parameters", params);
    return f;
}

char *tools_definitions_json(void)
{
    JVal *tools = j_arr_new();
    SB b;
    char *s;

    {
        static const char *pn[] = { "path", "limit", "offset" };
        static const char *pt[] = { "string", "integer", "integer" };
        static const char *pd[] = {
            "Path of the file to read (may include ~ or be relative to the "
            "working directory).",
            "Maximum number of lines to return (optional; default: whole "
            "file).",
            "1-based line number to start reading from (optional; default: "
            "1)." };
        JVal *f = j_obj_new();
        JVal *fn = build_function(
            "read_file",
            "Read a text or code file from disk and return its contents "
            "(truncated to ~128 KB). Use this whenever you need to inspect "
            "or edit an existing file.",
            pn, pt, pd, 3, 1);
        j_obj_set(f, "type", j_str_new("function"));
        j_obj_set(f, "function", fn);
        j_arr_push(tools, f);
    }
    {
        static const char *pn[] = { "path", "oldString", "newString" };
        static const char *pt[] = { "string", "string", "string" };
        static const char *pd[] = {
            "Path of the file to edit (may include ~ or be relative to the "
            "working directory).",
            "The exact literal text currently in the file that should be "
            "replaced.",
            "The new text to put in place of oldString. Use an empty string "
            "to delete oldString." };
        JVal *f = j_obj_new();
        JVal *fn = build_function(
            "edit_file",
            "Make a targeted change to a text file by replacing an exact "
            "existing substring (oldString) with a new substring (newString). "
            "Prefer edit_file over write_file for editing existing large "
            "files: it only needs the changed fragments, not the whole file. "
            "oldString must match the file content exactly.",
            pn, pt, pd, 3, 3);
        j_obj_set(f, "type", j_str_new("function"));
        j_obj_set(f, "function", fn);
        j_arr_push(tools, f);
    }
    {
        static const char *pn[] = { "path", "content" };
        static const char *pt[] = { "string", "string" };
        static const char *pd[] = {
            "Path of the file to write (may include ~ or be relative to the "
            "working directory).",
            "Full new contents of the file." };
        JVal *f = j_obj_new();
        JVal *fn = build_function(
            "write_file",
            "Create a new file or overwrite an existing file with the given "
            "contents. Use this when the user asks you to create, edit or "
            "modify files. Prefer saving Brackets programs as .l1com files.",
            pn, pt, pd, 2, 2);
        j_obj_set(f, "type", j_str_new("function"));
        j_obj_set(f, "function", fn);
        j_arr_push(tools, f);
    }
    {
        static const char *pn[] = { "dir" };
        static const char *pt[] = { "string" };
        static const char *pd[] = {
            "Directory to list; defaults to the working directory." };
        JVal *f = j_obj_new();
        JVal *fn = build_function(
            "list_files",
            "List the files and sub-directories of a directory.",
            pn, pt, pd, 1, 0);
        j_obj_set(f, "type", j_str_new("function"));
        j_obj_set(f, "function", fn);
        j_arr_push(tools, f);
    }

    sb_init(&b);
    j_emit(tools, &b);
    s = strdup(sb_cstr(&b));
    sb_free(&b);
    j_free(tools);
    return s;
}

/* ---------------------------------------------------------------- */
/* dispatch                                                          */
/* ---------------------------------------------------------------- */

int tool_run(const char *name, const char *args_json, char **out)
{
    JVal *args;

    *out = NULL;
    args = j_parse(args_json ? args_json : "{}");
    if (!j_is(args, J_OBJ)) {
        j_free(args);
        return hasmsg_alloc(out, "error: tool arguments must be a JSON object");
    }

    if (strcmp(name, "read_file") == 0) {
        const JVal *v = j_get(args, "path");
        const char *path = v ? j_str(v) : NULL;
        if (!path) {
            j_free(args);
            return hasmsg_alloc(out, "error: read_file requires \"path\"");
        }
        {
            const JVal *vl = j_get(args, "limit");
            const JVal *vo = j_get(args, "offset");
            long limit = -1, offset = 1;
            if (vl && j_is(vl, J_NUM) && j_num(vl) >= 0)
                limit = (long)j_num(vl);
            if (vo && j_is(vo, J_NUM) && j_num(vo) >= 1)
                offset = (long)j_num(vo);
            {
                int rc = tool_read(path, limit, offset, out);
                j_free(args);
                return rc;
            }
        }
    }
    if (strcmp(name, "edit_file") == 0) {
        const JVal *vp = j_get(args, "path");
        const JVal *vo = j_get(args, "oldString");
        const JVal *vn = j_get(args, "newString");
        const char *path = vp ? j_str(vp) : NULL;
        const char *oldstr = vo ? j_str(vo) : NULL;
        const char *newstr = vn ? j_str(vn) : NULL;
        if (!path || !oldstr) {
            j_free(args);
            return hasmsg_alloc(out, "error: edit_file requires \"path\" and "
                                     "\"oldString\"");
        }
        {
            int r = tool_edit(path, oldstr, newstr ? newstr : "", out);
            j_free(args);
            return r;
        }
    }
    if (strcmp(name, "write_file") == 0) {
        const JVal *vp = j_get(args, "path");
        const JVal *vc = j_get(args, "content");
        const char *path = vp ? j_str(vp) : NULL;
        const char *content = vc ? j_str(vc) : NULL;
        if (!path || !content) {
            j_free(args);
            return hasmsg_alloc(out,
                                "error: write_file requires \"path\" and "
                                "\"content\"");
        }
        {
            int rc = tool_write(path, content, out);
            j_free(args);
            return rc;
        }
    }
    if (strcmp(name, "list_files") == 0) {
        const JVal *vd = j_get(args, "dir");
        const char *dir = vd ? j_str(vd) : NULL;
        {
            int rc = tool_list(dir, out);
            j_free(args);
            return rc;
        }
    }

    j_free(args);
    return hasmsg_alloc(out, "error: unknown tool");
}
