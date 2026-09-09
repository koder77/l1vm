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
 * brackets-llm - agent tools: read_file / write_file / list_files
 *
 * Executes tool calls requested by the LLM, opencode-style. File writes are
 * allowed automatically; set BRACKETS_LLM_CONFIRM=1 to be asked for
 * confirmation on every write (in interactive mode).
 */

#define _POSIX_C_SOURCE 200809L

#include "tools.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "json.h"
#include "sb.h"

#define TOOL_READ_CAP 128000L

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

static int hasmsg_alloc(char **out, const char *msg)
{
    *out = strdup(msg ? msg : "error");
    return *out ? -1 : -1;
}

static int tool_read(const char *path, char **out)
{
    char *p = tool_expand_path(path);
    FILE *f = p ? fopen(p, "rb") : NULL;
    SB b;
    char *r;

    *out = NULL;
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
        size_t n;
        long total = 0;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
            if (total + (long)n <= TOOL_READ_CAP) {
                sb_addn(&b, buf, n);
                total += (long)n;
            } else {
                size_t keep = (size_t)(TOOL_READ_CAP - (total > TOOL_READ_CAP ?
                                                        TOOL_READ_CAP : total));
                sb_addn(&b, buf, keep);
                sb_add(&b, "\n... [file truncated, content is larger than 128 KB]");
                break;
            }
        }
    }
    fclose(f);
    r = strdup(sb_cstr(&b));
    sb_free(&b);
    free(p);
    *out = r;
    return r ? 0 : -1;
}

static int confirm_write(const char *path)
{
    const char *e = getenv("BRACKETS_LLM_CONFIRM");
    char line[64] = "";
    if (!e || !*e)
        return 1;   /* automatic by default (opencode-like) */
    if (!(strcmp(e, "1") == 0 || strcmp(e, "yes") == 0 ||
          strcmp(e, "y") == 0 || strcmp(e, "true") == 0))
        return 1;
    if (!isatty(STDIN_FILENO))
        return 1;   /* piped/scripted input: cannot prompt */
    printf("The model wants to write '%s'. Allow? [y/N] ", path);
    fflush(stdout);
    if (!fgets(line, sizeof(line), stdin))
        return 0;
    return line[0] == 'y' || line[0] == 'Y';
}

static int tool_write(const char *path, const char *content, char **out)
{
    char *p = tool_expand_path(path);
    SB m;
    size_t n;
    FILE *f;

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
    char *p = tool_expand_path(dir && *dir ? dir : ".");
    DIR *d = p ? opendir(p) : NULL;
    SB b;
    struct dirent *e;
    size_t count = 0;

    if (!d) {
        SB m;
        sb_init(&m);
        sb_printf(&m, "error: cannot list directory '%s'", dir && *dir ? dir : ".");
        free(p);
        return hasmsg_alloc(out, sb_cstr(&m));
    }
    sb_init(&b);
    sb_printf(&b, "directory %s:\n", p);
    while ((e = readdir(d)) != NULL) {
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
    closedir(d);
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

static JVal *build_function(const char *name, const char *desc,
                            const char *prop_names[], const char *prop_descs[],
                            int nprops, int required_all)
{
    JVal *f = j_obj_new();
    JVal *params = j_obj_new();
    JVal *props = j_obj_new();
    JVal *required;
    int i;

    j_obj_set(f, "name", j_str_new(name));
    j_obj_set(f, "description", j_str_new(desc));
    for (i = 0; i < nprops; i++)
        add_param_string(props, prop_names[i], prop_descs[i]);
    j_obj_set(params, "type", j_str_new("object"));
    j_obj_set(params, "properties", props);
    if (nprops > 0 && required_all) {
        required = j_arr_new();
        for (i = 0; i < nprops; i++)
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
        static const char *pn[] = { "path" };
        static const char *pd[] = {
            "Path of the file to read (may include ~ or be relative to the "
            "working directory)." };
        JVal *f = j_obj_new();
        JVal *fn = build_function(
            "read_file",
            "Read a text or code file from disk and return its contents "
            "(truncated to ~128 KB). Use this whenever you need to inspect "
            "or edit an existing file.",
            pn, pd, 1, 1);
        j_obj_set(f, "type", j_str_new("function"));
        j_obj_set(f, "function", fn);
        j_arr_push(tools, f);
    }
    {
        static const char *pn[] = { "path", "content" };
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
            pn, pd, 2, 1);
        j_obj_set(f, "type", j_str_new("function"));
        j_obj_set(f, "function", fn);
        j_arr_push(tools, f);
    }
    {
        static const char *pn[] = { "dir" };
        static const char *pd[] = {
            "Directory to list; defaults to the working directory." };
        JVal *f = j_obj_new();
        JVal *fn = build_function(
            "list_files",
            "List the files and sub-directories of a directory.",
            pn, pd, 1, 0);
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
            int rc = tool_read(path, out);
            j_free(args);
            return rc;
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
