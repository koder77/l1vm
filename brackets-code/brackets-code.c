/*
 * This file brackets-code.c is part of L1vm.
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

// brackets-code.c
// Brackets Code Generator - CLI tool for generating Brackets (L1VM) code
// Generated with opencode: Big Pickle
//

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>

#include "brackets-code.h"
#include "dsl.h"

#define MAX_LINE 4096
#define MAX_VARS 48
#define MAX_FUNCS 24
#define MAX_PROMPT 8192
#define MAX_CODE 65536
#define MAX_OUTPUT 4096
#define MAX_VALUES 16

int validate_flag = 0;
int retry_seed = 0;
int verbose_flag = 0;
int dry_run_flag = 0;
int dataflow_quiet_mode = 0;
char out_dir[512] = "";
char l1vm_root[512] = "";
static const char *current_prompt = "";

int use_color = 1;
#define c_printf(color, ...) do { if (use_color) { printf(color); printf(__VA_ARGS__); printf(ANSI_RESET); } else printf(__VA_ARGS__); } while(0)

LearnedPattern learned_patterns[MAX_LEARNED];
int num_learned = 0;
int learned_loaded = 0;

// Dynamic allocation helpers
#define INIT_VARS_CAP 8
#define INIT_BODY_CAP 4096
#define INIT_FUNCS_CAP 4
#define INIT_INCLUDES_CAP 4

static int resize_vars(Function *f, int new_cap) {
    Variable *p = realloc(f->vars, (size_t)new_cap * sizeof(Variable));
    if (!p) return 0;
    f->vars = p;
    if (new_cap > f->vars_cap)
        memset(&f->vars[f->vars_cap], 0, (size_t)(new_cap - f->vars_cap) * sizeof(Variable));
    f->vars_cap = new_cap;
    return 1;
}

int ensure_vars_cap(Function *f, int needed) {
    if (needed <= f->vars_cap) return 1;
    if (f->vars_cap > INT_MAX / 2) return 0;
    int new_cap = f->vars_cap ? f->vars_cap * 2 : INIT_VARS_CAP;
    while (new_cap < needed) {
        if (new_cap > INT_MAX / 2) return 0;
        new_cap *= 2;
    }
    return resize_vars(f, new_cap);
}

static int resize_body(Function *f, int new_cap) {
    char *p = realloc(f->body, (size_t)new_cap);
    if (!p) return 0;
    f->body = p;
    if (new_cap > f->body_cap)
        memset(&f->body[f->body_cap], 0, (size_t)(new_cap - f->body_cap));
    f->body_cap = new_cap;
    return 1;
}

int ensure_body_cap(Function *f, int needed) {
    if (needed <= f->body_cap) return 1;
    if (f->body_cap > INT_MAX / 2) return 0;
    int new_cap = f->body_cap ? f->body_cap * 2 : INIT_BODY_CAP;
    while (new_cap < needed) {
        if (new_cap > INT_MAX / 2) return 0;
        new_cap *= 2;
    }
    return resize_body(f, new_cap);
}

int init_function(Function *f) {
    memset(f, 0, sizeof(Function));
    f->vars = NULL; f->vars_cap = 0;
    f->body = NULL; f->body_cap = 0;
    if (!resize_vars(f, INIT_VARS_CAP)) return 0;
    if (!resize_body(f, INIT_BODY_CAP)) {
        free(f->vars); f->vars = NULL; f->vars_cap = 0;
        return 0;
    }
    f->body[0] = '\0';
    return 1;
}

void free_function(Function *f) {
    free(f->vars); f->vars = NULL; f->vars_cap = 0;
    free(f->body); f->body = NULL; f->body_cap = 0;
    f->num_vars = 0;
}

static int resize_funcs_array(Function **pfuncs, int *cap, int new_cap) {
    Function *p = realloc(*pfuncs, (size_t)new_cap * sizeof(Function));
    if (!p) return 0;
    *pfuncs = p;
    if (new_cap > *cap) {
        for (int i = *cap; i < new_cap; i++) {
            if (!init_function(&(*pfuncs)[i])) {
                for (int j = *cap; j < i; j++) free_function(&(*pfuncs)[j]);
                return 0;
            }
        }
    }
    *cap = new_cap;
    return 1;
}

int ensure_funcs_cap(Function **pfuncs, int *cap, int needed) {
    if (needed <= *cap) return 1;
    if (*cap > INT_MAX / 2) return 0;
    int new_cap = *cap ? *cap * 2 : INIT_FUNCS_CAP;
    while (new_cap < needed) {
        if (new_cap > INT_MAX / 2) return 0;
        new_cap *= 2;
    }
    return resize_funcs_array(pfuncs, cap, new_cap);
}

static int resize_includes_arr(char (**pinc)[256], int *cap, int new_cap) {
    char (*p)[256] = realloc(*pinc, (size_t)new_cap * sizeof(char[256]));
    if (!p) return 0;
    *pinc = p;
    if (new_cap > *cap)
        memset(&(*pinc)[*cap], 0, (size_t)(new_cap - *cap) * sizeof(char[256]));
    *cap = new_cap;
    return 1;
}

int ensure_includes_cap(char (**pinc)[256], int *cap, int needed) {
    if (needed <= *cap) return 1;
    if (*cap > INT_MAX / 2) return 0;
    int new_cap = *cap ? *cap * 2 : INIT_INCLUDES_CAP;
    while (new_cap < needed) {
        if (new_cap > INT_MAX / 2) return 0;
        new_cap *= 2;
    }
    return resize_includes_arr(pinc, cap, new_cap);
}

int init_program(Program *prog) {
    memset(prog, 0, sizeof(Program));
    if (!resize_funcs_array(&prog->funcs, &prog->funcs_cap, INIT_FUNCS_CAP)) return 0;
    if (!resize_includes_arr(&prog->includes, &prog->includes_cap, INIT_INCLUDES_CAP)) {
        for (int i = 0; i < prog->funcs_cap; i++) free_function(&prog->funcs[i]);
        free(prog->funcs); prog->funcs = NULL; prog->funcs_cap = 0;
        return 0;
    }
    if (!resize_includes_arr(&prog->includes_post, &prog->includes_post_cap, INIT_INCLUDES_CAP)) {
        for (int i = 0; i < prog->funcs_cap; i++) free_function(&prog->funcs[i]);
        free(prog->funcs); prog->funcs = NULL; prog->funcs_cap = 0;
        free(prog->includes); prog->includes = NULL; prog->includes_cap = 0;
        return 0;
    }
    return 1;
}

void free_program(Program *prog) {
    for (int i = 0; i < prog->funcs_cap; i++)
        free_function(&prog->funcs[i]);
    free(prog->funcs); prog->funcs = NULL; prog->funcs_cap = 0;
    free(prog->includes); prog->includes = NULL; prog->includes_cap = 0;
    free(prog->includes_post); prog->includes_post = NULL; prog->includes_post_cap = 0;
}

int init_learned_pattern(LearnedPattern *lp) {
    memset(lp, 0, sizeof(LearnedPattern));
    if (!resize_includes_arr(&lp->includes, &lp->includes_cap, INIT_INCLUDES_CAP)) return 0;
    if (!resize_funcs_array(&lp->funcs, &lp->funcs_cap, INIT_FUNCS_CAP)) {
        free(lp->includes); lp->includes = NULL; lp->includes_cap = 0;
        return 0;
    }
    return 1;
}

void free_learned_pattern(LearnedPattern *lp) {
    for (int i = 0; i < lp->funcs_cap; i++)
        free_function(&lp->funcs[i]);
    free(lp->funcs); lp->funcs = NULL; lp->funcs_cap = 0;
    free(lp->includes); lp->includes = NULL; lp->includes_cap = 0;
}

void trim(char *s) {
    char *p = s;
    int l = strlen(p);
    if (l == 0) return;
    while (isspace(p[l-1])) p[--l] = 0;
    while (*p && isspace(*p)) ++p;
    memmove(s, p, l - (p - s) + 1);
}

int str_contains_word(const char *str, const char *word) {
    char buf[MAX_LINE];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", str);
    char *p = buf;
    while (*p) {
        while (*p && !isalpha(*p)) p++;
        if (!*p) break;
        char *start = p;
        while (*p && isalpha(*p)) p++;
        char saved = *p;
        *p = '\0';
        if (strcasecmp(start, word) == 0) { *p = saved; return 1; }
        *p = saved;
    }
    return 0;
}

void to_lowercase(char *s) {
    for (; *s; s++) *s = tolower(*s);
}

void add_include(Program *prog, const char *inc) {
    for (int i = 0; i < prog->num_includes; i++)
        if (strcmp(prog->includes[i], inc) == 0) return;
    if (!ensure_includes_cap(&prog->includes, &prog->includes_cap, prog->num_includes + 1)) return;
    SNPRINTF_CHECK(prog->includes[prog->num_includes], sizeof(prog->includes[0]), "%s", inc);
    prog->num_includes++;
}

void add_include_post(Program *prog, const char *inc) {
    for (int i = 0; i < prog->num_includes_post; i++)
        if (strcmp(prog->includes_post[i], inc) == 0) return;
    if (!ensure_includes_cap(&prog->includes_post, &prog->includes_post_cap, prog->num_includes_post + 1)) return;
    SNPRINTF_CHECK(prog->includes_post[prog->num_includes_post], sizeof(prog->includes_post[0]), "%s", inc);
    prog->num_includes_post++;
}

void add_func(Program *prog, const char *name) {
    for (int i = 0; i < prog->num_funcs; i++)
        if (strcmp(prog->funcs[i].name, name) == 0) return;
    if (!ensure_funcs_cap(&prog->funcs, &prog->funcs_cap, prog->num_funcs + 1)) return;
    Function *f = &prog->funcs[prog->num_funcs];
    init_function(f);
    SNPRINTF_CHECK(f->name, sizeof(f->name), "%s", name);
    f->is_local = 0;
    f->has_vars = 0;
    f->has_vardef = 0;
    f->num_vars = 0;
    f->body[0] = '\0';
    prog->num_funcs++;
}

void add_var_to_func(Function *f, const char *type, const char *name, int count, const char **values, int num_values) {
    // Skip if variable with same name already exists (prevents duplicate declarations)
    for (int i = 0; i < f->num_vars; i++)
        if (strcmp(f->vars[i].name, name) == 0) return;
    if (!ensure_vars_cap(f, f->num_vars + 1)) return;
    Variable *v = &f->vars[f->num_vars++];
    SNPRINTF_CHECK(v->name, sizeof(v->name), "%s", name);
    SNPRINTF_CHECK(v->type, sizeof(v->type), "%s", type);
    v->count = count;
    v->num_values = num_values;
    for (int i = 0; i < num_values && i < MAX_VALUES; i++)
        SNPRINTF_CHECK(v->values[i], sizeof(v->values[i]), "%s", values[i]);
}

void func_append(Function *f, const char *line) {
    size_t needed = strlen(f->body) + strlen(line) + 2;
    if (!ensure_body_cap(f, (int)needed + 1)) return;
    strncat(f->body, line, (size_t)f->body_cap - strlen(f->body) - 1);
    strncat(f->body, "\n", (size_t)f->body_cap - strlen(f->body) - 1);
}

void func_vardef(Function *f, const char *scope) {
    if (f->has_vardef) return;
    f->has_vardef = 1;
    f->is_local = 1;
    SNPRINTF_CHECK(f->vardef_name, sizeof(f->vardef_name), "%s", scope);
}

int prompt_to_filename(const char *prompt, char *out, int max) {
    char buf[MAX_PROMPT];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", prompt);
    to_lowercase(buf);
    for (int i = 0; buf[i]; i++)
        if (!isalnum(buf[i])) buf[i] = '_';
    while (buf[0] == '_') memmove(buf, buf+1, strlen(buf));
    char *p = buf + strlen(buf) - 1;
    while (p > buf && *p == '_') *p-- = '\0';
    SNPRINTF_CHECK(out, max, "%s.l1com", buf);
    return 1;
}

int is_question(const char *prompt) {
    const char *qwords[] = {"was", "wie", "wer", "wo", "warum", "wann", "welche", "what", "how", "why", "where", "when", "can", "does", "is", "are", NULL};
    char buf[MAX_PROMPT];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", prompt);
    to_lowercase(buf);
    if (strchr(buf, '?')) return 1;
    for (int i = 0; qwords[i]; i++) {
        size_t len = strlen(qwords[i]);
        if (strncmp(buf, qwords[i], len) == 0) {
            char c = buf[len];
            if (c == ' ' || c == '\0' || c == '?' || c == ',' || c == '.') return 1;
        }
    }
    return 0;
}

void answer_question(const char *prompt) {
    char buf[MAX_PROMPT];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", prompt);
    to_lowercase(buf);
    printf("\n");
    if (strstr(buf, "pointer") || strstr(buf, "zeiger") || strstr(buf, "speicher") || strstr(buf, "adresse")) {
        printf("Pointers in Brackets: Use the `pointer` keyword to get a variable's address.\n");
        printf("  (x Px pointer)   - get pointer to x -> Px\n");
        printf("  (Px [ offset ] val =) - read value at pointer+offset into val\n");
        printf("  (val Px [ offset ] =) - write val to pointer+offset\n");
    } else if (strstr(buf, "for") || strstr(buf, "loop") || strstr(buf, "schleife")) {
        printf("For-loop pattern in Brackets:\n");
        printf("  (for-loop)\n");
        printf("  (((i max <) f :=) f for)\n");
        printf("      // loop body\n");
        printf("      (i + one i :=)\n");
        printf("  (next)\n");
    } else if (strstr(buf, "if") || strstr(buf, "bedingung") || strstr(buf, "condition")) {
        printf("If/else pattern in Brackets:\n");
        printf("  (((x y <) f :=) f if)    - if (x < y)\n");
        printf("      // true branch\n");
        printf("  (endif)\n");
        printf("  Use (if+) and (else) for if-else chains.\n");
    } else if (strstr(buf, "array") || strstr(buf, "feld") || strstr(buf, "liste")) {
        printf("Array access in Brackets uses pointer arithmetic:\n");
        printf("  (set int64 5 arr 10 20 30 40 50)  - declare array\n");
        printf("  (i * int64_size realind :=)        - calculate offset\n");
        printf("  (arr [ realind ] val =)            - read element\n");
        printf("  (val arr [ realind ] =)            - write element\n");
    } else if (strstr(buf, "function") || strstr(buf, "funktion") || strstr(buf, "sub") || strstr(buf, "unter")) {
        printf("Function definition in Brackets:\n");
        printf("  (myfunc func)\n");
        printf("      #var ~ myfunc      - local scope\n");
        printf("      (set int64 1 x~ 0)  - local variable\n");
        printf("      (arg1~ stpop)       - pop argument from stack\n");
        printf("      ...\n");
        printf("      (retval~ stpush)    - push return value\n");
        printf("      (return)\n");
        printf("  (funcend)\n");
        printf("  Call: (arg :myfunc !) (ret stpop)\n");
    } else if (strstr(buf, "struct") || strstr(buf, "dotted") || strstr(buf, "punkt")) {
        printf("Struct/dotted variables in Brackets:\n");
        printf("  (set int64 1 person.age 0)  - dotted name acts like struct field\n");
        printf("  (person.age :print_i !)\n");
    } else if (strstr(buf, "string") || strstr(buf, "zeichen") || strstr(buf, "text")) {
        printf("String handling in Brackets:\n");
        printf("  (set const-string s msg \"Hello\")  - immutable string\n");
        printf("  (set string 256 buf \"\")           - mutable buffer\n");
        printf("  (msg :print_s !)                     - print string\n");
        printf("  (dest src :string_copy !)           - copy string\n");
    } else if (strstr(buf, "stack") || strstr(buf, "stapel") || strstr(buf, "push") || strstr(buf, "pop")) {
        printf("Stack operations in Brackets:\n");
        printf("  stpush  - push variable to stack\n");
        printf("  stpop   - pop from stack into variable\n");
        printf("  (val :func !) - call function, args pushed before call\n");
        printf("  (ret stpop)   - get return value after call\n");
    } else if (strstr(buf, "exit") || strstr(buf, "beenden")) {
        printf("To exit a Brackets program:\n");
        printf("  (zero :exit !)  - exit with status code in variable\n");
        printf("  zero should be (set const-int64 1 zero 0)\n");
    } else if (strstr(buf, "brackets") || strstr(buf, "l1vm") || strstr(buf, "l1com") || strstr(buf, "was ist")) {
        printf("Brackets %s (L1VM) is a stack-based virtual machine and assembly language.\n", VERSION_TXT);
        printf("Code is written in .l1com files using S-expressions (parentheses).\n");
        printf("Key concepts: (func), (set), (if), (for-loop), pointer access, local scoping via #var ~\n");
    } else {
        printf("I'm a Brackets %s (L1VM) code generator. Ask me about:\n", VERSION_TXT);
        printf("- How to write loops, if/else, functions\n");
        printf("- Pointers, arrays, structs, strings\n");
        printf("- Stack operations, math, exit\n");
        printf("Or give me a code request like \"Create code that adds two numbers\".\n");
    }
    printf("\n");
}

typedef struct {
    const char *op;
    const char *rpn_op;
} OpMap;

static const OpMap ops[] = {
    {"+", "add"},
    {"plus", "add"},
    {"add", "add"},
    {"addiere", "add"},
    {"sum", "add"},
    {"-", "sub"},
    {"minus", "sub"},
    {"subtract", "sub"},
    {"subtrahiere", "sub"},
    {"zieh", "sub"},
    {"differenz", "sub"},
    {"*", "mul"},
    {"mal", "mul"},
    {"multiply", "mul"},
    {"multipliziere", "mul"},
    {"product", "mul"},
    {"/", "div"},
    {"geteilt", "div"},
    {"divide", "div"},
    {"dividiere", "div"},
    {"quotient", "div"},
    {"%", "mod"},
    {"modulo", "mod"},
    {"mod", "mod"},
    {"rest", "mod"},
};

static const char* find_operation(const char *buf) {
    for (int i = 0; i < (int)(sizeof(ops)/sizeof(ops[0])); i++)
        if (strstr(buf, ops[i].op)) return ops[i].rpn_op;
    return NULL;
}

// ==================== FUZZY SYNONYM MATCHING ====================

#define SYNONYM_FILE "synonyms.txt"
#define SYNONYM_DIR ".config/brackets-code"
#define MAX_DYN_SYNONYMS 1024
static Synonym dyn_synonyms[MAX_DYN_SYNONYMS];
static int num_dyn_synonyms = 0;
static int synonyms_loaded = 0;

static FILE *open_synonym_file(void) {
    FILE *f = fopen(SYNONYM_FILE, "r");
    if (f) return f;
    const char *home = getenv("HOME");
    if (home) {
        char path[1024];
        SNPRINTF_CHECK(path, sizeof(path), "%s/%s/%s", home, SYNONYM_DIR, SYNONYM_FILE);
        f = fopen(path, "r");
        if (f) return f;
    }
    // Try next to the executable
    char self_path[1024];
    ssize_t len = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
    if (len > 0) {
        self_path[len] = '\0';
        char *slash = strrchr(self_path, '/');
        if (slash) {
            SNPRINTF_CHECK(slash + 1, sizeof(self_path) - (size_t)(slash - self_path + 1), "%s", SYNONYM_FILE);
            f = fopen(self_path, "r");
            if (f) return f;
        }
    }
    return NULL;
}

void load_synonyms(void) {
    if (synonyms_loaded) return;
    synonyms_loaded = 1;
    FILE *f = open_synonym_file();
    if (!f) return;
    char line[256];
    while (fgets(line, sizeof(line), f) && num_dyn_synonyms < MAX_DYN_SYNONYMS) {
        trim(line);
        if (line[0] == '#' || line[0] == '\0') continue;
        char *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';
        char *word = line;
        char *canonical = colon + 1;
        trim(word);
        trim(canonical);
        if (strlen(word) > 0 && strlen(canonical) > 0) {
            SNPRINTF_CHECK(dyn_synonyms[num_dyn_synonyms].word, sizeof(dyn_synonyms[0].word), "%s", word);
            SNPRINTF_CHECK(dyn_synonyms[num_dyn_synonyms].canonical, sizeof(dyn_synonyms[0].canonical), "%s", canonical);
            num_dyn_synonyms++;
        }
    }
    fclose(f);
}

typedef struct { char word[40]; char canonical[40]; } SynonymEntry;

static const SynonymEntry SYNONYM_TABLE[] = {
    {"accumulate", "sum"}, {"addiere", "add"}, {"addieren", "add"}, {"addition", "add"},
    {"aggregate", "sum"}, {"arrange", "sort"}, {"aufaddieren", "sum"}, {"aufrechnen", "sum"},
    {"aufsummieren", "sum"}, {"berechnen", "sum"}, {"berechne", "sum"}, {"bubble sort", "sort"},
    {"bubblesort", "sort"}, {"calculate", "sum"}, {"calculation", "sum"}, {"classify", "sort"},
    {"collect", "input"}, {"compute", "sum"}, {"count up", "sum"}, {"deduct", "sub"},
    {"differenz", "sub"}, {"divide", "div"}, {"dividieren", "div"}, {"enumeration", "print"},
    {"enumerate", "print"}, {"evaluate", "sum"}, {"figure out", "sum"}, {"gather", "input"},
    {"group", "sort"}, {"herabsetzen", "sub"}, {"increment", "add"}, {"iteration", "loop"},
    {"iterate", "loop"}, {"merge sort", "sort"}, {"minus", "sub"}, {"multipliziere", "mul"}, {"multiplizieren", "mul"},
    {"order", "sort"}, {"ordnen", "sort"}, {"organize", "sort"}, {"plus", "add"},
    {"produkt", "mul"}, {"product", "mul"}, {"quotient", "div"}, {"quick sort", "sort"},
    {"rechnen", "sum"},     {"reduce", "sub"}, {"zieh", "sub"}, {"subtrahiere", "sub"}, {"remainder", "mod"}, {"reorder", "sort"},
    {"repeat", "loop"}, {"rest", "mod"}, {"series", "loop"}, {"sortieren", "sort"},
    {"sortiert", "sort"}, {"subtrahieren", "sub"}, {"subtraktion", "sub"}, {"tally", "sum"},
    {"total", "sum"}, {"vermindern", "sub"}, {"zusammenzaehlen", "sum"}, {"zusammenzählen", "sum"},
    {"auffangen", "input"}, {"capture", "input"}, {"collect", "input"}, {"eingabe", "input"},
    {"einlesen", "read"}, {"erfassen", "input"}, {"erfasse", "input"}, {"erhalten", "input"},
    {"fetch", "read"}, {"gather", "input"}, {"gib ein", "input"}, {"import", "read"},
    {"importieren", "read"}, {"laden", "read"}, {"lade", "read"}, {"load", "read"},
    {"obtain", "input"}, {"oeffnen", "read"}, {"öffnen", "read"}, {"prompt", "input"},
    {"receive", "input"}, {"retrieve", "read"}, {"take", "input"}, {"uebernehmen", "input"},
    {"übernehmen", "input"}, {"anzeigen", "print"}, {"auflisten", "print"}, {"ausgabe", "print"},
    {"ausgeben", "print"}, {"ausspucken", "print"}, {"chart", "print"}, {"darstellen", "print"},
    {"display", "print"}, {"drucke", "print"}, {"drucken", "print"}, {"export", "write"},
    {"herausgeben", "print"}, {"indicate", "print"}, {"list", "print"}, {"output", "print"},
    {"present", "print"}, {"produce", "print"}, {"publish", "print"}, {"report", "print"},
    {"show", "print"}, {"speichern", "write"}, {"speicher", "write"}, {"save", "write"},
    {"sichern", "write"}, {"store", "write"}, {"veroeffentlichen", "print"}, {"veröffentlichen", "print"},
    {"visualize", "print"}, {"zeigen", "print"}, {"append", "assign"}, {"catalogue", "array"},
    {"catalog", "array"}, {"data set", "array"}, {"dataset", "array"}, {"element", "array"},
    {"feld", "array"}, {"index", "array"}, {"insert", "assign"}, {"liste", "array"},
    {"list", "array"}, {"push", "assign"}, {"sequence", "array"}, {"tabulate", "array"},
    {"table", "array"}, {"tabelle", "array"}, {"buchstabe", "string"}, {"buchstaben", "string"},
    {"character", "string"}, {"combine", "concat"}, {"concatenate", "concat"}, {"join", "concat"},
    {"merge", "concat"}, {"text", "string"}, {"verbinden", "concat"}, {"verbinde", "concat"},
    {"verketten", "concat"}, {"verkette", "concat"}, {"wort", "string"}, {"wörter", "string"},
    {"woerter", "string"}, {"zeichenkette", "string"}, {"dokument", "file"}, {"document", "file"},
    {"datei", "file"}, {"auffinden", "find"}, {"bestimmen", "find"}, {"determine", "find"},
    {"discover", "find"}, {"entdecken", "find"}, {"ermitteln", "find"}, {"extract", "find"},
    {"herausfinden", "find"}, {"identify", "find"}, {"isolate", "find"}, {"locate", "find"},
    {"lokalisieren", "find"}, {"pick", "find"}, {"search", "find"}, {"select", "find"},
    {"suche", "find"}, {"suchen", "find"}, {"backwards", "reverse"}, {"flip", "reverse"},
    {"invert", "reverse"}, {"mirror", "reverse"}, {"rueckwaerts", "reverse"}, {"rückwärts", "reverse"},
    {"spiegeln", "reverse"}, {"umdrehen", "reverse"}, {"umkehren", "reverse"}, {"turn around", "reverse"},
    {"biggest", "max"}, {"bottom", "min"}, {"choose", "max"}, {"greatest", "max"},
    {"highest", "max"}, {"hoehe", "max"}, {"höhe", "max"}, {"kleinste", "min"},
    {"largest", "max"}, {"least", "min"}, {"lowest", "min"}, {"maximal", "max"},
    {"minimal", "min"}, {"niedrigste", "min"}, {"peak", "max"}, {"smallest", "min"},
    {"top", "max"}, {"tiefste", "min"}, {"center", "median"}, {"central", "median"},
    {"durchschnittlich", "average"}, {"middle", "median"}, {"midpoint", "median"}, {"mitte", "median"},
    {"mittel", "median"}, {"mittelwert", "average"}, {"normal", "average"}, {"standard", "average"},
    {"typical", "average"}, {"typisch", "average"}, {"zentral", "median"}, {"zentrum", "median"},
    {"bedingung", "if"}, {"condition", "if"}, {"falls", "if"}, {"repeat", "for"},
    {"repetition", "for"}, {"schleife", "loop"}, {"wenn", "if"}, {"byte", "byte"},
    {"count", "number"}, {"decimal", "double"}, {"floating point", "double"}, {"ganzzahl", "int64"},
    {"integer", "int64"}, {"kommazahl", "double"}, {"natuerlich", "int64"}, {"natürlich", "int64"},
    {"quantity", "number"}, {"real", "double"}, {"size", "number"}, {"zaehle", "number"},
    {"zählen", "number"}, {"zähle", "number"}, {"aendern", "convert"}, {"ändern", "convert"},
    {"cast", "convert"}, {"change", "convert"}, {"coerce", "convert"}, {"konvertieren", "convert"},
    {"konvertiere", "convert"}, {"transforms", "convert"}, {"transform", "convert"},
    {"translate", "convert"}, {"turn", "convert"}, {"umrechnen", "convert"}, {"umwandeln", "convert"},
    {"wandle", "convert"}, {"wechseln", "convert"}, {"benchmark", "timer"}, {"chrono", "timer"},
    {"dauer", "timer"}, {"delay", "timer"}, {"duration", "timer"}, {"elapsed", "timer"},
    {"execution", "timer"}, {"how long", "timer"}, {"lauf", "timer"}, {"laufzeit", "timer"},
    {"measure", "timer"}, {"messen", "timer"}, {"mess", "timer"}, {"performance", "timer"},
    {"profile", "timer"}, {"speed", "timer"}, {"stopwatch", "timer"}, {"time", "timer"},
    {"wait", "timer"}, {"argument", "shell"}, {"args", "shell"}, {"command line", "shell"},
    {"commandline", "shell"}, {"cmd", "shell"}, {"kommando", "shell"}, {"kommandozeile", "shell"},
    {"parameter", "shell"}, {"sys", "shell"}, {"system", "shell"}, {"bonjour", "hello"},
    {"greet", "hello"}, {"greeting", "hello"}, {"grusse", "hello"}, {"grüße", "hello"},
    {"grüss", "hello"}, {"grüß", "hello"}, {"hi", "hello"}, {"salut", "hello"},
    {"salutation", "hello"}, {"willkommen", "hello"}, {"welcome", "hello"}, {"begruessen", "hello"},
    {"begrüßen", "hello"}, {"allocate", "pointer"}, {"address", "pointer"}, {"adresse", "pointer"},
    {"memory", "pointer"}, {"mem", "pointer"}, {"referenz", "pointer"}, {"reference", "pointer"},
    {"zeiger", "pointer"}, {"dotted", "struct"}, {"object", "struct"}, {"punkt", "struct"},
    {"record", "struct"}, {"struktur", "struct"}, {"structure", "struct"}, {"unterprogramm", "function"},
    {"funktion", "function"}, {"func", "function"}, {"method", "function"}, {"prozedur", "function"},
    {"procedure", "function"}, {"routine", "function"}, {"subroutine", "function"},
    {"binaer", "binary"}, {"binär", "binary"}, {"hexadezimal", "hex"}, {"hexadecimal", "hex"},
    {"oct", "hex"}, {"octal", "hex"}, {"fakultaet", "factorial"}, {"fakultät", "factorial"},
    {"fib", "fib"}, {"fibonacci", "fib"}, {"prime number", "prime"}, {"primzahl", "prime"},
    {"prime", "prime"}, {"erraten", "guess"}, {"guess", "guess"}, {"rate", "guess"},
    {"raten", "guess"}, {"fizz", "fizzbuzz"},
    {"fizz buzz", "fizzbuzz"}, {"fizzbuzz", "fizzbuzz"}, {"gerade", "even"}, {"odd", "odd"},
    {"ungerade", "odd"}, {"einmaleins", "multiplication"}, {"multiplikation", "multiplication"},
    {"multiplication", "multiplication"}, {"1x1", "multiplication"}, {"mal reihe", "multiplication"},
    {"exponent", "power"}, {"hoch", "power"}, {"potenz", "power"}, {"power", "power"},
    {"square", "power"}, {"squared", "power"}, {"quadrat", "power"}, {"quadrieren", "power"},
    {"cubic", "power"}, {"cube", "power"}, {"gemeinsamer", "gcd"}, {"ggt", "gcd"},
    {"greatest common", "gcd"}, {"gcm", "gcd"}, {"gcd", "gcd"}, {"countdown", "countdown"},
    {"herunter", "countdown"}, {"herunterzaehlen", "countdown"}, {"herunterzählen", "countdown"},
    {"runterzaehlen", "countdown"}, {"runterzählen", "countdown"}, {"runterz", "countdown"},
    {"timer", "countdown"}, {"zuweisen", "assign"}, {"zuweisung", "assign"}, {"schreiben", "write"},
    {"schreib", "write"},
    {"primes", "prime"}, {"numbers", "num"}, {"values", "value"},
    {"demo", "show"}, {"example", "show"},
    {"benchmark", "time"}, {"clock", "time"}, {"performance", "time"},
    {"network", "data"}, {"crypto", "data"}, {"encrypt", "data"},
    {"json", "data"}, {"config", "data"},
    {"graphics", "show"}, {"render", "show"},
    {"game", "guess"}, {"play", "guess"},
    {"float", "num"}, {"bignum", "num"}, {"big", "num"},
    {"include", "file"}, {"header", "file"},
    {"list", "array"}, {"vector", "array"}, {"collection", "array"},
    {"search", "find"}, {"locate", "find"}, {"lookup", "find"},
    {"copy", "assign"}, {"duplicate", "assign"}, {"clone", "assign"},
    {"delete", "remove"}, {"clear", "remove"}, {"erase", "remove"},
    {"insert", "add"}, {"append", "add"}, {"push", "add"},
    {"pop", "remove"}, {"extract", "remove"},
    {"call", "run"}, {"invoke", "run"}, {"execute", "run"},
    {"text", "string"}, {"word", "string"},
    {"fann", "fann"}, {"neural", "fann"}, {"netz", "fann"}, {"network", "fann"},
    {"neuronal", "fann"}, {"neuron", "fann"}, {"ki", "fann"}, {"ai", "fann"},
    {"machine learning", "fann"}, {"deep", "fann"}, {"training", "train"},
    {"trainieren", "train"}, {"trainiere", "train"}, {"lernen", "train"},
    {"learn", "train"}, {"inferenz", "run"}, {"inference", "run"},
    {"vorhersage", "run"}, {"predict", "run"}, {"prediction", "run"},
    {"vorhersagen", "run"}, {"klassifikation", "run"}, {"classification", "run"},
};
static const int NUM_SYNONYMS = sizeof(SYNONYM_TABLE) / sizeof(SYNONYM_TABLE[0]);

const char* resolve_synonym(const char *word) {
    for (int i = 0; i < num_dyn_synonyms; i++)
        if (strcmp(word, dyn_synonyms[i].word) == 0)
            return dyn_synonyms[i].canonical;
    for (int i = 0; i < NUM_SYNONYMS; i++)
        if (strcmp(word, SYNONYM_TABLE[i].word) == 0)
            return SYNONYM_TABLE[i].canonical;
    return NULL;
}

void expand_query(const char *query, char *expanded, int max_len) {
    SNPRINTF_CHECK(expanded, max_len, "%s", query);
    char buf[MAX_PROMPT];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", query);
    to_lowercase(buf);
    char *p = buf;
    while (*p) {
        while (*p && !isalpha(*p)) p++;
        if (!*p) break;
        char *start = p;
        while (*p && isalpha(*p)) p++;
        char saved = *p;
        *p = '\0';
        const char *canonical = resolve_synonym(start);
        if (canonical && strcmp(start, canonical) != 0) {
            strncat(expanded, " ", max_len - strlen(expanded) - 1);
            strncat(expanded, canonical, max_len - strlen(expanded) - 1);
        }
        *p = saved;
    }
}

int has_word_fuzzy(const char *text, const char *keyword) {
    char buf[MAX_PROMPT];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", text);
    to_lowercase(buf);
    int kwlen = strlen(keyword);
    char *p = buf;
    while ((p = strstr(p, keyword)) != NULL) {
        // check word boundaries before and after
        int at_start = (p == buf);
        int at_end = (p[kwlen] == '\0');
        int before_ok = at_start || !isalpha((unsigned char)*(p - 1));
        int after_ok = at_end || !isalpha((unsigned char)p[kwlen]);
        if (before_ok && after_ok) return 1;
        p++;
    }
    p = buf;
    while (*p) {
        while (*p && !isalpha(*p)) p++;
        if (!*p) break;
        char *start = p;
        while (*p && isalpha(*p)) p++;
        char saved = *p;
        *p = '\0';
        const char *canonical = resolve_synonym(start);
        if (canonical != NULL && strcmp(canonical, keyword) == 0) { *p = saved; return 1; }
        *p = saved;
    }
    return 0;
}

int has_word(const char *prompt, const char *word) {
    return has_word_fuzzy(prompt, word);
}

static int is_negated(const char *prompt, const char *keyword) {
    char buf[MAX_PROMPT];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", prompt);
    to_lowercase(buf);
    int kwlen = strlen(keyword);
    char *p = buf;
    static const char *negations[] = {
        "not", "don't", "dont", "doesn't", "doesnt", "isn't", "isnt",
        "won't", "wont", "can't", "cant", "no", "without", "kein",
        "nicht", "keine", "keinen", "keinem", "never", "niemals",
        "weder", "nor", "neither", NULL
    };
    while ((p = strstr(p, keyword)) != NULL) {
        int at_start = (p == buf);
        int before_ok = at_start || !isalpha((unsigned char)*(p - 1));
        int after_ok = (p[kwlen] == '\0') || !isalpha((unsigned char)p[kwlen]);
        if (before_ok && after_ok) {
            // Look backwards from keyword for negation words within 5 words
            char *scan = p - 1;
            int words_back = 0;
            while (scan >= buf && words_back < 5) {
                while (scan >= buf && isspace((unsigned char)*scan)) scan--;
                if (scan < buf) break;
                char *word_end = scan;
                while (scan >= buf && isalpha((unsigned char)*scan)) scan--;
                char *word_start = scan + 1;
                int wlen = word_end - word_start + 1;
                if (wlen > 0 && wlen < 32) {
                    char wbuf[32];
                    SNPRINTF_CHECK(wbuf, sizeof(wbuf), "%.*s", wlen, word_start);
                    to_lowercase(wbuf);
                    for (int ni = 0; negations[ni]; ni++) {
                        if (strcmp(wbuf, negations[ni]) == 0) return 1;
                    }
                    words_back++;
                }
                if (scan >= buf) scan--;
            }
            return 0;
        }
        p++;
    }
    return 0;
}

// ==================== TASK PROFILE ====================

#define MAX_NUMS 8


static int word_to_num(const char *word);

static int extract_numbers(const char *prompt, int *nums, int max_nums) {
    char buf[MAX_PROMPT];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", prompt);
    to_lowercase(buf);
    int count = 0;
    char *p = buf;
    while (*p && count < max_nums) {
        if (isalpha(*p)) {
            char word[32]; int wi = 0;
            while (*p && isalpha(*p) && wi < 31) word[wi++] = *p++;
            word[wi] = '\0';
            int n = word_to_num(word);
            if (n >= 0) nums[count++] = n;
            continue;
        }
        p++;
    }
    p = buf;
    while (*p && count < max_nums) {
        if (isdigit(*p)) {
            char *prev = p - 1;
            while (prev >= buf && (isalpha(*prev) || *prev == '_')) prev--;
            prev++;
            if (prev < p) {
                char prefix[16]; int pi = 0;
                while (prev < p && pi < 15) prefix[pi++] = *prev++;
                prefix[pi] = '\0';
                if (prefix[0] == 'i' && prefix[1] == 'n' && prefix[2] == 't') {
                    while (*p && isdigit(*p)) p++;
                    continue;
                }
                if (strcmp(prefix, "const") == 0) {
                    while (*p && isdigit(*p)) p++;
                    continue;
                }
            }
            char *endptr = NULL;
            long val = strtol(p, &endptr, 10);
            if (endptr != p) nums[count++] = (int)val;
            while (*p && isdigit(*p)) p++;
            continue;
        }
        p++;
    }
    return count;
}

// Forward declarations for plan-based generator
int parse_task(const char *prompt, TaskProfile *task);
int generate_from_task(Program *prog, TaskProfile *task, int last_step);


int parse_task(const char *prompt, TaskProfile *task) {
    char buf[MAX_PROMPT];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", prompt);
    to_lowercase(buf);
    SNPRINTF_CHECK(task->prompt, sizeof(task->prompt), "%s", prompt);

    // default type
    SNPRINTF_CHECK(task->type, sizeof(task->type), "%s", "int64");

    // detect type
    if (strstr(buf, "byte") || strstr(buf, "int8")) SNPRINTF_CHECK(task->type, sizeof(task->type), "%s", "byte");
    else if (strstr(buf, "int16") || strstr(buf, "short")) SNPRINTF_CHECK(task->type, sizeof(task->type), "%s", "int16");
    else if (strstr(buf, "int32")) SNPRINTF_CHECK(task->type, sizeof(task->type), "%s", "int32");
    else if (strstr(buf, "int64") || has_word(buf, "int") || strstr(buf, "long")) SNPRINTF_CHECK(task->type, sizeof(task->type), "%s", "int64");
    else if (strstr(buf, "double") || strstr(buf, "float") || strstr(buf, "real") || strstr(buf, "komma")) SNPRINTF_CHECK(task->type, sizeof(task->type), "%s", "double");

    // extract numbers
    task->num_literals = extract_numbers(prompt, task->literals, MAX_NUMS);
    // also extract double literals (decimal numbers)
    int has_decimals = 0;
    for (int di = 0; di < task->num_literals && di < MAX_NUMS; di++)
        task->double_literals[di] = (double)task->literals[di];
    {
        char dscan[MAX_PROMPT];
        SNPRINTF_CHECK(dscan, sizeof(dscan), "%s", buf);
        char *dp = dscan;
        int di = 0;
        while (*dp && di < MAX_NUMS) {
            if (isdigit(*dp)) {
                char *start = dp;
                while (*dp && (isdigit(*dp) || *dp == '.')) dp++;
                if (strchr(start, '.') != NULL) {
                    char *endptr_d = NULL;
                    task->double_literals[di] = strtod(start, &endptr_d);
                    if (endptr_d == start) task->double_literals[di] = 0.0;
                    has_decimals = 1;
                } else {
                    task->double_literals[di] = (double)task->literals[di];
                }
                di++;
                continue;
            }
            dp++;
        }
        // override num_literals with count from this pass (handles decimals as single values)
        if (has_decimals) {
            task->num_literals = di;
            SNPRINTF_CHECK(task->type, sizeof(task->type), "%s", "double");
        }
    }
    task->has_literals = (task->num_literals > 0);

    // count detection: word before "numbers/werte/zahlen/values"
    char *p = buf;
    while (*p) {
        if (strncmp(p, "numbers", 7) == 0 || strncmp(p, "werte", 5) == 0 || strncmp(p, "zahlen", 6) == 0 || strncmp(p, "values", 6) == 0) {
            char *q = p - 1;
            while (q >= buf && isspace(*q)) q--;
            while (q >= buf && isdigit(*q)) q--;
            q++;
            if (q < p && isdigit(*q)) {
                task->input_count = (int)strtol(q, NULL, 10);
                task->has_input = 1;
            } else {
                // check for spelled-out number before "numbers"
                q = p - 1;
                while (q >= buf && isspace(*q)) q--;
                char *end = q + 1;
                while (q >= buf && isalpha(*q)) q--;
                q++;
                if (q < end) {
                    char w[32]; int wi = 0;
                    while (q < end && wi < 31) w[wi++] = *q++;
                    w[wi] = '\0';
                    int n = word_to_num(w);
                    if (n > 0) { task->input_count = n; task->has_input = 1; }
                }
            }
            break;
        }
        p++;
    }
    // detect input keywords
    if (has_word(buf, "lies") || has_word(buf, "lese") || has_word(buf, "read") || has_word(buf, "input")
        || has_word(buf, "gib") || has_word(buf, "enter") || has_word(buf, "erfasse")
        || has_word(buf, "collect") || has_word(buf, "eingabe") || has_word(buf, "einlesen"))
        task->has_input = 1;

    // detect output
    if (has_word(buf, "print") || has_word(buf, "druck") || has_word(buf, "ausg")
        || has_word(buf, "show") || has_word(buf, "display") || has_word(buf, "zeig")
        || has_word(buf, "output") || has_word(buf, "schreib"))
        task->has_output = 1;

    // detect print variable
    if (task->has_output && strstr(buf, "variable"))
        task->has_print_var = 1;

    // detect operation
    const char *op = find_operation(buf);
    if (op) {
        task->has_operation = 1;
        SNPRINTF_CHECK(task->op, sizeof(task->op), "%s", op);
        if (strcmp(op, "add") == 0) task->has_add = 1;
        else if (strcmp(op, "sub") == 0) task->has_sub = 1;
        else if (strcmp(op, "mul") == 0) task->has_mul = 1;
        else if (strcmp(op, "div") == 0) task->has_div = 1;
    }
    // also detect each operation independently via strstr (like find_operation)
    for (int oi = 0; oi < (int)(sizeof(ops)/sizeof(ops[0])); oi++) {
        if (strstr(buf, ops[oi].op)) {
            if (strcmp(ops[oi].rpn_op, "add") == 0) task->has_add = 1;
            else if (strcmp(ops[oi].rpn_op, "sub") == 0) task->has_sub = 1;
            else if (strcmp(ops[oi].rpn_op, "mul") == 0) task->has_mul = 1;
            else if (strcmp(ops[oi].rpn_op, "div") == 0) task->has_div = 1;
        }
    }

    // build operation sequence in prompt order (left-to-right)
    task->num_ops = 0;
    {
        // Find all operation keywords via strstr (mirrors the old detection loop)
        // Record position and sort by position to maintain prompt order
        int found_ops = 0;
        int op_positions[32];
        char op_names[32][64];
        for (int oi = 0; oi < (int)(sizeof(ops)/sizeof(ops[0])); oi++) {
            char *p = buf;
            while ((p = strstr(p, ops[oi].op)) != NULL && found_ops < 32) {
                // word-boundary check (before only, matching strstr-loop behavior)
                int at_start = (p == buf);
                int before_ok = at_start || !isalpha((unsigned char)*(p - 1));
                if (before_ok) {
                    int pos = (int)(p - buf);
                    int dup = 0;
                    for (int di = 0; di < found_ops; di++) {
                        if (op_positions[di] == pos) { dup = 1; break; }
                    }
                    if (!dup) {
                        op_positions[found_ops] = pos;
                        SNPRINTF_CHECK(op_names[found_ops], sizeof(op_names[0]), "%s", ops[oi].rpn_op);
                        found_ops++;
                    }
                }
                p++;
            }
        }
        // Sort by position to restore prompt order
        for (int i = 0; i < found_ops - 1; i++) {
            for (int j = i + 1; j < found_ops; j++) {
                if (op_positions[j] < op_positions[i]) {
                    int tp = op_positions[i]; op_positions[i] = op_positions[j]; op_positions[j] = tp;
                    char tn[64]; memcpy(tn, op_names[i], sizeof(tn));
                    memcpy(op_names[i], op_names[j], sizeof(op_names[i]));
                    memcpy(op_names[j], tn, sizeof(op_names[j]));
                }
            }
        }
        // Pair operations with literals in prompt order
        int lit_idx = 0;
        for (int oi = 0; oi < found_ops && lit_idx < task->num_literals; oi++) {
            SNPRINTF_CHECK(task->op_seq[task->num_ops], sizeof(task->op_seq[0]), "%.*s", (int)sizeof(task->op_seq[0]) - 1, op_names[oi]);
            task->op_seq_literals[task->num_ops] = task->literals[lit_idx++];
            task->num_ops++;
        }
    }

    // if count not yet set and single literal could be a count (only when no operation present)
    if (!task->has_input && task->num_literals == 1 && task->has_literals && !task->has_operation) {
        int lit = task->literals[0];
        if (lit >= 1 && lit <= 1000) {
            task->input_count = lit;
            task->has_input = 1;
        }
    }

    // detect loops
    if (has_word(buf, "loop") || has_word(buf, "schleife") || has_word(buf, "for")
        || has_word(buf, "while") || has_word(buf, "wiederhol") || has_word(buf, "iterate"))
        task->has_loop = 1;

    // detect condition
    if (has_word(buf, "if") || has_word(buf, "bedingung") || has_word(buf, "wenn")
        || has_word(buf, "condition") || has_word(buf, "falls"))
        task->has_condition = 1;

    // algorithm detection
    if (has_word(buf, "sort") || has_word(buf, "bubble") || has_word(buf, "sortiere"))
        task->has_sort = 1;

    if (has_word(buf, "descending") || has_word(buf, "descendig") || has_word(buf, "desc") || has_word(buf, "absteigend"))
        task->has_descending = 1;

    if ((has_word(buf, "bignum") || (has_word(buf, "big") && has_word(buf, "number")) || has_word(buf, "mpfr")) && (has_word(buf, "power") || has_word(buf, "pow") || has_word(buf, "potenz") || has_word(buf, "calculate") || has_word(buf, "^")))
        task->has_bignum_math = 1;

    if (has_word(buf, "power") || has_word(buf, "potenz") || has_word(buf, "exponent")
        || has_word(buf, "square") || has_word(buf, "quadrat") || has_word(buf, "cube"))
        task->has_power = 1;

    if (has_word(buf, "max") || has_word(buf, "größt") || has_word(buf, "largest")
        || has_word(buf, "greatest") || has_word(buf, "highest") || has_word(buf, "maximum"))
        task->has_max = 1;

    if (has_word(buf, "min") || has_word(buf, "kleinste") || has_word(buf, "smallest")
        || has_word(buf, "least") || has_word(buf, "lowest") || has_word(buf, "minimum"))
        task->has_min = 1;

    if (has_word(buf, "gcd") || has_word(buf, "ggt") || has_word(buf, "gcm") || strstr(buf, "common divisor") || strstr(buf, "greatest common"))
        task->has_gcd = 1;

    if (has_word(buf, "countdown") || has_word(buf, "count down"))
        task->has_countdown = 1;

    if (!has_word(buf, "ascii") && (has_word(buf, "table") || has_word(buf, "einmaleins") || has_word(buf, "multiplication")))
        task->has_mult_table = 1;

    if (has_word(buf, "guess") || has_word(buf, "rate") || has_word(buf, "raten"))
        task->has_guess = 1;

    if (has_word(buf, "random") || has_word(buf, "zufall") || has_word(buf, "rand"))
        task->has_random = 1;

    if (has_word(buf, "point") || has_word(buf, "zeiger") || has_word(buf, "adresse")
        || has_word(buf, "address") || has_word(buf, "memory") || has_word(buf, "mem"))
        task->has_pointer = 1;

    if (has_word(buf, "struct") || has_word(buf, "dotted") || has_word(buf, "punkt")
        || has_word(buf, "struktur") || has_word(buf, "record"))
        task->has_struct = 1;

    if (has_word(buf, "function") || has_word(buf, "funktion") || has_word(buf, "subroutine"))
        task->has_function = 1;

    if (has_word(buf, "hex") || has_word(buf, "binary") || has_word(buf, "binaer"))
        task->has_hex_binary = 1;

    if (has_word(buf, "average") || has_word(buf, "durchschnitt") || has_word(buf, "mean"))
        task->has_average = 1;

    if (has_word(buf, "fizzbuzz") || has_word(buf, "fizz buzz") || has_word(buf, "fizz"))
        task->has_fizzbuzz = 1;

    if (has_word(buf, "prime") || has_word(buf, "prim"))
        task->has_primes = 1;

    if (has_word(buf, "sum") || has_word(buf, "summe") || has_word(buf, "add"))
        task->has_sum = 1;

    if (strcmp(task->op, "add") == 0) task->has_sum = 1;

    if (has_word(buf, "factorial") || has_word(buf, "fakult"))
        task->has_factorial = 1;

    if (has_word(buf, "fib") || has_word(buf, "fibonacci"))
        task->has_fibonacci = 1;

    if (has_word(buf, "median") || has_word(buf, "mitte"))
        task->has_median = 1;

    if ((has_word(buf, "string") || strstr(buf, "strings") || strstr(buf, "string ")
        || has_word(buf, "zeichen") || has_word(buf, "text")
        || has_word(buf, "wort"))
        && !has_word(buf, "length") && !has_word(buf, "laenge"))
        task->has_string_cat = 1;

    if (has_word(buf, "compare") || has_word(buf, "vergleich") || has_word(buf, "cmp"))
        task->has_string_compare = 1;

    if (has_word(buf, "file") || has_word(buf, "datei"))
        { task->has_read_file = 1; task->has_write_file = 1; }

    if (has_word(buf, "timer") || has_word(buf, "zeit") || has_word(buf, "time")
        || has_word(buf, "benchmark") || has_word(buf, "measure") || has_word(buf, "mess")
        || has_word(buf, "dauer") || has_word(buf, "execution") || has_word(buf, "lauf"))
        task->has_timer = 1;

    if (has_word(buf, "hello") || has_word(buf, "hallo") || has_word(buf, "name")
        || (has_word(buf, "what") && has_word(buf, "your")))
        task->has_hello_name = 1;

    if ((has_word(buf, "even") || has_word(buf, "odd") || has_word(buf, "gerade") || has_word(buf, "ungerade")))
        task->has_even_odd = 1;

    // enhanced pattern detection
    if ((has_word(buf, "sum") || has_word(buf, "summe")) && has_word(buf, "to") && task->has_literals) {
        task->has_sum_range = 1;
        if (task->num_literals >= 1) task->sum_range_n = task->literals[0];
        else task->sum_range_n = 100;
        // override generic flags
        task->has_algorithm = 0;
        task->has_operation = 0;
    }

    // "count from 1 to 20" → treat as sum_range (prints numbers in range)
    if ((has_word(buf, "count") || has_word(buf, "zähle") || has_word(buf, "zaehle"))
        && has_word(buf, "to") && task->has_literals) {
        task->has_sum_range = 1;
        if (task->num_literals >= 2) task->sum_range_n = task->literals[1];
        else if (task->num_literals >= 1) task->sum_range_n = task->literals[0];
        else task->sum_range_n = 100;
        task->has_algorithm = 0;
        task->has_operation = 0;
    }

    if ((has_word(buf, "even") || has_word(buf, "gerade")) && task->has_literals
        && !has_word(buf, "ungerade") && !has_word(buf, "odd")) {
        task->has_print_even = 1;
        if (task->num_literals >= 1) task->print_even_n = task->literals[0];
        else task->print_even_n = 100;
        task->has_algorithm = 0;
    }

    if (has_word(buf, "find") && (has_word(buf, "max") || has_word(buf, "largest") || has_word(buf, "greatest") || has_word(buf, "größt"))) {
        task->has_find_max = 1;
        if (task->num_literals >= 1) task->find_max_count = task->literals[0];
        else task->find_max_count = 5;
        task->has_input = 1;
        task->has_algorithm = 0;
    }

    if (has_word(buf, "fib") && task->has_literals) {
        task->has_fib_seq = 1;
        if (task->num_literals >= 1) task->fib_seq_n = task->literals[0];
        else task->fib_seq_n = 10;
        task->has_algorithm = 0;
    }

    if ((has_word(buf, "countdown") || strstr(buf, "count down")) && task->has_literals) {
        task->has_countdown_from = 1;
        if (task->num_literals >= 1) task->countdown_start = task->literals[0];
        else task->countdown_start = 10;
        task->has_algorithm = 0;
    }

    if (task->has_sort && (task->has_input || task->has_descending)) {
        task->has_input_sort = 1;
        if (task->input_count > 0) task->input_sort_count = task->input_count;
        else if (task->num_literals >= 1) task->input_sort_count = task->literals[0];
        else task->input_sort_count = 5;
        task->has_algorithm = 0;
    }

    if (task->has_median && task->has_input) {
        if (task->input_count > 0) task->median_count = task->input_count;
        else if (task->num_literals >= 1) task->median_count = task->literals[0];
        else task->median_count = 5;
        task->has_algorithm = 0;
    }

    // "first N primes", "first N fibonacci" patterns
    if (task->has_primes && !task->has_input && task->num_literals == 0) {
        if (has_word(buf, "first") || has_word(buf, "erste") || has_word(buf, "ersten")) {
            // need to find the number after "first" - extract from raw prompt
            int nums2[MAX_NUMS];
            int n2 = extract_numbers(prompt, nums2, MAX_NUMS);
            if (n2 > 0) { task->num_literals = n2; task->has_literals = 1; task->literals[0] = nums2[0]; }
        }
    }
    if (task->has_primes && (has_word(buf, "up") || has_word(buf, "bis")) && has_word(buf, "to") && task->num_literals == 0) {
        int nums2[MAX_NUMS];
        int n2 = extract_numbers(prompt, nums2, MAX_NUMS);
        if (n2 > 0) { task->num_literals = n2; task->has_literals = 1; task->literals[0] = nums2[0]; }
    }
    if (task->has_fib_seq && task->num_literals == 0 &&
        (has_word(buf, "first") || has_word(buf, "erste") || has_word(buf, "ersten"))) {
        int nums2[MAX_NUMS];
        int n2 = extract_numbers(prompt, nums2, MAX_NUMS);
        if (n2 > 0) { task->num_literals = n2; task->has_literals = 1; task->literals[0] = nums2[0]; }
        if (task->num_literals >= 1) task->fib_seq_n = task->literals[0];
        else task->fib_seq_n = 10;
    }

    // "from X to Y" pattern for sum_range
    if (has_word(buf, "from") || has_word(buf, "von")) {
        if (task->has_sum_range && task->num_literals >= 2) {
            task->has_sum_range = 1;
            task->sum_range_n = task->literals[1];
        }
    }

    // "up to N" / "bis N" for countdown
    if (task->has_countdown_from && task->num_literals == 0 &&
        (has_word(buf, "up") || has_word(buf, "bis"))) {
        int nums2[MAX_NUMS];
        int n2 = extract_numbers(prompt, nums2, MAX_NUMS);
        if (n2 > 0) { task->num_literals = n2; task->has_literals = 1; task->literals[0] = nums2[0]; }
        if (task->num_literals >= 1) task->countdown_start = task->literals[0];
        else task->countdown_start = 10;
    }

    if (task->has_input && task->has_factorial) {
        task->has_input_fact = 1;
        task->has_algorithm = 0;
    }

    if (task->has_string_cat && !has_word(buf, "compare") && !has_word(buf, "vergleich")) {
        /* string_cat unless compare is explicit (intentionally empty) */
    }

    if (has_word(buf, "array") && has_word(buf, "reverse")) {
        task->has_array_reverse = 1;
        task->has_algorithm = 0;
    }
    if (has_word(buf, "reverse") && (has_word(buf, "element") || has_word(buf, "list") || has_word(buf, "array") || has_word(buf, "order") || has_word(buf, "reihenfolge"))) {
        task->has_array_reverse = 1;
        task->has_algorithm = 0;
    }

    if ((has_word(buf, "array") && (has_word(buf, "find") || has_word(buf, "search") || has_word(buf, "suche"))) || (has_word(buf, "search") && has_word(buf, "array"))) {
        task->has_array_find = 1;
        task->has_algorithm = 0;
    }

    if (has_word(buf, "array") && has_word(buf, "assign")) {
        task->has_array_assign = 1;
        task->has_algorithm = 0;
    }

    if ((has_word(buf, "array") && (has_word(buf, "min") || has_word(buf, "max") || has_word(buf, "average") || has_word(buf, "vmath"))) || (has_word(buf, "stat") && has_word(buf, "array"))) {
        task->has_array_vmath = 1;
        task->has_algorithm = 0;
    }

    if (has_word(buf, "read") && has_word(buf, "file")) {
        task->has_read_file = 1;
        task->has_algorithm = 0;
    }

    if (has_word(buf, "write") && has_word(buf, "file")) {
        task->has_write_file = 1;
        task->has_algorithm = 0;
    }

    if (has_word(buf, "string") && (has_word(buf, "to") || has_word(buf, "parse") || has_word(buf, "convert")) && has_word(buf, "number")) {
        task->has_string_to_num = 1;
        task->has_algorithm = 0;
    }

    if (has_word(buf, "min") && has_word(buf, "max") && has_word(buf, "array")) {
        task->has_array_min_max = 1;
        task->has_algorithm = 0;
    }

    if (has_word(buf, "bool") || strstr(buf, "boolean")) {
        task->has_bool_demo = 1;
        task->has_algorithm = 0;
    }

    if ((has_word(buf, "bit") || has_word(buf, "check")) && (has_word(buf, "check") || has_word(buf, "set") || has_word(buf, "clear"))) {
        task->has_bit_check = 1;
        task->has_algorithm = 0;
    }

    if ((has_word(buf, "leap") || has_word(buf, "schalt")) && (has_word(buf, "year") || has_word(buf, "jahr"))) {
        task->has_leap_year = 1;
        task->has_algorithm = 0;
    }

    if ((has_word(buf, "celsius") || has_word(buf, "fahrenheit") || has_word(buf, "temperature")) && (has_word(buf, "convert") || has_word(buf, "umrechnen") || has_word(buf, "to"))) {
        task->has_temp_convert = 1;
        task->has_algorithm = 0;
    }

    if ((has_word(buf, "circle") || has_word(buf, "kreis")) && (has_word(buf, "area") || has_word(buf, "fläche") || has_word(buf, "flaeche"))) {
        task->has_circle_area = 1;
        task->has_algorithm = 0;
    }

    if (has_word(buf, "palindrome") || (has_word(buf, "reverse") && has_word(buf, "number")))
        task->has_palindrome = 1;

    if (has_word(buf, "lcm") || (has_word(buf, "least") && has_word(buf, "multiple")))
        task->has_lcm = 1;

    if (has_word(buf, "collatz") || (has_word(buf, "3n") && has_word(buf, "1")))
        task->has_collatz = 1;

    if (has_word(buf, "sum") && has_word(buf, "digit"))
        task->has_sum_of_digits = 1;

    if (has_word(buf, "reverse") && (has_word(buf, "string") || has_word(buf, "text") || has_word(buf, "wort")))
        task->has_reverse_string = 1;

    if (has_word(buf, "armstrong"))
        task->has_armstrong = 1;

    if ((has_word(buf, "perfect") || has_word(buf, "vollkommen")) && has_word(buf, "number"))
        task->has_perfect_number = 1;

    if (has_word(buf, "vowel") || has_word(buf, "vokale") || has_word(buf, "selbstlaut"))
        task->has_count_vowels = 1;

    if (has_word(buf, "anagram"))
        task->has_anagram_check = 1;

    if ((has_word(buf, "string") || has_word(buf, "zeichen") || has_word(buf, "text")) && has_word(buf, "upper"))
        task->has_string_to_upper = 1;

    if ((has_word(buf, "string") || has_word(buf, "zeichen") || has_word(buf, "text")) && has_word(buf, "lower"))
        task->has_string_to_lower = 1;

    if (has_word(buf, "caesar") || (has_word(buf, "shift") && has_word(buf, "cipher")))
        task->has_caesar_cipher = 1;

    if (has_word(buf, "palindrome") && (has_word(buf, "string") || has_word(buf, "text") || has_word(buf, "wort")))
        task->has_palindrome_string = 1;

    if (has_word(buf, "bubble") && has_word(buf, "sort"))
        task->has_bubble_sort = 1;

    if (has_word(buf, "binary") && has_word(buf, "search") && !has_word(buf, "tree"))
        task->has_binary_search = 1;

    if (has_word(buf, "square") && has_word(buf, "root"))
        task->has_square_root = 1;

    if (has_word(buf, "prime") && (has_word(buf, "factor") || has_word(buf, "teil")))
        task->has_prime_factorization = 1;

    if (has_word(buf, "standard") && has_word(buf, "deviation"))
        task->has_standard_deviation = 1;

    if (has_word(buf, "compound") && has_word(buf, "interest"))
        task->has_compound_interest = 1;

    if (has_word(buf, "binary") && (has_word(buf, "decimal") || has_word(buf, "convert") || has_word(buf, "base")))
        task->has_decimal_to_binary = 1;

    if (has_word(buf, "dice") || has_word(buf, "würfel") || (has_word(buf, "roll") && has_word(buf, "die")))
        task->has_dice_roll = 1;

    if (has_word(buf, "double") && has_word(buf, "math"))
        task->has_double_math = 1;

    if (has_word(buf, "double") && has_word(buf, "circle"))
        task->has_double_circle_area = 1;

    if (has_word(buf, "double") && has_word(buf, "average"))
        task->has_double_average = 1;

    if (has_word(buf, "double") && has_word(buf, "interest"))
        task->has_double_compound_interest = 1;

    if (has_word(buf, "double") && has_word(buf, "pythagoras"))
        task->has_double_pythagoras = 1;

    if (has_word(buf, "double") && has_word(buf, "temp"))
        task->has_double_temp_convert = 1;

    if (has_word(buf, "double") && has_word(buf, "sqrt"))
        task->has_double_sqrt = 1;

    if (has_word(buf, "double") && has_word(buf, "power"))
        task->has_double_power = 1;

    if (has_word(buf, "double") && has_word(buf, "volume") && has_word(buf, "sphere"))
        task->has_double_volume_sphere = 1;

    if (has_word(buf, "double") && has_word(buf, "discount"))
        task->has_double_discount = 1;

    if (has_word(buf, "double") && has_word(buf, "simple") && has_word(buf, "interest"))
        task->has_double_simple_interest = 1;

    if (has_word(buf, "double") && has_word(buf, "bmi"))
        task->has_double_bmi = 1;

    if ((has_word(buf, "double") && has_word(buf, "stddev")) || (has_word(buf, "double") && has_word(buf, "standard") && has_word(buf, "deviation")))
        task->has_double_standard_deviation = 1;

    if (has_word(buf, "double") && (has_word(buf, "kinetic") || has_word(buf, "energy")))
        task->has_double_kinetic_energy = 1;

    /* Clear integer flags when double variant is detected to prevent
       both integer and double rules from generating conflicting code */
    if (task->has_double_circle_area) task->has_circle_area = 0;
    if (task->has_double_temp_convert) task->has_temp_convert = 0;

    if ((has_word(buf, "string") && has_word(buf, "length"))
        || (has_word(buf, "string") && has_word(buf, "laenge"))
        || (has_word(buf, "zeichen") && has_word(buf, "laenge")))
        task->has_string_length = 1;

    if (has_word(buf, "stack") || (has_word(buf, "push") && has_word(buf, "pop")))
        task->has_stack = 1;

    if (has_word(buf, "queue") || (has_word(buf, "enqueue") && has_word(buf, "dequeue")))
        task->has_queue = 1;

    if (has_word(buf, "insertion") && has_word(buf, "sort"))
        task->has_insertion_sort = 1;

    if (has_word(buf, "calculator") || (has_word(buf, "calc") && has_word(buf, "repl")))
        task->has_calculator = 1;

    if ((has_word(buf, "unit") || has_word(buf, "einheit")) && (has_word(buf, "convert") || has_word(buf, "converter") || has_word(buf, "umrechnen") || has_word(buf, "umwandeln")))
        task->has_unit_converter = 1;

    if (has_word(buf, "rock") && has_word(buf, "paper") && has_word(buf, "scissors"))
        task->has_rock_paper_scissors = 1;

    if ((has_word(buf, "pyramid") || has_word(buf, "pyramide")) && (has_word(buf, "number") || has_word(buf, "zahl") || has_word(buf, "pattern") || has_word(buf, "muster")))
        task->has_pyramid = 1;

    if ((has_word(buf, "temperature") || has_word(buf, "temperatur")) && (has_word(buf, "convert") || has_word(buf, "converter") || has_word(buf, "umrechnen")))
        task->has_temp_converter_menu = 1;

    if (has_word(buf, "sort") && (has_word(buf, "stats") || has_word(buf, "statistics") || has_word(buf, "statistik") || has_word(buf, "analyze") || has_word(buf, "analyse") || has_word(buf, "min") || has_word(buf, "max") || has_word(buf, "average") || has_word(buf, "avg")))
        task->has_sort_stats = 1;

    if ((has_word(buf, "analyze") || has_word(buf, "analyse")) && (has_word(buf, "string") || has_word(buf, "text") || has_word(buf, "zeichenkette")))
        task->has_string_analyzer = 1;

    if ((has_word(buf, "analyze") || has_word(buf, "analyse")) && (has_word(buf, "number") || has_word(buf, "zahl")))
        task->has_number_analyzer = 1;

    if (has_word(buf, "filter") && (has_word(buf, "number") || has_word(buf, "numbers") || has_word(buf, "zahlen") || has_word(buf, "values") || has_word(buf, "werte")))
        task->has_filter_numbers = 1;

    if ((has_word(buf, "random") || has_word(buf, "zufall")) && (has_word(buf, "generator") || has_word(buf, "generieren")))
        task->has_random_generator = 1;
    if (task->has_random_generator)
        task->has_random = 0;

    if ((has_word(buf, "math") || has_word(buf, "mathe")) && (has_word(buf, "menu") || has_word(buf, "menue") || has_word(buf, "rechner")))
        task->has_math_menu = 1;

    if ((has_word(buf, "quiz") || has_word(buf, "frage")) && (has_word(buf, "game") || has_word(buf, "spiel")))
        task->has_quiz_game = 1;

    if (has_word(buf, "bmi") || ((has_word(buf, "body") || has_word(buf, "koerper")) && has_word(buf, "mass") && has_word(buf, "index")))
        task->has_bmi_calculator = 1;

    if ((has_word(buf, "statistics") || has_word(buf, "statistik")) && (has_word(buf, "all") || has_word(buf, "suite") || has_word(buf, "alle") || has_word(buf, "full") || has_word(buf, "komplett")))
        task->has_statistics_suite = 1;

    if (has_word(buf, "linked") && has_word(buf, "list"))
        task->has_linked_list = 1;
    if ((has_word(buf, "binary") || has_word(buf, "binaer")) && has_word(buf, "tree") && has_word(buf, "search"))
        task->has_binary_search_tree = 1;
    if (has_word(buf, "tree") && (has_word(buf, "traversal") || has_word(buf, "inorder") || has_word(buf, "preorder") || has_word(buf, "postorder")))
        task->has_tree_traversal = 1;
    if ((has_word(buf, "graph") || has_word(buf, "graf")) && (has_word(buf, "bfs") || has_word(buf, "dfs") || has_word(buf, "traverse") || has_word(buf, "durchlauf")))
        task->has_graph_bfs_dfs = 1;
    if ((has_word(buf, "n") || has_word(buf, "eight") || has_word(buf, "acht")) && has_word(buf, "queen"))
        task->has_n_queens = 1;
    if (has_word(buf, "sudoku") || (has_word(buf, "sodoku") && has_word(buf, "solver")))
        task->has_sudoku = 1;
    if ((has_word(buf, "levenshtein") || has_word(buf, "edit")) && has_word(buf, "distance"))
        task->has_levenshtein = 1;
    if ((has_word(buf, "maze") || has_word(buf, "labyrinth") || has_word(buf, "irrgarten")) && (has_word(buf, "generate") || has_word(buf, "generieren") || has_word(buf, "erzeugen")))
        task->has_maze_generator = 1;
    if ((has_word(buf, "maze") || has_word(buf, "labyrinth") || has_word(buf, "irrgarten")) && (has_word(buf, "solve") || has_word(buf, "solver") || has_word(buf, "loesen") || has_word(buf, "lösung")))
        task->has_maze_solver = 1;
    if ((has_word(buf, "monte") && has_word(buf, "carlo")) || (has_word(buf, "pi") && has_word(buf, "estimate")))
        task->has_monte_carlo = 1;
    if ((has_word(buf, "matrix") || has_word(buf, "matrize")) && has_word(buf, "multiply"))
        task->has_matrix_mul = 1;
    if (has_word(buf, "matrix") && (has_word(buf, "transpose") || has_word(buf, "transponieren")))
        task->has_matrix_transpose = 1;
    if ((has_word(buf, "numerical") || has_word(buf, "numerisch") || has_word(buf, "numeric")) && (has_word(buf, "integrate") || has_word(buf, "integration") || has_word(buf, "integral")))
        task->has_numerical_integration = 1;
    if ((has_word(buf, "complex") || has_word(buf, "komplex")) && has_word(buf, "number"))
        task->has_complex_numbers = 1;
    if ((has_word(buf, "linear") || has_word(buf, "linreg")) && (has_word(buf, "regression") || has_word(buf, "regress")))
        task->has_linear_regression = 1;
    if ((has_word(buf, "base") || has_word(buf, "basis")) && (has_word(buf, "convert") || has_word(buf, "converter") || has_word(buf, "umwandeln") || has_word(buf, "konvert")))
        task->has_base_converter = 1;
    if ((has_word(buf, "frequency") || has_word(buf, "haeufigkeit") || has_word(buf, "frequenz")) && (has_word(buf, "analyze") || has_word(buf, "analyse") || has_word(buf, "count") || has_word(buf, "zaehle")))
        task->has_freq_analysis = 1;
    if (has_word(buf, "shuffle") || has_word(buf, "mischen") || (has_word(buf, "random") && has_word(buf, "permute")))
        task->has_shuffle = 1;
    if ((has_word(buf, "weighted") || has_word(buf, "gewichtet")) && (has_word(buf, "random") || has_word(buf, "zufall")))
        task->has_weighted_random = 1;
    if (has_word(buf, "ascii") && (has_word(buf, "table") || has_word(buf, "tabelle") || has_word(buf, "format")))
        task->has_ascii_table = 1;

    if ((has_word(buf, "password") || has_word(buf, "passwort") || has_word(buf, "passwd")) && (has_word(buf, "card") || has_word(buf, "karte") || has_word(buf, "generate") || has_word(buf, "generator")))
        task->has_password_card = 1;
    if ((has_word(buf, "chess") || has_word(buf, "schach") || has_word(buf, "rice") || has_word(buf, "reis")) && (has_word(buf, "problem") || has_word(buf, "board") || has_word(buf, "brett") || has_word(buf, "exponential")))
        task->has_chess_problem = 1;
    if ((has_word(buf, "repl") || has_word(buf, "terminal")) && (has_word(buf, "run") || has_word(buf, "execute") || has_word(buf, "ausfuehren") || has_word(buf, "ausführen")))
        task->has_shell_repl = 1;
    if ((has_word(buf, "shell") || has_word(buf, "interactive")) && (has_word(buf, "run") || has_word(buf, "running") || has_word(buf, "execute") || has_word(buf, "command") || has_word(buf, "ausfuehren") || has_word(buf, "ausführen") || has_word(buf, "befehl")))
        task->has_shell_repl = 1;

    if (has_word(buf, "webserver") || has_word(buf, "http") || (has_word(buf, "web") && has_word(buf, "server")))
        task->has_webserver = 1;
    if ((has_word(buf, "sdl") || has_word(buf, "screen") || has_word(buf, "bildschirm")) && (has_word(buf, "open") || has_word(buf, "window") || has_word(buf, "fenster") || has_word(buf, "pixel") || has_word(buf, "grafik") || has_word(buf, "graphics")))
        task->has_sdl_window = 1;
    if ((has_word(buf, "sdl") || has_word(buf, "gui") || has_word(buf, "gadget")) && (has_word(buf, "button") || has_word(buf, "knopf") || has_word(buf, "click")))
        task->has_sdl_button = 1;
    if (has_word(buf, "thread") || has_word(buf, "nebenlauf") || (has_word(buf, "parallel") && has_word(buf, "run")))
        task->has_thread = 1;
    if (has_word(buf, "scheduler") || has_word(buf, "planer") || (has_word(buf, "task") && has_word(buf, "schedule")))
        task->has_scheduler = 1;
    if ((has_word(buf, "shell") || has_word(buf, "command") || has_word(buf, "befehl") || has_word(buf, "kommando")) && (has_word(buf, "exec") || has_word(buf, "run") || has_word(buf, "ausführen") || has_word(buf, "ausfuehren")))
        task->has_shell_exec = 1;
    if (has_word(buf, "json") || has_word(buf, "javascript") || (has_word(buf, "parse") && has_word(buf, "json")))
        task->has_json = 1;
    if (has_word(buf, "crypto") || has_word(buf, "encrypt") || has_word(buf, "decrypt") || has_word(buf, "verschlüssel") || has_word(buf, "chiffre") || has_word(buf, "cipher"))
        task->has_crypto = 1;
    if (has_word(buf, "bluetooth") || has_word(buf, "ble") || (has_word(buf, "bluetooth") && has_word(buf, "low")))
        task->has_bluetooth_ble = 1;
    if ((has_word(buf, "serial") || has_word(buf, "rs232") || has_word(buf, "seriell")) && (has_word(buf, "port") || has_word(buf, "schnittstelle") || has_word(buf, "interface")))
        task->has_serial_rs232 = 1;
    if (has_word(buf, "gpio") || (has_word(buf, "general") && has_word(buf, "purpose")))
        task->has_gpio = 1;
    if (has_word(buf, "gps") || has_word(buf, "navigation") || (has_word(buf, "global") && has_word(buf, "position")))
        task->has_gps = 1;
    if ((has_word(buf, "date") || has_word(buf, "datum") || has_word(buf, "kalender") || has_word(buf, "calendar")) && (has_word(buf, "time") || has_word(buf, "zeit") || has_word(buf, "uhr") || has_word(buf, "clock")))
        task->has_timer_date = 1;
    if ((has_word(buf, "sdl") || has_word(buf, "sound") || has_word(buf, "audio") || has_word(buf, "ton")) && (has_word(buf, "play") || has_word(buf, "spielen") || has_word(buf, "abspielen") || has_word(buf, "wiedergabe")))
        task->has_sdl_sound = 1;
    if ((has_word(buf, "sdl") || has_word(buf, "joystick") || has_word(buf, "gamepad") || has_word(buf, "controller")) && (has_word(buf, "joystick") || has_word(buf, "axis") || has_word(buf, "achse") || has_word(buf, "button")))
        task->has_sdl_joystick = 1;
    if ((has_word(buf, "sdl") || has_word(buf, "mouse") || has_word(buf, "maus")) && (has_word(buf, "mouse") || has_word(buf, "maus") || has_word(buf, "pointer") || has_word(buf, "cursor")))
        task->has_sdl_mouse = 1;
    if (has_word(buf, "fractal") || has_word(buf, "mandelbrot") || has_word(buf, "julia") || (has_word(buf, "fraktal") && has_word(buf, "set")))
        task->has_fractal = 1;
    if ((has_word(buf, "cluster") || has_word(buf, "worker") || has_word(buf, "verteilt") || has_word(buf, "distributed")) && (has_word(buf, "compute") || has_word(buf, "rechnen") || has_word(buf, "3x1") || has_word(buf, "arbeit")))
        task->has_cluster_3x1 = 1;
    if (has_word(buf, "reload") || has_word(buf, "module") || (has_word(buf, "hot") && has_word(buf, "swap")) || (has_word(buf, "neu") && has_word(buf, "laden")))
        task->has_reload = 1;
    if ((has_word(buf, "coordinate") || has_word(buf, "koordinate") || has_word(buf, "grid") || has_word(buf, "gitter")) && (has_word(buf, "xy") || has_word(buf, "2d") || has_word(buf, "position") || has_word(buf, "raster")))
        task->has_coordinate_grid = 1;
    if (has_word(buf, "turmite") || (has_word(buf, "turmite") && has_word(buf, "ant")))
        task->has_turmite = 1;
    if (has_word(buf, "crossword") || has_word(buf, "kreuzwort") || has_word(buf, "rätsel") || (has_word(buf, "word") && has_word(buf, "puzzle")))
        task->has_crossword = 1;
    if (has_word(buf, "linter") || has_word(buf, "lint") || (has_word(buf, "code") && has_word(buf, "check")) || (has_word(buf, "static") && has_word(buf, "analyze")))
        task->has_linter = 1;

    // new small emitters
    if (has_word(buf, "hello") && has_word(buf, "world"))
        task->has_hello_world = 1;
    if (has_word(buf, "string") && has_word(buf, "find") && !has_word(buf, "max") && !has_word(buf, "largest"))
        task->has_string_find = 1;
    if ((has_word(buf, "string") || has_word(buf, "text")) && has_word(buf, "split"))
        task->has_string_split = 1;
    if (has_word(buf, "switch") && !has_word(buf, "string"))
        task->has_switch_demo = 1;
    if (has_word(buf, "type") && (has_word(buf, "convert") || has_word(buf, "conversion")
        || has_word(buf, "umwandeln") || has_word(buf, "cast")))
        task->has_type_convert = 1;
    if ((has_word(buf, "factorial") || has_word(buf, "fakult")) && (has_word(buf, "loop") || has_word(buf, "iterative") || has_word(buf, "schleife") || has_word(buf, "while")))
        task->has_iterative_factorial = 1;
    if (has_word(buf, "random") && has_word(buf, "walk"))
        task->has_random_walk = 1;
    if (has_word(buf, "bar") && (has_word(buf, "chart") || has_word(buf, "graph") || has_word(buf, "diagramm")))
        task->has_bar_chart = 1;
    if (has_word(buf, "hanoi") || (has_word(buf, "tower") && has_word(buf, "hanoi"))
        || (has_word(buf, "turm") && has_word(buf, "hanoi")))
        task->has_hanoi_tower = 1;
    if (has_word(buf, "ascii") && has_word(buf, "art"))
        task->has_ascii_art = 1;
    if ((has_word(buf, "number") || has_word(buf, "zahl")) && (has_word(buf, "word") || has_word(buf, "wort") || has_word(buf, "text")))
        task->has_number_to_words = 1;
    if ((has_word(buf, "temperature") || has_word(buf, "temperatur")) && (has_word(buf, "table") || has_word(buf, "tabelle")))
        task->has_temperature_table = 1;
    if (has_word(buf, "loop") && (has_word(buf, "demo") || has_word(buf, "beispiel") || has_word(buf, "example")))
        task->has_loop_demo = 1;
    if ((has_word(buf, "time") || has_word(buf, "zeit") || has_word(buf, "uhr") || has_word(buf, "clock"))
        && (has_word(buf, "demo") || has_word(buf, "beispiel") || has_word(buf, "example") || has_word(buf, "current")))
        task->has_time = 1;
    if (has_word(buf, "shell") && (has_word(buf, "arg") || has_word(buf, "parameter") || has_word(buf, "command") || has_word(buf, "befehl")))
        task->has_shell_args = 1;

    if (has_word(buf, "fann") || has_word(buf, "neural") || (has_word(buf, "network") && has_word(buf, "ai"))) {
        if (has_word(buf, "train") || has_word(buf, "learn")) {
            task->has_fann_create = 1;
            task->has_fann_train = 1;
            task->has_algorithm = 0;
        } else if (has_word(buf, "run") || has_word(buf, "predict") || has_word(buf, "infer")) {
            task->has_fann_run = 1;
            task->has_algorithm = 0;
        } else if (has_word(buf, "create") || has_word(buf, "make") || has_word(buf, "generate")) {
            task->has_fann_create = 1;
            task->has_algorithm = 0;
        } else {
            task->has_fann_create = 1;
            task->has_fann_train = 1;
            task->has_fann_run = 1;
            task->has_algorithm = 0;
        }
    }

    // build title
    { SNPRINTF_CHECK(task->title, sizeof(task->title), "%s", prompt); }

    // has action if any flag set
    int has_any = task->has_input || task->has_output || task->has_operation || task->has_algorithm
        || task->has_loop || task->has_condition || task->has_sort || task->has_power
        || task->has_max || task->has_gcd || task->has_countdown || task->has_mult_table
        || task->has_guess || task->has_random || task->has_hello_name || task->has_time || task->has_pointer
        || task->has_struct || task->has_hex_binary || task->has_shell_args || task->has_array
        || task->has_function || task->has_average || task->has_fizzbuzz || task->has_even_odd
        || task->has_primes || task->has_sum || task->has_factorial || task->has_fibonacci
        || task->has_median || task->has_string_cat || task->has_string_compare
        || task->has_array_assign || task->has_array_reverse || task->has_array_find
        || task->has_sum_range || task->has_print_even || task->has_find_max
        || task->has_fib_seq || task->has_countdown_from || task->has_input_sort
        || task->has_input_fact || task->has_read_file || task->has_write_file
        || task->has_array_vmath || task->has_string_to_num || task->has_timer
        || task->has_array_min_max || task->has_bool_demo || task->has_print_var || task->has_bit_check
        || task->has_leap_year || task->has_temp_convert || task->has_circle_area
        || task->has_fann_create || task->has_fann_train || task->has_fann_run
        || task->has_palindrome || task->has_lcm || task->has_collatz
        || task->has_sum_of_digits || task->has_reverse_string || task->has_armstrong
        || task->has_perfect_number || task->has_count_vowels || task->has_anagram_check
        || task->has_string_to_upper || task->has_string_to_lower || task->has_caesar_cipher
        || task->has_palindrome_string || task->has_bubble_sort || task->has_binary_search
        || task->has_square_root || task->has_prime_factorization || task->has_standard_deviation
        || task->has_compound_interest || task->has_decimal_to_binary || task->has_dice_roll
        || task->has_double_math || task->has_double_circle_area || task->has_double_average
        || task->has_double_compound_interest || task->has_double_pythagoras
         || task->has_double_temp_convert || task->has_double_sqrt
        || task->has_double_power || task->has_double_volume_sphere
        || task->has_double_discount || task->has_double_simple_interest
        || task->has_double_bmi || task->has_double_standard_deviation
        || task->has_double_kinetic_energy
        || task->has_string_length || task->has_stack || task->has_queue
        || task->has_insertion_sort || task->has_calculator || task->has_unit_converter
        || task->has_rock_paper_scissors || task->has_pyramid
        || task->has_temp_converter_menu || task->has_sort_stats
        || task->has_string_analyzer || task->has_number_analyzer
        || task->has_filter_numbers || task->has_random_generator
        || task->has_math_menu || task->has_quiz_game
        || task->has_bmi_calculator || task->has_statistics_suite
        || task->has_linked_list || task->has_binary_search_tree
        || task->has_tree_traversal || task->has_graph_bfs_dfs
        || task->has_n_queens || task->has_sudoku
        || task->has_levenshtein || task->has_maze_generator
        || task->has_maze_solver || task->has_monte_carlo
        || task->has_matrix_mul || task->has_matrix_transpose
        || task->has_numerical_integration || task->has_complex_numbers
        || task->has_linear_regression || task->has_base_converter
        || task->has_freq_analysis || task->has_shuffle
        || task->has_weighted_random || task->has_ascii_table
        || task->has_bignum_math || task->has_password_card
        || task->has_chess_problem || task->has_shell_repl
        || task->has_webserver || task->has_sdl_window || task->has_sdl_button
        || task->has_thread || task->has_scheduler || task->has_shell_exec
        || task->has_json || task->has_crypto || task->has_bluetooth_ble
        || task->has_serial_rs232 || task->has_gpio || task->has_gps
        || task->has_timer_date || task->has_sdl_sound || task->has_sdl_joystick
        || task->has_sdl_mouse || task->has_fractal || task->has_cluster_3x1
        || task->has_reload || task->has_coordinate_grid || task->has_turmite
         || task->has_crossword || task->has_linter
         || task->has_hello_world || task->has_string_find || task->has_string_split
         || task->has_switch_demo || task->has_type_convert || task->has_iterative_factorial
         || task->has_random_walk || task->has_bar_chart || task->has_hanoi_tower
         || task->has_ascii_art || task->has_number_to_words || task->has_temperature_table
         || task->has_loop_demo;

    SNPRINTF_CHECK(task->title, sizeof(task->title), "%s", prompt);

    // Negation post-processing: clear flags if keyword is negated
    if (task->has_sort && is_negated(buf, "sort")) task->has_sort = 0;
    if (task->has_loop && is_negated(buf, "loop")) task->has_loop = 0;
    if (task->has_condition && is_negated(buf, "if")) task->has_condition = 0;
    if (task->has_input && is_negated(buf, "input")) task->has_input = 0;
    if (task->has_output && is_negated(buf, "print")) task->has_output = 0;
    if (task->has_sum && is_negated(buf, "sum")) task->has_sum = 0;
    if (task->has_average && is_negated(buf, "average")) task->has_average = 0;
    if (task->has_power && is_negated(buf, "power")) task->has_power = 0;
    if (task->has_gcd && is_negated(buf, "gcd")) task->has_gcd = 0;

    return has_any;
}
static void ensure_exit(Function *f, int last_step) {
    if (!last_step) return;
    const char *zv[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    func_append(f, "\t(zero :exit !)");
}

int generate_from_task(Program *prog, TaskProfile *task, int last_step) {
    // check if main function already exists
    Function *f = NULL;
    int found = 0;
    for (int i = 0; i < prog->num_funcs; i++) {
        if (strcmp(prog->funcs[i].name, "main") == 0) { f = &prog->funcs[i]; found = 1; break; }
    }
    if (!found) {
        add_func(prog, "main");
        f = &prog->funcs[prog->num_funcs - 1];
    }

    add_include(prog, "intr-func.l1h");

    if (dsl_generate_from_task(prog, task, f)) {
        ensure_exit(f, last_step);
        return 1;
    }

    // Handle "sort them" when an array was inherited from a prior step
    if (task->has_sort && task->inherit_var[0]) {
        const char *zv[] = {"0"};
        const char *ov[] = {"1"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        add_var_to_func(f, "const-int64", "one", 1, ov, 1);
        char cvs[16];
        SNPRINTF_CHECK(cvs, sizeof(cvs), "%d", task->inherit_count > 0 ? task->inherit_count : 5);
        const char *cv[] = {cvs};
        add_var_to_func(f, "int64", "count", 1, cv, 1);
        add_var_to_func(f, "int64", "i", 1, zv, 1);
        add_var_to_func(f, "int64", "j", 1, zv, 1);
        add_var_to_func(f, "int64", "temp", 1, zv, 1);
        add_var_to_func(f, "int64", "realind", 1, zv, 1);
        add_var_to_func(f, "int64", "realind2", 1, zv, 1);
        add_var_to_func(f, "int64", "a", 1, zv, 1);
        add_var_to_func(f, "int64", "b", 1, zv, 1);
        add_var_to_func(f, "int64", "x", 1, zv, 1);
        add_var_to_func(f, "int64", "y", 1, zv, 1);
        add_var_to_func(f, "int64", "f", 1, zv, 1);
        func_append(f, "\t// bubble sort");
        func_append(f, "\t(zero i :=)");
        func_append(f, "\t(for-loop)");
        func_append(f, "\t(((i count <) f :=) f for)");
        func_append(f, "\t\t(zero j :=)");
        func_append(f, "\t\t(count one - temp :=)");
        func_append(f, "\t\t(for-loop)");
        func_append(f, "\t\t(((j temp <) f :=) f for)");
        func_append(f, "\t\t\t(j * int64_size realind :=)");
        func_append(f, "\t\t\t((j + one) * int64_size realind2 :=)");
        char ln[512];
        SNPRINTF_CHECK(ln, sizeof(ln), "\t\t\t(%s [ realind ] a =)", task->inherit_var);
        func_append(f, ln);
        func_append(f, "\t\t\t(a x :=)");
        SNPRINTF_CHECK(ln, sizeof(ln), "\t\t\t(%s [ realind2 ] b =)", task->inherit_var);
        func_append(f, ln);
        func_append(f, "\t\t\t(b y :=)");
        func_append(f, "\t\t\t(((x y >) f :=) f if)");
        SNPRINTF_CHECK(ln, sizeof(ln), "\t\t\t\t(y %s [ realind ] =)", task->inherit_var);
        func_append(f, ln);
        SNPRINTF_CHECK(ln, sizeof(ln), "\t\t\t\t(x %s [ realind2 ] =)", task->inherit_var);
        func_append(f, ln);
        func_append(f, "\t\t\t(endif)");
        func_append(f, "\t\t\t(j + one j :=)");
        func_append(f, "\t\t(next)");
        func_append(f, "\t\t(i + one i :=)");
        func_append(f, "\t(next)");
        ensure_exit(f, last_step);
        return 1;
    }

    // Handle "print them" when an array was inherited from a prior step
    if (task->has_output && task->inherit_var[0] && !task->has_max && !task->has_min) {
        const char *zv[] = {"0"};
        const char *ov[] = {"1"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        add_var_to_func(f, "const-int64", "one", 1, ov, 1);
        char cvs[16];
        SNPRINTF_CHECK(cvs, sizeof(cvs), "%d", task->inherit_count > 0 ? task->inherit_count : 5);
        const char *cv[] = {cvs};
        add_var_to_func(f, "int64", "count", 1, cv, 1);
        add_var_to_func(f, "int64", "i", 1, zv, 1);
        add_var_to_func(f, "int64", "realind", 1, zv, 1);
        add_var_to_func(f, "int64", "a", 1, zv, 1);
        add_var_to_func(f, "int64", "f", 1, zv, 1);
        func_append(f, "\t// print array");
        func_append(f, "\t(zero i :=)");
        func_append(f, "\t(for-loop)");
        func_append(f, "\t(((i count <) f :=) f for)");
        func_append(f, "\t\t(i * int64_size realind :=)");
        char ln[512];
        SNPRINTF_CHECK(ln, sizeof(ln), "\t\t(%s [ realind ] a =)", task->inherit_var);
        func_append(f, ln);
        func_append(f, "\t\t(a + zero a :=)");
        func_append(f, "\t\t(a :print_i !)");
        func_append(f, "\t\t(:print_n !)");
        func_append(f, "\t\t(i + one i :=)");
        func_append(f, "\t(next)");
        ensure_exit(f, last_step);
        return 1;
    }

    // Handle "print the largest" when an array was inherited from a prior step
    if (task->has_max && task->inherit_var[0]) {
        const char *zv[] = {"0"};
        const char *ov[] = {"1"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        add_var_to_func(f, "const-int64", "one", 1, ov, 1);
        char cvs[16];
        SNPRINTF_CHECK(cvs, sizeof(cvs), "%d", task->inherit_count > 0 ? task->inherit_count : 5);
        const char *cv[] = {cvs};
        add_var_to_func(f, "int64", "count", 1, cv, 1);
    add_var_to_func(f, "int64", "temp", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
        add_var_to_func(f, "int64", "realind", 1, zv, 1);
        add_var_to_func(f, "int64", "a", 1, zv, 1);
        func_append(f, "\t// print largest");
        func_append(f, "\t(count one - temp :=)");
        func_append(f, "\t(temp * int64_size realind :=)");
        char ln[512];
        SNPRINTF_CHECK(ln, sizeof(ln), "\t(%s [ realind ] a =)", task->inherit_var);
        func_append(f, ln);
        func_append(f, "\t(a + zero a :=)");
        func_append(f, "\t(a :print_i !)");
        func_append(f, "\t(:print_n !)");
        ensure_exit(f, last_step);
        return 1;
    }

    // Handle "print the smallest" when an array was inherited from a prior step
    if (task->has_min && task->inherit_var[0]) {
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        add_var_to_func(f, "int64", "realind", 1, zv, 1);
        add_var_to_func(f, "int64", "a", 1, zv, 1);
        char ln[512];
        SNPRINTF_CHECK(ln, sizeof(ln), "\t(zero * int64_size realind :=)");
        func_append(f, ln);
        func_append(f, "\t// print smallest");
        SNPRINTF_CHECK(ln, sizeof(ln), "\t(%s [ realind ] a =)", task->inherit_var);
        func_append(f, ln);
        func_append(f, "\t(a + zero a :=)");
        func_append(f, "\t(a :print_i !)");
        func_append(f, "\t(:print_n !)");
        ensure_exit(f, last_step);
        return 1;
    }

    if (task->has_print_var) {
        add_include(prog, "intr-func.l1h");
        const char *zv[] = {"0"};
        const char *mv[] = {"42"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        add_var_to_func(f, "int64", "myvar", 1, mv, 1);
        func_append(f, "\t// print a variable");
        func_append(f, "\t(myvar :print_i !)");
        func_append(f, "\t(:print_n !)");
        ensure_exit(f, last_step);
        return 1;
    }

    return 0;
}

void gen_arithmetic(Program *prog, const char *op, const char *type, const int *vals, int num_vals) {
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];

    const char *zero_v[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, zero_v, 1);

    const char *sym = "+";
    if (strcmp(op, "sub") == 0) sym = "-";
    else if (strcmp(op, "mul") == 0) sym = "*";
    else if (strcmp(op, "div") == 0) sym = "/";
    else if (strcmp(op, "mod") == 0) sym = "%";

    char line[256], vname[64], vstr[16];
    const char *r_v[] = {"0"};

    for (int i = 0; i < num_vals; i++) {
        SNPRINTF_CHECK(vname, sizeof(vname), "%s_%c", type, 'a' + i);
        SNPRINTF_CHECK(vstr, sizeof(vstr), "%d", vals[i]);
        const char *vv[] = {vstr};
        add_var_to_func(f, type, vname, 1, vv, 1);
    }

    char rname[64];
    SNPRINTF_CHECK(rname, sizeof(rname), "%s_r", type);
    add_var_to_func(f, type, rname, 1, r_v, 1);

    if (num_vals == 2) {
        SNPRINTF_CHECK(line, sizeof(line), "\t(%s_%c %s %s_%c %s :=)", type, 'a', sym, type, 'b', rname);
    } else {
        SNPRINTF_CHECK(line, sizeof(line), "\t((%s_%c %s %s_%c) %s %s_%c %s :=)", type, 'a', sym, type, 'b', sym, type, 'c', rname);
    }
    func_append(f, line);

    func_append(f, "\t// Ergebnis ausgeben:");
    SNPRINTF_CHECK(line, sizeof(line), "\t(%s :print_i !)", rname);
    func_append(f, line);
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");
}

static int word_to_num(const char *word) {
    if (strcmp(word, "null") == 0 || strcmp(word, "zero") == 0) return 0;
    if (strcmp(word, "eins") == 0 || strcmp(word, "one") == 0) return 1;
    if (strcmp(word, "zwei") == 0 || strcmp(word, "two") == 0) return 2;
    if (strcmp(word, "drei") == 0 || strcmp(word, "three") == 0) return 3;
    if (strcmp(word, "vier") == 0 || strcmp(word, "four") == 0) return 4;
    if (strcmp(word, "fünf") == 0 || strcmp(word, "fuenf") == 0 || strcmp(word, "five") == 0) return 5;
    if (strcmp(word, "sechs") == 0 || strcmp(word, "six") == 0) return 6;
    if (strcmp(word, "sieben") == 0 || strcmp(word, "seven") == 0) return 7;
    if (strcmp(word, "acht") == 0 || strcmp(word, "eight") == 0) return 8;
    if (strcmp(word, "neun") == 0 || strcmp(word, "nine") == 0) return 9;
    if (strcmp(word, "zehn") == 0 || strcmp(word, "ten") == 0) return 10;
    if (strcmp(word, "elf") == 0 || strcmp(word, "eleven") == 0) return 11;
    if (strcmp(word, "zwoelf") == 0 || strcmp(word, "zwölf") == 0 || strcmp(word, "twelve") == 0) return 12;
    if (strcmp(word, "dreizehn") == 0 || strcmp(word, "thirteen") == 0) return 13;
    if (strcmp(word, "vierzehn") == 0 || strcmp(word, "fourteen") == 0) return 14;
    if (strcmp(word, "fuenfzehn") == 0 || strcmp(word, "fünfzehn") == 0 || strcmp(word, "fifteen") == 0) return 15;
    if (strcmp(word, "sechzehn") == 0 || strcmp(word, "sixteen") == 0) return 16;
    if (strcmp(word, "siebzehn") == 0 || strcmp(word, "seventeen") == 0) return 17;
    if (strcmp(word, "achtzehn") == 0 || strcmp(word, "eighteen") == 0) return 18;
    if (strcmp(word, "neunzehn") == 0 || strcmp(word, "nineteen") == 0) return 19;
    if (strcmp(word, "zwanzig") == 0 || strcmp(word, "twenty") == 0) return 20;
    if (strcmp(word, "dreissig") == 0 || strcmp(word, "dreißig") == 0 || strcmp(word, "thirty") == 0) return 30;
    if (strcmp(word, "vierzig") == 0 || strcmp(word, "forty") == 0) return 40;
    if (strcmp(word, "fuenfzig") == 0 || strcmp(word, "fünfzig") == 0 || strcmp(word, "fifty") == 0) return 50;
    if (strcmp(word, "sechzig") == 0 || strcmp(word, "sixty") == 0) return 60;
    if (strcmp(word, "siebzig") == 0 || strcmp(word, "seventy") == 0) return 70;
    if (strcmp(word, "achtzig") == 0 || strcmp(word, "eighty") == 0) return 80;
    if (strcmp(word, "neunzig") == 0 || strcmp(word, "ninety") == 0) return 90;
    if (strcmp(word, "hundert") == 0 || strcmp(word, "hundred") == 0) return 100;
    return -1;
}

int smart_generate(Program *prog, const char *prompt, char *desc, int desc_size) {
    init_embeddings();
    index_examples();

    // load learned patterns
    load_learned_patterns();

    // Full-prompt learned pattern match (takes priority)
    {
        int lscore;
        int lidx = match_learned_pattern(prompt, &lscore);
        if (lidx >= 0) {
            if (emit_learned_pattern(prog, lidx)) {
                SNPRINTF_CHECK(desc, desc_size, "learned pattern: %s (score: %d)", learned_patterns[lidx].id, lscore);
                return 1;
            }
        }
    }

    // Multi-step support
    char steps[MAX_STEPS][MAX_PROMPT];
    int num_steps = split_prompt_steps(prompt, steps);

    if (num_steps > 1) {
        char inherited_names[64][256] = {{0}};
        char inherited_types[64][64] = {{0}};
        int inherited_counts[64] = {0};
        int num_inherited = 0;
        for (int i = 0; i < num_steps; i++) {
            c_printf(ANSI_CYAN, "  Step %d/%d: %s\n", i + 1, num_steps, steps[i]);
            int learned_emitted = 0;
            // Try per-step learned pattern match
            {
                int lscore;
                int lidx = match_learned_pattern(steps[i], &lscore);
                if (lidx >= 0) {
                    if (emit_learned_step(prog, lidx)) {
                        learned_emitted = 1;
                    }
                }
            }
            if (!learned_emitted) {
                TaskProfile task;
                memset(&task, 0, sizeof(task));
                if (!parse_task(steps[i], &task)) {
                    c_printf(ANSI_RED, "Error: Could not understand step %d: \"%s\"\n", i + 1, steps[i]);
                    c_printf(ANSI_YELLOW, "  Hint: Try rephrasing with clearer keywords (e.g., 'sort numbers', 'print result')\n");
                    dataflow_quiet_mode = 0;
                    return 0;
                }
                task.skip_input = (i > 0);
                task.suppress_output = (i < num_steps - 1);
                dataflow_quiet_mode = task.suppress_output;
                    // populate inherited vars from previous steps
                    for (int iv = 0; iv < num_inherited; iv++) {
                        SNPRINTF_CHECK(task.inherit_var_names[task.num_inherit_vars], sizeof(task.inherit_var_names[task.num_inherit_vars]), "%.*s", (int)sizeof(task.inherit_var_names[task.num_inherit_vars]) - 1, inherited_names[iv]);
                        SNPRINTF_CHECK(task.inherit_var_types[task.num_inherit_vars], sizeof(task.inherit_var_types[task.num_inherit_vars]), "%.*s", (int)sizeof(task.inherit_var_types[task.num_inherit_vars]) - 1, inherited_types[iv]);
                        task.inherit_var_counts[task.num_inherit_vars] = inherited_counts[iv];
                        task.num_inherit_vars++;
                    }
                    // backward compat: first array var
                    if (i > 0) {
                        for (int iv = 0; iv < num_inherited; iv++) {
                            if (inherited_counts[iv] > 1 && !task.inherit_var[0]) {
                                SNPRINTF_CHECK(task.inherit_var, sizeof(task.inherit_var), "%.*s", (int)sizeof(task.inherit_var) - 1, inherited_names[iv]);
                                task.inherit_count = inherited_counts[iv];
                            }
                        }
                    }
                    // Also try LLM emitter selection for this step to get extra emitters
                    // The LLM scores all 133 emitters and selects strongly-scored secondaries
                    if (num_steps > 0) {
                        current_prompt = steps[i];
                        int vs_indices2[3];
                        float vs_scores2[3];
                        int vs_count2 = search_examples(steps[i], 3, vs_indices2, vs_scores2);
                        if (vs_count2 > 0 && vs_scores2[0] > 0.3f) {
                            vs_boost_count = tokenize(example_docs[vs_indices2[0]].stem, vs_boost_tokens, 64);
                        }
                        llm_select_emitter(steps[i], &task);
                    }
                    // generate_from_task runs primary emitter(s) AND extra emitters in sequence
                    generate_from_task(prog, &task, (i == num_steps - 1));
            }
            // collect inherited variables from the main function for next steps
            // Now also collects scalar variables (not just arrays) for data flow
            Function *fcur = NULL;
            for (int fi = 0; fi < prog->num_funcs; fi++)
                if (strcmp(prog->funcs[fi].name, "main") == 0) { fcur = &prog->funcs[fi]; break; }
            if (fcur) {
                for (int vi = 0; vi < fcur->num_vars && num_inherited < 64; vi++) {
                    int is_const = (strstr(fcur->vars[vi].type, "const") != NULL);
                    int is_standard_constant = (strcmp(fcur->vars[vi].name, "zero") == 0 || strcmp(fcur->vars[vi].name, "one") == 0
                        || strcmp(fcur->vars[vi].name, "two") == 0 || strcmp(fcur->vars[vi].name, "three") == 0
                        || strcmp(fcur->vars[vi].name, "four") == 0 || strcmp(fcur->vars[vi].name, "five") == 0);
                    int is_input_prompt = (strcmp(fcur->vars[vi].name, "input_prompt") == 0);
                    // Skip const types, standard constants (zero-one-five), and input_prompt
                    // Keep arrays AND scalars — let the emitter handle dedup
                    if (!is_const && !is_standard_constant && !is_input_prompt) {
                        int already = 0;
                        for (int ck = 0; ck < num_inherited; ck++) {
                            if (strcmp(inherited_names[ck], fcur->vars[vi].name) == 0) already = 1;
                        }
                        if (!already) {
                            SNPRINTF_CHECK(inherited_names[num_inherited], sizeof(inherited_names[num_inherited]), "%s", fcur->vars[vi].name);
                            SNPRINTF_CHECK(inherited_types[num_inherited], sizeof(inherited_types[num_inherited]), "%s", fcur->vars[vi].type);
                            inherited_counts[num_inherited] = fcur->vars[vi].count;
                            num_inherited++;
                        }
                    }
                }
            }
        }
        // Exits are now handled by ensure_exit in generate_from_task, which only
        // adds :exit ! for the last step. No post-processing needed.
        dataflow_quiet_mode = 0;
        SNPRINTF_CHECK(desc, desc_size, "multi-step generation (%d steps)", num_steps);
        return 1;
    }

    // Single-step: try learned pattern match first
    {
        int lscore;
        int lidx = match_learned_pattern(prompt, &lscore);
        if (lidx >= 0) {
            if (emit_learned_pattern(prog, lidx)) {
                SNPRINTF_CHECK(desc, desc_size, "learned pattern: %s (score: %d)", learned_patterns[lidx].id, lscore);
                return 1;
            }
        }
    }

    // Single-step: parse task and use LLM to select emitter
    TaskProfile task;
    memset(&task, 0, sizeof(task));
    current_prompt = prompt;
    int parsed = parse_task(prompt, &task);
    if (!parsed) return 0;

    // Vector search over example codebase
    vs_boost_count = 0;
    int vs_indices[3];
    float vs_scores[3];
    int vs_count = search_examples(prompt, 3, vs_indices, vs_scores);
    if (vs_count > 0 && vs_scores[0] > 0.3f) {
        printf("  Vector search: \"%s\" (similarity: %.2f)\n",
               example_docs[vs_indices[0]].stem, vs_scores[0]);
        vs_boost_count = tokenize(example_docs[vs_indices[0]].stem, vs_boost_tokens, 64);
    }

    int emitter_idx = llm_select_emitter(prompt, &task);
    if (emitter_idx >= 0) {
        if (generate_from_task(prog, &task, 1)) {
            SNPRINTF_CHECK(desc, desc_size, "llm plan: %s", task.title);
            return 1;
        }
    }

    // Fallback: old arithmetic generator for simple math prompts
    char buf[MAX_PROMPT];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", prompt);
    to_lowercase(buf);
    const char *op = find_operation(buf);
    if (!op) return 0;

    const char *type = "int64";
    if (strstr(buf, "byte") || strstr(buf, "int8")) type = "byte";
    else if (strstr(buf, "int16") || strstr(buf, "short")) type = "int16";
    else if (strstr(buf, "int32")) type = "int32";
    else if (strstr(buf, "int64") || strstr(buf, "int") || strstr(buf, "long")) type = "int64";

    int nums[4];
    int num_count = 0;
    char *p = buf;
    while (*p && num_count < 4) {
        if (isalpha(*p)) {
            char word[32]; int wi = 0;
            while (*p && isalpha(*p) && wi < 31) word[wi++] = *p++;
            word[wi] = '\0';
            int n = word_to_num(word);
            if (n >= 0) nums[num_count++] = n;
            continue;
        }
        p++;
    }
    p = buf;
    while (*p && num_count < 4) {
        if (isdigit(*p)) {
            char *prev = p - 1;
            while (prev >= buf && (isalpha(*prev) || *prev == '_')) prev--;
            prev++;
            if (prev < p) {
                char prefix[16]; int pi = 0;
                while (prev < p && pi < 15) prefix[pi++] = *prev++;
                prefix[pi] = '\0';
                if (strcmp(prefix, "int") == 0 || strcmp(prefix, "const") == 0) {
                    while (*p && isdigit(*p)) p++;
                    continue;
                }
            }
            nums[num_count++] = (int)strtol(p, NULL, 10);
            while (*p && isdigit(*p)) p++;
            continue;
        }
        p++;
    }
    int n_vals = num_count < 2 ? 2 : (num_count > 3 ? 3 : num_count);
    int avals[3] = {10, 3, 5};
    if (num_count >= 1) avals[0] = nums[0];
    if (num_count >= 2) avals[1] = nums[1];
    if (num_count >= 3) avals[2] = nums[2];

    gen_arithmetic(prog, op, type, avals, n_vals);
    const char *op_name;
    const char *sym;
    if (strcmp(op, "sub") == 0) { op_name = "subtraction"; sym = "-"; }
    else if (strcmp(op, "mul") == 0) { op_name = "multiplication"; sym = "*"; }
    else if (strcmp(op, "div") == 0) { op_name = "division"; sym = "/"; }
    else if (strcmp(op, "mod") == 0) { op_name = "modulo"; sym = "%"; }
    else { op_name = "addition"; sym = "+"; }
    if (n_vals == 3)
        SNPRINTF_CHECK(desc, desc_size, "%s of three %s numbers (%d %s %d %s %d)", op_name, type, avals[0], sym, avals[1], sym, avals[2]);
    else
        SNPRINTF_CHECK(desc, desc_size, "%s of two %s numbers (%d %s %d)", op_name, type, avals[0], sym, avals[1]);
    return 1;
}

void write_program(Program *prog, const char *filename) {
    FILE *f = NULL;
    if (dry_run_flag) {
        f = stdout;
    } else {
        f = fopen(filename, "w");
        if (!f) {
            c_printf(ANSI_RED, "Error: Cannot write to file: %s\n", filename);
            c_printf(ANSI_YELLOW, "  Hint: Check if the directory exists and you have write permissions\n");
            return;
        }
    }

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char date[64];
    strftime(date, sizeof(date), "%Y-%m-%d", tm);

    if (!dry_run_flag) {
        fprintf(f, "// %s\n", filename);
        fprintf(f, "// Generated by Brackets Code on %s\n", date);
        fprintf(f, "//\n\n");
    } else {
        fprintf(f, "# %s (dry-run)\n", filename);
    }

    for (int i = 0; i < prog->num_includes; i++)
        fprintf(f, "#include <%s>\n", prog->includes[i]);
    fprintf(f, "\n");

    if (strlen(prog->globals) > 0)
        fprintf(f, "%s\n\n", prog->globals);

    for (int fi = 0; fi < prog->num_funcs; fi++) {
        Function *fn = &prog->funcs[fi];
        fprintf(f, "(%s func)\n", fn->name);
        if (fn->has_vardef)
            fprintf(f, "\t#var ~ %s\n", fn->vardef_name);
        // Pre-scan body (set ...) lines to update f->vars values when body overrides
        // the default value set by token declarations (e.g. token count=0, body (set int64 1 count 5))
        {
            const char *bp = fn->body;
            while (*bp) {
                const char *nl = strchr(bp, '\n');
                size_t llen = nl ? (size_t)(nl - bp) : strlen(bp);
                if (llen > 0 && llen < 4096) {
                    char line[4096];
                    memcpy(line, bp, llen);
                    line[llen] = '\0';
                    const char *p = line;
                    while (*p == ' ' || *p == '\t') p++;
                    if (strncmp(p, "(set ", 5) == 0) {
                        char type_buf[64], count_buf[64], name_buf[256];
                        char rest[4096];
                        int n = sscanf(p + 5, "%63s %63s %255s %4095[^\n)]", type_buf, count_buf, name_buf, rest);
                        if (n >= 3) {
                            for (int vi = 0; vi < fn->num_vars; vi++) {
                                if (strcmp(fn->vars[vi].name, name_buf) == 0 && n >= 4) {
                                    // parse values from rest
                                    char vals[16][64];
                                    int nv = 0;
                                    char *rp = rest;
                                    char *tok;
                                    while ((tok = strtok_r(rp, " ", &rp)) && nv < 16)
                                        SNPRINTF_CHECK(vals[nv++], sizeof(vals[0]), "%s", tok);
                                    if (nv > 0) {
                                        fn->vars[vi].num_values = nv;
                                        for (int vj = 0; vj < nv; vj++)
                                            SNPRINTF_CHECK(fn->vars[vi].values[vj], sizeof(fn->vars[vi].values[0]), "%.*s", (int)sizeof(fn->vars[vi].values[0]) - 1, vals[vj]);
                                    }
                                    break;
                                }
                            }
                        }
                    }
                }
                if (!nl) break;
                bp = nl + 1;
            }
        }
        // Track all variable names already emitted as (set ...) in the output
        char emitted_names[4096] = " ";
        for (int vi = 0; vi < fn->num_vars; vi++) {
            Variable *v = &fn->vars[vi];
            fprintf(f, "\t(set %s %d %s", v->type, v->count, v->name);
            for (int vj = 0; vj < v->num_values; vj++)
                fprintf(f, " %s", v->values[vj]);
            fprintf(f, ")\n");
            size_t en_len = strlen(emitted_names);
            if (en_len + strlen(v->name) + 2 < sizeof(emitted_names)) {
                strncat(emitted_names, v->name, sizeof(emitted_names) - en_len - 1);
                en_len = strlen(emitted_names);
                strncat(emitted_names, " ", sizeof(emitted_names) - en_len - 1);
            }
        }
        {
            // Filter duplicate (set ...) lines from body whose variable name
            // already appeared in fn->vars[] or a prior body (set ...) line
            const char *bp = fn->body;
            while (*bp) {
                const char *nl = strchr(bp, '\n');
                size_t llen = nl ? (size_t)(nl - bp) : strlen(bp);
                int skip = 0;
                char line[4096];
                if (llen > 0 && llen < sizeof(line)) {
                    memcpy(line, bp, llen);
                    line[llen] = '\0';
                    const char *p = line;
                    while (*p == ' ' || *p == '\t') p++;
                    if (strncmp(p, "(set ", 5) == 0) {
                        const char *sp = p + 5;
                        while (*sp && *sp != ' ') sp++;
                        if (*sp) { sp++; while (*sp && *sp != ' ') sp++; }
                        if (*sp) { sp++; while (*sp == ' ') sp++; }
                        char vn[256]; int vni = 0;
                        while (*sp && *sp != ' ' && *sp != ')' && vni < 255)
                            vn[vni++] = *sp++;
                        vn[vni] = '\0';
                        if (vni > 0) {
                            char check[384];
                            SNPRINTF_CHECK(check, sizeof(check), " %s ", vn);
                            if (strstr(emitted_names, check) != NULL) {
                                skip = 1;
                            } else if (strlen(emitted_names) + vni + 2 < sizeof(emitted_names)) {
                                size_t en2_len = strlen(emitted_names);
                                strncat(emitted_names, vn, sizeof(emitted_names) - en2_len - 1);
                                en2_len = strlen(emitted_names);
                                strncat(emitted_names, " ", sizeof(emitted_names) - en2_len - 1);
                            }
                        }
                    }
                }
                if (!skip)
                    fprintf(f, "%.*s\n", (int)llen, bp);
                if (!nl) break;
                bp = nl + 1;
            }
        }
        fprintf(f, "(funcend)\n\n");
    }

    for (int i = 0; i < prog->num_includes_post; i++)
        fprintf(f, "#include <%s>\n", prog->includes_post[i]);

    if (!dry_run_flag) {
        fclose(f);
        printf("Written: %s\n", filename);
    }
}

// ==================== TEMPLATES ====================

int dsl_apply_to_func(Program *prog, Function *f, const char *keyword) {
    for (int i = 0; i < dsl_num_rules; i++)
        if (strcmp(dsl_rules[i].keyword, keyword) == 0)
            return dsl_generate_code(prog, &dsl_rules[i], f);
    return 0;
}

void gen_from_dsl_keyword(Program *prog, const char *dsl_keyword) {
    DslRule *rule = NULL;
    for (int i = 0; i < dsl_num_rules; i++) {
        if (strcmp(dsl_rules[i].keyword, dsl_keyword) == 0) {
            rule = &dsl_rules[i];
            break;
        }
    }
    if (!rule) return;
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    dsl_generate_code(prog, rule, f);
    if (!strstr(f->body, ":exit !"))
        func_append(f, "\t(zero :exit !)");
}

Template templates[] = {
    {"hello world", "Hello World program", NULL, "hello world"},
    {"hello", "Hello World program", NULL, "hello world"},
    {"factorial", "Calculate factorial of a number", NULL, "factorial"},
    {"fibonacci", "Calculate fibonacci sequence", NULL, "fibonacci"},
    {"fizzbuzz", "FizzBuzz program", NULL, "fizzbuzz"},
    {"fizz buzz", "FizzBuzz program", NULL, "fizzbuzz"},
    {"for loop", "For loop example", NULL, "for loop"},
    {"loop", "For loop example", NULL, "for loop"},
    {"while loop", "While loop example", NULL, "while loop"},
    {"while", "While loop example", NULL, "while loop"},
    {"countdown", "Countdown from 10 to 1", NULL, "countdown"},
    {"count down", "Countdown from 10 to 1", NULL, "countdown"},
    {"if else", "If-else conditionals", NULL, "if else"},
    {"if", "If-else conditionals", NULL, "if else"},
    {"condition", "If-else conditionals", NULL, "if else"},
    {"positive negative", "Check positive/negative/zero", NULL, "positive negative"},
    {"switch", "Switch/case example", NULL, "switch"},
    {"case", "Switch/case example", NULL, "switch"},
    {"array min max", "Array min/max/average", NULL, "array min max"},
    {"array", "Array index access demo", NULL, "array demo"},
    {"print var", "Print a variable", NULL, "print var"},
    {"print variable", "Print a variable", NULL, "print var"},
    {"math", "Math operations demo", NULL, "math"},
    {"string concat", "String concatenation demo", NULL, "string concat"},
    {"string", "String operations demo", NULL, "string concat"},
    {"input", "User input example", NULL, "user input"},
    {"user input", "Get user input", NULL, "user input"},
    {"shell args", "Command line arguments", NULL, "shell args"},
    {"arguments", "Command line arguments", NULL, "shell args"},
    {"sum", "Sum of numbers from 1 to N", NULL, "sum"},
    {"primes", "Print prime numbers up to N", NULL, "primes"},
    {"prime", "Print prime numbers up to N", NULL, "primes"},
    {"function", "Function definition example", NULL, "function"},
    {"guess", "Number guessing game", NULL, "guess number"},
    {"guess number", "Number guessing game", NULL, "guess number"},
    {"sort", "Sort numbers using bubble sort", NULL, "bubble sort"},
    {"multiplication table", "Multiplication table", NULL, "multiplication table"},
    {"table", "Multiplication table", NULL, "multiplication table"},
    {"even odd", "Even/Odd number checker", NULL, "even odd"},
    {"even", "Even/Odd number checker", NULL, "even odd"},
    {"power", "Power/exponentiation function", NULL, "power"},
    {"exponent", "Power/exponentiation function", NULL, "power"},
    {"max of three", "Maximum of three numbers", NULL, "max of three"},
    {"max", "Maximum of three numbers", NULL, "max of three"},
    {"what is your name", "Ask name and greet", NULL, "hello name"},
    {"your name", "Ask name and greet", NULL, "hello name"},

    {"struct", "Person struct (dotted vars) demo", NULL, "struct demo"},
    {"person", "Person struct (dotted vars) demo", NULL, "struct demo"},
    {"pointer", "Pointer access demo", NULL, "pointer demo"},
    {"time", "Get current time and epoch ms", NULL, "time"},
    {"clock", "Get current time and epoch ms", NULL, "time"},
    {"boolean variable", "Boolean variable demo", NULL, "boolean variable"},
    {"concatenate strings", "String concatenation demo", NULL, "concatenate strings"},
    {"gcd", "GCD (greatest common divisor)", NULL, "gcd"},
    {"greatest common divisor", "GCD (greatest common divisor)", NULL, "gcd"},
    {"hex", "Hex and binary number demo", NULL, "hex binary"},
    {"binary", "Hex and binary number demo", NULL, "hex binary"},
};

int num_templates = sizeof(templates) / sizeof(templates[0]);

int match_template(const char *prompt, int *best_score) {
    char buf[MAX_PROMPT];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", prompt);
    to_lowercase(buf);

    int best_idx = -1;
    *best_score = 0;

    // Count total words in prompt (to avoid matching generic templates against complex prompts)
    int total_words = 0;
    {
        char wc_buf[MAX_PROMPT];
        char *saveptr;
        SNPRINTF_CHECK(wc_buf, sizeof(wc_buf), "%s", buf);
        char *wc = strtok_r(wc_buf, " ", &saveptr);
        while (wc) { trim(wc); if (strlen(wc) > 0) total_words++; wc = strtok_r(NULL, " ", &saveptr); }
    }

    for (int i = 0; i < num_templates; i++) {
        char kwbuf[512];
        SNPRINTF_CHECK(kwbuf, sizeof(kwbuf), "%s", templates[i].keywords);
        to_lowercase(kwbuf);

        int score = 0;
        int num_kw = 0;
        char kwcopy[512];
        SNPRINTF_CHECK(kwcopy, sizeof(kwcopy), "%s", kwbuf);
        char *saveptr2;
        char *kw = strtok_r(kwcopy, " ,/", &saveptr2);
        int all_match = 1;
        while (kw) {
            trim(kw);
            if (strlen(kw) > 0) {
                num_kw++;
                if (str_contains_word(buf, kw)) {
                    score++;
                } else {
                    all_match = 0;
                }
            }
            kw = strtok_r(NULL, " ,/", &saveptr2);
        }
        if (all_match && score > 0) {
            // Avoid matching generic short-keyword templates against complex prompts.
            // If the prompt has more than 3x the template's keyword count, it likely
            // contains additional intent (e.g. "read", "minimum") that the simple
            // template handler would ignore.
            if (total_words > num_kw * 8) continue;
            if (score > *best_score) {
                *best_score = score;
                best_idx = i;
            }
        }
    }
    return best_idx;
}

void prepend_out_dir(const char *fname, char *buf, int bufsize);

extern char **environ;

static int run_cmd(const char **argv) {
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO);

    pid_t pid;
    int ret = posix_spawnp(&pid, argv[0], &actions, NULL, (char *const *)argv, environ);
    posix_spawn_file_actions_destroy(&actions);

    if (ret != 0) {
        c_printf(ANSI_RED, "Error: Failed to execute '%s'\n", argv[0]);
        c_printf(ANSI_YELLOW, "  Hint: Check if the program is installed and in your PATH\n");
        return -1;
    }

    int status;
    if (waitpid(pid, &status, 0) == -1) return -1;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
}

int validate_code(const char *filename) {
    char ppname[512];
    const char *dot = strrchr(filename, '.');
    if (dot) {
        int len = dot - filename;
        SNPRINTF_CHECK(ppname, sizeof(ppname), "%.*s_pp.l1com", len, filename);
    } else {
        SNPRINTF_CHECK(ppname, sizeof(ppname), "%s_pp.l1com", filename);
    }
    const char *include_dir = getenv("L1VM_INCLUDE");
    if (!include_dir) {
        static char fallback[1024];
        if (l1vm_root[0]) {
            SNPRINTF_CHECK(fallback, sizeof(fallback), "%s/include/", l1vm_root);
        } else {
            const char *home = getenv("HOME");
            if (home) SNPRINTF_CHECK(fallback, sizeof(fallback), "%s/l1vm/include/", home);
            else SNPRINTF_CHECK(fallback, sizeof(fallback), "/usr/local/l1vm/include/");
        }
        include_dir = fallback;
    }

    const char *l1pre_bin = "l1pre";
    const char *l1com_bin = "l1com";
    char l1pre_path[1024];
    char l1com_path[1024];
    if (l1vm_root[0]) {
        SNPRINTF_CHECK(l1pre_path, sizeof(l1pre_path), "%s/bin/l1pre", l1vm_root);
        l1pre_bin = l1pre_path;
        SNPRINTF_CHECK(l1com_path, sizeof(l1com_path), "%s/bin/l1com", l1vm_root);
        l1com_bin = l1com_path;
    }

    const char *l1pre_argv[] = {l1pre_bin, filename, ppname, include_dir, NULL};
    int ret = run_cmd(l1pre_argv);
    if (ret != 0) {
        if (ret > 0) c_printf(ANSI_RED, "Validation: l1pre preprocessing failed (exit code %d)\n", ret);
        (void)remove(ppname);
        return 0;
    }

    char compname[512];
    SNPRINTF_CHECK(compname, sizeof(compname), "%.*s_pp", (int)(dot ? dot - filename : (int)strlen(filename)), filename);
    const char *l1com_argv[] = {l1com_bin, compname, NULL};
    ret = run_cmd(l1com_argv);
    if (ret == 0) {
        c_printf(ANSI_GREEN, "Validation: OK\n");
    } else if (ret > 0) {
        c_printf(ANSI_RED, "Validation: l1com compilation failed (exit code %d)\n", ret);
        c_printf(ANSI_YELLOW, "  Hint: Check for undefined variables, missing labels, or syntax errors in the generated code\n");
    }
    (void)remove(ppname);
    return ret == 0;
}

void prepend_out_dir(const char *fname, char *buf, int bufsize) {
    if (out_dir[0]) {
        SNPRINTF_CHECK(buf, bufsize, "%s/%s", out_dir, fname);
        buf[bufsize - 1] = '\0';
    } else {
        SNPRINTF_CHECK(buf, bufsize, "%s", fname);
    }
}

int generate_code(const char *prompt, const char *filename) {
    int is_q = is_question(prompt);
    if (is_q)
        answer_question(prompt);

    Program *prog = malloc(sizeof(Program));
    if (!prog) { c_printf(ANSI_RED, "Out of memory\n"); return 0; }
    if (!init_program(prog)) { free_program(prog); free(prog); c_printf(ANSI_RED, "Out of memory\n"); return 0; }
    SNPRINTF_CHECK(prog->filename, sizeof(prog->filename), "%s", filename);
    // (temp/func counters removed)

    // Try exact keyword template match first (most reliable)
    // Skip for multi-step prompts (e.g. "input numbers then sort them then print them")
    // to let smart_generate() handle them with proper step splitting.
    int score = 0;
    int idx = -1;
    if (!has_sequential_pattern(prompt)) {
        idx = match_template(prompt, &score);
    }

    if (idx >= 0) {
        if (is_q) c_printf(ANSI_CYAN, "Code example written to: %s\n", filename);
        else c_printf(ANSI_GREEN, "Matched template: %s (score: %d)\n", templates[idx].desc, score);
        if (templates[idx].dsl_keyword)
            gen_from_dsl_keyword(prog, templates[idx].dsl_keyword);
        else
            templates[idx].gen(prog, prompt);
        write_program(prog, filename);
        free_program(prog); free(prog);
        return 1;
    }

    if (is_q) {
        free_program(prog); free(prog);
        return 1;
    }

    // Smart generation via LLM emitter system
    char desc[512] = {0};
    if (smart_generate(prog, prompt, desc, sizeof(desc))) {
        if (is_q) c_printf(ANSI_CYAN, "Code example written to: %s\n", filename);
        else c_printf(ANSI_GREEN, "Smart generated (plan): %s\n", desc);
        write_program(prog, filename);
        free_program(prog); free(prog);
        return 1;
    }

    // Learned patterns as fallback emitter
    load_learned_patterns();
    {
        int lscore;
        int lidx = match_learned_pattern(prompt, &lscore);
        if (lidx >= 0) {
            if (emit_learned_pattern(prog, lidx)) {
                c_printf(ANSI_GREEN, "Using learned pattern: %s (score: %d)\n", learned_patterns[lidx].id, lscore);
                write_program(prog, filename);
                free_program(prog); free(prog);
                return 1;
            }
        }
    }

    // Nearest-neighbor fallback: use vector search to find closest example
    init_embeddings();
    index_examples();
    int nn_indices[1];
    float nn_scores[1];
    int nn_count = search_examples(prompt, 1, nn_indices, nn_scores);
    // Dynamic NN threshold: short prompts get lower threshold, long prompts higher
    float nn_factor = (float)strlen(prompt) / 60.0f;
    if (nn_factor > 2.0f) nn_factor = 2.0f;
    if (nn_factor < 0.6f) nn_factor = 0.6f;
    if (nn_count > 0 && nn_scores[0] > 0.25f * nn_factor) {
        c_printf(ANSI_YELLOW, "  Nearest neighbor: \"%s\" (similarity: %.2f)\n",
               example_docs[nn_indices[0]].stem, nn_scores[0]);
        gen_from_dsl_keyword(prog, "hello world");
        Function *f = NULL;
        for (int i = 0; i < prog->num_funcs; i++)
            if (strcmp(prog->funcs[i].name, "main") == 0) { f = &prog->funcs[i]; break; }
        if (f) {
            char nn_msg[1100];
            SNPRINTF_CHECK(nn_msg, sizeof(nn_msg), "\t// nearest example: %s", example_docs[nn_indices[0]].filename);
            func_append(f, nn_msg);
        }
        c_printf(ANSI_YELLOW, "Nearest-neighbor generated: %s\n", example_docs[nn_indices[0]].stem);
        write_program(prog, filename);
        free_program(prog); free(prog);
        return 1;
    }

    c_printf(ANSI_YELLOW, "No template matched for: '%s'. Generating default hello world.\n", prompt);
    gen_from_dsl_keyword(prog, "hello world");
    write_program(prog, filename);
    free_program(prog); free(prog);
    return 0;
}

int self_test(void) {
    const char *test_prompts[] = {
        "hello world", "sum from 1 to 100", "print even numbers up to 10",
        "count down from 5", "print numbers 1 to 5 in a loop", "calculate 10 plus 5",
        "factorial of 5", "fizzbuzz", "prime numbers up to 20",
        "check if 7 is even or odd", "2 to the power of 8", "multiplication table",
        "guess number game", "gcd of 12 and 8", "random number",
        "say hello to Alice", "reverse an array", "find a value in an array",
        "concatenate two strings", "assign values to an array",
        "find min max and average in array", "boolean variable demo",
        "check bits of number 134", "check if leap year",
        "convert celsius to fahrenheit", "area of a circle",
        "average of 5 numbers", "sort numbers",
    };
    int np = sizeof(test_prompts) / sizeof(test_prompts[0]);
    int passed = 0, failed = 0;
    printf("Self-test: running %d prompts through full pipeline + validation\n\n", np);
    for (int i = 0; i < np; i++) {
        char fname[256];
        prompt_to_filename(test_prompts[i], fname, sizeof(fname));
        printf("[%3d/%3d] %-40s ", i + 1, np, test_prompts[i]);
        fflush(stdout);
        validate_flag = 1;
        retry_seed = 0;
        int ok = 0;
        for (int r = 0; r < 3; r++) {
            retry_seed = r;
            if (generate_code(test_prompts[i], fname)) {
                if (validate_code(fname)) { ok = 1; break; }
            }
        }
        if (ok) { c_printf(ANSI_GREEN, "  PASS\n"); passed++; }
        else    { c_printf(ANSI_RED,   "  FAIL\n"); failed++; }
        char fullpath[1024];
        prepend_out_dir(fname, fullpath, sizeof(fullpath));
        (void)remove(fullpath);
    }
    printf("\n--- self-test: ");
    c_printf(ANSI_GREEN, "%d passed", passed);
    printf(", ");
    c_printf(ANSI_RED, "%d failed", failed);
    printf(", %d total ---\n", np);
    return failed == 0;
}

void show_help(void) {
    printf("\nBrackets Code Generator %s for Brackets (L1VM) Language\n", VERSION_TXT);
    printf("============================================================\n");
    printf("Usage:\n");
    printf("  Interactive mode:       brackets-code\n");
    printf("  One-shot mode:          brackets-code \"<prompt>\" [filename]\n");
    printf("  Pipe mode:              echo \"<prompt>\" | brackets-code - [filename]\n");
    printf("  With validation:        brackets-code --validate \"<prompt>\" [filename]\n");
    printf("  Self-test:              brackets-code --self-test\n");
    printf("  Batch mode:             brackets-code --batch prompts.txt\n");
    printf("  Vector search:          brackets-code --search \"<query>\"\n");
    printf("  Learn from file:        brackets-code --learn <file.l1com> [keywords] [\"description\"]\n");
    printf("  Forget pattern:         brackets-code --forget <pattern-id>\n");
    printf("  List learned patterns:  brackets-code --list-learned\n");
    printf("  List templates:         brackets-code --list\n\n");
    printf("Flags:\n");
    printf("  --dry-run               Dry run (print filename only, no output)\n");
    printf("  --verbose               Show emitter selection scores\n");
    printf("  --out-dir <dir>         Output directory for generated files\n");
    printf("  --l1vm-root <path>      L1VM installation root (for --validate)\n");
    printf("  --bash-completion       Print bash completion script\n\n");
    printf("Available templates:\n");
    for (int i = 0; i < num_templates; i++) {
        printf("  - %s: %s\n", templates[i].keywords, templates[i].desc);
    }
    printf("\nSpecial commands in interactive mode:\n");
    printf("  /help      - Show this help\n");
    printf("  /list      - List all templates\n");
    printf("  /learn <f> <kw> [desc] - Learn pattern from .l1com file\n");
    printf("  /forget <id> - Forget a learned pattern\n");
    printf("  /list-learned /ll - List learned patterns\n");
    printf("  /search <q>- Vector search example code\n");
    printf("  /exit      - Exit\n");
    printf("  /save <fn> - Save last generated code to file\n");
#ifdef HAVE_READLINE
    printf("  [Tab]      - Command completion\n");
#endif
    printf("\n");
}

#ifdef HAVE_READLINE
static char *cmd_completion_generator(const char *text, int state) {
    static int idx, len;
    const char *cmds[] = {"/exit", "/quit", "/help", "/?", "/list", "/search ", "/save ", "/self-test", NULL};
    if (!state) { idx = 0; len = strlen(text); }
    while (cmds[idx]) {
        if (strncmp(cmds[idx], text, len) == 0) return strdup(cmds[idx++]);
        idx++;
    }
    return NULL;
}
static char **cmd_completion(const char *text, int start, int end) {
    (void)end;
    if (start == 0) return rl_completion_matches(text, cmd_completion_generator);
    return NULL;
}
#endif

void interactive_mode(void) {
    char prompt[MAX_PROMPT];
    char last_fname[256] = {0};
    int has_last = 0;

    printf("Brackets Code Generator %s\n", VERSION_TXT);
    printf("Type /help for help, /exit to quit.\n");

#ifdef HAVE_READLINE
    rl_attempted_completion_function = cmd_completion;
    using_history();
    const char *histfile = getenv("HOME");
    char histpath[512] = "";
    if (histfile) { SNPRINTF_CHECK(histpath, sizeof(histpath), "%s/.brackets-code_history", histfile); read_history(histpath); }
    char *rl_line;
    while ((rl_line = readline("> ")) != NULL) {
        SNPRINTF_CHECK(prompt, sizeof(prompt), "%s", rl_line);
        free(rl_line);
        if (*prompt) add_history(prompt);
        trim(prompt);
#else
    while (1) {
        printf("> ");
        if (!fgets(prompt, sizeof(prompt), stdin)) break;
        trim(prompt);
#endif
        if (strlen(prompt) == 0) continue;

        if (strcmp(prompt, "/exit") == 0 || strcmp(prompt, "/quit") == 0) break;

        if (strcmp(prompt, "/help") == 0 || strcmp(prompt, "/?") == 0) {
            show_help();
            continue;
        }

        if (strcmp(prompt, "/list") == 0) {
            printf("Available templates:\n");
            for (int i = 0; i < num_templates; i++)
                printf("  %-25s %s\n", templates[i].keywords, templates[i].desc);
            continue;
        }

        if (strncmp(prompt, "/save", 5) == 0) {
            char fname[MAX_PROMPT] = {0};
            if (strlen(prompt) > 6) {
                SNPRINTF_CHECK(fname, sizeof(fname), "%s", prompt + 6);
                trim(fname);
            }
            if (strlen(fname) == 0) {
                printf("Please specify a filename.\n");
                printf("Last generated was: %s\n", has_last ? last_fname : "(none)");
            } else {
                printf("Use one-shot mode to generate specific code.\n");
            }
            continue;
        }

        if (strncmp(prompt, "/learn", 6) == 0) {
            char lpath[1024] = {0}, lkeywords[512] = {0}, ldesc[512] = {0};
            int n = sscanf(prompt + 6, " %1023s %511[^\"] \"%255[^\"]\"", lpath, lkeywords, ldesc);
            if (n == 0) {
                printf("Usage: /learn <file.l1com> [keywords] [\"description\"]\n");
            } else {
                // Shift args if no description
                if (n == 2) {
                    SNPRINTF_CHECK(ldesc, sizeof(ldesc), "%s", lkeywords);
                    lkeywords[0] = '\0';
                }
                ensure_learned_dir();
                learn_from_file(lpath, lkeywords, ldesc);
            }
            continue;
        }

        if (strncmp(prompt, "/forget", 7) == 0) {
            const char *id = prompt + 7;
            while (*id == ' ') id++;
            if (strlen(id) == 0) {
                printf("Usage: /forget <pattern-id>\n");
            } else {
                ensure_learned_dir();
                load_learned_patterns();
                forget_learned(id);
            }
            continue;
        }

        if (strcmp(prompt, "/list-learned") == 0 || strcmp(prompt, "/ll") == 0) {
            ensure_learned_dir();
            load_learned_patterns();
            list_learned();
            continue;
        }

        if (strncmp(prompt, "/search", 7) == 0) {
            const char *query = prompt + 7;
            while (*query == ' ') query++;
            if (strlen(query) == 0) {
                printf("Usage: /search <query>\n");
            } else {
                init_embeddings();
                index_examples();
                if (num_examples == 0) {
                    printf("No examples indexed.\n");
                } else {
                    int indices[MAX_TOP_K];
                    float scores[MAX_TOP_K];
                    int n = search_examples(query, MAX_TOP_K, indices, scores);
                    printf("Vector search results for: \"%s\"\n", query);
                    printf("  Found %d matches in %d examples\n", n, num_examples);
                    for (int i = 0; i < n; i++)
                        printf("  %2d. %-45s (%.2f)\n", i+1, example_docs[indices[i]].stem, scores[i]);
                }
            }
            continue;
        }

        char fname[256];
        prompt_to_filename(prompt, fname, sizeof(fname));
        generate_code(prompt, fname);
        SNPRINTF_CHECK(last_fname, sizeof(last_fname), "%s", fname);
        has_last = 1;
    }
    printf("\nBye!\n");
#ifdef HAVE_READLINE
    {
        const char *hf = getenv("HOME");
        if (hf) { char hp[512]; SNPRINTF_CHECK(hp, sizeof(hp), "%s/.brackets-code_history", hf); write_history(hp); }
    }
#endif
}

int main(int argc, char *argv[]) {
    use_color = isatty(STDOUT_FILENO);
    load_synonyms();
    dsl_load_rules("dsl");
    if (argc == 1) {
        interactive_mode();
        return 0;
    }

    if (argc >= 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        show_help();
        return 0;
    }

    if (argc >= 2 && (strcmp(argv[1], "--list") == 0 || strcmp(argv[1], "-l") == 0)) {
        printf("Available templates:\n");
        for (int i = 0; i < num_templates; i++)
            printf("  %-25s %s\n", templates[i].keywords, templates[i].desc);
        return 0;
    }

    if (argc >= 2 && strcmp(argv[1], "--learn") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s --learn <file.l1com> [keywords] [description]\n", argv[0]);
            return 1;
        }
        const char *lpath = argv[2];
        const char *lkeywords = (argc > 3) ? argv[3] : "";
        const char *ldesc = (argc > 4) ? argv[4] : "";
        ensure_learned_dir();
        return learn_from_file(lpath, lkeywords, ldesc) ? 0 : 1;
    }

    if (argc >= 2 && strcmp(argv[1], "--forget") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s --forget <pattern-id>\n", argv[0]);
            return 1;
        }
        ensure_learned_dir();
        load_learned_patterns();
        return forget_learned(argv[2]) ? 0 : 1;
    }

    if (argc >= 2 && (strcmp(argv[1], "--list-learned") == 0 || strcmp(argv[1], "-L") == 0)) {
        ensure_learned_dir();
        load_learned_patterns();
        list_learned();
        return 0;
    }

    if (argc >= 2 && (strcmp(argv[1], "--search") == 0 || strcmp(argv[1], "-s") == 0)) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s --search <query>\n", argv[0]);
            return 1;
        }
        init_embeddings();
        index_examples();
        if (num_examples == 0) {
            printf("No examples indexed (directory '%s' not found).\n", EXAMPLE_DIR);
            return 1;
        }
        int indices[MAX_TOP_K];
        float scores[MAX_TOP_K];
        int n = search_examples(argv[2], MAX_TOP_K, indices, scores);
        printf("Vector search results for: \"%s\"\n", argv[2]);
        printf("  Found %d matches in %d examples\n", n, num_examples);
        for (int i = 0; i < n; i++)
            printf("  %2d. %-40s (%.2f)\n", i+1, example_docs[indices[i]].stem, scores[i]);
        return 0;
    }

    int arg_idx = 1;
    if (argc >= 2 && (strcmp(argv[1], "--self-test") == 0 || strcmp(argv[1], "-t") == 0)) {
        validate_flag = 1;
        return self_test() ? 0 : 1;
    }
    if (argc >= 2 && (strcmp(argv[1], "--bash-completion") == 0 || strcmp(argv[1], "--completion") == 0)) {
        printf("_brackets_code_completions() {\n");
        printf("  local cur=\"${COMP_WORDS[COMP_CWORD]}\"\n");
        printf("            opts=\"--help -h --list -l --search --validate -v --self-test -t --verbose --dry-run --out-dir --l1vm-root --batch --bash-completion --learn --forget --list-learned\"\n");
        printf("  COMPREPLY=($(compgen -W \"${opts}\" -- \"$cur\"))\n");
        printf("}\n");
        printf("complete -F _brackets_code_completions brackets-code\n");
        return 0;
    }

    // Scan for global flags before positional args
    while (arg_idx < argc) {
        if (strcmp(argv[arg_idx], "--verbose") == 0) { verbose_flag = 1; arg_idx++; }
        else if (strcmp(argv[arg_idx], "--dry-run") == 0) { dry_run_flag = 1; arg_idx++; }
        else if (strcmp(argv[arg_idx], "--out-dir") == 0) {
            if (arg_idx + 1 < argc) { SNPRINTF_CHECK(out_dir, sizeof(out_dir), "%s", argv[arg_idx + 1]); arg_idx += 2; }
            else { fprintf(stderr, "Usage: ... --out-dir <directory>\n"); return 1; }
        }
        else if (strcmp(argv[arg_idx], "--l1vm-root") == 0) {
            if (arg_idx + 1 < argc) { SNPRINTF_CHECK(l1vm_root, sizeof(l1vm_root), "%s", argv[arg_idx + 1]); arg_idx += 2; }
            else { fprintf(stderr, "Usage: ... --l1vm-root <path>\n"); return 1; }
        }
        else if (strcmp(argv[arg_idx], "--validate") == 0 || strcmp(argv[arg_idx], "-v") == 0) { validate_flag = 1; arg_idx++; }
        else break;
    }

    // Batch mode: consumes all remaining flags, expects batch-file right after --batch
    if (argc >= arg_idx + 2 && (strcmp(argv[arg_idx], "--batch") == 0)) {
        const char *batch_file = argv[arg_idx + 1];
        FILE *bf = fopen(batch_file, "r");
        if (!bf) {
            c_printf(ANSI_RED, "Error: Cannot open batch file: %s\n", batch_file);
            c_printf(ANSI_YELLOW, "  Hint: Check if the file exists and the path is correct\n");
            return 1;
        }
        char bline[MAX_PROMPT];
        int total = 0, good = 0;
        while (fgets(bline, sizeof(bline), bf)) {
            trim(bline);
            if (strlen(bline) == 0) continue;
            total++;
            char bfname[512];
            prompt_to_filename(bline, bfname, sizeof(bfname));
            char bfull[1024];
            prepend_out_dir(bfname, bfull, sizeof(bfull));
            int bgen = generate_code(bline, bfull);
            int bok = 0;
            if (bgen && validate_flag) {
                if (validate_code(bfull)) { bok = 1; }
            } else if (bgen) { bok = 1; }
            if (bok) { c_printf(ANSI_GREEN, "[batch] %s -> %s OK\n", bline, bfull); good++; }
            else     { c_printf(ANSI_RED,   "[batch] %s -> %s FAIL\n", bline, bfull); }
        }
        fclose(bf);
        printf("\n--- batch: %d/%d succeeded ---\n", good, total);
        return (good == total) ? 0 : 1;
    }
    if (arg_idx >= argc) {
        c_printf(ANSI_RED, "Error: No prompt provided\n");
        c_printf(ANSI_YELLOW, "Usage: %s [--validate] [--l1vm-root <path>] <prompt> [filename]\n", argv[0]);
        c_printf(ANSI_YELLOW, "  Example: %s \"hello world\" output.l1com\n", argv[0]);
        return 1;
    }

    const char *prompt = argv[arg_idx++];
    // Pipe mode: prompt "-" reads from stdin
    char pipe_buf[MAX_PROMPT];
    if (strcmp(prompt, "-") == 0) {
        size_t nread = 0;
        int ch;
        while ((ch = getchar()) != EOF && nread < MAX_PROMPT - 1)
            pipe_buf[nread++] = (char)ch;
        pipe_buf[nread] = '\0';
        prompt = pipe_buf;
    }
    char fname[256];

    if (arg_idx < argc) {
        SNPRINTF_CHECK(fname, sizeof(fname), "%s", argv[arg_idx]);
        // block path traversal in filename
        if (strstr(fname, "..")) {
            c_printf(ANSI_RED, "Error: Filename must not contain '..' (path traversal blocked)\n");
            return 1;
        }
        if (!strstr(fname, ".l1com")) strncat(fname, ".l1com", sizeof(fname) - strlen(fname) - 1);
    } else {
        prompt_to_filename(prompt, fname, sizeof(fname));
    }
    char fullpath[1024];
    prepend_out_dir(fname, fullpath, sizeof(fullpath));

    int validated = 0;
    int exit_code = 1;
    for (int retry = 0; retry < 3; retry++) {
        retry_seed = retry;
        int gen_ok = generate_code(prompt, fullpath);
        if (gen_ok && validate_flag) {
            if (validate_code(fullpath)) {
                validated = 1;
                exit_code = 0;
                break;
            } else {
                c_printf(ANSI_YELLOW, "Validation failed, retrying with different code generation (attempt %d/3)...\n", retry + 1);
            }
        } else if (gen_ok) {
            validated = 1;
            exit_code = 0;
            break;
        }
    }
    if (!validated && validate_flag) {
        c_printf(ANSI_RED, "Warning: Code could not be validated after 3 attempts.\n");
        c_printf(ANSI_YELLOW, "  Hint: The generated code may have syntax errors. Try rephrasing your prompt.\n");
        exit_code = 2;
    } else if (!validated) {
        exit_code = 1;
    }
    return exit_code;
}
