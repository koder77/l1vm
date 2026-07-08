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

#define ANSI_RESET   "\033[0m"
#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_BOLD    "\033[1m"
int use_color = 1;
#define c_printf(color, ...) do { if (use_color) { printf(color); printf(__VA_ARGS__); printf(ANSI_RESET); } else printf(__VA_ARGS__); } while(0)

#include "brackets-code.h"

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
    snprintf(buf, sizeof(buf), "%s", str);
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
    snprintf(prog->includes[prog->num_includes], sizeof(prog->includes[0]), "%s", inc);
    prog->num_includes++;
}

void add_include_post(Program *prog, const char *inc) {
    for (int i = 0; i < prog->num_includes_post; i++)
        if (strcmp(prog->includes_post[i], inc) == 0) return;
    if (!ensure_includes_cap(&prog->includes_post, &prog->includes_post_cap, prog->num_includes_post + 1)) return;
    snprintf(prog->includes_post[prog->num_includes_post], sizeof(prog->includes_post[0]), "%s", inc);
    prog->num_includes_post++;
}

void add_func(Program *prog, const char *name) {
    for (int i = 0; i < prog->num_funcs; i++)
        if (strcmp(prog->funcs[i].name, name) == 0) return;
    if (!ensure_funcs_cap(&prog->funcs, &prog->funcs_cap, prog->num_funcs + 1)) return;
    Function *f = &prog->funcs[prog->num_funcs];
    init_function(f);
    snprintf(f->name, sizeof(f->name), "%s", name);
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
    snprintf(v->name, sizeof(v->name), "%s", name);
    snprintf(v->type, sizeof(v->type), "%s", type);
    v->count = count;
    v->num_values = num_values;
    for (int i = 0; i < num_values && i < MAX_VALUES; i++)
        snprintf(v->values[i], sizeof(v->values[i]), "%s", values[i]);
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
    snprintf(f->vardef_name, sizeof(f->vardef_name), "%s", scope);
}

int prompt_to_filename(const char *prompt, char *out, int max) {
    char buf[MAX_PROMPT];
    snprintf(buf, sizeof(buf), "%s", prompt);
    to_lowercase(buf);
    for (int i = 0; buf[i]; i++)
        if (!isalnum(buf[i])) buf[i] = '_';
    while (buf[0] == '_') memmove(buf, buf+1, strlen(buf));
    char *p = buf + strlen(buf) - 1;
    while (p > buf && *p == '_') *p-- = '\0';
    snprintf(out, max, "%s.l1com", buf);
    return 1;
}

int is_question(const char *prompt) {
    const char *qwords[] = {"was", "wie", "wer", "wo", "warum", "wann", "welche", "what", "how", "why", "where", "when", "can", "does", "is", "are", NULL};
    char buf[MAX_PROMPT];
    snprintf(buf, sizeof(buf), "%s", prompt);
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
    snprintf(buf, sizeof(buf), "%s", prompt);
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
        snprintf(path, sizeof(path), "%s/%s/%s", home, SYNONYM_DIR, SYNONYM_FILE);
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
            snprintf(slash + 1, sizeof(self_path) - (size_t)(slash - self_path + 1), "%s", SYNONYM_FILE);
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
            snprintf(dyn_synonyms[num_dyn_synonyms].word, sizeof(dyn_synonyms[0].word), "%s", word);
            snprintf(dyn_synonyms[num_dyn_synonyms].canonical, sizeof(dyn_synonyms[0].canonical), "%s", canonical);
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
    snprintf(expanded, max_len, "%s", query);
    char buf[MAX_PROMPT];
    snprintf(buf, sizeof(buf), "%s", query);
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
    snprintf(buf, sizeof(buf), "%s", text);
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
    snprintf(buf, sizeof(buf), "%s", prompt);
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
                    snprintf(wbuf, sizeof(wbuf), "%.*s", wlen, word_start);
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
    snprintf(buf, sizeof(buf), "%s", prompt);
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
            nums[count++] = (int)strtol(p, NULL, 10);
            while (*p && isdigit(*p)) p++;
            continue;
        }
        p++;
    }
    return count;
}

// ==================== 21 EMITTER BLOCKS ====================

// 1-3: Basic emitters
void emit_math(Program *prog, Function *f, const char *type, const char *op, const int *vals, int n, int last_step);
void emit_input_loop(Program *prog, Function *f, int count, const char *type, const char *op);

void emit_input_sort(Program *prog, Function *f, int count, int skip_input, int descending, const char *type);
void emit_median(Program *prog, Function *f, int count, int skip_input);

void emit_array_reverse(Program *prog, Function *f, int skip_input);
void emit_array_find(Program *prog, Function *f, int skip_input);
void emit_array_vmath(Program *prog, Function *f, int skip_input);

void emit_average(Program *prog, Function *f, int skip_input);
void emit_selection_sort(Program *prog, Function *f, int count, int skip_input);
void emit_fann_create(Program *prog, Function *f);
void emit_fann_train(Program *prog, Function *f);
void emit_fann_run(Program *prog, Function *f);

void emit_bubble_sort(Program *prog, Function *f, int skip_input);

void emit_standard_deviation(Program *prog, Function *f, int skip_input);

void emit_double_math(Program *prog, Function *f, const char *op);
void emit_double_average(Program *prog, Function *f, int skip_input);

void emit_insertion_sort(Program *prog, Function *f, int count, int skip_input);

// Forward declarations for plan-based generator
int parse_task(const char *prompt, TaskProfile *task);
int generate_from_task(Program *prog, TaskProfile *task, int last_step);

// ==================== 21 EMITTER BLOCK IMPLEMENTATIONS ====================

void emit_math(Program *prog, Function *f, const char *type, const char *op, const int *vals, int n, int last_step) {
    (void)last_step;
    int is_double = (strcmp(type, "double") == 0);
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *twov[] = {"2"};
    const char *threev[] = {"3"};
    const char *fourv[] = {"4"};
    const char *fivev[] = {"5"};
    const char *sixv[] = {"6"};
    const char *sevenv[] = {"7"};
    const char *tenv[] = {"10"};
    add_var_to_func(f, "const-int64", "two", 1, twov, 1);
    add_var_to_func(f, "const-int64", "three", 1, threev, 1);
    add_var_to_func(f, "const-int64", "four", 1, fourv, 1);
    add_var_to_func(f, "const-int64", "five", 1, fivev, 1);
    add_var_to_func(f, "const-int64", "six", 1, sixv, 1);
    add_var_to_func(f, "const-int64", "seven", 1, sevenv, 1);
    add_var_to_func(f, "const-int64", "ten", 1, tenv, 1);
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    char vn[64], vs[32], ln[256];
    for (int i = 0; i < n; i++) {
        snprintf(vn, sizeof(vn), "%s_%c", type, 'a' + i);
        if (is_double)
            snprintf(vs, sizeof(vs), "%d.0", vals[i]);
        else
            snprintf(vs, sizeof(vs), "%d", vals[i]);
        const char *vv[] = {vs};
        add_var_to_func(f, type, vn, 1, vv, 1);
    }
    char rn[64];
    snprintf(rn, sizeof(rn), "%s_r", type);
    const char *rv[] = {is_double ? "0.0" : "0"};
    add_var_to_func(f, type, rn, 1, rv, 1);
    const char *sym = "+";
    const char *dsym = " +d";
    if (strcmp(op, "sub") == 0) { sym = " -"; dsym = " -d"; }
    else if (strcmp(op, "mul") == 0) { sym = " *"; dsym = " *d"; }
    else if (strcmp(op, "div") == 0) { sym = " /"; dsym = " /d"; }
    else if (strcmp(op, "mod") == 0) { sym = " %"; dsym = " %"; }
    if (is_double) {
        if (n == 2)
            snprintf(ln, sizeof(ln), "\t(%s_%c %s_%c%s %s =)", type, 'a', type, 'b', dsym, rn);
        else
            snprintf(ln, sizeof(ln), "\t((%s_%c %s_%c%s) %s_%c%s %s =)", type, 'a', type, 'b', dsym, type, 'c', dsym, rn);
    } else {
        if (n == 2)
            snprintf(ln, sizeof(ln), "\t(%s_%c%s %s_%c %s :=)", type, 'a', sym, type, 'b', rn);
        else
            snprintf(ln, sizeof(ln), "\t((%s_%c%s %s_%c)%s %s_%c %s :=)", type, 'a', sym, type, 'b', sym, type, 'c', rn);
    }
    if (!is_double && (strcmp(op, "div") == 0 || strcmp(op, "mod") == 0)) {
        const char *errdiv[] = {"\"Error: division by zero!\""};
        add_var_to_func(f, "const-string", "div_err", 25, errdiv, 1);
        const char *err_code[] = {"1"};
        add_var_to_func(f, "const-int64", "exit_code", 1, err_code, 1);
        add_var_to_func(f, "int64", "f", 1, zv, 1);
        char div_check[256];
        snprintf(div_check, sizeof(div_check), "\t(((%s_%c zero ==) f :=) f if)", type, 'a' + (n - 1));
        func_append(f, "\t// check division by zero");
        func_append(f, div_check);
        func_append(f, "\t\t(div_err :print_s !)");
        func_append(f, "\t\t(:print_n !)");
        func_append(f, "\t\t(exit_code :exit !)");
        func_append(f, "\t(endif)");
    }
    func_append(f, ln);
    func_append(f, "\t// Ergebnis ausgeben:");
    if (is_double)
        snprintf(ln, sizeof(ln), "\t(%s :print_d !)", rn);
    else
        snprintf(ln, sizeof(ln), "\t(%s :print_i !)", rn);
    func_append(f, ln);
    func_append(f, "\t(:print_n !)");
}

void emit_input_loop(Program *prog, Function *f, int count, const char *type, const char *op) {
    add_include(prog, "intr-func.l1h");
    int is_double = (strcmp(type, "double") == 0);
    int mul_div = op && (strcmp(op, "mul") == 0 || strcmp(op, "div") == 0);
    const char *ziv[] = {"0"};
    const char *zdv[] = {"0.0"};
    const char *onev[] = {"1"};
    const char *onedv[] = {"1.0"};
    add_var_to_func(f, "const-int64", "zero", 1, ziv, 1);
    add_var_to_func(f, "const-int64", "one", 1, onev, 1);
    char vs[16], ln[256];
    snprintf(vs, sizeof(vs), "%d", count);
    const char *cv[] = {vs};
    add_var_to_func(f, "int64", "count", 1, cv, 1);
    add_var_to_func(f, type, "val", 1, is_double ? zdv : ziv, 1);
    add_var_to_func(f, "int64", "i", 1, ziv, 1);
    const char **sv;
    if (mul_div) sv = is_double ? onedv : onev;
    else sv = is_double ? zdv : ziv;
    add_var_to_func(f, is_double ? "double" : "int64", "sum", 1, sv, 1);
    add_var_to_func(f, "int64", "f", 1, ziv, 1);
    const char *ps[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "input_prompt", 18, ps, 1);
    const char *rps[] = {"\"Result: \""};
    add_var_to_func(f, "const-string", "result_prompt", 9, rps, 1);
    func_append(f, "\t(for-loop)");
    snprintf(ln, sizeof(ln), "\t(((i count <) f :=) f for)");
    func_append(f, ln);
    func_append(f, "\t\t(input_prompt :print_s !)");
    if (is_double)
        func_append(f, "\t\t(val :input_d !)");
    else
        func_append(f, "\t\t(val :input_i !)");
    if (op) {
        const char *infix_op = NULL;
        if (strcmp(op, "add") == 0) infix_op = "+";
        else if (strcmp(op, "sub") == 0) infix_op = "-";
        else if (strcmp(op, "mul") == 0) infix_op = "*";
        else if (strcmp(op, "div") == 0) infix_op = "/";
        if (infix_op) {
            snprintf(ln, sizeof(ln), is_double
                ? "\t\t(sum %s val sum =)"
                : "\t\t(sum %s val sum :=)", infix_op);
            func_append(f, ln);
        }
    }
    func_append(f, is_double ? "\t\t(val :print_d !)" : "\t\t(val :print_i !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(result_prompt :print_s !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, is_double ? "\t(sum :print_d !)" : "\t(sum :print_i !)");
    func_append(f, "\t(:print_n !)");
}

void emit_input_sort(Program *prog, Function *f, int count, int skip_input, int descending, const char *type) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    int is_double = (strcmp(type, "double") == 0);
    const char *zv[] = {"0"};
    const char *zd[] = {"0.0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    if (is_double)
        add_var_to_func(f, "const-double", "zerod", 1, zd, 1);
    else
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    const char *dtype = is_double ? "double" : "int64";
    char vs[16], ln[256];
    snprintf(vs, sizeof(vs), "%d", count);
    const char *cv[] = {vs};
    add_var_to_func(f, "int64", "count", 1, cv, 1);
    add_var_to_func(f, dtype, "arr", (count > 0 ? count : 1), cv, 0);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "j", 1, zv, 1);
    add_var_to_func(f, "int64", "temp", 1, zv, 1);
    add_var_to_func(f, "int64", "realind", 1, zv, 1);
    add_var_to_func(f, "int64", "realind2", 1, zv, 1);
    add_var_to_func(f, dtype, "a", 1, is_double ? zd : zv, 1);
    add_var_to_func(f, dtype, "b", 1, is_double ? zd : zv, 1);
    add_var_to_func(f, dtype, "mid", 1, is_double ? zd : zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "input_prompt", 18, ps, 1);
    const char *size_var = is_double ? "double_size" : "int64_size";
    const char *input_fmt = is_double ? "\t\t(a :input_d !)" : "\t\t(a :input_i !)";
    const char *copy_a = is_double ? "\t\t\t(a + zerod a :=)" : "\t\t\t(a + zero a :=)";
    const char *copy_b = is_double ? "\t\t\t(b + zerod b :=)" : "\t\t\t(b + zero b :=)";
    if (!skip_input) {
        func_append(f, "\t(zero i :=)");
        func_append(f, "\t(for-loop)");
        snprintf(ln, sizeof(ln), "\t(((i count <) f :=) f for)");
        func_append(f, ln);
        func_append(f, "\t\t(input_prompt :print_s !)");
        snprintf(ln, sizeof(ln), "\t\t(i * %s realind :=)", size_var);
        func_append(f, ln);
        func_append(f, input_fmt);
        if (is_double) func_append(f, "\t\t(a + zerod a :=)");
        func_append(f, "\t\t(a arr [ realind ] =)");
        func_append(f, "\t\t(i + one i :=)");
        func_append(f, "\t(next)");
    }
    func_append(f, "\t// bubble sort");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    snprintf(ln, sizeof(ln), "\t(((i count <) f :=) f for)");
    func_append(f, ln);
    func_append(f, "\t\t(zero j :=)");
    func_append(f, "\t\t(for-loop)");
    func_append(f, "\t\t\t(count one - temp :=)");
    snprintf(ln, sizeof(ln), "\t\t(((j temp <) f :=) f for)");
    func_append(f, ln);
    snprintf(ln, sizeof(ln), "\t\t\t(j * %s realind :=)", size_var);
    func_append(f, ln);
    snprintf(ln, sizeof(ln), "\t\t\t((j + one) * %s realind2 :=)", size_var);
    func_append(f, ln);
    func_append(f, "\t\t\t(arr [ realind ] a =)");
    func_append(f, copy_a);
    func_append(f, "\t\t\t(arr [ realind2 ] b =)");
    func_append(f, copy_b);
    func_append(f, descending ? "\t\t\t(((a b <) f :=) f if)" : "\t\t\t(((a b >) f :=) f if)");
    func_append(f, "\t\t\t\t(b arr [ realind ] =)");
    func_append(f, "\t\t\t\t(a arr [ realind2 ] =)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t\t(j + one j :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t// print sorted");
        func_append(f, "\t(zero i :=)");
        func_append(f, "\t(for-loop)");
        snprintf(ln, sizeof(ln), "\t(((i count <) f :=) f for)");
        func_append(f, ln);
        snprintf(ln, sizeof(ln), "\t\t(i * %s realind :=)", size_var);
        func_append(f, ln);
        func_append(f, "\t\t(arr [ realind ] a =)");
        func_append(f, is_double ? "\t\t(a + zerod a :=)" : "\t\t(a + zero a :=)");
        func_append(f, is_double ? "\t\t(a :print_d !)" : "\t\t(a :print_i !)");
        func_append(f, "\t\t(:print_n !)");
        func_append(f, "\t\t(i + one i :=)");
        func_append(f, "\t(next)");
    }
}

void emit_median(Program *prog, Function *f, int count, int skip_input) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    char vs[16], ln[256];
    snprintf(vs, sizeof(vs), "%d", count);
    const char *cv[] = {vs};
    add_var_to_func(f, "int64", "count", 1, cv, 1);
    add_var_to_func(f, "int64", "arr", (count > 0 ? count : 1), cv, 0);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "j", 1, zv, 1);
    add_var_to_func(f, "int64", "temp", 1, zv, 1);
    add_var_to_func(f, "int64", "realind", 1, zv, 1);
    add_var_to_func(f, "int64", "realind2", 1, zv, 1);
    add_var_to_func(f, "int64", "a", 1, zv, 1);
    add_var_to_func(f, "int64", "b", 1, zv, 1);
    add_var_to_func(f, "int64", "mid", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "input_prompt", 18, ps, 1);
    if (!skip_input) {
        func_append(f, "\t(zero i :=)");
        func_append(f, "\t(for-loop)");
        snprintf(ln, sizeof(ln), "\t(((i count <) f :=) f for)");
        func_append(f, ln);
        func_append(f, "\t\t(input_prompt :print_s !)");
        func_append(f, "\t\t(i * int64_size realind :=)");
        func_append(f, "\t\t(a :input_i !)");
        func_append(f, "\t\t(a temp :=)");
        func_append(f, "\t\t(temp arr [ realind ] =)");
        func_append(f, "\t\t(i + one i :=)");
        func_append(f, "\t(next)");
    }
    func_append(f, "\t// bubble sort for median");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    snprintf(ln, sizeof(ln), "\t(((i count <) f :=) f for)");
    func_append(f, ln);
    func_append(f, "\t\t(zero j :=)");
    func_append(f, "\t\t(for-loop)");
    func_append(f, "\t\t\t(count one - temp :=)");
    snprintf(ln, sizeof(ln), "\t\t(((j temp <) f :=) f for)");
    func_append(f, ln);
    func_append(f, "\t\t\t(j * int64_size realind :=)");
    func_append(f, "\t\t\t((j + one) * int64_size realind2 :=)");
    func_append(f, "\t\t\t(arr [ realind ] a =)");
    func_append(f, "\t\t\t(a + zero a :=)");
    func_append(f, "\t\t\t(arr [ realind2 ] b =)");
    func_append(f, "\t\t\t(b + zero b :=)");
    func_append(f, "\t\t\t(((a b >) f :=) f if)");
    func_append(f, "\t\t\t\t(b arr [ realind ] =)");
    func_append(f, "\t\t\t\t(a arr [ realind2 ] =)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t\t(j + one j :=)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t// median at midpoint");
    func_append(f, "\t(count / two mid :=)");
    snprintf(ln, sizeof(ln), "\t(mid * int64_size realind :=)");
    func_append(f, ln);
    func_append(f, "\t(arr [ realind ] a =)");
    func_append(f, "\t(a + zero a :=)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(a :print_i !)");
        func_append(f, "\t(:print_n !)");
    }
}



void emit_array_reverse(Program *prog, Function *f, int skip_input) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    const char *cv[] = {"5"};
    add_var_to_func(f, "int64", "count", 1, cv, 1);
    add_var_to_func(f, "int64", "arr", 5, cv, 0);
    add_var_to_func(f, "int64", "reverse", 5, cv, 0);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "j", 1, zv, 1);
    add_var_to_func(f, "int64", "temp", 1, zv, 1);
    add_var_to_func(f, "int64", "realind", 1, zv, 1);
    add_var_to_func(f, "int64", "realind2", 1, zv, 1);
    add_var_to_func(f, "int64", "a", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "input_prompt", 18, ps, 1);
    if (!skip_input) {
        func_append(f, "\t(zero i :=)");
        func_append(f, "\t(for-loop)");
        func_append(f, "\t(((i count <) f :=) f for)");
    func_append(f, "\t\t(input_prompt :print_s !)");
        func_append(f, "\t\t(i * int64_size realind :=)");
        func_append(f, "\t\t(a :input_i !)");
        func_append(f, "\t\t(a temp :=)");
        func_append(f, "\t\t(temp arr [ realind ] =)");
        func_append(f, "\t\t(i + one i :=)");
        func_append(f, "\t(next)");
    }
    func_append(f, "\t// reverse");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i count <) f :=) f for)");
    func_append(f, "\t\t(i * int64_size realind :=)");
    func_append(f, "\t\t(count - i - one j :=)");
    func_append(f, "\t\t(j * int64_size realind2 :=)");
    func_append(f, "\t\t(arr [ realind2 ] a =)");
    func_append(f, "\t\t(a + zero a :=)");
    func_append(f, "\t\t(a reverse [ realind ] =)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t// copy array");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i count <) f :=) f for)");
    func_append(f, "\t\t(i * int64_size realind :=)");
    func_append(f, "\t\t(reverse [ realind ] a =)");
    func_append(f, "\t\t(a + zero a :=)");
    func_append(f, "\t\t(a arr [ realind ] =)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t// print reversed");
        func_append(f, "\t(zero i :=)");
        func_append(f, "\t(for-loop)");
        func_append(f, "\t(((i count <) f :=) f for)");
        func_append(f, "\t\t(i * int64_size realind :=)");
        func_append(f, "\t\t(arr [ realind ] a =)");
        func_append(f, "\t\t(a + zero a :=)");
        func_append(f, "\t\t(a :print_i !)");
        func_append(f, "\t\t(:print_n !)");
        func_append(f, "\t\t(i + one i :=)");
        func_append(f, "\t(next)");
    }
}

void emit_array_find(Program *prog, Function *f, int skip_input) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    const char *cv[] = {"5"};
    add_var_to_func(f, "int64", "count", 1, cv, 1);
    add_var_to_func(f, "int64", "arr", 5, cv, 0);
    add_var_to_func(f, "int64", "key", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "found", 1, zv, 1);
    add_var_to_func(f, "int64", "realind", 1, zv, 1);
    add_var_to_func(f, "int64", "a", 1, zv, 1);
    add_var_to_func(f, "int64", "aux", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Enter a number: \""};
    const char *pk[] = {"\"Enter search key: \""};
    const char *vn[] = {"\" not found\""};
    add_var_to_func(f, "const-string", "input_prompt", 18, ps, 1);
    add_var_to_func(f, "const-string", "key_prompt", 20, pk, 1);
    add_var_to_func(f, "const-string", "not_str", 11, vn, 1);
    if (!skip_input) {
        func_append(f, "\t(zero i :=)");
        func_append(f, "\t(for-loop)");
        func_append(f, "\t(((i count <) f :=) f for)");
        func_append(f, "\t\t(input_prompt :print_s !)");
        func_append(f, "\t\t(i * int64_size realind :=)");
        func_append(f, "\t\t(a :input_i !)");
        func_append(f, "\t\t(a aux :=)");
        func_append(f, "\t\t(aux arr [ realind ] =)");
        func_append(f, "\t\t(i + one i :=)");
        func_append(f, "\t(next)");
    }
    func_append(f, "\t(key_prompt :print_s !)");
    func_append(f, "\t(key :input_i !)");
    func_append(f, "\t(zero found :=)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i count <) f :=) f for)");
    func_append(f, "\t\t(i * int64_size realind :=)");
    func_append(f, "\t\t(arr [ realind ] a =)");
    func_append(f, "\t\t(a + zero a :=)");
    func_append(f, "\t\t(((a key ==) f :=) f if)");
    func_append(f, "\t\t\t(one found :=)");
    func_append(f, "\t\t\t(:found_break jmp)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(:found_break)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(((found one ==) f :=) f if+)");
        func_append(f, "\t\t(i :print_i !)");
        func_append(f, "\t\t(:print_n !)");
        func_append(f, "\t(else)");
        func_append(f, "\t\t(not_str :print_s !)");
        func_append(f, "\t\t(:print_n !)");
        func_append(f, "\t(endif)");
    }
}



void emit_array_vmath(Program *prog, Function *f, int skip_input) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    const char *cv[] = {"5"};
    add_var_to_func(f, "int64", "count", 1, cv, 1);
    add_var_to_func(f, "int64", "arr", 5, cv, 0);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "mn", 1, zv, 1);
    add_var_to_func(f, "int64", "mx", 1, zv, 1);
    add_var_to_func(f, "int64", "sum", 1, zv, 1);
    add_var_to_func(f, "int64", "avg", 1, zv, 1);
    add_var_to_func(f, "int64", "realind", 1, zv, 1);
    add_var_to_func(f, "int64", "val", 1, zv, 1);
    add_var_to_func(f, "int64", "aux", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "input_prompt", 18, ps, 1);
    if (!skip_input) {
        func_append(f, "\t(zero i :=)");
        func_append(f, "\t(for-loop)");
        func_append(f, "\t(((i count <) f :=) f for)");
        func_append(f, "\t\t(input_prompt :print_s !)");
        func_append(f, "\t\t(i * int64_size realind :=)");
        func_append(f, "\t\t(val :input_i !)");
        func_append(f, "\t\t(val aux :=)");
        func_append(f, "\t\t(aux arr [ realind ] =)");
        func_append(f, "\t\t(i + one i :=)");
        func_append(f, "\t(next)");
    }
    func_append(f, "\t// compute stats");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(arr [ i ] mn =)");
    func_append(f, "\t(mn + zero mn :=)");
    func_append(f, "\t(arr [ i ] mx =)");
    func_append(f, "\t(mx + zero mx :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i count <) f :=) f for)");
    func_append(f, "\t\t(i * int64_size realind :=)");
    func_append(f, "\t\t(arr [ realind ] val =)");
    func_append(f, "\t\t(val + zero val :=)");
    func_append(f, "\t\t(sum + val sum :=)");
    func_append(f, "\t\t(((val mn <) f :=) f if)");
    func_append(f, "\t\t\t(val mn :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((val mx >) f :=) f if)");
    func_append(f, "\t\t\t(val mx :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(sum count / avg :=)");
    func_append(f, "\t// min");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(mn :print_i !)");
        func_append(f, "\t(:print_n !)");
    }
    func_append(f, "\t// max");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(mx :print_i !)");
        func_append(f, "\t(:print_n !)");
    }
    func_append(f, "\t// avg");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(avg :print_i !)");
        func_append(f, "\t(:print_n !)");
    }
}











void emit_fann_create(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "fann-lib.l1h");
    const char *zv[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "max_ann", 1, (const char *[]){"2"}, 1);
    add_var_to_func(f, "const-int64", "mod_handle", 1, zv, 1);
    add_var_to_func(f, "int64", "ret", 1, zv, 1);
    const char *fn[] = {"\"my_ann.net\""};
    add_var_to_func(f, "const-string", "ann_file", 12, fn, 1);
    const char *ps[] = {"\"Creating FANN neural network...\""};
    add_var_to_func(f, "const-string", "msg_str", 33, ps, 1);
    func_append(f, "\t(msg_str :print_s !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t// init FANN library");
    func_append(f, "\t(max_ann stpushi)");
    func_append(f, "\t(mod_handle stpushi)");
    func_append(f, "\t(fann_init_lib call)");
    func_append(f, "\t// create ANN from file");
    func_append(f, "\t(ann_file addr stpushi)");
    func_append(f, "\t(ret stpushi)");
    func_append(f, "\t(fann_create call)");
    func_append(f, "\t(:print_n !)");
}

void emit_fann_train(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "fann-lib.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1000"};
    const char *tv[] = {"100"};
    const char *thv[] = {"0001"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "max_ann", 1, (const char *[]){"2"}, 1);
    add_var_to_func(f, "const-int64", "mod_handle", 1, zv, 1);
    add_var_to_func(f, "int64", "ret", 1, zv, 1);
    add_var_to_func(f, "int64", "max_epochs", 1, ov, 1);
    add_var_to_func(f, "int64", "epochs_between", 1, tv, 1);
    add_var_to_func(f, "double", "desired_error", 1, thv, 1);
    const char *fn[] = {"\"my_ann.net\""};
    add_var_to_func(f, "const-string", "ann_file", 12, fn, 1);
    const char *df[] = {"\"train_data.dat\""};
    add_var_to_func(f, "const-string", "data_file", 16, df, 1);
    const char *ps[] = {"\"Training FANN neural network...\""};
    add_var_to_func(f, "const-string", "msg_str", 33, ps, 1);
    func_append(f, "\t(msg_str :print_s !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t// init FANN library");
    func_append(f, "\t(max_ann stpushi)");
    func_append(f, "\t(mod_handle stpushi)");
    func_append(f, "\t(fann_init_lib call)");
    func_append(f, "\t// create ANN from file");
    func_append(f, "\t(ann_file addr stpushi)");
    func_append(f, "\t(ret stpushi)");
    func_append(f, "\t(fann_create call)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t// train from data file");
    func_append(f, "\t(data_file addr stpushi)");
    func_append(f, "\t(max_epochs stpushi)");
    func_append(f, "\t(epochs_between stpushi)");
    func_append(f, "\t(desired_error stpushd)");
    func_append(f, "\t(ret stpushi)");
    func_append(f, "\t(fann_train_ann_file call)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t// save trained ANN");
    func_append(f, "\t(ann_file addr stpushi)");
    func_append(f, "\t(ret stpushi)");
    func_append(f, "\t(fann_save call)");
    func_append(f, "\t(:print_n !)");
}

void emit_fann_run(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "fann-lib.l1h");
    const char *zv[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "max_ann", 1, (const char *[]){"2"}, 1);
    add_var_to_func(f, "const-int64", "mod_handle", 1, zv, 1);
    add_var_to_func(f, "int64", "ret", 1, zv, 1);
    add_var_to_func(f, "int64", "num_inputs", 1, (const char *[]){"3"}, 1);
    add_var_to_func(f, "int64", "num_outputs", 1, (const char *[]){"1"}, 1);
    add_var_to_func(f, "double", "inputs", 3, (const char *[]){"10", "05", "03"}, 3);
    add_var_to_func(f, "double", "outputs", 1, zv, 1);
    const char *fn[] = {"\"my_ann.net\""};
    add_var_to_func(f, "const-string", "ann_file", 12, fn, 1);
    const char *ps[] = {"\"Running FANN inference...\""};
    add_var_to_func(f, "const-string", "msg_str", 28, ps, 1);
    const char *rs[] = {"\"Output: \""};
    add_var_to_func(f, "const-string", "out_str", 9, rs, 1);
    func_append(f, "\t(msg_str :print_s !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t// init FANN library");
    func_append(f, "\t(max_ann stpushi)");
    func_append(f, "\t(mod_handle stpushi)");
    func_append(f, "\t(fann_init_lib call)");
    func_append(f, "\t// load ANN from file");
    func_append(f, "\t(ann_file addr stpushi)");
    func_append(f, "\t(ret stpushi)");
    func_append(f, "\t(fann_read call)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t// run inference");
    func_append(f, "\t(inputs addr stpushi)");
    func_append(f, "\t(outputs addr stpushi)");
    func_append(f, "\t(num_inputs stpushi)");
    func_append(f, "\t(num_outputs stpushi)");
    func_append(f, "\t(ret stpushi)");
    func_append(f, "\t(fann_run call)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(out_str :print_s !)");
    func_append(f, "\t(outputs :print_d !)");
    func_append(f, "\t(:print_n !)");
}

void emit_average(Program *prog, Function *f, int skip_input) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "int64", "count", 1, (const char *[]){"5"}, 1);
    add_var_to_func(f, "int64", "arr", 5, zv, 0);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "sum", 1, zv, 1);
    add_var_to_func(f, "int64", "avg", 1, zv, 1);
    add_var_to_func(f, "int64", "realind", 1, zv, 1);
    add_var_to_func(f, "int64", "val", 1, zv, 1);
    add_var_to_func(f, "int64", "aux", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "input_prompt", 18, ps, 1);
    if (!skip_input) {
        // input loop
        func_append(f, "\t(zero i :=)");
        func_append(f, "\t(for-loop)");
        func_append(f, "\t(((i count <) f :=) f for)");
        func_append(f, "\t\t(input_prompt :print_s !)");
        func_append(f, "\t\t(i * int64_size realind :=)");
        func_append(f, "\t\t(val :input_i !)");
        func_append(f, "\t\t(val aux :=)");
        func_append(f, "\t\t(aux arr [ realind ] =)");
        func_append(f, "\t\t(i + one i :=)");
        func_append(f, "\t(next)");
    }
    // compute average
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i count <) f :=) f for)");
    func_append(f, "\t\t(i * int64_size realind :=)");
    func_append(f, "\t\t(arr [ realind ] val =)");
    func_append(f, "\t\t(val + zero val :=)");
    func_append(f, "\t\t(sum + val sum :=)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(sum count / avg :=)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(avg :print_i !)");
        func_append(f, "\t(:print_n !)");
    }
}

void emit_selection_sort(Program *prog, Function *f, int count, int skip_input) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    char vs[16];
    snprintf(vs, sizeof(vs), "%d", count);
    const char *cv[] = {vs};
    add_var_to_func(f, "int64", "count", 1, cv, 1);
    add_var_to_func(f, "int64", "arr", count, cv, 0);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "j", 1, zv, 1);
    add_var_to_func(f, "int64", "min_idx", 1, zv, 1);
    add_var_to_func(f, "int64", "temp", 1, zv, 1);
    add_var_to_func(f, "int64", "realind", 1, zv, 1);
    add_var_to_func(f, "int64", "realind2", 1, zv, 1);
    add_var_to_func(f, "int64", "a", 1, zv, 1);
    add_var_to_func(f, "int64", "b", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "input_prompt", 18, ps, 1);
    if (!skip_input) {
        // input values
        func_append(f, "\t(zero i :=)");
        func_append(f, "\t(for-loop)");
        func_append(f, "\t(((i count <) f :=) f for)");
        func_append(f, "\t\t(input_prompt :print_s !)");
        func_append(f, "\t\t(i * int64_size realind :=)");
        func_append(f, "\t\t(a :input_i !)");
        func_append(f, "\t\t(a temp :=)");
        func_append(f, "\t\t(temp arr [ realind ] =)");
        func_append(f, "\t\t(i + one i :=)");
        func_append(f, "\t(next)");
    }
    // selection sort
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i count <) f :=) f for)");
    func_append(f, "\t\t(i min_idx :=)");
    func_append(f, "\t\t(i + one j :=)");
    func_append(f, "\t\t(for-loop)");
    func_append(f, "\t\t(((j count <) f :=) f for)");
    func_append(f, "\t\t\t(j * int64_size realind :=)");
    func_append(f, "\t\t\t(min_idx * int64_size realind2 :=)");
    func_append(f, "\t\t\t(arr [ realind ] a =)");
    func_append(f, "\t\t\t(a + zero a :=)");
    func_append(f, "\t\t\t(arr [ realind2 ] b =)");
    func_append(f, "\t\t\t(b + zero b :=)");
    func_append(f, "\t\t\t(((a b <) f :=) f if)");
    func_append(f, "\t\t\t\t(j min_idx :=)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t\t(j + one j :=)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t(i * int64_size realind :=)");
    func_append(f, "\t\t(min_idx * int64_size realind2 :=)");
    func_append(f, "\t\t(arr [ realind ] b =)");
    func_append(f, "\t\t(b temp :=)");
    func_append(f, "\t\t(arr [ realind2 ] a =)");
    func_append(f, "\t\t(a b :=)");
    func_append(f, "\t\t(b arr [ realind ] =)");
    func_append(f, "\t\t(temp arr [ realind2 ] =)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(zero i :=)");
        func_append(f, "\t(for-loop)");
        func_append(f, "\t(((i count <) f :=) f for)");
        func_append(f, "\t\t(i * int64_size realind :=)");
        func_append(f, "\t\t(arr [ realind ] a =)");
        func_append(f, "\t\t(a + zero a :=)");
        func_append(f, "\t\t(a :print_i !)");
        func_append(f, "\t\t(:print_n !)");
        func_append(f, "\t\t(i + one i :=)");
        func_append(f, "\t(next)");
    }
}



void emit_bubble_sort(Program *prog, Function *f, int skip_input) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "int64", "count", 1, (const char *[]){"5"}, 1);
    add_var_to_func(f, "int64", "arr", 5, zv, 0);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "j", 1, zv, 1);
    add_var_to_func(f, "int64", "temp", 1, zv, 1);
    add_var_to_func(f, "int64", "realind", 1, zv, 1);
    add_var_to_func(f, "int64", "realind2", 1, zv, 1);
    add_var_to_func(f, "int64", "a", 1, zv, 1);
    add_var_to_func(f, "int64", "b", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "input_prompt", 18, ps, 1);
    if (!skip_input) {
        func_append(f, "\t(zero i :=)");
        func_append(f, "\t(for-loop)");
        func_append(f, "\t(((i count <) f :=) f for)");
        func_append(f, "\t\t(input_prompt :print_s !)");
        func_append(f, "\t\t(i * int64_size realind :=)");
        func_append(f, "\t\t(a :input_i !)");
        func_append(f, "\t\t(a temp :=)");
        func_append(f, "\t\t(temp arr [ realind ] =)");
        func_append(f, "\t\t(i + one i :=)");
        func_append(f, "\t(next)");
    }
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i count <) f :=) f for)");
    func_append(f, "\t\t(zero j :=)");
    func_append(f, "\t\t(for-loop)");
    func_append(f, "\t\t(((j count i - one - <) f :=) f for)");
    func_append(f, "\t\t\t(j * int64_size realind :=)");
    func_append(f, "\t\t\t(j + one * int64_size realind2 :=)");
    func_append(f, "\t\t\t(arr [ realind ] a =)");
    func_append(f, "\t\t\t(arr [ realind2 ] b =)");
    func_append(f, "\t\t\t(a + zero a :=)");
    func_append(f, "\t\t\t(b + zero b :=)");
    func_append(f, "\t\t\t(((a b >) f :=) f if)");
    func_append(f, "\t\t\t\t(a temp :=)");
    func_append(f, "\t\t\t\t(b a :=)");
    func_append(f, "\t\t\t\t(temp arr [ realind ] =)");
    func_append(f, "\t\t\t\t(a arr [ realind2 ] =)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t\t(j + one j :=)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(zero i :=)");
        func_append(f, "\t(for-loop)");
        func_append(f, "\t(((i count <) f :=) f for)");
        func_append(f, "\t\t(i * int64_size realind :=)");
        func_append(f, "\t\t(arr [ realind ] a =)");
        func_append(f, "\t\t(a + zero a :=)");
        func_append(f, "\t\t(a :print_i !)");
        func_append(f, "\t\t(:print_n !)");
        func_append(f, "\t\t(i + one i :=)");
        func_append(f, "\t(next)");
    }
}

void emit_standard_deviation(Program *prog, Function *f, int skip_input) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "int64", "count", 1, (const char *[]){"5"}, 1);
    add_var_to_func(f, "int64", "arr", 5, zv, 0);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "sum", 1, zv, 1);
    add_var_to_func(f, "int64", "mean", 1, zv, 1);
    add_var_to_func(f, "int64", "diff", 1, zv, 1);
    add_var_to_func(f, "int64", "sq_sum", 1, zv, 1);
    add_var_to_func(f, "int64", "variance", 1, zv, 1);
    add_var_to_func(f, "int64", "stddev", 1, zv, 1);
    add_var_to_func(f, "int64", "realind", 1, zv, 1);
    add_var_to_func(f, "int64", "val", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *oh[] = {"100"};
    add_var_to_func(f, "const-int64", "one_hundred", 1, oh, 1);
    const char *ps[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "input_prompt", 18, ps, 1);
    const char *rs[] = {"\"Standard deviation: \""};
    add_var_to_func(f, "const-string", "result_str", 22, rs, 1);
    if (!skip_input) {
        func_append(f, "\t(zero i :=)");
        func_append(f, "\t(for-loop)");
        func_append(f, "\t(((i count <) f :=) f for)");
        func_append(f, "\t\t(input_prompt :print_s !)");
        func_append(f, "\t\t(i * int64_size realind :=)");
        func_append(f, "\t\t(val :input_i !)");
        func_append(f, "\t\t(val arr [ realind ] =)");
        func_append(f, "\t\t(i + one i :=)");
        func_append(f, "\t(next)");
    }
    func_append(f, "\t(zero sum :=)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i count <) f :=) f for)");
    func_append(f, "\t\t(i * int64_size realind :=)");
    func_append(f, "\t\t(arr [ realind ] val =)");
    func_append(f, "\t\t(sum val + sum :=)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(sum count / mean :=)");
    func_append(f, "\t(zero sq_sum :=)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i count <) f :=) f for)");
    func_append(f, "\t\t(i * int64_size realind :=)");
    func_append(f, "\t\t(arr [ realind ] val =)");
    func_append(f, "\t\t(mean val - diff :=)");
    func_append(f, "\t\t(((diff zero <) f :=) f if)");
    func_append(f, "\t\t\t(zero diff - diff :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(diff diff * val :=)");
    func_append(f, "\t\t(sq_sum val + sq_sum :=)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(sq_sum count / variance :=)");
    func_append(f, "\t(variance stddev :=)");
    func_append(f, "\t(zero sq_sum :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((sq_sum one_hundred <) f :=) f for)");
    func_append(f, "\t\t(sq_sum one + sq_sum :=)");
    func_append(f, "\t\t(stddev stddev + variance stddev / - stddev :=)");
    func_append(f, "\t(next)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(result_str :print_s !)");
        func_append(f, "\t(stddev :print_i !)");
        func_append(f, "\t(:print_n !)");
    }
}

void emit_double_math(Program *prog, Function *f, const char *op) {
    add_include(prog, "intr-func.l1h");
    const char *zd[] = {"0.0"};
    const char *fz[] = {"0"};
    add_var_to_func(f, "const-double", "zerod", 1, zd, 1);
    add_var_to_func(f, "double", "a", 1, zd, 1);
    add_var_to_func(f, "double", "b", 1, zd, 1);
    add_var_to_func(f, "double", "r", 1, zd, 1);
    add_var_to_func(f, "int64", "f", 1, fz, 1);
    const char *pa[] = {"\"Enter first number: \""};
    const char *pb[] = {"\"Enter second number: \""};
    add_var_to_func(f, "const-string", "prompt_a", 21, pa, 1);
    add_var_to_func(f, "const-string", "prompt_b", 22, pb, 1);
    func_append(f, "\t(prompt_a :print_s !)");
    func_append(f, "\t(a :input_d !)");
    func_append(f, "\t(prompt_b :print_s !)");
    func_append(f, "\t(b :input_d !)");
    const char *opr = "+d";
    if (strcmp(op, "sub") == 0) opr = "-d";
    else if (strcmp(op, "mul") == 0) opr = "*d";
    else if (strcmp(op, "div") == 0) opr = "/d";
    char ln[256];
    snprintf(ln, sizeof(ln), "\t((a b %s) r =)", opr);
    func_append(f, ln);
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(r :print_d !)");
        func_append(f, "\t(:print_n !)");
    }
}

void emit_double_average(Program *prog, Function *f, int skip_input) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zd[] = {"0.0"};
    const char *cd[] = {"5.0"};
    const char *fz[] = {"0"};
    add_var_to_func(f, "const-double", "zerod", 1, zd, 1);
    add_var_to_func(f, "const-double", "countd", 1, cd, 1);
    add_var_to_func(f, "const-int64", "zero", 1, fz, 1);
    add_var_to_func(f, "const-int64", "one", 1, (const char *[]){"1"}, 1);
    add_var_to_func(f, "int64", "count", 1, (const char *[]){"5"}, 1);
    add_var_to_func(f, "double", "arr", 5, zd, 0);
    add_var_to_func(f, "int64", "i", 1, fz, 1);
    add_var_to_func(f, "int64", "realind", 1, fz, 1);
    add_var_to_func(f, "double", "val", 1, zd, 1);
    add_var_to_func(f, "double", "sum", 1, zd, 1);
    add_var_to_func(f, "double", "avg", 1, zd, 1);
    add_var_to_func(f, "int64", "f", 1, fz, 1);
    const char *ps[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "input_prompt", 18, ps, 1);
    if (!skip_input) {
        func_append(f, "\t(zero i :=)");
        func_append(f, "\t(for-loop)");
        func_append(f, "\t(((i count <) f :=) f for)");
        func_append(f, "\t\t(input_prompt :print_s !)");
        func_append(f, "\t\t(i * double_size realind :=)");
        func_append(f, "\t\t(val :input_d !)");
        func_append(f, "\t\t(val arr [ realind ] =)");
        func_append(f, "\t\t(i + one i :=)");
        func_append(f, "\t(next)");
    }
    func_append(f, "\t(zerod sum :=)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i count <) f :=) f for)");
    func_append(f, "\t\t(i * double_size realind :=)");
    func_append(f, "\t\t(arr [ realind ] val =)");
    func_append(f, "\t\t((sum val +d) sum =)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t((sum countd /d) avg =)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(avg :print_d !)");
        func_append(f, "\t(:print_n !)");
    }
}







































int parse_task(const char *prompt, TaskProfile *task) {
    char buf[MAX_PROMPT];
    snprintf(buf, sizeof(buf), "%s", prompt);
    to_lowercase(buf);
    snprintf(task->prompt, sizeof(task->prompt), "%s", prompt);

    // default type
    snprintf(task->type, sizeof(task->type), "%s", "int64");

    // detect type
    if (strstr(buf, "byte") || strstr(buf, "int8")) snprintf(task->type, sizeof(task->type), "%s", "byte");
    else if (strstr(buf, "int16") || strstr(buf, "short")) snprintf(task->type, sizeof(task->type), "%s", "int16");
    else if (strstr(buf, "int32")) snprintf(task->type, sizeof(task->type), "%s", "int32");
    else if (strstr(buf, "int64") || has_word(buf, "int") || strstr(buf, "long")) snprintf(task->type, sizeof(task->type), "%s", "int64");
    else if (strstr(buf, "double") || strstr(buf, "float") || strstr(buf, "real") || strstr(buf, "komma")) snprintf(task->type, sizeof(task->type), "%s", "double");

    // extract numbers
    task->num_literals = extract_numbers(prompt, task->literals, MAX_NUMS);
    // also extract double literals (decimal numbers)
    int has_decimals = 0;
    for (int di = 0; di < task->num_literals && di < MAX_NUMS; di++)
        task->double_literals[di] = (double)task->literals[di];
    {
        char dscan[MAX_PROMPT];
        snprintf(dscan, sizeof(dscan), "%s", buf);
        char *dp = dscan;
        int di = 0;
        while (*dp && di < MAX_NUMS) {
            if (isdigit(*dp)) {
                char *start = dp;
                while (*dp && (isdigit(*dp) || *dp == '.')) dp++;
                if (strchr(start, '.') != NULL) {
                    task->double_literals[di] = atof(start);
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
            snprintf(task->type, sizeof(task->type), "%s", "double");
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
        snprintf(task->op, sizeof(task->op), "%s", op);
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
        char op_names[32][8];
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
                        snprintf(op_names[found_ops], sizeof(op_names[0]), "%s", ops[oi].rpn_op);
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
                    char tn[8]; memcpy(tn, op_names[i], sizeof(tn));
                    memcpy(op_names[i], op_names[j], sizeof(op_names[i]));
                    memcpy(op_names[j], tn, sizeof(op_names[j]));
                }
            }
        }
        // Pair operations with literals in prompt order
        int lit_idx = 0;
        for (int oi = 0; oi < found_ops && lit_idx < task->num_literals; oi++) {
            snprintf(task->op_seq[task->num_ops], sizeof(task->op_seq[0]), "%s", op_names[oi]);
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

    if (task->has_sort && task->has_input) {
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
        // string_cat unless compare is explicit (intentionally empty)
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

    if (has_word(buf, "sort") && (has_word(buf, "stats") || has_word(buf, "statistics") || has_word(buf, "statistik") || has_word(buf, "analyze") || has_word(buf, "analyse")))
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
    { snprintf(task->title, sizeof(task->title), "%s", prompt); }

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

    snprintf(task->title, sizeof(task->title), "%s", prompt);

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

// ==================== FUNCTION EMITTER ====================





void emit_insertion_sort(Program *prog, Function *f, int count, int skip_input) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    char vs[16], ln[256];
    snprintf(vs, sizeof(vs), "%d", count);
    const char *cv[] = {vs};
    add_var_to_func(f, "int64", "count", 1, cv, 1);
    add_var_to_func(f, "int64", "arr", (count > 0 ? count : 1), cv, 0);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "j", 1, zv, 1);
    add_var_to_func(f, "int64", "key", 1, zv, 1);
    add_var_to_func(f, "int64", "realind", 1, zv, 1);
    add_var_to_func(f, "int64", "realind2", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "input_prompt", 17, ps, 1);
    if (!skip_input) {
        func_append(f, "\t(zero i :=)");
        func_append(f, "\t(for-loop)");
        snprintf(ln, sizeof(ln), "\t(((i count <) f :=) f for)");
        func_append(f, ln);
        func_append(f, "\t\t(input_prompt :print_s !)");
        func_append(f, "\t\t(i * int64_size realind :=)");
        func_append(f, "\t\t(key :input_i !)");
        func_append(f, "\t\t(key arr [ realind ] =)");
        func_append(f, "\t\t(i + one i :=)");
        func_append(f, "\t(next)");
    }
    func_append(f, "\t// insertion sort");
    func_append(f, "\t(one i :=)");
    func_append(f, "\t(for-loop)");
    snprintf(ln, sizeof(ln), "\t(((i count <) f :=) f for)");
    func_append(f, ln);
    func_append(f, "\t\t(i * int64_size realind :=)");
    func_append(f, "\t\t(arr [ realind ] key =)");
    func_append(f, "\t\t(i one - j :=)");
    func_append(f, "\t\t(for-loop)");
    func_append(f, "\t\t(((j zero >=) f :=) (f :=) f for)");
    func_append(f, "\t\t\t(j * int64_size realind2 :=)");
    func_append(f, "\t\t\t(arr [ realind2 ] f =)");
    func_append(f, "\t\t\t((f key >) f :=) f if)");
    func_append(f, "\t\t\t\t((j + one) * int64_size realind :=)");
    func_append(f, "\t\t\t\t(f arr [ realind ] =)");
    func_append(f, "\t\t\t\t(j one - j :=)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t\t(((j zero >=) ! f :=) f for)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t((j + one) * int64_size realind :=)");
    func_append(f, "\t\t(key arr [ realind ] =)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t// print sorted");
        func_append(f, "\t(zero i :=)");
        func_append(f, "\t(for-loop)");
        snprintf(ln, sizeof(ln), "\t(((i count <) f :=) f for)");
        func_append(f, ln);
        func_append(f, "\t\t(i * int64_size realind :=)");
        func_append(f, "\t\t(arr [ realind ] f =)");
        func_append(f, "\t\t(f :print_i !)");
        func_append(f, "\t\t(:print_n !)");
        func_append(f, "\t\t(i + one i :=)");
        func_append(f, "\t(next)");
    }
}
static void ensure_exit(Function *f, int last_step) {
    if (!last_step) return;
    const char *zv[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    func_append(f, "\t(zero :exit !)");
}

// Dispatch macro for uniform emitter blocks (no early return)
#define DISPATCH(field, func) \
    if (task->field) { func(prog, f); emitted = 1; }

// ==================== EXTRA EMITTER DISPATCH TABLE ====================
// Replaces the ~100-line switch with a function-pointer table
typedef void (*ExtraEmitterFunc)(Program*, Function*, TaskProfile*);

#define EE_SIMPLE(name) \
    static void ee_##name(Program *p, Function *f, TaskProfile *t) { \
        (void)t; emit_##name(p, f); \
    }

EE_SIMPLE(fann_create)
EE_SIMPLE(fann_train)
EE_SIMPLE(fann_run)

// Wrappers for emitters with extra parameters
static void ee_double_math(Program *p, Function *f, TaskProfile *t) { emit_double_math(p, f, t->op); }
static void ee_double_average(Program *p, Function *f, TaskProfile *t) { emit_double_average(p, f, t->skip_input); }
static void ee_bubble_sort(Program *p, Function *f, TaskProfile *t) { emit_bubble_sort(p, f, t->skip_input); }
static void ee_standard_deviation(Program *p, Function *f, TaskProfile *t) { emit_standard_deviation(p, f, t->skip_input); }
static void ee_insertion_sort(Program *p, Function *f, TaskProfile *t) { emit_insertion_sort(p, f, 5, t->skip_input); }
static void ee_selection_sort(Program *p, Function *f, TaskProfile *t) { emit_selection_sort(p, f, 5, t->skip_input); }
static void ee_average(Program *p, Function *f, TaskProfile *t) { emit_average(p, f, t->skip_input); }
static void ee_array_reverse(Program *p, Function *f, TaskProfile *t) { emit_array_reverse(p, f, t->skip_input); }
static void ee_array_find(Program *p, Function *f, TaskProfile *t) { emit_array_find(p, f, t->skip_input); }
static void ee_array_vmath(Program *p, Function *f, TaskProfile *t) { emit_array_vmath(p, f, t->skip_input); }
static void ee_math(Program *p, Function *f, TaskProfile *t) { emit_math(p, f, t->type, t->op, t->literals, t->num_literals > 2 ? 3 : 2, 1); }
static void ee_input_loop(Program *p, Function *f, TaskProfile *t) { emit_input_loop(p, f, t->input_count > 0 ? t->input_count : 5, t->type, NULL); }
static void ee_input_sort(Program *p, Function *f, TaskProfile *t) { emit_input_sort(p, f, t->input_sort_count > 0 ? t->input_sort_count : 5, t->skip_input, t->has_descending, t->type); }
static void ee_median(Program *p, Function *f, TaskProfile *t) {
    int c = t->median_count > 0 ? t->median_count : 5;
    emit_median(p, f, c, t->skip_input);
}

#define EE_MATH_OP(name, type, op) \
    static void ee_##name(Program *p, Function *f, TaskProfile *t) { \
        int n = t->num_literals > 2 ? 3 : (t->num_literals < 2 ? 2 : t->num_literals); \
        int vals[3] = {10, 3, 5}; \
        if (t->num_literals >= 1) vals[0] = t->literals[0]; \
        if (t->num_literals >= 2) vals[1] = t->literals[1]; \
        if (t->num_literals >= 3) vals[2] = t->literals[2]; \
        emit_math(p, f, type, op, vals, n, 1); \
    }

EE_MATH_OP(add, "int64", "add")
EE_MATH_OP(sub, "int64", "sub")
EE_MATH_OP(mul, "int64", "mul")
EE_MATH_OP(div, "int64", "div")
EE_MATH_OP(double_add, "double", "add")
EE_MATH_OP(double_sub, "double", "sub")
EE_MATH_OP(double_mul, "double", "mul")
EE_MATH_OP(double_div, "double", "div")

// Table indexed by emitter index (0..NUM_EMITTERS-1), NULL = no-op
static ExtraEmitterFunc ee_table[NUM_EMITTERS] = {
    [0] = ee_math, [1] = ee_input_loop,
    [8] = ee_input_sort, [9] = ee_median,
    [13] = ee_array_reverse,
    [14] = ee_array_find, [16] = ee_array_vmath,
    [34] = ee_fann_create,
    [35] = ee_fann_train, [36] = ee_fann_run, [37] = ee_average,
    [38] = ee_selection_sort, [52] = ee_bubble_sort,
    [56] = ee_standard_deviation, [60] = ee_double_math,
    [62] = ee_double_average,
    [71] = ee_insertion_sort,
    [140] = ee_add, [141] = ee_sub, [142] = ee_mul, [143] = ee_div,
    [144] = ee_double_add, [145] = ee_double_sub,
    [146] = ee_double_mul, [147] = ee_double_div,
};

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

    int emitted = 0;

    // Pre-declare inherited variables (dedup by name ensures emitter's own decls get skipped)
    for (int vi = 0; vi < task->num_inherit_vars; vi++) {
        int adj_count = task->inherit_var_counts[vi];
        if (adj_count <= 1) adj_count = 5 + retry_seed;
        char cvs[16];
        snprintf(cvs, sizeof(cvs), "%d", adj_count);
        const char *iv[1] = {cvs};
        add_var_to_func(f, task->inherit_var_types[vi], task->inherit_var_names[vi],
                        adj_count, iv, 1);
    }
    // Extra emitters: these run in sequence after the primary emitter
    // They are called via generate_from_task on the same function

    // Dispatch to emitter based on task profile
    if (task->has_input_sort) {
        int c = task->input_sort_count > 0 ? task->input_sort_count : 5;
        if (task->inherit_var[0]) c = task->inherit_count > 0 ? task->inherit_count : c;
        int saved_quiet = dataflow_quiet_mode;
        if (task->has_max || task->has_min) dataflow_quiet_mode = 1;
        emit_input_sort(prog, f, c, task->skip_input, task->has_descending, task->type);
        dataflow_quiet_mode = saved_quiet;
        int is_double = (strcmp(task->type, "double") == 0);
        const char *size_var = is_double ? "double_size" : "int64_size";
        if (task->has_max) {
    char ln[512];
    const char *zv[] = {"0"};
    const char *zd[] = {"0.0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    const char *thv[] = {"3"};
    const char *fov[] = {"4"};
    const char *fiv[] = {"5"};
    const char *cellsv[] = {"25"};
    add_var_to_func(f, is_double ? "const-double" : "const-int64", is_double ? "zerod" : "zero", 1, is_double ? zd : zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "three", 1, thv, 1);
    add_var_to_func(f, "const-int64", "four", 1, fov, 1);
    add_var_to_func(f, "const-int64", "five", 1, fiv, 1);
    add_var_to_func(f, "const-int64", "cells", 1, cellsv, 1);
    add_var_to_func(f, "const-int64", "cols", 1, fiv, 1);
            func_append(f, "\t// print largest");
            func_append(f, "\t(count one - temp :=)");
            snprintf(ln, sizeof(ln), "\t(temp * %s realind :=)", size_var);
            func_append(f, ln);
            func_append(f, "\t(arr [ realind ] a =)");
            func_append(f, is_double ? "\t(a + zerod a :=)" : "\t(a + zero a :=)");
            func_append(f, is_double ? "\t(a :print_d !)" : "\t(a :print_i !)");
            func_append(f, "\t(:print_n !)");
        }
        if (task->has_min) {
            char ln[512];
            const char *zv[] = {"0"};
            const char *zd[] = {"0.0"};
            add_var_to_func(f, is_double ? "const-double" : "const-int64", is_double ? "zerod" : "zero", 1, is_double ? zd : zv, 1);
            func_append(f, "\t// print smallest");
            snprintf(ln, sizeof(ln), "\t(zero * %s realind :=)", size_var);
            func_append(f, ln);
            func_append(f, "\t(arr [ realind ] a =)");
            func_append(f, is_double ? "\t(a + zerod a :=)" : "\t(a + zero a :=)");
            func_append(f, is_double ? "\t(a :print_d !)" : "\t(a :print_i !)");
            func_append(f, "\t(:print_n !)");
        }
        emitted = 1;
    }
    // Standalone min/max output for inherited arrays (when sort+input was in a previous step)
    if ((task->has_max || task->has_min) && !task->has_input_sort && task->inherit_var[0]) {
        int is_double = (strcmp(task->type, "double") == 0);
        if (!is_double) {
            for (int vi = 0; vi < task->num_inherit_vars; vi++)
                if (strcmp(task->inherit_var_types[vi], "double") == 0) { is_double = 1; break; }
        }
        const char *zv[] = {"0"};
        const char *zd[] = {"0.0"};
        const char *ov[] = {"1"};
        add_var_to_func(f, is_double ? "const-double" : "const-int64", is_double ? "zerod" : "zero", 1, is_double ? zd : zv, 1);
        add_var_to_func(f, "const-int64", "one", 1, ov, 1);
        char cvs[16];
        snprintf(cvs, sizeof(cvs), "%d", task->inherit_count > 0 ? task->inherit_count : 5);
        const char *cv[] = {cvs};
        add_var_to_func(f, "int64", "count", 1, cv, 1);
        add_var_to_func(f, "int64", "temp", 1, zv, 1);
        add_var_to_func(f, "int64", "realind", 1, zv, 1);
        add_var_to_func(f, is_double ? "double" : "int64", "a", 1, is_double ? zd : zv, 1);
        add_var_to_func(f, "int64", "f", 1, zv, 1);
        const char *size_var = is_double ? "double_size" : "int64_size";
        if (task->has_max) {
            char ln[512];
            func_append(f, "\t// print largest");
            func_append(f, "\t(count one - temp :=)");
            snprintf(ln, sizeof(ln), "\t(temp * %s realind :=)", size_var);
            func_append(f, ln);
            snprintf(ln, sizeof(ln), "\t(%s [ realind ] a =)", task->inherit_var);
            func_append(f, ln);
            func_append(f, is_double ? "\t(a + zerod a :=)" : "\t(a + zero a :=)");
            func_append(f, is_double ? "\t(a :print_d !)" : "\t(a :print_i !)");
            func_append(f, "\t(:print_n !)");
        }
        if (task->has_min) {
            char ln[512];
            func_append(f, "\t// print smallest");
            snprintf(ln, sizeof(ln), "\t(zero * %s realind :=)", size_var);
            func_append(f, ln);
            snprintf(ln, sizeof(ln), "\t(%s [ realind ] a =)", task->inherit_var);
            func_append(f, ln);
            func_append(f, is_double ? "\t(a + zerod a :=)" : "\t(a + zero a :=)");
            func_append(f, is_double ? "\t(a :print_d !)" : "\t(a :print_i !)");
            func_append(f, "\t(:print_n !)");
        }
        emitted = 1;
    }
    if (task->has_median) {
        int c = task->median_count > 0 ? task->median_count : 5;
        emit_median(prog, f, c, task->skip_input);
        emitted = 1;
    }
    if (task->has_array_reverse) {
        if (task->inherit_var[0]) {
            char cvs[16];
            snprintf(cvs, sizeof(cvs), "%d", task->inherit_count > 0 ? task->inherit_count : 5);
            const char *icv[] = {cvs};
            add_var_to_func(f, "int64", "count", 1, icv, 1);
        }
        emit_array_reverse(prog, f, task->skip_input);
        emitted = 1;
    }
    if (task->has_array_find) {
        if (task->inherit_var[0]) {
            char cvs[16];
            snprintf(cvs, sizeof(cvs), "%d", task->inherit_count > 0 ? task->inherit_count : 5);
            const char *icv[] = {cvs};
            add_var_to_func(f, "int64", "count", 1, icv, 1);
        }
        emit_array_find(prog, f, task->skip_input);
        emitted = 1;
    }
    if (task->has_array_vmath) {
        if (task->inherit_var[0]) {
            char cvs[16];
            snprintf(cvs, sizeof(cvs), "%d", task->inherit_count > 0 ? task->inherit_count : 5);
            const char *icv[] = {cvs};
            add_var_to_func(f, "int64", "count", 1, icv, 1);
        }
        emit_array_vmath(prog, f, task->skip_input);
        emitted = 1;
    }
    // specific operation + literals (higher priority than generic)
    if (task->has_add && task->has_literals && !task->has_input) {
        int n = task->num_literals > 2 ? 3 : (task->num_literals < 2 ? 2 : task->num_literals);
        int vals[3] = {10, 3, 5};
        if (task->num_literals >= 1) vals[0] = task->literals[0];
        if (task->num_literals >= 2) vals[1] = task->literals[1];
        if (task->num_literals >= 3) vals[2] = task->literals[2];
        emit_math(prog, f, task->type, "add", vals, n, 1);
        emitted = 1;
    }
    if (task->has_sub && task->has_literals && !task->has_input) {
        int n = task->num_literals > 2 ? 3 : (task->num_literals < 2 ? 2 : task->num_literals);
        int vals[3] = {10, 3, 5};
        if (task->num_literals >= 1) vals[0] = task->literals[0];
        if (task->num_literals >= 2) vals[1] = task->literals[1];
        if (task->num_literals >= 3) vals[2] = task->literals[2];
        emit_math(prog, f, task->type, "sub", vals, n, 1);
        emitted = 1;
    }
    if (task->has_mul && task->has_literals && !task->has_input) {
        int n = task->num_literals > 2 ? 3 : (task->num_literals < 2 ? 2 : task->num_literals);
        int vals[3] = {10, 3, 5};
        if (task->num_literals >= 1) vals[0] = task->literals[0];
        if (task->num_literals >= 2) vals[1] = task->literals[1];
        if (task->num_literals >= 3) vals[2] = task->literals[2];
        emit_math(prog, f, task->type, "mul", vals, n, 1);
        emitted = 1;
    }
    if (task->has_div && task->has_literals && !task->has_input) {
        int n = task->num_literals > 2 ? 3 : (task->num_literals < 2 ? 2 : task->num_literals);
        int vals[3] = {10, 3, 5};
        if (task->num_literals >= 1) vals[0] = task->literals[0];
        if (task->num_literals >= 2) vals[1] = task->literals[1];
        if (task->num_literals >= 3) vals[2] = task->literals[2];
        emit_math(prog, f, task->type, "div", vals, n, 1);
        emitted = 1;
    }
    // Input + operation(s) with literal operand(s): read one number, apply ops
    if (!emitted && task->has_input && task->has_operation && task->has_literals
        && task->input_count == 0 && task->num_literals >= 1) {
        int is_double = (strcmp(task->type, "double") == 0);
        const char *const_type = is_double ? "const-double" : "const-int64";
        const char *var_type = is_double ? "double" : "int64";
        const char *input_op = is_double ? ":input_d !" : ":input_i !";
        const char *print_op = is_double ? ":print_d !" : ":print_i !";
        const char *zero_val = is_double ? "0.0" : "0";
        const char *one_val = is_double ? "1.0" : "1";
        const char *zv[] = {zero_val};
        const char *ov[] = {one_val};
        add_var_to_func(f, const_type, "zero", 1, zv, 1);
        add_var_to_func(f, const_type, "one", 1, ov, 1);
        add_var_to_func(f, var_type, "a", 1, zv, 1);
        add_var_to_func(f, "int64", "f", 1, (const char *[]){"0"}, 1);
        if (task->has_even_odd && !is_double) {
            const char *tv[] = {"2"};
            add_var_to_func(f, "const-int64", "two", 1, tv, 1);
            add_var_to_func(f, "int64", "mod", 1, zv, 1);
            const char *es[] = {"\" is even\""};
            const char *os[] = {"\" is odd\""};
            add_var_to_func(f, "const-string", "even_str", 10, es, 1);
            add_var_to_func(f, "const-string", "odd_str", 9, os, 1);
        }
        const char *ps[] = {"\"Enter a number: \""};
        add_var_to_func(f, "const-string", "input_prompt", 18, ps, 1);
        char ln[512];
        func_append(f, "\t(input_prompt :print_s !)");
        snprintf(ln, sizeof(ln), "\t(a %s", input_op);
        func_append(f, ln);
        // declare each literal as a named variable
        char var_names[32][16];
        int num_vars = 0;
        for (int li = 0; li < task->num_literals && li < 32; li++) {
            snprintf(var_names[li], sizeof(var_names[li]), "v%d", li);
            char vs[32];
            if (is_double)
                snprintf(vs, sizeof(vs), "%.10g", task->double_literals[li]);
            else
                snprintf(vs, sizeof(vs), "%d", task->literals[li]);
            const char *vv[] = {vs};
            add_var_to_func(f, const_type, var_names[li], 1, vv, 1);
            num_vars++;
        }
        if (task->num_ops > 0) {
            for (int oi = 0; oi < task->num_ops && oi < num_vars; oi++) {
                const char *sym = is_double ? "+d" : "+";
                if (strcmp(task->op_seq[oi], "sub") == 0) sym = is_double ? "-d" : "-";
                else if (strcmp(task->op_seq[oi], "mul") == 0) sym = is_double ? "*d" : "*";
                else if (strcmp(task->op_seq[oi], "div") == 0) sym = is_double ? "/d" : "/";
                snprintf(ln, sizeof(ln), "\t(a %s %s a :=)", sym, var_names[oi]);
                func_append(f, ln);
            }
        } else {
            // fallback: use boolean flags (unordered)
            int lit_idx = 0;
            const char *order[] = {"add", "sub", "mul", "div"};
            const char *sym_add = is_double ? "+d" : "+";
            const char *sym_sub = is_double ? "-d" : "-";
            const char *sym_mul = is_double ? "*d" : "*";
            const char *sym_div = is_double ? "/d" : "/";
            for (int oi = 0; oi < 4 && lit_idx < num_vars; oi++) {
                int has_op = 0;
                if (strcmp(order[oi], "add") == 0) has_op = task->has_add;
                else if (strcmp(order[oi], "sub") == 0) has_op = task->has_sub;
                else if (strcmp(order[oi], "mul") == 0) has_op = task->has_mul;
                else if (strcmp(order[oi], "div") == 0) has_op = task->has_div;
                if (has_op) {
                    const char *sym = sym_add;
                    if (strcmp(order[oi], "sub") == 0) sym = sym_sub;
                    else if (strcmp(order[oi], "mul") == 0) sym = sym_mul;
                    else if (strcmp(order[oi], "div") == 0) sym = sym_div;
                    snprintf(ln, sizeof(ln), "\t(a %s %s a :=)", sym, var_names[lit_idx++]);
                    func_append(f, ln);
                }
            }
        }
        if (task->has_even_odd && !is_double) {
            func_append(f, "\t(a % two mod :=)");
        }
        snprintf(ln, sizeof(ln), "\t(a %s", print_op);
        func_append(f, ln);
        func_append(f, "\t(:print_n !)");
        if (task->has_even_odd && !is_double) {
            func_append(f, "\t(((mod zero ==) f :=) f if+)");
            func_append(f, "\t\t(even_str :print_s !)");
            func_append(f, "\t(else)");
            func_append(f, "\t\t(odd_str :print_s !)");
            func_append(f, "\t(endif)");
            func_append(f, "\t(:print_n !)");
        }
        emitted = 1;
    }
    if (!emitted && task->has_operation && task->has_input) {
        int c = task->input_count > 0 ? task->input_count : 5;
        emit_input_loop(prog, f, c, task->type, task->op);
        emitted = 1;
    }
    if (!emitted && task->has_operation && task->has_literals && !task->has_input) {
        int n = task->num_literals > 2 ? 3 : (task->num_literals < 2 ? 2 : task->num_literals);
        int vals[3] = {10, 3, 5};
        if (task->num_literals >= 1) vals[0] = task->literals[0];
        if (task->num_literals >= 2) vals[1] = task->literals[1];
        if (task->num_literals >= 3) vals[2] = task->literals[2];
        emit_math(prog, f, task->type, task->op, vals, n, 1);
        emitted = 1;
    }
    if (task->has_average && task->has_input) {
        emit_average(prog, f, task->skip_input);
        emitted = 1;
    }
    if (task->has_sort && !task->has_input) {
        int c = task->num_literals > 0 ? task->literals[0] : 5;
        emit_selection_sort(prog, f, c, task->skip_input);
        emitted = 1;
    }
    if (task->has_fann_create && task->has_fann_train) {
        emit_fann_train(prog, f);
        emitted = 1;
    }
    if (task->has_fann_create && !task->has_fann_train) {
        emit_fann_create(prog, f);
        emitted = 1;
    }
    DISPATCH(has_fann_run, emit_fann_run);
    if (!emitted && task->has_input && !task->has_operation) {
        int c = task->input_count > 0 ? task->input_count : 5;
        emit_input_loop(prog, f, c, task->type, NULL);
        emitted = 1;
    }
    if (task->has_bubble_sort) {
        emit_bubble_sort(prog, f, task->skip_input);
        emitted = 1;
    }
    if (task->has_standard_deviation) {
        emit_standard_deviation(prog, f, task->skip_input);
        emitted = 1;
    }
    if (task->has_double_math) {
        emit_double_math(prog, f, task->op);
        emitted = 1;
    }
    if (task->has_double_average) {
        emit_double_average(prog, f, task->skip_input);
        emitted = 1;
    }
    if (task->has_insertion_sort) {
        int c = task->num_literals > 0 ? task->literals[0] : 5;
        emit_insertion_sort(prog, f, c, task->skip_input);
        emitted = 1;
    }
    // If we emitted code, also run any LLM-selected extra emitters in sequence
    if (emitted || task->num_extra_emitters > 0) {
        // Run extra emitters (LLM-selected secondary operations for the same step)
        if (task->num_extra_emitters > 0) {
            for (int ei = 0; ei < task->num_extra_emitters && ei < 32; ei++) {
                int idx = task->extra_emitters[ei];
                // Dispatch by index directly via function pointer table
                if (idx >= 0 && idx < NUM_EMITTERS && ee_table[idx])
                    ee_table[idx](prog, f, task);
            }
        }
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
        snprintf(cvs, sizeof(cvs), "%d", task->inherit_count > 0 ? task->inherit_count : 5);
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
        snprintf(ln, sizeof(ln), "\t\t(%s [ realind ] a =)", task->inherit_var);
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
        snprintf(cvs, sizeof(cvs), "%d", task->inherit_count > 0 ? task->inherit_count : 5);
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
        snprintf(ln, sizeof(ln), "\t(%s [ realind ] a =)", task->inherit_var);
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
        snprintf(ln, sizeof(ln), "\t(zero * int64_size realind :=)");
        func_append(f, ln);
        func_append(f, "\t// print smallest");
        snprintf(ln, sizeof(ln), "\t(%s [ realind ] a =)", task->inherit_var);
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
        snprintf(vname, sizeof(vname), "%s_%c", type, 'a' + i);
        snprintf(vstr, sizeof(vstr), "%d", vals[i]);
        const char *vv[] = {vstr};
        add_var_to_func(f, type, vname, 1, vv, 1);
    }

    char rname[64];
    snprintf(rname, sizeof(rname), "%s_r", type);
    add_var_to_func(f, type, rname, 1, r_v, 1);

    if (num_vals == 2) {
        snprintf(line, sizeof(line), "\t(%s_%c %s %s_%c %s :=)", type, 'a', sym, type, 'b', rname);
    } else {
        snprintf(line, sizeof(line), "\t((%s_%c %s %s_%c) %s %s_%c %s :=)", type, 'a', sym, type, 'b', sym, type, 'c', rname);
    }
    func_append(f, line);

    func_append(f, "\t// Ergebnis ausgeben:");
    snprintf(line, sizeof(line), "\t(%s :print_i !)", rname);
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
                snprintf(desc, desc_size, "learned pattern: %s (score: %d)", learned_patterns[lidx].id, lscore);
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
                if (parse_task(steps[i], &task)) {
                    task.skip_input = (i > 0);
                    task.suppress_output = (i < num_steps - 1);
                    dataflow_quiet_mode = task.suppress_output;
                    // populate inherited vars from previous steps
                    for (int iv = 0; iv < num_inherited; iv++) {
                        snprintf(task.inherit_var_names[task.num_inherit_vars], sizeof(task.inherit_var_names[task.num_inherit_vars]), "%.*s", (int)sizeof(task.inherit_var_names[task.num_inherit_vars]) - 1, inherited_names[iv]);
                        snprintf(task.inherit_var_types[task.num_inherit_vars], sizeof(task.inherit_var_types[task.num_inherit_vars]), "%.*s", (int)sizeof(task.inherit_var_types[task.num_inherit_vars]) - 1, inherited_types[iv]);
                        task.inherit_var_counts[task.num_inherit_vars] = inherited_counts[iv];
                        task.num_inherit_vars++;
                    }
                    // backward compat: first array var
                    if (i > 0) {
                        for (int iv = 0; iv < num_inherited; iv++) {
                            if (inherited_counts[iv] > 1 && !task.inherit_var[0]) {
                                snprintf(task.inherit_var, sizeof(task.inherit_var), "%s", inherited_names[iv]);
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
            }
            // collect inherited variables from the main function for next steps
            // Now also collects scalar variables (not just arrays) for data flow
            Function *fcur = NULL;
            for (int fi = 0; fi < prog->num_funcs; fi++)
                if (strcmp(prog->funcs[fi].name, "main") == 0) { fcur = &prog->funcs[fi]; break; }
            if (fcur) {
                for (int vi = 0; vi < fcur->num_vars && num_inherited < 64; vi++) {
                    int is_const = (strstr(fcur->vars[vi].type, "const") != NULL);
                    int is_standard = (strcmp(fcur->vars[vi].name, "zero") == 0 || strcmp(fcur->vars[vi].name, "one") == 0
                        || strcmp(fcur->vars[vi].name, "two") == 0 || strcmp(fcur->vars[vi].name, "three") == 0
                        || strcmp(fcur->vars[vi].name, "four") == 0 || strcmp(fcur->vars[vi].name, "five") == 0);
                    int is_loop_var = (strcmp(fcur->vars[vi].name, "i") == 0 || strcmp(fcur->vars[vi].name, "j") == 0
                        || strcmp(fcur->vars[vi].name, "f") == 0 || strcmp(fcur->vars[vi].name, "temp") == 0
                        || strcmp(fcur->vars[vi].name, "realind") == 0 || strcmp(fcur->vars[vi].name, "realind2") == 0
                        || strcmp(fcur->vars[vi].name, "a") == 0 || strcmp(fcur->vars[vi].name, "b") == 0
                        || strcmp(fcur->vars[vi].name, "min_idx") == 0 || strcmp(fcur->vars[vi].name, "modtmp") == 0
                        || strcmp(fcur->vars[vi].name, "is_prime") == 0 || strcmp(fcur->vars[vi].name, "mid") == 0
                        || strcmp(fcur->vars[vi].name, "num") == 0 || strcmp(fcur->vars[vi].name, "aux") == 0);
                    // Skip standard constants, loop variables, but keep arrays AND scalars that look like result variables
                    if (!is_const && !is_standard && !is_loop_var) {
                        int already = 0;
                        for (int ck = 0; ck < num_inherited; ck++) {
                            if (strcmp(inherited_names[ck], fcur->vars[vi].name) == 0) already = 1;
                        }
                        if (!already) {
                            snprintf(inherited_names[num_inherited], sizeof(inherited_names[num_inherited]), "%s", fcur->vars[vi].name);
                            snprintf(inherited_types[num_inherited], sizeof(inherited_types[num_inherited]), "%s", fcur->vars[vi].type);
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
        snprintf(desc, desc_size, "multi-step generation (%d steps)", num_steps);
        return 1;
    }

    // Single-step: try learned pattern match first
    {
        int lscore;
        int lidx = match_learned_pattern(prompt, &lscore);
        if (lidx >= 0) {
            if (emit_learned_pattern(prog, lidx)) {
                snprintf(desc, desc_size, "learned pattern: %s (score: %d)", learned_patterns[lidx].id, lscore);
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
            snprintf(desc, desc_size, "llm plan: %s", task.title);
            return 1;
        }
    }

    // Fallback: old arithmetic generator for simple math prompts
    char buf[MAX_PROMPT];
    snprintf(buf, sizeof(buf), "%s", prompt);
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
        snprintf(desc, desc_size, "%s of three %s numbers (%d %s %d %s %d)", op_name, type, avals[0], sym, avals[1], sym, avals[2]);
    else
        snprintf(desc, desc_size, "%s of two %s numbers (%d %s %d)", op_name, type, avals[0], sym, avals[1]);
    return 1;
}

void write_program(Program *prog, const char *filename) {
    FILE *f = NULL;
    if (dry_run_flag) {
        f = stdout;
    } else {
        f = fopen(filename, "w");
        if (!f) { fprintf(stderr, "Error: cannot write %s\n", filename); return; }
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
        for (int vi = 0; vi < fn->num_vars; vi++) {
            Variable *v = &fn->vars[vi];
            fprintf(f, "\t(set %s %d %s", v->type, v->count, v->name);
            for (int vj = 0; vj < v->num_values; vj++)
                fprintf(f, " %s", v->values[vj]);
            fprintf(f, ")\n");
        }
        {
            // Filter duplicate (set ...) lines from body that match fn->vars[] declarations
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
                            for (int vi = 0; vi < fn->num_vars; vi++) {
                                if (strcmp(fn->vars[vi].name, vn) == 0) {
                                    // Build canonical declaration for this variable
                                    char canon[4096];
                                    int cpos = snprintf(canon, sizeof(canon), "(set %s %d %s",
                                        fn->vars[vi].type, fn->vars[vi].count, fn->vars[vi].name);
                                    for (int vj = 0; vj < fn->vars[vi].num_values; vj++)
                                        cpos += snprintf(canon + cpos, sizeof(canon) - cpos, " %s", fn->vars[vi].values[vj]);
                                    snprintf(canon + cpos, sizeof(canon) - cpos, ")");
                                    if (strcmp(p, canon) == 0)
                                        { skip = 1; break; }
                                }
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

void gen_hello_world(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    func_append(f, "\t(set const-string s hello \"Hello world!\")");
    func_append(f, "\t(hello :print_s !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");
}

void gen_print_var(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "int64", "x", 1, (const char *[]){"42"}, 1);
    func_append(f, "\t(x :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");
}

void gen_math_ops(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "const-int64", "x", 1, (const char *[]){"23"}, 1);
    add_var_to_func(f, "const-int64", "y", 1, (const char *[]){"42"}, 1);
    add_var_to_func(f, "int64", "ret", 1, (const char *[]){"0"}, 1);
    func_append(f, "\t// ret = x + y and ret = x * y");
    func_append(f, "\t(x + y ret :=)");
    func_append(f, "\t(ret :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(x * y ret :=)");
    func_append(f, "\t(ret :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");
}

void gen_for_loop(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    const char *vals1[] = {"1"};
    const char *vals10[] = {"10"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "const-int64", "one", 1, vals1, 1);
    add_var_to_func(f, "int64", "i", 1, vals, 1);
    add_var_to_func(f, "int64", "max", 1, vals10, 1);
    add_var_to_func(f, "int64", "f", 1, vals, 1);
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i max <) f :=) f for)");
    func_append(f, "\t\t(i :print_i !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(zero :exit !)");
}

void gen_while_loop(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    const char *vals1[] = {"1"};
    const char *vals10[] = {"10"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "int64", "one", 1, vals1, 1);
    add_var_to_func(f, "int64", "i", 1, vals10, 1);
    add_var_to_func(f, "int64", "f", 1, vals, 1);
    func_append(f, "\t(do)");
    func_append(f, "\t\t(i :print_i !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(i - one i :=)");
    func_append(f, "\t(((i zero >) f :=) f while)");
    func_append(f, "\t(zero :exit !)");
}

void gen_if_else(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    const char *vals7[] = {"7"};
    const char *vals10[] = {"10"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "int64", "x", 1, vals7, 1);
    add_var_to_func(f, "int64", "y", 1, vals10, 1);
    add_var_to_func(f, "int64", "f", 1, vals, 1);
    const char *vless[] = {"\"x < y\""};
    add_var_to_func(f, "const-string", "less_str", 6, vless, 1);
    const char *vmore[] = {"\"x >= y\""};
    add_var_to_func(f, "const-string", "more_str", 7, vmore, 1);
    func_append(f, "\t(((x y <) f :=) f if)");
    func_append(f, "\t\t(less_str :print_s !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(((x y >=) f :=) f if)");
    func_append(f, "\t\t(more_str :print_s !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(zero :exit !)");
}

void gen_if_else_chain(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "const-int64", "one", 1, (const char *[]){"1"}, 1);
    add_var_to_func(f, "int64", "x", 1, (const char *[]){"5"}, 1);
    add_var_to_func(f, "int64", "f", 1, vals, 1);
    const char *vpos[] = {"\"positive\""};
    add_var_to_func(f, "const-string", "pos_str", 10, vpos, 1);
    const char *vneg[] = {"\"negative\""};
    add_var_to_func(f, "const-string", "neg_str", 10, vneg, 1);
    const char *vzero[] = {"\"zero\""};
    add_var_to_func(f, "const-string", "zero_str", 6, vzero, 1);
    func_append(f, "\t(((x zero >) f :=) f if+)");
    func_append(f, "\t\t(pos_str :print_s !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t(else)");
    func_append(f, "\t\t(((x zero <) f :=) f if+)");
    func_append(f, "\t\t\t(neg_str :print_s !)");
    func_append(f, "\t\t\t(:print_n !)");
    func_append(f, "\t\t(else)");
    func_append(f, "\t\t\t(zero_str :print_s !)");
    func_append(f, "\t\t\t(:print_n !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(zero :exit !)");
}

void gen_switch(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    const char *vals23[] = {"23"};
    const char *vals42[] = {"42"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "int64", "y", 1, vals23, 1);
    add_var_to_func(f, "const-int64", "c23", 1, vals23, 1);
    add_var_to_func(f, "const-int64", "c42", 1, vals42, 1);
    const char *v23[] = {"\"y = 23\""};
    add_var_to_func(f, "const-string", "str23", 7, v23, 1);
    const char *v42[] = {"\"y = 42\""};
    add_var_to_func(f, "const-string", "str42", 7, v42, 1);
    func_append(f, "\t(switch)");
    func_append(f, "\t(y c23 ?)");
    func_append(f, "\t\t(str23 :print_s !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t(break)");
    func_append(f, "\t(y c42 ?)");
    func_append(f, "\t\t(str42 :print_s !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t(break)");
    func_append(f, "\t(switchend)");
    func_append(f, "\t(zero :exit !)");
}

void gen_factorial(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    const char *vals5[] = {"5"};
    f->is_local = 1;
    func_vardef(f, "main");
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "const-int64", "num~", 1, vals5, 1);
    add_var_to_func(f, "int64", "fac~", 1, vals, 1);
    func_append(f, "\t(num~ :factorial !)");
    func_append(f, "\t(fac~ stpop)");
    func_append(f, "\t(fac~ :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");

    add_func(prog, "factorial");
    Function *ff = &prog->funcs[prog->num_funcs - 1];
    ff->is_local = 1;
    func_vardef(ff, "factorial");
    add_var_to_func(ff, "const-int64", "zero~", 1, vals, 1);
    add_var_to_func(ff, "const-int64", "one~", 1, (const char *[]){"1"}, 1);
    add_var_to_func(ff, "int64", "n~", 1, vals, 1);
    add_var_to_func(ff, "int64", "i~", 1, vals, 1);
    add_var_to_func(ff, "int64", "fac~", 1, (const char *[]){"1"}, 1);
    add_var_to_func(ff, "int64", "f~", 1, vals, 1);
    func_append(ff, "\t(n~ stpop)");
    func_append(ff, "\t(n~ i~ :=)");
    func_append(ff, "\t(for-loop)");
    func_append(ff, "\t(((i~ zero~ >) f~ :=) f~ for)");
    func_append(ff, "\t\t(fac~ i~ * fac~ :=)");
    func_append(ff, "\t\t(i~ one~ - i~ :=)");
    func_append(ff, "\t(next)");
    func_append(ff, "\t(fac~ stpush)");
    func_append(ff, "\t(return)");
}

void gen_fibonacci(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    const char *vals10[] = {"10"};
    f->is_local = 1;
    func_vardef(f, "main");
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "const-int64", "n~", 1, vals10, 1);
    add_var_to_func(f, "int64", "r~", 1, vals, 1);
    func_append(f, "\t(n~ :fibonacci !)");
    func_append(f, "\t(r~ stpop)");
    func_append(f, "\t(r~ :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");

    add_func(prog, "fibonacci");
    Function *ff = &prog->funcs[prog->num_funcs - 1];
    ff->is_local = 1;
    func_vardef(ff, "fibonacci");
    add_var_to_func(ff, "const-int64", "zero~", 1, vals, 1);
    add_var_to_func(ff, "const-int64", "one~", 1, (const char *[]){"1"}, 1);
    add_var_to_func(ff, "int64", "n~", 1, vals, 1);
    add_var_to_func(ff, "int64", "a~", 1, vals, 1);
    add_var_to_func(ff, "int64", "b~", 1, (const char *[]){"1"}, 1);
    add_var_to_func(ff, "int64", "i~", 1, (const char *[]){"2"}, 1);
    add_var_to_func(ff, "int64", "c~", 1, vals, 1);
    add_var_to_func(ff, "int64", "f~", 1, vals, 1);
    func_append(ff, "\t(n~ stpop)");
    func_append(ff, "\t(for-loop)");
    func_append(ff, "\t(((i~ n~ <=) f~ :=) f~ for)");
    func_append(ff, "\t\t(a~ b~ + c~ :=)");
    func_append(ff, "\t\t(b~ a~ :=)");
    func_append(ff, "\t\t(c~ b~ :=)");
    func_append(ff, "\t\t(i~ one~ + i~ :=)");
    func_append(ff, "\t(next)");
    func_append(ff, "\t(b~ stpush)");
    func_append(ff, "\t(return)");
}

void gen_fizzbuzz(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    const char *vals1[] = {"1"};
    const char *vals3[] = {"3"};
    const char *vals5[] = {"5"};
    const char *vals100[] = {"100"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "const-int64", "one", 1, vals1, 1);
    add_var_to_func(f, "const-int64", "fizz", 1, vals3, 1);
    add_var_to_func(f, "const-int64", "buzz", 1, vals5, 1);
    const char *vfizz[] = {"\"fizz\""};
    add_var_to_func(f, "const-string", "fizzstr", 6, vfizz, 1);
    const char *vbuzz[] = {"\"buzz\""};
    add_var_to_func(f, "const-string", "buzzstr", 6, vbuzz, 1);
    add_var_to_func(f, "const-int64", "max", 1, vals100, 1);
    add_var_to_func(f, "int64", "i", 1, vals1, 1);
    add_var_to_func(f, "int64", "f", 1, vals, 1);
    add_var_to_func(f, "int64", "modfizz", 1, vals, 1);
    add_var_to_func(f, "int64", "modbuzz", 1, vals, 1);
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i max <=) f :=) f for)");
    func_append(f, "\t\t(i % fizz modfizz :=)");
    func_append(f, "\t\t(i % buzz modbuzz :=)");
    func_append(f, "\t\t((modfizz zero ==) && (modbuzz zero ==) f :=)");
    func_append(f, "\t\t(f if+)");
    func_append(f, "\t\t\t(fizzstr :print_s !)");
    func_append(f, "\t\t\t(buzzstr :print_s !)");
    func_append(f, "\t\t\t(:print_n !)");
    func_append(f, "\t\t\t(:skip jmp)");
    func_append(f, "\t\t(else)");
    func_append(f, "\t\t\t(((modfizz zero ==) f :=) f if)");
    func_append(f, "\t\t\t\t(fizzstr :print_s !)");
    func_append(f, "\t\t\t\t(:print_n !)");
    func_append(f, "\t\t\t\t(:skip jmp)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t\t(((modbuzz zero ==) f :=) f if)");
    func_append(f, "\t\t\t\t(buzzstr :print_s !)");
    func_append(f, "\t\t\t\t(:print_n !)");
    func_append(f, "\t\t\t\t(:skip jmp)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t\t(i :print_i !)");
    func_append(f, "\t\t\t(:print_n !)");
    func_append(f, "\t\t(:skip)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t(next)");
    func_append(f, "\t(zero :exit !)");
}

void gen_array_demo(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    const char *vals1[] = {"1"};
    const char *vals5[] = {"5"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "int64", "one", 1, vals1, 1);
    add_var_to_func(f, "int64", "n", 1, vals5, 1);
    const char *arrvals[] = {"10", "5", "8", "4", "3"};
    add_var_to_func(f, "int64", "arr", 5, arrvals, 5);
    add_var_to_func(f, "int64", "i", 1, vals, 1);
    add_var_to_func(f, "int64", "realind", 1, vals, 1);
    add_var_to_func(f, "int64", "num", 1, vals, 1);
    add_var_to_func(f, "int64", "f", 1, vals, 1);
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i n <) f :=) f for)");
    func_append(f, "\t\t(i * int64_size realind :=)");
    func_append(f, "\t\t(arr [ realind ] num =)");
    func_append(f, "\t\t(num + zero num :=)");
    func_append(f, "\t\t(num :print_i !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(zero :exit !)");
}

void gen_string_demo(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "int64", "strmod", 1, vals, 1);
    const char *vhello[] = {"\"Hello \""};
    add_var_to_func(f, "string", "greeting", 8, vhello, 1);
    const char *vname[] = {"\"Brackets\""};
    add_var_to_func(f, "string", "name", 10, vname, 1);
    add_var_to_func(f, "string", "result", 256, (const char *[]){"\"\""}, 1);
    func_append(f, "\t(strmod :string_init !)");
    func_append(f, "\t(greeting :print_s !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(result greeting :string_copy !)");
    func_append(f, "\t(result name :string_cat !)");
    func_append(f, "\t(result :print_s !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");
    add_include_post(prog, "string.l1h");
}

void gen_user_input(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "int64", "x", 1, vals, 1);
    const char *vprompt[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "prompt", 18, vprompt, 1);
    func_append(f, "\t(prompt :print_s !)");
    func_append(f, "\t(x :input_i !)");
    func_append(f, "\t(x :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");
}

void gen_shell_args(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "int64", "one", 1, (const char *[]){"1"}, 1);
    add_var_to_func(f, "int64", "args", 1, vals, 1);
    add_var_to_func(f, "string", "shell_arg", 256, (const char *[]){"\"\""}, 1);
    add_var_to_func(f, "int64", "i", 1, vals, 1);
    add_var_to_func(f, "int64", "f", 1, vals, 1);
    const char *vargstr[] = {"\" shell arguments:\""};
    add_var_to_func(f, "const-string", "arg_str", 19, vargstr, 1);
    func_append(f, "\t(args :shell_args !)");
    func_append(f, "\t(args :print_i !)");
    func_append(f, "\t(arg_str :print_s !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(:loop)");
    func_append(f, "\t(i shell_arg :get_shell_arg !)");
    func_append(f, "\t(shell_arg :print_s !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(i + one i :=)");
    func_append(f, "\t(((i args <) f :=) f if)");
    func_append(f, "\t\t(:loop jmp)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(zero :exit !)");
}

void gen_countdown(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    const char *vals1[] = {"1"};
    const char *vals10[] = {"10"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "int64", "one", 1, vals1, 1);
    add_var_to_func(f, "int64", "i", 1, vals10, 1);
    add_var_to_func(f, "int64", "f", 1, vals, 1);
    func_append(f, "\t(do)");
    func_append(f, "\t\t(i :print_i !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(i - one i :=)");
    func_append(f, "\t(((i zero >) f :=) f while)");
    func_append(f, "\t(zero :exit !)");
}

void gen_sum(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    const char *vals1[] = {"1"};
    const char *vals100[] = {"100"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "int64", "one", 1, vals1, 1);
    add_var_to_func(f, "int64", "n", 1, vals100, 1);
    add_var_to_func(f, "int64", "sum", 1, vals, 1);
    add_var_to_func(f, "int64", "i", 1, vals, 1);
    add_var_to_func(f, "int64", "f", 1, vals, 1);
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i n <=) f :=) f for)");
    func_append(f, "\t\t(sum + i sum :=)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(sum :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");
}

void gen_primes(Program *prog, const char *prompt) {
    int nums[4];
    int num_count = extract_numbers(prompt, nums, 4);
    int max_val = 50;
    if (num_count > 0) max_val = nums[0];
    char max_str[16];
    snprintf(max_str, sizeof(max_str), "%d", max_val);
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    const char *vals1[] = {"1"};
    const char *vals2[] = {"2"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "int64", "one", 1, vals1, 1);
    add_var_to_func(f, "int64", "two", 1, vals2, 1);
    add_var_to_func(f, "int64", "num", 1, vals2, 1);
    add_var_to_func(f, "int64", "max", 1, (const char *[]){max_str}, 1);
    add_var_to_func(f, "int64", "f", 1, vals, 1);
    add_var_to_func(f, "int64", "i", 1, vals, 1);
    add_var_to_func(f, "int64", "is_prime", 1, vals, 1);
    add_var_to_func(f, "int64", "modtmp", 1, vals, 1);
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((num max <=) f :=) f for)");
    func_append(f, "\t\t(two i :=)");
    func_append(f, "\t\t(one is_prime :=)");
    func_append(f, "\t\t(for-loop)");
    func_append(f, "\t\t(((i num <) f :=) f for)");
    func_append(f, "\t\t\t(num i % modtmp :=)");
    func_append(f, "\t\t\t(((modtmp zero ==) f :=) f if)");
    func_append(f, "\t\t\t\t(zero is_prime :=)");
    func_append(f, "\t\t\t\t(:break jmp)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t\t(i + one i :=)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t(:break)");
    func_append(f, "\t\t(is_prime if)");
    func_append(f, "\t\t\t(num :print_i !)");
    func_append(f, "\t\t\t(:print_n !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(num + one num :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(zero :exit !)");
}

void gen_function_demo(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "int64", "x", 1, (const char *[]){"23"}, 1);
    add_var_to_func(f, "int64", "y", 1, (const char *[]){"42"}, 1);
    add_var_to_func(f, "int64", "a", 1, vals, 1);
    add_var_to_func(f, "int64", "sq", 1, vals, 1);
    const char *vhello[] = {"\"Hello world!\""};
    add_var_to_func(f, "const-string", "hello", 14, vhello, 1);
    func_append(f, "\t(hello :print_s !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(x * y a :=)");
    func_append(f, "\t(a :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(y sq :square !)");
    func_append(f, "\t(sq :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");

    add_func(prog, "square");
    Function *sf = &prog->funcs[prog->num_funcs - 1];
    sf->is_local = 1;
    func_vardef(sf, "square");
    add_var_to_func(sf, "const-int64", "zero~", 1, vals, 1);
    add_var_to_func(sf, "int64", "n~", 1, vals, 1);
    add_var_to_func(sf, "int64", "r~", 1, vals, 1);
    func_append(sf, "\t(n~ stpop)");
    func_append(sf, "\t(n~ n~ * r~ :=)");
    func_append(sf, "\t(r~ stpush)");
    func_append(sf, "\t(return)");
}

void gen_guess_number(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    add_var_to_func(f, "int64", "zero", 1, vals, 1);
    add_var_to_func(f, "int64", "secret", 1, (const char *[]){"42"}, 1);
    add_var_to_func(f, "int64", "guess", 1, vals, 1);
    add_var_to_func(f, "int64", "f", 1, vals, 1);
    const char *vtxt[] = {"\"Guess a number: \""};
    add_var_to_func(f, "const-string", "prompt_str", 18, vtxt, 1);
    const char *vlow[] = {"\"Too low!\""};
    add_var_to_func(f, "const-string", "low_str", 10, vlow, 1);
    const char *vhigh[] = {"\"Too high!\""};
    add_var_to_func(f, "const-string", "high_str", 11, vhigh, 1);
    const char *vwin[] = {"\"Correct!\""};
    add_var_to_func(f, "const-string", "win_str", 9, vwin, 1);
    func_append(f, "\t(do)");
    func_append(f, "\t\t(prompt_str :print_s !)");
    func_append(f, "\t\t(guess :input_i !)");
    func_append(f, "\t\t(((guess secret <) f :=) f if)");
    func_append(f, "\t\t\t(low_str :print_s !)");
    func_append(f, "\t\t\t(:print_n !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((guess secret >) f :=) f if)");
    func_append(f, "\t\t\t(high_str :print_s !)");
    func_append(f, "\t\t\t(:print_n !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t(((guess secret ==) f :=) f while)");
    func_append(f, "\t(win_str :print_s !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");
}

void gen_multiplication_table(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    const char *vals1[] = {"1"};
    const char *vals10[] = {"10"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "int64", "one", 1, vals1, 1);
    add_var_to_func(f, "int64", "i", 1, vals, 1);
    add_var_to_func(f, "int64", "j", 1, vals, 1);
    add_var_to_func(f, "int64", "mx", 1, vals10, 1);
    add_var_to_func(f, "int64", "f", 1, vals, 1);
    add_var_to_func(f, "int64", "p", 1, vals, 1);
    add_var_to_func(f, "string", "row", 512, (const char *[]){"\"\""}, 1);
    add_var_to_func(f, "string", "ns", 32, (const char *[]){"\"\""}, 1);
    add_var_to_func(f, "string", "sp", 2, (const char *[]){"\" \""}, 1);
    add_var_to_func(f, "string", "empty", 2, (const char *[]){"\"\""}, 1);
    add_var_to_func(f, "int64", "strmod", 1, vals, 1);
    const char *nsv[] = {"32"};
    add_var_to_func(f, "const-int64", "ns_len", 1, nsv, 1);
    func_append(f, "\t(strmod :string_init !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(one i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i mx <=) f :=) f for)");
    func_append(f, "\t\t(empty row :string_copy !)");
    func_append(f, "\t\t(one j :=)");
    func_append(f, "\t\t(for-loop)");
    func_append(f, "\t\t(((j mx <=) f :=) f for)");
    func_append(f, "\t\t\t(i * j p :=)");
    func_append(f, "\t\t\t(p ns ns_len :string_int64tostring !)");
    func_append(f, "\t\t\t(ns row :string_cat !)");
    func_append(f, "\t\t\t(sp row :string_cat !)");
    func_append(f, "\t\t\t(j + one j :=)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t(row :print_s !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(zero :exit !)");
    add_include_post(prog, "string.l1h");
}

void gen_even_odd(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    const char *vals1[] = {"1"};
    const char *vals2[] = {"2"};
    const char *vals20[] = {"20"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "int64", "one", 1, vals1, 1);
    add_var_to_func(f, "int64", "two", 1, vals2, 1);
    add_var_to_func(f, "int64", "i", 1, vals, 1);
    add_var_to_func(f, "int64", "max", 1, vals20, 1);
    add_var_to_func(f, "int64", "f", 1, vals, 1);
    add_var_to_func(f, "int64", "mod", 1, vals, 1);
    const char *veven[] = {"\" is even\""};
    add_var_to_func(f, "const-string", "even_str", 10, veven, 1);
    const char *vodd[] = {"\" is odd\""};
    add_var_to_func(f, "const-string", "odd_str", 9, vodd, 1);
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i max <=) f :=) f for)");
    func_append(f, "\t\t(i % two mod :=)");
    func_append(f, "\t\t(i :print_i !)");
    func_append(f, "\t\t(((mod zero ==) f :=) f if+)");
    func_append(f, "\t\t\t(even_str :print_s !)");
    func_append(f, "\t\t(else)");
    func_append(f, "\t\t\t(odd_str :print_s !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(zero :exit !)");
}

void gen_power(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "int64", "base", 1, (const char *[]){"2"}, 1);
    add_var_to_func(f, "int64", "exp", 1, (const char *[]){"10"}, 1);
    add_var_to_func(f, "int64", "ret", 1, vals, 1);
    func_append(f, "\t(base exp :power !)");
    func_append(f, "\t(ret stpop)");
    func_append(f, "\t(ret :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");

    add_func(prog, "power");
    Function *pf = &prog->funcs[prog->num_funcs - 1];
    pf->is_local = 1;
    func_vardef(pf, "power");
    add_var_to_func(pf, "const-int64", "zero~", 1, vals, 1);
    add_var_to_func(pf, "const-int64", "one~", 1, (const char *[]){"1"}, 1);
    add_var_to_func(pf, "int64", "b~", 1, vals, 1);
    add_var_to_func(pf, "int64", "e~", 1, vals, 1);
    add_var_to_func(pf, "int64", "r~", 1, (const char *[]){"1"}, 1);
    add_var_to_func(pf, "int64", "f~", 1, vals, 1);
    func_append(pf, "\t(e~ stpop)");
    func_append(pf, "\t(b~ stpop)");
    func_append(pf, "\t(for-loop)");
    func_append(pf, "\t(((e~ zero~ >) f~ :=) f~ for)");
    func_append(pf, "\t\t(r~ b~ * r~ :=)");
    func_append(pf, "\t\t(e~ one~ - e~ :=)");
    func_append(pf, "\t(next)");
    func_append(pf, "\t(r~ stpush)");
    func_append(pf, "\t(return)");
}

void gen_max_of_three(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "int64", "a", 1, (const char *[]){"17"}, 1);
    add_var_to_func(f, "int64", "b", 1, (const char *[]){"42"}, 1);
    add_var_to_func(f, "int64", "c", 1, (const char *[]){"8"}, 1);
    add_var_to_func(f, "int64", "mx", 1, vals, 1);
    add_var_to_func(f, "int64", "f", 1, vals, 1);
    func_append(f, "\t(a b :max !)");
    func_append(f, "\t(mx stpop)");
    func_append(f, "\t(mx c :max !)");
    func_append(f, "\t(mx stpop)");
    func_append(f, "\t(mx :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");

    add_func(prog, "max");
    Function *mf = &prog->funcs[prog->num_funcs - 1];
    mf->is_local = 1;
    func_vardef(mf, "max");
    add_var_to_func(mf, "const-int64", "zero~", 1, vals, 1);
    add_var_to_func(mf, "int64", "x~", 1, vals, 1);
    add_var_to_func(mf, "int64", "y~", 1, vals, 1);
    add_var_to_func(mf, "int64", "f~", 1, vals, 1);
    func_append(mf, "\t(y~ stpop)");
    func_append(mf, "\t(x~ stpop)");
    func_append(mf, "\t(((x~ y~ <) f~ :=) f~ if+)");
    func_append(mf, "\t\t(y~ stpush)");
    func_append(mf, "\t(else)");
    func_append(mf, "\t\t(x~ stpush)");
    func_append(mf, "\t(endif)");
    func_append(mf, "\t(return)");
}

void gen_hello_name(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "const-int64", "maxlen", 1, (const char *[]){"256"}, 1);
    add_var_to_func(f, "string", "name", 256, (const char *[]){"\"\""}, 1);
    const char *vask[] = {"\"What is your name? \""};
    add_var_to_func(f, "const-string", "ask_str", 22, vask, 1);
    const char *vhello[] = {"\"Hello, \""};
    add_var_to_func(f, "const-string", "hello_str", 8, vhello, 1);
    func_append(f, "\t(ask_str :print_s !)");
    func_append(f, "\t(maxlen name :input_s !)");
    func_append(f, "\t(hello_str :print_s !)");
    func_append(f, "\t(name :print_s !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");
}

void gen_bubble_sort(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    const char *vals1[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "int64", "one", 1, vals1, 1);
    add_var_to_func(f, "int64", "n", 1, (const char *[]){"6"}, 1);
    const char *arrvals[] = {"5", "2", "9", "1", "7", "3"};
    add_var_to_func(f, "int64", "arr", 6, arrvals, 6);
    add_var_to_func(f, "int64", "i", 1, vals, 1);
    add_var_to_func(f, "int64", "j", 1, vals, 1);
    add_var_to_func(f, "int64", "temp", 1, vals, 1);
    add_var_to_func(f, "int64", "realind", 1, vals, 1);
    add_var_to_func(f, "int64", "realind2", 1, vals, 1);
    add_var_to_func(f, "int64", "a", 1, vals, 1);
    add_var_to_func(f, "int64", "b", 1, vals, 1);
    add_var_to_func(f, "int64", "f", 1, vals, 1);
    func_append(f, "\t// Bubble sort with array pointer access");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i n <) f :=) f for)");
    func_append(f, "\t\t(zero j :=)");
    func_append(f, "\t\t(n one - temp :=)");
    func_append(f, "\t\t(for-loop)");
    func_append(f, "\t\t(((j temp <) f :=) f for)");
    func_append(f, "\t\t\t(j * int64_size realind :=)");
    func_append(f, "\t\t\t((j + one) * int64_size realind2 :=)");
    func_append(f, "\t\t\t(arr [ realind ] a =)");
    func_append(f, "\t\t\t(a + zero a :=)");
    func_append(f, "\t\t\t(arr [ realind2 ] b =)");
    func_append(f, "\t\t\t(b + zero b :=)");
    func_append(f, "\t\t\t(((a b >) f :=) f if)");
    func_append(f, "\t\t\t\t(b arr [ realind ] =)");
    func_append(f, "\t\t\t\t(a arr [ realind2 ] =)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t\t(j + one j :=)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t// print sorted array");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i n <) f :=) f for)");
    func_append(f, "\t\t(i * int64_size realind :=)");
    func_append(f, "\t\t(arr [ realind ] a =)");
    func_append(f, "\t\t(a + zero a :=)");
    func_append(f, "\t\t(a :print_i !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(zero :exit !)");
}

void gen_struct_demo(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "const-string", "person.name", 64, (const char *[]){"\"Jane\""}, 1);
    add_var_to_func(f, "const-string", "person.city", 64, (const char *[]){"\"New York\""}, 1);
    add_var_to_func(f, "int64", "person.age", 1, (const char *[]){"30"}, 1);
    func_append(f, "\t(person.name :print_s !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(person.city :print_s !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(person.age :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");
}

void gen_pointer_demo(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    const char *const100_v[] = {"100"};
    add_var_to_func(f, "const-int64", "const100", 1, const100_v, 1);
    add_var_to_func(f, "int64", "x", 1, (const char *[]){"42"}, 1);
    add_var_to_func(f, "int64", "Px", 1, vals, 1);
    add_var_to_func(f, "int64", "val", 1, vals, 1);
    func_append(f, "\t// get pointer to x");
    func_append(f, "\t(x Px pointer)");
    func_append(f, "\t(Px [ zero ] val =)");
    func_append(f, "\t(val :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t// change value via pointer");
    func_append(f, "\t(const100 Px [ zero ] =)");
    func_append(f, "\t(x :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");
}

void gen_time_demo(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_include(prog, "misc-macros.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "int64", "hour", 1, vals, 1);
    add_var_to_func(f, "int64", "min", 1, vals, 1);
    add_var_to_func(f, "int64", "sec", 1, vals, 1);
    add_var_to_func(f, "int64", "ten", 1, (const char *[]){"10"}, 1);
    add_var_to_func(f, "int64", "f", 1, vals, 1);
    add_var_to_func(f, "int64", "epchms", 1, vals, 1);
    const char *vepoch[] = {"\"ms after epoch: \""};
    add_var_to_func(f, "const-string", "epochstr", 17, vepoch, 1);
    add_var_to_func(f, "const-string", "colon", 2, (const char *[]){"\":\""}, 1);
    func_append(f, "\t(hour min sec :get_time !)");
    func_append(f, "\t(hour :pull_int64_var !)");
    func_append(f, "\t(min :pull_int64_var !)");
    func_append(f, "\t(sec :pull_int64_var !)");
    func_append(f, "\t(((hour ten <) f =) f if)");
    func_append(f, "\t\t(zero :print_i !)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(hour :print_i !)");
    func_append(f, "\t(colon :print_s !)");
    func_append(f, "\t(((min ten <) f =) f if)");
    func_append(f, "\t\t(zero :print_i !)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(min :print_i !)");
    func_append(f, "\t(colon :print_s !)");
    func_append(f, "\t(((sec ten <) f =) f if)");
    func_append(f, "\t\t(zero :print_i !)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(sec :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(epochstr :print_s !)");
    func_append(f, "\t(epchms :epochms !)");
    func_append(f, "\t(epchms :pull_int64_var !)");
    func_append(f, "\t(epchms :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");
}

void gen_gcd(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    const char *vals9[] = {"9"};
    const char *vals42[] = {"42"};
    f->is_local = 1;
    func_vardef(f, "main");
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "int64", "a~", 1, vals42, 1);
    add_var_to_func(f, "int64", "b~", 1, vals9, 1);
    add_var_to_func(f, "int64", "ret~", 1, vals, 1);
    func_append(f, "\t(a~ b~ :gcd !)");
    func_append(f, "\t(ret~ stpop)");
    func_append(f, "\t(ret~ :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");

    add_func(prog, "gcd");
    Function *gf = &prog->funcs[prog->num_funcs - 1];
    gf->is_local = 1;
    func_vardef(gf, "gcd");
    add_var_to_func(gf, "const-int64", "zero~", 1, vals, 1);
    add_var_to_func(gf, "int64", "a~", 1, vals, 1);
    add_var_to_func(gf, "int64", "b~", 1, vals, 1);
    add_var_to_func(gf, "int64", "mod~", 1, vals, 1);
    add_var_to_func(gf, "int64", "f~", 1, vals, 1);
    func_append(gf, "\t(b~ stpop)");
    func_append(gf, "\t(a~ stpop)");
    func_append(gf, "\t(((b~ zero~ ==) f~ :=) f~ if)");
    func_append(gf, "\t\t(a~ stpush)");
    func_append(gf, "\t\t(return)");
    func_append(gf, "\t(endif)");
    func_append(gf, "\t((a~ b~ %) mod~ =)");
    func_append(gf, "\t(b~ mod~ :gcd !)");
    func_append(gf, "\t(ret~ stpop)");
    func_append(gf, "\t(ret~ stpush)");
    func_append(gf, "\t(return)");
    add_var_to_func(gf, "int64", "ret~", 1, vals, 1);
}

void gen_hex_binary(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "byte", "b", 1, (const char *[]){"&FF"}, 1);
    add_var_to_func(f, "int64", "i", 1, (const char *[]){"&FFFFFFFFFFFFFFFF"}, 1);
    add_var_to_func(f, "byte", "bbin", 1, (const char *[]){"B$10000001"}, 1);
    func_append(f, "\t(b :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(i :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(bbin :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");
}

void gen_bool_demo(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    const char *vone[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "const-int64", "one", 1, vone, 1);
    add_var_to_func(f, "int64", "flag", 1, vone, 1);
    add_var_to_func(f, "int64", "f", 1, vals, 1);
    const char *vtrue[] = {"\"flag is true\""};
    const char *vfalse[] = {"\"flag is false\""};
    add_var_to_func(f, "const-string", "true_str", 13, vtrue, 1);
    add_var_to_func(f, "const-string", "false_str", 14, vfalse, 1);
    func_append(f, "\t// boolean variable demo");
    func_append(f, "\t(((flag one ==) f :=) f if+)");
    func_append(f, "\t\t(true_str :print_s !)");
    func_append(f, "\t(else)");
    func_append(f, "\t\t(false_str :print_s !)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");
}

void gen_string_cat(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "int64", "strmod", 1, vals, 1);
    const char *vhello[] = {"\"Hello \""};
    const char *vname[] = {"\"Brackets\""};
    add_var_to_func(f, "string", "a", 8, vhello, 1);
    add_var_to_func(f, "string", "b", 10, vname, 1);
    add_var_to_func(f, "string", "result", 256, (const char *[]){"\"\""}, 1);
    func_append(f, "\t(strmod :string_init !)");
    func_append(f, "\t(result a :string_copy !)");
    func_append(f, "\t(result b :string_cat !)");
    func_append(f, "\t(result :print_s !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");
    add_include_post(prog, "string.l1h");
}

Template templates[] = {
    {"hello world", "Hello World program", gen_hello_world},
    {"hello", "Hello World program", gen_hello_world},
    {"factorial", "Calculate factorial of a number", gen_factorial},
    {"fibonacci", "Calculate fibonacci sequence", gen_fibonacci},
    {"fizzbuzz", "FizzBuzz program", gen_fizzbuzz},
    {"fizz buzz", "FizzBuzz program", gen_fizzbuzz},
    {"for loop", "For loop example", gen_for_loop},
    {"loop", "For loop example", gen_for_loop},
    {"while loop", "While loop example", gen_while_loop},
    {"while", "While loop example", gen_while_loop},
    {"countdown", "Countdown from 10 to 1", gen_countdown},
    {"count down", "Countdown from 10 to 1", gen_countdown},
    {"if else", "If-else conditionals", gen_if_else},
    {"if", "If-else conditionals", gen_if_else},
    {"condition", "If-else conditionals", gen_if_else},
    {"positive negative", "Check positive/negative/zero", gen_if_else_chain},
    {"switch", "Switch/case example", gen_switch},
    {"case", "Switch/case example", gen_switch},
    {"array", "Array index access demo", gen_array_demo},
    {"print var", "Print a variable", gen_print_var},
    {"print variable", "Print a variable", gen_print_var},
    {"math", "Math operations demo", gen_math_ops},
    {"string concat", "String concatenation demo", gen_string_demo},
    {"string", "String operations demo", gen_string_demo},
    {"input", "User input example", gen_user_input},
    {"user input", "Get user input", gen_user_input},
    {"shell args", "Command line arguments", gen_shell_args},
    {"arguments", "Command line arguments", gen_shell_args},
    {"sum", "Sum of numbers from 1 to N", gen_sum},
    {"primes", "Print prime numbers up to N", gen_primes},
    {"prime", "Print prime numbers up to N", gen_primes},
    {"function", "Function definition example", gen_function_demo},
    {"guess", "Number guessing game", gen_guess_number},
    {"guess number", "Number guessing game", gen_guess_number},
    {"multiplication table", "Multiplication table", gen_multiplication_table},
    {"table", "Multiplication table", gen_multiplication_table},
    {"even odd", "Even/Odd number checker", gen_even_odd},
    {"even", "Even/Odd number checker", gen_even_odd},
    {"power", "Power/exponentiation function", gen_power},
    {"exponent", "Power/exponentiation function", gen_power},
    {"max of three", "Maximum of three numbers", gen_max_of_three},
    {"max", "Maximum of three numbers", gen_max_of_three},
    {"what is your name", "Ask name and greet", gen_hello_name},
    {"your name", "Ask name and greet", gen_hello_name},
    {"bubble sort", "Bubble sort algorithm", gen_bubble_sort},
    {"sort", "Bubble sort algorithm", gen_bubble_sort},
    {"struct", "Person struct (dotted vars) demo", gen_struct_demo},
    {"person", "Person struct (dotted vars) demo", gen_struct_demo},
    {"pointer", "Pointer access demo", gen_pointer_demo},
    {"time", "Get current time and epoch ms", gen_time_demo},
    {"clock", "Get current time and epoch ms", gen_time_demo},
    {"boolean variable", "Boolean variable demo", gen_bool_demo},
    {"concatenate strings", "String concatenation demo", gen_string_cat},
    {"gcd", "GCD (greatest common divisor)", gen_gcd},
    {"greatest common divisor", "GCD (greatest common divisor)", gen_gcd},
    {"hex", "Hex and binary number demo", gen_hex_binary},
    {"binary", "Hex and binary number demo", gen_hex_binary},
};

int num_templates = sizeof(templates) / sizeof(templates[0]);

int match_template(const char *prompt, int *best_score) {
    char buf[MAX_PROMPT];
    snprintf(buf, sizeof(buf), "%s", prompt);
    to_lowercase(buf);

    int best_idx = -1;
    *best_score = 0;

    // Count total words in prompt (to avoid matching generic templates against complex prompts)
    int total_words = 0;
    {
        char wc_buf[MAX_PROMPT];
        char *saveptr;
        snprintf(wc_buf, sizeof(wc_buf), "%s", buf);
        char *wc = strtok_r(wc_buf, " ", &saveptr);
        while (wc) { trim(wc); if (strlen(wc) > 0) total_words++; wc = strtok_r(NULL, " ", &saveptr); }
    }

    for (int i = 0; i < num_templates; i++) {
        char kwbuf[512];
        snprintf(kwbuf, sizeof(kwbuf), "%s", templates[i].keywords);
        to_lowercase(kwbuf);

        int score = 0;
        int num_kw = 0;
        char kwcopy[512];
        snprintf(kwcopy, sizeof(kwcopy), "%s", kwbuf);
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
            if (total_words > num_kw * 3) continue;
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
        c_printf(ANSI_RED, "Error: failed to execute '%s'\n", argv[0]);
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
        snprintf(ppname, sizeof(ppname), "%.*s_pp.l1com", len, filename);
    } else {
        snprintf(ppname, sizeof(ppname), "%s_pp.l1com", filename);
    }
    const char *include_dir = getenv("L1VM_INCLUDE");
    if (!include_dir) {
        static char fallback[1024];
        if (l1vm_root[0]) {
            snprintf(fallback, sizeof(fallback), "%s/include/", l1vm_root);
        } else {
            const char *home = getenv("HOME");
            if (home) snprintf(fallback, sizeof(fallback), "%s/l1vm/include/", home);
            else snprintf(fallback, sizeof(fallback), "/usr/local/l1vm/include/");
        }
        include_dir = fallback;
    }

    const char *l1pre_bin = "l1pre";
    const char *l1com_bin = "l1com";
    char l1pre_path[1024];
    char l1com_path[1024];
    if (l1vm_root[0]) {
        snprintf(l1pre_path, sizeof(l1pre_path), "%s/bin/l1pre", l1vm_root);
        l1pre_bin = l1pre_path;
        snprintf(l1com_path, sizeof(l1com_path), "%s/bin/l1com", l1vm_root);
        l1com_bin = l1com_path;
    }

    const char *l1pre_argv[] = {l1pre_bin, filename, ppname, include_dir, NULL};
    int ret = run_cmd(l1pre_argv);
    if (ret != 0) {
        if (ret > 0) c_printf(ANSI_RED, "Validation: l1pre FAILED (exit code %d)\n", ret);
        (void)remove(ppname);
        return 0;
    }

    char compname[512];
    snprintf(compname, sizeof(compname), "%.*s_pp", (int)(dot ? dot - filename : (int)strlen(filename)), filename);
    const char *l1com_argv[] = {l1com_bin, compname, NULL};
    ret = run_cmd(l1com_argv);
    if (ret == 0) {
        c_printf(ANSI_GREEN, "Validation: OK\n");
    } else if (ret > 0) {
        c_printf(ANSI_RED, "Validation: FAILED (exit code %d)\n", ret);
    }
    (void)remove(ppname);
    return ret == 0;
}

void prepend_out_dir(const char *fname, char *buf, int bufsize) {
    if (out_dir[0]) {
        snprintf(buf, bufsize, "%s/%s", out_dir, fname);
        buf[bufsize - 1] = '\0';
    } else {
        snprintf(buf, bufsize, "%s", fname);
    }
}

int generate_code(const char *prompt, const char *filename) {
    int is_q = is_question(prompt);
    if (is_q)
        answer_question(prompt);

    Program *prog = malloc(sizeof(Program));
    if (!prog) { c_printf(ANSI_RED, "Out of memory\n"); return 0; }
    if (!init_program(prog)) { free_program(prog); free(prog); c_printf(ANSI_RED, "Out of memory\n"); return 0; }
    snprintf(prog->filename, sizeof(prog->filename), "%s", filename);
    // (temp/func counters removed)

    // Try exact keyword template match first (most reliable)
    int score;
    int idx = match_template(prompt, &score);

    if (idx >= 0) {
        if (is_q) c_printf(ANSI_CYAN, "Code example written to: %s\n", filename);
        else c_printf(ANSI_GREEN, "Matched template: %s (score: %d)\n", templates[idx].desc, score);
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
        gen_hello_world(prog, prompt);
        Function *f = NULL;
        for (int i = 0; i < prog->num_funcs; i++)
            if (strcmp(prog->funcs[i].name, "main") == 0) { f = &prog->funcs[i]; break; }
        if (f) {
            char nn_msg[1100];
            snprintf(nn_msg, sizeof(nn_msg), "\t// nearest example: %s", example_docs[nn_indices[0]].filename);
            func_append(f, nn_msg);
        }
        c_printf(ANSI_YELLOW, "Nearest-neighbor generated: %s\n", example_docs[nn_indices[0]].stem);
        write_program(prog, filename);
        free_program(prog); free(prog);
        return 1;
    }

    c_printf(ANSI_YELLOW, "No template matched for: '%s'. Generating default hello world.\n", prompt);
    gen_hello_world(prog, prompt);
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
    if (histfile) { snprintf(histpath, sizeof(histpath), "%s/.brackets-code_history", histfile); read_history(histpath); }
    char *rl_line;
    while ((rl_line = readline("> ")) != NULL) {
        snprintf(prompt, sizeof(prompt), "%s", rl_line);
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
                snprintf(fname, sizeof(fname), "%s", prompt + 6);
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
                    snprintf(ldesc, sizeof(ldesc), "%s", lkeywords);
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
        snprintf(last_fname, sizeof(last_fname), "%s", fname);
        has_last = 1;
    }
    printf("\nBye!\n");
#ifdef HAVE_READLINE
    {
        const char *hf = getenv("HOME");
        if (hf) { char hp[512]; snprintf(hp, sizeof(hp), "%s/.brackets-code_history", hf); write_history(hp); }
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
            if (arg_idx + 1 < argc) { snprintf(out_dir, sizeof(out_dir), "%s", argv[arg_idx + 1]); arg_idx += 2; }
            else { fprintf(stderr, "Usage: ... --out-dir <directory>\n"); return 1; }
        }
        else if (strcmp(argv[arg_idx], "--l1vm-root") == 0) {
            if (arg_idx + 1 < argc) { snprintf(l1vm_root, sizeof(l1vm_root), "%s", argv[arg_idx + 1]); arg_idx += 2; }
            else { fprintf(stderr, "Usage: ... --l1vm-root <path>\n"); return 1; }
        }
        else if (strcmp(argv[arg_idx], "--validate") == 0 || strcmp(argv[arg_idx], "-v") == 0) { validate_flag = 1; arg_idx++; }
        else break;
    }

    // Batch mode: consumes all remaining flags, expects batch-file right after --batch
    if (argc >= arg_idx + 2 && (strcmp(argv[arg_idx], "--batch") == 0)) {
        const char *batch_file = argv[arg_idx + 1];
        FILE *bf = fopen(batch_file, "r");
        if (!bf) { c_printf(ANSI_RED, "Cannot open batch file: %s\n", batch_file); return 1; }
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
        fprintf(stderr, "Usage: %s [--validate] [--l1vm-root <path>] <prompt> [filename]\n", argv[0]);
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
        snprintf(fname, sizeof(fname), "%s", argv[arg_idx]);
        // block path traversal in filename
        if (strstr(fname, "..")) {
            fprintf(stderr, "Error: filename must not contain '..'\n");
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
                c_printf(ANSI_YELLOW, "Validation failed, retrying (%d/3)...\n", retry + 1);
            }
        } else if (gen_ok) {
            validated = 1;
            exit_code = 0;
            break;
        }
    }
    if (!validated && validate_flag) {
        c_printf(ANSI_RED, "Warning: code could not be validated after 3 attempts.\n");
        exit_code = 2;
    } else if (!validated) {
        exit_code = 1;
    }
    return exit_code;
}
