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

int validate_flag = 0;
int retry_seed = 0;
int verbose_flag = 0;
int dry_run_flag = 0;
char out_dir[PATH_BUF_SIZE] = "";
char l1vm_root[PATH_BUF_SIZE] = "";

int use_color = 1;

LearnedPattern learned_patterns[MAX_LEARNED];
int num_learned = 0;
int learned_loaded = 0;

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

static int resize_includes_arr(char (**pinc)[WORD_BUF_SIZE], int *cap, int new_cap) {
    char (*p)[WORD_BUF_SIZE] = realloc(*pinc, (size_t)new_cap * sizeof(char[WORD_BUF_SIZE]));
    if (!p) return 0;
    *pinc = p;
    if (new_cap > *cap)
        memset(&(*pinc)[*cap], 0, (size_t)(new_cap - *cap) * sizeof(char[WORD_BUF_SIZE]));
    *cap = new_cap;
    return 1;
}

int ensure_includes_cap(char (**pinc)[WORD_BUF_SIZE], int *cap, int needed) {
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
    while (isspace((unsigned char)p[l-1])) p[--l] = 0;
    while (*p && isspace((unsigned char)*p)) ++p;
    memmove(s, p, l - (p - s) + 1);
}

int str_contains_word(const char *str, const char *word) {
    char buf[MAX_LINE];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", str);
    char *p = buf;
    while (*p) {
        while (*p && !isalpha((unsigned char)*p)) p++;
        if (!*p) break;
        char *start = p;
        while (*p && isalpha((unsigned char)*p)) p++;
        char saved = *p;
        *p = '\0';
        if (strcasecmp(start, word) == 0) { *p = saved; return 1; }
        *p = saved;
    }
    return 0;
}

void to_lowercase(char *s) {
    for (; *s; s++) *s = (char)tolower((unsigned char)*s);
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
    const char *best = NULL;
    int best_len = 0;
    for (int i = 0; i < (int)(sizeof(ops)/sizeof(ops[0])); i++) {
        const char *hit = strstr(buf, ops[i].op);
        if (hit) {
            int oplen = (int)strlen(ops[i].op);
            if (oplen > best_len) {
                best_len = oplen;
                best = ops[i].rpn_op;
            }
        }
    }
    return best;
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
        char path[FULLPATH_BUF_SIZE];
        SNPRINTF_CHECK(path, sizeof(path), "%s/%s/%s", home, SYNONYM_DIR, SYNONYM_FILE);
        f = fopen(path, "r");
        if (f) return f;
    }
    // Try next to the executable
    char self_path[FULLPATH_BUF_SIZE];
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
    char line[WORD_BUF_SIZE];
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
    {"auffangen", "input"}, {"capture", "input"}, {"eingabe", "input"},
    {"einlesen", "read"}, {"erfassen", "input"}, {"erfasse", "input"}, {"erhalten", "input"},
    {"fetch", "read"}, {"gib ein", "input"}, {"import", "read"},
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
    {"bedingung", "if"}, {"condition", "if"}, {"falls", "if"}, {"repeat", "loop"},
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
    {"benchmark", "timer"}, {"clock", "timer"}, {"performance", "timer"},
    {"network", "data"}, {"crypto", "data"}, {"encrypt", "data"},
    {"json", "data"}, {"config", "data"},
    {"graphics", "show"}, {"render", "show"},
    {"game", "guess"}, {"play", "guess"},
    {"float", "num"}, {"bignum", "num"}, {"big", "num"},
    {"include", "file"}, {"header", "file"},
    {"vector", "array"}, {"collection", "array"},
    {"lookup", "find"},
    {"copy", "assign"}, {"duplicate", "assign"}, {"clone", "assign"},
    {"delete", "remove"}, {"clear", "remove"}, {"erase", "remove"},
    {"pop", "remove"}, {"extract", "remove"},
    {"call", "run"}, {"invoke", "run"}, {"execute", "run"},
    {"word", "string"},
    {"fann", "fann"}, {"neural", "fann"}, {"netz", "fann"}, {"network", "data"},
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
        while (*p && !isalpha((unsigned char)*p)) p++;
        if (!*p) break;
        char *start = p;
        while (*p && isalpha((unsigned char)*p)) p++;
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

static int _has_word_lowered(const char *lowered_text, const char *keyword);

int has_word_fuzzy(const char *text, const char *keyword) {
    char buf[MAX_PROMPT];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", text);
    to_lowercase(buf);
    return _has_word_lowered(buf, keyword);
}

static int _has_word_lowered(const char *lowered_text, const char *keyword) {
    int kwlen = strlen(keyword);
    const char *p = lowered_text;
    while ((p = strstr(p, keyword)) != NULL) {
        int at_start = (p == lowered_text);
        int at_end = (p[kwlen] == '\0');
        int before_ok = at_start || !isalpha((unsigned char)*(p - 1));
        int after_ok = at_end || !isalpha((unsigned char)p[kwlen]);
        if (before_ok && after_ok) return 1;
        p++;
    }
    p = lowered_text;
    while (*p) {
        while (*p && !isalpha((unsigned char)*p)) p++;
        if (!*p) break;
        const char *start = p;
        while (*p && isalpha((unsigned char)*p)) p++;
        int len = (int)(p - start);
        if (len >= WORD_BUF_SIZE) len = WORD_BUF_SIZE - 1;
        char word_buf[WORD_BUF_SIZE];
        memcpy(word_buf, start, len);
        word_buf[len] = '\0';
        const char *canonical = resolve_synonym(word_buf);
        if (canonical != NULL && strcmp(canonical, keyword) == 0) return 1;
    }
    return 0;
}

int has_word(const char *prompt, const char *word) {
    return has_word_fuzzy(prompt, word);
}

static int _is_negated_lowered(const char *lowered_text, const char *keyword) {
    int kwlen = strlen(keyword);
    const char *p = lowered_text;
    static const char *negations[] = {
        "not", "don't", "dont", "doesn't", "doesnt", "isn't", "isnt",
        "won't", "wont", "can't", "cant", "no", "without", "kein",
        "nicht", "keine", "keinen", "keinem", "never", "niemals",
        "weder", "nor", "neither", NULL
    };
    while ((p = strstr(p, keyword)) != NULL) {
        int at_start = (p == lowered_text);
        int before_ok = at_start || !isalpha((unsigned char)*(p - 1));
        int after_ok = (p[kwlen] == '\0') || !isalpha((unsigned char)p[kwlen]);
        if (before_ok && after_ok) {
            const char *scan = p - 1;
            int words_back = 0;
            while (scan >= lowered_text && words_back < 5) {
                while (scan >= lowered_text && isspace((unsigned char)*scan)) scan--;
                if (scan < lowered_text) break;
                const char *word_end = scan;
                while (scan >= lowered_text && isalpha((unsigned char)*scan)) scan--;
                const char *word_start = scan + 1;
                int wlen = word_end - word_start + 1;
                if (wlen > 0 && wlen < 32) {
                    char wbuf[32];
                    memcpy(wbuf, word_start, wlen);
                    wbuf[wlen] = '\0';
                    for (int ni = 0; negations[ni]; ni++) {
                        if (strcmp(wbuf, negations[ni]) == 0) return 1;
                    }
                    words_back++;
                }
                if (scan >= lowered_text) scan--;
            }
            return 0;
        }
        p++;
    }
    return 0;
}

// ==================== TASK PROFILE ====================


static int word_to_num(const char *word);

static int extract_numbers(const char *prompt, int *nums, int max_nums) {
    char buf[MAX_PROMPT];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", prompt);
    to_lowercase(buf);
    int count = 0;
    char *p = buf;
    while (*p && count < max_nums) {
        if (isalpha((unsigned char)*p)) {
            char word[32]; int wi = 0;
            while (*p && isalpha((unsigned char)*p) && wi < 31) word[wi++] = *p++;
            word[wi] = '\0';
            int n = word_to_num(word);
            if (n >= 0) nums[count++] = n;
            continue;
        }
        p++;
    }
    p = buf;
    while (*p && count < max_nums) {
        if (isdigit((unsigned char)*p)) {
            char *prev = p - 1;
            while (prev >= buf && (isalpha((unsigned char)*prev) || *prev == '_')) prev--;
            prev++;
            if (prev < p) {
                char prefix[16]; int pi = 0;
                while (prev < p && pi < 15) prefix[pi++] = *prev++;
                prefix[pi] = '\0';
                if (prefix[0] == 'i' && prefix[1] == 'n' && prefix[2] == 't') {
                    while (*p && isdigit((unsigned char)*p)) p++;
                    continue;
                }
                if (strcmp(prefix, "const") == 0) {
                    while (*p && isdigit((unsigned char)*p)) p++;
                    continue;
                }
            }
            char *endptr = NULL;
            long val = strtol(p, &endptr, 10);
            if (endptr != p && val >= INT_MIN && val <= INT_MAX)
                nums[count++] = (int)val;
            while (*p && isdigit((unsigned char)*p)) p++;
            continue;
        }
        p++;
    }
    return count;
}

// Forward declarations for plan-based generator

/* ── parse_task helpers ─────────────────────────────────────────────────── */

static void parse_task_detect_type(const char *buf, TaskProfile *task) {
    if (strstr(buf, "byte") || strstr(buf, "int8")) SNPRINTF_CHECK(task->type, sizeof(task->type), "%s", "byte");
    else if (strstr(buf, "int16") || strstr(buf, "short")) SNPRINTF_CHECK(task->type, sizeof(task->type), "%s", "int16");
    else if (strstr(buf, "int32")) SNPRINTF_CHECK(task->type, sizeof(task->type), "%s", "int32");
    else if (strstr(buf, "int64") || _has_word_lowered(buf, "int") || strstr(buf, "long")) SNPRINTF_CHECK(task->type, sizeof(task->type), "%s", "int64");
    else if (strstr(buf, "double") || strstr(buf, "float") || strstr(buf, "real") || strstr(buf, "komma")) SNPRINTF_CHECK(task->type, sizeof(task->type), "%s", "double");
}

static void parse_task_detect_literals(const char *prompt, const char *buf, TaskProfile *task) {
    task->num_literals = extract_numbers(prompt, task->literals, MAX_NUMS);
    int has_decimals = 0;
    for (int di = 0; di < task->num_literals && di < MAX_NUMS; di++)
        task->double_literals[di] = (double)task->literals[di];
    {
        char dscan[MAX_PROMPT];
        SNPRINTF_CHECK(dscan, sizeof(dscan), "%s", buf);
        char *dp = dscan;
        int di = 0;
        while (*dp && di < MAX_NUMS) {
            if (isdigit((unsigned char)*dp)) {
                char *start = dp;
                while (*dp && (isdigit((unsigned char)*dp) || *dp == '.')) dp++;
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
        if (has_decimals) {
            task->num_literals = di;
            SNPRINTF_CHECK(task->type, sizeof(task->type), "%s", "double");
        }
    }
    if (task->num_literals > 0) TF_SET(task, FLAG_literals); else TF_CLR(task, FLAG_literals);
}

static void parse_task_detect_io(char *buf, TaskProfile *task) {
    /* count detection: word before "numbers/werte/zahlen/values" */
    char *p = buf;
    while (*p) {
        if (strncmp(p, "numbers", 7) == 0 || strncmp(p, "werte", 5) == 0 || strncmp(p, "zahlen", 6) == 0 || strncmp(p, "values", 6) == 0) {
            char *q = p - 1;
            while (q >= buf && isspace((unsigned char)*q)) q--;
            while (q >= buf && isdigit((unsigned char)*q)) q--;
            q++;
            if (q < p && isdigit((unsigned char)*q)) {
                task->input_count = (int)strtol(q, NULL, 10);
                TF_SET(task, FLAG_input);
            } else {
                q = p - 1;
                while (q >= buf && isspace((unsigned char)*q)) q--;
                char *end = q + 1;
                while (q >= buf && isalpha((unsigned char)*q)) q--;
                q++;
                if (q < end) {
                    char w[32]; int wi = 0;
                    while (q < end && wi < 31) w[wi++] = *q++;
                    w[wi] = '\0';
                    int n = word_to_num(w);
                    if (n > 0) { task->input_count = n; TF_SET(task, FLAG_input); }
                }
            }
            break;
        }
        p++;
    }

    /* detect input keywords */
    if (_has_word_lowered(buf, "lies") || _has_word_lowered(buf, "lese") || _has_word_lowered(buf, "read") || _has_word_lowered(buf, "input")
        || _has_word_lowered(buf, "gib") || _has_word_lowered(buf, "enter") || _has_word_lowered(buf, "erfasse")
        || _has_word_lowered(buf, "collect") || _has_word_lowered(buf, "eingabe") || _has_word_lowered(buf, "einlesen"))
        TF_SET(task, FLAG_input);

    /* detect output */
    if (_has_word_lowered(buf, "print") || _has_word_lowered(buf, "druck") || _has_word_lowered(buf, "ausg")
        || _has_word_lowered(buf, "show") || _has_word_lowered(buf, "display") || _has_word_lowered(buf, "zeig")
        || _has_word_lowered(buf, "output") || _has_word_lowered(buf, "schreib"))
        TF_SET(task, FLAG_output);

    /* detect print variable */
    if (TF_ISSET(task, FLAG_output) && strstr(buf, "variable"))
        TF_SET(task, FLAG_print_var);

    /* detect operation */
    const char *op = find_operation(buf);
    if (op) {
        TF_SET(task, FLAG_operation);
        SNPRINTF_CHECK(task->op, sizeof(task->op), "%s", op);
        if (strcmp(op, "add") == 0) TF_SET(task, FLAG_add);
        else if (strcmp(op, "sub") == 0) TF_SET(task, FLAG_sub);
        else if (strcmp(op, "mul") == 0) TF_SET(task, FLAG_mul);
        else if (strcmp(op, "div") == 0) TF_SET(task, FLAG_div);
    }
    for (int oi = 0; oi < (int)(sizeof(ops)/sizeof(ops[0])); oi++) {
        if (strstr(buf, ops[oi].op)) {
            if (strcmp(ops[oi].rpn_op, "add") == 0) TF_SET(task, FLAG_add);
            else if (strcmp(ops[oi].rpn_op, "sub") == 0) TF_SET(task, FLAG_sub);
            else if (strcmp(ops[oi].rpn_op, "mul") == 0) TF_SET(task, FLAG_mul);
            else if (strcmp(ops[oi].rpn_op, "div") == 0) TF_SET(task, FLAG_div);
        }
    }

    /* build operation sequence in prompt order */
    task->num_ops = 0;
    {
        int found_ops = 0;
        int op_positions[32];
        char op_names[32][64];
        for (int oi = 0; oi < (int)(sizeof(ops)/sizeof(ops[0])); oi++) {
            char *pp = buf;
            while ((pp = strstr(pp, ops[oi].op)) != NULL && found_ops < 32) {
                int at_start = (pp == buf);
                int before_ok = at_start || !isalpha((unsigned char)*(pp - 1));
                if (before_ok) {
                    int pos = (int)(pp - buf);
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
                pp++;
            }
        }
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
        int lit_idx = 0;
        for (int oi = 0; oi < found_ops && lit_idx < task->num_literals; oi++) {
            SNPRINTF_CHECK(task->op_seq[task->num_ops], sizeof(task->op_seq[0]), "%.*s", (int)sizeof(task->op_seq[0]) - 1, op_names[oi]);
            task->op_seq_literals[task->num_ops] = task->literals[lit_idx++];
            task->num_ops++;
        }
    }

    /* if count not yet set and single literal could be a count */
    if (!TF_ISSET(task, FLAG_input) && task->num_literals == 1 && TF_ISSET(task, FLAG_literals) && !TF_ISSET(task, FLAG_operation)) {
        int lit = task->literals[0];
        if (lit >= 1 && lit <= 1000) {
            task->input_count = lit;
            TF_SET(task, FLAG_input);
        }
    }
}

static void parse_task_detect_flags(const char *buf, TaskProfile *task) {
    /* Table-driven simple OR-pattern rules: each entry is a flag + NULL-terminated word list */
    static const struct { int flag; const char *words[8]; } flag_rules[] = {
        { FLAG_loop,          {"loop", "schleife", "for", "while", "wiederhol", "iterate", NULL} },
        { FLAG_condition,     {"if", "bedingung", "wenn", "condition", "falls", NULL} },
        { FLAG_sort,          {"sort", "bubble", "sortiere", NULL} },
        { FLAG_descending,    {"descending", "desc", "absteigend", NULL} },
        { FLAG_power,         {"power", "potenz", "exponent", "square", "quadrat", "cube", NULL} },
        { FLAG_max,           {"max", "largest", "greatest", "highest", "maximum", NULL} },
        { FLAG_min,           {"min", "kleinste", "smallest", "least", "lowest", "minimum", NULL} },
        { FLAG_countdown,     {"countdown", "count down", NULL} },
        { FLAG_guess,         {"guess", "rate", "raten", NULL} },
        { FLAG_random,        {"random", "zufall", "rand", NULL} },
        { FLAG_pointer,       {"point", "zeiger", "adresse", "address", "memory", "mem", NULL} },
        { FLAG_struct,        {"struct", "dotted", "punkt", "struktur", "record", NULL} },
        { FLAG_function,      {"function", "funktion", "subroutine", NULL} },
        { FLAG_hex_binary,    {"hex", "binary", "binaer", NULL} },
        { FLAG_average,       {"average", "durchschnitt", "mean", NULL} },
        { FLAG_fizzbuzz,      {"fizzbuzz", "fizz buzz", "fizz", NULL} },
        { FLAG_primes,        {"prime", "prim", NULL} },
        { FLAG_sum,           {"sum", "summe", "add", NULL} },
        { FLAG_factorial,     {"factorial", "fakult", NULL} },
        { FLAG_fibonacci,     {"fib", "fibonacci", NULL} },
        { FLAG_median,        {"median", "mitte", NULL} },
        { FLAG_string_compare,{"compare", "vergleich", "cmp", NULL} },
        { FLAG_timer,         {"timer", "zeit", "time", "benchmark", "measure", "mess", "dauer", NULL} },
        { FLAG_even_odd,      {"even", "odd", "gerade", "ungerade", NULL} },
    };
    for (size_t i = 0; i < sizeof(flag_rules)/sizeof(flag_rules[0]); i++) {
        for (int j = 0; flag_rules[i].words[j]; j++) {
            if (_has_word_lowered(buf, flag_rules[i].words[j])) {
                TF_SET(task, flag_rules[i].flag);
                break;
            }
        }
    }

    /* Special cases: compound conditions, multi-flag, or negative checks */
    if ((_has_word_lowered(buf, "bignum") || (_has_word_lowered(buf, "big") && _has_word_lowered(buf, "number")) || _has_word_lowered(buf, "mpfr"))
        && (_has_word_lowered(buf, "power") || _has_word_lowered(buf, "pow") || _has_word_lowered(buf, "potenz") || _has_word_lowered(buf, "calculate") || _has_word_lowered(buf, "^")))
        TF_SET(task, FLAG_bignum_math);

    if (_has_word_lowered(buf, "gcd") || _has_word_lowered(buf, "ggt") || _has_word_lowered(buf, "gcm") || strstr(buf, "common divisor") || strstr(buf, "greatest common"))
        TF_SET(task, FLAG_gcd);

    if (!_has_word_lowered(buf, "ascii") && (_has_word_lowered(buf, "table") || _has_word_lowered(buf, "einmaleins") || _has_word_lowered(buf, "multiplication")))
        TF_SET(task, FLAG_mult_table);

    if ((_has_word_lowered(buf, "string") || strstr(buf, "strings") || strstr(buf, "string ")
        || _has_word_lowered(buf, "zeichen") || _has_word_lowered(buf, "text")
        || _has_word_lowered(buf, "wort"))
        && !_has_word_lowered(buf, "length") && !_has_word_lowered(buf, "laenge"))
        TF_SET(task, FLAG_string_cat);

    if (_has_word_lowered(buf, "file") || _has_word_lowered(buf, "datei"))
        { TF_SET(task, FLAG_read_file); TF_SET(task, FLAG_write_file); }

    if (strcmp(task->op, "add") == 0) TF_SET(task, FLAG_sum);

    if ((_has_word_lowered(buf, "hello") || _has_word_lowered(buf, "hallo") || _has_word_lowered(buf, "name")
        || (_has_word_lowered(buf, "what") && _has_word_lowered(buf, "your"))))
        TF_SET(task, FLAG_hello_name);
}

/* ── Pattern table for keyword→flag mapping ──────────────────────────── */

static const PatternTableEntry pattern_table[] = {
    /* array operations */
    { {"array"},    {"reverse"}, {NULL}, FLAG_array_reverse, 1 },
    { {"reverse"}, {"element"}, {NULL}, FLAG_array_reverse, 1 },
    { {"reverse"},    {"list"}, {NULL}, FLAG_array_reverse, 1 },
    { {"reverse"},   {"order"}, {NULL}, FLAG_array_reverse, 1 },
    { {"reverse"}, {"reihenfolge"}, {NULL}, FLAG_array_reverse, 1 },
    { {"array"},      {"find"}, {NULL}, FLAG_array_find, 1 },
    { {"array"},    {"search"}, {NULL}, FLAG_array_find, 1 },
    { {"array"},     {"suche"}, {NULL}, FLAG_array_find, 1 },
    { {"array"},    {"assign"}, {NULL}, FLAG_array_assign, 1 },
    { {"array"},    {"access"}, {NULL}, FLAG_array_access, 1 },
    { {"array"},      {"read"}, {NULL}, FLAG_array_access, 1 },
    { {"array"},       {"get"}, {NULL}, FLAG_array_access, 1 },
    { {"array"},   {"element"}, {NULL}, FLAG_array_access, 1 },
    { {"array"},     {"index"}, {NULL}, FLAG_array_access, 1 },
    { {"array"},     {"write"}, {NULL}, FLAG_array_write, 1 },
    { {"array"},       {"set"}, {NULL}, FLAG_array_write, 1 },
    { {"array"},     {"store"}, {NULL}, FLAG_array_write, 1 },
    { {"array"},       {"min"}, {NULL}, FLAG_array_vmath, 1 },
    { {"array"},       {"max"}, {NULL}, FLAG_array_vmath, 1 },
    { {"array"},   {"average"}, {NULL}, FLAG_array_vmath, 1 },
    { {"array"},     {"vmath"}, {NULL}, FLAG_array_vmath, 1 },
    { {"stat"},      {"array"}, {NULL}, FLAG_array_vmath, 1 },
    { {"min"},{"max", "array"}, {NULL}, FLAG_array_min_max, 1 },

    /* I/O */
    { {"read"},       {"file"}, {NULL}, FLAG_read_file, 1 },
    { {"write"},      {"file"}, {NULL}, FLAG_write_file, 1 },

    /* string / number */
    { {"string"},   {"to", "number"}, {NULL}, FLAG_string_to_num, 1 },
    { {"string"},{"parse", "number"}, {NULL}, FLAG_string_to_num, 1 },
    { {"string"},{"convert", "number"}, {NULL}, FLAG_string_to_num, 1 },
    { {"string"},      {"to", "num"}, {NULL}, FLAG_string_to_num, 1 },

    /* misc */
    { {"bool"},               {NULL}, {NULL}, FLAG_bool_demo, 1 },
    { {"boolean"},            {NULL}, {NULL}, FLAG_bool_demo, 1 },
    { {"bit"},       {"check"}, {NULL}, FLAG_bit_check, 1 },
    { {"bit"},         {"set"}, {NULL}, FLAG_bit_check, 1 },
    { {"bit"},       {"clear"}, {NULL}, FLAG_bit_check, 1 },
    { {"leap"},       {"year"}, {NULL}, FLAG_leap_year, 1 },
    { {"schalt"},     {"jahr"}, {NULL}, FLAG_leap_year, 1 },
    { {"celsius"}, {"convert"}, {NULL}, FLAG_temp_convert, 1 },
    { {"celsius"},{"umrechnen"},{NULL}, FLAG_temp_convert, 1 },
    { {"celsius"},      {"to"}, {NULL}, FLAG_temp_convert, 1 },
    { {"fahrenheit"},{"convert"},{NULL}, FLAG_temp_convert, 1 },
    { {"fahrenheit"},{"umrechnen"},{NULL},FLAG_temp_convert, 1 },
    { {"fahrenheit"},   {"to"}, {NULL}, FLAG_temp_convert, 1 },
    { {"temperature"},{"convert"},{NULL},FLAG_temp_convert, 1 },
    { {"temperature"},{"umrechnen"},{NULL},FLAG_temp_convert, 1 },
    { {"temperature"},  {"to"}, {NULL}, FLAG_temp_convert, 1 },
    { {"circle"},     {"area"}, {NULL}, FLAG_circle_area, 1 },
    { {"circle"},   {"fläche"}, {NULL}, FLAG_circle_area, 1 },
    { {"circle"},  {"flaeche"}, {NULL}, FLAG_circle_area, 1 },
    { {"kreis"},      {"area"}, {NULL}, FLAG_circle_area, 1 },
    { {"kreis"},    {"fläche"}, {NULL}, FLAG_circle_area, 1 },
    { {"kreis"},   {"flaeche"}, {NULL}, FLAG_circle_area, 1 },

    /* palindrome / math */
    { {"palindrome"},         {NULL}, {NULL}, FLAG_palindrome, 0 },
    { {"reverse"},  {"number"}, {NULL}, FLAG_palindrome, 0 },
    { {"lcm"},                {NULL}, {NULL}, FLAG_lcm, 0 },
    { {"least"},  {"multiple"}, {NULL}, FLAG_lcm, 0 },
    { {"collatz"},            {NULL}, {NULL}, FLAG_collatz, 0 },
    { {"3n"},            {"1"}, {NULL}, FLAG_collatz, 0 },
    { {"sum"},       {"digit"}, {NULL}, FLAG_sum_of_digits, 0 },
    { {"reverse"},  {"string"}, {NULL}, FLAG_reverse_string, 0 },
    { {"reverse"},    {"text"}, {NULL}, FLAG_reverse_string, 0 },
    { {"reverse"},    {"wort"}, {NULL}, FLAG_reverse_string, 0 },
    { {"armstrong"},          {NULL}, {NULL}, FLAG_armstrong, 0 },
    { {"perfect"},  {"number"}, {NULL}, FLAG_perfect_number, 0 },
    { {"vollkommen"},{"number"},{NULL}, FLAG_perfect_number, 0 },
    { {"vowel"},              {NULL}, {NULL}, FLAG_count_vowels, 0 },
    { {"vokale"},             {NULL}, {NULL}, FLAG_count_vowels, 0 },
    { {"selbstlaut"},         {NULL}, {NULL}, FLAG_count_vowels, 0 },
    { {"anagram"},            {NULL}, {NULL}, FLAG_anagram_check, 0 },

    /* string ops */
    { {"string"},    {"upper"}, {NULL}, FLAG_string_to_upper, 0 },
    { {"zeichen"},   {"upper"}, {NULL}, FLAG_string_to_upper, 0 },
    { {"text"},      {"upper"}, {NULL}, FLAG_string_to_upper, 0 },
    { {"string"},    {"lower"}, {NULL}, FLAG_string_to_lower, 0 },
    { {"zeichen"},   {"lower"}, {NULL}, FLAG_string_to_lower, 0 },
    { {"text"},      {"lower"}, {NULL}, FLAG_string_to_lower, 0 },
    { {"caesar"},             {NULL}, {NULL}, FLAG_caesar_cipher, 0 },
    { {"shift"},    {"cipher"}, {NULL}, FLAG_caesar_cipher, 0 },
    { {"palindrome"},{"string"},{NULL}, FLAG_palindrome_string, 0 },
    { {"palindrome"}, {"text"}, {NULL}, FLAG_palindrome_string, 0 },
    { {"palindrome"}, {"wort"}, {NULL}, FLAG_palindrome_string, 0 },

    /* sort / search */
    { {"bubble"},     {"sort"}, {NULL}, FLAG_bubble_sort, 0 },
    { {"binary"},   {"search"}, {"tree"}, FLAG_binary_search, 0 },
    { {"square"},     {"root"}, {NULL}, FLAG_square_root, 0 },
    { {"prime"},    {"factor"}, {NULL}, FLAG_prime_factorization, 0 },
    { {"prime"},      {"teil"}, {NULL}, FLAG_prime_factorization, 0 },
    { {"standard"},{"deviation"},{NULL},FLAG_standard_deviation, 0 },
    { {"compound"},{"interest"},{NULL}, FLAG_compound_interest, 0 },
    { {"binary"},  {"decimal"}, {NULL}, FLAG_decimal_to_binary, 0 },
    { {"binary"},  {"convert"}, {NULL}, FLAG_decimal_to_binary, 0 },
    { {"binary"},     {"base"}, {NULL}, FLAG_decimal_to_binary, 0 },

    /* dice / double */
    { {"dice"},               {NULL}, {NULL}, FLAG_dice_roll, 0 },
    { {"würfel"},             {NULL}, {NULL}, FLAG_dice_roll, 0 },
    { {"roll"},        {"die"}, {NULL}, FLAG_dice_roll, 0 },
    { {"double"},     {"math"}, {NULL}, FLAG_double_math, 0 },
    { {"double"},   {"circle"}, {NULL}, FLAG_double_circle_area, 0 },
    { {"double"},  {"average"}, {NULL}, FLAG_double_average, 0 },
    { {"double"}, {"interest"}, {NULL}, FLAG_double_compound_interest, 0 },
    { {"double"},{"pythagoras"},{NULL}, FLAG_double_pythagoras, 0 },
    { {"double"},     {"temp"}, {NULL}, FLAG_double_temp_convert, 0 },
    { {"double"},     {"sqrt"}, {NULL}, FLAG_double_sqrt, 0 },
    { {"double"},    {"power"}, {NULL}, FLAG_double_power, 0 },
    { {"double"},{"volume", "sphere"},{NULL},FLAG_double_volume_sphere, 0 },
    { {"double"}, {"discount"}, {NULL}, FLAG_double_discount, 0 },
    { {"double"},{"simple", "interest"},{NULL},FLAG_double_simple_interest, 0 },
    { {"double"},      {"bmi"}, {NULL}, FLAG_double_bmi, 0 },
    { {"double"},   {"stddev"}, {NULL}, FLAG_double_standard_deviation, 0 },
    { {"double"},{"standard", "deviation"},{NULL},FLAG_double_standard_deviation, 0 },
    { {"double"},  {"kinetic"}, {NULL}, FLAG_double_kinetic_energy, 0 },
    { {"double"},   {"energy"}, {NULL}, FLAG_double_kinetic_energy, 0 },

    /* string length / data structures */
    { {"string"},   {"length"}, {NULL}, FLAG_string_length, 0 },
    { {"string"},   {"laenge"}, {NULL}, FLAG_string_length, 0 },
    { {"zeichen"},  {"laenge"}, {NULL}, FLAG_string_length, 0 },
    { {"stack"},              {NULL}, {NULL}, FLAG_stack, 0 },
    { {"push"},        {"pop"}, {NULL}, FLAG_stack, 0 },
    { {"queue"},              {NULL}, {NULL}, FLAG_queue, 0 },
    { {"enqueue"}, {"dequeue"}, {NULL}, FLAG_queue, 0 },
    { {"insertion"},  {"sort"}, {NULL}, FLAG_insertion_sort, 0 },

    /* tools / converters */
    { {"calculator"},         {NULL}, {NULL}, FLAG_calculator, 0 },
    { {"calc"},       {"repl"}, {NULL}, FLAG_calculator, 0 },
    { {"unit"},    {"convert"}, {NULL}, FLAG_unit_converter, 0 },
    { {"unit"},  {"converter"}, {NULL}, FLAG_unit_converter, 0 },
    { {"unit"},  {"umrechnen"}, {NULL}, FLAG_unit_converter, 0 },
    { {"unit"},  {"umwandeln"}, {NULL}, FLAG_unit_converter, 0 },
    { {"einheit"}, {"convert"}, {NULL}, FLAG_unit_converter, 0 },
    { {"einheit"},{"converter"},{NULL}, FLAG_unit_converter, 0 },
    { {"einheit"},{"umrechnen"},{NULL},FLAG_unit_converter, 0 },
    { {"einheit"},{"umwandeln"},{NULL},FLAG_unit_converter, 0 },
    { {"rock"},{"paper", "scissors"},{NULL},FLAG_rock_paper_scissors, 0 },

    /* patterns / pyramids */
    { {"pyramid"},  {"number"}, {NULL}, FLAG_pyramid, 0 },
    { {"pyramid"},    {"zahl"}, {NULL}, FLAG_pyramid, 0 },
    { {"pyramid"}, {"pattern"}, {NULL}, FLAG_pyramid, 0 },
    { {"pyramid"},  {"muster"}, {NULL}, FLAG_pyramid, 0 },
    { {"pyramid"},  {"height"}, {NULL}, FLAG_pyramid, 0 },
    { {"pyramid"},   {"hoehe"}, {NULL}, FLAG_pyramid, 0 },
    { {"pyramid"},    {"rows"}, {NULL}, FLAG_pyramid, 0 },
    { {"pyramid"},  {"zeilen"}, {NULL}, FLAG_pyramid, 0 },
    { {"pyramide"}, {"number"}, {NULL}, FLAG_pyramid, 0 },
    { {"pyramide"},   {"zahl"}, {NULL}, FLAG_pyramid, 0 },
    { {"pyramide"},{"pattern"}, {NULL}, FLAG_pyramid, 0 },
    { {"pyramide"}, {"muster"}, {NULL}, FLAG_pyramid, 0 },
    { {"pyramide"}, {"height"}, {NULL}, FLAG_pyramid, 0 },
    { {"pyramide"},  {"hoehe"}, {NULL}, FLAG_pyramid, 0 },
    { {"pyramide"},   {"rows"}, {NULL}, FLAG_pyramid, 0 },
    { {"pyramide"}, {"zeilen"}, {NULL}, FLAG_pyramid, 0 },
    { {"temperature"},{"table"},{NULL}, FLAG_temperature_table, 0 },
    { {"temperature"},{"tabelle"},{NULL},FLAG_temperature_table, 0 },
    { {"temperatur"},{"table"}, {NULL}, FLAG_temperature_table, 0 },
    { {"temperatur"},{"tabelle"},{NULL},FLAG_temperature_table, 0 },

    /* temp converter menu / sort stats */
    { {"temperature"},{"converter"},{NULL},FLAG_temp_converter_menu, 0 },
    { {"temperature"},{"umrechnen"},{NULL},FLAG_temp_converter_menu, 0 },
    { {"temperatur"},{"convert"},{NULL},FLAG_temp_converter_menu, 0 },
    { {"temperatur"},{"converter"},{NULL},FLAG_temp_converter_menu, 0 },
    { {"temperatur"},{"umrechnen"},{NULL},FLAG_temp_converter_menu, 0 },
    { {"sort"},      {"stats"}, {NULL}, FLAG_sort_stats, 0 },
    { {"sort"}, {"statistics"}, {NULL}, FLAG_sort_stats, 0 },
    { {"sort"},  {"statistik"}, {NULL}, FLAG_sort_stats, 0 },
    { {"sort"},    {"analyze"}, {NULL}, FLAG_sort_stats, 0 },
    { {"sort"},    {"analyse"}, {NULL}, FLAG_sort_stats, 0 },
    { {"sort"},        {"min"}, {NULL}, FLAG_sort_stats, 0 },
    { {"sort"},        {"max"}, {NULL}, FLAG_sort_stats, 0 },
    { {"sort"},    {"average"}, {NULL}, FLAG_sort_stats, 0 },
    { {"sort"},        {"avg"}, {NULL}, FLAG_sort_stats, 0 },

    /* analyzers */
    { {"analyze"},  {"string"}, {NULL}, FLAG_string_analyzer, 0 },
    { {"analyze"},    {"text"}, {NULL}, FLAG_string_analyzer, 0 },
    { {"analyze"},{"zeichenkette"},{NULL},FLAG_string_analyzer, 0 },
    { {"analyse"},  {"string"}, {NULL}, FLAG_string_analyzer, 0 },
    { {"analyse"},    {"text"}, {NULL}, FLAG_string_analyzer, 0 },
    { {"analyse"},{"zeichenkette"},{NULL},FLAG_string_analyzer, 0 },
    { {"analyze"},  {"number"}, {NULL}, FLAG_number_analyzer, 0 },
    { {"analyze"},    {"zahl"}, {NULL}, FLAG_number_analyzer, 0 },
    { {"analyse"},  {"number"}, {NULL}, FLAG_number_analyzer, 0 },
    { {"analyse"},    {"zahl"}, {NULL}, FLAG_number_analyzer, 0 },
    { {"analyzer"}, {"number"}, {NULL}, FLAG_number_analyzer, 0 },
    { {"analyzer"},   {"zahl"}, {NULL}, FLAG_number_analyzer, 0 },
    { {"analyser"}, {"number"}, {NULL}, FLAG_number_analyzer, 0 },
    { {"analyser"},   {"zahl"}, {NULL}, FLAG_number_analyzer, 0 },

    /* filter / generator / menu */
    { {"filter"},   {"number"}, {NULL}, FLAG_filter_numbers, 0 },
    { {"filter"},  {"numbers"}, {NULL}, FLAG_filter_numbers, 0 },
    { {"filter"},   {"zahlen"}, {NULL}, FLAG_filter_numbers, 0 },
    { {"filter"},   {"values"}, {NULL}, FLAG_filter_numbers, 0 },
    { {"filter"},    {"werte"}, {NULL}, FLAG_filter_numbers, 0 },
    { {"random"},{"generator"}, {NULL}, FLAG_random_generator, 0 },
    { {"random"},{"generieren"},{NULL}, FLAG_random_generator, 0 },
    { {"zufall"},{"generator"}, {NULL}, FLAG_random_generator, 0 },
    { {"zufall"},{"generieren"},{NULL}, FLAG_random_generator, 0 },
    { {"math"},       {"menu"}, {NULL}, FLAG_math_menu, 0 },
    { {"math"},      {"menue"}, {NULL}, FLAG_math_menu, 0 },
    { {"math"},    {"rechner"}, {NULL}, FLAG_math_menu, 0 },
    { {"mathe"},      {"menu"}, {NULL}, FLAG_math_menu, 0 },
    { {"mathe"},     {"menue"}, {NULL}, FLAG_math_menu, 0 },
    { {"mathe"},   {"rechner"}, {NULL}, FLAG_math_menu, 0 },
    { {"quiz"},       {"game"}, {NULL}, FLAG_quiz_game, 0 },
    { {"quiz"},      {"spiel"}, {NULL}, FLAG_quiz_game, 0 },
    { {"frage"},      {"game"}, {NULL}, FLAG_quiz_game, 0 },
    { {"frage"},     {"spiel"}, {NULL}, FLAG_quiz_game, 0 },

    /* bmi / statistics */
    { {"bmi"},                {NULL}, {NULL}, FLAG_bmi_calculator, 0 },
    { {"body"},{"mass", "index"},{NULL},FLAG_bmi_calculator, 0 },
    { {"koerper"},{"mass", "index"},{NULL},FLAG_bmi_calculator, 0 },
    { {"statistics"},  {"all"}, {NULL}, FLAG_statistics_suite, 0 },
    { {"statistics"},{"suite"}, {NULL}, FLAG_statistics_suite, 0 },
    { {"statistics"}, {"alle"}, {NULL}, FLAG_statistics_suite, 0 },
    { {"statistics"}, {"full"}, {NULL}, FLAG_statistics_suite, 0 },
    { {"statistics"},{"komplett"},{NULL},FLAG_statistics_suite, 0 },
    { {"statistik"},   {"all"}, {NULL}, FLAG_statistics_suite, 0 },
    { {"statistik"}, {"suite"}, {NULL}, FLAG_statistics_suite, 0 },
    { {"statistik"},  {"alle"}, {NULL}, FLAG_statistics_suite, 0 },
    { {"statistik"},  {"full"}, {NULL}, FLAG_statistics_suite, 0 },
    { {"statistik"},{"komplett"},{NULL},FLAG_statistics_suite, 0 },

    /* data structures */
    { {"linked"},     {"list"}, {NULL}, FLAG_linked_list, 0 },
    { {"binary"},{"tree", "search"},{NULL},FLAG_binary_search_tree, 0 },
    { {"binaer"},{"tree", "search"},{NULL},FLAG_binary_search_tree, 0 },
    { {"tree"},  {"traversal"}, {NULL}, FLAG_tree_traversal, 0 },
    { {"tree"},    {"inorder"}, {NULL}, FLAG_tree_traversal, 0 },
    { {"tree"},   {"preorder"}, {NULL}, FLAG_tree_traversal, 0 },
    { {"tree"},  {"postorder"}, {NULL}, FLAG_tree_traversal, 0 },
    { {"graph"},       {"bfs"}, {NULL}, FLAG_graph_bfs_dfs, 0 },
    { {"graph"},       {"dfs"}, {NULL}, FLAG_graph_bfs_dfs, 0 },
    { {"graph"},  {"traverse"}, {NULL}, FLAG_graph_bfs_dfs, 0 },
    { {"graph"}, {"durchlauf"}, {NULL}, FLAG_graph_bfs_dfs, 0 },
    { {"graf"},        {"bfs"}, {NULL}, FLAG_graph_bfs_dfs, 0 },
    { {"graf"},        {"dfs"}, {NULL}, FLAG_graph_bfs_dfs, 0 },
    { {"graf"},   {"traverse"}, {NULL}, FLAG_graph_bfs_dfs, 0 },
    { {"graf"},  {"durchlauf"}, {NULL}, FLAG_graph_bfs_dfs, 0 },
    { {"n"},         {"queen"}, {NULL}, FLAG_n_queens, 0 },
    { {"eight"},     {"queen"}, {NULL}, FLAG_n_queens, 0 },
    { {"acht"},      {"queen"}, {NULL}, FLAG_n_queens, 0 },
    { {"sudoku"},             {NULL}, {NULL}, FLAG_sudoku, 0 },
    { {"sodoku"},   {"solver"}, {NULL}, FLAG_sudoku, 0 },
    { {"levenshtein"},{"distance"},{NULL},FLAG_levenshtein, 0 },
    { {"edit"},   {"distance"}, {NULL}, FLAG_levenshtein, 0 },

    /* maze */
    { {"maze"},   {"generate"}, {NULL}, FLAG_maze_generator, 0 },
    { {"maze"}, {"generieren"}, {NULL}, FLAG_maze_generator, 0 },
    { {"maze"},   {"erzeugen"}, {NULL}, FLAG_maze_generator, 0 },
    { {"labyrinth"},{"generate"},{NULL}, FLAG_maze_generator, 0 },
    { {"labyrinth"},{"generieren"},{NULL},FLAG_maze_generator, 0 },
    { {"labyrinth"},{"erzeugen"},{NULL},FLAG_maze_generator, 0 },
    { {"irrgarten"},{"generate"},{NULL},FLAG_maze_generator, 0 },
    { {"irrgarten"},{"generieren"},{NULL},FLAG_maze_generator, 0 },
    { {"irrgarten"},{"erzeugen"},{NULL},FLAG_maze_generator, 0 },
    { {"maze"},      {"solve"}, {NULL}, FLAG_maze_solver, 0 },
    { {"maze"},     {"solver"}, {NULL}, FLAG_maze_solver, 0 },
    { {"maze"},     {"loesen"}, {NULL}, FLAG_maze_solver, 0 },
    { {"maze"},     {"lösung"}, {NULL}, FLAG_maze_solver, 0 },
    { {"labyrinth"}, {"solve"}, {NULL}, FLAG_maze_solver, 0 },
    { {"labyrinth"},{"solver"}, {NULL}, FLAG_maze_solver, 0 },
    { {"labyrinth"},{"loesen"}, {NULL}, FLAG_maze_solver, 0 },
    { {"labyrinth"},{"lösung"}, {NULL}, FLAG_maze_solver, 0 },
    { {"irrgarten"}, {"solve"}, {NULL}, FLAG_maze_solver, 0 },
    { {"irrgarten"},{"solver"}, {NULL}, FLAG_maze_solver, 0 },
    { {"irrgarten"},{"loesen"}, {NULL}, FLAG_maze_solver, 0 },
    { {"irrgarten"},{"lösung"}, {NULL}, FLAG_maze_solver, 0 },

    /* math / science */
    { {"monte"},     {"carlo"}, {NULL}, FLAG_monte_carlo, 0 },
    { {"pi"},     {"estimate"}, {NULL}, FLAG_monte_carlo, 0 },
    { {"matrix"}, {"multiply"}, {NULL}, FLAG_matrix_mul, 0 },
    { {"matrize"},{"multiply"}, {NULL}, FLAG_matrix_mul, 0 },
    { {"matrix"},{"transpose"}, {NULL}, FLAG_matrix_transpose, 0 },
    { {"matrix"},{"transponieren"},{NULL},FLAG_matrix_transpose, 0 },
    { {"numerical"},{"integrate"},{NULL},FLAG_numerical_integration, 0 },
    { {"numerical"},{"integration"},{NULL},FLAG_numerical_integration, 0 },
    { {"numerical"},{"integral"},{NULL},FLAG_numerical_integration, 0 },
    { {"numerisch"},{"integrate"},{NULL},FLAG_numerical_integration, 0 },
    { {"numerisch"},{"integration"},{NULL},FLAG_numerical_integration, 0 },
    { {"numerisch"},{"integral"},{NULL},FLAG_numerical_integration, 0 },
    { {"numeric"},{"integrate"},{NULL},FLAG_numerical_integration, 0 },
    { {"numeric"},{"integration"},{NULL},FLAG_numerical_integration, 0 },
    { {"numeric"},{"integral"},{NULL},FLAG_numerical_integration, 0 },
    { {"complex"},  {"number"}, {NULL}, FLAG_complex_numbers, 0 },
    { {"komplex"},  {"number"}, {NULL}, FLAG_complex_numbers, 0 },
    { {"linear"},{"regression"},{NULL}, FLAG_linear_regression, 0 },
    { {"linear"},  {"regress"}, {NULL}, FLAG_linear_regression, 0 },
    { {"linreg"},{"regression"},{NULL},FLAG_linear_regression, 0 },
    { {"linreg"},  {"regress"}, {NULL}, FLAG_linear_regression, 0 },
    { {"base"},    {"convert"}, {NULL}, FLAG_base_converter, 0 },
    { {"base"},  {"converter"}, {NULL}, FLAG_base_converter, 0 },
    { {"base"},  {"umwandeln"}, {NULL}, FLAG_base_converter, 0 },
    { {"base"},    {"konvert"}, {NULL}, FLAG_base_converter, 0 },
    { {"basis"},   {"convert"}, {NULL}, FLAG_base_converter, 0 },
    { {"basis"}, {"converter"}, {NULL}, FLAG_base_converter, 0 },
    { {"basis"}, {"umwandeln"}, {NULL}, FLAG_base_converter, 0 },
    { {"basis"},   {"konvert"}, {NULL}, FLAG_base_converter, 0 },
    { {"frequency"},{"analyze"},{NULL}, FLAG_freq_analysis, 0 },
    { {"frequency"},{"analyse"},{NULL},FLAG_freq_analysis, 0 },
    { {"frequency"}, {"count"}, {NULL}, FLAG_freq_analysis, 0 },
    { {"frequency"},{"zaehle"}, {NULL}, FLAG_freq_analysis, 0 },
    { {"frequency"},{"analysis"},{NULL},FLAG_freq_analysis, 0 },
    { {"haeufigkeit"},{"analyze"},{NULL},FLAG_freq_analysis, 0 },
    { {"haeufigkeit"},{"analyse"},{NULL},FLAG_freq_analysis, 0 },
    { {"haeufigkeit"},{"count"},{NULL},FLAG_freq_analysis, 0 },
    { {"haeufigkeit"},{"zaehle"},{NULL},FLAG_freq_analysis, 0 },
    { {"haeufigkeit"},{"analysis"},{NULL},FLAG_freq_analysis, 0 },
    { {"frequenz"},{"analyze"}, {NULL}, FLAG_freq_analysis, 0 },
    { {"frequenz"},{"analyse"}, {NULL}, FLAG_freq_analysis, 0 },
    { {"frequenz"},  {"count"}, {NULL}, FLAG_freq_analysis, 0 },
    { {"frequenz"}, {"zaehle"}, {NULL}, FLAG_freq_analysis, 0 },
    { {"frequenz"},{"analysis"},{NULL}, FLAG_freq_analysis, 0 },
    { {"freq"},    {"analyze"}, {NULL}, FLAG_freq_analysis, 0 },
    { {"freq"},    {"analyse"}, {NULL}, FLAG_freq_analysis, 0 },
    { {"freq"},      {"count"}, {NULL}, FLAG_freq_analysis, 0 },
    { {"freq"},     {"zaehle"}, {NULL}, FLAG_freq_analysis, 0 },
    { {"freq"},   {"analysis"}, {NULL}, FLAG_freq_analysis, 0 },

    /* shuffle / random */
    { {"shuffle"},            {NULL}, {NULL}, FLAG_shuffle, 0 },
    { {"mischen"},            {NULL}, {NULL}, FLAG_shuffle, 0 },
    { {"random"},  {"permute"}, {NULL}, FLAG_shuffle, 0 },
    { {"weighted"}, {"random"}, {NULL}, FLAG_weighted_random, 0 },
    { {"weighted"}, {"zufall"}, {NULL}, FLAG_weighted_random, 0 },
    { {"gewichtet"},{"random"}, {NULL}, FLAG_weighted_random, 0 },
    { {"gewichtet"},{"zufall"}, {NULL}, FLAG_weighted_random, 0 },
    { {"ascii"},     {"table"}, {NULL}, FLAG_ascii_table, 0 },
    { {"ascii"},   {"tabelle"}, {NULL}, FLAG_ascii_table, 0 },
    { {"ascii"},    {"format"}, {NULL}, FLAG_ascii_table, 0 },

    /* tools / apps */
    { {"password"},   {"card"}, {NULL}, FLAG_password_card, 0 },
    { {"password"},  {"karte"}, {NULL}, FLAG_password_card, 0 },
    { {"password"},{"generate"},{NULL}, FLAG_password_card, 0 },
    { {"password"},{"generator"},{NULL},FLAG_password_card, 0 },
    { {"passwort"},   {"card"}, {NULL}, FLAG_password_card, 0 },
    { {"passwort"},  {"karte"}, {NULL}, FLAG_password_card, 0 },
    { {"passwort"},{"generate"},{NULL},FLAG_password_card, 0 },
    { {"passwort"},{"generator"},{NULL},FLAG_password_card, 0 },
    { {"passwd"},     {"card"}, {NULL}, FLAG_password_card, 0 },
    { {"passwd"},    {"karte"}, {NULL}, FLAG_password_card, 0 },
    { {"passwd"}, {"generate"}, {NULL}, FLAG_password_card, 0 },
    { {"passwd"},{"generator"}, {NULL}, FLAG_password_card, 0 },
    { {"chess"},   {"problem"}, {NULL}, FLAG_chess_problem, 0 },
    { {"chess"},     {"board"}, {NULL}, FLAG_chess_problem, 0 },
    { {"chess"},     {"brett"}, {NULL}, FLAG_chess_problem, 0 },
    { {"chess"},{"exponential"},{NULL}, FLAG_chess_problem, 0 },
    { {"schach"},  {"problem"}, {NULL}, FLAG_chess_problem, 0 },
    { {"schach"},    {"board"}, {NULL}, FLAG_chess_problem, 0 },
    { {"schach"},    {"brett"}, {NULL}, FLAG_chess_problem, 0 },
    { {"schach"},{"exponential"},{NULL},FLAG_chess_problem, 0 },
    { {"rice"},    {"problem"}, {NULL}, FLAG_chess_problem, 0 },
    { {"rice"},      {"board"}, {NULL}, FLAG_chess_problem, 0 },
    { {"rice"},      {"brett"}, {NULL}, FLAG_chess_problem, 0 },
    { {"rice"},{"exponential"}, {NULL}, FLAG_chess_problem, 0 },
    { {"reis"},    {"problem"}, {NULL}, FLAG_chess_problem, 0 },
    { {"reis"},      {"board"}, {NULL}, FLAG_chess_problem, 0 },
    { {"reis"},      {"brett"}, {NULL}, FLAG_chess_problem, 0 },
    { {"reis"},{"exponential"}, {NULL}, FLAG_chess_problem, 0 },
    { {"repl"},        {"run"}, {NULL}, FLAG_shell_repl, 0 },
    { {"repl"},    {"execute"}, {NULL}, FLAG_shell_repl, 0 },
    { {"repl"}, {"ausfuehren"}, {NULL}, FLAG_shell_repl, 0 },
    { {"repl"},  {"ausführen"}, {NULL}, FLAG_shell_repl, 0 },
    { {"terminal"},    {"run"}, {NULL}, FLAG_shell_repl, 0 },
    { {"terminal"},{"execute"}, {NULL}, FLAG_shell_repl, 0 },
    { {"terminal"},{"ausfuehren"},{NULL},FLAG_shell_repl, 0 },
    { {"terminal"},{"ausführen"},{NULL},FLAG_shell_repl, 0 },
    { {"shell"},       {"run"}, {NULL}, FLAG_shell_repl, 0 },
    { {"shell"},   {"running"}, {NULL}, FLAG_shell_repl, 0 },
    { {"shell"},   {"execute"}, {NULL}, FLAG_shell_repl, 0 },
    { {"shell"},   {"command"}, {NULL}, FLAG_shell_repl, 0 },
    { {"shell"},{"ausfuehren"}, {NULL}, FLAG_shell_repl, 0 },
    { {"shell"}, {"ausführen"}, {NULL}, FLAG_shell_repl, 0 },
    { {"shell"},    {"befehl"}, {NULL}, FLAG_shell_repl, 0 },
    { {"interactive"}, {"run"}, {NULL}, FLAG_shell_repl, 0 },
    { {"interactive"},{"running"},{NULL},FLAG_shell_repl, 0 },
    { {"interactive"},{"execute"},{NULL},FLAG_shell_repl, 0 },
    { {"interactive"},{"command"},{NULL},FLAG_shell_repl, 0 },
    { {"interactive"},{"ausfuehren"},{NULL},FLAG_shell_repl, 0 },
    { {"interactive"},{"ausführen"},{NULL},FLAG_shell_repl, 0 },
    { {"interactive"},{"befehl"},{NULL},FLAG_shell_repl, 0 },

    /* web / SDL / system */
    { {"webserver"},          {NULL}, {NULL}, FLAG_webserver, 0 },
    { {"http"},               {NULL}, {NULL}, FLAG_webserver, 0 },
    { {"web"},      {"server"}, {NULL}, FLAG_webserver, 0 },
    { {"sdl"},        {"open"}, {NULL}, FLAG_sdl_window, 0 },
    { {"sdl"},      {"window"}, {NULL}, FLAG_sdl_window, 0 },
    { {"sdl"},     {"fenster"}, {NULL}, FLAG_sdl_window, 0 },
    { {"sdl"},       {"pixel"}, {NULL}, FLAG_sdl_window, 0 },
    { {"sdl"},      {"grafik"}, {NULL}, FLAG_sdl_window, 0 },
    { {"sdl"},    {"graphics"}, {NULL}, FLAG_sdl_window, 0 },
    { {"screen"},     {"open"}, {NULL}, FLAG_sdl_window, 0 },
    { {"screen"},   {"window"}, {NULL}, FLAG_sdl_window, 0 },
    { {"screen"},  {"fenster"}, {NULL}, FLAG_sdl_window, 0 },
    { {"screen"},    {"pixel"}, {NULL}, FLAG_sdl_window, 0 },
    { {"screen"},   {"grafik"}, {NULL}, FLAG_sdl_window, 0 },
    { {"screen"}, {"graphics"}, {NULL}, FLAG_sdl_window, 0 },
    { {"bildschirm"}, {"open"}, {NULL}, FLAG_sdl_window, 0 },
    { {"bildschirm"},{"window"},{NULL}, FLAG_sdl_window, 0 },
    { {"bildschirm"},{"fenster"},{NULL},FLAG_sdl_window, 0 },
    { {"bildschirm"},{"pixel"}, {NULL}, FLAG_sdl_window, 0 },
    { {"bildschirm"},{"grafik"},{NULL},FLAG_sdl_window, 0 },
    { {"bildschirm"},{"graphics"},{NULL},FLAG_sdl_window, 0 },
    { {"sdl"},      {"button"}, {NULL}, FLAG_sdl_button, 0 },
    { {"sdl"},       {"knopf"}, {NULL}, FLAG_sdl_button, 0 },
    { {"sdl"},       {"click"}, {NULL}, FLAG_sdl_button, 0 },
    { {"gui"},      {"button"}, {NULL}, FLAG_sdl_button, 0 },
    { {"gui"},       {"knopf"}, {NULL}, FLAG_sdl_button, 0 },
    { {"gui"},       {"click"}, {NULL}, FLAG_sdl_button, 0 },
    { {"gadget"},   {"button"}, {NULL}, FLAG_sdl_button, 0 },
    { {"gadget"},    {"knopf"}, {NULL}, FLAG_sdl_button, 0 },
    { {"gadget"},    {"click"}, {NULL}, FLAG_sdl_button, 0 },
    { {"thread"},             {NULL}, {NULL}, FLAG_thread, 0 },
    { {"nebenlauf"},          {NULL}, {NULL}, FLAG_thread, 0 },
    { {"parallel"},    {"run"}, {NULL}, FLAG_thread, 0 },
    { {"scheduler"},          {NULL}, {NULL}, FLAG_scheduler, 0 },
    { {"planer"},             {NULL}, {NULL}, FLAG_scheduler, 0 },
    { {"task"},   {"schedule"}, {NULL}, FLAG_scheduler, 0 },
    { {"shell"},      {"exec"}, {NULL}, FLAG_shell_exec, 0 },
    { {"command"},    {"exec"}, {NULL}, FLAG_shell_exec, 0 },
    { {"command"},     {"run"}, {NULL}, FLAG_shell_exec, 0 },
    { {"command"},{"ausführen"},{NULL}, FLAG_shell_exec, 0 },
    { {"command"},{"ausfuehren"},{NULL},FLAG_shell_exec, 0 },
    { {"befehl"},     {"exec"}, {NULL}, FLAG_shell_exec, 0 },
    { {"befehl"},      {"run"}, {NULL}, FLAG_shell_exec, 0 },
    { {"befehl"},{"ausführen"}, {NULL}, FLAG_shell_exec, 0 },
    { {"befehl"},{"ausfuehren"},{NULL},FLAG_shell_exec, 0 },
    { {"kommando"},   {"exec"}, {NULL}, FLAG_shell_exec, 0 },
    { {"kommando"},    {"run"}, {NULL}, FLAG_shell_exec, 0 },
    { {"kommando"},{"ausführen"},{NULL},FLAG_shell_exec, 0 },
    { {"kommando"},{"ausfuehren"},{NULL},FLAG_shell_exec, 0 },

    /* json / crypto / bluetooth */
    { {"json"},               {NULL}, {NULL}, FLAG_json, 0 },
    { {"javascript"},         {NULL}, {NULL}, FLAG_json, 0 },
    { {"parse"},      {"json"}, {NULL}, FLAG_json, 0 },
    { {"crypto"},             {NULL}, {NULL}, FLAG_crypto, 0 },
    { {"encrypt"},            {NULL}, {NULL}, FLAG_crypto, 0 },
    { {"decrypt"},            {NULL}, {NULL}, FLAG_crypto, 0 },
    { {"verschlüssel"},       {NULL}, {NULL}, FLAG_crypto, 0 },
    { {"chiffre"},            {NULL}, {NULL}, FLAG_crypto, 0 },
    { {"cipher"},             {NULL}, {NULL}, FLAG_crypto, 0 },
    { {"bluetooth"},          {NULL}, {NULL}, FLAG_bluetooth_ble, 0 },
    { {"ble"},                {NULL}, {NULL}, FLAG_bluetooth_ble, 0 },
    { {"bluetooth"},   {"low"}, {NULL}, FLAG_bluetooth_ble, 0 },

    /* serial / GPIO / GPS */
    { {"serial"},     {"port"}, {NULL}, FLAG_serial_rs232, 0 },
    { {"serial"},{"schnittstelle"},{NULL},FLAG_serial_rs232, 0 },
    { {"serial"},{"interface"}, {NULL}, FLAG_serial_rs232, 0 },
    { {"rs232"},      {"port"}, {NULL}, FLAG_serial_rs232, 0 },
    { {"rs232"},{"schnittstelle"},{NULL},FLAG_serial_rs232, 0 },
    { {"rs232"}, {"interface"}, {NULL}, FLAG_serial_rs232, 0 },
    { {"seriell"},    {"port"}, {NULL}, FLAG_serial_rs232, 0 },
    { {"seriell"},{"schnittstelle"},{NULL},FLAG_serial_rs232, 0 },
    { {"seriell"},{"interface"},{NULL},FLAG_serial_rs232, 0 },
    { {"gpio"},               {NULL}, {NULL}, FLAG_gpio, 0 },
    { {"general"}, {"purpose"}, {NULL}, FLAG_gpio, 0 },
    { {"gps"},                {NULL}, {NULL}, FLAG_gps, 0 },
    { {"navigation"},         {NULL}, {NULL}, FLAG_gps, 0 },
    { {"global"}, {"position"}, {NULL}, FLAG_gps, 0 },

    /* timer / sound / joystick / mouse */
    { {"date"},       {"time"}, {NULL}, FLAG_timer_date, 0 },
    { {"date"},       {"zeit"}, {NULL}, FLAG_timer_date, 0 },
    { {"date"},        {"uhr"}, {NULL}, FLAG_timer_date, 0 },
    { {"date"},      {"clock"}, {NULL}, FLAG_timer_date, 0 },
    { {"datum"},      {"time"}, {NULL}, FLAG_timer_date, 0 },
    { {"datum"},      {"zeit"}, {NULL}, FLAG_timer_date, 0 },
    { {"datum"},       {"uhr"}, {NULL}, FLAG_timer_date, 0 },
    { {"datum"},     {"clock"}, {NULL}, FLAG_timer_date, 0 },
    { {"kalender"},   {"time"}, {NULL}, FLAG_timer_date, 0 },
    { {"kalender"},   {"zeit"}, {NULL}, FLAG_timer_date, 0 },
    { {"kalender"},    {"uhr"}, {NULL}, FLAG_timer_date, 0 },
    { {"kalender"},  {"clock"}, {NULL}, FLAG_timer_date, 0 },
    { {"calendar"},   {"time"}, {NULL}, FLAG_timer_date, 0 },
    { {"calendar"},   {"zeit"}, {NULL}, FLAG_timer_date, 0 },
    { {"calendar"},    {"uhr"}, {NULL}, FLAG_timer_date, 0 },
    { {"calendar"},  {"clock"}, {NULL}, FLAG_timer_date, 0 },
    { {"sdl"},        {"play"}, {NULL}, FLAG_sdl_sound, 0 },
    { {"sdl"},     {"spielen"}, {NULL}, FLAG_sdl_sound, 0 },
    { {"sdl"},   {"abspielen"}, {NULL}, FLAG_sdl_sound, 0 },
    { {"sdl"},  {"wiedergabe"}, {NULL}, FLAG_sdl_sound, 0 },
    { {"sound"},      {"play"}, {NULL}, FLAG_sdl_sound, 0 },
    { {"sound"},   {"spielen"}, {NULL}, FLAG_sdl_sound, 0 },
    { {"sound"}, {"abspielen"}, {NULL}, FLAG_sdl_sound, 0 },
    { {"sound"},{"wiedergabe"}, {NULL}, FLAG_sdl_sound, 0 },
    { {"audio"},      {"play"}, {NULL}, FLAG_sdl_sound, 0 },
    { {"audio"},   {"spielen"}, {NULL}, FLAG_sdl_sound, 0 },
    { {"audio"}, {"abspielen"}, {NULL}, FLAG_sdl_sound, 0 },
    { {"audio"},{"wiedergabe"}, {NULL}, FLAG_sdl_sound, 0 },
    { {"ton"},        {"play"}, {NULL}, FLAG_sdl_sound, 0 },
    { {"ton"},     {"spielen"}, {NULL}, FLAG_sdl_sound, 0 },
    { {"ton"},   {"abspielen"}, {NULL}, FLAG_sdl_sound, 0 },
    { {"ton"},  {"wiedergabe"}, {NULL}, FLAG_sdl_sound, 0 },
    { {"sdl"},    {"joystick"}, {NULL}, FLAG_sdl_joystick, 0 },
    { {"sdl"},        {"axis"}, {NULL}, FLAG_sdl_joystick, 0 },
    { {"sdl"},       {"achse"}, {NULL}, FLAG_sdl_joystick, 0 },
    { {"sdl"},      {"button"}, {NULL}, FLAG_sdl_joystick, 0 },
    { {"gamepad"},{"joystick"}, {NULL}, FLAG_sdl_joystick, 0 },
    { {"gamepad"},    {"axis"}, {NULL}, FLAG_sdl_joystick, 0 },
    { {"gamepad"},   {"achse"}, {NULL}, FLAG_sdl_joystick, 0 },
    { {"gamepad"},  {"button"}, {NULL}, FLAG_sdl_joystick, 0 },
    { {"controller"},{"joystick"},{NULL},FLAG_sdl_joystick, 0 },
    { {"controller"}, {"axis"}, {NULL}, FLAG_sdl_joystick, 0 },
    { {"controller"},{"achse"}, {NULL}, FLAG_sdl_joystick, 0 },
    { {"controller"},{"button"},{NULL}, FLAG_sdl_joystick, 0 },
    { {"sdl"},       {"mouse"}, {NULL}, FLAG_sdl_mouse, 0 },
    { {"sdl"},        {"maus"}, {NULL}, FLAG_sdl_mouse, 0 },
    { {"sdl"},     {"pointer"}, {NULL}, FLAG_sdl_mouse, 0 },
    { {"sdl"},      {"cursor"}, {NULL}, FLAG_sdl_mouse, 0 },
    { {"mouse"},     {"mouse"}, {NULL}, FLAG_sdl_mouse, 0 },
    { {"mouse"},      {"maus"}, {NULL}, FLAG_sdl_mouse, 0 },
    { {"mouse"},   {"pointer"}, {NULL}, FLAG_sdl_mouse, 0 },
    { {"mouse"},    {"cursor"}, {NULL}, FLAG_sdl_mouse, 0 },
    { {"maus"},      {"mouse"}, {NULL}, FLAG_sdl_mouse, 0 },
    { {"maus"},       {"maus"}, {NULL}, FLAG_sdl_mouse, 0 },
    { {"maus"},    {"pointer"}, {NULL}, FLAG_sdl_mouse, 0 },
    { {"maus"},     {"cursor"}, {NULL}, FLAG_sdl_mouse, 0 },

    /* fractal / cluster / misc */
    { {"fractal"},            {NULL}, {NULL}, FLAG_fractal, 0 },
    { {"mandelbrot"},         {NULL}, {NULL}, FLAG_fractal, 0 },
    { {"julia"},              {NULL}, {NULL}, FLAG_fractal, 0 },
    { {"fraktal"},     {"set"}, {NULL}, FLAG_fractal, 0 },
    { {"cluster"}, {"compute"}, {NULL}, FLAG_cluster_3x1, 0 },
    { {"cluster"}, {"rechnen"}, {NULL}, FLAG_cluster_3x1, 0 },
    { {"cluster"},     {"3x1"}, {NULL}, FLAG_cluster_3x1, 0 },
    { {"cluster"},  {"arbeit"}, {NULL}, FLAG_cluster_3x1, 0 },
    { {"worker"},  {"compute"}, {NULL}, FLAG_cluster_3x1, 0 },
    { {"worker"},  {"rechnen"}, {NULL}, FLAG_cluster_3x1, 0 },
    { {"worker"},      {"3x1"}, {NULL}, FLAG_cluster_3x1, 0 },
    { {"worker"},   {"arbeit"}, {NULL}, FLAG_cluster_3x1, 0 },
    { {"verteilt"},{"compute"}, {NULL}, FLAG_cluster_3x1, 0 },
    { {"verteilt"},{"rechnen"}, {NULL}, FLAG_cluster_3x1, 0 },
    { {"verteilt"},    {"3x1"}, {NULL}, FLAG_cluster_3x1, 0 },
    { {"verteilt"}, {"arbeit"}, {NULL}, FLAG_cluster_3x1, 0 },
    { {"distributed"},{"compute"},{NULL},FLAG_cluster_3x1, 0 },
    { {"distributed"},{"rechnen"},{NULL},FLAG_cluster_3x1, 0 },
    { {"distributed"}, {"3x1"}, {NULL}, FLAG_cluster_3x1, 0 },
    { {"distributed"},{"arbeit"},{NULL},FLAG_cluster_3x1, 0 },
    { {"reload"},             {NULL}, {NULL}, FLAG_reload, 0 },
    { {"module"},             {NULL}, {NULL}, FLAG_reload, 0 },
    { {"hot"},        {"swap"}, {NULL}, FLAG_reload, 0 },
    { {"neu"},       {"laden"}, {NULL}, FLAG_reload, 0 },
    { {"coordinate"},   {"xy"}, {NULL}, FLAG_coordinate_grid, 0 },
    { {"coordinate"},   {"2d"}, {NULL}, FLAG_coordinate_grid, 0 },
    { {"coordinate"},{"position"},{NULL},FLAG_coordinate_grid, 0 },
    { {"coordinate"},{"raster"},{NULL},FLAG_coordinate_grid, 0 },
    { {"koordinate"},   {"xy"}, {NULL}, FLAG_coordinate_grid, 0 },
    { {"koordinate"},   {"2d"}, {NULL}, FLAG_coordinate_grid, 0 },
    { {"koordinate"},{"position"},{NULL},FLAG_coordinate_grid, 0 },
    { {"koordinate"},{"raster"},{NULL},FLAG_coordinate_grid, 0 },
    { {"grid"},         {"xy"}, {NULL}, FLAG_coordinate_grid, 0 },
    { {"grid"},         {"2d"}, {NULL}, FLAG_coordinate_grid, 0 },
    { {"grid"},   {"position"}, {NULL}, FLAG_coordinate_grid, 0 },
    { {"grid"},     {"raster"}, {NULL}, FLAG_coordinate_grid, 0 },
    { {"gitter"},       {"xy"}, {NULL}, FLAG_coordinate_grid, 0 },
    { {"gitter"},       {"2d"}, {NULL}, FLAG_coordinate_grid, 0 },
    { {"gitter"}, {"position"}, {NULL}, FLAG_coordinate_grid, 0 },
    { {"gitter"},   {"raster"}, {NULL}, FLAG_coordinate_grid, 0 },
    { {"turmite"},            {NULL}, {NULL}, FLAG_turmite, 0 },
    { {"turmite"},     {"ant"}, {NULL}, FLAG_turmite, 0 },
    { {"crossword"},          {NULL}, {NULL}, FLAG_crossword, 0 },
    { {"kreuzwort"},          {NULL}, {NULL}, FLAG_crossword, 0 },
    { {"rätsel"},             {NULL}, {NULL}, FLAG_crossword, 0 },
    { {"word"},     {"puzzle"}, {NULL}, FLAG_crossword, 0 },
    { {"linter"},             {NULL}, {NULL}, FLAG_linter, 0 },
    { {"lint"},               {NULL}, {NULL}, FLAG_linter, 0 },
    { {"code"},      {"check"}, {NULL}, FLAG_linter, 0 },
    { {"static"},  {"analyze"}, {NULL}, FLAG_linter, 0 },

    /* small emitters */
    { {"hello"},     {"world"}, {NULL}, FLAG_hello_world, 0 },
    { {"string"},     {"find"}, {"max", "largest"}, FLAG_string_find, 0 },
    { {"string"},    {"split"}, {NULL}, FLAG_string_split, 0 },
    { {"text"},      {"split"}, {NULL}, FLAG_string_split, 0 },
    { {"switch"},             {NULL}, {"string"}, FLAG_switch_demo, 0 },
    { {"type"},    {"convert"}, {NULL}, FLAG_type_convert, 0 },
    { {"type"}, {"conversion"}, {NULL}, FLAG_type_convert, 0 },
    { {"type"},  {"umwandeln"}, {NULL}, FLAG_type_convert, 0 },
    { {"type"},       {"cast"}, {NULL}, FLAG_type_convert, 0 },
    { {"factorial"},  {"loop"}, {NULL}, FLAG_iterative_factorial, 0 },
    { {"factorial"},{"iterative"},{NULL},FLAG_iterative_factorial, 0 },
    { {"factorial"},{"schleife"},{NULL},FLAG_iterative_factorial, 0 },
    { {"factorial"}, {"while"}, {NULL}, FLAG_iterative_factorial, 0 },
    { {"fakult"},     {"loop"}, {NULL}, FLAG_iterative_factorial, 0 },
    { {"fakult"},{"iterative"}, {NULL}, FLAG_iterative_factorial, 0 },
    { {"fakult"}, {"schleife"}, {NULL}, FLAG_iterative_factorial, 0 },
    { {"fakult"},    {"while"}, {NULL}, FLAG_iterative_factorial, 0 },
    { {"random"},     {"walk"}, {NULL}, FLAG_random_walk, 0 },
    { {"bar"},       {"chart"}, {NULL}, FLAG_bar_chart, 0 },
    { {"bar"},       {"graph"}, {NULL}, FLAG_bar_chart, 0 },
    { {"bar"},    {"diagramm"}, {NULL}, FLAG_bar_chart, 0 },
    { {"hanoi"},              {NULL}, {NULL}, FLAG_hanoi_tower, 0 },
    { {"tower"},     {"hanoi"}, {NULL}, FLAG_hanoi_tower, 0 },
    { {"turm"},      {"hanoi"}, {NULL}, FLAG_hanoi_tower, 0 },
    { {"ascii"},       {"art"}, {NULL}, FLAG_ascii_art, 0 },
    { {"number"},     {"word"}, {NULL}, FLAG_number_to_words, 0 },
    { {"number"},    {"words"}, {NULL}, FLAG_number_to_words, 0 },
    { {"number"},     {"wort"}, {NULL}, FLAG_number_to_words, 0 },
    { {"number"},   {"worter"}, {NULL}, FLAG_number_to_words, 0 },
    { {"number"},     {"text"}, {NULL}, FLAG_number_to_words, 0 },
    { {"zahl"},       {"word"}, {NULL}, FLAG_number_to_words, 0 },
    { {"zahl"},      {"words"}, {NULL}, FLAG_number_to_words, 0 },
    { {"zahl"},       {"wort"}, {NULL}, FLAG_number_to_words, 0 },
    { {"zahl"},     {"worter"}, {NULL}, FLAG_number_to_words, 0 },
    { {"zahl"},       {"text"}, {NULL}, FLAG_number_to_words, 0 },
    { {"temperature"},{"table"},{NULL}, FLAG_temperature_table, 0 },
    { {"temperature"},{"tabelle"},{NULL},FLAG_temperature_table, 0 },
    { {"temperatur"},{"table"}, {NULL}, FLAG_temperature_table, 0 },
    { {"temperatur"},{"tabelle"},{NULL},FLAG_temperature_table, 0 },
    { {"loop"},       {"demo"}, {NULL}, FLAG_loop_demo, 0 },
    { {"loop"},   {"beispiel"}, {NULL}, FLAG_loop_demo, 0 },
    { {"loop"},    {"example"}, {NULL}, FLAG_loop_demo, 0 },
    { {"time"},       {"demo"}, {NULL}, FLAG_time, 0 },
    { {"time"},   {"beispiel"}, {NULL}, FLAG_time, 0 },
    { {"time"},    {"example"}, {NULL}, FLAG_time, 0 },
    { {"time"},    {"current"}, {NULL}, FLAG_time, 0 },
    { {"zeit"},       {"demo"}, {NULL}, FLAG_time, 0 },
    { {"zeit"},   {"beispiel"}, {NULL}, FLAG_time, 0 },
    { {"zeit"},    {"example"}, {NULL}, FLAG_time, 0 },
    { {"zeit"},    {"current"}, {NULL}, FLAG_time, 0 },
    { {"uhr"},        {"demo"}, {NULL}, FLAG_time, 0 },
    { {"uhr"},    {"beispiel"}, {NULL}, FLAG_time, 0 },
    { {"uhr"},     {"example"}, {NULL}, FLAG_time, 0 },
    { {"uhr"},     {"current"}, {NULL}, FLAG_time, 0 },
    { {"clock"},      {"demo"}, {NULL}, FLAG_time, 0 },
    { {"clock"},  {"beispiel"}, {NULL}, FLAG_time, 0 },
    { {"clock"},   {"example"}, {NULL}, FLAG_time, 0 },
    { {"clock"},   {"current"}, {NULL}, FLAG_time, 0 },
    { {"shell"},       {"arg"}, {NULL}, FLAG_shell_args, 0 },
    { {"shell"}, {"parameter"}, {NULL}, FLAG_shell_args, 0 },
    { {"shell"},   {"command"}, {NULL}, FLAG_shell_args, 0 },
    { {"shell"},    {"befehl"}, {NULL}, FLAG_shell_args, 0 },
};

#define PATTERN_TABLE_SIZE (sizeof(pattern_table) / sizeof(pattern_table[0]))

static int match_pattern_entry(const char *buf, const PatternTableEntry *e) {
    if (e->any_of[0] == NULL) return 0;
    int any_match = 0;
    for (int i = 0; i < PATTERN_MAX_KEYWORDS && e->any_of[i]; i++) {
        if (_has_word_lowered(buf, e->any_of[i])) { any_match = 1; break; }
    }
    if (!any_match) return 0;
    if (e->must_have[0] != NULL) {
        for (int i = 0; i < PATTERN_MAX_KEYWORDS && e->must_have[i]; i++) {
            if (!_has_word_lowered(buf, e->must_have[i])) return 0;
        }
    }
    for (int i = 0; i < PATTERN_MAX_EXCLUDES && e->excludes[i]; i++) {
        if (_has_word_lowered(buf, e->excludes[i])) return 0;
    }
    return 1;
}

static void match_pattern_table(const char *buf, TaskProfile *task) {
    for (size_t i = 0; i < PATTERN_TABLE_SIZE; i++) {
        const PatternTableEntry *e = &pattern_table[i];
        if (match_pattern_entry(buf, e)) {
            TF_SET(task, e->flag);
            if (e->clear_algorithm) TF_CLR(task, FLAG_algorithm);
        }
    }
}

/* ── Helper functions for complex patterns ────────────────────────────── */

static void detect_sum_range_pattern(const char *prompt, const char *buf, TaskProfile *task) {
    (void)prompt;
    if ((_has_word_lowered(buf, "sum") || _has_word_lowered(buf, "summe")) && _has_word_lowered(buf, "to") && TF_ISSET(task, FLAG_literals)) {
        TF_SET(task, FLAG_sum_range);
        if (task->num_literals >= 1) task->sum_range_n = task->literals[0];
        else task->sum_range_n = 100;
        TF_CLR(task, FLAG_algorithm);
        TF_CLR(task, FLAG_operation);
    }
    if ((_has_word_lowered(buf, "count") || _has_word_lowered(buf, "z\u00e4hle") || _has_word_lowered(buf, "zaehle"))
        && _has_word_lowered(buf, "to") && TF_ISSET(task, FLAG_literals)) {
        TF_SET(task, FLAG_sum_range);
        if (task->num_literals >= 2) task->sum_range_n = task->literals[1];
        else if (task->num_literals >= 1) task->sum_range_n = task->literals[0];
        else task->sum_range_n = 100;
        TF_CLR(task, FLAG_algorithm);
        TF_CLR(task, FLAG_operation);
    }
}

static void detect_print_even_pattern(const char *buf, TaskProfile *task) {
    if ((_has_word_lowered(buf, "even") || _has_word_lowered(buf, "gerade")) && TF_ISSET(task, FLAG_literals)
        && !_has_word_lowered(buf, "ungerade") && !_has_word_lowered(buf, "odd")) {
        TF_SET(task, FLAG_print_even);
        if (task->num_literals >= 1) task->print_even_n = task->literals[0];
        else task->print_even_n = 100;
        TF_CLR(task, FLAG_algorithm);
    }
}

static void detect_find_max_pattern(const char *buf, TaskProfile *task) {
    if (_has_word_lowered(buf, "find") && (_has_word_lowered(buf, "max") || _has_word_lowered(buf, "largest") || _has_word_lowered(buf, "greatest") || _has_word_lowered(buf, "gr\u00f6\u00dft"))) {
        TF_SET(task, FLAG_find_max);
        if (task->num_literals >= 1) task->find_max_count = task->literals[0];
        else task->find_max_count = DEFAULT_ELEMENT_COUNT;
        TF_SET(task, FLAG_input);
        TF_CLR(task, FLAG_algorithm);
    }
}

static void detect_fib_seq_pattern(const char *buf, TaskProfile *task) {
    if (_has_word_lowered(buf, "fib") && TF_ISSET(task, FLAG_literals)) {
        TF_SET(task, FLAG_fib_seq);
        if (task->num_literals >= 1) task->fib_seq_n = task->literals[0];
        else task->fib_seq_n = 10;
        TF_CLR(task, FLAG_algorithm);
    }
}

static void detect_countdown_pattern(const char *buf, TaskProfile *task) {
    if ((_has_word_lowered(buf, "countdown") || strstr(buf, "count down")) && TF_ISSET(task, FLAG_literals)) {
        TF_SET(task, FLAG_countdown_from);
        if (task->num_literals >= 1) task->countdown_start = task->literals[0];
        else task->countdown_start = 10;
        TF_CLR(task, FLAG_algorithm);
    }
}

static void detect_input_sort_pattern(const char *buf, TaskProfile *task) {
    (void)buf;
    if (TF_ISSET(task, FLAG_sort) && (TF_ISSET(task, FLAG_input) || TF_ISSET(task, FLAG_descending))) {
        TF_SET(task, FLAG_input_sort);
        if (task->input_count > 0) task->input_sort_count = task->input_count;
        else if (task->num_literals >= 1) task->input_sort_count = task->literals[0];
        else task->input_sort_count = DEFAULT_ELEMENT_COUNT;
        TF_CLR(task, FLAG_algorithm);
    }
}

static void detect_median_pattern(const char *buf, TaskProfile *task) {
    (void)buf;
    if (TF_ISSET(task, FLAG_median) && TF_ISSET(task, FLAG_input)) {
        if (task->input_count > 0) task->median_count = task->input_count;
        else if (task->num_literals >= 1) task->median_count = task->literals[0];
        else task->median_count = DEFAULT_ELEMENT_COUNT;
        TF_CLR(task, FLAG_algorithm);
    }
}

static void detect_primes_pattern(const char *prompt, const char *buf, TaskProfile *task) {
    if (TF_ISSET(task, FLAG_primes) && !TF_ISSET(task, FLAG_input) && task->num_literals == 0) {
        if (_has_word_lowered(buf, "first") || _has_word_lowered(buf, "erste") || _has_word_lowered(buf, "ersten")) {
            int nums2[MAX_NUMS];
            int n2 = extract_numbers(prompt, nums2, MAX_NUMS);
            if (n2 > 0) { task->num_literals = n2; TF_SET(task, FLAG_literals); task->literals[0] = nums2[0]; }
        }
    }
    if (TF_ISSET(task, FLAG_primes) && (_has_word_lowered(buf, "up") || _has_word_lowered(buf, "bis")) && _has_word_lowered(buf, "to") && task->num_literals == 0) {
        int nums2[MAX_NUMS];
        int n2 = extract_numbers(prompt, nums2, MAX_NUMS);
        if (n2 > 0) { task->num_literals = n2; TF_SET(task, FLAG_literals); task->literals[0] = nums2[0]; }
    }
}

static void detect_fib_first_pattern(const char *prompt, const char *buf, TaskProfile *task) {
    if (TF_ISSET(task, FLAG_fib_seq) && task->num_literals == 0 &&
        (_has_word_lowered(buf, "first") || _has_word_lowered(buf, "erste") || _has_word_lowered(buf, "ersten"))) {
        int nums2[MAX_NUMS];
        int n2 = extract_numbers(prompt, nums2, MAX_NUMS);
        if (n2 > 0) { task->num_literals = n2; TF_SET(task, FLAG_literals); task->literals[0] = nums2[0]; }
        if (task->num_literals >= 1) task->fib_seq_n = task->literals[0];
        else task->fib_seq_n = 10;
    }
}

static void detect_from_sum_pattern(const char *buf, TaskProfile *task) {
    if (_has_word_lowered(buf, "from") || _has_word_lowered(buf, "von")) {
        if (TF_ISSET(task, FLAG_sum_range) && task->num_literals >= 2) {
            TF_SET(task, FLAG_sum_range);
            task->sum_range_n = task->literals[1];
        }
    }
}

static void detect_countdown_from_pattern(const char *prompt, const char *buf, TaskProfile *task) {
    if (TF_ISSET(task, FLAG_countdown_from) && task->num_literals == 0 &&
        (_has_word_lowered(buf, "up") || _has_word_lowered(buf, "bis"))) {
        int nums2[MAX_NUMS];
        int n2 = extract_numbers(prompt, nums2, MAX_NUMS);
        if (n2 > 0) { task->num_literals = n2; TF_SET(task, FLAG_literals); task->literals[0] = nums2[0]; }
        if (task->num_literals >= 1) task->countdown_start = task->literals[0];
        else task->countdown_start = 10;
    }
}

static void detect_input_fact_pattern(TaskProfile *task) {
    if (TF_ISSET(task, FLAG_input) && TF_ISSET(task, FLAG_factorial)) {
        TF_SET(task, FLAG_input_fact);
        TF_CLR(task, FLAG_algorithm);
    }
}

static void detect_fann_pattern(const char *buf, TaskProfile *task) {
    if (_has_word_lowered(buf, "fann") || _has_word_lowered(buf, "neural") || (_has_word_lowered(buf, "network") && _has_word_lowered(buf, "ai"))) {
        if (_has_word_lowered(buf, "train") || _has_word_lowered(buf, "learn")) {
            TF_SET(task, FLAG_fann_create);
            TF_SET(task, FLAG_fann_train);
            TF_CLR(task, FLAG_algorithm);
        } else if (_has_word_lowered(buf, "run") || _has_word_lowered(buf, "predict") || _has_word_lowered(buf, "infer")) {
            TF_SET(task, FLAG_fann_run);
            TF_CLR(task, FLAG_algorithm);
        } else if (_has_word_lowered(buf, "create") || _has_word_lowered(buf, "make") || _has_word_lowered(buf, "generate")) {
            TF_SET(task, FLAG_fann_create);
            TF_CLR(task, FLAG_algorithm);
        } else {
            TF_SET(task, FLAG_fann_create);
            TF_SET(task, FLAG_fann_train);
            TF_SET(task, FLAG_fann_run);
            TF_CLR(task, FLAG_algorithm);
        }
    }
}

/* ── Refactored enhanced pattern detection ────────────────────────────── */

static void parse_task_detect_enhanced_patterns(const char *prompt, const char *buf, TaskProfile *task) {
    detect_sum_range_pattern(prompt, buf, task);
    detect_print_even_pattern(buf, task);
    detect_find_max_pattern(buf, task);
    detect_fib_seq_pattern(buf, task);
    detect_countdown_pattern(buf, task);
    detect_input_sort_pattern(buf, task);
    detect_median_pattern(buf, task);
    detect_primes_pattern(prompt, buf, task);
    detect_fib_first_pattern(prompt, buf, task);
    detect_from_sum_pattern(buf, task);
    detect_countdown_from_pattern(prompt, buf, task);
    detect_input_fact_pattern(task);

    match_pattern_table(buf, task);

    if (TF_ISSET(task, FLAG_double_circle_area)) TF_CLR(task, FLAG_circle_area);
    if (TF_ISSET(task, FLAG_double_temp_convert)) TF_CLR(task, FLAG_temp_convert);
    if (TF_ISSET(task, FLAG_random_generator)) TF_CLR(task, FLAG_random);

    detect_fann_pattern(buf, task);
}

static void add_standard_constants(Function *f) {
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
}

static int try_learned_pattern(Program *prog, const char *prompt, char *desc, int desc_size) {
    int lscore;
    int lidx = match_learned_pattern(prompt, &lscore);
    if (lidx >= 0 && emit_learned_pattern(prog, lidx)) {
        if (desc) SNPRINTF_CHECK(desc, desc_size, "learned pattern: %s (score: %d)", learned_patterns[lidx].id, lscore);
        return 1;
    }
    return 0;
}

static int try_learned_pattern_info(Program *prog, const char *prompt, const char **out_id, int *out_score) {
    int lscore;
    int lidx = match_learned_pattern(prompt, &lscore);
    if (lidx >= 0 && emit_learned_pattern(prog, lidx)) {
        if (out_id) *out_id = learned_patterns[lidx].id;
        if (out_score) *out_score = lscore;
        return 1;
    }
    return 0;
}

static void ensure_exit(Function *f, int last_step);

/* ── Inherited array operations for multi-step generation ─────────────── */

static int emit_sort_inherited_array(Function *f, TaskProfile *task, int last_step) {
    const char *zv[] = {"0"};
    add_standard_constants(f);
    char cvs[16];
    SNPRINTF_CHECK(cvs, sizeof(cvs), "%d", task->inherit_count > 0 ? task->inherit_count : DEFAULT_ELEMENT_COUNT);
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
    char ln[PATH_BUF_SIZE];
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

static int emit_print_inherited_array(Function *f, TaskProfile *task, int last_step) {
    const char *zv[] = {"0"};
    add_standard_constants(f);
    char cvs[16];
    SNPRINTF_CHECK(cvs, sizeof(cvs), "%d", task->inherit_count > 0 ? task->inherit_count : DEFAULT_ELEMENT_COUNT);
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
    char ln[PATH_BUF_SIZE];
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

static int emit_print_max_inherited(Function *f, TaskProfile *task, int last_step) {
    const char *zv[] = {"0"};
    add_standard_constants(f);
    char cvs[16];
    SNPRINTF_CHECK(cvs, sizeof(cvs), "%d", task->inherit_count > 0 ? task->inherit_count : DEFAULT_ELEMENT_COUNT);
    const char *cv[] = {cvs};
    add_var_to_func(f, "int64", "count", 1, cv, 1);
    add_var_to_func(f, "int64", "temp", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "int64", "realind", 1, zv, 1);
    add_var_to_func(f, "int64", "a", 1, zv, 1);
    func_append(f, "\t// print largest");
    func_append(f, "\t(count one - temp :=)");
    func_append(f, "\t(temp * int64_size realind :=)");
    char ln[PATH_BUF_SIZE];
    SNPRINTF_CHECK(ln, sizeof(ln), "\t(%s [ realind ] a =)", task->inherit_var);
    func_append(f, ln);
    func_append(f, "\t(a + zero a :=)");
    func_append(f, "\t(a :print_i !)");
    func_append(f, "\t(:print_n !)");
    ensure_exit(f, last_step);
    return 1;
}

static int emit_print_min_inherited(Function *f, TaskProfile *task, int last_step) {
    const char *zv[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "int64", "realind", 1, zv, 1);
    add_var_to_func(f, "int64", "a", 1, zv, 1);
    char ln[PATH_BUF_SIZE];
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

static int parse_task_finalize(const char *prompt, const char *buf, TaskProfile *task) {
    SNPRINTF_CHECK(task->title, sizeof(task->title), "%s", prompt);

    int has_any = 0;
    for (int i = 0; i < TF_WORDS; i++)
        has_any |= (task->flags[i] != 0);

    /* Negation post-processing: clear flags if keyword is negated */
    if (TF_ISSET(task, FLAG_sort) && _is_negated_lowered(buf, "sort")) TF_CLR(task, FLAG_sort);
    if (TF_ISSET(task, FLAG_loop) && _is_negated_lowered(buf, "loop")) TF_CLR(task, FLAG_loop);
    if (TF_ISSET(task, FLAG_condition) && _is_negated_lowered(buf, "if")) TF_CLR(task, FLAG_condition);
    if (TF_ISSET(task, FLAG_input) && _is_negated_lowered(buf, "input")) TF_CLR(task, FLAG_input);
    if (TF_ISSET(task, FLAG_output) && _is_negated_lowered(buf, "print")) TF_CLR(task, FLAG_output);
    if (TF_ISSET(task, FLAG_sum) && _is_negated_lowered(buf, "sum")) TF_CLR(task, FLAG_sum);
    if (TF_ISSET(task, FLAG_average) && _is_negated_lowered(buf, "average")) TF_CLR(task, FLAG_average);
    if (TF_ISSET(task, FLAG_power) && _is_negated_lowered(buf, "power")) TF_CLR(task, FLAG_power);
    if (TF_ISSET(task, FLAG_gcd) && _is_negated_lowered(buf, "gcd")) TF_CLR(task, FLAG_gcd);

    return has_any;
}

int parse_task(const char *prompt, TaskProfile *task) {
    char buf[MAX_PROMPT];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", prompt);
    to_lowercase(buf);
    SNPRINTF_CHECK(task->prompt, sizeof(task->prompt), "%s", prompt);
    SNPRINTF_CHECK(task->type, sizeof(task->type), "%s", "int64");

    parse_task_detect_type(buf, task);
    parse_task_detect_literals(prompt, buf, task);
    parse_task_detect_io(buf, task);
    parse_task_detect_flags(buf, task);
    parse_task_detect_enhanced_patterns(prompt, buf, task);
    return parse_task_finalize(prompt, buf, task);
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

    if (TF_ISSET(task, FLAG_sort) && task->inherit_var[0])
        return emit_sort_inherited_array(f, task, last_step);

    if (TF_ISSET(task, FLAG_output) && task->inherit_var[0] && !TF_ISSET(task, FLAG_max) && !TF_ISSET(task, FLAG_min))
        return emit_print_inherited_array(f, task, last_step);

    if (TF_ISSET(task, FLAG_max) && task->inherit_var[0])
        return emit_print_max_inherited(f, task, last_step);

    if (TF_ISSET(task, FLAG_min) && task->inherit_var[0])
        return emit_print_min_inherited(f, task, last_step);

    if (TF_ISSET(task, FLAG_print_var)) {
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

    char line[WORD_BUF_SIZE], vname[64], vstr[16];
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

static int try_smart_multi_step(Program *prog, const char *prompt, char *desc, int desc_size) {
    char steps[MAX_STEPS][MAX_PROMPT];
    int num_steps = split_prompt_steps(prompt, steps);
    if (num_steps <= 1) return 0;

    char inherited_names[64][WORD_BUF_SIZE] = {{0}};
    char inherited_types[64][64] = {{0}};
    int inherited_counts[64] = {0};
    int num_inherited = 0;
    for (int i = 0; i < num_steps; i++) {
        c_printf(ANSI_CYAN, "  Step %d/%d: %s\n", i + 1, num_steps, steps[i]);
        int learned_emitted = 0;
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
                return 0;
            }
            task.skip_input = (i > 0);
            task.suppress_output = (i < num_steps - 1);
            for (int iv = 0; iv < num_inherited; iv++) {
                SNPRINTF_CHECK(task.inherit_var_names[task.num_inherit_vars], sizeof(task.inherit_var_names[task.num_inherit_vars]), "%.*s", (int)sizeof(task.inherit_var_names[task.num_inherit_vars]) - 1, inherited_names[iv]);
                SNPRINTF_CHECK(task.inherit_var_types[task.num_inherit_vars], sizeof(task.inherit_var_types[task.num_inherit_vars]), "%.*s", (int)sizeof(task.inherit_var_types[task.num_inherit_vars]) - 1, inherited_types[iv]);
                task.inherit_var_counts[task.num_inherit_vars] = inherited_counts[iv];
                task.num_inherit_vars++;
            }
            if (i > 0) {
                for (int iv = 0; iv < num_inherited; iv++) {
                    if (inherited_counts[iv] > 1 && !task.inherit_var[0]) {
                        SNPRINTF_CHECK(task.inherit_var, sizeof(task.inherit_var), "%.*s", (int)sizeof(task.inherit_var) - 1, inherited_names[iv]);
                        task.inherit_count = inherited_counts[iv];
                    }
                }
            }
            if (num_steps > 0) {
                int vs_indices2[3];
                float vs_scores2[3];
                int vs_count2 = search_examples(steps[i], 3, vs_indices2, vs_scores2);
                if (vs_count2 > 0 && vs_scores2[0] > 0.3f) {
                    vs_boost_count = tokenize(example_docs[vs_indices2[0]].stem, vs_boost_tokens, 64);
                }
                llm_select_emitter(steps[i], &task);
            }
            generate_from_task(prog, &task, (i == num_steps - 1));
        }
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
    SNPRINTF_CHECK(desc, desc_size, "multi-step generation (%d steps)", num_steps);
    return 1;
}

static int try_smart_single_step(Program *prog, const char *prompt, char *desc, int desc_size) {
    TaskProfile task;
    memset(&task, 0, sizeof(task));
    if (!parse_task(prompt, &task)) return 0;

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
    return 0;
}

static int try_fallback_arithmetic(Program *prog, const char *prompt, char *desc, int desc_size) {
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
    int num_count = extract_numbers(buf, nums, 4);
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

int smart_generate(Program *prog, const char *prompt, char *desc, int desc_size) {
    init_embeddings();
    index_examples();
    load_learned_patterns();

    if (try_learned_pattern(prog, prompt, desc, desc_size))
        return 1;

    if (try_smart_multi_step(prog, prompt, desc, desc_size))
        return 1;

    if (try_smart_single_step(prog, prompt, desc, desc_size))
        return 1;

    return try_fallback_arithmetic(prog, prompt, desc, desc_size);
}

static void write_prog_header(FILE *f, const char *filename, Program *prog) {
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
}

static void sync_vars_from_body(Function *fn) {
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
                char type_buf[64], count_buf[64], name_buf[WORD_BUF_SIZE];
                char rest[4096];
                int n = sscanf(p + 5, "%63s %63s %255s %4095[^\n)]", type_buf, count_buf, name_buf, rest);
                if (n >= 3) {
                    for (int vi = 0; vi < fn->num_vars; vi++) {
                        if (strcmp(fn->vars[vi].name, name_buf) == 0 && n >= 4) {
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

static void write_function_body(FILE *f, Function *fn) {
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
                char vn[WORD_BUF_SIZE]; int vni = 0;
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

    write_prog_header(f, filename, prog);

    for (int fi = 0; fi < prog->num_funcs; fi++) {
        Function *fn = &prog->funcs[fi];
        fprintf(f, "(%s func)\n", fn->name);
        if (fn->has_vardef)
            fprintf(f, "\t#var ~ %s\n", fn->vardef_name);
        sync_vars_from_body(fn);
        write_function_body(f, fn);
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
        char kwbuf[PATH_BUF_SIZE];
        SNPRINTF_CHECK(kwbuf, sizeof(kwbuf), "%s", templates[i].keywords);
        to_lowercase(kwbuf);

        int score = 0;
        int num_kw = 0;
        char kwcopy[PATH_BUF_SIZE];
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
    char ppname[PATH_BUF_SIZE];
    const char *dot = strrchr(filename, '.');
    if (dot) {
        int len = dot - filename;
        SNPRINTF_CHECK(ppname, sizeof(ppname), "%.*s_pp.l1com", len, filename);
    } else {
        SNPRINTF_CHECK(ppname, sizeof(ppname), "%s_pp.l1com", filename);
    }
    const char *include_dir = getenv("L1VM_INCLUDE");
    if (!include_dir) {
        static char fallback[FULLPATH_BUF_SIZE];
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
    char l1pre_path[FULLPATH_BUF_SIZE];
    char l1com_path[FULLPATH_BUF_SIZE];
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

    char compname[PATH_BUF_SIZE];
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
    char desc[PATH_BUF_SIZE] = {0};
    if (smart_generate(prog, prompt, desc, sizeof(desc))) {
        c_printf(ANSI_GREEN, "Smart generated (plan): %s\n", desc);
        write_program(prog, filename);
        free_program(prog); free(prog);
        return 1;
    }

    // Learned patterns as fallback emitter
    load_learned_patterns();
    {
        const char *lid; int lsc;
        if (try_learned_pattern_info(prog, prompt, &lid, &lsc)) {
            c_printf(ANSI_GREEN, "Using learned pattern: %s (score: %d)\n", lid, lsc);
            write_program(prog, filename);
            free_program(prog); free(prog);
            return 1;
        }
    }

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
        char fname[WORD_BUF_SIZE];
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
        char fullpath[FULLPATH_BUF_SIZE];
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
    printf("  Save DSL rule:          brackets-code --learn-dsl <keyword> [options]\n");
    printf("                          Options: --token <type> <name> --include <file> --desc <text> --code <file.l1com>\n");
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
    char last_fname[WORD_BUF_SIZE] = {0};
    int has_last = 0;

    printf("Brackets Code Generator %s\n", VERSION_TXT);
    printf("Type /help for help, /exit to quit.\n");

#ifdef HAVE_READLINE
    rl_attempted_completion_function = cmd_completion;
    using_history();
    const char *histfile = getenv("HOME");
    char histpath[PATH_BUF_SIZE] = "";
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
                printf("Usage: /save <filename>\n");
                printf("Last generated was: %s\n", has_last ? last_fname : "(none)");
            } else if (!has_last) {
                printf("No code generated yet. Generate something first.\n");
            } else {
                char src[FULLPATH_BUF_SIZE], dst[FULLPATH_BUF_SIZE];
                SNPRINTF_CHECK(src, sizeof(src), "%s", last_fname);
                prepend_out_dir(fname, dst, sizeof(dst));
                FILE *sf = fopen(src, "r");
                if (!sf) {
                    printf("Cannot read source: %s\n", src);
                } else {
                    FILE *df = fopen(dst, "w");
                    if (!df) {
                        printf("Cannot write to: %s\n", dst);
                        fclose(sf);
                    } else {
                        char buf[4096];
                        size_t n;
                        while ((n = fread(buf, 1, sizeof(buf), sf)) > 0)
                            fwrite(buf, 1, n, df);
                        fclose(df);
                        fclose(sf);
                        printf("Saved: %s\n", dst);
                    }
                }
            }
            continue;
        }

        if (strncmp(prompt, "/learn", 6) == 0) {
            char lpath[FULLPATH_BUF_SIZE] = {0}, lkeywords[PATH_BUF_SIZE] = {0}, ldesc[PATH_BUF_SIZE] = {0};
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

        char fname[WORD_BUF_SIZE];
        prompt_to_filename(prompt, fname, sizeof(fname));
        generate_code(prompt, fname);
        SNPRINTF_CHECK(last_fname, sizeof(last_fname), "%s", fname);
        has_last = 1;
    }
    printf("\nBye!\n");
#ifdef HAVE_READLINE
    {
        const char *hf = getenv("HOME");
        if (hf) { char hp[PATH_BUF_SIZE]; SNPRINTF_CHECK(hp, sizeof(hp), "%s/.brackets-code_history", hf); write_history(hp); }
    }
#endif
}

#ifndef TEST_MODE
static int dispatch_subcommands(int argc, char *argv[]) {
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
        if (argc < 3) { fprintf(stderr, "Usage: %s --learn <file.l1com> [keywords] [description]\n", argv[0]); return 1; }
        ensure_learned_dir();
        return learn_from_file(argv[2], (argc > 3) ? argv[3] : "", (argc > 4) ? argv[4] : "") ? 0 : 1;
    }
    if (argc >= 2 && strcmp(argv[1], "--forget") == 0) {
        if (argc < 3) { fprintf(stderr, "Usage: %s --forget <pattern-id>\n", argv[0]); return 1; }
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
    if (argc >= 2 && strcmp(argv[1], "--learn-dsl") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: %s --learn-dsl <keyword> [options] [out-dir]\n", argv[0]);
            fprintf(stderr, "  Options:\n");
            fprintf(stderr, "    --token <type> <name>   Add a token (e.g. --token int64 n)\n");
            fprintf(stderr, "    --include <file>        Add an include (e.g. --include intr-func.l1h)\n");
            fprintf(stderr, "    --desc <text>           Set description\n");
            fprintf(stderr, "    --code <file>           Read code from .l1com file\n");
            fprintf(stderr, "  If no options given, saves an existing in-memory rule.\n");
            return 1;
        }
        const char *keyword = argv[2];
        const char *out_dir = "dsl";

        // Check if this is a new rule creation (has --token or --code)
        int has_flags = 0;
        for (int i = 3; i < argc; i++) {
            if (strcmp(argv[i], "--token") == 0 || strcmp(argv[i], "--code") == 0 ||
                strcmp(argv[i], "--include") == 0 || strcmp(argv[i], "--desc") == 0) {
                has_flags = 1;
                break;
            }
        }

        if (!has_flags) {
            // Legacy mode: save existing in-memory rule
            if (argc > 3) out_dir = argv[3];
            return learn_dsl(keyword, out_dir, NULL) ? 0 : 1;
        }

        // New rule creation mode
        DslRule new_rule;
        memset(&new_rule, 0, sizeof(DslRule));
        SNPRINTF_CHECK(new_rule.keyword, sizeof(new_rule.keyword), "%s", keyword);

        int i = 3;
        while (i < argc) {
            if (strcmp(argv[i], "--token") == 0 && i + 2 < argc) {
                if (new_rule.num_tokens < MAX_DSL_TOKENS) {
                    DslToken *tok = &new_rule.tokens[new_rule.num_tokens];
                    char tok_line[256];
                    SNPRINTF_CHECK(tok_line, sizeof(tok_line), "%s %s", argv[i+1], argv[i+2]);
                    if (parse_token_decl(tok_line, tok)) {
                        new_rule.num_tokens++;
                    } else {
                        fprintf(stderr, "Error: invalid token '%s %s'\n", argv[i+1], argv[i+2]);
                        return 1;
                    }
                }
                i += 3;
            } else if (strcmp(argv[i], "--include") == 0 && i + 1 < argc) {
                if (new_rule.num_includes < MAX_DSL_INCLUDES) {
                    SNPRINTF_CHECK(new_rule.includes[new_rule.num_includes],
                        sizeof(new_rule.includes[0]), "%s", argv[i+1]);
                    new_rule.num_includes++;
                }
                i += 2;
            } else if (strcmp(argv[i], "--desc") == 0 && i + 1 < argc) {
                SNPRINTF_CHECK(new_rule.desc, sizeof(new_rule.desc), "%s", argv[i+1]);
                i += 2;
            } else if (strcmp(argv[i], "--code") == 0 && i + 1 < argc) {
                if (!learn_dsl_load_code_file(argv[i+1], &new_rule)) return 1;
                i += 2;
            } else if (strcmp(argv[i], "--out-dir") == 0 && i + 1 < argc) {
                out_dir = argv[i+1];
                i += 2;
            } else {
                i++;
            }
        }

        if (new_rule.num_code_lines == 0) {
            fprintf(stderr, "Error: no code provided. Use --code <file> to supply code.\n");
            return 1;
        }

        return learn_dsl(keyword, out_dir, &new_rule) ? 0 : 1;
    }
    if (argc >= 2 && (strcmp(argv[1], "--search") == 0 || strcmp(argv[1], "-s") == 0)) {
        if (argc < 3) { fprintf(stderr, "Usage: %s --search <query>\n", argv[0]); return 1; }
        init_embeddings();
        index_examples();
        if (num_examples == 0) { printf("No examples indexed (directory '%s' not found).\n", EXAMPLE_DIR); return 1; }
        int indices[MAX_TOP_K]; float scores[MAX_TOP_K];
        int n = search_examples(argv[2], MAX_TOP_K, indices, scores);
        printf("Vector search results for: \"%s\"\n", argv[2]);
        printf("  Found %d matches in %d examples\n", n, num_examples);
        for (int i = 0; i < n; i++)
            printf("  %2d. %-40s (%.2f)\n", i+1, example_docs[indices[i]].stem, scores[i]);
        return 0;
    }
    if (argc >= 2 && (strcmp(argv[1], "--self-test") == 0 || strcmp(argv[1], "-t") == 0)) {
        validate_flag = 1;
        return self_test() ? 0 : 1;
    }
    if (argc >= 2 && (strcmp(argv[1], "--bash-completion") == 0 || strcmp(argv[1], "--completion") == 0)) {
        printf("_brackets_code_completions() {\n");
        printf("  local cur=\"${COMP_WORDS[COMP_CWORD]}\"\n");
        printf("            opts=\"--help -h --list -l --search --validate -v --self-test -t --verbose --dry-run --out-dir --l1vm-root --batch --bash-completion --learn --learn-dsl --forget --list-learned\"\n");
        printf("  COMPREPLY=($(compgen -W \"${opts}\" -- \"$cur\"))\n");
        printf("}\n");
        printf("complete -F _brackets_code_completions brackets-code\n");
        return 0;
    }
    return -1;
}

static int parse_global_flags(int argc, char *argv[], int *arg_idx) {
    while (*arg_idx < argc) {
        if (strcmp(argv[*arg_idx], "--verbose") == 0) { verbose_flag = 1; (*arg_idx)++; }
        else if (strcmp(argv[*arg_idx], "--dry-run") == 0) { dry_run_flag = 1; (*arg_idx)++; }
        else if (strcmp(argv[*arg_idx], "--out-dir") == 0) {
            if (*arg_idx + 1 < argc) { SNPRINTF_CHECK(out_dir, sizeof(out_dir), "%s", argv[*arg_idx + 1]); *arg_idx += 2; }
            else { fprintf(stderr, "Usage: ... --out-dir <directory>\n"); return 1; }
        }
        else if (strcmp(argv[*arg_idx], "--l1vm-root") == 0) {
            if (*arg_idx + 1 < argc) {
                SNPRINTF_CHECK(l1vm_root, sizeof(l1vm_root), "%s", argv[*arg_idx + 1]); *arg_idx += 2;
                struct stat st;
                if (stat(l1vm_root, &st) != 0 || !S_ISDIR(st.st_mode)) {
                    fprintf(stderr, "Error: --l1vm-root '%s' is not a valid directory\n", l1vm_root);
                    return 1;
                }
            } else { fprintf(stderr, "Usage: ... --l1vm-root <path>\n"); return 1; }
        }
        else if (strcmp(argv[*arg_idx], "--validate") == 0 || strcmp(argv[*arg_idx], "-v") == 0) { validate_flag = 1; (*arg_idx)++; }
        else break;
    }
    return 0;
}

static int run_single_prompt(int argc, char *argv[], int arg_idx) {
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
            char bfname[PATH_BUF_SIZE];
            prompt_to_filename(bline, bfname, sizeof(bfname));
            char bfull[FULLPATH_BUF_SIZE];
            prepend_out_dir(bfname, bfull, sizeof(bfull));
            int bgen = generate_code(bline, bfull);
            int bok = 0;
            if (bgen && validate_flag) { if (validate_code(bfull)) bok = 1; }
            else if (bgen) bok = 1;
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
    char pipe_buf[MAX_PROMPT];
    if (strcmp(prompt, "-") == 0) {
        size_t nread = 0;
        int ch;
        while ((ch = getchar()) != EOF && nread < MAX_PROMPT - 1)
            pipe_buf[nread++] = (char)ch;
        pipe_buf[nread] = '\0';
        prompt = pipe_buf;
    }
    char fname[WORD_BUF_SIZE];
    if (arg_idx < argc) {
        SNPRINTF_CHECK(fname, sizeof(fname), "%s", argv[arg_idx]);
        if (strstr(fname, "..") && (
            fname[0] == '.' ||
            strstr(fname, "/..") || strstr(fname, "\\..") ||
            strstr(fname, "..\\") || strstr(fname, "../") ||
            (strlen(fname) >= 2 && strcmp(fname + strlen(fname) - 2, "..") == 0)
        )) {
            c_printf(ANSI_RED, "Error: Filename must not contain '..' path components (path traversal blocked)\n");
            return 1;
        }
        if (!strstr(fname, ".l1com")) strncat(fname, ".l1com", sizeof(fname) - strlen(fname) - 1);
    } else {
        prompt_to_filename(prompt, fname, sizeof(fname));
    }
    char fullpath[FULLPATH_BUF_SIZE];
    prepend_out_dir(fname, fullpath, sizeof(fullpath));

    int validated = 0;
    int exit_code = 1;
    for (int retry = 0; retry < 3; retry++) {
        retry_seed = retry;
        int gen_ok = generate_code(prompt, fullpath);
        if (gen_ok && validate_flag) {
            if (validate_code(fullpath)) { validated = 1; exit_code = 0; break; }
            else c_printf(ANSI_YELLOW, "Validation failed, retrying with different code generation (attempt %d/3)...\n", retry + 1);
        } else if (gen_ok) { validated = 1; exit_code = 0; break; }
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

int main(int argc, char *argv[]) {
    use_color = isatty(STDOUT_FILENO);
    load_synonyms();
    dsl_load_rules("dsl");
    if (argc == 1) { interactive_mode(); return 0; }

    int sc = dispatch_subcommands(argc, argv);
    if (sc >= 0) return sc;

    int arg_idx = 1;
    if (parse_global_flags(argc, argv, &arg_idx) != 0)
        return 1;

    return run_single_prompt(argc, argv, arg_idx);
}
#endif
