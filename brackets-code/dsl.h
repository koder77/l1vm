/*
 * This file dsl.h is part of L1vm.
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

#ifndef DSL_H
#define DSL_H

#include "brackets-code.h"

#define MAX_DSL_RULES 512
#define MAX_DSL_TOKENS 16
#define MAX_DSL_CODE_LINES 256
#define MAX_DSL_VARS 64
#define MAX_DSL_INCLUDES 16
#define MAX_DSL_INCLUDES_POST 16
#define DSL_LINE_SIZE 1024
#define MAX_DSL_KEYWORDS 32

typedef enum {
    DSL_TOKEN_INT64,
    DSL_TOKEN_DOUBLE,
    DSL_TOKEN_STRING,
    DSL_TOKEN_CONST_INT64,
    DSL_TOKEN_CONST_DOUBLE,
    DSL_TOKEN_CONST_STRING,
    DSL_TOKEN_INT64_ARRAY,
    DSL_TOKEN_DOUBLE_ARRAY,
    DSL_TOKEN_STRING_ARRAY,
    DSL_TOKEN_CONST_INT64_ARRAY,
    DSL_TOKEN_INT64_REF,
    DSL_TOKEN_DOUBLE_REF,
    DSL_TOKEN_UNKNOWN
} DslTokenType;

typedef struct {
    DslTokenType type;
    char name[64];
    char l1vm_type[32];
    int is_array;
    int array_size;
    char default_value[256];
} DslToken;

typedef struct {
    char type[64];
    char name[64];
    int count;
    char value[256];
} DslVarDecl;

typedef struct {
    char keyword[64];
    DslToken tokens[MAX_DSL_TOKENS];
    int num_tokens;
    DslToken result;
    int has_result;
    char includes[MAX_DSL_INCLUDES][256];
    int num_includes;
    char includes_post[MAX_DSL_INCLUDES_POST][256];
    int num_includes_post;
    DslVarDecl vars[MAX_DSL_VARS];
    int num_vars;
    char desc[512];
    char code[MAX_DSL_CODE_LINES][DSL_LINE_SIZE];
    int num_code_lines;

    char array_name[64];
    char array_index[64];
    char array_element_type[32];
    int has_array_rule;

    char match_flags[MAX_DSL_TOKENS][64];
    int num_match_flags;

    char filename[512];
    int is_learned;
} DslRule;

typedef struct {
    char parser_keywords[256];
    char token_decl[1024];
    char result_decl[256];
    char include_list[1024];
    char include_post_list[1024];
    char var_decls[2048];
    char code_lines[4096];
    char desc_text[512];
    char array_rule_name[64];
    char array_rule_index[64];
    char array_rule_elem_type[32];
    char match_text[1024];
} DslRawRule;

extern DslRule dsl_rules[MAX_DSL_RULES];
extern int dsl_num_rules;

int dsl_load_rules(const char *dir_path);
int dsl_load_rule_file(const char *path);
void dsl_free_rules(void);
int dsl_match_rule(const char *prompt, DslRule **rule, float *score);
int dsl_match_all_rules(const char *prompt, DslRule **rules, float *scores, int max_rules, float min_score);
int dsl_generate_code(Program *prog, DslRule *rule, Function *f);

int dsl_add_array_rule(const char *name, const char *index_var, const char *element_type);
int dsl_parse_raw(DslRawRule *raw, DslRule *rule);
int parse_token_decl(const char *line, DslToken *token);
int dsl_save_rule(DslRule *rule, const char *path);
int learn_dsl(const char *keyword, const char *out_dir, DslRule *new_rule);
int learn_dsl_load_code_file(const char *code_file, DslRule *rule);
void dsl_print_rule(DslRule *rule);

int dsl_generate_from_task(Program *prog, TaskProfile *task, Function *f);
int dsl_match_task_flags(DslRule *rule, TaskProfile *task);

DslTokenType dsl_token_type_from_string(const char *str);
const char *dsl_token_type_to_string(DslTokenType type);
const char *dsl_token_l1vm_type(DslTokenType type);

#endif
