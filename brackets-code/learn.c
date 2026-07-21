/*
 * This file learn.c is part of L1vm.
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

#include "brackets-code.h"

// ==================== LEARNED PATTERN SYSTEM ====================

char* learned_dir_path(char *buf, int bufsize) {
    const char *home = getenv("HOME");
    if (home) {
        SNPRINTF_CHECK(buf, bufsize, "%s/%s", home, LEARNED_DIR);
    } else {
        SNPRINTF_CHECK(buf, bufsize, "%s", LEARNED_DIR);
    }
    return buf;
}

void ensure_learned_dir(void) {
    char path[1024];
    learned_dir_path(path, sizeof(path));
    // POSIX mkdir - create each directory level
    char tmp[1024];
    SNPRINTF_CHECK(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

int learn_from_file(const char *path, const char *keywords, const char *description) {
    FILE *f = fopen(path, "r");
    if (!f) {
        c_printf(ANSI_RED, "Error: cannot open file '%s'\n", path);
        return 0;
    }

    // Extract id from filename
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    char id[64];
    SNPRINTF_CHECK(id, sizeof(id), "%s", base);
    char *dot = strrchr(id, '.');
    if (dot) *dot = '\0';

    // Check if already learned
    if (has_learned_id(id)) {
        c_printf(ANSI_YELLOW, "Pattern '%s' already exists. Use --forget %s to remove it first.\n", id, id);
        fclose(f);
        return 0;
    }

    if (num_learned >= MAX_LEARNED) {
        c_printf(ANSI_RED, "Maximum number of learned patterns (%d) reached.\n", MAX_LEARNED);
        fclose(f);
        return 0;
    }

    LearnedPattern *lp = &learned_patterns[num_learned];
    init_learned_pattern(lp);
    SNPRINTF_CHECK(lp->id, sizeof(lp->id), "%s", id);
    SNPRINTF_CHECK(lp->source_path, sizeof(lp->source_path), "%s", path);
    lp->is_learned = 1;

    if (keywords && strlen(keywords) > 0) {
        SNPRINTF_CHECK(lp->keywords, sizeof(lp->keywords), "%s %s", id, keywords);
    } else {
        SNPRINTF_CHECK(lp->keywords, sizeof(lp->keywords), "%s", id);
    }

    if (description && strlen(description) > 0) {
        SNPRINTF_CHECK(lp->description, sizeof(lp->description), "%s", description);
    } else {
        SNPRINTF_CHECK(lp->description, sizeof(lp->description), "Learned pattern from %s", path);
    }

    // Parse the .l1com file into includes, globals and function structs
    char line[MAX_LINE];
    L1vmFunction *cur_func = NULL;
    int in_func = 0;

    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (strlen(line) == 0) continue;

        // Skip comments
        if (line[0] == '/' && line[1] == '/') continue;

        // Handle #include directives
        if (strncmp(line, "#include", 8) == 0) {
            char inc[256] = {0};
            if (sscanf(line, "#include <%255[^>]>", inc) == 1 ||
                sscanf(line, "#include \"%255[^\"]\"", inc) == 1) {
                int found = 0;
                for (int i = 0; i < lp->num_includes; i++) {
                    if (strcmp(lp->includes[i], inc) == 0) { found = 1; break; }
                }
                if (!found) {
                    if (!ensure_includes_cap(&lp->includes, &lp->includes_cap, lp->num_includes + 1)) continue;
                    SNPRINTF_CHECK(lp->includes[lp->num_includes++], sizeof(lp->includes[0]), "%s", inc);
                }
            }
            continue;
        }

        // Function definition
        if (strstr(line, " func)")) {
            char fname[256] = {0};
            if (sscanf(line, "(%255s func)", fname) == 1) {
                if (!ensure_funcs_cap(&lp->funcs, &lp->funcs_cap, lp->num_funcs + 1)) continue;
                cur_func = &lp->funcs[lp->num_funcs++];
                init_function(cur_func);
                SNPRINTF_CHECK(cur_func->name, sizeof(cur_func->name), "%s", fname);
                in_func = 1;
            }
            continue;
        }

        // Function end
        if (strcmp(line, "(funcend)") == 0) {
            in_func = 0;
            cur_func = NULL;
            continue;
        }

        if (!in_func) {
            // Global variable declarations (outside functions)
            size_t cur_len = strlen(lp->globals);
            if (cur_len + strlen(line) + 2 < MAX_CODE) {
                strncat(lp->globals, line, MAX_CODE - cur_len - 1);
                strncat(lp->globals, "\n", MAX_CODE - strlen(lp->globals) - 1);
            }
            continue;
        }

        // Inside a function: parse variable declarations and body
        if (strncmp(line, "(set ", 5) == 0) {
            char vtype[32], vname[256];
            int vcount;
            if (sscanf(line, "(set %31s %d %255s", vtype, &vcount, vname) >= 3) {
                size_t vnlen = strlen(vname);
                if (vnlen > 0 && vname[vnlen-1] == ')') vname[vnlen-1] = '\0';
                // Check for duplicates
                int found = 0;
                for (int i = 0; i < cur_func->num_vars; i++) {
                    if (strcmp(cur_func->vars[i].name, vname) == 0) { found = 1; break; }
                }
                if (!found) {
                    if (!ensure_vars_cap(cur_func, cur_func->num_vars + 1)) continue;
                    Variable *v = &cur_func->vars[cur_func->num_vars++];
                    SNPRINTF_CHECK(v->name, sizeof(v->name), "%s", vname);
                    SNPRINTF_CHECK(v->type, sizeof(v->type), "%s", vtype);
                    v->count = vcount;
                    // Parse values
                    char line_copy[MAX_LINE];
                    SNPRINTF_CHECK(line_copy, sizeof(line_copy), "%s", line);
                    // format: (set TYPE COUNT NAME [VAL1 VAL2 ...] )
                    // tokens: ["(set", TYPE, COUNT, NAME, VAL1, VAL2, ..., ")"]
                    int tok_idx = 0;
                    char *saveptr;
                    char *token = strtok_r(line_copy, " \t", &saveptr);
                    while (token) {
                        if (tok_idx >= 4) {
                            if (strcmp(token, ")") == 0) break;
                            size_t tlen = strlen(token);
                            if (token[tlen-1] == ')') token[tlen-1] = '\0';
                            if (v->num_values < MAX_VALUES) {
                                SNPRINTF_CHECK(v->values[v->num_values], sizeof(v->values[0]), "%s", token);
                                v->num_values++;
                            }
                        }
                        tok_idx++;
                        token = strtok_r(NULL, " \t", &saveptr);
                    }
                }
            }
            continue;
        }

        // Check for #var ~ (local scope)
        if (strncmp(line, "#var", 4) == 0) {
            char scope[256] = {0};
            if (sscanf(line, "#var ~ %255s", scope) == 1) {
                if (cur_func) {
                    cur_func->has_vardef = 1;
                    cur_func->is_local = 1;
                    SNPRINTF_CHECK(cur_func->vardef_name, sizeof(cur_func->vardef_name), "%s", scope);
                }
            }
            continue;
        }

        // Body code
        if (cur_func) {
            size_t needed = strlen(cur_func->body) + strlen(line) + 2;
            if (!ensure_body_cap(cur_func, (int)needed + 1)) continue;
            strncat(cur_func->body, line, (size_t)cur_func->body_cap - strlen(cur_func->body) - 1);
            strncat(cur_func->body, "\n", (size_t)cur_func->body_cap - strlen(cur_func->body) - 1);
        }
    }
    fclose(f);

    // Check we got at least one function
    if (lp->num_funcs == 0) {
        c_printf(ANSI_RED, "Error: no functions found in '%s'\n", path);
        free_learned_pattern(lp);
        return 0;
    }

    num_learned++;

    // Persist to disk
    if (save_learned_pattern(lp)) {
        c_printf(ANSI_GREEN, "Learned pattern '%s' from %s\n", lp->id, path);
        if (verbose_flag) {
            printf("  Keywords: %s\n", lp->keywords);
            printf("  Description: %s\n", lp->description);
            printf("  Includes: %d, Functions: %d\n", lp->num_includes, lp->num_funcs);
        }
        return 1;
    }
    free_learned_pattern(lp);
    num_learned--;
    memset(&learned_patterns[num_learned], 0, sizeof(LearnedPattern));
    return 0;
}

int save_learned_pattern(LearnedPattern *lp) {
    ensure_learned_dir();
    char filepath[1100];
    char ldir[1024];
    learned_dir_path(ldir, sizeof(ldir));
    SNPRINTF_CHECK(filepath, sizeof(filepath), "%s/%s.l1lp", ldir, lp->id);

    FILE *f = fopen(filepath, "w");
    if (!f) {
        c_printf(ANSI_RED, "Error: cannot write '%s'\n", filepath);
        return 0;
    }

    fprintf(f, "# Pattern: %s\n", lp->id);
    fprintf(f, "# Description: %s\n", lp->description);
    fprintf(f, "# Keywords: %s\n", lp->keywords);
    fprintf(f, "# File: %s\n", lp->source_path);
    fprintf(f, "# Includes: %d\n", lp->num_includes);
    for (int i = 0; i < lp->num_includes; i++)
        fprintf(f, "# Include: %s\n", lp->includes[i]);
    fprintf(f, "# Functions: %d\n", lp->num_funcs);
    fprintf(f, "---SOURCE---\n");

    // Write the reconstructed .l1com source
    for (int i = 0; i < lp->num_includes; i++)
        fprintf(f, "#include <%s>\n", lp->includes[i]);

    if (strlen(lp->globals) > 0)
        fprintf(f, "%s\n", lp->globals);

    for (int fi = 0; fi < lp->num_funcs; fi++) {
        L1vmFunction *fn = &lp->funcs[fi];
        fprintf(f, "(%s func)\n", fn->name);
        if (fn->has_vardef)
            fprintf(f, "\t#var ~ %s\n", fn->vardef_name);
        for (int vi = 0; vi < fn->num_vars; vi++) {
            Variable *v = &fn->vars[vi];
            fprintf(f, "\t(set %s %d %s", v->type, v->count, v->name);
            for (int vj = 0; vj < v->num_values; vj++)
                fprintf(f, " %s", v->values[vj]);
            fprintf(f, ")\n");
        }
        fprintf(f, "%s", fn->body);
        fprintf(f, "(funcend)\n\n");
    }

    fprintf(f, "---END---\n");
    fclose(f);

    if (verbose_flag)
        printf("  Saved to %s\n", filepath);
    return 1;
}

void load_learned_patterns(void) {
    if (learned_loaded) return;
    learned_loaded = 1;

    char dirpath[1024];
    learned_dir_path(dirpath, sizeof(dirpath));

    DIR *d = opendir(dirpath);
    if (!d) return;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && num_learned < MAX_LEARNED) {
        const char *ext = strrchr(entry->d_name, '.');
        if (!ext || strcmp(ext, ".l1lp") != 0) continue;

        char filepath[2048];
        SNPRINTF_CHECK(filepath, sizeof(filepath), "%s/%s", dirpath, entry->d_name);

        FILE *f = fopen(filepath, "r");
        if (!f) continue;

        LearnedPattern *lp = &learned_patterns[num_learned];
        init_learned_pattern(lp);
        lp->is_learned = 1;

        char line[MAX_LINE];
        int in_source = 0;
        L1vmFunction *cur_func = NULL;

        while (fgets(line, sizeof(line), f)) {
            trim(line);

            if (strcmp(line, "---SOURCE---") == 0) {
                in_source = 1;
                continue;
            }
            if (strcmp(line, "---END---") == 0) {
                in_source = 0;
                continue;
            }

            if (!in_source) {
                if (sscanf(line, "# Pattern: %63s", lp->id) == 1) continue;
                char *val;
                if ((val = strstr(line, "# Description: ")) != NULL) {
                    SNPRINTF_CHECK(lp->description, sizeof(lp->description), "%s", val + 15);
                    continue;
                }
                if ((val = strstr(line, "# Keywords: ")) != NULL) {
                    SNPRINTF_CHECK(lp->keywords, sizeof(lp->keywords), "%s", val + 12);
                    continue;
                }
                if ((val = strstr(line, "# File: ")) != NULL) {
                    SNPRINTF_CHECK(lp->source_path, sizeof(lp->source_path), "%s", val + 8);
                    continue;
                }
                continue;
            }

            // Parse source content
            if (strncmp(line, "#include", 8) == 0) {
                char inc[256] = {0};
                if (sscanf(line, "#include <%255[^>]>", inc) == 1) {
                    if (ensure_includes_cap(&lp->includes, &lp->includes_cap, lp->num_includes + 1))
                        SNPRINTF_CHECK(lp->includes[lp->num_includes++], sizeof(lp->includes[0]), "%s", inc);
                }
                continue;
            }

            if (strstr(line, " func)")) {
                char fname[256] = {0};
                if (sscanf(line, "(%255s func)", fname) == 1) {
                    if (ensure_funcs_cap(&lp->funcs, &lp->funcs_cap, lp->num_funcs + 1)) {
                        cur_func = &lp->funcs[lp->num_funcs++];
                        init_function(cur_func);
                        SNPRINTF_CHECK(cur_func->name, sizeof(cur_func->name), "%s", fname);
                    }
                }
                continue;
            }

            if (strcmp(line, "(funcend)") == 0) {
                cur_func = NULL;
                continue;
            }

            if (!cur_func) {
                size_t cur_len = strlen(lp->globals);
                if (cur_len + strlen(line) + 2 < MAX_CODE) {
                    strncat(lp->globals, line, MAX_CODE - cur_len - 1);
                    strncat(lp->globals, "\n", MAX_CODE - strlen(lp->globals) - 1);
                }
                continue;
            }

            if (strncmp(line, "(set ", 5) == 0) {
                char vtype[32], vname[256];
                int vcount;
                if (sscanf(line, "(set %31s %d %255s", vtype, &vcount, vname) >= 3) {
                    size_t vnlen = strlen(vname);
                    if (vnlen > 0 && vname[vnlen-1] == ')') vname[vnlen-1] = '\0';
                    int found = 0;
                    for (int i = 0; i < cur_func->num_vars; i++) {
                        if (strcmp(cur_func->vars[i].name, vname) == 0) { found = 1; break; }
                    }
                    if (!found) {
                        if (!ensure_vars_cap(cur_func, cur_func->num_vars + 1)) continue;
                        Variable *v = &cur_func->vars[cur_func->num_vars++];
                        SNPRINTF_CHECK(v->name, sizeof(v->name), "%s", vname);
                        SNPRINTF_CHECK(v->type, sizeof(v->type), "%s", vtype);
                        v->count = vcount;
                        char lc[MAX_LINE];
                        SNPRINTF_CHECK(lc, sizeof(lc), "%s", line);
                        int tok_idx = 0;
                        char *saveptr2;
                        char *token = strtok_r(lc, " \t", &saveptr2);
                        while (token) {
                            if (tok_idx >= 4) {
                                if (strcmp(token, ")") == 0) break;
                                size_t tlen = strlen(token);
                                if (token[tlen-1] == ')') token[tlen-1] = '\0';
                                if (v->num_values < MAX_VALUES) {
                                    SNPRINTF_CHECK(v->values[v->num_values], sizeof(v->values[0]), "%s", token);
                                    v->num_values++;
                                }
                            }
                            tok_idx++;
                            token = strtok_r(NULL, " \t", &saveptr2);
                        }
                    }
                }
                continue;
            }

            if (strncmp(line, "#var", 4) == 0) {
                char scope[256] = {0};
                if (sscanf(line, "#var ~ %255s", scope) == 1 && cur_func) {
                    cur_func->has_vardef = 1;
                    cur_func->is_local = 1;
                    SNPRINTF_CHECK(cur_func->vardef_name, sizeof(cur_func->vardef_name), "%s", scope);
                }
                continue;
            }

            if (cur_func) {
                size_t needed = strlen(cur_func->body) + strlen(line) + 2;
                if (!ensure_body_cap(cur_func, (int)needed + 1)) continue;
                strncat(cur_func->body, line, (size_t)cur_func->body_cap - strlen(cur_func->body) - 1);
                strncat(cur_func->body, "\n", (size_t)cur_func->body_cap - strlen(cur_func->body) - 1);
            }
        }
        fclose(f);

        if (lp->num_funcs > 0) {
            num_learned++;
            if (verbose_flag) {
                printf("  Loaded learned pattern: %s (%s)\n", lp->id, lp->description);
            }
        } else {
            free_learned_pattern(lp);
        }
    }
    closedir(d);
}

int match_learned_pattern(const char *prompt, int *best_score) {
    if (num_learned == 0) return -1;

    char buf[MAX_PROMPT];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", prompt);
    to_lowercase(buf);

    int best_idx = -1;
    *best_score = 0;

    for (int i = 0; i < num_learned; i++) {
        if (!learned_patterns[i].is_learned) continue;
        if (strlen(learned_patterns[i].keywords) == 0) continue;

        char kwbuf[1024];
        SNPRINTF_CHECK(kwbuf, sizeof(kwbuf), "%s", learned_patterns[i].keywords);
        to_lowercase(kwbuf);

        int score = 0;
        int total_kw = 0;
        int any_match = 0;
        char kwcopy[1024];
        SNPRINTF_CHECK(kwcopy, sizeof(kwcopy), "%s", kwbuf);
        char *saveptr3;
        char *kw = strtok_r(kwcopy, " ,/", &saveptr3);
        while (kw) {
            trim(kw);
            if (strlen(kw) > 0) {
                total_kw++;
                // Match as word OR as substring (handles hyphens in filenames)
                if (str_contains_word(buf, kw) || strstr(buf, kw)) {
                    score++;
                    any_match = 1;
                }
            }
            kw = strtok_r(NULL, " ,/", &saveptr3);
        }
        // Match if at least one keyword matches and score >= 25% of total keywords
        if (any_match && total_kw > 0) {
            float ratio = (float)score / total_kw;
            if ((score >= 1 && ratio >= 0.25f) || total_kw <= 2) {
                // Weight: keyword matches + bonus for filename match
                int final_score = score * 10;
                if (strstr(buf, learned_patterns[i].id))
                    final_score += 5;
                if (final_score > *best_score) {
                    *best_score = final_score;
                    best_idx = i;
                }
            }
        }
    }
    return best_idx;
}

int emit_learned_pattern(Program *prog, int learned_idx) {
    if (learned_idx < 0 || learned_idx >= num_learned) return 0;
    LearnedPattern *lp = &learned_patterns[learned_idx];
    if (!lp->is_learned) return 0;

    // Copy includes
    for (int i = 0; i < lp->num_includes; i++)
        add_include(prog, lp->includes[i]);

    // Copy globals
    if (strlen(lp->globals) > 0) {
        size_t cur_len = strlen(prog->globals);
        if (cur_len + strlen(lp->globals) < MAX_CODE)
            strncat(prog->globals, lp->globals, MAX_CODE - cur_len - 1);
    }

    // Copy functions
    for (int fi = 0; fi < lp->num_funcs; fi++) {
        L1vmFunction *src = &lp->funcs[fi];
        add_func(prog, src->name);
        L1vmFunction *dst = &prog->funcs[prog->num_funcs - 1];

        dst->is_local = src->is_local;
        dst->has_vars = src->has_vars;
        dst->has_vardef = src->has_vardef;
        SNPRINTF_CHECK(dst->vardef_name, sizeof(dst->vardef_name), "%s", src->vardef_name);

        // Copy variables
        for (int vi = 0; vi < src->num_vars; vi++) {
            if (!ensure_vars_cap(dst, dst->num_vars + 1)) continue;
            Variable *sv = &src->vars[vi];
            Variable *dv = &dst->vars[dst->num_vars++];
            SNPRINTF_CHECK(dv->name, sizeof(dv->name), "%s", sv->name);
            SNPRINTF_CHECK(dv->type, sizeof(dv->type), "%s", sv->type);
            dv->count = sv->count;
            dv->num_values = sv->num_values;
            for (int vj = 0; vj < sv->num_values; vj++)
                SNPRINTF_CHECK(dv->values[vj], sizeof(dv->values[vj]), "%s", sv->values[vj]);
        }

        // Copy body
        size_t needed = strlen(src->body) + 1;
        if (ensure_body_cap(dst, (int)needed))
            SNPRINTF_CHECK(dst->body, (size_t)dst->body_cap, "%s", src->body);
    }

    return 1;
}

int emit_learned_step(Program *prog, int learned_idx) {
    if (learned_idx < 0 || learned_idx >= num_learned) return 0;
    LearnedPattern *lp = &learned_patterns[learned_idx];
    if (!lp->is_learned) return 0;

    // Copy includes (add_include handles duplicates)
    for (int i = 0; i < lp->num_includes; i++)
        add_include(prog, lp->includes[i]);

    // Append globals
    if (strlen(lp->globals) > 0) {
        size_t cur_len = strlen(prog->globals);
        if (cur_len + strlen(lp->globals) < MAX_CODE)
            strncat(prog->globals, lp->globals, MAX_CODE - cur_len - 1);
    }

    // Copy functions - for existing main, append body & vars
    for (int fi = 0; fi < lp->num_funcs; fi++) {
        L1vmFunction *src = &lp->funcs[fi];

        int existing = -1;
        for (int i = 0; i < prog->num_funcs; i++)
            if (strcmp(prog->funcs[i].name, src->name) == 0) { existing = i; break; }

        if (existing >= 0) {
            L1vmFunction *dst = &prog->funcs[existing];
            size_t needed = strlen(dst->body) + strlen(src->body) + 1;
            if (ensure_body_cap(dst, (int)needed + 1))
                strncat(dst->body, src->body, (size_t)dst->body_cap - strlen(dst->body) - 1);
            for (int vi = 0; vi < src->num_vars; vi++) {
                int found = 0;
                for (int dv = 0; dv < dst->num_vars; dv++)
                    if (strcmp(dst->vars[dv].name, src->vars[vi].name) == 0) { found = 1; break; }
                if (!found) {
                    if (!ensure_vars_cap(dst, dst->num_vars + 1)) continue;
                    Variable *sv = &src->vars[vi];
                    Variable *dv = &dst->vars[dst->num_vars++];
                    SNPRINTF_CHECK(dv->name, sizeof(dv->name), "%s", sv->name);
                    SNPRINTF_CHECK(dv->type, sizeof(dv->type), "%s", sv->type);
                    dv->count = sv->count;
                    dv->num_values = sv->num_values;
                    for (int vj = 0; vj < sv->num_values; vj++)
                        SNPRINTF_CHECK(dv->values[vj], sizeof(dv->values[vj]), "%s", sv->values[vj]);
                }
            }
        } else {
            add_func(prog, src->name);
            L1vmFunction *dst = &prog->funcs[prog->num_funcs - 1];
            dst->is_local = src->is_local;
            dst->has_vars = src->has_vars;
            dst->has_vardef = src->has_vardef;
            SNPRINTF_CHECK(dst->vardef_name, sizeof(dst->vardef_name), "%s", src->vardef_name);
            for (int vi = 0; vi < src->num_vars; vi++) {
                if (!ensure_vars_cap(dst, dst->num_vars + 1)) continue;
                Variable *sv = &src->vars[vi];
                Variable *dv = &dst->vars[dst->num_vars++];
                SNPRINTF_CHECK(dv->name, sizeof(dv->name), "%s", sv->name);
                SNPRINTF_CHECK(dv->type, sizeof(dv->type), "%s", sv->type);
                dv->count = sv->count;
                dv->num_values = sv->num_values;
                for (int vj = 0; vj < sv->num_values; vj++)
                    SNPRINTF_CHECK(dv->values[vj], sizeof(dv->values[vj]), "%s", sv->values[vj]);
            }
            size_t needed = strlen(src->body) + 1;
            if (ensure_body_cap(dst, (int)needed))
                SNPRINTF_CHECK(dst->body, (size_t)dst->body_cap, "%s", src->body);
        }
    }
    return 1;
}

int has_learned_id(const char *id) {
    for (int i = 0; i < num_learned; i++) {
        if (strcmp(learned_patterns[i].id, id) == 0)
            return 1;
    }
    return 0;
}

static int is_safe_id(const char *id) {
    if (!id || !*id) return 0;
    if (strstr(id, "..") || strchr(id, '/')) return 0;
    for (const char *p = id; *p; p++) {
        if (!isalnum(*p) && *p != '-' && *p != '_' && *p != '.')
            return 0;
    }
    return 1;
}

int forget_learned(const char *id) {
    if (!is_safe_id(id)) {
        c_printf(ANSI_RED, "Error: invalid pattern id (path traversal blocked)\n");
        return 0;
    }

    int found = -1;
    for (int i = 0; i < num_learned; i++) {
        if (strcmp(learned_patterns[i].id, id) == 0) {
            found = i;
            break;
        }
    }
    if (found < 0) {
        c_printf(ANSI_RED, "Pattern '%s' not found.\n", id);
        return 0;
    }

    // Remove file from disk
    char filepath[1100];
    char ldir[1024];
    learned_dir_path(ldir, sizeof(ldir));
    SNPRINTF_CHECK(filepath, sizeof(filepath), "%s/%s.l1lp", ldir, learned_patterns[found].id);
    remove(filepath);

    // Free allocated memory in the pattern being removed
    free_learned_pattern(&learned_patterns[found]);

    // Shift array
    for (int i = found; i < num_learned - 1; i++)
        learned_patterns[i] = learned_patterns[i + 1];
    num_learned--;

    // Clear the now-unused slot to prevent stale pointer copies
    memset(&learned_patterns[num_learned], 0, sizeof(LearnedPattern));

    c_printf(ANSI_GREEN, "Forgot pattern '%s'.\n", id);
    return 1;
}

void list_learned(void) {
    if (num_learned == 0) {
        printf("No learned patterns.\n");
        printf("Use --learn <file.l1com> [keywords] [description] to learn one.\n");
        return;
    }
    printf("Learned patterns (%d):\n", num_learned);
    for (int i = 0; i < num_learned; i++) {
        LearnedPattern *lp = &learned_patterns[i];
        printf("  %2d. %-20s Keywords: %s\n", i + 1, lp->id, lp->keywords);
        printf("      Description: %s\n", lp->description);
        printf("      Source: %s\n", lp->source_path);
    }
}
