/*
 * This file dsl.c is part of L1vm.
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

#include "dsl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <dirent.h>
#include <sys/stat.h>

DslRule dsl_rules[MAX_DSL_RULES];
int dsl_num_rules = 0;

static DslTokenType parse_token_type(const char *s)
{
    if (strcmp(s, "int64") == 0) return DSL_TOKEN_INT64;
    if (strcmp(s, "double") == 0) return DSL_TOKEN_DOUBLE;
    if (strcmp(s, "string") == 0) return DSL_TOKEN_STRING;
    if (strcmp(s, "const-int64") == 0) return DSL_TOKEN_CONST_INT64;
    if (strcmp(s, "const-double") == 0) return DSL_TOKEN_CONST_DOUBLE;
    if (strcmp(s, "const-string") == 0) return DSL_TOKEN_CONST_STRING;
    return DSL_TOKEN_UNKNOWN;
}

DslTokenType dsl_token_type_from_string(const char *str)
{
    return parse_token_type(str);
}

const char* dsl_token_type_to_string(DslTokenType type)
{
    switch (type) {
        case DSL_TOKEN_INT64: return "int64";
        case DSL_TOKEN_DOUBLE: return "double";
        case DSL_TOKEN_STRING: return "string";
        case DSL_TOKEN_CONST_INT64: return "const-int64";
        case DSL_TOKEN_CONST_DOUBLE: return "const-double";
        case DSL_TOKEN_CONST_STRING: return "const-string";
        case DSL_TOKEN_INT64_ARRAY: return "int64[]";
        case DSL_TOKEN_DOUBLE_ARRAY: return "double[]";
        case DSL_TOKEN_STRING_ARRAY: return "string[]";
        case DSL_TOKEN_INT64_REF: return "int64&";
        case DSL_TOKEN_DOUBLE_REF: return "double&";
        default: return "unknown";
    }
}

const char* dsl_token_l1vm_type(DslTokenType type)
{
    switch (type) {
        case DSL_TOKEN_INT64:
        case DSL_TOKEN_INT64_ARRAY:
        case DSL_TOKEN_CONST_INT64:
        case DSL_TOKEN_INT64_REF:
            return "int64";
        case DSL_TOKEN_DOUBLE:
        case DSL_TOKEN_DOUBLE_ARRAY:
        case DSL_TOKEN_CONST_DOUBLE:
        case DSL_TOKEN_DOUBLE_REF:
            return "double";
        case DSL_TOKEN_STRING:
        case DSL_TOKEN_STRING_ARRAY:
        case DSL_TOKEN_CONST_STRING:
            return "string";
        default:
            return "int64";
    }
}

int parse_token_decl(const char *line, DslToken *token)
{
    char buf[512];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", line);
    trim(buf);

    char *space = strchr(buf, ' ');
    if (!space) return 0;
    *space = '\0';
    char *type_str = buf;
    char *name_str = space + 1;
    trim(type_str);
    trim(name_str);

    int is_array = 0;
    int array_size = 1;
    char *lbracket = strchr(name_str, '[');
    if (lbracket) {
        is_array = 1;
        *lbracket = '\0';
        char *rbracket = strchr(lbracket + 1, ']');
        if (rbracket) {
            *rbracket = '\0';
            array_size = (int)strtol(lbracket + 1, NULL, 10);
            if (array_size < 1) array_size = 1;
        }
    }

    SNPRINTF_CHECK(token->name, sizeof(token->name), "%s", name_str);
    SNPRINTF_CHECK(token->l1vm_type, sizeof(token->l1vm_type), "%.31s", type_str);
    token->type = parse_token_type(type_str);
    token->is_array = is_array;
    token->array_size = array_size;
    token->default_value[0] = '\0';

    if (token->type == DSL_TOKEN_UNKNOWN) {
        if (is_array) {
            if (strcmp(type_str, "int64") == 0) token->type = DSL_TOKEN_INT64_ARRAY;
            else if (strcmp(type_str, "double") == 0) token->type = DSL_TOKEN_DOUBLE_ARRAY;
            else if (strcmp(type_str, "string") == 0) token->type = DSL_TOKEN_STRING_ARRAY;
            else token->type = DSL_TOKEN_INT64_ARRAY;
        } else {
            if (strcmp(type_str, "int64") == 0) token->type = DSL_TOKEN_INT64;
            else if (strcmp(type_str, "double") == 0) token->type = DSL_TOKEN_DOUBLE;
            else if (strcmp(type_str, "string") == 0) token->type = DSL_TOKEN_STRING;
            else token->type = DSL_TOKEN_INT64;
        }
    }
    return 1;
}

static int parse_var_decl(const char *line, DslVarDecl *vd)
{
    char buf[512];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", line);
    trim(buf);

    char type[64], name[64];
    int count = 1;
    char value[256] = "";

    int n = sscanf(buf, "%63s %63s %d %255[^\n\r]", type, name, &count, value);
    if (n < 2) return 0;

    SNPRINTF_CHECK(vd->type, sizeof(vd->type), "%s", type);
    SNPRINTF_CHECK(vd->name, sizeof(vd->name), "%s", name);
    vd->count = (n >= 3) ? count : 1;
    if (n >= 4) {
        trim(value);
        SNPRINTF_CHECK(vd->value, sizeof(vd->value), "%s", value);
    } else {
        vd->value[0] = '\0';
    }
    return 1;
}

static int parse_param_decl(const char *line, DslParam *param)
{
    char buf[512];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", line);
    trim(buf);

    char type[32], name[64];
    int n = sscanf(buf, "%31s %63s", type, name);
    if (n < 2) return 0;

    SNPRINTF_CHECK(param->type, sizeof(param->type), "%s", type);
    SNPRINTF_CHECK(param->name, sizeof(param->name), "%s", name);
    param->desc[0] = '\0';
    param->default_value[0] = '\0';
    param->min[0] = '\0';
    param->max[0] = '\0';
    param->pattern[0] = '\0';
    param->required = 0;

    char *p = buf;
    while (*p && !isspace((unsigned char)*p)) p++;
    while (*p && isspace((unsigned char)*p)) p++;
    while (*p && !isspace((unsigned char)*p)) p++;
    while (*p && isspace((unsigned char)*p)) p++;

    if (*p == '"') {
        p++;
        char *end = strchr(p, '"');
        if (end) {
            size_t len = (size_t)(end - p);
            if (len >= sizeof(param->desc)) len = sizeof(param->desc) - 1;
            strncpy(param->desc, p, len);
            param->desc[len] = '\0';
            p = end + 1;
            while (*p && isspace((unsigned char)*p)) p++;
        }
    }

    while (*p) {
        if (strncmp(p, "required", 8) == 0 && (p[8] == '\0' || isspace((unsigned char)p[8]))) {
            param->required = 1;
            while (*p && !isspace((unsigned char)*p)) p++;
            while (*p && isspace((unsigned char)*p)) p++;
            continue;
        }
        char key[64] = {0};
        char value[256] = {0};
        int ki = 0;
        while (*p && *p != '=' && !isspace((unsigned char)*p) && ki < 63) {
            key[ki++] = *p++;
        }
        key[ki] = '\0';
        if (*p == '=') {
            p++;
            int vi = 0;
            if (*p == '"') {
                p++;
                while (*p && *p != '"' && vi < 255) value[vi++] = *p++;
                if (*p == '"') p++;
            } else {
                while (*p && !isspace((unsigned char)*p) && vi < 255) value[vi++] = *p++;
            }
            value[vi] = '\0';
            if (strcmp(key, "min") == 0) SNPRINTF_CHECK(param->min, sizeof(param->min), "%s", value);
            else if (strcmp(key, "max") == 0) SNPRINTF_CHECK(param->max, sizeof(param->max), "%s", value);
            else if (strcmp(key, "default") == 0) SNPRINTF_CHECK(param->default_value, sizeof(param->default_value), "%s", value);
            else if (strcmp(key, "pattern") == 0) SNPRINTF_CHECK(param->pattern, sizeof(param->pattern), "%s", value);
            else if (strcmp(key, "desc") == 0) SNPRINTF_CHECK(param->desc, sizeof(param->desc), "%s", value);
        } else {
            break;
        }
        while (*p && isspace((unsigned char)*p)) p++;
    }
    return 1;
}

static int parse_test_decl(const char *line, DslTest *test)
{
    char buf[512];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", line);
    trim(buf);

    test->input[0] = '\0';
    test->expect[0] = '\0';

    if (*buf != '"') return 0;
    char *p = buf + 1;
    char *end = strchr(p, '"');
    if (!end) return 0;
    size_t len = (size_t)(end - p);
    if (len >= sizeof(test->input)) len = sizeof(test->input) - 1;
    strncpy(test->input, p, len);
    test->input[len] = '\0';
    p = end + 1;
    while (*p && isspace((unsigned char)*p)) p++;

    if (strncmp(p, "expect:", 7) == 0) {
        p += 7;
        while (*p && isspace((unsigned char)*p)) p++;
        if (*p == '"') {
            p++;
            end = strchr(p, '"');
            if (end) {
                len = (size_t)(end - p);
                if (len >= sizeof(test->expect)) len = sizeof(test->expect) - 1;
                strncpy(test->expect, p, len);
                test->expect[len] = '\0';
            }
        }
    }
    return 1;
}

void dsl_free_rules(void)
{
    dsl_num_rules = 0;
}

static int is_dsl_comment(const char *line)
{
    while (*line && isspace((unsigned char)*line)) line++;
    return (line[0] == '/' && line[1] == '/');
}

static int parse_dsl_line(const char *line, const char *key, char *value, int val_size)
{
    const char *p = line;
    while (*p && isspace((unsigned char)*p)) p++;
    if (p[0] == '/' && p[1] == '/') return 0;
    size_t klen = strlen(key);
    if (strncmp(p, key, klen) != 0) return 0;
    p += klen;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != ':') return 0;
    p++;
    while (*p && isspace((unsigned char)*p)) p++;
    SNPRINTF_CHECK(value, val_size, "%s", p);
    trim(value);
    int vlen = strlen(value);
    if (vlen >= 2 && value[0] == '"' && value[vlen-1] == '"') {
        memmove(value, value+1, vlen-1);
        value[vlen-2] = '\0';
    }
    return 1;
}

int dsl_load_rule_file(const char *path)
{
    if (dsl_num_rules >= MAX_DSL_RULES) return 0;

    FILE *f = fopen(path, "r");
    if (!f) return 0;

    DslRawRule raw;
    memset(&raw, 0, sizeof(raw));

    char line[DSL_LINE_SIZE];
    int in_code_block = 0;
    int code_line_count = 0;

    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (line[0] == '\0') continue;

        if (strcmp(line, "code:") == 0 || strcmp(line, "code:|") == 0) {
            in_code_block = 1;
            code_line_count = 0;
            continue;
        }
        if (strcmp(line, "init:") == 0 || strcmp(line, "init:|") == 0) {
            in_code_block = 2;
            code_line_count = 0;
            continue;
        }
        if (strcmp(line, "cleanup:") == 0 || strcmp(line, "cleanup:|") == 0) {
            in_code_block = 3;
            code_line_count = 0;
            continue;
        }

        if (in_code_block) {
            if (is_dsl_comment(line)) {
                if (in_code_block == 1) {
                    strncat(raw.code_lines, line, sizeof(raw.code_lines) - strlen(raw.code_lines) - 1);
                    strncat(raw.code_lines, "\n", sizeof(raw.code_lines) - strlen(raw.code_lines) - 1);
                } else if (in_code_block == 2) {
                    strncat(raw.init_code_lines, line, sizeof(raw.init_code_lines) - strlen(raw.init_code_lines) - 1);
                    strncat(raw.init_code_lines, "\n", sizeof(raw.init_code_lines) - strlen(raw.init_code_lines) - 1);
                } else {
                    strncat(raw.cleanup_code_lines, line, sizeof(raw.cleanup_code_lines) - strlen(raw.cleanup_code_lines) - 1);
                    strncat(raw.cleanup_code_lines, "\n", sizeof(raw.cleanup_code_lines) - strlen(raw.cleanup_code_lines) - 1);
                }
                continue;
            }
            if (line[0] == '\0') {
                if (in_code_block == 1) {
                    strncat(raw.code_lines, "\n", sizeof(raw.code_lines) - strlen(raw.code_lines) - 1);
                } else if (in_code_block == 2) {
                    strncat(raw.init_code_lines, "\n", sizeof(raw.init_code_lines) - strlen(raw.init_code_lines) - 1);
                } else {
                    strncat(raw.cleanup_code_lines, "\n", sizeof(raw.cleanup_code_lines) - strlen(raw.cleanup_code_lines) - 1);
                }
                continue;
            }
            if (strncmp(line, "parser:", 7) == 0 ||
                strncmp(line, "token:", 6) == 0 ||
                strncmp(line, "result:", 7) == 0 ||
                strncmp(line, "include:", 8) == 0 ||
                strncmp(line, "include-post:", 13) == 0 ||
                strncmp(line, "var:", 4) == 0 ||
                strncmp(line, "desc:", 5) == 0 ||
                strncmp(line, "array:", 6) == 0 ||
                strncmp(line, "param:", 6) == 0 ||
                strncmp(line, "require:", 8) == 0 ||
                strncmp(line, "example:", 8) == 0 ||
                strncmp(line, "category:", 9) == 0 ||
                strncmp(line, "version:", 8) == 0 ||
                strncmp(line, "complexity:", 11) == 0 ||
                strncmp(line, "alias:", 6) == 0 ||
                strncmp(line, "test:", 5) == 0 ||
                strncmp(line, "help:", 5) == 0 ||
                strncmp(line, "validate:", 9) == 0 ||
                strncmp(line, "compose:", 8) == 0 ||
                (strncmp(line, "//", 2) == 0 && line[2] != '/' && code_line_count == 0)) {
                in_code_block = 0;
            } else {
                if (in_code_block == 1) {
                    strncat(raw.code_lines, line, sizeof(raw.code_lines) - strlen(raw.code_lines) - 1);
                    strncat(raw.code_lines, "\n", sizeof(raw.code_lines) - strlen(raw.code_lines) - 1);
                } else if (in_code_block == 2) {
                    strncat(raw.init_code_lines, line, sizeof(raw.init_code_lines) - strlen(raw.init_code_lines) - 1);
                    strncat(raw.init_code_lines, "\n", sizeof(raw.init_code_lines) - strlen(raw.init_code_lines) - 1);
                } else {
                    strncat(raw.cleanup_code_lines, line, sizeof(raw.cleanup_code_lines) - strlen(raw.cleanup_code_lines) - 1);
                    strncat(raw.cleanup_code_lines, "\n", sizeof(raw.cleanup_code_lines) - strlen(raw.cleanup_code_lines) - 1);
                }
                code_line_count++;
                continue;
            }
        }

        if (!in_code_block) {
            char val[4096];
            if (parse_dsl_line(line, "parser", val, sizeof(val))) {
                SNPRINTF_CHECK(raw.parser_keywords, sizeof(raw.parser_keywords), "%.255s", val);
            } else if (parse_dsl_line(line, "token", val, sizeof(val))) {
                strncat(raw.token_decl, val, sizeof(raw.token_decl) - strlen(raw.token_decl) - 1);
                strncat(raw.token_decl, "\n", sizeof(raw.token_decl) - strlen(raw.token_decl) - 1);
            } else if (parse_dsl_line(line, "result", val, sizeof(val))) {
                SNPRINTF_CHECK(raw.result_decl, sizeof(raw.result_decl), "%.255s", val);
            } else if (parse_dsl_line(line, "include", val, sizeof(val))) {
                strncat(raw.include_list, val, sizeof(raw.include_list) - strlen(raw.include_list) - 1);
                strncat(raw.include_list, "\n", sizeof(raw.include_list) - strlen(raw.include_list) - 1);
            } else if (parse_dsl_line(line, "include-post", val, sizeof(val))) {
                strncat(raw.include_post_list, val, sizeof(raw.include_post_list) - strlen(raw.include_post_list) - 1);
                strncat(raw.include_post_list, "\n", sizeof(raw.include_post_list) - strlen(raw.include_post_list) - 1);
            } else if (parse_dsl_line(line, "var", val, sizeof(val))) {
                strncat(raw.var_decls, val, sizeof(raw.var_decls) - strlen(raw.var_decls) - 1);
                strncat(raw.var_decls, "\n", sizeof(raw.var_decls) - strlen(raw.var_decls) - 1);
            } else if (parse_dsl_line(line, "desc", val, sizeof(val))) {
                SNPRINTF_CHECK(raw.desc_text, sizeof(raw.desc_text), "%.511s", val);
            } else if (parse_dsl_line(line, "match", val, sizeof(val))) {
                SNPRINTF_CHECK(raw.match_text, sizeof(raw.match_text), "%.1023s", val);
            } else if (parse_dsl_line(line, "array", val, sizeof(val))) {
                char aname[64], aidx[64], atyp[32];
                if (sscanf(val, "%63s %63s %31s", aname, aidx, atyp) >= 2) {
                    SNPRINTF_CHECK(raw.array_rule_name, sizeof(raw.array_rule_name), "%s", aname);
                    SNPRINTF_CHECK(raw.array_rule_index, sizeof(raw.array_rule_index), "%s", aidx);
                    if (strlen(atyp) > 0)
                        SNPRINTF_CHECK(raw.array_rule_elem_type, sizeof(raw.array_rule_elem_type), "%s", atyp);
                    else
                        SNPRINTF_CHECK(raw.array_rule_elem_type, sizeof(raw.array_rule_elem_type), "int64");
                }
            } else if (parse_dsl_line(line, "param", val, sizeof(val))) {
                strncat(raw.param_decls, val, sizeof(raw.param_decls) - strlen(raw.param_decls) - 1);
                strncat(raw.param_decls, "\n", sizeof(raw.param_decls) - strlen(raw.param_decls) - 1);
            } else if (parse_dsl_line(line, "require", val, sizeof(val))) {
                strncat(raw.include_list, val, sizeof(raw.include_list) - strlen(raw.include_list) - 1);
                strncat(raw.include_list, "\n", sizeof(raw.include_list) - strlen(raw.include_list) - 1);
            } else if (parse_dsl_line(line, "example", val, sizeof(val))) {
                strncat(raw.example_lines, val, sizeof(raw.example_lines) - strlen(raw.example_lines) - 1);
                strncat(raw.example_lines, "\n", sizeof(raw.example_lines) - strlen(raw.example_lines) - 1);
            } else if (parse_dsl_line(line, "category", val, sizeof(val))) {
                SNPRINTF_CHECK(raw.category_text, sizeof(raw.category_text), "%.255s", val);
            } else if (parse_dsl_line(line, "version", val, sizeof(val))) {
                SNPRINTF_CHECK(raw.version_text, sizeof(raw.version_text), "%.31s", val);
            } else if (parse_dsl_line(line, "complexity", val, sizeof(val))) {
                SNPRINTF_CHECK(raw.complexity_text, sizeof(raw.complexity_text), "%.31s", val);
            } else if (parse_dsl_line(line, "alias", val, sizeof(val))) {
                strncat(raw.alias_text, val, sizeof(raw.alias_text) - strlen(raw.alias_text) - 1);
                strncat(raw.alias_text, "\n", sizeof(raw.alias_text) - strlen(raw.alias_text) - 1);
            } else if (parse_dsl_line(line, "test", val, sizeof(val))) {
                strncat(raw.test_lines, val, sizeof(raw.test_lines) - strlen(raw.test_lines) - 1);
                strncat(raw.test_lines, "\n", sizeof(raw.test_lines) - strlen(raw.test_lines) - 1);
            } else if (parse_dsl_line(line, "help", val, sizeof(val))) {
                SNPRINTF_CHECK(raw.help_text, sizeof(raw.help_text), "%.2047s", val);
            } else if (parse_dsl_line(line, "validate", val, sizeof(val))) {
                strncat(raw.validate_text, val, sizeof(raw.validate_text) - strlen(raw.validate_text) - 1);
                strncat(raw.validate_text, "\n", sizeof(raw.validate_text) - strlen(raw.validate_text) - 1);
            } else if (parse_dsl_line(line, "compose", val, sizeof(val))) {
                strncat(raw.compose_text, val, sizeof(raw.compose_text) - strlen(raw.compose_text) - 1);
                strncat(raw.compose_text, "\n", sizeof(raw.compose_text) - strlen(raw.compose_text) - 1);
            }
        }
    }
    fclose(f);

    DslRule *rule = &dsl_rules[dsl_num_rules];
    memset(rule, 0, sizeof(DslRule));
    SNPRINTF_CHECK(rule->filename, sizeof(rule->filename), "%s", path);

    if (!dsl_parse_raw(&raw, rule)) return 0;

    dsl_num_rules++;
    return 1;
}

int dsl_parse_raw(DslRawRule *raw, DslRule *rule)
{
    SNPRINTF_CHECK(rule->desc, sizeof(rule->desc), "%s", raw->desc_text);

    if (strlen(raw->match_text) > 0) {
        char mcopy[1024];
        SNPRINTF_CHECK(mcopy, sizeof(mcopy), "%s", raw->match_text);
        char *mctx = NULL;
        char *mtok = strtok_r(mcopy, ", ", &mctx);
        while (mtok && rule->num_match_flags < MAX_DSL_TOKENS) {
            trim(mtok);
            if (strlen(mtok) > 0) {
                SNPRINTF_CHECK(rule->match_flags[rule->num_match_flags], sizeof(rule->match_flags[0]), "%s", mtok);
                rule->num_match_flags++;
            }
            mtok = strtok_r(NULL, ", ", &mctx);
        }
    }

    if (strlen(raw->array_rule_name) > 0) {
        rule->has_array_rule = 1;
        SNPRINTF_CHECK(rule->array_name, sizeof(rule->array_name), "%s", raw->array_rule_name);
        SNPRINTF_CHECK(rule->array_index, sizeof(rule->array_index), "%s", raw->array_rule_index);
        SNPRINTF_CHECK(rule->array_element_type, sizeof(rule->array_element_type), "%s", raw->array_rule_elem_type);
    }

    char keyword_buf[512];
    SNPRINTF_CHECK(keyword_buf, sizeof(keyword_buf), "%s", raw->parser_keywords);
    char *comma = strchr(keyword_buf, ',');
    if (comma) {
        *comma = '\0';
        char *k = keyword_buf;
        trim(k);
        SNPRINTF_CHECK(rule->keyword, sizeof(rule->keyword), "%.63s", k);
    } else {
        SNPRINTF_CHECK(rule->keyword, sizeof(rule->keyword), "%.63s", keyword_buf);
    }

    char *tlines[32];
    int nt = 0;
    char tbuf[1024];
    SNPRINTF_CHECK(tbuf, sizeof(tbuf), "%s", raw->token_decl);
    char *saveptr_t;
    char *tline = strtok_r(tbuf, "\n", &saveptr_t);
    while (tline && nt < 32) {
        trim(tline);
        if (strlen(tline) > 0) {
            tlines[nt] = tline;
            nt++;
        }
        tline = strtok_r(NULL, "\n", &saveptr_t);
    }
    for (int i = 0; i < nt && rule->num_tokens < MAX_DSL_TOKENS; i++) {
        char *comma_pos = tlines[i];
        char *token_start = comma_pos;
        while (*comma_pos) {
            if (*comma_pos == ',') {
                *comma_pos = '\0';
                trim(token_start);
                if (strlen(token_start) > 0) {
                    parse_token_decl(token_start, &rule->tokens[rule->num_tokens]);
                    rule->num_tokens++;
                }
                comma_pos++;
                token_start = comma_pos;
            } else {
                comma_pos++;
            }
        }
        trim(token_start);
        if (strlen(token_start) > 0 && rule->num_tokens < MAX_DSL_TOKENS) {
            parse_token_decl(token_start, &rule->tokens[rule->num_tokens]);
            rule->num_tokens++;
        }
    }

    if (strlen(raw->result_decl) > 0) {
        rule->has_result = 1;
        parse_token_decl(raw->result_decl, &rule->result);
    }

    char *ilines[16];
    int ni = 0;
    char ibuf[1024];
    SNPRINTF_CHECK(ibuf, sizeof(ibuf), "%s", raw->include_list);
    char *saveptr_i;
    char *iline = strtok_r(ibuf, "\n", &saveptr_i);
    while (iline && ni < 16) {
        trim(iline);
        if (strlen(iline) > 0) {
            ilines[ni] = iline;
            ni++;
        }
        iline = strtok_r(NULL, "\n", &saveptr_i);
    }
    for (int i = 0; i < ni && rule->num_includes < MAX_DSL_INCLUDES; i++) {
        SNPRINTF_CHECK(rule->includes[rule->num_includes], sizeof(rule->includes[0]), "%s", ilines[i]);
        rule->num_includes++;
    }

    char *ipost_lines[16];
    int nip = 0;
    char ipost_buf[1024];
    SNPRINTF_CHECK(ipost_buf, sizeof(ipost_buf), "%s", raw->include_post_list);
    char *saveptr_ip;
    char *ipost_line = strtok_r(ipost_buf, "\n", &saveptr_ip);
    while (ipost_line && nip < 16) {
        trim(ipost_line);
        if (strlen(ipost_line) > 0) {
            ipost_lines[nip] = ipost_line;
            nip++;
        }
        ipost_line = strtok_r(NULL, "\n", &saveptr_ip);
    }
    for (int i = 0; i < nip && rule->num_includes_post < MAX_DSL_INCLUDES_POST; i++) {
        SNPRINTF_CHECK(rule->includes_post[rule->num_includes_post], sizeof(rule->includes_post[0]), "%s", ipost_lines[i]);
        rule->num_includes_post++;
    }

    char *vlines[32];
    int nv = 0;
    char vbuf[2048];
    SNPRINTF_CHECK(vbuf, sizeof(vbuf), "%s", raw->var_decls);
    char *saveptr_v;
    char *vline = strtok_r(vbuf, "\n", &saveptr_v);
    while (vline && nv < 32) {
        trim(vline);
        if (strlen(vline) > 0) {
            vlines[nv] = vline;
            nv++;
        }
        vline = strtok_r(NULL, "\n", &saveptr_v);
    }
    for (int i = 0; i < nv && rule->num_vars < MAX_DSL_VARS; i++) {
        parse_var_decl(vlines[i], &rule->vars[rule->num_vars]);
        rule->num_vars++;
    }

    char *clines[MAX_DSL_CODE_LINES];
    int nc = 0;
    char cbuf[4096];
    SNPRINTF_CHECK(cbuf, sizeof(cbuf), "%s", raw->code_lines);
    char *saveptr_c;
    char *cline = strtok_r(cbuf, "\n", &saveptr_c);
    while (cline && nc < MAX_DSL_CODE_LINES) {
        trim(cline);
        if (strlen(cline) > 0) {
            clines[nc] = cline;
            nc++;
        }
        cline = strtok_r(NULL, "\n", &saveptr_c);
    }
    for (int i = 0; i < nc && rule->num_code_lines < MAX_DSL_CODE_LINES; i++) {
        SNPRINTF_CHECK(rule->code[rule->num_code_lines], sizeof(rule->code[0]), "%s", clines[i]);
        rule->num_code_lines++;
    }

    if (strlen(raw->param_decls) > 0) {
        char pbuf[4096];
        SNPRINTF_CHECK(pbuf, sizeof(pbuf), "%s", raw->param_decls);
        char *saveptr_p;
        char *pline = strtok_r(pbuf, "\n", &saveptr_p);
        while (pline && rule->num_params < MAX_DSL_PARAMS) {
            trim(pline);
            if (strlen(pline) > 0) {
                parse_param_decl(pline, &rule->params[rule->num_params]);
                rule->num_params++;
            }
            pline = strtok_r(NULL, "\n", &saveptr_p);
        }
    }

    if (strlen(raw->alias_text) > 0) {
        char abuf[1024];
        SNPRINTF_CHECK(abuf, sizeof(abuf), "%s", raw->alias_text);
        char *saveptr_a;
        char *atok = strtok_r(abuf, ",\n", &saveptr_a);
        while (atok && rule->num_aliases < MAX_DSL_ALIASES) {
            trim(atok);
            if (strlen(atok) > 0) {
                SNPRINTF_CHECK(rule->aliases[rule->num_aliases], sizeof(rule->aliases[0]), "%s", atok);
                rule->num_aliases++;
            }
            atok = strtok_r(NULL, ",\n", &saveptr_a);
        }
    }

    if (strlen(raw->category_text) > 0)
        SNPRINTF_CHECK(rule->category, sizeof(rule->category), "%s", raw->category_text);
    if (strlen(raw->version_text) > 0)
        SNPRINTF_CHECK(rule->version, sizeof(rule->version), "%s", raw->version_text);
    if (strlen(raw->complexity_text) > 0)
        SNPRINTF_CHECK(rule->complexity, sizeof(rule->complexity), "%s", raw->complexity_text);
    if (strlen(raw->help_text) > 0)
        SNPRINTF_CHECK(rule->help, sizeof(rule->help), "%s", raw->help_text);

    if (strlen(raw->example_lines) > 0) {
        char ebuf[4096];
        SNPRINTF_CHECK(ebuf, sizeof(ebuf), "%s", raw->example_lines);
        char *saveptr_e;
        char *eline = strtok_r(ebuf, "\n", &saveptr_e);
        while (eline && rule->num_examples < MAX_DSL_EXAMPLES) {
            trim(eline);
            if (strlen(eline) > 0) {
                parse_test_decl(eline, (DslTest *)&rule->examples[rule->num_examples]);
                rule->num_examples++;
            }
            eline = strtok_r(NULL, "\n", &saveptr_e);
        }
    }

    if (strlen(raw->test_lines) > 0) {
        char tbuf2[4096];
        SNPRINTF_CHECK(tbuf2, sizeof(tbuf2), "%s", raw->test_lines);
        char *saveptr_t2;
        char *tline2 = strtok_r(tbuf2, "\n", &saveptr_t2);
        while (tline2 && rule->num_tests < MAX_DSL_TESTS) {
            trim(tline2);
            if (strlen(tline2) > 0) {
                parse_test_decl(tline2, &rule->tests[rule->num_tests]);
                rule->num_tests++;
            }
            tline2 = strtok_r(NULL, "\n", &saveptr_t2);
        }
    }

    if (strlen(raw->validate_text) > 0) {
        char vbuf2[1024];
        SNPRINTF_CHECK(vbuf2, sizeof(vbuf2), "%s", raw->validate_text);
        char *saveptr_v2;
        char *vtok = strtok_r(vbuf2, ",\n", &saveptr_v2);
        while (vtok && rule->num_validate < MAX_DSL_VALIDATE) {
            trim(vtok);
            if (strlen(vtok) > 0) {
                SNPRINTF_CHECK(rule->validate_names[rule->num_validate], sizeof(rule->validate_names[0]), "%s", vtok);
                rule->num_validate++;
            }
            vtok = strtok_r(NULL, ",\n", &saveptr_v2);
        }
    }

    if (strlen(raw->compose_text) > 0) {
        char cobuf[1024];
        SNPRINTF_CHECK(cobuf, sizeof(cobuf), "%s", raw->compose_text);
        char *saveptr_co;
        char *cotok = strtok_r(cobuf, ",\n", &saveptr_co);
        while (cotok && rule->num_compose < MAX_DSL_COMPOSE) {
            trim(cotok);
            if (strlen(cotok) > 0) {
                SNPRINTF_CHECK(rule->compose_names[rule->num_compose], sizeof(rule->compose_names[0]), "%s", cotok);
                rule->num_compose++;
            }
            cotok = strtok_r(NULL, ",\n", &saveptr_co);
        }
    }

    if (strlen(raw->init_code_lines) > 0) {
        char *ilines2[MAX_DSL_CODE_LINES];
        int ni2 = 0;
        char ibuf2[4096];
        SNPRINTF_CHECK(ibuf2, sizeof(ibuf2), "%s", raw->init_code_lines);
        char *saveptr_i2;
        char *iline2 = strtok_r(ibuf2, "\n", &saveptr_i2);
        while (iline2 && ni2 < MAX_DSL_CODE_LINES) {
            trim(iline2);
            if (strlen(iline2) > 0) {
                ilines2[ni2] = iline2;
                ni2++;
            }
            iline2 = strtok_r(NULL, "\n", &saveptr_i2);
        }
        for (int i = 0; i < ni2 && rule->num_init_lines < MAX_DSL_CODE_LINES; i++) {
            SNPRINTF_CHECK(rule->init_code[rule->num_init_lines], sizeof(rule->init_code[0]), "%s", ilines2[i]);
            rule->num_init_lines++;
        }
    }

    if (strlen(raw->cleanup_code_lines) > 0) {
        char *clines2[MAX_DSL_CODE_LINES];
        int nc2 = 0;
        char cbuf2[4096];
        SNPRINTF_CHECK(cbuf2, sizeof(cbuf2), "%s", raw->cleanup_code_lines);
        char *saveptr_c2;
        char *cline2 = strtok_r(cbuf2, "\n", &saveptr_c2);
        while (cline2 && nc2 < MAX_DSL_CODE_LINES) {
            trim(cline2);
            if (strlen(cline2) > 0) {
                clines2[nc2] = cline2;
                nc2++;
            }
            cline2 = strtok_r(NULL, "\n", &saveptr_c2);
        }
        for (int i = 0; i < nc2 && rule->num_cleanup_lines < MAX_DSL_CODE_LINES; i++) {
            SNPRINTF_CHECK(rule->cleanup_code[rule->num_cleanup_lines], sizeof(rule->cleanup_code[0]), "%s", clines2[i]);
            rule->num_cleanup_lines++;
        }
    }

    return 1;
}

int dsl_load_rules(const char *dir_path)
{
    DIR *d = opendir(dir_path);
    if (!d) return 0;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        const char *name = entry->d_name;
        size_t len = strlen(name);
        if (len > 6 && strcmp(name + len - 6, ".l1dsl") == 0) {
            char full_path[1024];
            SNPRINTF_CHECK(full_path, sizeof(full_path), "%s/%s", dir_path, name);
            dsl_load_rule_file(full_path);
        }
    }
    closedir(d);
    return dsl_num_rules;
}

int dsl_match_all_rules(const char *prompt, DslRule **rules, float *scores, int max_rules, float min_score)
{
    if (dsl_num_rules == 0 || max_rules <= 0) return 0;

    char prompt_lower[1024];
    SNPRINTF_CHECK(prompt_lower, sizeof(prompt_lower), "%s", prompt);
    for (int i = 0; prompt_lower[i]; i++)
        prompt_lower[i] = tolower((unsigned char)prompt_lower[i]);

    int count = 0;

    for (int i = 0; i < dsl_num_rules && count < max_rules; i++) {
        char keyword_lower[256];
        SNPRINTF_CHECK(keyword_lower, sizeof(keyword_lower), "%.255s", dsl_rules[i].keyword);
        for (int j = 0; keyword_lower[j]; j++)
            keyword_lower[j] = tolower((unsigned char)keyword_lower[j]);

        float match = 0.0f;
        int num_keywords = 0;

        char *saveptr;
        char kw_copy[256];
        SNPRINTF_CHECK(kw_copy, sizeof(kw_copy), "%s", keyword_lower);
        char *kw = strtok_r(kw_copy, ",", &saveptr);
        while (kw) {
            while (*kw == ' ') kw++;
            char *end = kw + strlen(kw) - 1;
            while (end > kw && *end == ' ') end--;
            *(end+1) = '\0';
            if (strlen(kw) > 0) {
                num_keywords++;
                if (strstr(prompt_lower, kw))
                    match += 1.0f;
            }
            kw = strtok_r(NULL, ",", &saveptr);
        }

        int min_match = (num_keywords >= 3) ? 2 : 1;
        if (match >= (float)min_match) {
            float word_ratio = match;
            int prompt_words = 1;
            for (int j = 0; prompt_lower[j]; j++)
                if (prompt_lower[j] == ' ') prompt_words++;
            if (prompt_words > 0)
                word_ratio = match / (float)prompt_words;

            float score_val = match + word_ratio * 0.5f;
            if (score_val >= min_score) {
                rules[count] = &dsl_rules[i];
                scores[count] = score_val;
                count++;
            }
        }
    }
    return count;
}

int dsl_match_rule(const char *prompt, DslRule **rule, float *score)
{
    DslRule *best_rule = NULL;
    float best_score = 0.0f;
    int count = dsl_match_all_rules(prompt, &best_rule, &best_score, 1, 0.0f);
    if (count > 0 && best_rule) {
        *rule = best_rule;
        *score = best_score;
        return 1;
    }
    return 0;
}

static void substitute_template(char *out, int out_size, const char *template_str,
                                DslRule *rule, const char *arg1, const char *arg2)
{
    char result[4096];
    result[0] = '\0';

    const char *p = template_str;
    while (*p) {
        const char *brace = strchr(p, '{');
        if (!brace) {
            strncat(result, p, sizeof(result) - strlen(result) - 1);
            break;
        }
        size_t before_len = (size_t)(brace - p);
        if (before_len > 0) {
            size_t avail = sizeof(result) - strlen(result) - 1;
            strncat(result, p, before_len < avail ? before_len : avail);
        }
        p = brace + 1;
        const char *close = strchr(p, '}');
        if (!close) {
            strncat(result, "{", sizeof(result) - strlen(result) - 1);
            strncat(result, p, sizeof(result) - strlen(result) - 1);
            break;
        }
        char varname[128];
        size_t vlen = (size_t)(close - p);
        if (vlen >= sizeof(varname)) vlen = sizeof(varname) - 1;
        strncpy(varname, p, vlen);
        varname[vlen] = '\0';

        if (strcmp(varname, "result") == 0 && rule->has_result) {
            strncat(result, rule->result.name, sizeof(result) - strlen(result) - 1);
        } else if (strcmp(varname, "arg1") == 0 && arg1) {
            strncat(result, arg1, sizeof(result) - strlen(result) - 1);
        } else if (strcmp(varname, "arg2") == 0 && arg2) {
            strncat(result, arg2, sizeof(result) - strlen(result) - 1);
        } else {
            int found = 0;
            for (int i = 0; i < rule->num_tokens; i++) {
                if (strcmp(rule->tokens[i].name, varname) == 0) {
                    strncat(result, rule->tokens[i].name, sizeof(result) - strlen(result) - 1);
                    found = 1;
                    break;
                }
            }
            if (!found && rule->has_result && strcmp(rule->result.name, varname) == 0) {
                strncat(result, rule->result.name, sizeof(result) - strlen(result) - 1);
            }
        }
        p = close + 1;
    }

    SNPRINTF_CHECK(out, out_size, "%s", result);
}

int dsl_generate_code(Program *prog, DslRule *rule, L1vmFunction *f)
{
    if (!rule || !prog || !f) return 0;

    for (int i = 0; i < rule->num_includes; i++) {
        add_include(prog, rule->includes[i]);
    }

    for (int i = 0; i < rule->num_includes_post; i++) {
        add_include_post(prog, rule->includes_post[i]);
    }

    for (int i = 0; i < rule->num_vars; i++) {
        DslVarDecl *vd = &rule->vars[i];
        int count = vd->count;
        if (count < 1) count = 1;
        const char *values[1];
        char default_val[256];
        if (strlen(vd->value) > 0) {
            SNPRINTF_CHECK(default_val, sizeof(default_val), "%s", vd->value);
        } else {
            SNPRINTF_CHECK(default_val, sizeof(default_val), "%s",
                (strstr(vd->type, "int64") || strstr(vd->type, "const-int64")) ? "0" :
                (strstr(vd->type, "double") || strstr(vd->type, "const-double")) ? "0.0" : "\"\"");
        }
        values[0] = default_val;

        char *type_ptr = vd->type;
        int array_count = count;

        add_var_to_func(f, type_ptr, vd->name, array_count, values, 1);
    }

    for (int i = 0; i < rule->num_tokens; i++) {
        DslToken *tok = &rule->tokens[i];
        int count = tok->is_array ? tok->array_size : 1;
        const char *tv[1];
        char td[256];
        if (strlen(tok->default_value) > 0) {
            SNPRINTF_CHECK(td, sizeof(td), "%s", tok->default_value);
        } else {
            switch (tok->type) {
                case DSL_TOKEN_CONST_INT64:
                case DSL_TOKEN_INT64: SNPRINTF_CHECK(td, sizeof(td), "0"); break;
                case DSL_TOKEN_CONST_DOUBLE:
                case DSL_TOKEN_DOUBLE: SNPRINTF_CHECK(td, sizeof(td), "0.0"); break;
                case DSL_TOKEN_CONST_STRING:
                case DSL_TOKEN_STRING: SNPRINTF_CHECK(td, sizeof(td), "\"\""); break;
                default: SNPRINTF_CHECK(td, sizeof(td), "0"); break;
            }
        }
        tv[0] = td;
        add_var_to_func(f, tok->l1vm_type, tok->name, count, tv, 1);
    }

    if (rule->has_result) {
        DslToken *res = &rule->result;
        int count = res->is_array ? res->array_size : 1;
        const char *rv[1];
        char rd[64];
        switch (res->type) {
            case DSL_TOKEN_CONST_INT64:
            case DSL_TOKEN_INT64: SNPRINTF_CHECK(rd, sizeof(rd), "0"); break;
            case DSL_TOKEN_CONST_DOUBLE:
            case DSL_TOKEN_DOUBLE: SNPRINTF_CHECK(rd, sizeof(rd), "0.0"); break;
            case DSL_TOKEN_CONST_STRING:
            case DSL_TOKEN_STRING: SNPRINTF_CHECK(rd, sizeof(rd), "\"\""); break;
            default: SNPRINTF_CHECK(rd, sizeof(rd), "0"); break;
        }
        rv[0] = rd;
        add_var_to_func(f, res->l1vm_type, res->name, count, rv, 1);
    }

    for (int i = 0; i < rule->num_init_lines; i++) {
        char substituted[4096];
        substitute_template(substituted, sizeof(substituted),
            rule->init_code[i], rule, NULL, NULL);
        char indented[4100];
        SNPRINTF_CHECK(indented, sizeof(indented), "\t%s", substituted);
        func_append(f, indented);
    }

    for (int i = 0; i < rule->num_code_lines; i++) {
        char substituted[4096];
        substitute_template(substituted, sizeof(substituted),
            rule->code[i], rule, NULL, NULL);

        char indented[4100];
        SNPRINTF_CHECK(indented, sizeof(indented), "\t%s", substituted);

        // Skip duplicate (set ...) lines already in body (common in matching DSL rules)
        if (strncmp(substituted, "(set ", 5) == 0 && strstr(f->body, substituted) != NULL)
            continue;
        func_append(f, indented);
    }

    for (int i = 0; i < rule->num_cleanup_lines; i++) {
        char substituted[4096];
        substitute_template(substituted, sizeof(substituted),
            rule->cleanup_code[i], rule, NULL, NULL);
        char indented[4100];
        SNPRINTF_CHECK(indented, sizeof(indented), "\t%s", substituted);
        func_append(f, indented);
    }

    return 1;
}

int dsl_add_array_rule(const char *name, const char *index_var, const char *element_type)
{
    if (dsl_num_rules >= MAX_DSL_RULES) return 0;

    DslRule *rule = &dsl_rules[dsl_num_rules];
    memset(rule, 0, sizeof(DslRule));

    rule->has_array_rule = 1;
    SNPRINTF_CHECK(rule->array_name, sizeof(rule->array_name), "%s", name);
    SNPRINTF_CHECK(rule->array_index, sizeof(rule->array_index), "%s", index_var);
    SNPRINTF_CHECK(rule->array_element_type, sizeof(rule->array_element_type), "%s", element_type);
    SNPRINTF_CHECK(rule->keyword, sizeof(rule->keyword), "array %s", name);
    SNPRINTF_CHECK(rule->desc, sizeof(rule->desc), "Array access rule for '%s' with index '%s' type '%s'",
             name, index_var, element_type);
    rule->is_learned = 1;

    SNPRINTF_CHECK(rule->includes[rule->num_includes], sizeof(rule->includes[0]), "intr-func.l1h");
    rule->num_includes++;
    SNPRINTF_CHECK(rule->includes[rule->num_includes], sizeof(rule->includes[0]), "vars.l1h");
    rule->num_includes++;

    char size_var[32];
    SNPRINTF_CHECK(size_var, sizeof(size_var), "%s_size", element_type);

    DslVarDecl *vd = &rule->vars[rule->num_vars];
    SNPRINTF_CHECK(vd->type, sizeof(vd->type), "int64");
    SNPRINTF_CHECK(vd->name, sizeof(vd->name), "realind");
    vd->count = 1;
    SNPRINTF_CHECK(vd->value, sizeof(vd->value), "0");
    rule->num_vars++;

    vd = &rule->vars[rule->num_vars];
    SNPRINTF_CHECK(vd->type, sizeof(vd->type), "int64");
    SNPRINTF_CHECK(vd->name, sizeof(vd->name), "f");
    vd->count = 1;
    SNPRINTF_CHECK(vd->value, sizeof(vd->value), "0");
    rule->num_vars++;

    vd = &rule->vars[rule->num_vars];
    SNPRINTF_CHECK(vd->type, sizeof(vd->type), "const-int64");
    SNPRINTF_CHECK(vd->name, sizeof(vd->name), "zero");
    vd->count = 1;
    SNPRINTF_CHECK(vd->value, sizeof(vd->value), "0");
    rule->num_vars++;

    char code_line[DSL_LINE_SIZE];
    SNPRINTF_CHECK(code_line, sizeof(code_line), "\t// array access: %s[%s]", name, index_var);
    SNPRINTF_CHECK(rule->code[rule->num_code_lines], DSL_LINE_SIZE, "%s", code_line);
    rule->num_code_lines++;

    SNPRINTF_CHECK(code_line, sizeof(code_line), "\t(%s * %s realind :=)", index_var, size_var);
    SNPRINTF_CHECK(rule->code[rule->num_code_lines], DSL_LINE_SIZE, "%s", code_line);
    rule->num_code_lines++;

    SNPRINTF_CHECK(code_line, sizeof(code_line), "\t(%s [ realind ] val =)", name);
    SNPRINTF_CHECK(rule->code[rule->num_code_lines], DSL_LINE_SIZE, "%s", code_line);
    rule->num_code_lines++;

    SNPRINTF_CHECK(code_line, sizeof(code_line), "\t(val + zero val :=)");
    SNPRINTF_CHECK(rule->code[rule->num_code_lines], DSL_LINE_SIZE, "%s", code_line);
    rule->num_code_lines++;

    DslVarDecl *result_vd = &rule->vars[rule->num_vars];
    SNPRINTF_CHECK(result_vd->type, sizeof(result_vd->type), "%s", element_type);
    SNPRINTF_CHECK(result_vd->name, sizeof(result_vd->name), "val");
    result_vd->count = 1;
    SNPRINTF_CHECK(result_vd->value, sizeof(result_vd->value),
        strcmp(element_type, "double") == 0 ? "0.0" : "0");
    rule->num_vars++;

    dsl_num_rules++;
    return 1;
}

void dsl_print_rule(DslRule *rule)
{
    if (!rule) return;
    printf("DSL Rule: %s\n", rule->keyword);
    printf("  File: %s\n", rule->filename);
    printf("  Description: %s\n", rule->desc);
    if (strlen(rule->category) > 0)
        printf("  Category: %s\n", rule->category);
    if (strlen(rule->version) > 0)
        printf("  Version: %s\n", rule->version);
    if (strlen(rule->complexity) > 0)
        printf("  Complexity: %s\n", rule->complexity);
    if (strlen(rule->help) > 0)
        printf("  Help: %s\n", rule->help);
    if (rule->num_aliases > 0) {
        printf("  Aliases: ");
        for (int i = 0; i < rule->num_aliases; i++) {
            if (i > 0) printf(", ");
            printf("%s", rule->aliases[i]);
        }
        printf("\n");
    }
    printf("  Tokens: %d\n", rule->num_tokens);
    for (int i = 0; i < rule->num_tokens; i++) {
        printf("    %s: %s\n", dsl_token_type_to_string(rule->tokens[i].type), rule->tokens[i].name);
    }
    if (rule->num_params > 0) {
        printf("  Params: %d\n", rule->num_params);
        for (int i = 0; i < rule->num_params; i++) {
            printf("    %s %s", rule->params[i].type, rule->params[i].name);
            if (strlen(rule->params[i].desc) > 0) printf(" \"%s\"", rule->params[i].desc);
            if (strlen(rule->params[i].min) > 0) printf(" min=%s", rule->params[i].min);
            if (strlen(rule->params[i].max) > 0) printf(" max=%s", rule->params[i].max);
            if (strlen(rule->params[i].default_value) > 0) printf(" default=%s", rule->params[i].default_value);
            if (rule->params[i].required) printf(" required");
            printf("\n");
        }
    }
    if (rule->has_result) {
        printf("  Result: %s %s\n", dsl_token_type_to_string(rule->result.type), rule->result.name);
    }
    printf("  Includes: %d\n", rule->num_includes);
    for (int i = 0; i < rule->num_includes; i++)
        printf("    %s\n", rule->includes[i]);
    printf("  Includes-post: %d\n", rule->num_includes_post);
    printf("  Vars: %d\n", rule->num_vars);
    if (rule->has_array_rule) {
        printf("  Array rule: %s[%s] type=%s\n", rule->array_name, rule->array_index, rule->array_element_type);
    }
    if (rule->num_examples > 0) {
        printf("  Examples: %d\n", rule->num_examples);
        for (int i = 0; i < rule->num_examples; i++) {
            printf("    \"%s\"", rule->examples[i].prompt);
            if (strlen(rule->examples[i].expected) > 0)
                printf(" expect: \"%s\"", rule->examples[i].expected);
            printf("\n");
        }
    }
    if (rule->num_tests > 0) {
        printf("  Tests: %d\n", rule->num_tests);
        for (int i = 0; i < rule->num_tests; i++) {
            printf("    \"%s\" expect: \"%s\"\n", rule->tests[i].input, rule->tests[i].expect);
        }
    }
    if (rule->num_validate > 0) {
        printf("  Validate: ");
        for (int i = 0; i < rule->num_validate; i++) {
            if (i > 0) printf(", ");
            printf("%s", rule->validate_names[i]);
        }
        printf("\n");
    }
    if (rule->num_compose > 0) {
        printf("  Compose: ");
        for (int i = 0; i < rule->num_compose; i++) {
            if (i > 0) printf(", ");
            printf("%s", rule->compose_names[i]);
        }
        printf("\n");
    }
    printf("  Init code lines: %d\n", rule->num_init_lines);
    printf("  Code lines: %d\n", rule->num_code_lines);
    printf("  Cleanup code lines: %d\n", rule->num_cleanup_lines);
}

int dsl_save_rule(DslRule *rule, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f) return 0;

    fprintf(f, "// %s\n", rule->keyword);
    fprintf(f, "parser: \"%s\"\n", rule->keyword);

    if (rule->num_aliases > 0) {
        fprintf(f, "alias: \"");
        for (int i = 0; i < rule->num_aliases; i++) {
            if (i > 0) fprintf(f, ", ");
            fprintf(f, "%s", rule->aliases[i]);
        }
        fprintf(f, "\"\n");
    }

    fprintf(f, "desc: \"%s\"\n", rule->desc);

    if (strlen(rule->category) > 0)
        fprintf(f, "category: %s\n", rule->category);
    if (strlen(rule->version) > 0)
        fprintf(f, "version: %s\n", rule->version);
    if (strlen(rule->complexity) > 0)
        fprintf(f, "complexity: %s\n", rule->complexity);
    if (strlen(rule->help) > 0)
        fprintf(f, "help: \"%s\"\n", rule->help);

    if (rule->num_match_flags > 0) {
        fprintf(f, "match: ");
        for (int i = 0; i < rule->num_match_flags; i++) {
            if (i > 0) fprintf(f, ", ");
            fprintf(f, "%s", rule->match_flags[i]);
        }
        fprintf(f, "\n");
    }

    if (rule->num_tokens > 0) {
        fprintf(f, "token: ");
        for (int i = 0; i < rule->num_tokens; i++) {
            if (i > 0) fprintf(f, ", ");
            fprintf(f, "%s %s", dsl_token_type_to_string(rule->tokens[i].type), rule->tokens[i].name);
        }
        fprintf(f, "\n");
    }

    for (int i = 0; i < rule->num_params; i++) {
        fprintf(f, "param: %s %s", rule->params[i].type, rule->params[i].name);
        if (strlen(rule->params[i].desc) > 0)
            fprintf(f, " desc=\"%s\"", rule->params[i].desc);
        if (strlen(rule->params[i].min) > 0)
            fprintf(f, " min=%s", rule->params[i].min);
        if (strlen(rule->params[i].max) > 0)
            fprintf(f, " max=%s", rule->params[i].max);
        if (strlen(rule->params[i].default_value) > 0)
            fprintf(f, " default=%s", rule->params[i].default_value);
        if (strlen(rule->params[i].pattern) > 0)
            fprintf(f, " pattern=%s", rule->params[i].pattern);
        if (rule->params[i].required)
            fprintf(f, " required");
        fprintf(f, "\n");
    }

    if (rule->has_result) {
        fprintf(f, "result: %s %s\n", dsl_token_type_to_string(rule->result.type), rule->result.name);
    }

    for (int i = 0; i < rule->num_includes; i++)
        fprintf(f, "include: %s\n", rule->includes[i]);

    for (int i = 0; i < rule->num_includes_post; i++)
        fprintf(f, "include-post: %s\n", rule->includes_post[i]);

    for (int i = 0; i < rule->num_vars; i++) {
        fprintf(f, "var: %s %s %d", rule->vars[i].type, rule->vars[i].name, rule->vars[i].count);
        if (strlen(rule->vars[i].value) > 0)
            fprintf(f, " %s", rule->vars[i].value);
        fprintf(f, "\n");
    }

    if (rule->has_array_rule) {
        fprintf(f, "array: %s %s %s\n", rule->array_name, rule->array_index, rule->array_element_type);
    }

    for (int i = 0; i < rule->num_examples; i++) {
        fprintf(f, "example: \"%s\"", rule->examples[i].prompt);
        if (strlen(rule->examples[i].expected) > 0)
            fprintf(f, " expect: \"%s\"", rule->examples[i].expected);
        fprintf(f, "\n");
    }

    for (int i = 0; i < rule->num_tests; i++) {
        fprintf(f, "test: \"%s\"", rule->tests[i].input);
        if (strlen(rule->tests[i].expect) > 0)
            fprintf(f, " expect: \"%s\"", rule->tests[i].expect);
        fprintf(f, "\n");
    }

    if (rule->num_validate > 0) {
        fprintf(f, "validate: ");
        for (int i = 0; i < rule->num_validate; i++) {
            if (i > 0) fprintf(f, ", ");
            fprintf(f, "%s", rule->validate_names[i]);
        }
        fprintf(f, "\n");
    }

    if (rule->num_compose > 0) {
        fprintf(f, "compose: ");
        for (int i = 0; i < rule->num_compose; i++) {
            if (i > 0) fprintf(f, ", ");
            fprintf(f, "%s", rule->compose_names[i]);
        }
        fprintf(f, "\n");
    }

    if (rule->num_init_lines > 0) {
        fprintf(f, "\ninit:\n");
        for (int i = 0; i < rule->num_init_lines; i++)
            fprintf(f, "%s\n", rule->init_code[i]);
    }

    fprintf(f, "\ncode:\n");
    for (int i = 0; i < rule->num_code_lines; i++) {
        fprintf(f, "%s\n", rule->code[i]);
    }

    if (rule->num_cleanup_lines > 0) {
        fprintf(f, "\ncleanup:\n");
        for (int i = 0; i < rule->num_cleanup_lines; i++)
            fprintf(f, "%s\n", rule->cleanup_code[i]);
    }

    fclose(f);
    return 1;
}

static void sanitize_filename(const char *in, char *out, int out_size)
{
    int j = 0;
    for (const char *p = in; *p && j < out_size - 5; p++) {
        if (*p == ' ') { out[j++] = '-'; }
        else if (isalnum((unsigned char)*p) || *p == '-' || *p == '_') { out[j++] = *p; }
    }
    out[j] = '\0';
}

int learn_dsl(const char *keyword, const char *out_dir, DslRule *new_rule)
{
    if (!keyword || !*keyword) return 0;

    DslRule *rule = NULL;

    if (new_rule) {
        rule = new_rule;
    } else {
        for (int i = 0; i < dsl_num_rules; i++) {
            if (strcmp(dsl_rules[i].keyword, keyword) == 0) {
                rule = &dsl_rules[i];
                break;
            }
        }
        if (!rule) {
            printf("DSL rule '%s' not found. Available rules:\n", keyword);
            for (int i = 0; i < dsl_num_rules; i++)
                printf("  %s\n", dsl_rules[i].keyword);
            return 0;
        }
    }

    char safe_name[256];
    sanitize_filename(keyword, safe_name, sizeof(safe_name));
    if (strlen(safe_name) == 0) {
        printf("Error: cannot create filename from keyword '%s'\n", keyword);
        return 0;
    }

    char path[1024];
    SNPRINTF_CHECK(path, sizeof(path), "%s/%s.l1dsl", out_dir, safe_name);

    if (dsl_save_rule(rule, path)) {
        printf("Saved DSL rule '%s' to %s\n", keyword, path);
        return 1;
    }
    printf("Error: failed to write %s\n", path);
    return 0;
}

int learn_dsl_load_code_file(const char *code_file, DslRule *rule)
{
    FILE *f = fopen(code_file, "r");
    if (!f) {
        printf("Error: cannot open code file '%s'\n", code_file);
        return 0;
    }

    char line[DSL_LINE_SIZE];
    int in_code = 0;

    while (fgets(line, sizeof(line), f)) {
        trim(line);
        if (strlen(line) == 0) continue;

        if (strcmp(line, "code:") == 0 || strcmp(line, "code:|") == 0) {
            in_code = 1;
            continue;
        }

        if (in_code) {
            if (strncmp(line, "parser:", 7) == 0 ||
                strncmp(line, "token:", 6) == 0 ||
                strncmp(line, "result:", 7) == 0 ||
                strncmp(line, "include:", 8) == 0 ||
                strncmp(line, "include-post:", 13) == 0 ||
                strncmp(line, "var:", 4) == 0 ||
                strncmp(line, "desc:", 5) == 0 ||
                strncmp(line, "match:", 6) == 0 ||
                strncmp(line, "array:", 6) == 0) {
                break;
            }
            if (rule->num_code_lines < MAX_DSL_CODE_LINES) {
                SNPRINTF_CHECK(rule->code[rule->num_code_lines], DSL_LINE_SIZE, "%s", line);
                rule->num_code_lines++;
            }
        }
    }
    fclose(f);

    if (rule->num_code_lines == 0) {
        printf("Error: no code block found in '%s'\n", code_file);
        return 0;
    }
    return 1;
}

int dsl_match_task_flags(DslRule *rule, TaskProfile *task)
{
    if (rule->num_match_flags == 0) return 0;

    #define CHECK(f) if (strcmp(flag, "has_" #f) == 0 && TF_ISSET(task, FLAG_ ## f)) return 1;
    for (int i = 0; i < rule->num_match_flags; i++) {
        const char *flag = rule->match_flags[i];

        // Simple flag-to-flag mappings
        CHECK(fib_seq) CHECK(fizzbuzz) CHECK(power) CHECK(gcd)
        CHECK(hello_name) CHECK(bool_demo) CHECK(bit_check)
        CHECK(leap_year) CHECK(temp_convert) CHECK(circle_area)
        CHECK(palindrome) CHECK(lcm) CHECK(collatz)
        CHECK(sum_of_digits) CHECK(reverse_string) CHECK(armstrong)
        CHECK(perfect_number) CHECK(count_vowels) CHECK(anagram_check)
        CHECK(string_to_upper) CHECK(string_to_lower) CHECK(caesar_cipher)
        CHECK(palindrome_string) CHECK(string_cat) CHECK(string_compare)
        CHECK(string_to_num) CHECK(timer) CHECK(hello_world)
        CHECK(mult_table) CHECK(guess) CHECK(random)
        CHECK(stack) CHECK(queue) CHECK(binary_search)
        CHECK(square_root) CHECK(prime_factorization) CHECK(compound_interest)
        CHECK(decimal_to_binary) CHECK(dice_roll)
        CHECK(double_circle_area) CHECK(double_pythagoras)
        CHECK(double_temp_convert) CHECK(double_sqrt) CHECK(double_power)
        CHECK(double_volume_sphere) CHECK(double_discount)
        CHECK(double_simple_interest) CHECK(double_bmi)
        CHECK(double_kinetic_energy) CHECK(double_standard_deviation)
        CHECK(double_compound_interest) CHECK(string_length)
        CHECK(function) CHECK(print_var) CHECK(print_even)
        CHECK(countdown_from) CHECK(find_max) CHECK(primes)
        CHECK(array_assign) CHECK(input_fact) CHECK(read_file)
        CHECK(write_file) CHECK(array_min_max) CHECK(string_find)
        CHECK(string_split) CHECK(switch_demo) CHECK(type_convert)
        CHECK(iterative_factorial) CHECK(random_walk) CHECK(bar_chart)
        CHECK(hanoi_tower) CHECK(ascii_art) CHECK(number_to_words)
        CHECK(temperature_table) CHECK(loop_demo) CHECK(pointer)
        CHECK(struct) CHECK(hex_binary) CHECK(shell_args)
        CHECK(time) CHECK(pyramid) CHECK(filter_numbers)
        CHECK(random_generator) CHECK(ascii_table) CHECK(bignum_math)
        CHECK(password_card) CHECK(calculator) CHECK(unit_converter)
        CHECK(sort_stats) CHECK(string_analyzer) CHECK(number_analyzer)
        CHECK(math_menu) CHECK(quiz_game) CHECK(bmi_calculator)
        CHECK(base_converter) CHECK(freq_analysis) CHECK(shuffle)
        CHECK(weighted_random) CHECK(chess_problem) CHECK(shell_repl)
        CHECK(array_access) CHECK(array_write) CHECK(array_iterate)
        CHECK(array_sum) CHECK(array_average)
        CHECK(double_array_access) CHECK(double_array_write)
        CHECK(double_array_iterate) CHECK(double_array_sum)
        CHECK(double_array_min_max) CHECK(double_array_reverse)
        CHECK(double_array_average) CHECK(descending) CHECK(input_sort)
        CHECK(median) CHECK(array_reverse) CHECK(array_find)
        CHECK(array_vmath) CHECK(average) CHECK(sort)
        CHECK(fann_create) CHECK(fann_train) CHECK(fann_run)
        CHECK(standard_deviation) CHECK(double_math) CHECK(double_average)
        CHECK(insertion_sort) CHECK(add) CHECK(sub)
        CHECK(mul) CHECK(div)
        #undef CHECK

        // Special cases: flag name differs from task field or has extra conditions
        if (strcmp(flag, "has_for_sum") == 0 && TF_ISSET(task, FLAG_sum_range)) return 1;
        if (strcmp(flag, "has_factorial") == 0 && TF_ISSET(task, FLAG_factorial) && !TF_ISSET(task, FLAG_input)) return 1;
        if (strcmp(flag, "has_even_odd") == 0 && TF_ISSET(task, FLAG_even_odd) && !TF_ISSET(task, FLAG_print_even)) return 1;
        if (strcmp(flag, "has_bubble_sort") == 0 && TF_ISSET(task, FLAG_bubble_sort) && !TF_ISSET(task, FLAG_descending) && !TF_ISSET(task, FLAG_input_sort)) return 1;
    }
    return 0;
}

static void update_token_from_prompt(L1vmFunction *f, const char *prompt) {
    int nums[8];
    int num_count = 0;
    const char *p = prompt;
    while (*p && num_count < 8) {
        if (*p >= '0' && *p <= '9') {
            int val = 0;
            while (*p >= '0' && *p <= '9') {
                int digit = *p - '0';
                if (val > (INT_MAX - digit) / 10) { while (*p >= '0' && *p <= '9') p++; break; }
                val = val * 10 + digit;
                p++;
            }
            nums[num_count++] = val;
        } else { p++; }
    }
    int ni = 0;
    for (int vi = 0; vi < f->num_vars && ni < num_count; vi++) {
        if (strcmp(f->vars[vi].type, "int64") == 0 || strcmp(f->vars[vi].type, "const-int64") == 0) {
            if (f->vars[vi].num_values > 0 && strcmp(f->vars[vi].values[0], "0") == 0) {
                SNPRINTF_CHECK(f->vars[vi].values[0], sizeof(f->vars[vi].values[0]), "%d", nums[ni]);
                ni++;
            }
        }
    }
}

int dsl_generate_from_task(Program *prog, TaskProfile *task, L1vmFunction *f)
{
    int generated = 0;
    int applied[MAX_DSL_RULES] = {0};

    for (int i = 0; i < dsl_num_rules; i++) {
        if (dsl_match_task_flags(&dsl_rules[i], task)) {
            // Skip if a longer (more specific) rule also matches this task
            int skip = 0;
            for (int j = 0; j < dsl_num_rules; j++) {
                if (j != i && dsl_match_task_flags(&dsl_rules[j], task) &&
                    strlen(dsl_rules[j].keyword) > strlen(dsl_rules[i].keyword) &&
                    strstr(dsl_rules[j].keyword, dsl_rules[i].keyword) != NULL) {
                    skip = 1;
                    break;
                }
            }
            if (skip) continue;
            if (verbose_flag) {
                printf("DSL: matched rule '%s' (%s)\n", dsl_rules[i].keyword, dsl_rules[i].filename);
            }
            dsl_generate_code(prog, &dsl_rules[i], f);
            applied[i] = 1;
            generated = 1;
        }
    }

    if (strlen(task->prompt) > 0) {
        DslRule *kw_rules[MAX_DSL_RULES];
        float kw_scores[MAX_DSL_RULES];
        int kw_count = dsl_match_all_rules(task->prompt, kw_rules, kw_scores, MAX_DSL_RULES, 0.3f);
        for (int i = 0; i < kw_count; i++) {
            int idx = -1;
            for (int j = 0; j < dsl_num_rules; j++) {
                if (&dsl_rules[j] == kw_rules[i]) { idx = j; break; }
            }
            if (idx >= 0 && applied[idx]) continue;
            // Skip if rule has match flags that conflict with current task
            if (idx >= 0 && dsl_rules[idx].num_match_flags > 0 && !dsl_match_task_flags(&dsl_rules[idx], task))
                continue;
            // Skip if rule's primary keyword overlaps with an already-applied rule
            // (substring match: skip "compound interest" if "double compound interest" already applied)
            if (generated && idx >= 0) {
                int skip_kw = 0;
                for (int a = 0; a < dsl_num_rules; a++) {
                    if (applied[a]) {
                        const char *applied_kw = dsl_rules[a].keyword;
                        const char *new_kw = kw_rules[i]->keyword;
                        if (strcmp(applied_kw, new_kw) == 0) {
                            skip_kw = 1;
                            break;
                        }
                        if (strstr(applied_kw, new_kw) != NULL) {
                            skip_kw = 1;
                            break;
                        }
                        if (strstr(new_kw, applied_kw) != NULL) {
                            skip_kw = 1;
                            break;
                        }
                    }
                }
                if (skip_kw) continue;
            }
            if (verbose_flag) {
                printf("DSL: keyword matched rule '%s' (%s) score=%.2f\n",
                       kw_rules[i]->keyword, kw_rules[i]->filename, kw_scores[i]);
            }
            dsl_generate_code(prog, kw_rules[i], f);
            generated = 1;
        }
    }

    if (generated && strlen(task->prompt) > 0)
        update_token_from_prompt(f, task->prompt);

    return generated;
}

DslParam* dsl_find_param(DslRule *rule, const char *name)
{
    if (!rule || !name) return NULL;
    for (int i = 0; i < rule->num_params; i++) {
        if (strcmp(rule->params[i].name, name) == 0)
            return &rule->params[i];
    }
    return NULL;
}

int dsl_has_alias(DslRule *rule, const char *alias)
{
    if (!rule || !alias) return 0;
    for (int i = 0; i < rule->num_aliases; i++) {
        if (strcmp(rule->aliases[i], alias) == 0)
            return 1;
    }
    return 0;
}

int dsl_find_rule_by_keyword(const char *keyword)
{
    if (!keyword) return -1;
    for (int i = 0; i < dsl_num_rules; i++) {
        if (strcmp(dsl_rules[i].keyword, keyword) == 0)
            return i;
    }
    return -1;
}
