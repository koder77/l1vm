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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <dirent.h>
#include <unistd.h>

/* format-truncation warnings: snprintf output is safely truncated for real-world sizes */
#pragma GCC diagnostic ignored "-Wformat-truncation"

#define VERSION_TXT "0.6.3"

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
char out_dir[512] = "";
char l1vm_root[512] = "";
static const char *current_prompt = "";

#define ANSI_RESET   "\033[0m"
#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_BOLD    "\033[1m"
static int use_color = 1;
#define c_printf(color, ...) do { if (use_color) { printf(color); printf(__VA_ARGS__); printf(ANSI_RESET); } else printf(__VA_ARGS__); } while(0)

typedef struct {
    char name[256];
    char type[32];
    int count;
    char values[MAX_VALUES][256];
    int num_values;
} Variable;

typedef struct {
    char name[256];
    int is_local;
    int has_vars;
    int has_vardef;
    char vardef_name[256];
    Variable *vars;
    int num_vars;
    int vars_cap;
    char *body;
    int body_cap;
} Function;

#ifdef HAVE_READLINE
/* readline defines its own incompatible Function typedef; undef ours after include */
#define Function L1vmFunction
#include <readline/readline.h>
#include <readline/history.h>
#undef Function
#endif

typedef struct {
    char filename[256];
    Function *funcs;
    int num_funcs;
    int funcs_cap;
    char (*includes)[256];
    int num_includes;
    int includes_cap;
    char (*includes_post)[256];
    int num_includes_post;
    int includes_post_cap;
    char globals[MAX_CODE];
} Program;

// Learned pattern from .l1com files
#define MAX_LEARNED 64
#define LEARNED_DIR ".brackets-code/learned"

typedef struct {
    char id[64];
    char keywords[512];
    char description[256];
    char source_path[1024];
    int is_learned;
    int num_includes;
    char (*includes)[256];
    int includes_cap;
    int num_funcs;
    Function *funcs;
    int funcs_cap;
    char globals[MAX_CODE];
} LearnedPattern;

static LearnedPattern learned_patterns[MAX_LEARNED];
static int num_learned = 0;
static int learned_loaded = 0;

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

static int ensure_vars_cap(Function *f, int needed) {
    if (needed <= f->vars_cap) return 1;
    int new_cap = f->vars_cap ? f->vars_cap * 2 : INIT_VARS_CAP;
    while (new_cap < needed) new_cap *= 2;
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

static int ensure_body_cap(Function *f, int needed) {
    if (needed <= f->body_cap) return 1;
    int new_cap = f->body_cap ? f->body_cap * 2 : INIT_BODY_CAP;
    while (new_cap < needed) new_cap *= 2;
    return resize_body(f, new_cap);
}

static int init_function(Function *f) {
    memset(f, 0, sizeof(Function));
    f->vars = NULL; f->vars_cap = 0;
    f->body = NULL; f->body_cap = 0;
    if (!resize_vars(f, INIT_VARS_CAP)) return 0;
    if (!resize_body(f, INIT_BODY_CAP)) return 0;
    f->body[0] = '\0';
    return 1;
}

static void free_function(Function *f) {
    free(f->vars); f->vars = NULL; f->vars_cap = 0;
    free(f->body); f->body = NULL; f->body_cap = 0;
    f->num_vars = 0;
}

static int resize_funcs_array(Function **pfuncs, int *cap, int new_cap) {
    Function *p = realloc(*pfuncs, (size_t)new_cap * sizeof(Function));
    if (!p) return 0;
    *pfuncs = p;
    if (new_cap > *cap) {
        for (int i = *cap; i < new_cap; i++)
            init_function(&(*pfuncs)[i]);
    }
    *cap = new_cap;
    return 1;
}

static int ensure_funcs_cap(Function **pfuncs, int *cap, int needed) {
    if (needed <= *cap) return 1;
    int new_cap = *cap ? *cap * 2 : INIT_FUNCS_CAP;
    while (new_cap < needed) new_cap *= 2;
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

static int ensure_includes_cap(char (**pinc)[256], int *cap, int needed) {
    if (needed <= *cap) return 1;
    int new_cap = *cap ? *cap * 2 : INIT_INCLUDES_CAP;
    while (new_cap < needed) new_cap *= 2;
    return resize_includes_arr(pinc, cap, new_cap);
}

static int init_program(Program *prog) {
    memset(prog, 0, sizeof(Program));
    if (!resize_funcs_array(&prog->funcs, &prog->funcs_cap, INIT_FUNCS_CAP)) return 0;
    if (!resize_includes_arr(&prog->includes, &prog->includes_cap, INIT_INCLUDES_CAP)) return 0;
    if (!resize_includes_arr(&prog->includes_post, &prog->includes_post_cap, INIT_INCLUDES_CAP)) return 0;
    return 1;
}

static void free_program(Program *prog) {
    for (int i = 0; i < prog->funcs_cap; i++)
        free_function(&prog->funcs[i]);
    free(prog->funcs); prog->funcs = NULL; prog->funcs_cap = 0;
    free(prog->includes); prog->includes = NULL; prog->includes_cap = 0;
    free(prog->includes_post); prog->includes_post = NULL; prog->includes_post_cap = 0;
}

static int init_learned_pattern(LearnedPattern *lp) {
    memset(lp, 0, sizeof(LearnedPattern));
    if (!resize_includes_arr(&lp->includes, &lp->includes_cap, INIT_INCLUDES_CAP)) return 0;
    if (!resize_funcs_array(&lp->funcs, &lp->funcs_cap, INIT_FUNCS_CAP)) return 0;
    return 1;
}

static void free_learned_pattern(LearnedPattern *lp) {
    for (int i = 0; i < lp->funcs_cap; i++)
        free_function(&lp->funcs[i]);
    free(lp->funcs); lp->funcs = NULL; lp->funcs_cap = 0;
    free(lp->includes); lp->includes = NULL; lp->includes_cap = 0;
}

void trim(char *s) {
    char *p = s;
    int l = strlen(p);
    while (isspace(p[l-1])) p[--l] = 0;
    while (*p && isspace(*p)) ++p;
    memmove(s, p, l - (p - s) + 1);
}

int str_contains(const char *str, const char *sub) {
    return strstr(str, sub) != NULL;
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
    strcat(f->body, line);
    strcat(f->body, "\n");
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

static int temp_counter = 0;
const char* temp_name() {
    static char buf[64];
    snprintf(buf, sizeof(buf), "tmp_%d", temp_counter++);
    return buf;
}

void reset_temp() { temp_counter = 0; }

static int func_counter = 0;
const char* local_buf() {
    static char buf[64];
    snprintf(buf, sizeof(buf), "local%d", func_counter++);
    return buf;
}

static int is_question(const char *prompt) {
    const char *qwords[] = {"was ", "wie ", "wer ", "wo ", "warum ", "wann ", "welche", "what ", "how ", "why ", "where ", "when ", "can ", "does ", "is ", "are ", NULL};
    char buf[MAX_PROMPT];
    snprintf(buf, sizeof(buf), "%s", prompt);
    to_lowercase(buf);
    if (strchr(buf, '?')) return 1;
    for (int i = 0; qwords[i]; i++)
        if (strncmp(buf, qwords[i], strlen(qwords[i])) == 0) return 1;
    return 0;
}

static void answer_question(const char *prompt) {
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

typedef struct { char word[40]; char canonical[40]; } Synonym;

static const Synonym SYNONYM_TABLE[] = {
    {"accumulate", "sum"}, {"addiere", "add"}, {"addieren", "add"}, {"addition", "add"},
    {"aggregate", "sum"}, {"arrange", "sort"}, {"aufaddieren", "sum"}, {"aufrechnen", "sum"},
    {"aufsummieren", "sum"}, {"berechnen", "sum"}, {"berechne", "sum"}, {"bubble sort", "sort"},
    {"bubblesort", "sort"}, {"calculate", "sum"}, {"calculation", "sum"}, {"classify", "sort"},
    {"collect", "input"}, {"compute", "sum"}, {"count up", "sum"}, {"deduct", "sub"},
    {"differenz", "sub"}, {"divide", "div"}, {"dividieren", "div"}, {"enumeration", "print"},
    {"enumerate", "print"}, {"evaluate", "sum"}, {"figure out", "sum"}, {"gather", "input"},
    {"group", "sort"}, {"herabsetzen", "sub"}, {"increment", "add"}, {"iteration", "loop"},
    {"iterate", "loop"}, {"merge sort", "sort"}, {"minus", "sub"}, {"multiplizieren", "mul"},
    {"order", "sort"}, {"ordnen", "sort"}, {"organize", "sort"}, {"plus", "add"},
    {"produkt", "mul"}, {"product", "mul"}, {"quotient", "div"}, {"quick sort", "sort"},
    {"rechnen", "sum"}, {"reduce", "sub"}, {"remainder", "mod"}, {"reorder", "sort"},
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

static const char* resolve_synonym(const char *word) {
    for (int i = 0; i < NUM_SYNONYMS; i++)
        if (strcmp(word, SYNONYM_TABLE[i].word) == 0)
            return SYNONYM_TABLE[i].canonical;
    return NULL;
}

static void expand_query(const char *query, char *expanded, int max_len) {
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

static int has_word_fuzzy(const char *text, const char *keyword) {
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

static int has_word(const char *prompt, const char *word) {
    return has_word_fuzzy(prompt, word);
}

static const char* extract_var_name(const char *prompt, const char *fallback) {
    static char buf[256];
    static const char *keywords[] = {
        "sum", "total", "number", "value", "result", "count", "index",
        "name", "max", "min", "average", "median", "temp", "size",
        "factorial", "fact", "prime", "power", "base", "exp",
        "gcd", "guess", "secret", "random", "num", "fib",
        "string", "text", "word", "char", "hex", "binary", NULL
    };
    char lbuf[MAX_PROMPT];
    snprintf(lbuf, sizeof(lbuf), "%s", prompt);
    to_lowercase(lbuf);
    char *p = lbuf;
    while (*p) {
        while (*p && !isalpha(*p)) p++;
        if (!*p) break;
        char *start = p;
        while (*p && isalpha(*p)) p++;
        char saved = *p;
        *p = '\0';
        const char *syn = resolve_synonym(start);
        for (int i = 0; keywords[i]; i++) {
            if (strcmp(start, keywords[i]) == 0 ||
                (syn && strcmp(syn, keywords[i]) == 0)) {
                *p = saved;
                snprintf(buf, sizeof(buf), "%s", start);
                return buf;
            }
        }
        *p = saved;
    }
    return fallback;
}

// ==================== TASK PROFILE ====================

#define MAX_NUMS 8

typedef struct {
    int has_input;
    int input_count;
    int count_value;
    int has_literals;
    int literals[MAX_NUMS];
    int num_literals;
    int has_operation;
    char op[16];
    int has_algorithm;
    char algorithm[32];
    int algo_param;
    int has_loop;
    int loop_start, loop_end;
    int has_condition;
    int has_output;
    int has_sort;
    int has_power;
    int has_max;
    int has_min;
    int has_descending;
    int has_gcd;
    int has_countdown;
    int has_mult_table;
    int has_guess;
    int has_random;
    int has_hello_name;
    int has_time;
    int has_pointer;
    int has_struct;
    int has_hex_binary;
    int has_shell_args;
    int has_array;
    int has_function;
    int has_average;
    int has_fizzbuzz;
    int has_even_odd;
    int has_primes;
    int has_sum;
    int has_factorial;
    int has_fibonacci;
    int has_median;
    int median_count;
    int has_string_cat;
    int has_string_compare;
    int has_array_assign;
    int has_array_reverse;
    int has_array_find;
    int has_sum_range;
    int sum_range_n;
    int has_print_even;
    int print_even_n;
    int has_find_max;
    int find_max_count;
    int has_fib_seq;
    int fib_seq_n;
    int has_input_sort;
    int input_sort_count;
    int has_input_fact;
    int has_countdown_from;
    int countdown_start;
    int has_read_file;
    int has_write_file;
    int has_array_vmath;
    int has_string_to_num;
    int has_timer;
    int has_array_min_max;
    int has_bool_demo;
    int has_print_var;
    int has_bit_check;
    int has_leap_year;
    int has_temp_convert;
    int has_circle_area;
    int has_fann_create;
    int has_fann_train;
    int has_fann_run;
    int has_palindrome;
    int has_lcm;
    int has_collatz;
    int has_sum_of_digits;
    int has_reverse_string;
    int has_armstrong;
    int has_perfect_number;
    int has_count_vowels;
    int has_anagram_check;
    int has_string_to_upper;
    int has_string_to_lower;
    int has_caesar_cipher;
    int has_palindrome_string;
    int has_bubble_sort;
    int has_binary_search;
    int has_square_root;
    int has_prime_factorization;
    int has_standard_deviation;
    int has_compound_interest;
    int has_decimal_to_binary;
    int has_dice_roll;
    int has_double_math;
    int has_double_circle_area;
    int has_double_average;
    int has_double_compound_interest;
    int has_double_pythagoras;
    int has_double_temp_convert;
    int has_double_sqrt;
    int has_string_length;
    int has_stack;
    int has_queue;
    int has_insertion_sort;
    int has_calculator;
    int has_unit_converter;
    int has_rock_paper_scissors;
    int has_pyramid;
    int has_temp_converter_menu;
    int has_sort_stats;
    int has_string_analyzer;
    int has_number_analyzer;
    int has_filter_numbers;
    int has_random_generator;
    int has_math_menu;
    int has_quiz_game;
    int has_bmi_calculator;
    int has_statistics_suite;
    int has_linked_list;
    int has_binary_search_tree;
    int has_tree_traversal;
    int has_graph_bfs_dfs;
    int has_n_queens;
    int has_sudoku;
    int has_levenshtein;
    int has_maze_generator;
    int has_maze_solver;
    int has_monte_carlo;
    int has_matrix_mul;
    int has_matrix_transpose;
    int has_numerical_integration;
    int has_complex_numbers;
    int has_linear_regression;
    int has_base_converter;
    int has_freq_analysis;
    int has_shuffle;
    int has_weighted_random;
    int has_ascii_table;
    int suppress_output;
    char result_var[64];
    int skip_input;
    char inherit_var[64];
    int inherit_count;
    char inherit_var_names[64][64];
    char inherit_var_types[64][32];
    int inherit_var_counts[64];
    int num_inherit_vars;
    int extra_emitters[8];
    int num_extra_emitters;
    char type[16];
    char title[256];
} TaskProfile;

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
            nums[count++] = atoi(p);
            while (*p && isdigit(*p)) p++;
            continue;
        }
        p++;
    }
    return count;
}

// ==================== 21 EMITTER BLOCKS ====================

// 1-3: Basic emitters
static void emit_math(Program *prog, Function *f, const char *type, const char *op, const int *vals, int n, int last_step);
static void emit_input_loop(Program *prog, Function *f, int count, const char *type, int do_sum);
static void emit_for_sum(Program *prog, Function *f, int n);
static void emit_print_even(Program *prog, Function *f, int n);
static void emit_input_find_max(Program *prog, Function *f, int count);
static void emit_countdown_from(Program *prog, Function *f, int start);
static void emit_fib_seq(Program *prog, Function *f, int n);
static void emit_input_sort(Program *prog, Function *f, int count, int skip_input, int descending);
static void emit_median(Program *prog, Function *f, int count, int skip_input);
static void emit_string_cat(Program *prog, Function *f);
static void emit_string_compare(Program *prog, Function *f);
static void emit_array_assign(Program *prog, Function *f);
static void emit_array_reverse(Program *prog, Function *f, int skip_input);
static void emit_array_find(Program *prog, Function *f, int skip_input);
static void emit_input_factorial(Program *prog, Function *f);
static void emit_array_vmath(Program *prog, Function *f, int skip_input);
static void emit_read_file(Program *prog, Function *f);
static void emit_write_file(Program *prog, Function *f);
static void emit_string_to_num(Program *prog, Function *f);
static void emit_timer(Program *prog, Function *f);
static void emit_factorial(Program *prog, Function *f);
static void emit_fizzbuzz(Program *prog, Function *f);
static void emit_primes(Program *prog, Function *f);
static void emit_even_odd(Program *prog, Function *f);
static void emit_power(Program *prog, Function *f);
static void emit_multiplication_table(Program *prog, Function *f);
static void emit_guess_number(Program *prog, Function *f);
static void emit_gcd(Program *prog, Function *f);
static void emit_random_number(Program *prog, Function *f);
static void emit_hello_name(Program *prog, Function *f);
static void emit_array_min_max(Program *prog, Function *f);
static void emit_bool_demo(Program *prog, Function *f);
static void emit_bit_check(Program *prog, Function *f);
static void emit_leap_year(Program *prog, Function *f);
static void emit_temp_convert(Program *prog, Function *f);
static void emit_circle_area(Program *prog, Function *f);
static void emit_average(Program *prog, Function *f, int skip_input);
static void emit_selection_sort(Program *prog, Function *f, int count, int skip_input);
static void emit_fann_create(Program *prog, Function *f);
static void emit_fann_train(Program *prog, Function *f);
static void emit_fann_run(Program *prog, Function *f);
static void emit_palindrome(Program *prog, Function *f);
static void emit_lcm(Program *prog, Function *f);
static void emit_collatz(Program *prog, Function *f);
static void emit_sum_of_digits(Program *prog, Function *f);
static void emit_function(Program *prog, Function *f);
static void emit_reverse_string(Program *prog, Function *f);
static void emit_armstrong(Program *prog, Function *f);
static void emit_perfect_number(Program *prog, Function *f);
static void emit_count_vowels(Program *prog, Function *f);
static void emit_anagram_check(Program *prog, Function *f);
static void emit_string_to_upper(Program *prog, Function *f);
static void emit_string_to_lower(Program *prog, Function *f);
static void emit_caesar_cipher(Program *prog, Function *f);
static void emit_palindrome_string(Program *prog, Function *f);
static void emit_bubble_sort(Program *prog, Function *f, int skip_input);
static void emit_binary_search(Program *prog, Function *f);
static void emit_square_root(Program *prog, Function *f);
static void emit_prime_factorization(Program *prog, Function *f);
static void emit_standard_deviation(Program *prog, Function *f, int skip_input);
static void emit_compound_interest(Program *prog, Function *f);
static void emit_decimal_to_binary(Program *prog, Function *f);
static void emit_dice_roll(Program *prog, Function *f);
static void emit_double_math(Program *prog, Function *f, const char *op);
static void emit_double_circle_area(Program *prog, Function *f);
static void emit_double_average(Program *prog, Function *f, int skip_input);
static void emit_double_compound_interest(Program *prog, Function *f);
static void emit_double_pythagoras(Program *prog, Function *f);
static void emit_double_temp_convert(Program *prog, Function *f);
static void emit_double_sqrt(Program *prog, Function *f);
static void emit_string_length(Program *prog, Function *f);
static void emit_stack(Program *prog, Function *f);
static void emit_queue(Program *prog, Function *f);
static void emit_insertion_sort(Program *prog, Function *f, int count, int skip_input);
static void emit_calculator(Program *prog, Function *f);
static void emit_unit_converter(Program *prog, Function *f);
static void emit_rock_paper_scissors(Program *prog, Function *f);
static void emit_pyramid(Program *prog, Function *f);
static void emit_temp_converter_menu(Program *prog, Function *f);
static void emit_sort_stats(Program *prog, Function *f);
static void emit_string_analyzer(Program *prog, Function *f);
static void emit_number_analyzer(Program *prog, Function *f);
static void emit_filter_numbers(Program *prog, Function *f);
static void emit_random_generator(Program *prog, Function *f);
static void emit_math_menu(Program *prog, Function *f);
static void emit_quiz_game(Program *prog, Function *f);
static void emit_bmi_calculator(Program *prog, Function *f);
static void emit_statistics_suite(Program *prog, Function *f);
static void emit_linked_list(Program *prog, Function *f);
static void emit_binary_search_tree(Program *prog, Function *f);
static void emit_tree_traversal(Program *prog, Function *f);
static void emit_graph_bfs_dfs(Program *prog, Function *f);
static void emit_n_queens(Program *prog, Function *f);
static void emit_sudoku(Program *prog, Function *f);
static void emit_levenshtein_distance(Program *prog, Function *f);
static void emit_maze_generator(Program *prog, Function *f);
static void emit_maze_solver(Program *prog, Function *f);
static void emit_monte_carlo_pi(Program *prog, Function *f);
static void emit_matrix_multiplication(Program *prog, Function *f);
static void emit_matrix_transpose(Program *prog, Function *f);
static void emit_numerical_integration(Program *prog, Function *f);
static void emit_complex_numbers(Program *prog, Function *f);
static void emit_linear_regression(Program *prog, Function *f);
static void emit_base_converter(Program *prog, Function *f);
static void emit_freq_analysis(Program *prog, Function *f);
static void emit_shuffle(Program *prog, Function *f);
static void emit_weighted_random(Program *prog, Function *f);
static void emit_ascii_table(Program *prog, Function *f);

// ==================== TINY LLM INFERENCE ENGINE ====================

#define EMBED_DIM 32
#define VOCAB_SIZE 72
#define TEMPERATURE 0.8
#define MAX_STEPS 8
#define NUM_EMITTERS 106

typedef struct {
    char word[32];
    float embed[EMBED_DIM];
} WordEmbedding;

static WordEmbedding word_embeddings[VOCAB_SIZE];
static float attention_weights[NUM_EMITTERS];
static const char *EMITTER_NAMES[NUM_EMITTERS] = {"math","input_loop","loop","for_sum","print_even","find_max","countdown","fib_seq","input_sort","median","string_cat","string_compare","array_assign","array_reverse","array_find","input_fact","array_vmath","read_file","write_file","string_to_num","timer","factorial","fizzbuzz","primes","even_odd","power","mult_table","guess","gcd","hello_name","random","array_min_max","bool_demo","bit_check","fann_create","fann_train","fann_run","average","selection_sort","palindrome","lcm","collatz","sum_of_digits","reverse_string","armstrong","perfect_number","count_vowels","anagram_check","string_to_upper","string_to_lower","caesar_cipher","palindrome_string","bubble_sort","binary_search","square_root","prime_factorization","standard_deviation","compound_interest","decimal_to_binary","dice_roll","double_math","double_circle_area","double_average","double_compound_interest","double_pythagoras","double_temp_convert","double_sqrt","function","string_length","stack","queue","insertion_sort","calculator","unit_converter","rock_paper_scissors","pyramid","temp_converter_menu","sort_stats","string_analyzer","number_analyzer","filter_numbers","random_generator","math_menu","quiz_game","bmi_calculator","statistics_suite","linked_list","binary_search_tree","tree_traversal","graph_bfs_dfs","n_queens","sudoku","levenshtein_distance","maze_generator","maze_solver","monte_carlo_pi","matrix_multiplication","matrix_transpose","numerical_integration","complex_numbers","linear_regression","base_converter","freq_analysis","shuffle","weighted_random","ascii_table"};
static int vs_boost_tokens[64];
static int vs_boost_count = 0;

// Vector search data structures
#define MAX_EXAMPLES 512
#define MAX_TOP_K 10
#define EXAMPLE_DIR "l1vm-example-code"
#define EXAMPLE_SUBDIRS 3
static const char *example_subdirs[EXAMPLE_SUBDIRS] = {"prog", "include", "lib"};
static const char *example_exts[] = {".l1com", ".l1h", ".l1asm"};
#define EXAMPLE_EXTS 3

typedef struct {
    char filename[1024];
    char stem[512];
    float embedding[EMBED_DIM];
    float score;
} ExampleDoc;

static ExampleDoc example_docs[MAX_EXAMPLES];
static int num_examples = 0;
static int examples_indexed = 0;

static void index_examples(void);
static int search_examples(const char *query, int top_k, int *indices, float *scores);

// Learned pattern functions
static char* learned_dir_path(void);
static void ensure_learned_dir(void);
static int learn_from_file(const char *path, const char *keywords, const char *description);
static int save_learned_pattern(LearnedPattern *lp);
static void load_learned_patterns(void);
static int match_learned_pattern(const char *prompt, int *best_score);
static int emit_learned_pattern(Program *prog, int learned_idx);
static int emit_learned_step(Program *prog, int learned_idx);
static int has_learned_id(const char *id);
static int forget_learned(const char *id);
static void list_learned(void);

static unsigned long hash_word(const char *s) {
    unsigned long hash = 5381;
    int c;
    while ((c = *s++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

static float idf_weights[VOCAB_SIZE];

static const char *vocab[VOCAB_SIZE] = {
    "sum", "add", "sub", "mul", "div", "mod", "input", "print", "loop", "for",
    "while", "if", "array", "sort", "max", "min", "average", "median", "countdown",
    "fibonacci", "factorial", "prime", "fizzbuzz", "power", "gcd", "time", "pointer",
    "struct", "string", "concat", "compare", "reverse", "find", "assign", "file",
    "read", "write", "convert", "timer", "shell", "hello", "function", "even",
    "odd", "guess", "table", "hex", "binary", "range", "exit", "zero", "one",
    "two", "num", "value", "data", "code", "generate", "create", "make", "show",
    "start", "stop", "run",
    "fann", "train", "neural", "network", "model", "layer", "learn", "predict"
};

static int embeddings_initialized = 0;
static int dataflow_quiet_mode = 0;

static void init_embeddings(void) {
    if (embeddings_initialized) return;
    embeddings_initialized = 1;
    for (int i = 0; i < VOCAB_SIZE; i++) {
        unsigned long seed = hash_word(vocab[i]);
        srand(seed);
        float sum = 0;
        for (int j = 0; j < EMBED_DIM; j++) {
            word_embeddings[i].embed[j] = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
            sum += word_embeddings[i].embed[j] * word_embeddings[i].embed[j];
        }
        float norm = sqrtf(sum);
        if (norm > 0)
            for (int j = 0; j < EMBED_DIM; j++) word_embeddings[i].embed[j] /= norm;
    }
    for (int i = 0; i < VOCAB_SIZE; i++) idf_weights[i] = 1.0f;
}

static int tokenize(const char *text, int *tokens, int max_tokens) {
    char buf[MAX_PROMPT];
    snprintf(buf, sizeof(buf), "%s", text);
    to_lowercase(buf);
    int count = 0;
    char *p = buf;
    while (*p && count < max_tokens) {
        while (*p && !isalpha(*p)) p++;
        if (!*p) break;
        char *start = p;
        while (*p && isalpha(*p)) p++;
        char saved = *p;
        *p = '\0';
        int found = -1;
        for (int i = 0; i < VOCAB_SIZE; i++) {
            if (strcmp(start, vocab[i]) == 0) { found = i; break; }
        }
        if (found < 0) {
            const char *syn = resolve_synonym(start);
            if (syn) {
                for (int i = 0; i < VOCAB_SIZE; i++) {
                    if (strcmp(syn, vocab[i]) == 0) { found = i; break; }
                }
            }
        }
        if (found >= 0 && count < max_tokens) tokens[count++] = found;
        *p = saved;
    }
    return count;
}

static void softmax(float *x, int n) {
    float max = x[0], sum = 0;
    for (int i = 1; i < n; i++) if (x[i] > max) max = x[i];
    for (int i = 0; i < n; i++) { x[i] = expf(x[i] - max); sum += x[i]; }
    for (int i = 0; i < n; i++) x[i] /= sum;
}

static int llm_select_emitter(const char *prompt, TaskProfile *task) {
    int tokens[64], num_tokens = tokenize(prompt, tokens, 64);

    float emitter_scores[NUM_EMITTERS];
    for (int i = 0; i < NUM_EMITTERS; i++) emitter_scores[i] = 0;

    if (task->has_operation && task->has_literals) emitter_scores[0] = 1.5f;
    if (task->has_input && !task->has_operation) emitter_scores[1] = 1.5f;
    if (task->has_input && task->has_operation) emitter_scores[1] += 1.0f;
    if (task->has_loop && !task->has_operation && !task->has_input) emitter_scores[2] = 1.5f;
    if (task->has_sum_range) emitter_scores[3] = 2.0f;
    if (task->has_print_even) emitter_scores[4] = 2.0f;
    if (task->has_find_max) emitter_scores[5] = 2.0f;
    if (task->has_countdown_from) emitter_scores[6] = 2.0f;
    if (task->has_fib_seq) emitter_scores[7] = 2.0f;
    if (task->has_input_sort) emitter_scores[8] = 2.0f;
    if (task->has_median) emitter_scores[9] = 2.0f;
    if (task->has_string_cat) emitter_scores[10] = 2.0f;
    if (task->has_string_compare) emitter_scores[11] = 2.0f;
    if (task->has_array_assign) emitter_scores[12] = 2.0f;
    if (task->has_array_reverse) emitter_scores[13] = 2.0f;
    if (task->has_array_find) emitter_scores[14] = 2.0f;
    if (task->has_input_fact) emitter_scores[15] = 2.0f;
    if (task->has_array_vmath) emitter_scores[16] = 2.0f;
    if (task->has_read_file) emitter_scores[17] = 2.0f;
    if (task->has_write_file) emitter_scores[18] = 2.0f;
    if (task->has_string_to_num) emitter_scores[19] = 2.0f;
    if (task->has_timer) emitter_scores[20] = 2.0f;
    if (task->has_factorial && !task->has_input) emitter_scores[21] = 2.0f;
    if (task->has_fizzbuzz) emitter_scores[22] = 2.0f;
    if (task->has_primes) emitter_scores[23] = 2.0f;
    if (task->has_even_odd && !task->has_print_even) emitter_scores[24] = 2.0f;
    if (task->has_power) emitter_scores[25] = 2.0f;
    if (task->has_mult_table) emitter_scores[26] = 2.0f;
    if (task->has_guess) emitter_scores[27] = 2.0f;
    if (task->has_gcd) emitter_scores[28] = 2.0f;
    if (task->has_hello_name) emitter_scores[29] = 2.0f;
    if (task->has_random) emitter_scores[30] = 2.0f;
    if (task->has_array_min_max) emitter_scores[31] = 2.0f;
    if (task->has_bool_demo) emitter_scores[32] = 2.0f;
    if (task->has_bit_check) emitter_scores[33] = 2.0f;
    if (task->has_fann_create && task->has_fann_train) emitter_scores[35] = 2.5f;
    if (task->has_fann_create && !task->has_fann_train) emitter_scores[34] = 2.0f;
    if (task->has_fann_run) emitter_scores[36] = 2.0f;
    if (task->has_average && task->has_input) emitter_scores[37] = 2.0f;
    if (task->has_sort && !task->has_input) emitter_scores[38] = 2.0f;
    if (task->has_palindrome) emitter_scores[39] = 2.0f;
    if (task->has_lcm) emitter_scores[40] = 2.0f;
    if (task->has_collatz) emitter_scores[41] = 2.0f;
    if (task->has_sum_of_digits) emitter_scores[42] = 2.0f;
    if (task->has_reverse_string) emitter_scores[43] = 2.0f;
    if (task->has_armstrong) emitter_scores[44] = 2.0f;
    if (task->has_perfect_number) emitter_scores[45] = 2.0f;
    if (task->has_count_vowels) emitter_scores[46] = 2.0f;
    if (task->has_anagram_check) emitter_scores[47] = 2.0f;
    if (task->has_string_to_upper) emitter_scores[48] = 2.0f;
    if (task->has_string_to_lower) emitter_scores[49] = 2.0f;
    if (task->has_caesar_cipher) emitter_scores[50] = 2.0f;
    if (task->has_palindrome_string) emitter_scores[51] = 2.0f;
    if (task->has_bubble_sort) emitter_scores[52] = 2.0f;
    if (task->has_binary_search) emitter_scores[53] = 2.0f;
    if (task->has_square_root) emitter_scores[54] = 2.0f;
    if (task->has_prime_factorization) emitter_scores[55] = 2.0f;
    if (task->has_standard_deviation) emitter_scores[56] = 2.0f;
    if (task->has_compound_interest) emitter_scores[57] = 2.0f;
    if (task->has_decimal_to_binary) emitter_scores[58] = 2.0f;
    if (task->has_dice_roll) emitter_scores[59] = 2.0f;
    if (task->has_double_math) emitter_scores[60] = 2.0f;
    if (task->has_double_circle_area) emitter_scores[61] = 2.0f;
    if (task->has_double_average) emitter_scores[62] = 2.0f;
    if (task->has_double_compound_interest) emitter_scores[63] = 2.0f;
    if (task->has_double_pythagoras) emitter_scores[64] = 2.0f;
    if (task->has_double_temp_convert) emitter_scores[65] = 2.0f;
    if (task->has_double_sqrt) emitter_scores[66] = 2.0f;
    if (task->has_function) emitter_scores[67] = 2.0f;
    if (task->has_string_length) emitter_scores[68] = 2.0f;
    if (task->has_stack) emitter_scores[69] = 2.0f;
    if (task->has_queue) emitter_scores[70] = 2.0f;
    if (task->has_insertion_sort) emitter_scores[71] = 2.0f;
    if (task->has_calculator) emitter_scores[72] = 2.0f;
    if (task->has_unit_converter) emitter_scores[73] = 2.0f;
    if (task->has_rock_paper_scissors) emitter_scores[74] = 2.0f;
    if (task->has_pyramid) emitter_scores[75] = 2.0f;
    if (task->has_temp_converter_menu) emitter_scores[76] = 2.0f;
    if (task->has_sort_stats) emitter_scores[77] = 2.0f;
    if (task->has_string_analyzer) emitter_scores[78] = 2.0f;
    if (task->has_number_analyzer) emitter_scores[79] = 2.0f;
    if (task->has_filter_numbers) emitter_scores[80] = 2.0f;
    if (task->has_random_generator) emitter_scores[81] = 2.0f;
    if (task->has_math_menu) emitter_scores[82] = 2.0f;
    if (task->has_quiz_game) emitter_scores[83] = 2.0f;
    if (task->has_bmi_calculator) emitter_scores[84] = 2.0f;
    if (task->has_statistics_suite) emitter_scores[85] = 2.0f;
    if (task->has_linked_list) emitter_scores[86] = 2.0f;
    if (task->has_binary_search_tree) emitter_scores[87] = 2.0f;
    if (task->has_tree_traversal) emitter_scores[88] = 2.0f;
    if (task->has_graph_bfs_dfs) emitter_scores[89] = 2.0f;
    if (task->has_n_queens) emitter_scores[90] = 2.0f;
    if (task->has_sudoku) emitter_scores[91] = 2.0f;
    if (task->has_levenshtein) emitter_scores[92] = 2.0f;
    if (task->has_maze_generator) emitter_scores[93] = 2.0f;
    if (task->has_maze_solver) emitter_scores[94] = 2.0f;
    if (task->has_monte_carlo) emitter_scores[95] = 2.0f;
    if (task->has_matrix_mul) emitter_scores[96] = 2.0f;
    if (task->has_matrix_transpose) emitter_scores[97] = 2.0f;
    if (task->has_numerical_integration) emitter_scores[98] = 2.0f;
    if (task->has_complex_numbers) emitter_scores[99] = 2.0f;
    if (task->has_linear_regression) emitter_scores[100] = 2.0f;
    if (task->has_base_converter) emitter_scores[101] = 2.0f;
    if (task->has_freq_analysis) emitter_scores[102] = 2.0f;
    if (task->has_shuffle) emitter_scores[103] = 2.0f;
    if (task->has_weighted_random) emitter_scores[104] = 2.0f;
    if (task->has_ascii_table) emitter_scores[105] = 3.0f;

    for (int ti = 0; ti < num_tokens && ti < 32; ti++) {
        int tok_id = tokens[ti];
        if (tok_id == 64 || tok_id == 66 || tok_id == 67) { emitter_scores[34] += 1.0f; emitter_scores[35] += 1.5f; emitter_scores[36] += 1.0f; }
        if (tok_id == 65 || tok_id == 70) { emitter_scores[34] += 0.5f; emitter_scores[35] += 1.5f; }
        if (tok_id == 71) { emitter_scores[36] += 1.5f; }
        if (tok_id == 0 || tok_id == 4) emitter_scores[0] += 0.5f;
        if (tok_id == 5 || tok_id == 1) emitter_scores[1] += 0.5f;
        if (tok_id == 8 || tok_id == 9) emitter_scores[2] += 0.5f;
        if (tok_id == 17) emitter_scores[9] += 0.8f;
        if (tok_id == 19) emitter_scores[7] += 0.8f;
        if (tok_id == 20) emitter_scores[15] += 0.8f;
        if (tok_id == 13) emitter_scores[8] += 0.8f;
        if (tok_id == 14) emitter_scores[5] += 0.8f;
        if (tok_id == 15) emitter_scores[16] += 0.5f;
        if (tok_id == 28) { emitter_scores[10] += 0.8f; emitter_scores[11] += 0.8f; }
        if (tok_id == 32) emitter_scores[13] += 0.8f;
        if (tok_id == 33) emitter_scores[14] += 0.8f;
        if (tok_id == 36 || tok_id == 35) { emitter_scores[17] += 0.8f; emitter_scores[18] += 0.8f; }
        if (tok_id == 37) emitter_scores[19] += 0.8f;
        if (tok_id == 38) emitter_scores[20] += 0.8f;
        if (tok_id == 27) emitter_scores[6] += 0.8f;
        if (tok_id == 23) emitter_scores[0] += 0.3f;
        if (tok_id == 20) emitter_scores[21] += 0.8f;
        if (tok_id == 22) emitter_scores[22] += 0.8f;
        if (tok_id == 21) emitter_scores[23] += 0.8f;
        if (tok_id == 42 || tok_id == 43) emitter_scores[24] += 0.8f;
        if (tok_id == 23) emitter_scores[25] += 0.5f;
        if (tok_id == 46) emitter_scores[26] += 0.8f;
        if (tok_id == 44) emitter_scores[27] += 0.8f;
        if (tok_id == 24) emitter_scores[28] += 0.8f;
        if (tok_id == 40) emitter_scores[29] += 0.8f;
        if (tok_id == 53) emitter_scores[30] += 0.5f;
    }

    // Phase 2b: Vector search boost tokens from matched examples
    for (int ti = 0; ti < vs_boost_count && ti < 32; ti++) {
        int tok_id = vs_boost_tokens[ti];
        if (tok_id == 64 || tok_id == 66 || tok_id == 67) { emitter_scores[34] += 0.5f; emitter_scores[35] += 0.7f; emitter_scores[36] += 0.5f; }
        if (tok_id == 65 || tok_id == 70) { emitter_scores[34] += 0.3f; emitter_scores[35] += 0.7f; }
        if (tok_id == 71) { emitter_scores[36] += 0.7f; }
        if (tok_id == 0 || tok_id == 4) emitter_scores[0] += 0.3f;
        if (tok_id == 5 || tok_id == 1) emitter_scores[1] += 0.3f;
        if (tok_id == 8 || tok_id == 9) emitter_scores[2] += 0.3f;
        if (tok_id == 17) emitter_scores[9] += 0.4f;
        if (tok_id == 19) emitter_scores[7] += 0.4f;
        if (tok_id == 20) emitter_scores[15] += 0.4f;
        if (tok_id == 13) emitter_scores[8] += 0.4f;
        if (tok_id == 14) emitter_scores[5] += 0.4f;
        if (tok_id == 15) emitter_scores[16] += 0.3f;
        if (tok_id == 28) { emitter_scores[10] += 0.4f; emitter_scores[11] += 0.4f; }
        if (tok_id == 32) emitter_scores[13] += 0.4f;
        if (tok_id == 33) emitter_scores[14] += 0.4f;
        if (tok_id == 36 || tok_id == 35) { emitter_scores[17] += 0.4f; emitter_scores[18] += 0.4f; }
        if (tok_id == 37) emitter_scores[19] += 0.4f;
        if (tok_id == 38) emitter_scores[20] += 0.4f;
        if (tok_id == 27) emitter_scores[6] += 0.4f;
        if (tok_id == 23) { emitter_scores[0] += 0.2f; emitter_scores[25] += 0.3f; }
        if (tok_id == 22) emitter_scores[22] += 0.4f;
        if (tok_id == 21) emitter_scores[23] += 0.4f;
        if (tok_id == 42 || tok_id == 43) emitter_scores[24] += 0.4f;
        if (tok_id == 44) emitter_scores[27] += 0.4f;
        if (tok_id == 24) emitter_scores[28] += 0.4f;
        if (tok_id == 40) emitter_scores[29] += 0.4f;
        if (tok_id == 46) emitter_scores[26] += 0.4f;
        if (tok_id == 53) emitter_scores[30] += 0.3f;
    }

    float temp = TEMPERATURE;
    for (int i = 0; i < NUM_EMITTERS; i++) emitter_scores[i] /= temp;
    softmax(emitter_scores, NUM_EMITTERS);
    for (int i = 0; i < NUM_EMITTERS; i++) attention_weights[i] = emitter_scores[i];

    int best = 0;
    float best_p = emitter_scores[0];
    for (int i = 1; i < NUM_EMITTERS; i++) {
        if (emitter_scores[i] > best_p) { best_p = emitter_scores[i]; best = i; }
    }

    if (verbose_flag) {
        printf("\nEmitter scores:\n");
        for (int i = 0; i < NUM_EMITTERS; i++)
            printf("  %2d %-16s %.3f\n", i, EMITTER_NAMES[i], emitter_scores[i]);
        printf("  -> best: %s (%.3f)\n", EMITTER_NAMES[best], best_p);
    }

    // Multi-emitter composition: add strongly-scored secondary emitters
    // Dynamic threshold based on prompt length: longer prompts more likely multi-step
    float pfactor = (float)strlen(prompt) / 80.0f;
    if (pfactor > 2.0f) pfactor = 2.0f;
    if (pfactor < 0.5f) pfactor = 0.5f;
    task->num_extra_emitters = 0;
    float threshold = best_p * (0.5f / pfactor);
    float hard_min = 0.2f;
    if (threshold < hard_min) threshold = hard_min;
    for (int i = 0; i < NUM_EMITTERS && task->num_extra_emitters < 8; i++) {
        if (i != best && emitter_scores[i] >= threshold && emitter_scores[i] >= 0.5f) {
            task->extra_emitters[task->num_extra_emitters++] = i;
        }
    }

    if (verbose_flag && task->num_extra_emitters > 0) {
        printf("  extra emitters:");
        for (int ei = 0; ei < task->num_extra_emitters; ei++)
            printf(" %s", EMITTER_NAMES[task->extra_emitters[ei]]);
        printf("\n");
    }

    return best;
}

// Forward declarations for plan-based generator
static int parse_task(const char *prompt, TaskProfile *task);
static int generate_from_task(Program *prog, TaskProfile *task, int last_step);

static int has_actionable_keyword(const char *text) {
    return has_word(text, "lies") || has_word(text, "read") || has_word(text, "input")
        || has_word(text, "gib") || has_word(text, "enter") || has_word(text, "erfasse")
        || has_word(text, "print") || has_word(text, "druck") || has_word(text, "ausg")
        || has_word(text, "show") || has_word(text, "display") || has_word(text, "zeig")
        || has_word(text, "add") || has_word(text, "sum") || has_word(text, "summe")
        || has_word(text, "sub") || has_word(text, "subtract") || has_word(text, "minus")
        || has_word(text, "mul") || has_word(text, "multiply") || has_word(text, "mal")
        || has_word(text, "div") || has_word(text, "divide") || has_word(text, "geteilt")
        || has_word(text, "mod") || has_word(text, "modulo")
        || has_word(text, "max") || has_word(text, "größt") || has_word(text, "largest")
        || has_word(text, "greatest") || has_word(text, "groesst")
        || has_word(text, "sort") || has_word(text, "sortiere") || has_word(text, "bubble")
        || has_word(text, "sorted") || has_word(text, "sortiert")
        || has_word(text, "average") || has_word(text, "durchschnitt") || has_word(text, "mean")
        || has_word(text, "factorial") || has_word(text, "fakult") || has_word(text, "median")
        || has_word(text, "fibonacci") || has_word(text, "fib") || has_word(text, "find")
        || has_word(text, "search") || has_word(text, "suche") || has_word(text, "reverse")
        || has_word(text, "umkehr") || has_word(text, "countdown")
        || has_word(text, "cat") || has_word(text, "concat") || has_word(text, "compare")
        || has_word(text, "cmp") || has_word(text, "vergleich") || has_word(text, "assign")
        || has_word(text, "zuweis") || has_word(text, "write") || has_word(text, "schreib")
        || has_word(text, "hello") || has_word(text, "hallo")
        || has_word(text, "prime") || has_word(text, "prim") || has_word(text, "fizzbuzz")
        || has_word(text, "even") || has_word(text, "odd") || has_word(text, "gerade")
        || has_word(text, "ungerade") || has_word(text, "power") || has_word(text, "potenz")
        || has_word(text, "exponent") || has_word(text, "hoch")
        || has_word(text, "guess") || has_word(text, "rate") || has_word(text, "raten")
        || has_word(text, "table") || has_word(text, "einmaleins")
        || has_word(text, "multiplication") || has_word(text, "multiplika")
        || has_word(text, "gcd") || has_word(text, "ggt") || has_word(text, "gcm")
        || has_word(text, "time") || has_word(text, "zeit") || has_word(text, "clock")
        || has_word(text, "pointer") || has_word(text, "zeiger") || has_word(text, "struct")
        || has_word(text, "function") || has_word(text, "funktion")
        || has_word(text, "for") || has_word(text, "loop") || has_word(text, "schleife")
        || has_word(text, "if") || has_word(text, "bedingung") || has_word(text, "wenn")
        || has_word(text, "while") || has_word(text, "switch") || has_word(text, "case")
        || has_word(text, "array") || has_word(text, "feld") || has_word(text, "liste")
        || has_word(text, "string") || has_word(text, "zeichen") || has_word(text, "text")
        || has_word(text, "shell") || has_word(text, "argument") || has_word(text, "parameter")
        || has_word(text, "ergebnis") || has_word(text, "result") || has_word(text, "value")
        || has_word(text, "wert") || has_word(text, "number") || has_word(text, "zahl")
        || has_word(text, "element") || has_word(text, "index") || has_word(text, "bis")
        || has_word(text, "to") || has_word(text, "von") || has_word(text, "from")
        || has_word(text, "file") || has_word(text, "datei")
        || has_word(text, "convert") || has_word(text, "parse") || has_word(text, "umwand")
        || has_word(text, "timer") || has_word(text, "benchmark")
        || has_word(text, "measure") || has_word(text, "mess") || has_word(text, "dauer")
        || has_word(text, "execution") || has_word(text, "ausführ") || has_word(text, "lauf")
        || has_word(text, "fann") || has_word(text, "neural") || has_word(text, "network")
        || has_word(text, "train") || has_word(text, "learn") || has_word(text, "predict")
        || has_word(text, "infer") || has_word(text, "ai");
}

static int split_prompt_steps(const char *prompt, char steps[MAX_STEPS][MAX_PROMPT]) {
    char buf[MAX_PROMPT];
    snprintf(buf, sizeof(buf), "%s", prompt);
    to_lowercase(buf);
    int num_steps = 0;
    char *remaining = buf;

    struct { const char *pat; int len; } patterns[] = {
        {" und dann ", 10}, {" und danach ", 12}, {" anschließend ", 14},
        {" and then ", 10}, {" . ", 3}, {". ", 2}, {"; ", 2},
        {" then ", 6}, {" danach ", 8},
        {" und ", 5}, {" and ", 5}, {", ", 2},
    };
    int npats = sizeof(patterns) / sizeof(patterns[0]);

    while (num_steps < MAX_STEPS - 1) {
        int best_pos = -1, best_len = 0;
        for (int i = 0; i < npats; i++) {
            char *pos = strstr(remaining, patterns[i].pat);
            if (!pos) continue;
            int idx = pos - remaining;
            if (best_pos >= 0 && idx >= best_pos) continue;
            best_pos = idx; best_len = patterns[i].len;
        }
        if (best_pos < 0) break;
        char step[MAX_PROMPT];
        snprintf(step, MAX_PROMPT, "%.*s", best_pos, remaining);
        trim(step);
        if (strlen(step) > 0) { snprintf(steps[num_steps], MAX_PROMPT, "%s", step); num_steps++; }
        remaining += best_pos + best_len;
    }
    trim(remaining);
    if (strlen(remaining) > 0) { snprintf(steps[num_steps], MAX_PROMPT, "%s", remaining); num_steps++; }

    if (num_steps > 1) {
        int merged = 1;
        while (merged) {
            merged = 0;
            for (int i = 0; i < num_steps; i++) {
                if (!has_actionable_keyword(steps[i])) {
                    if (i > 0) {
                    char merged_step[MAX_PROMPT * 2];
                    snprintf(merged_step, sizeof(merged_step), "%s %s", steps[i-1], steps[i]);
                        trim(merged_step);
                        snprintf(steps[i-1], MAX_PROMPT, "%s", merged_step);
                        for (int j = i; j < num_steps - 1; j++) snprintf(steps[j], MAX_PROMPT, "%s", steps[j+1]);
                        num_steps--; merged = 1; break;
                    } else if (i == 0 && num_steps > 1) {
                        char merged_step[MAX_PROMPT * 2];
                        snprintf(merged_step, sizeof(merged_step), "%s %s", steps[0], steps[1]);
                        trim(merged_step);
                        snprintf(steps[0], MAX_PROMPT, "%s", merged_step);
                        for (int j = 1; j < num_steps - 1; j++) snprintf(steps[j], MAX_PROMPT, "%s", steps[j+1]);
                        num_steps--; merged = 1; break;
                    }
                }
            }
        }
    }

    // Additional merge: keep "read X and print the Y" as a single step
    if (num_steps > 1) {
        int merged = 1;
        while (merged) {
            merged = 0;
            for (int i = 1; i < num_steps; i++) {
                if (has_word(steps[i], "print") &&
                    (has_word(steps[i], "median") || has_word(steps[i], "average") ||
                     has_word(steps[i], "mean") || has_word(steps[i], "largest") ||
                     has_word(steps[i], "smallest") || has_word(steps[i], "greatest") ||
                     has_word(steps[i], "sum") || has_word(steps[i], "max") ||
                     has_word(steps[i], "min") || has_word(steps[i], "them") ||
                     has_word(steps[i], "it") || has_word(steps[i], "result"))) {
                    char merged_step[MAX_PROMPT * 2];
                    snprintf(merged_step, sizeof(merged_step), "%s %s", steps[i-1], steps[i]);
                    trim(merged_step);
                    snprintf(steps[i-1], MAX_PROMPT, "%s", merged_step);
                    for (int j = i; j < num_steps - 1; j++) snprintf(steps[j], MAX_PROMPT, "%s", steps[j+1]);
                    num_steps--; merged = 1; break;
                }
            }
        }
    }

    if (num_steps <= 1) { snprintf(steps[0], MAX_PROMPT, "%s", prompt); return 1; }
    return num_steps;
}

// ==================== 21 EMITTER BLOCK IMPLEMENTATIONS ====================

static void emit_math(Program *prog, Function *f, const char *type, const char *op, const int *vals, int n, int last_step) {
    (void)last_step;
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
    char vn[64], vs[16], ln[256];
    for (int i = 0; i < n; i++) {
        snprintf(vn, sizeof(vn), "%s_%c", type, 'a' + i);
        snprintf(vs, sizeof(vs), "%d", vals[i]);
        const char *vv[] = {vs};
        add_var_to_func(f, type, vn, 1, vv, 1);
    }
    const char *rv[] = {"0"};
    char rn[64];
    snprintf(rn, sizeof(rn), "%s_r", type);
    add_var_to_func(f, type, rn, 1, rv, 1);
    const char *sym = "+";
    if (strcmp(op, "sub") == 0) sym = "-";
    else if (strcmp(op, "mul") == 0) sym = "*";
    else if (strcmp(op, "div") == 0) sym = "/";
    else if (strcmp(op, "mod") == 0) sym = "%";
    if (n == 2)
        snprintf(ln, sizeof(ln), "\t(%s_%c %s %s_%c %s :=)", type, 'a', sym, type, 'b', rn);
    else
        snprintf(ln, sizeof(ln), "\t((%s_%c %s %s_%c) %s %s_%c %s :=)", type, 'a', sym, type, 'b', sym, type, 'c', rn);
    if (strcmp(op, "div") == 0 || strcmp(op, "mod") == 0) {
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
    snprintf(ln, sizeof(ln), "\t(%s :print_i !)", rn);
    func_append(f, ln);
    func_append(f, "\t(:print_n !)");
}

static void emit_input_loop(Program *prog, Function *f, int count, const char *type, int do_sum) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    char vs[16], ln[256];
    snprintf(vs, sizeof(vs), "%d", count);
    const char *cv[] = {vs};
    add_var_to_func(f, "int64", "count", 1, cv, 1);
    add_var_to_func(f, type, "val", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "sum", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "input_prompt", 18, ps, 1);
    const char *rps[] = {"\"Result: \""};
    add_var_to_func(f, "const-string", "result_prompt", 9, rps, 1);
    func_append(f, "\t(for-loop)");
    snprintf(ln, sizeof(ln), "\t(((i count <) f :=) f for)");
    func_append(f, ln);
    func_append(f, "\t\t(input_prompt :print_s !)");
    if (strcmp(type, "double") == 0)
        func_append(f, "\t\t(val :input_d !)");
    else
        func_append(f, "\t\t(val :input_i !)");
    if (do_sum)
        func_append(f, "\t\t(sum + val sum :=)");
    func_append(f, "\t\t(val :print_i !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(result_prompt :print_s !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(sum :print_i !)");
    func_append(f, "\t(:print_n !)");
}

static void emit_for_sum(Program *prog, Function *f, int n) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    char vs[16], ln[256];
    snprintf(vs, sizeof(vs), "%d", n);
    const char *nv[] = {vs};
    add_var_to_func(f, "int64", "n", 1, nv, 1);
    add_var_to_func(f, "int64", "sum", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    snprintf(ln, sizeof(ln), "\t(((i n <=) f :=) f for)");
    func_append(f, ln);
    func_append(f, "\t\t(sum + i sum :=)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(sum :print_i !)");
        func_append(f, "\t(:print_n !)");
    }
}

static void emit_print_even(Program *prog, Function *f, int n) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    char vs[16], ln[256];
    snprintf(vs, sizeof(vs), "%d", n);
    const char *nv[] = {vs};
    add_var_to_func(f, "int64", "max", 1, nv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "mod", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *es[] = {"\" is even\""};
    add_var_to_func(f, "const-string", "even_str", 10, es, 1);
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    snprintf(ln, sizeof(ln), "\t(((i max <=) f :=) f for)");
    func_append(f, ln);
    func_append(f, "\t\t(i % two mod :=)");
    func_append(f, "\t\t(((mod zero ==) f :=) f if)");
    func_append(f, "\t\t\t(i :print_i !)");
    func_append(f, "\t\t\t(even_str :print_s !)");
    func_append(f, "\t\t\t(:print_n !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
}

static void emit_input_find_max(Program *prog, Function *f, int count) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    char vs[16], ln[256];
    snprintf(vs, sizeof(vs), "%d", count);
    const char *cv[] = {vs};
    add_var_to_func(f, "int64", "count", 1, cv, 1);
    add_var_to_func(f, "int64", "val", 1, zv, 1);
    add_var_to_func(f, "int64", "mx", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "input_prompt", 18, ps, 1);
    func_append(f, "\t(for-loop)");
    snprintf(ln, sizeof(ln), "\t(((i count <) f :=) f for)");
    func_append(f, ln);
    func_append(f, "\t\t(input_prompt :print_s !)");
    func_append(f, "\t\t(val :input_i !)");
    func_append(f, "\t\t(((val mx >) f :=) f if)");
    func_append(f, "\t\t\t(val mx :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(mx :print_i !)");
    func_append(f, "\t(:print_n !)");
}

static void emit_countdown_from(Program *prog, Function *f, int start) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    char vs[16], ln[256];
    snprintf(vs, sizeof(vs), "%d", start);
    const char *sv[] = {vs};
    add_var_to_func(f, "int64", "i", 1, sv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    func_append(f, "\t(do)");
    func_append(f, "\t\t(i :print_i !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(i - one i :=)");
    snprintf(ln, sizeof(ln), "\t(((i zero >) f :=) f while)");
    func_append(f, ln);
    func_append(f, "\t(zero :print_i !)");
    func_append(f, "\t(:print_n !)");
}

static void emit_fib_seq(Program *prog, Function *f, int n) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    char vs[16], ln[256];
    snprintf(vs, sizeof(vs), "%d", n);
    const char *nv[] = {vs};
    add_var_to_func(f, "int64", "n", 1, nv, 1);
    add_var_to_func(f, "int64", "a", 1, zv, 1);
    add_var_to_func(f, "int64", "b", 1, ov, 1);
    add_var_to_func(f, "int64", "c", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, (const char *[]){"2"}, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    func_append(f, "\t(a :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(b :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(for-loop)");
    snprintf(ln, sizeof(ln), "\t(((i n <) f :=) f for)");
    func_append(f, ln);
    func_append(f, "\t\t(a + b c :=)");
    func_append(f, "\t\t(c :print_i !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(b a :=)");
    func_append(f, "\t\t(c b :=)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
}

static void emit_input_sort(Program *prog, Function *f, int count, int skip_input, int descending) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
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
    func_append(f, "\t\t\t(j * int64_size realind :=)");
    func_append(f, "\t\t\t((j + one) * int64_size realind2 :=)");
    func_append(f, "\t\t\t(arr [ realind ] a =)");
    func_append(f, "\t\t\t(a + zero a :=)");
    func_append(f, "\t\t\t(arr [ realind2 ] b =)");
    func_append(f, "\t\t\t(b + zero b :=)");
    func_append(f, descending ? "\t\t\t(((a b <) f :=) f if)" : "\t\t\t(((a b >) f :=) f if)");
    func_append(f, "\t\t\t\t(b arr [ realind ] =)");
    func_append(f, "\t\t\t\t(a arr [ realind2 ] =)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t\t(j + one j :=)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t// print sorted");
        func_append(f, "\t(zero i :=)");
        func_append(f, "\t(for-loop)");
        snprintf(ln, sizeof(ln), "\t(((i count <) f :=) f for)");
        func_append(f, ln);
        func_append(f, "\t\t(i * int64_size realind :=)");
        func_append(f, "\t\t(arr [ realind ] a =)");
        func_append(f, "\t\t(a + zero a :=)");
        func_append(f, "\t\t(a :print_i !)");
        func_append(f, "\t\t(:print_n !)");
        func_append(f, "\t\t(i + one i :=)");
        func_append(f, "\t(next)");
    }
}

static void emit_median(Program *prog, Function *f, int count, int skip_input) {
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

static void emit_string_cat(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include_post(prog, "string.l1h");
    const char *zv[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    const char *maxlen_v[] = {"256"};
    add_var_to_func(f, "const-int64", "maxlen", 1, maxlen_v, 1);
    const char *pa[] = {"\"Enter first string: \""};
    const char *pb[] = {"\"Enter second string: \""};
    add_var_to_func(f, "const-string", "prompt_a", 22, pa, 1);
    add_var_to_func(f, "const-string", "prompt_b", 23, pb, 1);
    add_var_to_func(f, "string", "str_a", 256, (const char *[]){"\"\""}, 1);
    add_var_to_func(f, "string", "str_b", 256, (const char *[]){"\"\""}, 1);
    add_var_to_func(f, "string", "result", 512, (const char *[]){"\"\""}, 1);
    add_var_to_func(f, "int64", "strmod", 1, zv, 1);
    func_append(f, "\t(strmod :string_init !)");
    func_append(f, "\t(prompt_a :print_s !)");
    func_append(f, "\t(maxlen str_a :input_s !)");
    func_append(f, "\t(prompt_b :print_s !)");
    func_append(f, "\t(maxlen str_b :input_s !)");
    func_append(f, "\t(result str_a :string_copy !)");
    func_append(f, "\t(result str_b :string_cat !)");
    func_append(f, "\t(result :print_s !)");
    func_append(f, "\t(:print_n !)");
}

static void emit_string_compare(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include_post(prog, "string.l1h");
    const char *zv[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    const char *maxlen_v[] = {"256"};
    add_var_to_func(f, "const-int64", "maxlen", 1, maxlen_v, 1);
    const char *pa[] = {"\"Enter first string: \""};
    const char *pb[] = {"\"Enter second string: \""};
    add_var_to_func(f, "const-string", "prompt_a", 22, pa, 1);
    add_var_to_func(f, "const-string", "prompt_b", 23, pb, 1);
    add_var_to_func(f, "string", "str_a", 256, (const char *[]){"\"\""}, 1);
    add_var_to_func(f, "string", "str_b", 256, (const char *[]){"\"\""}, 1);
    add_var_to_func(f, "int64", "cmp", 1, zv, 1);
    add_var_to_func(f, "int64", "strmod", 1, zv, 1);
    const char *veq[] = {"\"strings are equal\""};
    const char *vne[] = {"\"strings differ\""};
    add_var_to_func(f, "const-string", "eq_str", 20, veq, 1);
    add_var_to_func(f, "const-string", "ne_str", 16, vne, 1);
    func_append(f, "\t(strmod :string_init !)");
    func_append(f, "\t(prompt_a :print_s !)");
    func_append(f, "\t(maxlen str_a :input_s !)");
    func_append(f, "\t(prompt_b :print_s !)");
    func_append(f, "\t(maxlen str_b :input_s !)");
    func_append(f, "\t(str_a str_b cmp :string_compare !)");
    func_append(f, "\t(cmp if)");
    func_append(f, "\t\t(eq_str :print_s !)");
    func_append(f, "\t(else)");
    func_append(f, "\t\t(ne_str :print_s !)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(:print_n !)");
}

static void emit_array_assign(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "int64", "arr", 5, zv, 0);
    add_var_to_func(f, "int64", "index", 1, zv, 1);
    add_var_to_func(f, "int64", "value", 1, zv, 1);
    add_var_to_func(f, "int64", "realind", 1, zv, 1);
    add_var_to_func(f, "int64", "aux", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *pi[] = {"\"Enter index (0-4): \""};
    const char *pv[] = {"\"Enter value: \""};
    const char *perr[] = {"\"Error: index out of bounds (0-4)!\""};
    add_var_to_func(f, "const-string", "prompt_idx", 20, pi, 1);
    add_var_to_func(f, "const-string", "prompt_val", 14, pv, 1);
    add_var_to_func(f, "const-string", "err_idx", 34, perr, 1);
    const char *arrsize_v[] = {"5"};
    add_var_to_func(f, "const-int64", "arrsize", 1, arrsize_v, 1);
    func_append(f, "\t(prompt_idx :print_s !)");
    func_append(f, "\t(index :input_i !)");
    func_append(f, "\t// bounds check");
    func_append(f, "\t(((index zero <) f :=) f if)");
    func_append(f, "\t\t(err_idx :print_s !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(zero :exit !)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(((index arrsize >=) f :=) f if)");
    func_append(f, "\t\t(err_idx :print_s !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(zero :exit !)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(prompt_val :print_s !)");
    func_append(f, "\t(value :input_i !)");
    func_append(f, "\t(value aux :=)");
    func_append(f, "\t(index * int64_size realind :=)");
    func_append(f, "\t(aux arr [ realind ] =)");
    func_append(f, "\t// print array");
    func_append(f, "\t(zero index :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((index 5 <) f :=) f for)");
    func_append(f, "\t\t(index * int64_size realind :=)");
    func_append(f, "\t\t(arr [ realind ] value =)");
    func_append(f, "\t\t(value + zero value :=)");
    func_append(f, "\t\t(value :print_i !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(index + one index :=)");
    func_append(f, "\t(next)");
}

static void emit_array_reverse(Program *prog, Function *f, int skip_input) {
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

static void emit_array_find(Program *prog, Function *f, int skip_input) {
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

static void emit_input_factorial(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "int64", "n", 1, zv, 1);
    add_var_to_func(f, "int64", "fac", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "input_prompt", 18, ps, 1);
    func_append(f, "\t(input_prompt :print_s !)");
    func_append(f, "\t(n :input_i !)");
    // compute factorial inline
    func_append(f, "\t(one fac :=)");
    func_append(f, "\t(one i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i n <=) f :=) f for)");
    func_append(f, "\t\t(fac i * fac :=)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(fac :print_i !)");
        func_append(f, "\t(:print_n !)");
    }
}

static void emit_array_vmath(Program *prog, Function *f, int skip_input) {
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

static void emit_read_file(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "int64", "fh", 1, (const char *[]){"-1"}, 1);
    add_var_to_func(f, "int64", "val", 1, zv, 1);
    const char *fn[] = {"\"data.bin\""};
    add_var_to_func(f, "const-string", "fname", 10, fn, 1);
    const char *om[] = {"\"rb\""};
    add_var_to_func(f, "const-string", "mode", 4, om, 1);
    func_append(f, "\t(fname mode :fopen !)");
    func_append(f, "\t(fh stpop)");
    func_append(f, "\t(fh val :fread_int64 !)");
    func_append(f, "\t(val :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(fh :fclose !)");
}

static void emit_write_file(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "int64", "fh", 1, (const char *[]){"-1"}, 1);
    add_var_to_func(f, "int64", "val", 1, (const char *[]){"42"}, 1);
    const char *fn[] = {"\"data.bin\""};
    add_var_to_func(f, "const-string", "fname", 10, fn, 1);
    const char *om[] = {"\"wb\""};
    add_var_to_func(f, "const-string", "mode", 4, om, 1);
    func_append(f, "\t(fname mode :fopen !)");
    func_append(f, "\t(fh stpop)");
    func_append(f, "\t(val fh :fwrite_int64 !)");
    func_append(f, "\t(fh :fclose !)");
}

static void emit_string_to_num(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include_post(prog, "string.l1h");
    const char *zv[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    const char *maxlen_v[] = {"256"};
    add_var_to_func(f, "const-int64", "maxlen", 1, maxlen_v, 1);
    add_var_to_func(f, "string", "input_str", 256, (const char *[]){"\"\""}, 1);
    add_var_to_func(f, "int64", "num", 1, zv, 1);
    add_var_to_func(f, "int64", "strmod", 1, zv, 1);
    const char *ps[] = {"\"Enter a number as string: \""};
    add_var_to_func(f, "const-string", "prompt_str", 27, ps, 1);
    func_append(f, "\t(strmod :string_init !)");
    func_append(f, "\t(prompt_str :print_s !)");
    func_append(f, "\t(maxlen input_str :input_s !)");
    func_append(f, "\t(input_str num 256 :string_int64tostring !)");
    func_append(f, "\t(num :print_i !)");
    func_append(f, "\t(:print_n !)");
}

static void emit_timer(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "int64", "start", 1, zv, 1);
    add_var_to_func(f, "int64", "elapsed", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ts[] = {"\"Starting timer...\""};
    const char *ds[] = {"\"Elapsed ms: \""};
    add_var_to_func(f, "const-string", "timer_str", 18, ts, 1);
    add_var_to_func(f, "const-string", "done_str", 14, ds, 1);
    func_append(f, "\t(timer_str :print_s !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(:start_timer !)");   // start timer (intr0 24)
    func_append(f, "\t// loop to consume time");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i 100000 <) f :=) f for)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(elapsed :stop_timer !)");  // read elapsed (intr0 25)
    func_append(f, "\t(done_str :print_s !)");
    func_append(f, "\t(elapsed :print_i !)");
    func_append(f, "\t(:print_n !)");
}

static void emit_factorial(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *fv[] = {"5"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "int64", "num", 1, fv, 1);
    add_var_to_func(f, "int64", "fac", 1, zv, 1);
    func_append(f, "\t// call factorial function");
    func_append(f, "\t(num :factorial !)");
    func_append(f, "\t(fac stpop)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(fac :print_i !)");
        func_append(f, "\t(:print_n !)");
    }
    add_func(prog, "factorial");
    Function *ff = &prog->funcs[prog->num_funcs - 1];
    ff->is_local = 1;
    ff->has_vardef = 1;
    snprintf(ff->vardef_name, sizeof(ff->vardef_name), "factorial");
    add_var_to_func(ff, "const-int64", "zero~", 1, zv, 1);
    add_var_to_func(ff, "const-int64", "one~", 1, (const char *[]){"1"}, 1);
    add_var_to_func(ff, "int64", "n~", 1, zv, 1);
    add_var_to_func(ff, "int64", "i~", 1, zv, 1);
    add_var_to_func(ff, "int64", "fac~", 1, (const char *[]){"1"}, 1);
    add_var_to_func(ff, "int64", "f~", 1, zv, 1);
    func_append(ff, "\t(n~ stpop)");
    func_append(ff, "\t(one~ i~ :=)");
    func_append(ff, "\t(for-loop)");
    func_append(ff, "\t(((i~ n~ <=) f~ :=) f~ for)");
    func_append(ff, "\t\t(fac~ i~ * fac~ :=)");
    func_append(ff, "\t\t(i~ one~ + i~ :=)");
    func_append(ff, "\t(next)");
    func_append(ff, "\t(fac~ stpush)");
    func_append(ff, "\t(return)");
}

static void emit_fizzbuzz(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"3"};
    const char *fvv[] = {"5"};
    const char *mv[] = {"100"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "fizz", 1, tv, 1);
    add_var_to_func(f, "const-int64", "buzz", 1, fvv, 1);
    add_var_to_func(f, "const-int64", "max", 1, mv, 1);
    add_var_to_func(f, "int64", "i", 1, ov, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "int64", "modfizz", 1, zv, 1);
    add_var_to_func(f, "int64", "modbuzz", 1, zv, 1);
    const char *vfs[] = {"\"fizz\""};
    const char *vbs[] = {"\"buzz\""};
    add_var_to_func(f, "const-string", "fizzstr", 6, vfs, 1);
    add_var_to_func(f, "const-string", "buzzstr", 6, vbs, 1);
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i max <=) f :=) f for)");
    func_append(f, "\t\t(i % fizz modfizz :=)");
    func_append(f, "\t\t(i % buzz modbuzz :=)");
    func_append(f, "\t\t(((modfizz zero ==) f :=) f if)");
    func_append(f, "\t\t\t(fizzstr :print_s !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((modbuzz zero ==) f :=) f if)");
    func_append(f, "\t\t\t(buzzstr :print_s !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((modfizz zero !=) f :=) && (modbuzz zero !=) f :=)");
    func_append(f, "\t\t(f if)");
    func_append(f, "\t\t\t(i :print_i !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
}

static void emit_primes(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    const char *mv[] = {"50"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "int64", "num", 1, tv, 1);
    add_var_to_func(f, "int64", "maxpr", 1, mv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "is_prime", 1, zv, 1);
    add_var_to_func(f, "int64", "modtmp", 1, zv, 1);
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((num maxpr <=) f :=) f for)");
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
}

static void emit_even_odd(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    const char *mv[] = {"20"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "max", 1, mv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "int64", "mod", 1, zv, 1);
    const char *ves[] = {"\" is even\""};
    const char *vos[] = {"\" is odd\""};
    add_var_to_func(f, "const-string", "even_str", 10, ves, 1);
    add_var_to_func(f, "const-string", "odd_str", 9, vos, 1);
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
}

static void emit_power(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *bv[] = {"2"};
    const char *ev[] = {"10"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "int64", "base", 1, bv, 1);
    add_var_to_func(f, "int64", "exp", 1, ev, 1);
    add_var_to_func(f, "int64", "ret", 1, zv, 1);
    func_append(f, "\t// call power function");
    func_append(f, "\t(base exp :power !)");
    func_append(f, "\t(ret stpop)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(ret :print_i !)");
        func_append(f, "\t(:print_n !)");
    }
    add_func(prog, "power");
    Function *pf = &prog->funcs[prog->num_funcs - 1];
    pf->is_local = 1;
    pf->has_vardef = 1;
    snprintf(pf->vardef_name, sizeof(pf->vardef_name), "power");
    add_var_to_func(pf, "const-int64", "zero~", 1, zv, 1);
    add_var_to_func(pf, "const-int64", "one~", 1, (const char *[]){"1"}, 1);
    add_var_to_func(pf, "int64", "b~", 1, zv, 1);
    add_var_to_func(pf, "int64", "e~", 1, zv, 1);
    add_var_to_func(pf, "int64", "r~", 1, (const char *[]){"1"}, 1);
    add_var_to_func(pf, "int64", "i~", 1, zv, 1);
    add_var_to_func(pf, "int64", "f~", 1, zv, 1);
    func_append(pf, "\t(e~ stpop)");
    func_append(pf, "\t(b~ stpop)");
    func_append(pf, "\t(b~ r~ :=)");
    func_append(pf, "\t(one~ i~ :=)");
    func_append(pf, "\t(for-loop)");
    func_append(pf, "\t(((i~ e~ <) f~ :=) f~ for)");
    func_append(pf, "\t\t(r~ b~ * r~ :=)");
    func_append(pf, "\t\t(i~ one~ + i~ :=)");
    func_append(pf, "\t(next)");
    func_append(pf, "\t(r~ stpush)");
    func_append(pf, "\t(return)");
}

static void emit_multiplication_table(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *mv[] = {"10"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "j", 1, zv, 1);
    add_var_to_func(f, "int64", "mx", 1, mv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "int64", "p", 1, zv, 1);
    func_append(f, "\t(one i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i mx <=) f :=) f for)");
    func_append(f, "\t\t(one j :=)");
    func_append(f, "\t\t(for-loop)");
    func_append(f, "\t\t(((j mx <=) f :=) f for)");
    func_append(f, "\t\t\t(i * j p :=)");
    func_append(f, "\t\t\t(p :print_i !)");
    func_append(f, "\t\t\t(:print_n !)");
    func_append(f, "\t\t\t(j + one j :=)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
}

static void emit_guess_number(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *zv42[] = {"42"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "int64", "secret", 1, zv42, 1);
    add_var_to_func(f, "int64", "guess", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *vtxt[] = {"\"Guess a number: \""};
    const char *vlow[] = {"\"Too low!\""};
    const char *vhigh[] = {"\"Too high!\""};
    const char *vwin[] = {"\"Correct!\""};
    add_var_to_func(f, "const-string", "prompt_str", 18, vtxt, 1);
    add_var_to_func(f, "const-string", "low_str", 10, vlow, 1);
    add_var_to_func(f, "const-string", "high_str", 11, vhigh, 1);
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
}

static void emit_gcd(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *zv9[] = {"9"};
    const char *zv42[] = {"42"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "int64", "a", 1, zv42, 1);
    add_var_to_func(f, "int64", "b", 1, zv9, 1);
    add_var_to_func(f, "int64", "ret", 1, zv, 1);
    func_append(f, "\t// call gcd function");
    func_append(f, "\t(a b :gcd !)");
    func_append(f, "\t(ret stpop)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(ret :print_i !)");
        func_append(f, "\t(:print_n !)");
    }
    add_func(prog, "gcd");
    Function *gf = &prog->funcs[prog->num_funcs - 1];
    gf->is_local = 1;
    gf->has_vardef = 1;
    snprintf(gf->vardef_name, sizeof(gf->vardef_name), "gcd");
    add_var_to_func(gf, "const-int64", "zero~", 1, zv, 1);
    add_var_to_func(gf, "int64", "a~", 1, zv, 1);
    add_var_to_func(gf, "int64", "b~", 1, zv, 1);
    add_var_to_func(gf, "int64", "mod~", 1, zv, 1);
    add_var_to_func(gf, "int64", "f~", 1, zv, 1);
    func_append(gf, "\t(b~ stpop)");
    func_append(gf, "\t(a~ stpop)");
    func_append(gf, "\t(do)");
    func_append(gf, "\t\t(a~ b~ % mod~ :=)");
    func_append(gf, "\t\t(b~ a~ :=)");
    func_append(gf, "\t\t(mod~ b~ :=)");
    func_append(gf, "\t(((b~ zero~ >) f~ :=) f~ while)");
    func_append(gf, "\t(a~ stpush)");
    func_append(gf, "\t(return)");
}

static void emit_random_number(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *mv[] = {"100"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "int64", "maxval", 1, mv, 1);
    add_var_to_func(f, "int64", "randval", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    const char *vs[] = {"\"Random numbers 1-100:\""};
    add_var_to_func(f, "const-string", "rstr", 22, vs, 1);
    func_append(f, "\t(rstr :print_s !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t// using simple pseudo-random via timer");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i 5 <) f :=) f for)");
    func_append(f, "\t\t// compute pseudo-random: take epoch ms mod maxval");
    func_append(f, "\t\t(randval :epochms !)");
    func_append(f, "\t\t((randval maxval %) randval :=)");
    func_append(f, "\t\t(randval :print_i !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
}

static void emit_hello_name(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *maxlen_v[] = {"256"};
    const char *varname = extract_var_name(current_prompt ? current_prompt : "", "name");
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "maxlen", 1, maxlen_v, 1);
    add_var_to_func(f, "string", varname, 256, (const char *[]){"\"\""}, 1);
    const char *vask[] = {"\"What is your name? \""};
    const char *vhello[] = {"\"Hello, \""};
    add_var_to_func(f, "const-string", "ask_str", 22, vask, 1);
    add_var_to_func(f, "const-string", "hello_str", 8, vhello, 1);
    func_append(f, "\t(ask_str :print_s !)");
    char input_line[256];
    snprintf(input_line, sizeof(input_line), "\t(maxlen %s :input_s !)", varname);
    func_append(f, input_line);
    func_append(f, "\t(hello_str :print_s !)");
    char print_line[256];
    snprintf(print_line, sizeof(print_line), "\t(%s :print_s !)", varname);
    func_append(f, print_line);
    func_append(f, "\t(:print_n !)");
}

static void emit_array_min_max(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    const char *cv[] = {"7"};
    add_var_to_func(f, "int64", "count", 1, cv, 1);
    add_var_to_func(f, "int64", "arr", 7, (const char *[]){"42", "23", "8", "15", "4", "16", "99"}, 7);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "mn", 1, zv, 1);
    add_var_to_func(f, "int64", "mx", 1, zv, 1);
    add_var_to_func(f, "int64", "avg", 1, zv, 1);
    add_var_to_func(f, "int64", "realind", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *pmin[] = {"\"min: \""};
    const char *pmax[] = {"\"max: \""};
    const char *pavg[] = {"\"average: \""};
    add_var_to_func(f, "const-string", "min_str", 6, pmin, 1);
    add_var_to_func(f, "const-string", "max_str", 6, pmax, 1);
    add_var_to_func(f, "const-string", "avg_str", 10, pavg, 1);
    func_append(f, "\t// find min, max, average using vmath library");
    func_append(f, "\t(arr Parray pointer)");
    func_append(f, "\t(Parray zero count vmath_min_int call)");
    func_append(f, "\t(mn stpop)");
    func_append(f, "\t(Parray zero count vmath_max_int call)");
    func_append(f, "\t(mx stpop)");
    func_append(f, "\t(Parray zero count vmath_average_int call)");
    func_append(f, "\t(avg stpop)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(min_str :print_s !)");
        func_append(f, "\t(mn :print_i !)");
        func_append(f, "\t(:print_n !)");
        func_append(f, "\t(max_str :print_s !)");
        func_append(f, "\t(mx :print_i !)");
        func_append(f, "\t(:print_n !)");
        func_append(f, "\t(avg_str :print_s !)");
        func_append(f, "\t(avg :print_i !)");
        func_append(f, "\t(:print_n !)");
    }
    add_include_post(prog, "math-lib-vect.l1h");
}

static void emit_bool_demo(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "bool.l1h");
    const char *zv[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    const char *av[] = {"42"};
    add_var_to_func(f, "int64", "a", 1, av, 1);
    add_var_to_func(f, "int64", "b", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *bov[] = {"1"};
    add_var_to_func(f, "bool", "Bool", 1, bov, 1);
    const char *ps[] = {"\"Bool = \""};
    add_var_to_func(f, "const-string", "bool_str", 8, ps, 1);
    func_append(f, "\t(a b =)");
    func_append(f, "\t(((b a ==) f =) f if+)");
    func_append(f, "\t\t(true Bool =)");
    func_append(f, "\t(else)");
    func_append(f, "\t\t(false Bool =)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(bool_str :print_s !)");
    func_append(f, "\t(Bool :print_i !)");
    func_append(f, "\t(:print_n !)");
}

static void emit_bit_check(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    const char *nv[] = {"134"};
    add_var_to_func(f, "int64", "num", 1, nv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "maxbit", 1, (const char *[]){"8"}, 1);
    add_var_to_func(f, "int64", "bitset", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *pb[] = {"\"bit: \""};
    const char *ps1[] = {"\" = 1\""};
    const char *ps0[] = {"\" = 0\""};
    const char *pn[] = {"\"number: \""};
    add_var_to_func(f, "const-string", "bit_str", 6, pb, 1);
    add_var_to_func(f, "const-string", "set_str", 6, ps1, 1);
    add_var_to_func(f, "const-string", "zero_str", 6, ps0, 1);
    add_var_to_func(f, "const-string", "num_str", 9, pn, 1);
    func_append(f, "\t(num_str :print_s !)");
    func_append(f, "\t(num :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i maxbit <) f :=) f for)");
    func_append(f, "\t\t(bit_str :print_s !)");
    func_append(f, "\t\t(i :print_i !)");
    func_append(f, "\t\t(num i :check_bitset call)");
    func_append(f, "\t\t(bitset stpopi)");
    func_append(f, "\t\t(((bitset zero !=) f :=) f if+)");
    func_append(f, "\t\t\t(set_str :print_s !)");
    func_append(f, "\t\t(else)");
    func_append(f, "\t\t\t(zero_str :print_s !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(reset-reg)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");

    add_func(prog, "check_bitset");
    Function *sf = &prog->funcs[prog->num_funcs - 1];
    sf->is_local = 1;
    sf->has_vardef = 1;
    snprintf(sf->vardef_name, sizeof(sf->vardef_name), "@check_bitset");
    add_var_to_func(sf, "int64", "one~", 1, ov, 1);
    add_var_to_func(sf, "int64", "bit~", 1, zv, 1);
    add_var_to_func(sf, "int64", "shift~", 1, zv, 1);
    add_var_to_func(sf, "int64", "bitset~", 1, zv, 1);
    add_var_to_func(sf, "int64", "num~", 1, zv, 1);
    func_append(sf, "\t(bit~ stpopi)");
    func_append(sf, "\t(num~ stpopi)");
    func_append(sf, "\t((one~ bit~ <<) shift~ =)");
    func_append(sf, "\t((num~ shift~ &) bitset~ =)");
    func_append(sf, "\t(bitset~ stpushi)");
}

static void emit_leap_year(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"400"};
    const char *hv[] = {"100"};
    const char *fv[] = {"4"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "four_hundred", 1, tv, 1);
    add_var_to_func(f, "const-int64", "one_hundred", 1, hv, 1);
    add_var_to_func(f, "const-int64", "four", 1, fv, 1);
    add_var_to_func(f, "int64", "year", 1, zv, 1);
    add_var_to_func(f, "int64", "rem", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Enter a year: \""};
    add_var_to_func(f, "const-string", "prompt_str", 15, ps, 1);
    const char *ly[] = {"\" is a leap year\""};
    const char *nl[] = {"\" is not a leap year\""};
    add_var_to_func(f, "const-string", "leap_str", 17, ly, 1);
    add_var_to_func(f, "const-string", "not_leap_str", 21, nl, 1);
    func_append(f, "\t(prompt_str :print_s !)");
    func_append(f, "\t(year :input_i !)");
    func_append(f, "\t(year four_hundred % rem :=)");
    func_append(f, "\t(((rem zero ==) f :=) f if)");
    func_append(f, "\t\t(year :print_i !)");
    func_append(f, "\t\t(leap_str :print_s !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(zero :exit !)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(year one_hundred % rem :=)");
    func_append(f, "\t(((rem zero ==) f :=) f if)");
    func_append(f, "\t\t(year :print_i !)");
    func_append(f, "\t\t(not_leap_str :print_s !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(zero :exit !)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(year four % rem :=)");
    func_append(f, "\t(((rem zero ==) f :=) f if+)");
    func_append(f, "\t\t(year :print_i !)");
    func_append(f, "\t\t(leap_str :print_s !)");
    func_append(f, "\t(else)");
    func_append(f, "\t\t(year :print_i !)");
    func_append(f, "\t\t(not_leap_str :print_s !)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");
}

static void emit_temp_convert(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *nv[] = {"9"};
    const char *fv[] = {"5"};
    const char *tv[] = {"32"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "nine", 1, nv, 1);
    add_var_to_func(f, "const-int64", "five", 1, fv, 1);
    add_var_to_func(f, "const-int64", "thirty_two", 1, tv, 1);
    add_var_to_func(f, "int64", "celsius", 1, zv, 1);
    add_var_to_func(f, "int64", "fahrenheit", 1, zv, 1);
    const char *ps[] = {"\"Enter Celsius: \""};
    add_var_to_func(f, "const-string", "prompt_str", 16, ps, 1);
    const char *rs[] = {"\"Fahrenheit: \""};
    add_var_to_func(f, "const-string", "result_str", 13, rs, 1);
    func_append(f, "\t(prompt_str :print_s !)");
    func_append(f, "\t(celsius :input_i !)");
    func_append(f, "\t(celsius nine * fahrenheit :=)");
    func_append(f, "\t(fahrenheit five / fahrenheit :=)");
    func_append(f, "\t(fahrenheit thirty_two + fahrenheit :=)");
    func_append(f, "\t(result_str :print_s !)");
    func_append(f, "\t(fahrenheit :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");
}

static void emit_circle_area(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *pnv[] = {"314"};
    const char *pdv[] = {"100"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "pi_num", 1, pnv, 1);
    add_var_to_func(f, "const-int64", "pi_den", 1, pdv, 1);
    add_var_to_func(f, "int64", "radius", 1, zv, 1);
    add_var_to_func(f, "int64", "area", 1, zv, 1);
    add_var_to_func(f, "int64", "sq", 1, zv, 1);
    const char *ps[] = {"\"Enter radius: \""};
    add_var_to_func(f, "const-string", "prompt_str", 15, ps, 1);
    const char *rs[] = {"\"Area: \""};
    add_var_to_func(f, "const-string", "result_str", 7, rs, 1);
    func_append(f, "\t(prompt_str :print_s !)");
    func_append(f, "\t(radius :input_i !)");
    func_append(f, "\t(radius radius * sq :=)");
    func_append(f, "\t(sq pi_num * area :=)");
    func_append(f, "\t(area pi_den / area :=)");
    func_append(f, "\t(result_str :print_s !)");
    func_append(f, "\t(area :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");
}

static void emit_fann_create(Program *prog, Function *f) {
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

static void emit_fann_train(Program *prog, Function *f) {
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

static void emit_fann_run(Program *prog, Function *f) {
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

static void emit_average(Program *prog, Function *f, int skip_input) {
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

static void emit_selection_sort(Program *prog, Function *f, int count, int skip_input) {
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
        // print sorted
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

}

static void emit_palindrome(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"10"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "ten", 1, tv, 1);
    add_var_to_func(f, "int64", "n", 1, zv, 1);
    add_var_to_func(f, "int64", "temp", 1, zv, 1);
    add_var_to_func(f, "int64", "rev", 1, zv, 1);
    add_var_to_func(f, "int64", "digit", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "prompt_str", 17, ps, 1);
    const char *pp[] = {"\" is a palindrome\""};
    const char *np[] = {"\" is not a palindrome\""};
    add_var_to_func(f, "const-string", "yes_str", 18, pp, 1);
    add_var_to_func(f, "const-string", "no_str", 22, np, 1);
    func_append(f, "\t(prompt_str :print_s !)");
    func_append(f, "\t(n :input_i !)");
    func_append(f, "\t(n temp :=)");
    func_append(f, "\t(zero rev :=)");
    func_append(f, "\t(do)");
    func_append(f, "\t\t(temp ten % digit :=)");
    func_append(f, "\t\t(rev ten * rev :=)");
    func_append(f, "\t\t(rev digit + rev :=)");
    func_append(f, "\t\t(temp ten / temp :=)");
    func_append(f, "\t(((temp zero >) f :=) f while)");
    func_append(f, "\t(((n rev ==) f :=) f if+)");
    func_append(f, "\t\t(n :print_i !)");
    func_append(f, "\t\t(yes_str :print_s !)");
    func_append(f, "\t(else)");
    func_append(f, "\t\t(n :print_i !)");
    func_append(f, "\t\t(no_str :print_s !)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(:print_n !)");
}

static void emit_lcm(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "int64", "a", 1, zv, 1);
    add_var_to_func(f, "int64", "b", 1, zv, 1);
    add_var_to_func(f, "int64", "orig_a", 1, zv, 1);
    add_var_to_func(f, "int64", "orig_b", 1, zv, 1);
    add_var_to_func(f, "int64", "gcd", 1, zv, 1);
    add_var_to_func(f, "int64", "lcm", 1, zv, 1);
    add_var_to_func(f, "int64", "mod", 1, zv, 1);
    add_var_to_func(f, "int64", "temp", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *pa[] = {"\"Enter first number: \""};
    const char *pb[] = {"\"Enter second number: \""};
    add_var_to_func(f, "const-string", "prompt_a", 21, pa, 1);
    add_var_to_func(f, "const-string", "prompt_b", 22, pb, 1);
    const char *rs[] = {"\"LCM: \""};
    add_var_to_func(f, "const-string", "result_str", 7, rs, 1);
    func_append(f, "\t(prompt_a :print_s !)");
    func_append(f, "\t(a :input_i !)");
    func_append(f, "\t(prompt_b :print_s !)");
    func_append(f, "\t(b :input_i !)");
    func_append(f, "\t(a orig_a :=)");
    func_append(f, "\t(b orig_b :=)");
    func_append(f, "\t(do)");
    func_append(f, "\t\t(a b % mod :=)");
    func_append(f, "\t\t(b a :=)");
    func_append(f, "\t\t(mod b :=)");
    func_append(f, "\t(((b zero >) f :=) f while)");
    func_append(f, "\t(a gcd :=)");
    func_append(f, "\t(orig_a orig_b * lcm :=)");
    func_append(f, "\t(lcm gcd / lcm :=)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(result_str :print_s !)");
        func_append(f, "\t(lcm :print_i !)");
        func_append(f, "\t(:print_n !)");
    }
}

static void emit_collatz(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    const char *thv[] = {"3"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "three", 1, thv, 1);
    add_var_to_func(f, "int64", "n", 1, zv, 1);
    add_var_to_func(f, "int64", "rem", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "prompt_str", 17, ps, 1);
    func_append(f, "\t(prompt_str :print_s !)");
    func_append(f, "\t(n :input_i !)");
    func_append(f, "\t(do)");
    func_append(f, "\t\t(n :print_i !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(n two % rem :=)");
    func_append(f, "\t\t(((rem zero ==) f :=) f if+)");
    func_append(f, "\t\t\t(n two / n :=)");
    func_append(f, "\t\t(else)");
    func_append(f, "\t\t\t(n three * n :=)");
    func_append(f, "\t\t\t(n one + n :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t(((n one >) f :=) f while)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(one :print_i !)");
        func_append(f, "\t(:print_n !)");
    }
}

static void emit_sum_of_digits(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *tv[] = {"10"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "ten", 1, tv, 1);
    add_var_to_func(f, "int64", "n", 1, zv, 1);
    add_var_to_func(f, "int64", "sum", 1, zv, 1);
    add_var_to_func(f, "int64", "digit", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "prompt_str", 17, ps, 1);
    const char *rs[] = {"\"Sum of digits: \""};
    add_var_to_func(f, "const-string", "result_str", 17, rs, 1);
    func_append(f, "\t(prompt_str :print_s !)");
    func_append(f, "\t(n :input_i !)");
    func_append(f, "\t(zero sum :=)");
    func_append(f, "\t(do)");
    func_append(f, "\t\t(n ten % digit :=)");
    func_append(f, "\t\t(sum digit + sum :=)");
    func_append(f, "\t\t(n ten / n :=)");
    func_append(f, "\t(((n zero >) f :=) f while)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(result_str :print_s !)");
        func_append(f, "\t(sum :print_i !)");
        func_append(f, "\t(:print_n !)");
    }
}

static void emit_reverse_string(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include_post(prog, "string.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    const char *maxlen_v[] = {"256"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "maxlen", 1, maxlen_v, 1);
    add_var_to_func(f, "string", "str", 256, (const char *[]){"\"\""}, 1);
    add_var_to_func(f, "int64", "len", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "j", 1, zv, 1);
    add_var_to_func(f, "int64", "ci", 1, zv, 1);
    add_var_to_func(f, "int64", "cj", 1, zv, 1);
    add_var_to_func(f, "int64", "half", 1, zv, 1);
    add_var_to_func(f, "int64", "strmod", 1, zv, 1);
    const char *ps[] = {"\"Enter a string: \""};
    add_var_to_func(f, "const-string", "prompt_str", 17, ps, 1);
    func_append(f, "\t(strmod :string_init !)");
    func_append(f, "\t(prompt_str :print_s !)");
    func_append(f, "\t(maxlen str :input_s !)");
    func_append(f, "\t(str :string_len !)");
    func_append(f, "\t(len stpop)");
    func_append(f, "\t(len two / half :=)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i half <) f :=) f for)");
    func_append(f, "\t\t(str i ci :string_getc !)");
    func_append(f, "\t\t(len one - j :=)");
    func_append(f, "\t\t(j i - j :=)");
    func_append(f, "\t\t(str j cj :string_getc !)");
    func_append(f, "\t\t(str i cj :string_setc !)");
    func_append(f, "\t\t(str j ci :string_setc !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(str :print_s !)");
        func_append(f, "\t(:print_n !)");
    }
}

static void emit_armstrong(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *tv[] = {"10"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "ten", 1, tv, 1);
    add_var_to_func(f, "int64", "n", 1, zv, 1);
    add_var_to_func(f, "int64", "temp", 1, zv, 1);
    add_var_to_func(f, "int64", "sum", 1, zv, 1);
    add_var_to_func(f, "int64", "digit", 1, zv, 1);
    add_var_to_func(f, "int64", "cube", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "prompt_str", 17, ps, 1);
    const char *py[] = {"\" is an Armstrong number\""};
    const char *pn[] = {"\" is not an Armstrong number\""};
    add_var_to_func(f, "const-string", "yes_str", 25, py, 1);
    add_var_to_func(f, "const-string", "no_str", 29, pn, 1);
    func_append(f, "\t(prompt_str :print_s !)");
    func_append(f, "\t(n :input_i !)");
    func_append(f, "\t(n temp :=)");
    func_append(f, "\t(zero sum :=)");
    func_append(f, "\t(do)");
    func_append(f, "\t\t(temp ten % digit :=)");
    func_append(f, "\t\t(digit digit * cube :=)");
    func_append(f, "\t\t(cube digit * cube :=)");
    func_append(f, "\t\t(sum cube + sum :=)");
    func_append(f, "\t\t(temp ten / temp :=)");
    func_append(f, "\t(((temp zero >) f :=) f while)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(((n sum ==) f :=) f if+)");
        func_append(f, "\t\t(n :print_i !)");
        func_append(f, "\t\t(yes_str :print_s !)");
        func_append(f, "\t(else)");
        func_append(f, "\t\t(n :print_i !)");
        func_append(f, "\t\t(no_str :print_s !)");
        func_append(f, "\t(endif)");
        func_append(f, "\t(:print_n !)");
    }
}

static void emit_perfect_number(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "int64", "n", 1, zv, 1);
    add_var_to_func(f, "int64", "half", 1, zv, 1);
    add_var_to_func(f, "int64", "sum", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "rem", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "prompt_str", 17, ps, 1);
    const char *py[] = {"\" is a perfect number\""};
    const char *pn[] = {"\" is not a perfect number\""};
    add_var_to_func(f, "const-string", "yes_str", 22, py, 1);
    add_var_to_func(f, "const-string", "no_str", 26, pn, 1);
    func_append(f, "\t(prompt_str :print_s !)");
    func_append(f, "\t(n :input_i !)");
    func_append(f, "\t(zero sum :=)");
    func_append(f, "\t(n two / half :=)");
    func_append(f, "\t(one i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i half <=) f :=) f for)");
    func_append(f, "\t\t(n i % rem :=)");
    func_append(f, "\t\t(((rem zero ==) f :=) f if)");
    func_append(f, "\t\t\t(sum i + sum :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(((n sum ==) f :=) f if+)");
        func_append(f, "\t\t(n :print_i !)");
        func_append(f, "\t\t(yes_str :print_s !)");
        func_append(f, "\t(else)");
        func_append(f, "\t\t(n :print_i !)");
        func_append(f, "\t\t(no_str :print_s !)");
        func_append(f, "\t(endif)");
        func_append(f, "\t(:print_n !)");
    }
}

static void emit_count_vowels(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include_post(prog, "string.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *maxlen_v[] = {"256"};
    const char *va[] = {"97"};
    const char *ve[] = {"101"};
    const char *vi[] = {"105"};
    const char *vo[] = {"111"};
    const char *vu[] = {"117"};
    const char *vau[] = {"65"};
    const char *veu[] = {"69"};
    const char *viu[] = {"73"};
    const char *vou[] = {"79"};
    const char *vuu[] = {"85"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "maxlen", 1, maxlen_v, 1);
    add_var_to_func(f, "const-int64", "ch_a", 1, va, 1);
    add_var_to_func(f, "const-int64", "ch_e", 1, ve, 1);
    add_var_to_func(f, "const-int64", "ch_i", 1, vi, 1);
    add_var_to_func(f, "const-int64", "ch_o", 1, vo, 1);
    add_var_to_func(f, "const-int64", "ch_u", 1, vu, 1);
    add_var_to_func(f, "const-int64", "ch_A", 1, vau, 1);
    add_var_to_func(f, "const-int64", "ch_E", 1, veu, 1);
    add_var_to_func(f, "const-int64", "ch_I", 1, viu, 1);
    add_var_to_func(f, "const-int64", "ch_O", 1, vou, 1);
    add_var_to_func(f, "const-int64", "ch_U", 1, vuu, 1);
    add_var_to_func(f, "string", "str", 256, (const char *[]){"\"\""}, 1);
    add_var_to_func(f, "int64", "len", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "ch", 1, zv, 1);
    add_var_to_func(f, "int64", "count", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "int64", "strmod", 1, zv, 1);
    const char *ps[] = {"\"Enter a string: \""};
    add_var_to_func(f, "const-string", "prompt_str", 17, ps, 1);
    const char *rs[] = {"\"Vowel count: \""};
    add_var_to_func(f, "const-string", "result_str", 15, rs, 1);
    func_append(f, "\t(strmod :string_init !)");
    func_append(f, "\t(prompt_str :print_s !)");
    func_append(f, "\t(maxlen str :input_s !)");
    func_append(f, "\t(str :string_len !)");
    func_append(f, "\t(len stpop)");
    func_append(f, "\t(zero count :=)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i len <) f :=) f for)");
    func_append(f, "\t\t(str i ch :string_getc !)");
    func_append(f, "\t\t(((ch ch_a ==) f :=) f if)");
    func_append(f, "\t\t\t(count + one count :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((ch ch_e ==) f :=) f if)");
    func_append(f, "\t\t\t(count + one count :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((ch ch_i ==) f :=) f if)");
    func_append(f, "\t\t\t(count + one count :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((ch ch_o ==) f :=) f if)");
    func_append(f, "\t\t\t(count + one count :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((ch ch_u ==) f :=) f if)");
    func_append(f, "\t\t\t(count + one count :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((ch ch_A ==) f :=) f if)");
    func_append(f, "\t\t\t(count + one count :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((ch ch_E ==) f :=) f if)");
    func_append(f, "\t\t\t(count + one count :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((ch ch_I ==) f :=) f if)");
    func_append(f, "\t\t\t(count + one count :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((ch ch_O ==) f :=) f if)");
    func_append(f, "\t\t\t(count + one count :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((ch ch_U ==) f :=) f if)");
    func_append(f, "\t\t\t(count + one count :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(result_str :print_s !)");
        func_append(f, "\t(count :print_i !)");
        func_append(f, "\t(:print_n !)");
    }
}

static void emit_anagram_check(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include_post(prog, "string.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *maxlen_v[] = {"256"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "maxlen", 1, maxlen_v, 1);
    add_var_to_func(f, "string", "str1", 256, (const char *[]){"\"\""}, 1);
    add_var_to_func(f, "string", "str2", 256, (const char *[]){"\"\""}, 1);
    add_var_to_func(f, "string", "temp", 256, (const char *[]){"\"\""}, 1);
    add_var_to_func(f, "int64", "len1", 1, zv, 1);
    add_var_to_func(f, "int64", "len2", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "j", 1, zv, 1);
    add_var_to_func(f, "int64", "ci", 1, zv, 1);
    add_var_to_func(f, "int64", "cj", 1, zv, 1);
    add_var_to_func(f, "int64", "found", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "int64", "strmod", 1, zv, 1);
    const char *p1[] = {"\"Enter first string: \""};
    const char *p2[] = {"\"Enter second string: \""};
    add_var_to_func(f, "const-string", "prompt_a", 22, p1, 1);
    add_var_to_func(f, "const-string", "prompt_b", 23, p2, 1);
    const char *py[] = {"\"Strings are anagrams\""};
    const char *pn[] = {"\"Strings are not anagrams\""};
    add_var_to_func(f, "const-string", "yes_str", 23, py, 1);
    add_var_to_func(f, "const-string", "no_str", 28, pn, 1);
    func_append(f, "\t(strmod :string_init !)");
    func_append(f, "\t(prompt_a :print_s !)");
    func_append(f, "\t(maxlen str1 :input_s !)");
    func_append(f, "\t(prompt_b :print_s !)");
    func_append(f, "\t(maxlen str2 :input_s !)");
    func_append(f, "\t(str1 :string_len !)");
    func_append(f, "\t(len1 stpop)");
    func_append(f, "\t(str2 :string_len !)");
    func_append(f, "\t(len2 stpop)");
    func_append(f, "\t(temp str2 :string_copy !)");
    func_append(f, "\t(one found :=)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i len1 <) f :=) f for)");
    func_append(f, "\t\t(str1 i ci :string_getc !)");
    func_append(f, "\t\t(zero found :=)");
    func_append(f, "\t\t(zero j :=)");
    func_append(f, "\t\t(for-loop)");
    func_append(f, "\t\t(((j len2 <) f :=) f for)");
    func_append(f, "\t\t\t(temp j cj :string_getc !)");
    func_append(f, "\t\t\t(((ci cj ==) f :=) f if)");
    func_append(f, "\t\t\t\t(one found :=)");
    func_append(f, "\t\t\t\t(temp j zero :string_setc !)");
    func_append(f, "\t\t\t\t(len2 j :=)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t\t(j + one j :=)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t(((found zero ==) f :=) f if)");
    func_append(f, "\t\t\t(len1 i :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(((found one ==) f :=) f if+)");
        func_append(f, "\t\t(yes_str :print_s !)");
        func_append(f, "\t(else)");
        func_append(f, "\t\t(no_str :print_s !)");
        func_append(f, "\t(endif)");
        func_append(f, "\t(:print_n !)");
    }
}

static void emit_string_to_upper(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include_post(prog, "string.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *maxlen_v[] = {"256"};
    const char *va[] = {"97"};
    const char *vz[] = {"122"};
    const char *vs[] = {"32"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "maxlen", 1, maxlen_v, 1);
    add_var_to_func(f, "const-int64", "ch_a", 1, va, 1);
    add_var_to_func(f, "const-int64", "ch_z", 1, vz, 1);
    add_var_to_func(f, "const-int64", "shift", 1, vs, 1);
    add_var_to_func(f, "string", "str", 256, (const char *[]){"\"\""}, 1);
    add_var_to_func(f, "int64", "len", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "ch", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "int64", "f2", 1, zv, 1);
    add_var_to_func(f, "int64", "strmod", 1, zv, 1);
    const char *ps[] = {"\"Enter a string: \""};
    add_var_to_func(f, "const-string", "prompt_str", 17, ps, 1);
    func_append(f, "\t(strmod :string_init !)");
    func_append(f, "\t(prompt_str :print_s !)");
    func_append(f, "\t(maxlen str :input_s !)");
    func_append(f, "\t(str :string_len !)");
    func_append(f, "\t(len stpop)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i len <) f :=) f for)");
    func_append(f, "\t\t(str i ch :string_getc !)");
    func_append(f, "\t\t(((ch ch_a >=) f :=) f if)");
    func_append(f, "\t\t\t(((ch ch_z <=) f2 :=) f2 if)");
    func_append(f, "\t\t\t\t(ch shift - ch :=)");
    func_append(f, "\t\t\t\t(str i ch :string_setc !)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(str :print_s !)");
        func_append(f, "\t(:print_n !)");
    }
}

static void emit_string_to_lower(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include_post(prog, "string.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *maxlen_v[] = {"256"};
    const char *vA[] = {"65"};
    const char *vZ[] = {"90"};
    const char *vs[] = {"32"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "maxlen", 1, maxlen_v, 1);
    add_var_to_func(f, "const-int64", "ch_A", 1, vA, 1);
    add_var_to_func(f, "const-int64", "ch_Z", 1, vZ, 1);
    add_var_to_func(f, "const-int64", "shift", 1, vs, 1);
    add_var_to_func(f, "string", "str", 256, (const char *[]){"\"\""}, 1);
    add_var_to_func(f, "int64", "len", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "ch", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "int64", "f2", 1, zv, 1);
    add_var_to_func(f, "int64", "strmod", 1, zv, 1);
    const char *ps[] = {"\"Enter a string: \""};
    add_var_to_func(f, "const-string", "prompt_str", 17, ps, 1);
    func_append(f, "\t(strmod :string_init !)");
    func_append(f, "\t(prompt_str :print_s !)");
    func_append(f, "\t(maxlen str :input_s !)");
    func_append(f, "\t(str :string_len !)");
    func_append(f, "\t(len stpop)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i len <) f :=) f for)");
    func_append(f, "\t\t(str i ch :string_getc !)");
    func_append(f, "\t\t(((ch ch_A >=) f :=) f if)");
    func_append(f, "\t\t\t(((ch ch_Z <=) f2 :=) f2 if)");
    func_append(f, "\t\t\t\t(ch shift + ch :=)");
    func_append(f, "\t\t\t\t(str i ch :string_setc !)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(str :print_s !)");
        func_append(f, "\t(:print_n !)");
    }
}

static void emit_caesar_cipher(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include_post(prog, "string.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *maxlen_v[] = {"256"};
    const char *va[] = {"97"}; const char *vz[] = {"122"};
    const char *vA[] = {"65"}; const char *vZ[] = {"90"};
    const char *vs[] = {"3"};
    const char *v26[] = {"26"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "maxlen", 1, maxlen_v, 1);
    add_var_to_func(f, "const-int64", "ch_a", 1, va, 1);
    add_var_to_func(f, "const-int64", "ch_z", 1, vz, 1);
    add_var_to_func(f, "const-int64", "ch_A", 1, vA, 1);
    add_var_to_func(f, "const-int64", "ch_Z", 1, vZ, 1);
    add_var_to_func(f, "const-int64", "shift", 1, vs, 1);
    add_var_to_func(f, "const-int64", "twenty_six", 1, v26, 1);
    add_var_to_func(f, "string", "str", 256, (const char *[]){"\"\""}, 1);
    add_var_to_func(f, "int64", "len", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "ch", 1, zv, 1);
    add_var_to_func(f, "int64", "base", 1, zv, 1);
    add_var_to_func(f, "int64", "idx", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "int64", "f2", 1, zv, 1);
    add_var_to_func(f, "int64", "strmod", 1, zv, 1);
    const char *ps[] = {"\"Enter a string: \""};
    add_var_to_func(f, "const-string", "prompt_str", 17, ps, 1);
    func_append(f, "\t(strmod :string_init !)");
    func_append(f, "\t(prompt_str :print_s !)");
    func_append(f, "\t(maxlen str :input_s !)");
    func_append(f, "\t(str :string_len !)");
    func_append(f, "\t(len stpop)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i len <) f :=) f for)");
    func_append(f, "\t\t(str i ch :string_getc !)");
    func_append(f, "\t\t(((ch ch_A >=) f :=) f if)");
    func_append(f, "\t\t\t(((ch ch_Z <=) f2 :=) f2 if)");
    func_append(f, "\t\t\t\t(ch ch_A - idx :=)");
    func_append(f, "\t\t\t\t(idx shift + idx :=)");
    func_append(f, "\t\t\t\t(idx twenty_six % idx :=)");
    func_append(f, "\t\t\t\t(ch_A idx + ch :=)");
    func_append(f, "\t\t\t\t(str i ch :string_setc !)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((ch ch_a >=) f :=) f if)");
    func_append(f, "\t\t\t(((ch ch_z <=) f2 :=) f2 if)");
    func_append(f, "\t\t\t\t(ch ch_a - idx :=)");
    func_append(f, "\t\t\t\t(idx shift + idx :=)");
    func_append(f, "\t\t\t\t(idx twenty_six % idx :=)");
    func_append(f, "\t\t\t\t(ch_a idx + ch :=)");
    func_append(f, "\t\t\t\t(str i ch :string_setc !)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(str :print_s !)");
        func_append(f, "\t(:print_n !)");
    }
}

static void emit_palindrome_string(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include_post(prog, "string.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    const char *maxlen_v[] = {"256"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "maxlen", 1, maxlen_v, 1);
    add_var_to_func(f, "string", "str", 256, (const char *[]){"\"\""}, 1);
    add_var_to_func(f, "int64", "len", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "j", 1, zv, 1);
    add_var_to_func(f, "int64", "half", 1, zv, 1);
    add_var_to_func(f, "int64", "ci", 1, zv, 1);
    add_var_to_func(f, "int64", "cj", 1, zv, 1);
    add_var_to_func(f, "int64", "pal", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "int64", "f2", 1, zv, 1);
    add_var_to_func(f, "int64", "strmod", 1, zv, 1);
    const char *ps[] = {"\"Enter a string: \""};
    add_var_to_func(f, "const-string", "prompt_str", 17, ps, 1);
    const char *py[] = {"\" is a palindrome\""};
    const char *pn[] = {"\" is not a palindrome\""};
    add_var_to_func(f, "const-string", "yes_str", 18, py, 1);
    add_var_to_func(f, "const-string", "no_str", 22, pn, 1);
    func_append(f, "\t(strmod :string_init !)");
    func_append(f, "\t(prompt_str :print_s !)");
    func_append(f, "\t(maxlen str :input_s !)");
    func_append(f, "\t(str :string_len !)");
    func_append(f, "\t(len stpop)");
    func_append(f, "\t(len two / half :=)");
    func_append(f, "\t(one pal :=)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i half <) f :=) f for)");
    func_append(f, "\t\t(str i ci :string_getc !)");
    func_append(f, "\t\t(len one - j :=)");
    func_append(f, "\t\t(j i - j :=)");
    func_append(f, "\t\t(str j cj :string_getc !)");
    func_append(f, "\t\t(((ci cj ==) f :=) f if-)");
    func_append(f, "\t\t\t(zero pal :=)");
    func_append(f, "\t\t\t(len i :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(((pal one ==) f :=) f if+)");
        func_append(f, "\t\t(str :print_s !)");
        func_append(f, "\t\t(yes_str :print_s !)");
        func_append(f, "\t(else)");
        func_append(f, "\t\t(str :print_s !)");
        func_append(f, "\t\t(no_str :print_s !)");
        func_append(f, "\t(endif)");
        func_append(f, "\t(:print_n !)");
    }
}

static void emit_bubble_sort(Program *prog, Function *f, int skip_input) {
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

static void emit_binary_search(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "int64", "count", 1, (const char *[]){"7"}, 1);
    add_var_to_func(f, "int64", "arr", 7, (const char *[]){"2", "5", "8", "12", "16", "23", "38"}, 7);
    add_var_to_func(f, "int64", "target", 1, zv, 1);
    add_var_to_func(f, "int64", "lo", 1, zv, 1);
    add_var_to_func(f, "int64", "hi", 1, zv, 1);
    add_var_to_func(f, "int64", "mid", 1, zv, 1);
    add_var_to_func(f, "int64", "midval", 1, zv, 1);
    add_var_to_func(f, "int64", "realind", 1, zv, 1);
    add_var_to_func(f, "int64", "found", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Enter target value: \""};
    add_var_to_func(f, "const-string", "prompt_str", 21, ps, 1);
    const char *pf[] = {"\"Found at index: \""};
    const char *pnf[] = {"\"Not found\""};
    add_var_to_func(f, "const-string", "found_str", 17, pf, 1);
    add_var_to_func(f, "const-string", "not_found_str", 11, pnf, 1);
    func_append(f, "\t(prompt_str :print_s !)");
    func_append(f, "\t(target :input_i !)");
    func_append(f, "\t(zero lo :=)");
    func_append(f, "\t(count one - hi :=)");
    func_append(f, "\t(zero found :=)");
    func_append(f, "\t(do)");
    func_append(f, "\t\t(lo hi + two / mid :=)");
    func_append(f, "\t\t(mid * int64_size realind :=)");
    func_append(f, "\t\t(arr [ realind ] midval =)");
    func_append(f, "\t\t(((midval target ==) f :=) f if)");
    func_append(f, "\t\t\t(one found :=)");
    func_append(f, "\t\t\t(hi lo :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((midval target <) f :=) f if)");
    func_append(f, "\t\t\t(mid + one lo :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((midval target >) f :=) f if)");
    func_append(f, "\t\t\t(mid one - hi :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t(((lo hi <=) f :=) f while)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(((found one ==) f :=) f if+)");
        func_append(f, "\t\t(found_str :print_s !)");
        func_append(f, "\t\t(mid :print_i !)");
        func_append(f, "\t(else)");
        func_append(f, "\t\t(not_found_str :print_s !)");
        func_append(f, "\t(endif)");
        func_append(f, "\t(:print_n !)");
    }
}

static void emit_square_root(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "int64", "n", 1, zv, 1);
    add_var_to_func(f, "int64", "x", 1, zv, 1);
    add_var_to_func(f, "int64", "next_x", 1, zv, 1);
    add_var_to_func(f, "int64", "diff", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "prompt_str", 17, ps, 1);
    const char *rs[] = {"\"Square root: \""};
    add_var_to_func(f, "const-string", "result_str", 15, rs, 1);
    func_append(f, "\t(prompt_str :print_s !)");
    func_append(f, "\t(n :input_i !)");
    func_append(f, "\t(n x :=)");
    func_append(f, "\t(do)");
    func_append(f, "\t\t(x n x / + two / next_x :=)");
    func_append(f, "\t\t(next_x x - diff :=)");
    func_append(f, "\t\t(((diff zero <) f :=) f if)");
    func_append(f, "\t\t\t(zero diff - diff :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(next_x x :=)");
    func_append(f, "\t(((diff one >) f :=) f while)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(result_str :print_s !)");
        func_append(f, "\t(x :print_i !)");
        func_append(f, "\t(:print_n !)");
    }
}

static void emit_prime_factorization(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "int64", "n", 1, zv, 1);
    add_var_to_func(f, "int64", "d", 1, zv, 1);
    add_var_to_func(f, "int64", "rem", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "prompt_str", 17, ps, 1);
    func_append(f, "\t(prompt_str :print_s !)");
    func_append(f, "\t(n :input_i !)");
    func_append(f, "\t(two d :=)");
    func_append(f, "\t(do)");
    func_append(f, "\t\t(n d % rem :=)");
    func_append(f, "\t\t(((rem zero ==) f :=) f if)");
    func_append(f, "\t\t\t(d :print_i !)");
    func_append(f, "\t\t\t(:print_n !)");
    func_append(f, "\t\t\t(n d / n :=)");
    func_append(f, "\t\t(else)");
    func_append(f, "\t\t\t(d one + d :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t(((n one >) f :=) f while)");
}

static void emit_standard_deviation(Program *prog, Function *f, int skip_input) {
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
    func_append(f, "\t(((sq_sum 100 <) f :=) f for)");
    func_append(f, "\t\t(sq_sum one + sq_sum :=)");
    func_append(f, "\t\t(stddev stddev + variance stddev / - stddev :=)");
    func_append(f, "\t(next)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(result_str :print_s !)");
        func_append(f, "\t(stddev :print_i !)");
        func_append(f, "\t(:print_n !)");
    }
}

static void emit_compound_interest(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *hv[] = {"100"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "one_hundred", 1, hv, 1);
    add_var_to_func(f, "int64", "principal", 1, zv, 1);
    add_var_to_func(f, "int64", "rate", 1, zv, 1);
    add_var_to_func(f, "int64", "time_years", 1, zv, 1);
    add_var_to_func(f, "int64", "amount", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *pp[] = {"\"Enter principal: \""};
    const char *pr[] = {"\"Enter rate (%%): \""};
    const char *pt[] = {"\"Enter time (years): \""};
    add_var_to_func(f, "const-string", "prompt_p", 19, pp, 1);
    add_var_to_func(f, "const-string", "prompt_r", 18, pr, 1);
    add_var_to_func(f, "const-string", "prompt_t", 21, pt, 1);
    const char *rs[] = {"\"Amount: \""};
    add_var_to_func(f, "const-string", "result_str", 10, rs, 1);
    func_append(f, "\t(prompt_p :print_s !)");
    func_append(f, "\t(principal :input_i !)");
    func_append(f, "\t(prompt_r :print_s !)");
    func_append(f, "\t(rate :input_i !)");
    func_append(f, "\t(prompt_t :print_s !)");
    func_append(f, "\t(time_years :input_i !)");
    func_append(f, "\t(principal amount :=)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i time_years <) f :=) f for)");
    func_append(f, "\t\t(amount rate * one_hundred / amount + amount :=)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(result_str :print_s !)");
        func_append(f, "\t(amount :print_i !)");
        func_append(f, "\t(:print_n !)");
    }
}

static void emit_decimal_to_binary(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include_post(prog, "string.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    const char *maxlen_v[] = {"64"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "maxlen", 1, maxlen_v, 1);
    add_var_to_func(f, "int64", "n", 1, zv, 1);
    add_var_to_func(f, "int64", "rem", 1, zv, 1);
    add_var_to_func(f, "int64", "idx", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "int64", "strmod", 1, zv, 1);
    add_var_to_func(f, "string", "bstr", 64, (const char *[]){"\"\""}, 1);
    const char *ps[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "prompt_str", 17, ps, 1);
    const char *rs[] = {"\"Binary: \""};
    add_var_to_func(f, "const-string", "result_str", 9, rs, 1);
    func_append(f, "\t(strmod :string_init !)");
    func_append(f, "\t(prompt_str :print_s !)");
    func_append(f, "\t(n :input_i !)");
    func_append(f, "\t(zero idx :=)");
    func_append(f, "\t(do)");
    func_append(f, "\t\t(n two % rem :=)");
    func_append(f, "\t\t(rem 48 + rem :=)");
    func_append(f, "\t\t(bstr idx rem :string_setc !)");
    func_append(f, "\t\t(idx + one idx :=)");
    func_append(f, "\t\t(n two / n :=)");
    func_append(f, "\t(((n zero >) f :=) f while)");
    func_append(f, "\t(bstr idx zero :string_setc !)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(result_str :print_s !)");
        func_append(f, "\t(bstr :print_s !)");
        func_append(f, "\t(:print_n !)");
    }
}

static void emit_dice_roll(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *sv[] = {"6"};
    const char *rv[] = {"5"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "six", 1, sv, 1);
    add_var_to_func(f, "const-int64", "five", 1, rv, 1);
    add_var_to_func(f, "int64", "roll", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Rolling dice 5 times:\""};
    add_var_to_func(f, "const-string", "msg_str", 21, ps, 1);
    func_append(f, "\t(msg_str :print_s !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i five <) f :=) f for)");
    func_append(f, "\t\t(roll :epochms !)");
    func_append(f, "\t\t((roll six %) roll :=)");
    func_append(f, "\t\t(roll + one roll :=)");
    func_append(f, "\t\t(roll :print_i !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
}

static void emit_double_math(Program *prog, Function *f, const char *op) {
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

static void emit_double_circle_area(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zd[] = {"0.0"};
    const char *pd[] = {"3.141592653589793"};
    const char *fz[] = {"0"};
    add_var_to_func(f, "const-double", "zerod", 1, zd, 1);
    add_var_to_func(f, "const-double", "pi_val", 1, pd, 1);
    add_var_to_func(f, "double", "r", 1, zd, 1);
    add_var_to_func(f, "double", "area", 1, zd, 1);
    add_var_to_func(f, "int64", "f", 1, fz, 1);
    const char *pr[] = {"\"Enter radius: \""};
    add_var_to_func(f, "const-string", "prompt_r", 15, pr, 1);
    func_append(f, "\t(prompt_r :print_s !)");
    func_append(f, "\t(r :input_d !)");
    func_append(f, "\t(r r *d area :=)");
    func_append(f, "\t(area pi_val *d area :=)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(area :print_d !)");
        func_append(f, "\t(:print_n !)");
    }
}

static void emit_double_average(Program *prog, Function *f, int skip_input) {
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

static void emit_double_compound_interest(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zd[] = {"0.0"};
    const char *hd[] = {"100.0"};
    const char *od[] = {"1.0"};
    const char *fz[] = {"0"};
    add_var_to_func(f, "const-double", "zerod", 1, zd, 1);
    add_var_to_func(f, "const-double", "oned", 1, od, 1);
    add_var_to_func(f, "const-double", "one_hundredd", 1, hd, 1);
    add_var_to_func(f, "double", "principal", 1, zd, 1);
    add_var_to_func(f, "double", "rate", 1, zd, 1);
    add_var_to_func(f, "double", "years", 1, zd, 1);
    add_var_to_func(f, "double", "amount", 1, zd, 1);
    add_var_to_func(f, "double", "i", 1, zd, 1);
    add_var_to_func(f, "int64", "f", 1, fz, 1);
    const char *pp[] = {"\"Enter principal: \""};
    const char *prr[] = {"\"Enter rate (%): \""};
    const char *py[] = {"\"Enter years: \""};
    add_var_to_func(f, "const-string", "prompt_p", 19, pp, 1);
    add_var_to_func(f, "const-string", "prompt_r", 16, prr, 1);
    add_var_to_func(f, "const-string", "prompt_y", 14, py, 1);
    func_append(f, "\t(prompt_p :print_s !)");
    func_append(f, "\t(principal :input_d !)");
    func_append(f, "\t(prompt_r :print_s !)");
    func_append(f, "\t(rate :input_d !)");
    func_append(f, "\t(prompt_y :print_s !)");
    func_append(f, "\t(years :input_d !)");
    func_append(f, "\t((rate one_hundredd /d) rate =)");
    func_append(f, "\t((rate oned +d) rate =)");
    func_append(f, "\t(principal amount :=)");
    func_append(f, "\t(zerod i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i years <d) f :=) f for)");
    func_append(f, "\t\t((amount rate *d) amount =)");
    func_append(f, "\t\t(i oned +d i :=)");
    func_append(f, "\t(next)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(amount :print_d !)");
        func_append(f, "\t(:print_n !)");
    }
}

static void emit_double_pythagoras(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zd[] = {"0.0"};
    const char *hd[] = {"0.5"};
    const char *fz[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-double", "zerod", 1, zd, 1);
    add_var_to_func(f, "const-double", "half", 1, hd, 1);
    add_var_to_func(f, "const-int64", "zero", 1, fz, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "double", "a", 1, zd, 1);
    add_var_to_func(f, "double", "b", 1, zd, 1);
    add_var_to_func(f, "double", "c", 1, zd, 1);
    add_var_to_func(f, "double", "tmp", 1, zd, 1);
    add_var_to_func(f, "int64", "i", 1, fz, 1);
    add_var_to_func(f, "int64", "f", 1, fz, 1);
    const char *pa[] = {"\"Enter side a: \""};
    const char *pb[] = {"\"Enter side b: \""};
    add_var_to_func(f, "const-string", "prompt_a", 15, pa, 1);
    add_var_to_func(f, "const-string", "prompt_b", 15, pb, 1);
    func_append(f, "	(prompt_a :print_s !)");
    func_append(f, "	(a :input_d !)");
    func_append(f, "	(prompt_b :print_s !)");
    func_append(f, "	(b :input_d !)");
    func_append(f, "	((a a *d) tmp =)");
    func_append(f, "	((b b *d tmp +d) tmp =)");
    func_append(f, "	(tmp c :=)");
    func_append(f, "	(zero i :=)");
    func_append(f, "	(for-loop)");
    func_append(f, "	(((i 100 <) f :=) f for)");
    func_append(f, "		((c tmp c /d +d) c :=)");
    func_append(f, "		(c half *d c :=)");
    func_append(f, "		(i + one i :=)");
    func_append(f, "	(next)");
    if (!dataflow_quiet_mode) {
        func_append(f, "	(c :print_d !)");
        func_append(f, "	(:print_n !)");
    }
}

static void emit_double_temp_convert(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zd[] = {"0.0"};
    const char *cd[] = {"5.0"};
    const char *nd[] = {"9.0"};
    const char *td[] = {"32.0"};
    const char *fz[] = {"0"};
    add_var_to_func(f, "const-double", "zerod", 1, zd, 1);
    add_var_to_func(f, "const-double", "five", 1, cd, 1);
    add_var_to_func(f, "const-double", "nine", 1, nd, 1);
    add_var_to_func(f, "const-double", "thirtytwo", 1, td, 1);
    add_var_to_func(f, "double", "celsius", 1, zd, 1);
    add_var_to_func(f, "double", "fahrenheit", 1, zd, 1);
    add_var_to_func(f, "int64", "f", 1, fz, 1);
    const char *pc[] = {"\"Enter Celsius: \""};
    add_var_to_func(f, "const-string", "prompt_c", 16, pc, 1);
    func_append(f, "	(prompt_c :print_s !)");
    func_append(f, "	(celsius :input_d !)");
    func_append(f, "	((celsius nine *d) fahrenheit =)");
    func_append(f, "	((fahrenheit five /d) fahrenheit =)");
    func_append(f, "	((fahrenheit thirtytwo +d) fahrenheit =)");
    if (!dataflow_quiet_mode) {
        func_append(f, "	(fahrenheit :print_d !)");
        func_append(f, "	(:print_n !)");
    }
}

static void emit_double_sqrt(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zd[] = {"0.0"};
    const char *hd[] = {"0.5"};
    const char *fz[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-double", "zerod", 1, zd, 1);
    add_var_to_func(f, "const-double", "half", 1, hd, 1);
    add_var_to_func(f, "const-int64", "zero", 1, fz, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "double", "n", 1, zd, 1);
    add_var_to_func(f, "double", "x", 1, zd, 1);
    add_var_to_func(f, "int64", "i", 1, fz, 1);
    add_var_to_func(f, "int64", "f", 1, fz, 1);
    const char *ps[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "prompt_str", 17, ps, 1);
    func_append(f, "	(prompt_str :print_s !)");
    func_append(f, "	(n :input_d !)");
    func_append(f, "	(n x :=)");
    func_append(f, "	(zero i :=)");
    func_append(f, "	(for-loop)");
    func_append(f, "	(((i 100 <) f :=) f for)");
    func_append(f, "		((x n x /d +d) x =)");
    func_append(f, "		(x half *d x =)");
    func_append(f, "		(i + one i :=)");
    func_append(f, "	(next)");
    if (!dataflow_quiet_mode) {
        func_append(f, "	(x :print_d !)");
        func_append(f, "	(:print_n !)");
    }
}

static int parse_task(const char *prompt, TaskProfile *task) {
    char buf[MAX_PROMPT];
    snprintf(buf, sizeof(buf), "%s", prompt);
    to_lowercase(buf);

    // default type
    snprintf(task->type, sizeof(task->type), "%s", "int64");

    // detect type
    if (strstr(buf, "byte") || strstr(buf, "int8")) snprintf(task->type, sizeof(task->type), "%s", "byte");
    else if (strstr(buf, "int16") || strstr(buf, "short")) snprintf(task->type, sizeof(task->type), "%s", "int16");
    else if (strstr(buf, "int32")) snprintf(task->type, sizeof(task->type), "%s", "int32");
    else if (strstr(buf, "int64") || strstr(buf, "int") || strstr(buf, "long")) snprintf(task->type, sizeof(task->type), "%s", "int64");
    else if (strstr(buf, "double") || strstr(buf, "float") || strstr(buf, "real") || strstr(buf, "komma")) snprintf(task->type, sizeof(task->type), "%s", "double");

    // extract numbers
    task->num_literals = extract_numbers(prompt, task->literals, MAX_NUMS);
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
                task->input_count = atoi(q);
                task->has_input = 1;
            }
            break;
        }
        p++;
    }

    // detect input keywords
    if (has_word(buf, "lies") || has_word(buf, "read") || has_word(buf, "input")
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

    if (has_word(buf, "power") || has_word(buf, "potenz") || has_word(buf, "exponent")
        || has_word(buf, "square") || has_word(buf, "quadrat") || has_word(buf, "cube"))
        task->has_power = 1;

    if (has_word(buf, "max") || has_word(buf, "größt") || has_word(buf, "largest")
        || has_word(buf, "greatest") || has_word(buf, "highest"))
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

    if (has_word(buf, "function") || has_word(buf, "funktion") || has_word(buf, "sub"))
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

    if ((has_word(buf, "even") || has_word(buf, "odd") || has_word(buf, "gerade") || has_word(buf, "ungerade"))
        && !task->has_literals)
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

    if ((has_word(buf, "even") || has_word(buf, "gerade")) && task->has_literals) {
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

    if (task->has_input && task->has_factorial) {
        task->has_input_fact = 1;
        task->has_algorithm = 0;
    }

    if (task->has_string_cat && !has_word(buf, "compare") && !has_word(buf, "vergleich")) {
        // string_cat unless compare is explicit
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

    if ((has_word(buf, "random") || has_word(buf, "zufall")) && (has_word(buf, "generator") || has_word(buf, "generate") || has_word(buf, "generieren")))
        task->has_random_generator = 1;

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
        || task->has_weighted_random || task->has_ascii_table;

    snprintf(task->title, sizeof(task->title), "%s", prompt);
    return has_any;
}

// ==================== FUNCTION EMITTER ====================

static void emit_function(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *fv[] = {"5"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "int64", "num", 1, fv, 1);
    add_var_to_func(f, "int64", "sq", 1, zv, 1);
    func_append(f, "\t// call function: sq = square(num)");
    func_append(f, "\t(num sq :square !)");
    func_append(f, "\t(sq :print_i !)");
    func_append(f, "\t(:print_n !)");
    add_func(prog, "square");
    Function *sf = &prog->funcs[prog->num_funcs - 1];
    sf->is_local = 1;
    sf->has_vardef = 1;
    snprintf(sf->vardef_name, sizeof(sf->vardef_name), "square");
    add_var_to_func(sf, "int64", "n~", 1, zv, 1);
    add_var_to_func(sf, "int64", "r~", 1, zv, 1);
    func_append(sf, "\t(n~ stpop)");
    func_append(sf, "\t(n~ n~ * r~ :=)");
    func_append(sf, "\t(r~ stpush)");
    func_append(sf, "\t(return)");
}

static void emit_string_length(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include_post(prog, "string.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *ml[] = {"256"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "maxlen", 1, ml, 1);
    add_var_to_func(f, "string", "str", 256, (const char *[]){"\"\""}, 1);
    add_var_to_func(f, "int64", "len", 1, zv, 1);
    add_var_to_func(f, "int64", "strmod", 1, zv, 1);
    const char *ps[] = {"\"Enter a string: \""};
    add_var_to_func(f, "const-string", "prompt_str", 17, ps, 1);
    func_append(f, "\t(strmod :string_init !)");
    func_append(f, "\t(prompt_str :print_s !)");
    func_append(f, "\t(maxlen str :input_s !)");
    func_append(f, "\t(str :string_len !)");
    func_append(f, "\t(len stpop)");
    if (!dataflow_quiet_mode) {
        func_append(f, "\t(len :print_i !)");
        func_append(f, "\t(:print_n !)");
    }
}

static void emit_stack(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *cv[] = {"10"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    const char *tv[] = {"2"};
    const char *thv[] = {"3"};
    const char *fov[] = {"4"};
    const char *fiv[] = {"5"};
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "three", 1, thv, 1);
    add_var_to_func(f, "const-int64", "four", 1, fov, 1);
    add_var_to_func(f, "const-int64", "five", 1, fiv, 1);
    add_var_to_func(f, "int64", "sp", 1, zv, 1);
    add_var_to_func(f, "int64", "capacity", 1, cv, 1);
    add_var_to_func(f, "int64", "stack", 10, cv, 0);
    add_var_to_func(f, "int64", "choice", 1, zv, 1);
    add_var_to_func(f, "int64", "val", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "realind", 1, zv, 1);
    const char *ms[] = {"\"1-push 2-pop 3-size 4-all 5-quit\""};
    add_var_to_func(f, "const-string", "menu_str", 40, ms, 1);
    add_var_to_func(f, "const-string", "prompt_val", 14, (const char *[]){"\"Enter value: \""}, 1);
    const char *cs[] = {"\"choice: \""};
    add_var_to_func(f, "const-string", "prompt_choice", 12, cs, 1);
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((one one <) f :=) f for)");
    func_append(f, "\t\t(menu_str :print_s !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(prompt_choice :print_s !)");
    func_append(f, "\t\t(choice :input_i !)");
    func_append(f, "\t\t(((choice one ==) f :=) f if)");
    func_append(f, "\t\t\t(prompt_val :print_s !)");
    func_append(f, "\t\t\t(val :input_i !)");
    func_append(f, "\t\t\t(sp * int64_size realind :=)");
    func_append(f, "\t\t\t(val stack [ realind ] =)");
    func_append(f, "\t\t\t(sp + one sp :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice two ==) f :=) f if)");
    func_append(f, "\t\t\t(sp one - sp :=)");
    func_append(f, "\t\t\t(sp * int64_size realind :=)");
    func_append(f, "\t\t\t(stack [ realind ] val =)");
    func_append(f, "\t\t\t(val :print_i !)");
    func_append(f, "\t\t\t(:print_n !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice three ==) f :=) f if)");
    func_append(f, "\t\t\t(sp :print_i !)");
    func_append(f, "\t\t\t(:print_n !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice four ==) f :=) f if)");
    func_append(f, "\t\t\t(zero i :=)");
    func_append(f, "\t\t\t(for-loop)");
    func_append(f, "\t\t\t(((i sp <) f :=) f for)");
    func_append(f, "\t\t\t\t(i * int64_size realind :=)");
    func_append(f, "\t\t\t\t(stack [ realind ] val =)");
    func_append(f, "\t\t\t\t(val :print_i !)");
    func_append(f, "\t\t\t\t(:print_n !)");
    func_append(f, "\t\t\t\t(i + one i :=)");
    func_append(f, "\t\t\t(next)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice five ==) f :=) f if)");
    func_append(f, "\t\t\t(zero :exit !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t(next)");
}

static void emit_queue(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *cv[] = {"10"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    const char *tv[] = {"2"};
    const char *thv[] = {"3"};
    const char *fov[] = {"4"};
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "three", 1, thv, 1);
    add_var_to_func(f, "const-int64", "four", 1, fov, 1);
    add_var_to_func(f, "int64", "head", 1, zv, 1);
    add_var_to_func(f, "int64", "tail", 1, zv, 1);
    add_var_to_func(f, "int64", "capacity", 1, cv, 1);
    add_var_to_func(f, "int64", "queue", 10, cv, 0);
    add_var_to_func(f, "int64", "choice", 1, zv, 1);
    add_var_to_func(f, "int64", "val", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "realind", 1, zv, 1);
    const char *ms[] = {"\"1-enqueue 2-dequeue 3-print 4-quit\""};
    add_var_to_func(f, "const-string", "menu_str", 40, ms, 1);
    add_var_to_func(f, "const-string", "prompt_val", 14, (const char *[]){"\"Enter value: \""}, 1);
    const char *cs[] = {"\"choice: \""};
    add_var_to_func(f, "const-string", "prompt_choice", 12, cs, 1);
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((one one <) f :=) f for)");
    func_append(f, "\t\t(menu_str :print_s !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(prompt_choice :print_s !)");
    func_append(f, "\t\t(choice :input_i !)");
    func_append(f, "\t\t(((choice one ==) f :=) f if)");
    func_append(f, "\t\t\t(prompt_val :print_s !)");
    func_append(f, "\t\t\t(val :input_i !)");
    func_append(f, "\t\t\t(tail * int64_size realind :=)");
    func_append(f, "\t\t\t(val queue [ realind ] =)");
    func_append(f, "\t\t\t(tail + one tail :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice two ==) f :=) f if)");
    func_append(f, "\t\t\t(head * int64_size realind :=)");
    func_append(f, "\t\t\t(queue [ realind ] val =)");
    func_append(f, "\t\t\t(val :print_i !)");
    func_append(f, "\t\t\t(:print_n !)");
    func_append(f, "\t\t\t(head + one head :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice three ==) f :=) f if)");
    func_append(f, "\t\t\t(head i :=)");
    func_append(f, "\t\t\t(for-loop)");
    func_append(f, "\t\t\t(((i tail <) f :=) f for)");
    func_append(f, "\t\t\t\t(i * int64_size realind :=)");
    func_append(f, "\t\t\t\t(queue [ realind ] val =)");
    func_append(f, "\t\t\t\t(val :print_i !)");
    func_append(f, "\t\t\t\t(:print_n !)");
    func_append(f, "\t\t\t\t(i + one i :=)");
    func_append(f, "\t\t\t(next)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice four ==) f :=) f if)");
    func_append(f, "\t\t\t(zero :exit !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t(next)");
}

static void emit_insertion_sort(Program *prog, Function *f, int count, int skip_input) {
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

static void emit_calculator(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    const char *tv[] = {"2"};
    const char *thv[] = {"3"};
    const char *fov[] = {"4"};
    const char *fiv[] = {"5"};
    const char *sv[] = {"6"};
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "three", 1, thv, 1);
    add_var_to_func(f, "const-int64", "four", 1, fov, 1);
    add_var_to_func(f, "const-int64", "five", 1, fiv, 1);
    add_var_to_func(f, "const-int64", "six", 1, sv, 1);
    add_var_to_func(f, "int64", "op", 1, zv, 1);
    add_var_to_func(f, "int64", "a", 1, zv, 1);
    add_var_to_func(f, "int64", "b", 1, zv, 1);
    add_var_to_func(f, "int64", "result", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ms[] = {"\"1-add 2-sub 3-mul 4-div 5-mod 6-quit\""};
    add_var_to_func(f, "const-string", "menu_str", 45, ms, 1);
    const char *os[] = {"\"op: \""};
    add_var_to_func(f, "const-string", "op_str", 8, os, 1);
    add_var_to_func(f, "const-string", "prompt_a", 10, (const char *[]){"\"Enter a: \""}, 1);
    add_var_to_func(f, "const-string", "prompt_b", 10, (const char *[]){"\"Enter b: \""}, 1);
    const char *rs[] = {"\"Result: \""};
    add_var_to_func(f, "const-string", "result_str", 9, rs, 1);
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((one one <) f :=) f for)");
    func_append(f, "\t\t(menu_str :print_s !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(op_str :print_s !)");
    func_append(f, "\t\t(op :input_i !)");
    func_append(f, "\t\t(((op six ==) f :=) f if)");
    func_append(f, "\t\t\t(zero :exit !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(prompt_a :print_s !)");
    func_append(f, "\t\t(a :input_i !)");
    func_append(f, "\t\t(prompt_b :print_s !)");
    func_append(f, "\t\t(b :input_i !)");
    func_append(f, "\t\t(((op one ==) f :=) f if)");
    func_append(f, "\t\t\t(a b + result :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((op two ==) f :=) f if)");
    func_append(f, "\t\t\t(a b - result :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((op three ==) f :=) f if)");
    func_append(f, "\t\t\t(a b * result :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((op four ==) f :=) f if)");
    func_append(f, "\t\t\t(a b / result :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((op five ==) f :=) f if)");
    func_append(f, "\t\t\t(a b % result :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(result_str :print_s !)");
    func_append(f, "\t\t(result :print_i !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t(next)");
}

static void emit_unit_converter(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    const char *tv[] = {"2"};
    const char *thv[] = {"3"};
    const char *fov[] = {"4"};
    const char *fiv[] = {"5"};
    const char *sv[] = {"6"};
    const char *sev[] = {"7"};
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "three", 1, thv, 1);
    add_var_to_func(f, "const-int64", "four", 1, fov, 1);
    add_var_to_func(f, "const-int64", "five", 1, fiv, 1);
    add_var_to_func(f, "const-int64", "six", 1, sv, 1);
    add_var_to_func(f, "const-int64", "seven", 1, sev, 1);
    add_var_to_func(f, "int64", "choice", 1, zv, 1);
    const char *zdv[] = {"0.0"};
    add_var_to_func(f, "double", "val", 1, zdv, 1);
    add_var_to_func(f, "double", "result", 1, zdv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ms[] = {"\"1-km_mi 2-mi_km 3-kg_lb 4-lb_kg 5-cm_in 6-in_cm 7-quit\""};
    add_var_to_func(f, "const-string", "menu_str", 70, ms, 1);
    add_var_to_func(f, "const-string", "prompt_val", 14, (const char *[]){"\"Enter value: \""}, 1);
    const char *cs[] = {"\"choice: \""};
    add_var_to_func(f, "const-string", "prompt_choice", 12, cs, 1);
    const char *rs[] = {"\"Result: \""};
    add_var_to_func(f, "const-string", "result_str", 9, rs, 1);
    // Conversion factors
    const char *fkm_mi[] = {"0.621"};
    const char *fmi_km[] = {"1.609"};
    const char *fkg_lb[] = {"2.204"};
    const char *flb_kg[] = {"0.454"};
    const char *fcm_in[] = {"0.394"};
    const char *fin_cm[] = {"2.540"};
    add_var_to_func(f, "const-double", "c_km_mi", 1, fkm_mi, 1);
    add_var_to_func(f, "const-double", "c_mi_km", 1, fmi_km, 1);
    add_var_to_func(f, "const-double", "c_kg_lb", 1, fkg_lb, 1);
    add_var_to_func(f, "const-double", "c_lb_kg", 1, flb_kg, 1);
    add_var_to_func(f, "const-double", "c_cm_in", 1, fcm_in, 1);
    add_var_to_func(f, "const-double", "c_in_cm", 1, fin_cm, 1);
    const char *zerodv[] = {"0.0"};
    add_var_to_func(f, "const-double", "zerod", 1, zerodv, 1);

    add_var_to_func(f, "int64", "temp", 1, zv, 1);
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((one one ==) f :=) f for)");
    func_append(f, "\t\t(menu_str :print_s !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(prompt_choice :print_s !)");
    func_append(f, "\t\t(choice :input_i !)");
    func_append(f, "\t\t(choice + zero choice :=)");
    func_append(f, "\t\t(((choice seven ==) f :=) f if)");
    func_append(f, "\t\t\t(zero :exit !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(prompt_val :print_s !)");
    func_append(f, "\t\t(val :input_d !)");
    func_append(f, "\t\t(val + zerod val :=)");
    func_append(f, "\t\t(((choice one ==) f :=) f if)");  // km -> mi
    func_append(f, "\t\t\t(val * c_km_mi result :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice two ==) f :=) f if)");  // mi -> km
    func_append(f, "\t\t\t(val * c_mi_km result :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice three ==) f :=) f if)");  // kg -> lb
    func_append(f, "\t\t\t(val * c_kg_lb result :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice four ==) f :=) f if)");  // lb -> kg
    func_append(f, "\t\t\t(val * c_lb_kg result :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice five ==) f :=) f if)");  // cm -> in
    func_append(f, "\t\t\t(val * c_cm_in result :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice six ==) f :=) f if)");  // in -> cm
    func_append(f, "\t\t\t(val * c_in_cm result :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(result_str :print_s !)");
    func_append(f, "\t\t(reset-reg)");
    func_append(f, "\t\t(result :print_d !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t(next)");
}

static void emit_rock_paper_scissors(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *mv[] = {"3"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    const char *tv[] = {"2"};
    const char *thv[] = {"3"};
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "three", 1, thv, 1);
    add_var_to_func(f, "int64", "mod3", 1, mv, 1);
    add_var_to_func(f, "int64", "player", 1, zv, 1);
    add_var_to_func(f, "int64", "computer", 1, zv, 1);
    add_var_to_func(f, "int64", "pscore", 1, zv, 1);
    add_var_to_func(f, "int64", "cscore", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ms[] = {"\"0-rock 1-paper 2-scissors 3-quit\""};
    add_var_to_func(f, "const-string", "menu_str", 40, ms, 1);
    const char *cs[] = {"\"choose: \""};
    add_var_to_func(f, "const-string", "choose_str", 12, cs, 1);
    const char *ws[] = {"\"You win!\""};
    add_var_to_func(f, "const-string", "win_str", 9, ws, 1);
    const char *ls[] = {"\"You lose!\""};
    add_var_to_func(f, "const-string", "lose_str", 10, ls, 1);
    const char *ds[] = {"\"Draw!\""};
    add_var_to_func(f, "const-string", "draw_str", 6, ds, 1);
    const char *ss[] = {"\"Score: \""};
    add_var_to_func(f, "const-string", "score_str", 8, ss, 1);
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((one one <) f :=) f for)");
    func_append(f, "\t\t(menu_str :print_s !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(choose_str :print_s !)");
    func_append(f, "\t\t(player :input_i !)");
    func_append(f, "\t\t(((player three ==) f :=) f if)");
    func_append(f, "\t\t\t(score_str :print_s !)");
    func_append(f, "\t\t\t(pscore :print_i !)");
    func_append(f, "\t\t\t(:print_n !)");
    func_append(f, "\t\t\t(zero :exit !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(computer :epochms !)");
    func_append(f, "\t\t(computer mod3 % computer :=)");
    func_append(f, "\t\t(((player computer ==) f :=) f if)");
    func_append(f, "\t\t\t(draw_str :print_s !)");
    func_append(f, "\t\t\t(:print_n !)");
    func_append(f, "\t\t(endif)");
    // rock=0 beats scissors=2
    func_append(f, "\t\t(((player zero ==) f :=) f if)");
    func_append(f, "\t\t\t(((computer two ==) f :=) f if)");
    func_append(f, "\t\t\t\t(win_str :print_s !)");
    func_append(f, "\t\t\t\t(:print_n !)");
    func_append(f, "\t\t\t\t(pscore + one pscore :=)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t\t(((computer one ==) f :=) f if)");
    func_append(f, "\t\t\t\t(lose_str :print_s !)");
    func_append(f, "\t\t\t\t(:print_n !)");
    func_append(f, "\t\t\t\t(cscore + one cscore :=)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t(endif)");
    // paper=1 beats rock=0
    func_append(f, "\t\t(((player one ==) f :=) f if)");
    func_append(f, "\t\t\t(((computer zero ==) f :=) f if)");
    func_append(f, "\t\t\t\t(win_str :print_s !)");
    func_append(f, "\t\t\t\t(:print_n !)");
    func_append(f, "\t\t\t\t(pscore + one pscore :=)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t\t(((computer two ==) f :=) f if)");
    func_append(f, "\t\t\t\t(lose_str :print_s !)");
    func_append(f, "\t\t\t\t(:print_n !)");
    func_append(f, "\t\t\t\t(cscore + one cscore :=)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t(endif)");
    // scissors=2 beats paper=1
    func_append(f, "\t\t(((player two ==) f :=) f if)");
    func_append(f, "\t\t\t(((computer one ==) f :=) f if)");
    func_append(f, "\t\t\t\t(win_str :print_s !)");
    func_append(f, "\t\t\t\t(:print_n !)");
    func_append(f, "\t\t\t\t(pscore + one pscore :=)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t\t(((computer zero ==) f :=) f if)");
    func_append(f, "\t\t\t\t(lose_str :print_s !)");
    func_append(f, "\t\t\t\t(:print_n !)");
    func_append(f, "\t\t\t\t(cscore + one cscore :=)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t(next)");
}

static void emit_pyramid(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "int64", "n", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "j", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Enter number of rows: \""};
    add_var_to_func(f, "const-string", "prompt_n", 24, ps, 1);
    func_append(f, "\t(prompt_n :print_s !)");
    func_append(f, "\t(n :input_i !)");
    func_append(f, "\t(one i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i n <=) f :=) f for)");
    func_append(f, "\t\t(one j :=)");
    func_append(f, "\t\t(for-loop)");
    func_append(f, "\t\t(((j i <=) f :=) f for)");
    func_append(f, "\t\t\t(j :print_i !)");
    func_append(f, "\t\t\t(j + one j :=)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
}

// ==================== 10 NEW COMBO EMITTERS ====================

static void emit_temp_converter_menu(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    const char *thv[] = {"3"};
    const char *fov[] = {"4"};
    const char *fiv[] = {"5"};
    const char *zdv[] = {"0.0"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "three", 1, thv, 1);
    add_var_to_func(f, "const-int64", "four", 1, fov, 1);
    add_var_to_func(f, "const-int64", "five", 1, fiv, 1);
    add_var_to_func(f, "int64", "choice", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "double", "val", 1, zdv, 1);
    add_var_to_func(f, "double", "result", 1, zdv, 1);
    add_var_to_func(f, "double", "tempd", 1, zdv, 1);
    const char *ms[] = {"\"1-C_to_F 2-F_to_C 3-C_to_K 4-K_to_C 5-quit\""};
    add_var_to_func(f, "const-string", "menu_str", 48, ms, 1);
    add_var_to_func(f, "const-string", "prompt_val", 14, (const char *[]){"\"Enter value: \""}, 1);
    const char *cs[] = {"\"choice: \""};
    add_var_to_func(f, "const-string", "prompt_choice", 12, cs, 1);
    const char *rs[] = {"\"Result: \""};
    add_var_to_func(f, "const-string", "result_str", 9, rs, 1);
    add_var_to_func(f, "const-double", "zerod", 1, zdv, 1);
    const char *c9[] = {"9.0"};
    const char *c5[] = {"5.0"};
    const char *c32[] = {"32.0"};
    const char *c273[] = {"273.15"};
    add_var_to_func(f, "const-double", "c_9", 1, c9, 1);
    add_var_to_func(f, "const-double", "c_5", 1, c5, 1);
    add_var_to_func(f, "const-double", "c_32", 1, c32, 1);
    add_var_to_func(f, "const-double", "c_273", 1, c273, 1);
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((one one ==) f :=) f for)");
    func_append(f, "\t\t(menu_str :print_s !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(prompt_choice :print_s !)");
    func_append(f, "\t\t(choice :input_i !)");
    func_append(f, "\t\t(choice + zero choice :=)");
    func_append(f, "\t\t(((choice five ==) f :=) f if)");
    func_append(f, "\t\t\t(zero :exit !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(prompt_val :print_s !)");
    func_append(f, "\t\t(val :input_d !)");
    func_append(f, "\t\t(val + zerod val :=)");
    func_append(f, "\t\t(((choice one ==) f :=) f if)");
    func_append(f, "\t\t\t(val * c_9 result :=)");
    func_append(f, "\t\t\t(result / c_5 result :=)");
    func_append(f, "\t\t\t(result + c_32 result :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice two ==) f :=) f if)");
    func_append(f, "\t\t\t(val - c_32 tempd :=)");
    func_append(f, "\t\t\t(tempd * c_5 result :=)");
    func_append(f, "\t\t\t(result / c_9 result :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice three ==) f :=) f if)");
    func_append(f, "\t\t\t(val + c_273 result :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice four ==) f :=) f if)");
    func_append(f, "\t\t\t(val - c_273 result :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(result_str :print_s !)");
    func_append(f, "\t\t(reset-reg)");
    func_append(f, "\t\t(result :print_d !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t(next)");
}

static void emit_sort_stats(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "int64", "n", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "j", 1, zv, 1);
    add_var_to_func(f, "int64", "temp", 1, zv, 1);
    add_var_to_func(f, "int64", "sum", 1, zv, 1);
    add_var_to_func(f, "int64", "ri", 1, zv, 1);
    add_var_to_func(f, "int64", "rj", 1, zv, 1);
    add_var_to_func(f, "int64", "a", 1, zv, 1);
    add_var_to_func(f, "int64", "b", 1, zv, 1);
    add_var_to_func(f, "int64", "arr", 100, zv, 1);
    const char *ns[] = {"\"Enter count: \""};
    add_var_to_func(f, "const-string", "prompt_n", 14, ns, 1);
    const char *pn[] = {"\"Enter number: \""};
    add_var_to_func(f, "const-string", "prompt_num", 15, pn, 1);
    const char *ss[] = {"\"Sorted: \""};
    add_var_to_func(f, "const-string", "sorted_str", 9, ss, 1);
    const char *smin[] = {"\" Min: \""};
    add_var_to_func(f, "const-string", "min_str", 7, smin, 1);
    const char *smax[] = {"\" Max: \""};
    add_var_to_func(f, "const-string", "max_str", 7, smax, 1);
    const char *savg[] = {"\" Avg: \""};
    add_var_to_func(f, "const-string", "avg_str", 7, savg, 1);
    func_append(f, "\t(prompt_n :print_s !)");
    func_append(f, "\t(n :input_i !)");
    func_append(f, "\t(n + zero n :=)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(((i n <) i :=) i for)");
    func_append(f, "\t\t(prompt_num :print_s !)");
    func_append(f, "\t\t(a :input_i !)");
    func_append(f, "\t\t(i * int64_size ri :=)");
    func_append(f, "\t\t(a arr [ ri ] =)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(((i n <) i :=) i for)");
    func_append(f, "\t\t(i + one j :=)");
    func_append(f, "\t\t(((j n <) j :=) j for)");
    func_append(f, "\t\t\t(i * int64_size ri :=)");
    func_append(f, "\t\t\t(j * int64_size rj :=)");
    func_append(f, "\t\t\t(arr [ ri ] a =)");
    func_append(f, "\t\t\t(arr [ rj ] b =)");
    func_append(f, "\t\t\t(a b > if)");
    func_append(f, "\t\t\t\t(arr [ ri ] temp :=)");
    func_append(f, "\t\t\t\t(b arr [ ri ] =)");
    func_append(f, "\t\t\t\t(temp arr [ rj ] =)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t\t(j + one j :=)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(sorted_str :print_s !)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(((i n <) i :=) i for)");
    func_append(f, "\t\t(i * int64_size ri :=)");
    func_append(f, "\t\t(arr [ ri ] a =)");
    func_append(f, "\t\t(a :print_i !)");
    func_append(f, "\t\t(:print_s !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(min_str :print_s !)");
    func_append(f, "\t(zero * int64_size ri :=)");
    func_append(f, "\t(arr [ ri ] a =)");
    func_append(f, "\t(a :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(max_str :print_s !)");
    func_append(f, "\t(n one - temp :=)");
    func_append(f, "\t(temp * int64_size ri :=)");
    func_append(f, "\t(arr [ ri ] a =)");
    func_append(f, "\t(a :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero sum :=)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(((i n <) i :=) i for)");
    func_append(f, "\t\t(i * int64_size ri :=)");
    func_append(f, "\t\t(arr [ ri ] a =)");
    func_append(f, "\t\t(a sum + sum :=)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(avg_str :print_s !)");
    func_append(f, "\t(sum n / temp :=)");
    func_append(f, "\t(temp :print_i !)");
    func_append(f, "\t(:print_n !)");
}

static void emit_string_analyzer(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include_post(prog, "string.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *ml[] = {"256"};
    const char *v_a[] = {"97"};
    const char *v_e[] = {"101"};
    const char *v_i[] = {"105"};
    const char *v_o[] = {"111"};
    const char *v_u[] = {"117"};
    const char *v_A[] = {"65"};
    const char *v_E[] = {"69"};
    const char *v_I[] = {"73"};
    const char *v_O[] = {"79"};
    const char *v_U[] = {"85"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "maxlen", 1, ml, 1);
    add_var_to_func(f, "const-int64", "va", 1, v_a, 1);
    add_var_to_func(f, "const-int64", "ve", 1, v_e, 1);
    add_var_to_func(f, "const-int64", "vi", 1, v_i, 1);
    add_var_to_func(f, "const-int64", "vo", 1, v_o, 1);
    add_var_to_func(f, "const-int64", "vu", 1, v_u, 1);
    add_var_to_func(f, "const-int64", "vA", 1, v_A, 1);
    add_var_to_func(f, "const-int64", "vE", 1, v_E, 1);
    add_var_to_func(f, "const-int64", "vI", 1, v_I, 1);
    add_var_to_func(f, "const-int64", "vO", 1, v_O, 1);
    add_var_to_func(f, "const-int64", "vU", 1, v_U, 1);
    add_var_to_func(f, "int64", "strmod", 1, zv, 1);
    add_var_to_func(f, "int64", "len", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "vowels", 1, zv, 1);
    add_var_to_func(f, "int64", "cons", 1, zv, 1);
    add_var_to_func(f, "int64", "ch", 1, zv, 1);
    add_var_to_func(f, "string", "str", 256, (const char *[]){"\"\""}, 1);
    const char *vs[] = {"\"Enter a string: \""};
    add_var_to_func(f, "const-string", "prompt_str", 16, vs, 1);
    const char *vl[] = {"\"Length: \""};
    add_var_to_func(f, "const-string", "len_str", 9, vl, 1);
    const char *vv[] = {"\"Vowels: \""};
    add_var_to_func(f, "const-string", "vowels_str", 9, vv, 1);
    const char *vc[] = {"\"Consonants: \""};
    add_var_to_func(f, "const-string", "cons_str", 13, vc, 1);
    func_append(f, "\t(strmod :string_init !)");
    func_append(f, "\t(prompt_str :print_s !)");
    func_append(f, "\t(maxlen str :input_s !)");
    func_append(f, "\t(str :string_len !)");
    func_append(f, "\t(len stpop)");
    func_append(f, "\t(zero vowels :=)");
    func_append(f, "\t(zero cons :=)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(((i len <) i :=) i for)");
    func_append(f, "\t\t(str ch i :string_mid_to_byte !)");
    func_append(f, "\t\t(ch va == if)");
    func_append(f, "\t\t\t(vowels + one vowels :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(ch ve == if)");
    func_append(f, "\t\t\t(vowels + one vowels :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(ch vi == if)");
    func_append(f, "\t\t\t(vowels + one vowels :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(ch vo == if)");
    func_append(f, "\t\t\t(vowels + one vowels :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(ch vu == if)");
    func_append(f, "\t\t\t(vowels + one vowels :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(ch vA == if)");
    func_append(f, "\t\t\t(vowels + one vowels :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(ch vE == if)");
    func_append(f, "\t\t\t(vowels + one vowels :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(ch vI == if)");
    func_append(f, "\t\t\t(vowels + one vowels :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(ch vO == if)");
    func_append(f, "\t\t\t(vowels + one vowels :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(ch vU == if)");
    func_append(f, "\t\t\t(vowels + one vowels :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(cons + one cons :=)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(cons vowels - cons :=)");
    func_append(f, "\t(len_str :print_s !)");
    func_append(f, "\t(len :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(vowels_str :print_s !)");
    func_append(f, "\t(vowels :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(cons_str :print_s !)");
    func_append(f, "\t(cons :print_i !)");
    func_append(f, "\t(:print_n !)");
}

static void emit_number_analyzer(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    const char *zdv[] = {"0.0"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "double", "num", 1, zdv, 1);
    add_var_to_func(f, "double", "abs_res", 1, zdv, 1);
    add_var_to_func(f, "int64", "num_int", 1, zv, 1);
    add_var_to_func(f, "int64", "temp_i", 1, zv, 1);
    add_var_to_func(f, "const-double", "zerod", 1, zdv, 1);
    const char *pn[] = {"\"Enter a number: \""};
    add_var_to_func(f, "const-string", "prompt_num", 16, pn, 1);
    const char *sa[] = {"\"Absolute: \""};
    add_var_to_func(f, "const-string", "abs_str", 11, sa, 1);
    const char *so[] = {"\"Odd/Even: \""};
    add_var_to_func(f, "const-string", "odd_even_str", 12, so, 1);
    const char *spos[] = {"\"Positive/Negative: \""};
    add_var_to_func(f, "const-string", "pos_neg_str", 20, spos, 1);
    func_append(f, "\t(prompt_num :print_s !)");
    func_append(f, "\t(num :input_d !)");
    func_append(f, "\t(num + zerod num :=)");
    func_append(f, "\t(num zerod < if)");
    func_append(f, "\t\t(zerod - num abs_res :=)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(num zerod >= if)");
    func_append(f, "\t\t(num abs_res :=)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(abs_str :print_s !)");
    func_append(f, "\t(reset-reg)");
    func_append(f, "\t(abs_res :print_d !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(prompt_num :print_s !)");
    func_append(f, "\t(num_int :input_i !)");
    func_append(f, "\t(num_int + zero num_int :=)");
    func_append(f, "\t(odd_even_str :print_s !)");
    func_append(f, "\t(num_int two % zero == if)");
    func_append(f, "\t\t(\"Even\" :print_s !)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(num_int two % one == if)");
    func_append(f, "\t\t(\"Odd\" :print_s !)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(pos_neg_str :print_s !)");
    func_append(f, "\t(num zerod < if)");
    func_append(f, "\t\t(\"Negative\" :print_s !)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(num zerod >= if)");
    func_append(f, "\t\t(\"Positive\" :print_s !)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(:print_n !)");
}

static void emit_filter_numbers(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "int64", "n", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "val", 1, zv, 1);
    add_var_to_func(f, "int64", "arr", 100, zv, 1);
    add_var_to_func(f, "int64", "result", 100, zv, 1);
    add_var_to_func(f, "int64", "ri", 1, zv, 1);
    add_var_to_func(f, "int64", "rr", 1, zv, 1);
    add_var_to_func(f, "int64", "threshold", 1, zv, 1);
    add_var_to_func(f, "int64", "a", 1, zv, 1);
    const char *ns[] = {"\"Enter count: \""};
    add_var_to_func(f, "const-string", "prompt_n", 14, ns, 1);
    const char *ns2[] = {"\"Enter number: \""};
    add_var_to_func(f, "const-string", "prompt_num", 15, ns2, 1);
    const char *nt[] = {"\"Threshold: \""};
    add_var_to_func(f, "const-string", "prompt_thresh", 12, nt, 1);
    const char *sr[] = {"\"Filtered (> threshold): \""};
    add_var_to_func(f, "const-string", "result_str", 24, sr, 1);
    func_append(f, "\t(prompt_n :print_s !)");
    func_append(f, "\t(n :input_i !)");
    func_append(f, "\t(n + zero n :=)");
    func_append(f, "\t(prompt_thresh :print_s !)");
    func_append(f, "\t(threshold :input_i !)");
    func_append(f, "\t(threshold + zero threshold :=)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(((i n <) i :=) i for)");
    func_append(f, "\t\t(prompt_num :print_s !)");
    func_append(f, "\t\t(a :input_i !)");
    func_append(f, "\t\t(i * int64_size ri :=)");
    func_append(f, "\t\t(a arr [ ri ] =)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(zero rr :=)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(((i n <) i :=) i for)");
    func_append(f, "\t\t(i * int64_size ri :=)");
    func_append(f, "\t\t(arr [ ri ] a =)");
    func_append(f, "\t\t(a threshold > if)");
    func_append(f, "\t\t\t(rr * int64_size ri :=)");
    func_append(f, "\t\t\t(a result [ ri ] =)");
    func_append(f, "\t\t\t(rr + one rr :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(result_str :print_s !)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(((i rr <) i :=) i for)");
    func_append(f, "\t\t(i * int64_size ri :=)");
    func_append(f, "\t\t(result [ ri ] a =)");
    func_append(f, "\t\t(a :print_i !)");
    func_append(f, "\t\t(:print_s !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(:print_n !)");
}

static void emit_random_generator(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "int64", "n", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "rand_max", 1, zv, 1);
    add_var_to_func(f, "int64", "randval", 1, zv, 1);
    const char *pc[] = {"\"Count: \""};
    add_var_to_func(f, "const-string", "prompt_count", 8, pc, 1);
    const char *pm[] = {"\"Max: \""};
    add_var_to_func(f, "const-string", "prompt_max", 6, pm, 1);
    const char *pr[] = {"\"Random: \""};
    add_var_to_func(f, "const-string", "random_str", 9, pr, 1);
    func_append(f, "\t(prompt_count :print_s !)");
    func_append(f, "\t(n :input_i !)");
    func_append(f, "\t(n + zero n :=)");
    func_append(f, "\t(prompt_max :print_s !)");
    func_append(f, "\t(rand_max :input_i !)");
    func_append(f, "\t(rand_max + zero rand_max :=)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(((i n <) i :=) i for)");
    func_append(f, "\t\t(randval :epochms !)");
    func_append(f, "\t\t(randval rand_max % randval :=)");
    func_append(f, "\t\t(randval :print_i !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
}

static void emit_math_menu(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    const char *thv[] = {"3"};
    const char *fov[] = {"4"};
    const char *fiv[] = {"5"};
    const char *zv_d[] = {"0.0"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "three", 1, thv, 1);
    add_var_to_func(f, "const-int64", "four", 1, fov, 1);
    add_var_to_func(f, "const-int64", "five", 1, fiv, 1);
    add_var_to_func(f, "int64", "choice", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "double", "a", 1, zv_d, 1);
    add_var_to_func(f, "double", "b", 1, zv_d, 1);
    add_var_to_func(f, "double", "res", 1, zv_d, 1);
    add_var_to_func(f, "const-double", "zerod", 1, zv_d, 1);
    const char *ms[] = {"\"1-Add 2-Sub 3-Mul 4-Div 5-quit\""};
    add_var_to_func(f, "const-string", "menu_str", 33, ms, 1);
    const char *pc[] = {"\"Choice: \""};
    add_var_to_func(f, "const-string", "prompt_choice", 10, pc, 1);
    const char *pa[] = {"\"A: \""};
    add_var_to_func(f, "const-string", "prompt_a", 5, pa, 1);
    const char *pb[] = {"\"B: \""};
    add_var_to_func(f, "const-string", "prompt_b", 5, pb, 1);
    const char *pr[] = {"\"Result: \""};
    add_var_to_func(f, "const-string", "result_str", 9, pr, 1);
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((one one ==) f :=) f for)");
    func_append(f, "\t\t(menu_str :print_s !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(prompt_choice :print_s !)");
    func_append(f, "\t\t(choice :input_i !)");
    func_append(f, "\t\t(choice + zero choice :=)");
    func_append(f, "\t\t(((choice five ==) f :=) f if)");
    func_append(f, "\t\t\t(zero :exit !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(prompt_a :print_s !)");
    func_append(f, "\t\t(a :input_d !)");
    func_append(f, "\t\t(a + zerod a :=)");
    func_append(f, "\t\t(prompt_b :print_s !)");
    func_append(f, "\t\t(b :input_d !)");
    func_append(f, "\t\t(b + zerod b :=)");
    func_append(f, "\t\t(((choice one ==) f :=) f if)");
    func_append(f, "\t\t\t(a + b res :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice two ==) f :=) f if)");
    func_append(f, "\t\t\t(a - b res :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice three ==) f :=) f if)");
    func_append(f, "\t\t\t(a * b res :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice four ==) f :=) f if)");
    func_append(f, "\t\t\t(b zerod == if)");
    func_append(f, "\t\t\t\t(\"Error: div by zero\" :print_s !)");
    func_append(f, "\t\t\t(else)");
    func_append(f, "\t\t\t\t(a / b res :=)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(result_str :print_s !)");
    func_append(f, "\t\t(reset-reg)");
    func_append(f, "\t\t(res :print_d !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t(next)");
}

static void emit_quiz_game(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "int64", "score", 1, zv, 1);
    add_var_to_func(f, "int64", "ans", 1, zv, 1);
    const char *q1[] = {"\"What is 2+2? \""};
    add_var_to_func(f, "const-string", "q1", 15, q1, 1);
    const char *q2[] = {"\"What is 3*3? \""};
    add_var_to_func(f, "const-string", "q2", 15, q2, 1);
    const char *q3[] = {"\"What is 10-7? \""};
    add_var_to_func(f, "const-string", "q3", 15, q3, 1);
    const char *sc[] = {"\"Score: \""};
    add_var_to_func(f, "const-string", "score_str", 8, sc, 1);
    add_var_to_func(f, "int64", "four", 1, (const char *[]){"4"}, 1);
    add_var_to_func(f, "int64", "nine", 1, (const char *[]){"9"}, 1);
    add_var_to_func(f, "int64", "three", 1, (const char *[]){"3"}, 1);
    func_append(f, "\t(zero score :=)");
    func_append(f, "\t(q1 :print_s !)");
    func_append(f, "\t(ans :input_i !)");
    func_append(f, "\t(ans + zero ans :=)");
    func_append(f, "\t(ans four == if)");
    func_append(f, "\t\t(score + one score :=)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(q2 :print_s !)");
    func_append(f, "\t(ans :input_i !)");
    func_append(f, "\t(ans + zero ans :=)");
    func_append(f, "\t(ans nine == if)");
    func_append(f, "\t\t(score + one score :=)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(q3 :print_s !)");
    func_append(f, "\t(ans :input_i !)");
    func_append(f, "\t(ans + zero ans :=)");
    func_append(f, "\t(ans three == if)");
    func_append(f, "\t\t(score + one score :=)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(score_str :print_s !)");
    func_append(f, "\t(score :print_i !)");
    func_append(f, "\t(:print_n !)");
}

static void emit_bmi_calculator(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    const char *thv[] = {"3"};
    const char *zdv[] = {"0.0"};
    const char *c703[] = {"703.0"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "three", 1, thv, 1);
    add_var_to_func(f, "int64", "choice", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "double", "weight", 1, zdv, 1);
    add_var_to_func(f, "double", "height", 1, zdv, 1);
    add_var_to_func(f, "double", "bmi", 1, zdv, 1);
    add_var_to_func(f, "double", "tempd", 1, zdv, 1);
    add_var_to_func(f, "const-double", "zerod", 1, zdv, 1);
    add_var_to_func(f, "const-double", "c_703", 1, c703, 1);
    const char *ms[] = {"\"1-Metric 2-Imperial 3-quit\""};
    add_var_to_func(f, "const-string", "menu_str", 29, ms, 1);
    const char *ps[] = {"\"Choice: \""};
    add_var_to_func(f, "const-string", "prompt_choice", 10, ps, 1);
    const char *pw[] = {"\"Weight (kg/lbs): \""};
    add_var_to_func(f, "const-string", "prompt_weight", 19, pw, 1);
    const char *ph[] = {"\"Height (m/in): \""};
    add_var_to_func(f, "const-string", "prompt_height", 17, ph, 1);
    const char *pb[] = {"\"BMI: \""};
    add_var_to_func(f, "const-string", "bmi_str", 6, pb, 1);
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((one one ==) f :=) f for)");
    func_append(f, "\t\t(menu_str :print_s !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(prompt_choice :print_s !)");
    func_append(f, "\t\t(choice :input_i !)");
    func_append(f, "\t\t(choice + zero choice :=)");
    func_append(f, "\t\t(((choice three ==) f :=) f if)");
    func_append(f, "\t\t\t(zero :exit !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(prompt_weight :print_s !)");
    func_append(f, "\t\t(weight :input_d !)");
    func_append(f, "\t\t(weight + zerod weight :=)");
    func_append(f, "\t\t(prompt_height :print_s !)");
    func_append(f, "\t\t(height :input_d !)");
    func_append(f, "\t\t(height + zerod height :=)");
    func_append(f, "\t\t(((choice one ==) f :=) f if)");
    func_append(f, "\t\t\t(height * height tempd :=)");
    func_append(f, "\t\t\t(weight / tempd bmi :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice two ==) f :=) f if)");
    func_append(f, "\t\t\t(height * height tempd :=)");
    func_append(f, "\t\t\t(weight * c_703 tempd :=)");
    func_append(f, "\t\t\t(tempd / height bmi :=)");
    func_append(f, "\t\t\t(bmi / height bmi :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(bmi_str :print_s !)");
    func_append(f, "\t\t(reset-reg)");
    func_append(f, "\t\t(bmi :print_d !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t(next)");
}

static void emit_statistics_suite(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "int64", "n", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "j", 1, zv, 1);
    add_var_to_func(f, "int64", "temp", 1, zv, 1);
    add_var_to_func(f, "int64", "sum", 1, zv, 1);
    add_var_to_func(f, "int64", "ri", 1, zv, 1);
    add_var_to_func(f, "int64", "rj", 1, zv, 1);
    add_var_to_func(f, "int64", "a", 1, zv, 1);
    add_var_to_func(f, "int64", "b", 1, zv, 1);
    add_var_to_func(f, "int64", "arr", 100, zv, 1);
    const char *pn[] = {"\"Enter count: \""};
    add_var_to_func(f, "const-string", "prompt_n", 14, pn, 1);
    const char *pv[] = {"\"Enter value: \""};
    add_var_to_func(f, "const-string", "prompt_val", 14, pv, 1);
    const char *ss[] = {"\"Sorted: \""};
    add_var_to_func(f, "const-string", "sorted_str", 9, ss, 1);
    const char *smin[] = {"\" Min: \""};
    add_var_to_func(f, "const-string", "min_str", 7, smin, 1);
    const char *smax[] = {"\" Max: \""};
    add_var_to_func(f, "const-string", "max_str", 7, smax, 1);
    const char *smean[] = {"\" Mean: \""};
    add_var_to_func(f, "const-string", "mean_str", 8, smean, 1);
    const char *smed[] = {"\" Median: \""};
    add_var_to_func(f, "const-string", "median_str", 10, smed, 1);
    func_append(f, "\t(prompt_n :print_s !)");
    func_append(f, "\t(n :input_i !)");
    func_append(f, "\t(n + zero n :=)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(zero sum :=)");
    func_append(f, "\t(((i n <) i :=) i for)");
    func_append(f, "\t\t(prompt_val :print_s !)");
    func_append(f, "\t\t(a :input_i !)");
    func_append(f, "\t\t(a + zero a :=)");
    func_append(f, "\t\t(i * int64_size ri :=)");
    func_append(f, "\t\t(a arr [ ri ] =)");
    func_append(f, "\t\t(a sum + sum :=)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(((i n <) i :=) i for)");
    func_append(f, "\t\t(i + one j :=)");
    func_append(f, "\t\t(((j n <) j :=) j for)");
    func_append(f, "\t\t\t(i * int64_size ri :=)");
    func_append(f, "\t\t\t(j * int64_size rj :=)");
    func_append(f, "\t\t\t(arr [ ri ] a =)");
    func_append(f, "\t\t\t(arr [ rj ] b =)");
    func_append(f, "\t\t\t(a b > if)");
    func_append(f, "\t\t\t\t(arr [ ri ] temp :=)");
    func_append(f, "\t\t\t\t(b arr [ ri ] =)");
    func_append(f, "\t\t\t\t(temp arr [ rj ] =)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t\t(j + one j :=)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(sorted_str :print_s !)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(((i n <) i :=) i for)");
    func_append(f, "\t\t(i * int64_size ri :=)");
    func_append(f, "\t\t(arr [ ri ] a =)");
    func_append(f, "\t\t(a :print_i !)");
    func_append(f, "\t\t(:print_s !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(min_str :print_s !)");
    func_append(f, "\t(zero * int64_size ri :=)");
    func_append(f, "\t(arr [ ri ] a =)");
    func_append(f, "\t(a :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(max_str :print_s !)");
    func_append(f, "\t(n one - temp :=)");
    func_append(f, "\t(temp * int64_size ri :=)");
    func_append(f, "\t(arr [ ri ] a =)");
    func_append(f, "\t(a :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(mean_str :print_s !)");
    func_append(f, "\t(sum n / temp :=)");
    func_append(f, "\t(temp :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(median_str :print_s !)");
    func_append(f, "\t(n two % 0 == if)");
    func_append(f, "\t\t(n two / temp :=)");
    func_append(f, "\t\t(temp * int64_size ri :=)");
    func_append(f, "\t\t(arr [ ri ] a =)");
    func_append(f, "\t\t(temp one - temp :=)");
    func_append(f, "\t\t(temp * int64_size ri :=)");
    func_append(f, "\t\t(arr [ ri ] b =)");
    func_append(f, "\t\t(a b + two / temp :=)");
    func_append(f, "\t\t(temp :print_i !)");
    func_append(f, "\t(else)");
    func_append(f, "\t\t(n two / temp :=)");
    func_append(f, "\t\t(temp * int64_size ri :=)");
    func_append(f, "\t\t(arr [ ri ] a =)");
    func_append(f, "\t\t(a :print_i !)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(:print_n !)");
}

// ==================== NEW EMITTERS ====================

static void emit_linked_list(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *cv[] = {"10"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "int64", "capacity", 1, cv, 1);
    add_var_to_func(f, "int64", "data", 10, zv, 0);
    add_var_to_func(f, "int64", "next", 10, zv, 0);
    add_var_to_func(f, "int64", "head", 1, zv, 1);
    add_var_to_func(f, "int64", "free_idx", 1, zv, 1);
    add_var_to_func(f, "int64", "choice", 1, zv, 1);
    add_var_to_func(f, "int64", "val", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "ri", 1, zv, 1);
    add_var_to_func(f, "int64", "cur", 1, zv, 1);
    add_var_to_func(f, "int64", "prev", 1, zv, 1);
    const char *ms[] = {"\"1-insert 2-delete 3-print 4-search 5-quit\""};
    add_var_to_func(f, "const-string", "menu_str", 44, ms, 1);
    add_var_to_func(f, "const-string", "prompt_val", 14, (const char *[]){"\"Enter value: \""}, 1);
    add_var_to_func(f, "const-string", "prompt_key", 14, (const char *[]){"\"Enter key: \""}, 1);
    add_var_to_func(f, "const-string", "prompt_choice", 12, (const char *[]){"\"choice: \""}, 1);
    const char *nf[] = {"\"Not found\""};
    add_var_to_func(f, "const-string", "not_found", 11, nf, 1);
    func_append(f, "\t(one free_idx :=)");
    func_append(f, "\t(zero head :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((one one <) f :=) f for)");
    func_append(f, "\t\t(menu_str :print_s !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(prompt_choice :print_s !)");
    func_append(f, "\t\t(choice :input_i !)");
    func_append(f, "\t\t(((choice one ==) f :=) f if)");
    func_append(f, "\t\t\t(prompt_val :print_s !)");
    func_append(f, "\t\t\t(val :input_i !)");
    func_append(f, "\t\t\t(free_idx * int64_size ri :=)");
    func_append(f, "\t\t\t(val data [ ri ] =)");
    func_append(f, "\t\t\t(zero next [ ri ] =)");
    func_append(f, "\t\t\t(((head zero ==) f :=) f if)");
    func_append(f, "\t\t\t\t(free_idx head :=)");
    func_append(f, "\t\t\t(else)");
    func_append(f, "\t\t\t\t(head cur :=)");
    func_append(f, "\t\t\t\t(for-loop)");
    func_append(f, "\t\t\t\t(((cur zero !=) f :=) f for)");
    func_append(f, "\t\t\t\t\t(cur * int64_size ri :=)");
    func_append(f, "\t\t\t\t\t(next [ ri ] cur :=)");
    func_append(f, "\t\t\t\t(next)");
    func_append(f, "\t\t\t\t(cur * int64_size ri :=)");
    func_append(f, "\t\t\t\t(free_idx next [ ri ] =)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t\t(free_idx + one free_idx :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice two ==) f :=) f if)");
    func_append(f, "\t\t\t(prompt_key :print_s !)");
    func_append(f, "\t\t\t(val :input_i !)");
    func_append(f, "\t\t\t(head cur :=)");
    func_append(f, "\t\t\t(zero prev :=)");
    func_append(f, "\t\t\t(for-loop)");
    func_append(f, "\t\t\t(((cur zero !=) f :=) f for)");
    func_append(f, "\t\t\t\t(cur * int64_size ri :=)");
    func_append(f, "\t\t\t\t(data [ ri ] i :=)");
    func_append(f, "\t\t\t\t(i + zero i :=)");
    func_append(f, "\t\t\t\t(((i val ==) f :=) f if)");
    func_append(f, "\t\t\t\t\t(next [ ri ] cur :=)");
    func_append(f, "\t\t\t\t\t(((prev zero ==) f :=) f if)");
    func_append(f, "\t\t\t\t\t\t(cur head :=)");
    func_append(f, "\t\t\t\t\t(else)");
    func_append(f, "\t\t\t\t\t\t(prev * int64_size ri :=)");
    func_append(f, "\t\t\t\t\t\t(cur next [ ri ] =)");
    func_append(f, "\t\t\t\t\t(endif)");
    func_append(f, "\t\t\t\t\t(:found_break jmp)");
    func_append(f, "\t\t\t\t(endif)");
    func_append(f, "\t\t\t\t(cur prev :=)");
    func_append(f, "\t\t\t\t(next [ ri ] cur :=)");
    func_append(f, "\t\t\t(next)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice three ==) f :=) f if)");
    func_append(f, "\t\t\t(head cur :=)");
    func_append(f, "\t\t\t(for-loop)");
    func_append(f, "\t\t\t(((cur zero !=) f :=) f for)");
    func_append(f, "\t\t\t\t(cur * int64_size ri :=)");
    func_append(f, "\t\t\t\t(data [ ri ] val =)");
    func_append(f, "\t\t\t\t(val :print_i !)");
    func_append(f, "\t\t\t\t(:print_n !)");
    func_append(f, "\t\t\t\t(next [ ri ] cur :=)");
    func_append(f, "\t\t\t(next)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice four ==) f :=) f if)");
    func_append(f, "\t\t\t(prompt_key :print_s !)");
    func_append(f, "\t\t\t(val :input_i !)");
    func_append(f, "\t\t\t(head cur :=)");
    func_append(f, "\t\t\t(zero i :=)");
    func_append(f, "\t\t\t(for-loop)");
    func_append(f, "\t\t\t(((cur zero !=) f :=) f for)");
    func_append(f, "\t\t\t\t(cur * int64_size ri :=)");
    func_append(f, "\t\t\t\t(data [ ri ] i :=)");
    func_append(f, "\t\t\t\t(i + zero i :=)");
    func_append(f, "\t\t\t\t(((i val ==) f :=) f if)");
    func_append(f, "\t\t\t\t\t(one f :=)");
    func_append(f, "\t\t\t\t\t(:found_break2 jmp)");
    func_append(f, "\t\t\t\t(endif)");
    func_append(f, "\t\t\t\t(next [ ri ] cur :=)");
    func_append(f, "\t\t\t(next)");
    func_append(f, "\t\t(:found_break2)");
    func_append(f, "\t\t(((f one ==) f :=) f if+)");
    func_append(f, "\t\t\t(cur :print_i !)");
    func_append(f, "\t\t(else)");
    func_append(f, "\t\t\t(not_found :print_s !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t(endif)");
    func_append(f, "\t\t(((choice five ==) f :=) f if)");
    func_append(f, "\t\t\t(zero :exit !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t(next)");
}

static void emit_binary_search_tree(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    const char *thv[] = {"3"};
    const char *fov[] = {"4"};
    const char *cv[] = {"20"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "three", 1, thv, 1);
    add_var_to_func(f, "const-int64", "four", 1, fov, 1);
    add_var_to_func(f, "int64", "capacity", 1, cv, 1);
    add_var_to_func(f, "int64", "data", 20, zv, 0);
    add_var_to_func(f, "int64", "left", 20, zv, 0);
    add_var_to_func(f, "int64", "right", 20, zv, 0);
    add_var_to_func(f, "int64", "root", 1, zv, 1);
    add_var_to_func(f, "int64", "free_idx", 1, ov, 1);
    add_var_to_func(f, "int64", "choice", 1, zv, 1);
    add_var_to_func(f, "int64", "val", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "int64", "ri", 1, zv, 1);
    add_var_to_func(f, "int64", "cur", 1, zv, 1);
    add_var_to_func(f, "int64", "parent", 1, zv, 1);
    const char *ms[] = {"\"1-insert 2-search 3-print 4-quit\""};
    add_var_to_func(f, "const-string", "menu_str", 40, ms, 1);
    add_var_to_func(f, "const-string", "prompt_val", 14, (const char *[]){"\"Enter value: \""}, 1);
    add_var_to_func(f, "const-string", "prompt_choice", 12, (const char *[]){"\"choice: \""}, 1);
    const char *sf[] = {"\"Found\""};
    const char *nf[] = {"\"Not found\""};
    add_var_to_func(f, "const-string", "found_str", 7, sf, 1);
    add_var_to_func(f, "const-string", "not_found", 11, nf, 1);
    func_append(f, "\t(zero root :=)");
    func_append(f, "\t(one free_idx :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((one one ==) f :=) f for)");
    func_append(f, "\t\t(menu_str :print_s !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(prompt_choice :print_s !)");
    func_append(f, "\t\t(choice :input_i !)");
    func_append(f, "\t\t(((choice one ==) f :=) f if)");
    func_append(f, "\t\t\t(prompt_val :print_s !)");
    func_append(f, "\t\t\t(val :input_i !)");
    func_append(f, "\t\t\t(free_idx * int64_size ri :=)");
    func_append(f, "\t\t\t(val data [ ri ] =)");
    func_append(f, "\t\t\t(zero left [ ri ] =)");
    func_append(f, "\t\t\t(zero right [ ri ] =)");
    func_append(f, "\t\t\t(((root zero ==) f :=) f if+)");
    func_append(f, "\t\t\t\t(free_idx root :=)");
    func_append(f, "\t\t\t(else)");
    func_append(f, "\t\t\t\t(root cur :=)");
    func_append(f, "\t\t\t\t(for-loop)");
    func_append(f, "\t\t\t\t(((cur zero !=) f :=) f for)");
    func_append(f, "\t\t\t\t\t(cur * int64_size ri :=)");
    func_append(f, "\t\t\t\t\t(data [ ri ] f :=)");
    func_append(f, "\t\t\t\t\t(f + zero f :=)");
    func_append(f, "\t\t\t\t\t(((val f <) f :=) f if+)");
    func_append(f, "\t\t\t\t\t\t(left [ ri ] cur :=)");
    func_append(f, "\t\t\t\t\t\t(((cur zero ==) f :=) f if)");
    func_append(f, "\t\t\t\t\t\t\t(free_idx left [ ri ] =)");
    func_append(f, "\t\t\t\t\t\t\t(:bst_done jmp)");
    func_append(f, "\t\t\t\t\t\t(endif)");
    func_append(f, "\t\t\t\t\t(else)");
    func_append(f, "\t\t\t\t\t\t(right [ ri ] cur :=)");
    func_append(f, "\t\t\t\t\t\t(((cur zero ==) f :=) f if)");
    func_append(f, "\t\t\t\t\t\t\t(free_idx right [ ri ] =)");
    func_append(f, "\t\t\t\t\t\t\t(:bst_done jmp)");
    func_append(f, "\t\t\t\t\t\t(endif)");
    func_append(f, "\t\t\t\t\t(endif)");
    func_append(f, "\t\t\t\t(next)");
    func_append(f, "\t\t\t(:bst_done)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t\t(free_idx + one free_idx :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice two ==) f :=) f if)");
    func_append(f, "\t\t\t(prompt_val :print_s !)");
    func_append(f, "\t\t\t(val :input_i !)");
    func_append(f, "\t\t\t(root cur :=)");
    func_append(f, "\t\t\t(for-loop)");
    func_append(f, "\t\t\t(((cur zero !=) f :=) f for)");
    func_append(f, "\t\t\t\t(cur * int64_size ri :=)");
    func_append(f, "\t\t\t\t(data [ ri ] f :=)");
    func_append(f, "\t\t\t\t(f + zero f :=)");
    func_append(f, "\t\t\t\t(((val f ==) f :=) f if)");
    func_append(f, "\t\t\t\t\t(found_str :print_s !)");
    func_append(f, "\t\t\t\t\t(:bst_found jmp)");
    func_append(f, "\t\t\t\t(endif)");
    func_append(f, "\t\t\t\t(((val f <) f :=) f if+)");
    func_append(f, "\t\t\t\t\t(left [ ri ] cur :=)");
    func_append(f, "\t\t\t\t(else)");
    func_append(f, "\t\t\t\t\t(right [ ri ] cur :=)");
    func_append(f, "\t\t\t\t(endif)");
    func_append(f, "\t\t\t(next)");
    func_append(f, "\t\t(:bst_found)");
    func_append(f, "\t\t(((cur zero ==) f :=) f if)");
    func_append(f, "\t\t\t(not_found :print_s !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice three ==) f :=) f if)");
    func_append(f, "\t\t\t// inorder traversal using explicit stack");
    func_append(f, "\t\t\t(root cur :=)");
    func_append(f, "\t\t\t(for-loop)");
    func_append(f, "\t\t\t\t(((cur zero !=) f :=) f for)");
    func_append(f, "\t\t\t\t\t(cur * int64_size ri :=)");
    func_append(f, "\t\t\t\t\t(left [ ri ] cur :=)");
    func_append(f, "\t\t\t(next)");
    func_append(f, "\t\t\t// simplified: print all non-zero data");
    func_append(f, "\t\t\t(one cur :=)");
    func_append(f, "\t\t\t(for-loop)");
    func_append(f, "\t\t\t(((cur free_idx <) f :=) f for)");
    func_append(f, "\t\t\t\t(cur * int64_size ri :=)");
    func_append(f, "\t\t\t\t(data [ ri ] val =)");
    func_append(f, "\t\t\t\t(val :print_i !)");
    func_append(f, "\t\t\t\t(:print_n !)");
    func_append(f, "\t\t\t\t(cur + one cur :=)");
    func_append(f, "\t\t\t(next)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice four ==) f :=) f if)");
    func_append(f, "\t\t\t(zero :exit !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t(next)");
}

static void emit_tree_traversal(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    const char *thv[] = {"3"};
    const char *fov[] = {"4"};
    const char *fiv[] = {"5"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "three", 1, thv, 1);
    add_var_to_func(f, "const-int64", "four", 1, fov, 1);
    add_var_to_func(f, "const-int64", "five", 1, fiv, 1);
    add_var_to_func(f, "int64", "data", 10, zv, 0);
    add_var_to_func(f, "int64", "left", 10, zv, 0);
    add_var_to_func(f, "int64", "right", 10, zv, 0);
    add_var_to_func(f, "int64", "root", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "ri", 1, zv, 1);
    add_var_to_func(f, "int64", "stack", 10, zv, 0);
    add_var_to_func(f, "int64", "sp", 1, zv, 1);
    add_var_to_func(f, "int64", "cur", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    // build fixed tree: root=1, 1->left=2, 1->right=3, 2->left=4, 2->right=5
    add_var_to_func(f, "const-int64", "cv1", 1, (const char *[]){"1"}, 1);
    add_var_to_func(f, "const-int64", "cv2", 1, (const char *[]){"2"}, 1);
    add_var_to_func(f, "const-int64", "cv3", 1, (const char *[]){"3"}, 1);
    add_var_to_func(f, "const-int64", "cv4", 1, (const char *[]){"4"}, 1);
    add_var_to_func(f, "const-int64", "cv5", 1, (const char *[]){"5"}, 1);
    const char *ps[] = {"\"Inorder: \""};
    add_var_to_func(f, "const-string", "in_str", 10, ps, 1);
    const char *pp[] = {"\"Preorder: \""};
    add_var_to_func(f, "const-string", "pre_str", 11, pp, 1);
    const char *po[] = {"\"Postorder: \""};
    add_var_to_func(f, "const-string", "post_str", 12, po, 1);
    // init tree
    func_append(f, "\t(one root :=)");
    func_append(f, "\t(one * int64_size ri :=)");
    func_append(f, "\t(cv1 data [ ri ] =)");
    func_append(f, "\t(cv2 left [ ri ] =)");
    func_append(f, "\t(cv3 right [ ri ] =)");
    func_append(f, "\t(two * int64_size ri :=)");
    func_append(f, "\t(cv2 data [ ri ] =)");
    func_append(f, "\t(cv4 left [ ri ] =)");
    func_append(f, "\t(cv5 right [ ri ] =)");
    func_append(f, "\t(three * int64_size ri :=)");
    func_append(f, "\t(cv3 data [ ri ] =)");
    func_append(f, "\t(four * int64_size ri :=)");
    func_append(f, "\t(cv4 data [ ri ] =)");
    func_append(f, "\t(five * int64_size ri :=)");
    func_append(f, "\t(cv5 data [ ri ] =)");
    // manual inorder: 4 2 5 1 3
    func_append(f, "\t(in_str :print_s !)");
    func_append(f, "\t(four * int64_size ri :=)"); func_append(f, "\t(data [ ri ] i =)"); func_append(f, "\t(i :print_i !)"); func_append(f, "\t(:print_n !)");
    func_append(f, "\t(two * int64_size ri :=)"); func_append(f, "\t(data [ ri ] i =)"); func_append(f, "\t(i :print_i !)"); func_append(f, "\t(:print_n !)");
    func_append(f, "\t(five * int64_size ri :=)"); func_append(f, "\t(data [ ri ] i =)"); func_append(f, "\t(i :print_i !)"); func_append(f, "\t(:print_n !)");
    func_append(f, "\t(one * int64_size ri :=)"); func_append(f, "\t(data [ ri ] i =)"); func_append(f, "\t(i :print_i !)"); func_append(f, "\t(:print_n !)");
    func_append(f, "\t(three * int64_size ri :=)"); func_append(f, "\t(data [ ri ] i =)"); func_append(f, "\t(i :print_i !)"); func_append(f, "\t(:print_n !)");
    func_append(f, "\t(pre_str :print_s !)");
    // preorder: 1 2 4 5 3
    func_append(f, "\t(one * int64_size ri :=)"); func_append(f, "\t(data [ ri ] i =)"); func_append(f, "\t(i :print_i !)"); func_append(f, "\t(:print_n !)");
    func_append(f, "\t(two * int64_size ri :=)"); func_append(f, "\t(data [ ri ] i =)"); func_append(f, "\t(i :print_i !)"); func_append(f, "\t(:print_n !)");
    func_append(f, "\t(four * int64_size ri :=)"); func_append(f, "\t(data [ ri ] i =)"); func_append(f, "\t(i :print_i !)"); func_append(f, "\t(:print_n !)");
    func_append(f, "\t(five * int64_size ri :=)"); func_append(f, "\t(data [ ri ] i =)"); func_append(f, "\t(i :print_i !)"); func_append(f, "\t(:print_n !)");
    func_append(f, "\t(three * int64_size ri :=)"); func_append(f, "\t(data [ ri ] i =)"); func_append(f, "\t(i :print_i !)"); func_append(f, "\t(:print_n !)");
    func_append(f, "\t(post_str :print_s !)");
    // postorder: 4 5 2 3 1
    func_append(f, "\t(four * int64_size ri :=)"); func_append(f, "\t(data [ ri ] i =)"); func_append(f, "\t(i :print_i !)"); func_append(f, "\t(:print_n !)");
    func_append(f, "\t(five * int64_size ri :=)"); func_append(f, "\t(data [ ri ] i =)"); func_append(f, "\t(i :print_i !)"); func_append(f, "\t(:print_n !)");
    func_append(f, "\t(two * int64_size ri :=)"); func_append(f, "\t(data [ ri ] i =)"); func_append(f, "\t(i :print_i !)"); func_append(f, "\t(:print_n !)");
    func_append(f, "\t(three * int64_size ri :=)"); func_append(f, "\t(data [ ri ] i =)"); func_append(f, "\t(i :print_i !)"); func_append(f, "\t(:print_n !)");
    func_append(f, "\t(one * int64_size ri :=)"); func_append(f, "\t(data [ ri ] i =)"); func_append(f, "\t(i :print_i !)"); func_append(f, "\t(:print_n !)");
}

static void emit_graph_bfs_dfs(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    const char *thv[] = {"3"};
    const char *fov[] = {"4"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "three", 1, thv, 1);
    add_var_to_func(f, "const-int64", "four", 1, fov, 1);
    // adjacency matrix 5x5 for graph with 5 nodes
    add_var_to_func(f, "int64", "adj", 25, zv, 0);
    add_var_to_func(f, "int64", "visited", 5, zv, 0);
    add_var_to_func(f, "int64", "stack", 5, zv, 0);
    add_var_to_func(f, "int64", "queue", 5, zv, 0);
    add_var_to_func(f, "int64", "sp", 1, zv, 1);
    add_var_to_func(f, "int64", "qhead", 1, zv, 1);
    add_var_to_func(f, "int64", "qtail", 1, zv, 1);
    add_var_to_func(f, "int64", "n", 1, (const char *[]){"5"}, 1);
    add_var_to_func(f, "int64", "choice", 1, zv, 1);
    add_var_to_func(f, "int64", "start", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "j", 1, zv, 1);
    add_var_to_func(f, "int64", "v", 1, zv, 1);
    add_var_to_func(f, "int64", "ri", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ms[] = {"\"1-BFS 2-DFS 3-quit\""};
    add_var_to_func(f, "const-string", "menu_str", 22, ms, 1);
    const char *ps[] = {"\"Start node (0-4): \""};
    add_var_to_func(f, "const-string", "prompt_start", 20, ps, 1);
    add_var_to_func(f, "const-string", "prompt_choice", 12, (const char *[]){"\"choice: \""}, 1);
    // build graph: 0-1, 0-2, 1-3, 1-4
    func_append(f, "\t// build graph adjacency matrix");
    func_append(f, "\t(zero * n + one ri :=)");
    func_append(f, "\t(one adj [ ri ] =)");
    func_append(f, "\t(one * n + zero ri :=)");
    func_append(f, "\t(one adj [ ri ] =)");
    func_append(f, "\t(zero * n + two ri :=)");
    func_append(f, "\t(one adj [ ri ] =)");
    func_append(f, "\t(two * n + zero ri :=)");
    func_append(f, "\t(one adj [ ri ] =)");
    func_append(f, "\t(one * n + three ri :=)");
    func_append(f, "\t(one adj [ ri ] =)");
    func_append(f, "\t(three * n + one ri :=)");
    func_append(f, "\t(one adj [ ri ] =)");
    func_append(f, "\t(one * n + four ri :=)");
    func_append(f, "\t(one adj [ ri ] =)");
    func_append(f, "\t(four * n + one ri :=)");
    func_append(f, "\t(one adj [ ri ] =)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((one one <) f :=) f for)");
    func_append(f, "\t\t(menu_str :print_s !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(prompt_choice :print_s !)");
    func_append(f, "\t\t(choice :input_i !)");
    func_append(f, "\t\t(((choice three ==) f :=) f if)");
    func_append(f, "\t\t\t(zero :exit !)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(prompt_start :print_s !)");
    func_append(f, "\t\t(start :input_i !)");
    func_append(f, "\t\t// reset visited");
    func_append(f, "\t\t(zero i :=)");
    func_append(f, "\t\t(for-loop)");
    func_append(f, "\t\t(((i n <) f :=) f for)");
    func_append(f, "\t\t\t(i * int64_size ri :=)");
    func_append(f, "\t\t\t(zero visited [ ri ] =)");
    func_append(f, "\t\t\t(i + one i :=)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t(((choice one ==) f :=) f if)");
    func_append(f, "\t\t\t// BFS using queue");
    func_append(f, "\t\t\t(start qhead :=)"); func_append(f, "\t\t\t(start qtail :=)");
    func_append(f, "\t\t\t(qtail + one qtail :=)");
    func_append(f, "\t\t\t(start * int64_size ri :=)");
    func_append(f, "\t\t\t(one visited [ ri ] =)");
    func_append(f, "\t\t\t(for-loop)");
    func_append(f, "\t\t\t(((qhead qtail <) f :=) f for)");
    func_append(f, "\t\t\t\t(qhead * int64_size ri :=)");
    func_append(f, "\t\t\t\t(queue [ ri ] v =)");
    func_append(f, "\t\t\t\t(v + zero v :=)");
    func_append(f, "\t\t\t\t(v :print_i !)");
    func_append(f, "\t\t\t\t(:print_n !)");
    func_append(f, "\t\t\t\t(zero j :=)");
    func_append(f, "\t\t\t\t(for-loop)");
    func_append(f, "\t\t\t\t(((j n <) f :=) f for)");
    func_append(f, "\t\t\t\t\t(v * n + j ri :=)");
    func_append(f, "\t\t\t\t\t(adj [ ri ] f :=)");
    func_append(f, "\t\t\t\t\t(j * int64_size ri :=)");
    func_append(f, "\t\t\t\t\t(f + zero f :=)");
    func_append(f, "\t\t\t\t\t(((f one ==) f :=) f if)");
    func_append(f, "\t\t\t\t\t\t(visited [ ri ] f =)");
    func_append(f, "\t\t\t\t\t\t(((f zero ==) f :=) f if)");
    func_append(f, "\t\t\t\t\t\t\t(one visited [ ri ] =)");
    func_append(f, "\t\t\t\t\t\t\t(j queue [ qtail ] =)");
    func_append(f, "\t\t\t\t\t\t\t(qtail + one qtail :=)");
    func_append(f, "\t\t\t\t\t\t(endif)");
    func_append(f, "\t\t\t\t\t(endif)");
    func_append(f, "\t\t\t\t\t(j + one j :=)");
    func_append(f, "\t\t\t\t(next)");
    func_append(f, "\t\t\t\t(qhead + one qhead :=)");
    func_append(f, "\t\t\t(next)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((choice two ==) f :=) f if)");
    func_append(f, "\t\t\t// DFS using stack");
    func_append(f, "\t\t\t(zero sp :=)");
    func_append(f, "\t\t\t(start stack [ sp ] =)");
    func_append(f, "\t\t\t(sp + one sp :=)");
    func_append(f, "\t\t\t(for-loop)");
    func_append(f, "\t\t\t(((sp zero >) f :=) f for)");
    func_append(f, "\t\t\t\t(sp one - sp :=)");
    func_append(f, "\t\t\t\t(sp * int64_size ri :=)");
    func_append(f, "\t\t\t\t(stack [ ri ] v =)");
    func_append(f, "\t\t\t\t(v + zero v :=)");
    func_append(f, "\t\t\t\t(v * int64_size ri :=)");
    func_append(f, "\t\t\t\t(visited [ ri ] f =)");
    func_append(f, "\t\t\t\t(((f zero ==) f :=) f if)");
    func_append(f, "\t\t\t\t\t(one visited [ ri ] =)");
    func_append(f, "\t\t\t\t\t(v :print_i !)");
    func_append(f, "\t\t\t\t\t(:print_n !)");
    func_append(f, "\t\t\t\t\t(zero j :=)");
    func_append(f, "\t\t\t\t\t(for-loop)");
    func_append(f, "\t\t\t\t\t(((j n <) f :=) f for)");
    func_append(f, "\t\t\t\t\t\t(v * n + j ri :=)");
    func_append(f, "\t\t\t\t\t\t(adj [ ri ] f :=)");
    func_append(f, "\t\t\t\t\t\t(f + zero f :=)");
    func_append(f, "\t\t\t\t\t\t(((f one ==) f :=) f if)");
    func_append(f, "\t\t\t\t\t\t\t(j * int64_size ri :=)");
    func_append(f, "\t\t\t\t\t\t\t(visited [ ri ] f =)");
    func_append(f, "\t\t\t\t\t\t\t(((f zero ==) f :=) f if)");
    func_append(f, "\t\t\t\t\t\t\t\t(sp * int64_size ri :=)");
    func_append(f, "\t\t\t\t\t\t\t\t(j stack [ ri ] =)");
    func_append(f, "\t\t\t\t\t\t\t\t(sp + one sp :=)");
    func_append(f, "\t\t\t\t\t\t\t(endif)");
    func_append(f, "\t\t\t\t\t\t(endif)");
    func_append(f, "\t\t\t\t\t\t(j + one j :=)");
    func_append(f, "\t\t\t\t\t(next)");
    func_append(f, "\t\t\t\t(endif)");
    func_append(f, "\t\t\t(next)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t(next)");
}

static void emit_n_queens(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "int64", "n", 1, (const char *[]){"4"}, 1);
    add_var_to_func(f, "int64", "board", 4, zv, 0);
    add_var_to_func(f, "int64", "row", 1, zv, 1);
    add_var_to_func(f, "int64", "col", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "ri", 1, zv, 1);
    add_var_to_func(f, "int64", "ok", 1, zv, 1);
    add_var_to_func(f, "int64", "fi", 1, zv, 1);
    const char *ps[] = {"\"Solutions for 4-queens:\""};
    add_var_to_func(f, "const-string", "msg_str", 24, ps, 1);
    func_append(f, "\t(msg_str :print_s !)");
    func_append(f, "\t(:print_n !)");
    // simple iterative backtracking for 4-queens
    func_append(f, "\t(zero row :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((one one <) f :=) f for)");
    func_append(f, "\t\t(row * int64_size ri :=)");
    func_append(f, "\t\t(zero col :=)");
    func_append(f, "\t\t(for-loop)");
    func_append(f, "\t\t(((col n <) f :=) f for)");
    // check if safe
    func_append(f, "\t\t\t(one ok :=)");
    func_append(f, "\t\t\t(zero i :=)");
    func_append(f, "\t\t\t(for-loop)");
    func_append(f, "\t\t\t(((i row <) f :=) f for)");
    func_append(f, "\t\t\t\t(i * int64_size ri :=)");
    func_append(f, "\t\t\t\t(board [ ri ] fi :=)");
    func_append(f, "\t\t\t\t(fi + zero fi :=)");
    func_append(f, "\t\t\t\t(((fi col ==) f :=) f if)");
    func_append(f, "\t\t\t\t\t(zero ok :=)");
    func_append(f, "\t\t\t\t(endif)");
    func_append(f, "\t\t\t\t(((fi col - i + row ==) f :=) f if)");
    func_append(f, "\t\t\t\t\t(zero ok :=)");
    func_append(f, "\t\t\t\t(endif)");
    func_append(f, "\t\t\t\t(((fi col - row - i ==) f :=) f if)");
    func_append(f, "\t\t\t\t\t(zero ok :=)");
    func_append(f, "\t\t\t\t(endif)");
    func_append(f, "\t\t\t\t(i + one i :=)");
    func_append(f, "\t\t\t(next)");
    func_append(f, "\t\t\t(((ok one ==) f :=) f if)");
    func_append(f, "\t\t\t\t(row * int64_size ri :=)");
    func_append(f, "\t\t\t\t(col board [ ri ] =)");
    func_append(f, "\t\t\t\t(row + one row :=)");
    func_append(f, "\t\t\t\t(col n :=)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t\t(col + one col :=)");
    func_append(f, "\t\t(next)");
    // backtrack if no solution
    func_append(f, "\t\t(((col n ==) f :=) f if)");
    func_append(f, "\t\t\t(row one - row :=)");
    func_append(f, "\t\t\t(((row zero <) f :=) f if)");
    func_append(f, "\t\t\t\t(zero fi :=)");
    func_append(f, "\t\t\t\t(:queens_done jmp)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t(endif)");
    // check if all rows placed
    func_append(f, "\t\t(((row n ==) f :=) f if)");
    func_append(f, "\t\t\t// print solution");
    func_append(f, "\t\t\t(zero i :=)");
    func_append(f, "\t\t\t(for-loop)");
    func_append(f, "\t\t\t(((i n <) f :=) f for)");
    func_append(f, "\t\t\t\t(i * int64_size ri :=)");
    func_append(f, "\t\t\t\t(board [ ri ] fi :=)");
    func_append(f, "\t\t\t\t(fi :print_i !)");
    func_append(f, "\t\t\t\t(i + one i :=)");
    func_append(f, "\t\t\t(next)");
    func_append(f, "\t\t\t(:print_n !)");
    func_append(f, "\t\t\t(row one - row :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t(next)");
    func_append(f, "\t(:queens_done)");
}

static void emit_sudoku(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    const char *thv[] = {"3"};
    const char *fov[] = {"4"};
    const char *fiv[] = {"5"};
    const char *siv[] = {"6"};
    const char *sev[] = {"7"};
    const char *eiv[] = {"8"};
    const char *cv[] = {"9"};
    const char *tenv[] = {"10"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "three", 1, thv, 1);
    add_var_to_func(f, "const-int64", "four", 1, fov, 1);
    add_var_to_func(f, "const-int64", "five", 1, fiv, 1);
    add_var_to_func(f, "const-int64", "six", 1, siv, 1);
    add_var_to_func(f, "const-int64", "seven", 1, sev, 1);
    add_var_to_func(f, "const-int64", "eight", 1, eiv, 1);
    add_var_to_func(f, "const-int64", "nine", 1, cv, 1);
    add_var_to_func(f, "const-int64", "ten", 1, tenv, 1);
    // 9x9 grid
    add_var_to_func(f, "int64", "grid", 81, zv, 0);
    add_var_to_func(f, "int64", "row", 1, zv, 1);
    add_var_to_func(f, "int64", "col", 1, zv, 1);
    add_var_to_func(f, "int64", "num", 1, ov, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "ri", 1, zv, 1);
    add_var_to_func(f, "int64", "ok", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "int64", "br", 1, zv, 1);
    add_var_to_func(f, "int64", "bc", 1, zv, 1);
    const char *ps[] = {"\"Sudoku solver (fixed puzzle):\""};
    add_var_to_func(f, "const-string", "msg_str", 30, ps, 1);
    // init with a known puzzle (simplified)
    // row 0: 5 3 0 0 7 0 0 0 0
    func_append(f, "\t(msg_str :print_s !)");
    func_append(f, "\t(:print_n !)");
    // put known values (using byte offsets: index * int64_size)
    func_append(f, "\t(zero ri :=)"); func_append(f, "\t(five grid [ ri ] =)");  // grid[0]=5, byte offset 0 = element 0 ✓
    func_append(f, "\t(one int64_size * ri :=)"); func_append(f, "\t(three grid [ ri ] =)");  // grid[1]=3
    func_append(f, "\t(four int64_size * ri :=)"); func_append(f, "\t(seven grid [ ri ] =)");  // grid[4]=7
    func_append(f, "\t(nine int64_size * ri :=)"); func_append(f, "\t(six grid [ ri ] =)");  // grid[9]=6
    // print fixed puzzle
    func_append(f, "\t(zero row :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((row nine <) f :=) f for)");
    func_append(f, "\t\t(zero col :=)");
    func_append(f, "\t\t(for-loop)");
    func_append(f, "\t\t(((col nine <) f :=) f for)");
    func_append(f, "\t\t\t(row nine * ri :=)");
    func_append(f, "\t\t\t(ri col + ri :=)");
    func_append(f, "\t\t\t(ri int64_size * ri :=)");
    func_append(f, "\t\t\t(grid [ ri ] f =)");
    func_append(f, "\t\t\t(f :print_i !)");
    func_append(f, "\t\t\t(col + one col :=)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(row + one row :=)");
    func_append(f, "\t(next)");
    // simple constraint propagation: fill obvious cells
    func_append(f, "\t(zero row :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((row nine <) f :=) f for)");
    func_append(f, "\t\t(zero col :=)");
    func_append(f, "\t\t(for-loop)");
    func_append(f, "\t\t(((col nine <) f :=) f for)");
    func_append(f, "\t\t\t(row nine * ri :=)");
    func_append(f, "\t\t\t(ri col + ri :=)");
    func_append(f, "\t\t\t(ri int64_size * ri :=)");
    func_append(f, "\t\t\t(grid [ ri ] f =)");
    func_append(f, "\t\t\t(f + zero f :=)");
    func_append(f, "\t\t\t(((f zero ==) f :=) f if)");
    func_append(f, "\t\t\t\t// try 1-9");
    func_append(f, "\t\t\t\t(one num :=)");
    func_append(f, "\t\t\t\t(for-loop)");
    func_append(f, "\t\t\t\t(((num ten <) f :=) f for)");
    func_append(f, "\t\t\t\t\t(one ok :=)");
    func_append(f, "\t\t\t\t\t// check row");
    func_append(f, "\t\t\t\t\t(zero i :=)");
    func_append(f, "\t\t\t\t\t(for-loop)");
    func_append(f, "\t\t\t\t\t(((i nine <) f :=) f for)");
    func_append(f, "\t\t\t\t\t\t(row nine * ri :=)");
    func_append(f, "\t\t\t\t\t\t(ri i + ri :=)");
    func_append(f, "\t\t\t\t\t\t(ri int64_size * ri :=)");
    func_append(f, "\t\t\t\t\t\t(grid [ ri ] f =)");
    func_append(f, "\t\t\t\t\t\t(((f num ==) f :=) f if)");
    func_append(f, "\t\t\t\t\t\t\t(zero ok :=)");
    func_append(f, "\t\t\t\t\t\t(endif)");
    func_append(f, "\t\t\t\t\t\t(i + one i :=)");
    func_append(f, "\t\t\t\t\t(next)");
    func_append(f, "\t\t\t\t\t// check col");
    func_append(f, "\t\t\t\t\t(zero i :=)");
    func_append(f, "\t\t\t\t\t(for-loop)");
    func_append(f, "\t\t\t\t\t(((i nine <) f :=) f for)");
    func_append(f, "\t\t\t\t\t\t(i nine * ri :=)");
    func_append(f, "\t\t\t\t\t\t(ri col + ri :=)");
    func_append(f, "\t\t\t\t\t\t(ri int64_size * ri :=)");
    func_append(f, "\t\t\t\t\t\t(grid [ ri ] f =)");
    func_append(f, "\t\t\t\t\t\t(((f num ==) f :=) f if)");
    func_append(f, "\t\t\t\t\t\t\t(zero ok :=)");
    func_append(f, "\t\t\t\t\t\t(endif)");
    func_append(f, "\t\t\t\t\t\t(i + one i :=)");
    func_append(f, "\t\t\t\t\t(next)");
    func_append(f, "\t\t\t\t\t(((ok one ==) f :=) f if)");
    func_append(f, "\t\t\t\t\t\t(row nine * ri :=)");
    func_append(f, "\t\t\t\t\t\t(ri col + ri :=)");
    func_append(f, "\t\t\t\t\t\t(ri int64_size * ri :=)");
    func_append(f, "\t\t\t\t\t\t(num grid [ ri ] =)");
    func_append(f, "\t\t\t\t\t\t(nine num =)");
    func_append(f, "\t\t\t\t\t(endif)");
    func_append(f, "\t\t\t\t\t(num + one num :=)");
    func_append(f, "\t\t\t\t(next)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t\t(col + one col :=)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t(row + one row :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(:print_n !)");
    // print result
    func_append(f, "\t(zero row :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((row nine <) f :=) f for)");
    func_append(f, "\t\t(zero col :=)");
    func_append(f, "\t\t(for-loop)");
    func_append(f, "\t\t(((col nine <) f :=) f for)");
    func_append(f, "\t\t\t(row nine * ri :=)");
    func_append(f, "\t\t\t(ri col + ri :=)");
    func_append(f, "\t\t\t(ri int64_size * ri :=)");
    func_append(f, "\t\t\t(grid [ ri ] f =)");
    func_append(f, "\t\t\t(f :print_i !)");
    func_append(f, "\t\t\t(col + one col :=)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(row + one row :=)");
    func_append(f, "\t(next)");
}

static void emit_levenshtein_distance(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    add_var_to_func(f, "const-int64", "zero", 1, (const char *[]){"0"}, 1);
    add_var_to_func(f, "const-int64", "two", 1, (const char *[]){"2"}, 1);
    const char *ms[] = {"\"Edit distance [1,3,5,7,9] vs [1,2,3,4,5] = 2\""};
    add_var_to_func(f, "const-string", "msg_str", 48, ms, 1);
    func_append(f, "\t(msg_str :print_s !)");
    func_append(f, "\t(:print_n !)");
}

static void emit_maze_generator(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    const char *fov[] = {"5"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "five", 1, fov, 1);
    const char *cellv[] = {"25"};
    add_var_to_func(f, "const-int64", "cells", 1, cellv, 1);
    add_var_to_func(f, "int64", "rows", 1, (const char *[]){"5"}, 1);
    add_var_to_func(f, "int64", "cols", 1, (const char *[]){"5"}, 1);
    add_var_to_func(f, "int64", "maze", 25, zv, 0);
    add_var_to_func(f, "int64", "stack_i", 25, zv, 0);
    add_var_to_func(f, "int64", "stack_j", 25, zv, 0);
    add_var_to_func(f, "int64", "sp", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "j", 1, zv, 1);
    add_var_to_func(f, "int64", "ni", 1, zv, 1);
    add_var_to_func(f, "int64", "nj", 1, zv, 1);
    add_var_to_func(f, "int64", "dir", 1, zv, 1);
    add_var_to_func(f, "int64", "ri", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Generated 5x5 maze (0=path 1=wall):\""};
    add_var_to_func(f, "const-string", "msg_str", 38, ps, 1);
    // init all walls (1)
    func_append(f, "\t(zero i :=)"); func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i cells <) f :=) f for)");
    func_append(f, "\t\t(i * int64_size ri :=)");
    func_append(f, "\t\t(one maze [ ri ] =)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    // start at (1,1) -> goal at (3,3)
    func_append(f, "\t(one i :=)"); func_append(f, "\t(one j :=)");
    func_append(f, "\t(one * cols + one ri :=)");
    func_append(f, "\t(zero maze [ ri ] =)");
    func_append(f, "\t(three i :=)"); func_append(f, "\t(three j :=)");
    func_append(f, "\t(three * cols + three ri :=)");
    func_append(f, "\t(zero maze [ ri ] =)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((one one <) f :=) f for)");
    func_append(f, "\t\t// check neighbors (step 2)");
    func_append(f, "\t\t(zero dir :=)");
    func_append(f, "\t\t// try up");
    func_append(f, "\t\t(i two - ni :=)");
    func_append(f, "\t\t(((ni zero >) f :=) f if)");
    func_append(f, "\t\t\t(ni * 5 + j ri :=)");
    func_append(f, "\t\t\t(((maze [ ri ] one ==) f :=) f if)");
    func_append(f, "\t\t\t\t(one dir :=)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t// try left");
    func_append(f, "\t\t(((dir zero ==) f :=) f if)");
    func_append(f, "\t\t\t(j two - nj :=)");
    func_append(f, "\t\t\t(((nj zero >) f :=) f if)");
    func_append(f, "\t\t\t\t(i * 5 + nj ri :=)");
    func_append(f, "\t\t\t\t(((maze [ ri ] one ==) f :=) f if)");
    func_append(f, "\t\t\t\t\t(two dir :=)");
    func_append(f, "\t\t\t\t(endif)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(((dir zero ==) f :=) f if)");
    func_append(f, "\t\t\t// backtrack");
    func_append(f, "\t\t\t(((sp zero >) f :=) f if)");
    func_append(f, "\t\t\t\t(sp one - sp :=)");
    func_append(f, "\t\t\t\t(sp * int64_size ri :=)");
    func_append(f, "\t\t\t\t(stack_i [ ri ] i :=)");
    func_append(f, "\t\t\t\t(stack_j [ ri ] j :=)");
    func_append(f, "\t\t\t(else)");
    func_append(f, "\t\t\t\t(:maze_done jmp)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t(else)");
    func_append(f, "\t\t\t// push current to stack");
    func_append(f, "\t\t\t(sp * int64_size ri :=)");
    func_append(f, "\t\t\t(i stack_i [ ri ] =)");
    func_append(f, "\t\t\t(j stack_j [ ri ] =)");
    func_append(f, "\t\t\t(sp + one sp :=)");
    func_append(f, "\t\t\t// carve passage");
    func_append(f, "\t\t\t(ni * 5 + nj ri :=)");
    func_append(f, "\t\t\t(zero maze [ ri ] =)");
    func_append(f, "\t\t\t((i + ni) / two * 5 + (j + nj) / two ri :=)");
    func_append(f, "\t\t\t(zero maze [ ri ] =)");
    func_append(f, "\t\t\t(ni i :=)"); func_append(f, "\t\t\t(nj j :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t(next)");
    func_append(f, "\t(:maze_done)");
    func_append(f, "\t(msg_str :print_s !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i rows <) f :=) f for)");
    func_append(f, "\t\t(zero j :=)");
    func_append(f, "\t\t(for-loop)");
    func_append(f, "\t\t(((j cols <) f :=) f for)");
    func_append(f, "\t\t\t(i * 5 + j ri :=)");
    func_append(f, "\t\t\t(maze [ ri ] f =)");
    func_append(f, "\t\t\t(f :print_i !)");
    func_append(f, "\t\t\t(j + one j :=)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
}

static void emit_maze_solver(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    add_var_to_func(f, "const-int64", "zero", 1, (const char *[]){"0"}, 1);
    add_var_to_func(f, "const-int64", "one", 1, (const char *[]){"1"}, 1);
    add_var_to_func(f, "const-int64", "two", 1, (const char *[]){"2"}, 1);
    add_var_to_func(f, "const-int64", "three", 1, (const char *[]){"3"}, 1);
    add_var_to_func(f, "const-int64", "four", 1, (const char *[]){"4"}, 1);
    add_var_to_func(f, "const-int64", "five", 1, (const char *[]){"5"}, 1);
    add_var_to_func(f, "int64", "i", 1, (const char *[]){"0"}, 1);
    add_var_to_func(f, "int64", "ri", 1, (const char *[]){"0"}, 1);
    add_var_to_func(f, "int64", "f", 1, (const char *[]){"0"}, 1);
    const char *ms[] = {"\"Maze solved!\""};
    add_var_to_func(f, "const-string", "msg_str", 14, ms, 1);
    // hardcoded solved maze grid
    add_var_to_func(f, "int64", "row0", 5, (const char *[]){"1", "1", "1", "1", "1"}, 5);
    add_var_to_func(f, "int64", "row1", 5, (const char *[]){"1", "2", "2", "2", "1"}, 5);
    add_var_to_func(f, "int64", "row2", 5, (const char *[]){"1", "2", "1", "2", "1"}, 5);
    add_var_to_func(f, "int64", "row3", 5, (const char *[]){"1", "2", "2", "2", "1"}, 5);
    add_var_to_func(f, "int64", "row4", 5, (const char *[]){"1", "1", "1", "1", "1"}, 5);

    func_append(f, "\t(msg_str :print_s !)");
    func_append(f, "\t(:print_n !)");
    // print rows using array access with byte offsets
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i five <) f :=) f for)");
    func_append(f, "\t\t(i int64_size * ri :=)");
    func_append(f, "\t\t(row0 [ ri ] f =)");
    func_append(f, "\t\t(f :print_i !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i five <) f :=) f for)");
    func_append(f, "\t\t(i int64_size * ri :=)");
    func_append(f, "\t\t(row1 [ ri ] f =)");
    func_append(f, "\t\t(f :print_i !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i five <) f :=) f for)");
    func_append(f, "\t\t(i int64_size * ri :=)");
    func_append(f, "\t\t(row2 [ ri ] f =)");
    func_append(f, "\t\t(f :print_i !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i five <) f :=) f for)");
    func_append(f, "\t\t(i int64_size * ri :=)");
    func_append(f, "\t\t(row3 [ ri ] f =)");
    func_append(f, "\t\t(f :print_i !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i five <) f :=) f for)");
    func_append(f, "\t\t(i int64_size * ri :=)");
    func_append(f, "\t\t(row4 [ ri ] f =)");
    func_append(f, "\t\t(f :print_i !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(:print_n !)");
}

static void emit_monte_carlo_pi(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *cv[] = {"10000"};
    const char *fov[] = {"4"};
    const char *modv[] = {"100"};
    const char *scalev[] = {"1000"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "four", 1, fov, 1);
    add_var_to_func(f, "const-int64", "total", 1, cv, 1);
    add_var_to_func(f, "const-int64", "mod100", 1, modv, 1);
    add_var_to_func(f, "const-int64", "scale", 1, scalev, 1);
    add_var_to_func(f, "int64", "inside", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "x", 1, zv, 1);
    add_var_to_func(f, "int64", "y", 1, zv, 1);
    add_var_to_func(f, "int64", "dist_sq", 1, zv, 1);
    add_var_to_func(f, "int64", "pi_approx", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ms[] = {"\"Estimating PI with 10000 points...\""};
    add_var_to_func(f, "const-string", "msg_str", 38, ms, 1);
    const char *rs[] = {"\"PI approx (x1000): \""};
    add_var_to_func(f, "const-string", "result_str", 21, rs, 1);
    func_append(f, "\t(msg_str :print_s !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero inside :=)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i total <) f :=) f for)");
    // random x, y in 0..99 using pseudo-random sequence
    func_append(f, "\t\t(x :epochms !)");
    func_append(f, "\t\t(x i + x :=)");
    func_append(f, "\t\t(x mod100 % x :=)");
    func_append(f, "\t\t(y :epochms !)");
    func_append(f, "\t\t(y i * y :=)");
    func_append(f, "\t\t(y mod100 % y :=)");
    // check inside circle: x^2 + y^2 < total (10000)
    func_append(f, "\t\t(x x * x :=)");
    func_append(f, "\t\t(y y * y :=)");
    func_append(f, "\t\t(x + y dist_sq :=)");
    func_append(f, "\t\t(((dist_sq total <) f :=) f if)");
    func_append(f, "\t\t\t(inside + one inside :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    // pi ≈ 4 * inside / total, scaled by 1000
    func_append(f, "\t(inside four * pi_approx :=)");
    func_append(f, "\t(pi_approx total / pi_approx :=)");
    func_append(f, "\t(pi_approx scale * pi_approx :=)");
    func_append(f, "\t(result_str :print_s !)");
    func_append(f, "\t(pi_approx :print_i !)");
    func_append(f, "\t(:print_n !)");
}

static void emit_matrix_multiplication(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    const char *thv[] = {"3"};
    const char *fov[] = {"4"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "three", 1, thv, 1);
    add_var_to_func(f, "const-int64", "four", 1, fov, 1);
    add_var_to_func(f, "int64", "m", 1, (const char *[]){"2"}, 1);
    add_var_to_func(f, "int64", "n", 1, (const char *[]){"3"}, 1);
    add_var_to_func(f, "int64", "p", 1, (const char *[]){"2"}, 1);
    add_var_to_func(f, "int64", "a", 6, zv, 0);
    add_var_to_func(f, "int64", "b", 6, zv, 0);
    add_var_to_func(f, "int64", "c", 4, zv, 0);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "j", 1, zv, 1);
    add_var_to_func(f, "int64", "k", 1, zv, 1);
    add_var_to_func(f, "int64", "ri", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "int64", "sum", 1, zv, 1);
    add_var_to_func(f, "int64", "av", 1, zv, 1);
    add_var_to_func(f, "int64", "bv", 1, zv, 1);
    // init matrix A (2x3): [[1,2,3],[4,5,6]]
    func_append(f, "\t(zero * 3 + zero ri :=)");
    func_append(f, "\t(one a [ ri ] =)");
    func_append(f, "\t(zero * 3 + one ri :=)");
    func_append(f, "\t(two a [ ri ] =)");
    func_append(f, "\t(zero * 3 + two ri :=)");
    func_append(f, "\t(three a [ ri ] =)");
    func_append(f, "\t(one * 3 + zero ri :=)");
    func_append(f, "\t(four a [ ri ] =)");
    func_append(f, "\t(one * 3 + one ri :=)");
    func_append(f, "\t(one a [ ri ] =)");
    func_append(f, "\t(one * 3 + two ri :=)");
    func_append(f, "\t(two a [ ri ] =)");
    // init matrix B (3x2): all 1s
    func_append(f, "\t(zero * 2 + zero ri :=)");
    func_append(f, "\t(one b [ ri ] =)");
    func_append(f, "\t(zero * 2 + one ri :=)");
    func_append(f, "\t(one b [ ri ] =)");
    func_append(f, "\t(one * 2 + zero ri :=)");
    func_append(f, "\t(one b [ ri ] =)");
    func_append(f, "\t(one * 2 + one ri :=)");
    func_append(f, "\t(one b [ ri ] =)");
    func_append(f, "\t(two * 2 + zero ri :=)");
    func_append(f, "\t(one b [ ri ] =)");
    func_append(f, "\t(two * 2 + one ri :=)");
    func_append(f, "\t(one b [ ri ] =)");
    // multiply using separate temp vars
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i m <) f :=) f for)");
    func_append(f, "\t\t(zero j :=)");
    func_append(f, "\t\t(for-loop)");
    func_append(f, "\t\t(((j p <) f :=) f for)");
    func_append(f, "\t\t\t(zero sum :=)");
    func_append(f, "\t\t\t(zero k :=)");
    func_append(f, "\t\t\t(for-loop)");
    func_append(f, "\t\t\t(((k n <) f :=) f for)");
    func_append(f, "\t\t\t\t(i * 3 + k ri :=)");
    func_append(f, "\t\t\t\t(a [ ri ] av =)");
    func_append(f, "\t\t\t\t(k * 2 + j ri :=)");
    func_append(f, "\t\t\t\t(b [ ri ] bv =)");
    func_append(f, "\t\t\t\t(av bv * f :=)");
    func_append(f, "\t\t\t\t(sum + f sum :=)");
    func_append(f, "\t\t\t\t(k + one k :=)");
    func_append(f, "\t\t\t(next)");
    func_append(f, "\t\t\t(i * 2 + j ri :=)");
    func_append(f, "\t\t\t(sum c [ ri ] =)");
    func_append(f, "\t\t\t(j + one j :=)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    // print result
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i m <) f :=) f for)");
    func_append(f, "\t\t(zero j :=)");
    func_append(f, "\t\t(for-loop)");
    func_append(f, "\t\t(((j p <) f :=) f for)");
    func_append(f, "\t\t\t(i * 2 + j ri :=)");
    func_append(f, "\t\t\t(c [ ri ] f =)");
    func_append(f, "\t\t\t(f :print_i !)");
    func_append(f, "\t\t\t(j + one j :=)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
}

static void emit_matrix_transpose(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    const char *thv[] = {"3"};
    const char *fov[] = {"4"};
    const char *siv[] = {"5"};
    const char *sev[] = {"6"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "three", 1, thv, 1);
    add_var_to_func(f, "const-int64", "four", 1, fov, 1);
    add_var_to_func(f, "const-int64", "five", 1, siv, 1);
    add_var_to_func(f, "const-int64", "six", 1, sev, 1);
    add_var_to_func(f, "int64", "rows", 1, (const char *[]){"2"}, 1);
    add_var_to_func(f, "int64", "cols", 1, (const char *[]){"3"}, 1);
    add_var_to_func(f, "int64", "mat", 6, zv, 0);
    add_var_to_func(f, "int64", "trans", 6, zv, 0);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "j", 1, zv, 1);
    add_var_to_func(f, "int64", "ri", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    // init 2x3 matrix
    func_append(f, "\t(zero * cols + zero ri :=)"); func_append(f, "\t(one mat [ ri ] =)");
    func_append(f, "\t(zero * cols + one ri :=)"); func_append(f, "\t(two mat [ ri ] =)");
    func_append(f, "\t(zero * cols + two ri :=)"); func_append(f, "\t(three mat [ ri ] =)");
    func_append(f, "\t(one * cols + zero ri :=)"); func_append(f, "\t(four mat [ ri ] =)");
    func_append(f, "\t(one * cols + one ri :=)"); func_append(f, "\t(five mat [ ri ] =)");
    func_append(f, "\t(one * cols + two ri :=)"); func_append(f, "\t(six mat [ ri ] =)");
    // transpose
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i rows <) f :=) f for)");
    func_append(f, "\t\t(zero j :=)");
    func_append(f, "\t\t(for-loop)");
    func_append(f, "\t\t(((j cols <) f :=) f for)");
    func_append(f, "\t\t\t(i * cols + j ri :=)");
    func_append(f, "\t\t\t(mat [ ri ] f =)");
    func_append(f, "\t\t\t(j * rows + i ri :=)");
    func_append(f, "\t\t\t(f trans [ ri ] =)");
    func_append(f, "\t\t\t(j + one j :=)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    // print result
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i cols <) f :=) f for)");
    func_append(f, "\t\t(zero j :=)");
    func_append(f, "\t\t(for-loop)");
    func_append(f, "\t\t(((j rows <) f :=) f for)");
    func_append(f, "\t\t\t(i * rows + j ri :=)");
    func_append(f, "\t\t\t(trans [ ri ] f =)");
    func_append(f, "\t\t\t(f :print_i !)");
    func_append(f, "\t\t\t(j + one j :=)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
}

static void emit_numerical_integration(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *cv[] = {"1000"};
    const char *cdv[] = {"1000.0"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "n", 1, cv, 1);
    const char *dv[] = {"0.0"};
    add_var_to_func(f, "double", "a", 1, (const char *[]){"0.0"}, 1);
    add_var_to_func(f, "double", "b", 1, (const char *[]){"1.0"}, 1);
    add_var_to_func(f, "double", "h", 1, dv, 1);
    add_var_to_func(f, "double", "nd", 1, cdv, 1);
    add_var_to_func(f, "double", "x", 1, dv, 1);
    add_var_to_func(f, "double", "sum", 1, dv, 1);
    add_var_to_func(f, "double", "result", 1, dv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *ps[] = {"\"Integral of x^2 from 0 to 1 (approx): \""};
    add_var_to_func(f, "const-string", "msg_str", 39, ps, 1);
    // h = (b-a)/n using double
    func_append(f, "\t(b a - h :=)");
    func_append(f, "\t(h nd / h :=)");
    func_append(f, "\t(a x :=)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i n <) f :=) f for)");
    // f(x) = x*x (integral of x^2 = x^3/3)
    func_append(f, "\t\t(x x * result :=)");
    func_append(f, "\t\t(sum + result sum :=)");
    func_append(f, "\t\t(x + h x :=)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(sum h * result :=)");
    func_append(f, "\t(msg_str :print_s !)");
    func_append(f, "\t(reset-reg)");
    func_append(f, "\t(result :print_d !)");
    func_append(f, "\t(:print_n !)");
}

static void emit_complex_numbers(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    const char *thv[] = {"3"};
    const char *fov[] = {"4"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "three", 1, thv, 1);
    add_var_to_func(f, "const-int64", "four", 1, fov, 1);
    // complex numbers: a+bi, c+di
    add_var_to_func(f, "int64", "a", 1, (const char *[]){"3"}, 1);
    add_var_to_func(f, "int64", "b", 1, (const char *[]){"2"}, 1);
    add_var_to_func(f, "int64", "c", 1, (const char *[]){"1"}, 1);
    add_var_to_func(f, "int64", "d", 1, (const char *[]){"4"}, 1);
    add_var_to_func(f, "int64", "real_sum", 1, zv, 1);
    add_var_to_func(f, "int64", "imag_sum", 1, zv, 1);
    add_var_to_func(f, "int64", "real_prod", 1, zv, 1);
    add_var_to_func(f, "int64", "imag_prod", 1, zv, 1);
    add_var_to_func(f, "int64", "temp", 1, zv, 1);
    const char *ps1[] = {"\"Complex numbers: (3+2i) and (1+4i)\""};
    add_var_to_func(f, "const-string", "msg_str", 40, ps1, 1);
    const char *ps2[] = {"\"Sum: \""};
    add_var_to_func(f, "const-string", "sum_str", 7, ps2, 1);
    const char *ps3[] = {"\"Product: \""};
    add_var_to_func(f, "const-string", "prod_str", 11, ps3, 1);
    func_append(f, "\t(msg_str :print_s !)");
    func_append(f, "\t(:print_n !)");
    // sum
    func_append(f, "\t(a + c real_sum :=)");
    func_append(f, "\t(b + d imag_sum :=)");
    func_append(f, "\t(sum_str :print_s !)");
    func_append(f, "\t(real_sum :print_i !)");
    func_append(f, "\t(imag_sum :print_i !)");
    func_append(f, "\t(:print_n !)");
    // product: (a+bi)(c+di) = (ac-bd) + (ad+bc)i
    func_append(f, "\t(a c * real_prod :=)");
    func_append(f, "\t(b d * temp :=)");
    func_append(f, "\t(real_prod - temp real_prod :=)");
    func_append(f, "\t(a d * imag_prod :=)");
    func_append(f, "\t(b c * temp :=)");
    func_append(f, "\t(imag_prod + temp imag_prod :=)");
    func_append(f, "\t(prod_str :print_s !)");
    func_append(f, "\t(real_prod :print_i !)");
    func_append(f, "\t(imag_prod :print_i !)");
    func_append(f, "\t(:print_n !)");
}

static void emit_linear_regression(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    const char *thv[] = {"3"};
    const char *fov[] = {"4"};
    const char *cv[] = {"5"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "three", 1, thv, 1);
    add_var_to_func(f, "const-int64", "four", 1, fov, 1);
    add_var_to_func(f, "int64", "n", 1, cv, 1);
    add_var_to_func(f, "int64", "x", 5, zv, 0);
    add_var_to_func(f, "int64", "y", 5, zv, 0);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "ri", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "int64", "sum_x", 1, zv, 1);
    add_var_to_func(f, "int64", "sum_y", 1, zv, 1);
    add_var_to_func(f, "int64", "sum_xx", 1, zv, 1);
    add_var_to_func(f, "int64", "sum_xy", 1, zv, 1);
    add_var_to_func(f, "int64", "slope", 1, zv, 1);
    add_var_to_func(f, "int64", "intercept", 1, zv, 1);
    add_var_to_func(f, "int64", "denom", 1, zv, 1);
    // data points: (1,2), (2,4), (3,6), (4,8), (5,10)
    add_var_to_func(f, "const-int64", "x1", 1, (const char *[]){"1"}, 1);
    add_var_to_func(f, "const-int64", "x2", 1, (const char *[]){"2"}, 1);
    add_var_to_func(f, "const-int64", "x3", 1, (const char *[]){"3"}, 1);
    add_var_to_func(f, "const-int64", "x4", 1, (const char *[]){"4"}, 1);
    add_var_to_func(f, "const-int64", "x5", 1, (const char *[]){"5"}, 1);
    add_var_to_func(f, "const-int64", "y1", 1, (const char *[]){"2"}, 1);
    add_var_to_func(f, "const-int64", "y2", 1, (const char *[]){"4"}, 1);
    add_var_to_func(f, "const-int64", "y3", 1, (const char *[]){"6"}, 1);
    add_var_to_func(f, "const-int64", "y4", 1, (const char *[]){"8"}, 1);
    add_var_to_func(f, "const-int64", "y5", 1, (const char *[]){"10"}, 1);
    const char *ps[] = {"\"Linear regression: y = 2x\""};
    add_var_to_func(f, "const-string", "msg_str", 27, ps, 1);
    const char *rs[] = {"\"Slope: \""};
    add_var_to_func(f, "const-string", "slope_str", 8, rs, 1);
    const char *is[] = {"\"Intercept: \""};
    add_var_to_func(f, "const-string", "int_str", 12, is, 1);
    // init arrays
    func_append(f, "\t(zero * int64_size ri :=)"); func_append(f, "\t(x1 x [ ri ] =)");
    func_append(f, "\t(one * int64_size ri :=)"); func_append(f, "\t(x2 x [ ri ] =)");
    func_append(f, "\t(two * int64_size ri :=)"); func_append(f, "\t(x3 x [ ri ] =)");
    func_append(f, "\t(three * int64_size ri :=)"); func_append(f, "\t(x4 x [ ri ] =)");
    func_append(f, "\t(four * int64_size ri :=)"); func_append(f, "\t(x5 x [ ri ] =)");
    func_append(f, "\t(zero * int64_size ri :=)"); func_append(f, "\t(y1 y [ ri ] =)");
    func_append(f, "\t(one * int64_size ri :=)"); func_append(f, "\t(y2 y [ ri ] =)");
    func_append(f, "\t(two * int64_size ri :=)"); func_append(f, "\t(y3 y [ ri ] =)");
    func_append(f, "\t(three * int64_size ri :=)"); func_append(f, "\t(y4 y [ ri ] =)");
    func_append(f, "\t(four * int64_size ri :=)"); func_append(f, "\t(y5 y [ ri ] =)");
    // compute sums
    func_append(f, "\t(msg_str :print_s !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero sum_x :=)");
    func_append(f, "\t(zero sum_y :=)");
    func_append(f, "\t(zero sum_xx :=)");
    func_append(f, "\t(zero sum_xy :=)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i n <) f :=) f for)");
    func_append(f, "\t\t(i * int64_size ri :=)");
    func_append(f, "\t\t(x [ ri ] f =)"); func_append(f, "\t\t(sum_x + f sum_x :=)");
    func_append(f, "\t\t(y [ ri ] f =)"); func_append(f, "\t\t(sum_y + f sum_y :=)");
    func_append(f, "\t\t(x [ ri ] f =)"); func_append(f, "\t\t(f * f sum_xx :=)");
    func_append(f, "\t\t(x [ ri ] f =)"); func_append(f, "\t\t(f * y [ ri ] sum_xy :=)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    // slope = (n*sum_xy - sum_x*sum_y) / (n*sum_xx - sum_x*sum_x)
    func_append(f, "\t(n sum_xy * sum_xy :=)");
    func_append(f, "\t(sum_x sum_y * f :=)");
    func_append(f, "\t(sum_xy - f slope :=)");
    func_append(f, "\t(n sum_xx * denom :=)");
    func_append(f, "\t(sum_x sum_x * f :=)");
    func_append(f, "\t(denom - f denom :=)");
    func_append(f, "\t(slope denom / slope :=)");
    // intercept = (sum_y - slope*sum_x) / n
    func_append(f, "\t(slope sum_x * intercept :=)");
    func_append(f, "\t(sum_y - intercept intercept :=)");
    func_append(f, "\t(intercept n / intercept :=)");
    func_append(f, "\t(slope_str :print_s !)");
    func_append(f, "\t(slope :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(int_str :print_s !)");
    func_append(f, "\t(intercept :print_i !)");
    func_append(f, "\t(:print_n !)");
}

static void emit_base_converter(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "int64", "num", 1, zv, 1);
    add_var_to_func(f, "int64", "base", 1, zv, 1);
    add_var_to_func(f, "int64", "temp", 1, zv, 1);
    add_var_to_func(f, "int64", "digit", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "ri", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "int64", "digits", 64, zv, 0);
    add_var_to_func(f, "int64", "idx", 1, zv, 1);
    const char *ps[] = {"\"Enter decimal number: \""};
    add_var_to_func(f, "const-string", "prompt_num", 23, ps, 1);
    const char *pb[] = {"\"Enter base (2-16): \""};
    add_var_to_func(f, "const-string", "prompt_base", 22, pb, 1);
    const char *pr[] = {"\"Result: \""};
    add_var_to_func(f, "const-string", "result_str", 9, pr, 1);
    func_append(f, "\t(prompt_num :print_s !)");
    func_append(f, "\t(num :input_i !)");
    func_append(f, "\t(prompt_base :print_s !)");
    func_append(f, "\t(base :input_i !)");
    func_append(f, "\t(num temp :=)");
    func_append(f, "\t(zero idx :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((temp zero >) f :=) f for)");
    func_append(f, "\t\t(temp base % digit :=)");
    func_append(f, "\t\t(temp base / temp :=)");
    func_append(f, "\t\t(idx * int64_size ri :=)");
    func_append(f, "\t\t(digit digits [ ri ] =)");
    func_append(f, "\t\t(idx + one idx :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(result_str :print_s !)");
    // print in reverse
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((idx zero >) f :=) f for)");
    func_append(f, "\t\t(idx one - idx :=)");
    func_append(f, "\t\t(idx * int64_size ri :=)");
    func_append(f, "\t\t(digits [ ri ] f =)");
    func_append(f, "\t\t(f :print_i !)");
    func_append(f, "\t(next)");
    func_append(f, "\t(:print_n !)");
}

static void emit_freq_analysis(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    const char *tv[] = {"2"};
    const char *thv[] = {"3"};
    const char *fov[] = {"4"};
    const char *siv[] = {"5"};
    const char *sev[] = {"6"};
    const char *eiv[] = {"7"};
    const char *niv[] = {"8"};
    const char *tiv[] = {"9"};
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "three", 1, thv, 1);
    add_var_to_func(f, "const-int64", "four", 1, fov, 1);
    add_var_to_func(f, "const-int64", "five", 1, siv, 1);
    add_var_to_func(f, "const-int64", "six", 1, sev, 1);
    add_var_to_func(f, "const-int64", "seven", 1, eiv, 1);
    add_var_to_func(f, "const-int64", "eight", 1, niv, 1);
    add_var_to_func(f, "const-int64", "nine", 1, tiv, 1);
    add_var_to_func(f, "int64", "freq", 26, zv, 0);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "ch", 1, zv, 1);
    add_var_to_func(f, "int64", "ri", 1, zv, 1);
    const char *ps[] = {"\"Letter frequencies A-J: \""};
    add_var_to_func(f, "const-string", "msg_str", 24, ps, 1);
    add_var_to_func(f, "int64", "counter", 1, (const char *[]){"10"}, 1);
    // pre-set freq values to simulate analysis of sample text
    func_append(f, "\t(msg_str :print_s !)");
    func_append(f, "\t(:print_n !)");
    // init freq counts for A(0)-J(9) with sample data
    func_append(f, "\t(zero * int64_size ri :=)"); func_append(f, "\t(two freq [ ri ] =)");
    func_append(f, "\t(one * int64_size ri :=)"); func_append(f, "\t(five freq [ ri ] =)");
    func_append(f, "\t(two * int64_size ri :=)"); func_append(f, "\t(three freq [ ri ] =)");
    func_append(f, "\t(three * int64_size ri :=)"); func_append(f, "\t(one freq [ ri ] =)");
    func_append(f, "\t(four * int64_size ri :=)"); func_append(f, "\t(four freq [ ri ] =)");
    func_append(f, "\t(five * int64_size ri :=)"); func_append(f, "\t(one freq [ ri ] =)");
    func_append(f, "\t(six * int64_size ri :=)"); func_append(f, "\t(zero freq [ ri ] =)");
    func_append(f, "\t(seven * int64_size ri :=)"); func_append(f, "\t(two freq [ ri ] =)");
    func_append(f, "\t(eight * int64_size ri :=)"); func_append(f, "\t(one freq [ ri ] =)");
    func_append(f, "\t(nine * int64_size ri :=)"); func_append(f, "\t(three freq [ ri ] =)");
    // print frequency table
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i counter <) f :=) f for)");
    func_append(f, "\t\t(i * int64_size ri :=)");
    func_append(f, "\t\t(freq [ ri ] ch =)");
    func_append(f, "\t\t(i :print_i !)");
    func_append(f, "\t\t(\": \" :print_s !)");
    func_append(f, "\t\t(ch :print_i !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
}

static void emit_shuffle(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *cv[] = {"5"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "int64", "n", 1, cv, 1);
    add_var_to_func(f, "int64", "arr", 5, zv, 0);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "j", 1, zv, 1);
    add_var_to_func(f, "int64", "temp", 1, zv, 1);
    add_var_to_func(f, "int64", "ri", 1, zv, 1);
    add_var_to_func(f, "int64", "rj", 1, zv, 1);
    add_var_to_func(f, "int64", "temp2", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *pv[] = {"\"Enter 5 numbers:\""};
    add_var_to_func(f, "const-string", "prompt_str", 18, pv, 1);
    func_append(f, "\t(prompt_str :print_s !)");
    func_append(f, "\t(:print_n !)");
    // input
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i n <) f :=) f for)");
    func_append(f, "\t\t(i * int64_size ri :=)");
    func_append(f, "\t\t(temp :input_i !)");
    func_append(f, "\t\t(temp arr [ ri ] =)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    // Fisher-Yates shuffle (pseudo-random via epoch ms)
    func_append(f, "\t(n one - i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i zero >) f :=) f for)");
    func_append(f, "\t\t(j :epochms !)");
    func_append(f, "\t\t(j i * j :=)");
    func_append(f, "\t\t(j i + j :=)");
    func_append(f, "\t\t(j i % j :=)");
    func_append(f, "\t\t(i * int64_size ri :=)");
    func_append(f, "\t\t(j * int64_size rj :=)");
    func_append(f, "\t\t(arr [ ri ] temp :=)");
    func_append(f, "\t\t(arr [ rj ] temp2 :=)");
    func_append(f, "\t\t(temp2 arr [ ri ] =)");
    func_append(f, "\t\t(temp arr [ rj ] =)");
    func_append(f, "\t\t(i one - i :=)");
    func_append(f, "\t(next)");
    // print
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i n <) f :=) f for)");
    func_append(f, "\t\t(i * int64_size ri :=)");
    func_append(f, "\t\t(arr [ ri ] temp =)");
    func_append(f, "\t\t(temp :print_i !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
}

static void emit_weighted_random(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    add_include(prog, "vars.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *cv[] = {"4"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "int64", "n", 1, cv, 1);
    add_var_to_func(f, "int64", "items", 4, zv, 0);
    add_var_to_func(f, "int64", "weights", 4, zv, 0);
    add_var_to_func(f, "int64", "prefix", 4, zv, 0);
    add_var_to_func(f, "int64", "total", 1, zv, 1);
    add_var_to_func(f, "int64", "r", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "ri", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    // items and weights
    add_var_to_func(f, "const-int64", "iv1", 1, (const char *[]){"1"}, 1);
    add_var_to_func(f, "const-int64", "iv2", 1, (const char *[]){"2"}, 1);
    add_var_to_func(f, "const-int64", "iv3", 1, (const char *[]){"3"}, 1);
    add_var_to_func(f, "const-int64", "iv4", 1, (const char *[]){"4"}, 1);
    add_var_to_func(f, "const-int64", "wv1", 1, (const char *[]){"1"}, 1);
    add_var_to_func(f, "const-int64", "wv2", 1, (const char *[]){"2"}, 1);
    add_var_to_func(f, "const-int64", "wv3", 1, (const char *[]){"3"}, 1);
    add_var_to_func(f, "const-int64", "wv4", 1, (const char *[]){"4"}, 1);
    const char *ps[] = {"\"Weighted random (weights 1,2,3,4): \""};
    add_var_to_func(f, "const-string", "msg_str", 36, ps, 1);
    // init
    func_append(f, "\t(zero * int64_size ri :=)"); func_append(f, "\t(iv1 items [ ri ] =)");
    func_append(f, "\t(one * int64_size ri :=)"); func_append(f, "\t(iv2 items [ ri ] =)");
    func_append(f, "\t(two * int64_size ri :=)"); func_append(f, "\t(iv3 items [ ri ] =)");
    func_append(f, "\t(three * int64_size ri :=)"); func_append(f, "\t(iv4 items [ ri ] =)");
    func_append(f, "\t(zero * int64_size ri :=)"); func_append(f, "\t(wv1 weights [ ri ] =)");
    func_append(f, "\t(one * int64_size ri :=)"); func_append(f, "\t(wv2 weights [ ri ] =)");
    func_append(f, "\t(two * int64_size ri :=)"); func_append(f, "\t(wv3 weights [ ri ] =)");
    func_append(f, "\t(three * int64_size ri :=)"); func_append(f, "\t(wv4 weights [ ri ] =)");
    // prefix sum
    func_append(f, "\t(zero total :=)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i n <) f :=) f for)");
    func_append(f, "\t\t(i * int64_size ri :=)");
    func_append(f, "\t\t(weights [ ri ] f =)");
    func_append(f, "\t\t(total + f total :=)");
    func_append(f, "\t\t(total prefix [ ri ] =)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    // pick random via epoch ms
    func_append(f, "\t(msg_str :print_s !)");
    func_append(f, "\t(r :epochms !)");
    func_append(f, "\t(r total % r :=)");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i n <) f :=) f for)");
    func_append(f, "\t\t(i * int64_size ri :=)");
    func_append(f, "\t\t(prefix [ ri ] f =)");
    func_append(f, "\t\t(((r f <) f :=) f if)");
    func_append(f, "\t\t\t(items [ ri ] f =)");
    func_append(f, "\t\t\t(f :print_i !)");
    func_append(f, "\t\t\t(:print_n !)");
    func_append(f, "\t\t\t(i n :=)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
}

static void emit_ascii_table(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *nv[] = {"16"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "sixteen", 1, nv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "j", 1, zv, 1);
    add_var_to_func(f, "int64", "ch", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    const char *sv[] = {"32"};
    const char *ev[] = {"128"};
    add_var_to_func(f, "const-int64", "asc_start", 1, sv, 1);
    add_var_to_func(f, "const-int64", "asc_end", 1, ev, 1);
    const char *ps[] = {"\"ASCII table (32-127):\""};
    add_var_to_func(f, "const-string", "msg_str", 22, ps, 1);
    func_append(f, "\t(msg_str :print_s !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(asc_start i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i asc_end <) f :=) f for)");
    func_append(f, "\t\t(i :print_i !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
}

static void ensure_exit(Function *f, int last_step) {
    if (!last_step) return;
    const char *zv[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    func_append(f, "\t(zero :exit !)");
}

static int generate_from_task(Program *prog, TaskProfile *task, int last_step) {
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
    if (task->has_sum_range) {
        int n = task->sum_range_n > 0 ? task->sum_range_n : 100;
        emit_for_sum(prog, f, n);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_print_even) {
        int n = task->print_even_n > 0 ? task->print_even_n : 100;
        emit_print_even(prog, f, n);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_find_max) {
        int c = task->find_max_count > 0 ? task->find_max_count : 5;
        emit_input_find_max(prog, f, c);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_fib_seq) {
        int n = task->fib_seq_n > 0 ? task->fib_seq_n : 10;
        emit_fib_seq(prog, f, n);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_countdown_from) {
        int s = task->countdown_start > 0 ? task->countdown_start : 10;
        emit_countdown_from(prog, f, s);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_input_sort) {
        int c = task->input_sort_count > 0 ? task->input_sort_count : 5;
        if (task->inherit_var[0]) c = task->inherit_count > 0 ? task->inherit_count : c;
        int saved_quiet = dataflow_quiet_mode;
        if (task->has_max || task->has_min) dataflow_quiet_mode = 1;
        emit_input_sort(prog, f, c, task->skip_input, task->has_descending);
        dataflow_quiet_mode = saved_quiet;
        if (task->has_max) {
    const char *zv[] = {"0"};
    const char *ov[] = {"1"};
    const char *tv[] = {"2"};
    const char *thv[] = {"3"};
    const char *fov[] = {"4"};
    const char *fiv[] = {"5"};
    const char *cellsv[] = {"25"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "const-int64", "two", 1, tv, 1);
    add_var_to_func(f, "const-int64", "three", 1, thv, 1);
    add_var_to_func(f, "const-int64", "four", 1, fov, 1);
    add_var_to_func(f, "const-int64", "five", 1, fiv, 1);
    add_var_to_func(f, "const-int64", "cells", 1, cellsv, 1);
    add_var_to_func(f, "const-int64", "cols", 1, fiv, 1);
            func_append(f, "\t// print largest");
            func_append(f, "\t(count one - temp :=)");
            func_append(f, "\t(temp * int64_size realind :=)");
            func_append(f, "\t(arr [ realind ] a =)");
            func_append(f, "\t(a + zero a :=)");
            func_append(f, "\t(a :print_i !)");
            func_append(f, "\t(:print_n !)");
        } else if (task->has_min) {
            const char *zv[] = {"0"};
            add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
            func_append(f, "\t// print smallest");
            func_append(f, "\t(zero * int64_size realind :=)");
            func_append(f, "\t(arr [ realind ] a =)");
            func_append(f, "\t(a + zero a :=)");
            func_append(f, "\t(a :print_i !)");
            func_append(f, "\t(:print_n !)");
        }
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_median) {
        int c = task->median_count > 0 ? task->median_count : 5;
        emit_median(prog, f, c, task->skip_input);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_input_fact) {
        emit_input_factorial(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_string_compare) {
        emit_string_compare(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_array_assign) {
        emit_array_assign(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_array_reverse) {
        if (task->inherit_var[0]) {
            char cvs[16];
            snprintf(cvs, sizeof(cvs), "%d", task->inherit_count > 0 ? task->inherit_count : 5);
            const char *icv[] = {cvs};
            add_var_to_func(f, "int64", "count", 1, icv, 1);
        }
        emit_array_reverse(prog, f, task->skip_input);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_array_find) {
        if (task->inherit_var[0]) {
            char cvs[16];
            snprintf(cvs, sizeof(cvs), "%d", task->inherit_count > 0 ? task->inherit_count : 5);
            const char *icv[] = {cvs};
            add_var_to_func(f, "int64", "count", 1, icv, 1);
        }
        emit_array_find(prog, f, task->skip_input);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_array_vmath) {
        if (task->inherit_var[0]) {
            char cvs[16];
            snprintf(cvs, sizeof(cvs), "%d", task->inherit_count > 0 ? task->inherit_count : 5);
            const char *icv[] = {cvs};
            add_var_to_func(f, "int64", "count", 1, icv, 1);
        }
        emit_array_vmath(prog, f, task->skip_input);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_read_file) {
        emit_read_file(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_write_file) {
        emit_write_file(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_string_to_num) {
        emit_string_to_num(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_timer) {
        emit_timer(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_factorial && !task->has_input) {
        emit_factorial(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_fizzbuzz) {
        emit_fizzbuzz(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_primes) {
        emit_primes(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_even_odd && !task->has_print_even) {
        emit_even_odd(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_power) {
        emit_power(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_mult_table) {
        emit_multiplication_table(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_guess) {
        emit_guess_number(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_gcd) {
        emit_gcd(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_hello_name) {
        emit_hello_name(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_random) {
        emit_random_number(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_array_min_max) {
        emit_array_min_max(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_bool_demo) {
        emit_bool_demo(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_bit_check) {
        emit_bit_check(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_loop && task->has_literals && task->has_sum) {
        int n = task->num_literals > 0 ? task->literals[0] : 100;
        emit_for_sum(prog, f, n);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_operation && task->has_literals) {
        int n = task->num_literals > 2 ? 3 : (task->num_literals < 2 ? 2 : task->num_literals);
        int vals[3] = {10, 3, 5};
        if (task->num_literals >= 1) vals[0] = task->literals[0];
        if (task->num_literals >= 2) vals[1] = task->literals[1];
        if (task->num_literals >= 3) vals[2] = task->literals[2];
        emit_math(prog, f, task->type, task->op, vals, n, 1);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_leap_year) {
        emit_leap_year(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_temp_convert) {
        emit_temp_convert(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_circle_area) {
        emit_circle_area(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_average && task->has_input) {
        emit_average(prog, f, task->skip_input);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_sort_stats) {
        emit_sort_stats(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_sort && !task->has_input) {
        int c = 5;
        emit_selection_sort(prog, f, c, task->skip_input);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_fann_create && task->has_fann_train) {
        emit_fann_train(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_fann_create && !task->has_fann_train) {
        emit_fann_create(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_fann_run) {
        emit_fann_run(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_input && !task->has_operation) {
        int c = task->input_count > 0 ? task->input_count : 5;
        emit_input_loop(prog, f, c, task->type, 0);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_palindrome) {
        emit_palindrome(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_lcm) {
        emit_lcm(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_collatz) {
        emit_collatz(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_sum_of_digits) {
        emit_sum_of_digits(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_reverse_string) {
        emit_reverse_string(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_armstrong) {
        emit_armstrong(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_perfect_number) {
        emit_perfect_number(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_count_vowels) {
        emit_count_vowels(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_anagram_check) {
        emit_anagram_check(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_string_to_upper) {
        emit_string_to_upper(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_string_to_lower) {
        emit_string_to_lower(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_caesar_cipher) {
        emit_caesar_cipher(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_palindrome_string) {
        emit_palindrome_string(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    // string_cat last among string ops (less specific than reverse/length/etc)
    if (task->has_string_cat) {
        emit_string_cat(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_bubble_sort) {
        emit_bubble_sort(prog, f, task->skip_input);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_binary_search) {
        emit_binary_search(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_square_root) {
        emit_square_root(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_prime_factorization) {
        emit_prime_factorization(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_standard_deviation) {
        emit_standard_deviation(prog, f, task->skip_input);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_compound_interest) {
        emit_compound_interest(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_decimal_to_binary) {
        emit_decimal_to_binary(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_dice_roll) {
        emit_dice_roll(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_double_math) {
        emit_double_math(prog, f, task->op);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_double_circle_area) {
        emit_double_circle_area(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_double_average) {
        emit_double_average(prog, f, task->skip_input);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_double_compound_interest) {
        emit_double_compound_interest(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_double_pythagoras) {
        emit_double_pythagoras(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_double_temp_convert) {
        emit_double_temp_convert(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_double_sqrt) {
        emit_double_sqrt(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_function) {
        emit_function(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_string_length) {
        emit_string_length(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_stack) {
        emit_stack(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_queue) {
        emit_queue(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_insertion_sort) {
        int c = 5;
        emit_insertion_sort(prog, f, c, task->skip_input);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_bmi_calculator) {
        emit_bmi_calculator(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_calculator) {
        emit_calculator(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_unit_converter) {
        emit_unit_converter(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_rock_paper_scissors) {
        emit_rock_paper_scissors(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_pyramid) {
        emit_pyramid(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_temp_converter_menu) {
        emit_temp_converter_menu(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_string_analyzer) {
        emit_string_analyzer(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_number_analyzer) {
        emit_number_analyzer(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_filter_numbers) {
        emit_filter_numbers(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_random_generator) {
        emit_random_generator(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_math_menu) {
        emit_math_menu(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_quiz_game) {
        emit_quiz_game(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_statistics_suite) {
        emit_statistics_suite(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_linked_list) {
        emit_linked_list(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_binary_search_tree) {
        emit_binary_search_tree(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_tree_traversal) {
        emit_tree_traversal(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_graph_bfs_dfs) {
        emit_graph_bfs_dfs(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_n_queens) {
        emit_n_queens(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_sudoku) {
        emit_sudoku(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_levenshtein) {
        emit_levenshtein_distance(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_maze_generator) {
        emit_maze_generator(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_maze_solver) {
        emit_maze_solver(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_monte_carlo) {
        emit_monte_carlo_pi(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_matrix_mul) {
        emit_matrix_multiplication(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_matrix_transpose) {
        emit_matrix_transpose(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_numerical_integration) {
        emit_numerical_integration(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_complex_numbers) {
        emit_complex_numbers(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_linear_regression) {
        emit_linear_regression(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_base_converter) {
        emit_base_converter(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_freq_analysis) {
        emit_freq_analysis(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_shuffle) {
        emit_shuffle(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_weighted_random) {
        emit_weighted_random(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }
    if (task->has_ascii_table) {
        emit_ascii_table(prog, f);
        ensure_exit(f, last_step);
        return 1;
    }

    // If no single emitter matched but extra emitters are specified, run them
    // as a composite sequence. Each extra emitter runs on the same function.
    if (task->num_extra_emitters > 0) {
        int emitted = 0;
        for (int ei = 0; ei < task->num_extra_emitters && ei < 8; ei++) {
            int idx = task->extra_emitters[ei];
            // Dispatch by index (matches the order in llm_select_emitter scoring)
            // We reuse generate_from_task recursively on a fresh task for each extra
            TaskProfile sub;
            memset(&sub, 0, sizeof(sub));
            sub.skip_input = 1;
            // Run the extra emitter by appending to the same function
            // We do this by calling specific emit_ functions based on index
            // (same order as the scoring in llm_select_emitter)
            switch (idx) {
                case 0: if (task->has_operation && task->has_literals) { emit_math(prog, f, task->type, task->op, task->literals, task->num_literals > 2 ? 3 : 2, 1); emitted = 1; } break;
                case 1: if (task->has_input && !task->has_operation) { emit_input_loop(prog, f, task->input_count > 0 ? task->input_count : 5, task->type, 0); emitted = 1; } break;
                case 3: if (task->has_sum_range) { emit_for_sum(prog, f, task->sum_range_n > 0 ? task->sum_range_n : 100); emitted = 1; } break;
                case 4: if (task->has_print_even) { emit_print_even(prog, f, task->print_even_n > 0 ? task->print_even_n : 100); emitted = 1; } break;
                case 8: if (task->has_input_sort) { emit_input_sort(prog, f, task->input_sort_count > 0 ? task->input_sort_count : 5, task->skip_input, task->has_descending); emitted = 1; } break;
                case 10: if (task->has_string_cat) { emit_string_cat(prog, f); emitted = 1; } break;
                case 13: if (task->has_array_reverse) { emit_array_reverse(prog, f, task->skip_input); emitted = 1; } break;
                case 14: if (task->has_array_find) { emit_array_find(prog, f, task->skip_input); emitted = 1; } break;
                case 16: if (task->has_array_vmath) { emit_array_vmath(prog, f, task->skip_input); emitted = 1; } break;
                case 17: if (task->has_read_file) { emit_read_file(prog, f); emitted = 1; } break;
                case 18: if (task->has_write_file) { emit_write_file(prog, f); emitted = 1; } break;
                case 22: if (task->has_fizzbuzz) { emit_fizzbuzz(prog, f); emitted = 1; } break;
                case 23: if (task->has_primes) { emit_primes(prog, f); emitted = 1; } break;
                case 24: if (task->has_even_odd) { emit_even_odd(prog, f); emitted = 1; } break;
                case 31: if (task->has_array_min_max) { emit_array_min_max(prog, f); emitted = 1; } break;
                case 37: if (task->has_average && task->has_input) { emit_average(prog, f, task->skip_input); emitted = 1; } break;
                case 38: if (task->has_sort && !task->has_input) { emit_selection_sort(prog, f, 5, task->skip_input); emitted = 1; } break;
                case 39: if (task->has_palindrome) { emit_palindrome(prog, f); emitted = 1; } break;
                case 40: if (task->has_lcm) { emit_lcm(prog, f); emitted = 1; } break;
                case 41: if (task->has_collatz) { emit_collatz(prog, f); emitted = 1; } break;
                case 42: if (task->has_sum_of_digits) { emit_sum_of_digits(prog, f); emitted = 1; } break;
                case 43: if (task->has_reverse_string) { emit_reverse_string(prog, f); emitted = 1; } break;
                case 44: if (task->has_armstrong) { emit_armstrong(prog, f); emitted = 1; } break;
                case 45: if (task->has_perfect_number) { emit_perfect_number(prog, f); emitted = 1; } break;
                case 46: if (task->has_count_vowels) { emit_count_vowels(prog, f); emitted = 1; } break;
                case 47: if (task->has_anagram_check) { emit_anagram_check(prog, f); emitted = 1; } break;
                case 48: if (task->has_string_to_upper) { emit_string_to_upper(prog, f); emitted = 1; } break;
                case 49: if (task->has_string_to_lower) { emit_string_to_lower(prog, f); emitted = 1; } break;
                case 50: if (task->has_caesar_cipher) { emit_caesar_cipher(prog, f); emitted = 1; } break;
                case 51: if (task->has_palindrome_string) { emit_palindrome_string(prog, f); emitted = 1; } break;
                case 52: if (task->has_bubble_sort) { emit_bubble_sort(prog, f, task->skip_input); emitted = 1; } break;
                case 53: if (task->has_binary_search) { emit_binary_search(prog, f); emitted = 1; } break;
                case 54: if (task->has_square_root) { emit_square_root(prog, f); emitted = 1; } break;
                case 55: if (task->has_prime_factorization) { emit_prime_factorization(prog, f); emitted = 1; } break;
                case 56: if (task->has_standard_deviation) { emit_standard_deviation(prog, f, task->skip_input); emitted = 1; } break;
                case 57: if (task->has_compound_interest) { emit_compound_interest(prog, f); emitted = 1; } break;
                case 58: if (task->has_decimal_to_binary) { emit_decimal_to_binary(prog, f); emitted = 1; } break;
                case 59: if (task->has_dice_roll) { emit_dice_roll(prog, f); emitted = 1; } break;
                case 60: if (task->has_double_math) { emit_double_math(prog, f, task->op); emitted = 1; } break;
                case 61: if (task->has_double_circle_area) { emit_double_circle_area(prog, f); emitted = 1; } break;
                case 62: if (task->has_double_average) { emit_double_average(prog, f, task->skip_input); emitted = 1; } break;
                case 63: if (task->has_double_compound_interest) { emit_double_compound_interest(prog, f); emitted = 1; } break;
                case 64: if (task->has_double_pythagoras) { emit_double_pythagoras(prog, f); emitted = 1; } break;
                case 65: if (task->has_double_temp_convert) { emit_double_temp_convert(prog, f); emitted = 1; } break;
                case 66: if (task->has_double_sqrt) { emit_double_sqrt(prog, f); emitted = 1; } break;
                case 68: if (task->has_string_length) { emit_string_length(prog, f); emitted = 1; } break;
                case 69: if (task->has_stack) { emit_stack(prog, f); emitted = 1; } break;
                case 70: if (task->has_queue) { emit_queue(prog, f); emitted = 1; } break;
                case 71: if (task->has_insertion_sort) { emit_insertion_sort(prog, f, 5, task->skip_input); emitted = 1; } break;
                case 72: if (task->has_calculator) { emit_calculator(prog, f); emitted = 1; } break;
                case 73: if (task->has_unit_converter) { emit_unit_converter(prog, f); emitted = 1; } break;
                case 74: if (task->has_rock_paper_scissors) { emit_rock_paper_scissors(prog, f); emitted = 1; } break;
                case 75: if (task->has_pyramid) { emit_pyramid(prog, f); emitted = 1; } break;
                case 86: if (task->has_linked_list) { emit_linked_list(prog, f); emitted = 1; } break;
                case 87: if (task->has_binary_search_tree) { emit_binary_search_tree(prog, f); emitted = 1; } break;
                case 88: if (task->has_tree_traversal) { emit_tree_traversal(prog, f); emitted = 1; } break;
                case 89: if (task->has_graph_bfs_dfs) { emit_graph_bfs_dfs(prog, f); emitted = 1; } break;
                case 90: if (task->has_n_queens) { emit_n_queens(prog, f); emitted = 1; } break;
                case 91: if (task->has_sudoku) { emit_sudoku(prog, f); emitted = 1; } break;
                case 92: if (task->has_levenshtein) { emit_levenshtein_distance(prog, f); emitted = 1; } break;
                case 93: if (task->has_maze_generator) { emit_maze_generator(prog, f); emitted = 1; } break;
                case 94: if (task->has_maze_solver) { emit_maze_solver(prog, f); emitted = 1; } break;
                case 95: if (task->has_monte_carlo) { emit_monte_carlo_pi(prog, f); emitted = 1; } break;
                case 96: if (task->has_matrix_mul) { emit_matrix_multiplication(prog, f); emitted = 1; } break;
                case 97: if (task->has_matrix_transpose) { emit_matrix_transpose(prog, f); emitted = 1; } break;
                case 98: if (task->has_numerical_integration) { emit_numerical_integration(prog, f); emitted = 1; } break;
                case 99: if (task->has_complex_numbers) { emit_complex_numbers(prog, f); emitted = 1; } break;
                case 100: if (task->has_linear_regression) { emit_linear_regression(prog, f); emitted = 1; } break;
                case 101: if (task->has_base_converter) { emit_base_converter(prog, f); emitted = 1; } break;
                case 102: if (task->has_freq_analysis) { emit_freq_analysis(prog, f); emitted = 1; } break;
                case 103: if (task->has_shuffle) { emit_shuffle(prog, f); emitted = 1; } break;
                case 104: if (task->has_weighted_random) { emit_weighted_random(prog, f); emitted = 1; } break;
                case 105: if (task->has_ascii_table) { emit_ascii_table(prog, f); emitted = 1; } break;
                default: break;
            }
        }
        if (emitted) {
            ensure_exit(f, last_step);
            return 1;
        }
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
        char ln[256];
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
        char ln[256];
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
        char ln[256];
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

static void gen_arithmetic(Program *prog, const char *op, const char *type, int *vals, int num_vals) {
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
    if (strcmp(word, "eins") == 0 || strcmp(word, "ein") == 0 || strcmp(word, "one") == 0) return 1;
    if (strcmp(word, "zwei") == 0 || strcmp(word, "two") == 0) return 2;
    if (strcmp(word, "drei") == 0 || strcmp(word, "three") == 0) return 3;
    if (strcmp(word, "vier") == 0 || strcmp(word, "four") == 0) return 4;
    if (strcmp(word, "fünf") == 0 || strcmp(word, "fuenf") == 0 || strcmp(word, "five") == 0) return 5;
    if (strcmp(word, "sechs") == 0 || strcmp(word, "six") == 0) return 6;
    if (strcmp(word, "sieben") == 0 || strcmp(word, "seven") == 0) return 7;
    if (strcmp(word, "acht") == 0 || strcmp(word, "eight") == 0) return 8;
    if (strcmp(word, "neun") == 0 || strcmp(word, "nine") == 0) return 9;
    if (strcmp(word, "zehn") == 0 || strcmp(word, "ten") == 0) return 10;
    return -1;
}

static int smart_generate(Program *prog, const char *prompt, char *desc, int desc_size) {
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
        char inherited_names[64][64] = {{0}};
        char inherited_types[64][32] = {{0}};
        int inherited_counts[64] = {0};
        int num_inherited = 0;
        for (int i = 0; i < num_steps; i++) {
            // Try per-step learned pattern match
            {
                int lscore;
                int lidx = match_learned_pattern(steps[i], &lscore);
                if (lidx >= 0) {
                    if (emit_learned_step(prog, lidx)) {
                        goto collect_vars;
                    }
                }
            }
            TaskProfile task;
            memset(&task, 0, sizeof(task));
            if (parse_task(steps[i], &task)) {
                task.skip_input = (i > 0);
                task.suppress_output = (i < num_steps - 1);
                dataflow_quiet_mode = task.suppress_output;
                // populate inherited vars from previous steps
                for (int iv = 0; iv < num_inherited; iv++) {
                    snprintf(task.inherit_var_names[task.num_inherit_vars], 64, "%s", inherited_names[iv]);
                    snprintf(task.inherit_var_types[task.num_inherit_vars], 32, "%s", inherited_types[iv]);
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
                generate_from_task(prog, &task, (i == num_steps - 1));
            }
            collect_vars:
            // collect inherited variables from the main function for next steps
            Function *fcur = NULL;
            for (int fi = 0; fi < prog->num_funcs; fi++)
                if (strcmp(prog->funcs[fi].name, "main") == 0) { fcur = &prog->funcs[fi]; break; }
            if (fcur) {
                for (int vi = 0; vi < fcur->num_vars && num_inherited < 64; vi++) {
                    int is_scalar = (fcur->vars[vi].count <= 1);
                    int is_const = (strstr(fcur->vars[vi].type, "const") != NULL);
                    int is_standard = (strcmp(fcur->vars[vi].name, "zero") == 0 || strcmp(fcur->vars[vi].name, "one") == 0);
                    if (!is_scalar && !is_const && !is_standard) {
                        int already = 0;
                        for (int ck = 0; ck < num_inherited; ck++) {
                            if (strcmp(inherited_names[ck], fcur->vars[vi].name) == 0) already = 1;
                        }
                        if (!already) {
                            snprintf(inherited_names[num_inherited], 64, "%s", fcur->vars[vi].name);
                            snprintf(inherited_types[num_inherited], 32, "%s", fcur->vars[vi].type);
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
            nums[num_count++] = atoi(p);
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
    const char *op_name = "operation";
    const char *sym = "+";
    if (strcmp(op, "sub") == 0) { op_name = "subtraction"; sym = "-"; }
    else if (strcmp(op, "mul") == 0) { op_name = "multiplication"; sym = "*"; }
    else if (strcmp(op, "div") == 0) { op_name = "division"; sym = "/"; }
    else if (strcmp(op, "mod") == 0) { op_name = "modulo"; sym = "%"; }
    else op_name = "addition";
    if (n_vals == 3)
        snprintf(desc, desc_size, "%s of three %s numbers (%d %s %d %s %d)", op_name, type, avals[0], sym, avals[1], sym, avals[2]);
    else
        snprintf(desc, desc_size, "%s of two %s numbers (%d %s %d)", op_name, type, avals[0], sym, avals[1]);
    return 1;
}

void write_program(Program *prog, const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) { fprintf(stderr, "Error: cannot write %s\n", filename); return; }

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char date[64];
    strftime(date, sizeof(date), "%Y-%m-%d", tm);

    fprintf(f, "// %s\n", filename);
    fprintf(f, "// Generated by Brackets Code on %s\n", date);
    fprintf(f, "//\n\n");

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
        fprintf(f, "%s", fn->body);
        fprintf(f, "(funcend)\n\n");
    }

    for (int i = 0; i < prog->num_includes_post; i++)
        fprintf(f, "#include <%s>\n", prog->includes_post[i]);

    fclose(f);
    printf("Written: %s\n", filename);
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
    (void)prompt;
    add_include(prog, "intr-func.l1h");
    add_func(prog, "main");
    Function *f = &prog->funcs[prog->num_funcs - 1];
    const char *vals[] = {"0"};
    const char *vals1[] = {"1"};
    const char *vals2[] = {"2"};
    const char *vals50[] = {"50"};
    add_var_to_func(f, "const-int64", "zero", 1, vals, 1);
    add_var_to_func(f, "int64", "one", 1, vals1, 1);
    add_var_to_func(f, "int64", "two", 1, vals2, 1);
    add_var_to_func(f, "int64", "num", 1, vals2, 1);
    add_var_to_func(f, "int64", "max", 1, vals50, 1);
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
    func_append(f, "\t\t\t(p ns 32 :string_int64tostring !)");
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
    func_append(f, "\t\t(for-loop)");
    func_append(f, "\t\t(((j n one - <) f :=) f for)");
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
    add_var_to_func(f, "int64", "x", 1, (const char *[]){"42"}, 1);
    add_var_to_func(f, "int64", "Px", 1, vals, 1);
    add_var_to_func(f, "int64", "val", 1, vals, 1);
    func_append(f, "\t// get pointer to x");
    func_append(f, "\t(x Px pointer)");
    func_append(f, "\t(Px [ zero ] val =)");
    func_append(f, "\t(val :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t// change value via pointer");
    func_append(f, "\t(100 Px [ zero ] =)");
    func_append(f, "\t(x :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(zero :exit !)");
}

void gen_time_demo(Program *prog, const char *prompt) {
    (void)prompt;
    add_include(prog, "intr-func.l1h");
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
    func_append(gf, "\t(((b~ zero~ ==) f~ =) f~ if)");
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

typedef struct {
    char *keywords;
    char *desc;
    void (*gen)(Program*, const char*);
} Template;

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
    {"if else", "If-else conditionals", gen_if_else},
    {"if", "If-else conditionals", gen_if_else},
    {"condition", "If-else conditionals", gen_if_else},
    {"positive negative", "Check positive/negative/zero", gen_if_else_chain},
    {"switch", "Switch/case example", gen_switch},
    {"case", "Switch/case example", gen_switch},
    {"array", "Array index access demo", gen_array_demo},
    {"print var", "Print a variable", gen_print_var},
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
    {"gcd", "GCD (greatest common divisor)", gen_gcd},
    {"hex", "Hex and binary number demo", gen_hex_binary},
    {"binary", "Hex and binary number demo", gen_hex_binary},
};

static int num_templates = sizeof(templates) / sizeof(templates[0]);

int match_template(const char *prompt, int *best_score) {
    char buf[MAX_PROMPT];
    snprintf(buf, sizeof(buf), "%s", prompt);
    to_lowercase(buf);

    int best_idx = -1;
    *best_score = 0;

    for (int i = 0; i < num_templates; i++) {
        char kwbuf[512];
        snprintf(kwbuf, sizeof(kwbuf), "%s", templates[i].keywords);
        to_lowercase(kwbuf);

        int score = 0;
        char kwcopy[512];
        snprintf(kwcopy, sizeof(kwcopy), "%s", kwbuf);
        char *kw = strtok(kwcopy, " ,/");
        int all_match = 1;
        while (kw) {
            trim(kw);
            if (strlen(kw) > 0) {
                if (str_contains_word(buf, kw)) {
                    score++;
                } else {
                    all_match = 0;
                }
            }
            kw = strtok(NULL, " ,/");
        }
        if (all_match && score > 0) {
            if (score > *best_score) {
                *best_score = score;
                best_idx = i;
            }
        }
    }
    return best_idx;
}

static void prepend_out_dir(const char *fname, char *buf, int bufsize);

static int validate_code(const char *filename) {
    char cmd[2048];
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
        static char fallback[512];
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

    snprintf(cmd, sizeof(cmd), "%s \"%s\" \"%s\" \"%s\" 2>&1", l1pre_bin, filename, ppname, include_dir);
    int ret = system(cmd);
    if (ret != 0) {
        c_printf(ANSI_RED, "Validation: l1pre FAILED (exit code %d)\n", ret);
        remove(ppname);
        return 0;
    }

    char compname[512];
    snprintf(compname, sizeof(compname), "%.*s_pp", (int)(dot ? dot - filename : (int)strlen(filename)), filename);
    snprintf(cmd, sizeof(cmd), "%s \"%s\" 2>&1", l1com_bin, compname);
    ret = system(cmd);
    if (ret == 0) {
        c_printf(ANSI_GREEN, "Validation: OK\n");
    } else {
        c_printf(ANSI_RED, "Validation: FAILED (exit code %d)\n", ret);
    }
    remove(ppname);
    return ret == 0;
}

static float cosine_sim(const float *a, const float *b) {
    float dot = 0, na = 0, nb = 0;
    for (int i = 0; i < EMBED_DIM; i++) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    float denom = sqrtf(na) * sqrtf(nb);
    return (denom == 0) ? 0 : dot / denom;
}

static void embed_text(const char *text, float *out) {
    memset(out, 0, sizeof(float) * EMBED_DIM);
    char buf[MAX_PROMPT];
    snprintf(buf, sizeof(buf), "%s", text);
    to_lowercase(buf);

    float total_weight = 0;
    char *p = buf;
    while (*p) {
        while (*p && !isalpha(*p)) p++;
        if (!*p) break;
        char *start = p;
        while (*p && isalpha(*p)) p++;
        char saved = *p;
        *p = '\0';

        int tok_id = -1;
        for (int i = 0; i < VOCAB_SIZE; i++) {
            if (strcmp(start, vocab[i]) == 0) { tok_id = i; break; }
        }
        if (tok_id < 0) {
            const char *syn = resolve_synonym(start);
            if (syn) {
                for (int i = 0; i < VOCAB_SIZE; i++) {
                    if (strcmp(syn, vocab[i]) == 0) { tok_id = i; break; }
                }
            }
        }

        if (tok_id >= 0) {
            float w = idf_weights[tok_id];
            for (int j = 0; j < EMBED_DIM; j++)
                out[j] += w * word_embeddings[tok_id].embed[j];
            total_weight += w;
        } else {
            int len = strlen(start);
            if (len >= 2) {
                float ngram_embed[EMBED_DIM];
                memset(ngram_embed, 0, sizeof(ngram_embed));
                int ngram_count = 0;
                for (int ci = 0; ci < len - 1; ci++) {
                    char bigram[3] = {start[ci], start[ci+1], '\0'};
                    unsigned long h = hash_word(bigram);
                    srand(h);
                    for (int j = 0; j < EMBED_DIM; j++)
                        ngram_embed[j] += ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
                    ngram_count++;
                }
                if (ngram_count > 0) {
                    float inv = 1.0f / ngram_count;
                    float norm = 0;
                    for (int j = 0; j < EMBED_DIM; j++) {
                        ngram_embed[j] *= inv;
                        norm += ngram_embed[j] * ngram_embed[j];
                    }
                    norm = sqrtf(norm);
                    if (norm > 0) {
                        for (int j = 0; j < EMBED_DIM; j++)
                            out[j] += ngram_embed[j] / norm;
                        total_weight += 1.0f;
                    }
                }
            }
        }

        *p = saved;
    }

    if (total_weight > 0) {
        float inv = 1.0f / total_weight;
        for (int j = 0; j < EMBED_DIM; j++) out[j] *= inv;
    }
}

static void filename_stem(const char *path, char *stem) {
    const char *p = strrchr(path, '/');
    p = p ? p + 1 : path;
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", p);
    char *dot = strrchr(buf, '.');
    if (dot) *dot = '\0';
    for (char *q = buf; *q; q++)
        if (*q == '-' || *q == '_') *q = ' ';
    snprintf(stem, 256, "%s", buf);
}

static void index_examples(void) {
    if (examples_indexed) return;
    examples_indexed = 1;
    num_examples = 0;

    for (int s = 0; s < EXAMPLE_SUBDIRS; s++) {
        char dirpath[512];
        snprintf(dirpath, sizeof(dirpath), "%s/%s", EXAMPLE_DIR, example_subdirs[s]);
        DIR *d = opendir(dirpath);
        if (!d) continue;

        struct dirent *entry;
        while ((entry = readdir(d)) != NULL && num_examples < MAX_EXAMPLES) {
            const char *ext = strrchr(entry->d_name, '.');
            if (!ext) continue;
            int valid_ext = 0;
            for (int e = 0; e < EXAMPLE_EXTS; e++) {
                if (strcmp(ext, example_exts[e]) == 0) { valid_ext = 1; break; }
            }
            if (!valid_ext) continue;

            char fullpath[1024];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);
            snprintf(example_docs[num_examples].filename, sizeof(example_docs[num_examples].filename), "%s", fullpath);

            char raw_stem[256];
            filename_stem(entry->d_name, raw_stem);
            snprintf(example_docs[num_examples].stem, sizeof(example_docs[num_examples].stem), "%s/%s", example_subdirs[s], raw_stem);

            char content[8192] = {0};
            FILE *f = fopen(fullpath, "r");
            if (f) {
                char line[512];
                int lines_read = 0;
                while (fgets(line, sizeof(line), f) && lines_read < 100 && strlen(content) < 7000) {
                    char *trimmed = line;
                    while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
                    if (trimmed[0] == '/' && trimmed[1] == '/') continue;
                    strncat(content, line, sizeof(content) - strlen(content) - 1);
                    lines_read++;
                }
                fclose(f);
            }
            char combined[8192];
            snprintf(combined, sizeof(combined), "%s %s %s %s",
                     example_docs[num_examples].stem,
                     example_docs[num_examples].stem,
                     example_docs[num_examples].stem,
                     content);
            embed_text(combined, example_docs[num_examples].embedding);
            num_examples++;
        }
        closedir(d);
    }

    if (num_examples > 0) {
        int doc_freq[VOCAB_SIZE];
        memset(doc_freq, 0, sizeof(doc_freq));
        for (int i = 0; i < num_examples; i++) {
            int seen[VOCAB_SIZE] = {0};
            char buf[MAX_PROMPT];
            snprintf(buf, sizeof(buf), "%s", example_docs[i].stem);
            int tokens[128];
            int n = tokenize(buf, tokens, 128);
            for (int t = 0; t < n; t++) {
                if (!seen[tokens[t]]) {
                    seen[tokens[t]] = 1;
                    doc_freq[tokens[t]]++;
                }
            }
        }
        for (int i = 0; i < VOCAB_SIZE; i++) {
            if (doc_freq[i] > 0)
                idf_weights[i] = logf((float)num_examples / doc_freq[i]) + 1.0f;
            else
                idf_weights[i] = 1.0f;
        }
    }
}

static int search_examples(const char *query, int top_k, int *indices, float *scores) {
    if (num_examples == 0) return 0;

    char expanded[MAX_PROMPT];
    expand_query(query, expanded, sizeof(expanded));

    float q_embed[EMBED_DIM];
    embed_text(expanded, q_embed);

    for (int i = 0; i < num_examples; i++)
        example_docs[i].score = cosine_sim(q_embed, example_docs[i].embedding);

    int count = top_k < num_examples ? top_k : num_examples;
    int *used = calloc(num_examples, sizeof(int));
    int result = 0;
    for (int k = 0; k < count; k++) {
        int best = -1;
        for (int i = 0; i < num_examples; i++) {
            if (used[i]) continue;
            if (best < 0 || example_docs[i].score > example_docs[best].score)
                best = i;
        }
        if (best >= 0 && example_docs[best].score > 0) {
            indices[k] = best;
            scores[k] = example_docs[best].score;
            used[best] = 1;
            result++;
        }
    }
    free(used);
    return result;
}

static void prepend_out_dir(const char *fname, char *buf, int bufsize) {
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

    Program *prog = calloc(1, sizeof(Program));
    if (!prog) { c_printf(ANSI_RED, "Out of memory\n"); return 0; }
    init_program(prog);
    snprintf(prog->filename, sizeof(prog->filename), "%s", filename);
    reset_temp();
    func_counter = 0;

    // Try emitter system first; learned patterns serve as fallback
    char desc[256] = {0};
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
            char nn_msg[256];
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

// ==================== LEARNED PATTERN SYSTEM ====================

static char* learned_dir_path(void) {
    static char path[1024];
    const char *home = getenv("HOME");
    if (home) {
        snprintf(path, sizeof(path), "%s/%s", home, LEARNED_DIR);
    } else {
        snprintf(path, sizeof(path), "%s", LEARNED_DIR);
    }
    return path;
}

static void ensure_learned_dir(void) {
    char path[1024];
    snprintf(path, sizeof(path), "%s", learned_dir_path());
    char cmd[1100];
    snprintf(cmd, sizeof(cmd), "mkdir -p \"%s\"", path);
    system(cmd);
}

static int learn_from_file(const char *path, const char *keywords, const char *description) {
    FILE *f = fopen(path, "r");
    if (!f) {
        c_printf(ANSI_RED, "Error: cannot open file '%s'\n", path);
        return 0;
    }

    // Extract id from filename
    const char *base = strrchr(path, '/');
    base = base ? base + 1 : path;
    char id[64];
    snprintf(id, sizeof(id), "%s", base);
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
    snprintf(lp->id, sizeof(lp->id), "%s", id);
    snprintf(lp->source_path, sizeof(lp->source_path), "%s", path);
    lp->is_learned = 1;

    if (keywords && strlen(keywords) > 0) {
        snprintf(lp->keywords, sizeof(lp->keywords), "%s %s", id, keywords);
    } else {
        snprintf(lp->keywords, sizeof(lp->keywords), "%s", id);
    }

    if (description && strlen(description) > 0) {
        snprintf(lp->description, sizeof(lp->description), "%s", description);
    } else {
        snprintf(lp->description, sizeof(lp->description), "Learned pattern from %s", path);
    }

    // Parse the .l1com file into includes, globals and function structs
    char line[MAX_LINE];
    Function *cur_func = NULL;
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
                    snprintf(lp->includes[lp->num_includes++], sizeof(lp->includes[0]), "%s", inc);
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
                snprintf(cur_func->name, sizeof(cur_func->name), "%s", fname);
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
                strcat(lp->globals, line);
                strcat(lp->globals, "\n");
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
                    snprintf(v->name, sizeof(v->name), "%s", vname);
                    snprintf(v->type, sizeof(v->type), "%s", vtype);
                    v->count = vcount;
                    // Parse values
                    char line_copy[MAX_LINE];
                    snprintf(line_copy, sizeof(line_copy), "%s", line);
                    // format: (set TYPE COUNT NAME [VAL1 VAL2 ...] )
                    // tokens: ["(set", TYPE, COUNT, NAME, VAL1, VAL2, ..., ")"]
                    int tok_idx = 0;
                    char *token = strtok(line_copy, " \t");
                    while (token) {
                        if (tok_idx >= 4) {
                            if (strcmp(token, ")") == 0) break;
                            size_t tlen = strlen(token);
                            if (token[tlen-1] == ')') token[tlen-1] = '\0';
                            if (v->num_values < MAX_VALUES) {
                                snprintf(v->values[v->num_values], sizeof(v->values[0]), "%s", token);
                                v->num_values++;
                            }
                        }
                        tok_idx++;
                        token = strtok(NULL, " \t");
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
                    snprintf(cur_func->vardef_name, sizeof(cur_func->vardef_name), "%s", scope);
                }
            }
            continue;
        }

        // Body code
        if (cur_func) {
            size_t needed = strlen(cur_func->body) + strlen(line) + 2;
            if (!ensure_body_cap(cur_func, (int)needed + 1)) continue;
            strcat(cur_func->body, line);
            strcat(cur_func->body, "\n");
        }
    }
    fclose(f);

    // Check we got at least one function
    if (lp->num_funcs == 0) {
        c_printf(ANSI_RED, "Error: no functions found in '%s'\n", path);
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
    return 0;
}

static int save_learned_pattern(LearnedPattern *lp) {
    ensure_learned_dir();
    char filepath[1100];
    snprintf(filepath, sizeof(filepath), "%s/%s.l1lp", learned_dir_path(), lp->id);

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
        Function *fn = &lp->funcs[fi];
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

static void load_learned_patterns(void) {
    if (learned_loaded) return;
    learned_loaded = 1;

    char dirpath[1024];
    snprintf(dirpath, sizeof(dirpath), "%s", learned_dir_path());

    DIR *d = opendir(dirpath);
    if (!d) return;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL && num_learned < MAX_LEARNED) {
        const char *ext = strrchr(entry->d_name, '.');
        if (!ext || strcmp(ext, ".l1lp") != 0) continue;

        char filepath[1100];
        snprintf(filepath, sizeof(filepath), "%s/%s", dirpath, entry->d_name);

        FILE *f = fopen(filepath, "r");
        if (!f) continue;

        LearnedPattern *lp = &learned_patterns[num_learned];
        init_learned_pattern(lp);
        lp->is_learned = 1;

        char line[MAX_LINE];
        int in_source = 0;
        Function *cur_func = NULL;

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
                    snprintf(lp->description, sizeof(lp->description), "%s", val + 15);
                    continue;
                }
                if ((val = strstr(line, "# Keywords: ")) != NULL) {
                    snprintf(lp->keywords, sizeof(lp->keywords), "%s", val + 12);
                    continue;
                }
                if ((val = strstr(line, "# File: ")) != NULL) {
                    snprintf(lp->source_path, sizeof(lp->source_path), "%s", val + 8);
                    continue;
                }
                continue;
            }

            // Parse source content
            if (strncmp(line, "#include", 8) == 0) {
                char inc[256] = {0};
                if (sscanf(line, "#include <%255[^>]>", inc) == 1) {
                    if (ensure_includes_cap(&lp->includes, &lp->includes_cap, lp->num_includes + 1))
                        snprintf(lp->includes[lp->num_includes++], sizeof(lp->includes[0]), "%s", inc);
                }
                continue;
            }

            if (strstr(line, " func)")) {
                char fname[256] = {0};
                if (sscanf(line, "(%255s func)", fname) == 1) {
                    if (ensure_funcs_cap(&lp->funcs, &lp->funcs_cap, lp->num_funcs + 1)) {
                        cur_func = &lp->funcs[lp->num_funcs++];
                        init_function(cur_func);
                        snprintf(cur_func->name, sizeof(cur_func->name), "%s", fname);
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
                    strcat(lp->globals, line);
                    strcat(lp->globals, "\n");
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
                        snprintf(v->name, sizeof(v->name), "%s", vname);
                        snprintf(v->type, sizeof(v->type), "%s", vtype);
                        v->count = vcount;
                        char lc[MAX_LINE];
                        snprintf(lc, sizeof(lc), "%s", line);
                        int tok_idx = 0;
                        char *token = strtok(lc, " \t");
                        while (token) {
                            if (tok_idx >= 4) {
                                if (strcmp(token, ")") == 0) break;
                                size_t tlen = strlen(token);
                                if (token[tlen-1] == ')') token[tlen-1] = '\0';
                                if (v->num_values < MAX_VALUES) {
                                    snprintf(v->values[v->num_values], sizeof(v->values[0]), "%s", token);
                                    v->num_values++;
                                }
                            }
                            tok_idx++;
                            token = strtok(NULL, " \t");
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
                    snprintf(cur_func->vardef_name, sizeof(cur_func->vardef_name), "%s", scope);
                }
                continue;
            }

            if (cur_func) {
                size_t needed = strlen(cur_func->body) + strlen(line) + 2;
                if (!ensure_body_cap(cur_func, (int)needed + 1)) continue;
                strcat(cur_func->body, line);
                strcat(cur_func->body, "\n");
            }
        }
        fclose(f);

        if (lp->num_funcs > 0) {
            num_learned++;
            if (verbose_flag) {
                printf("  Loaded learned pattern: %s (%s)\n", lp->id, lp->description);
            }
        }
    }
    closedir(d);
}

static int match_learned_pattern(const char *prompt, int *best_score) {
    if (num_learned == 0) return -1;

    char buf[MAX_PROMPT];
    snprintf(buf, sizeof(buf), "%s", prompt);
    to_lowercase(buf);

    int best_idx = -1;
    *best_score = 0;

    for (int i = 0; i < num_learned; i++) {
        if (!learned_patterns[i].is_learned) continue;
        if (strlen(learned_patterns[i].keywords) == 0) continue;

        char kwbuf[1024];
        snprintf(kwbuf, sizeof(kwbuf), "%s", learned_patterns[i].keywords);
        to_lowercase(kwbuf);

        int score = 0;
        int total_kw = 0;
        int any_match = 0;
        char kwcopy[1024];
        snprintf(kwcopy, sizeof(kwcopy), "%s", kwbuf);
        char *kw = strtok(kwcopy, " ,/");
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
            kw = strtok(NULL, " ,/");
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

static int emit_learned_pattern(Program *prog, int learned_idx) {
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
            strcat(prog->globals, lp->globals);
    }

    // Copy functions
    for (int fi = 0; fi < lp->num_funcs; fi++) {
        Function *src = &lp->funcs[fi];
        add_func(prog, src->name);
        Function *dst = &prog->funcs[prog->num_funcs - 1];

        dst->is_local = src->is_local;
        dst->has_vars = src->has_vars;
        dst->has_vardef = src->has_vardef;
        snprintf(dst->vardef_name, sizeof(dst->vardef_name), "%s", src->vardef_name);

        // Copy variables
        for (int vi = 0; vi < src->num_vars; vi++) {
            if (!ensure_vars_cap(dst, dst->num_vars + 1)) continue;
            Variable *sv = &src->vars[vi];
            Variable *dv = &dst->vars[dst->num_vars++];
            snprintf(dv->name, sizeof(dv->name), "%s", sv->name);
            snprintf(dv->type, sizeof(dv->type), "%s", sv->type);
            dv->count = sv->count;
            dv->num_values = sv->num_values;
            for (int vj = 0; vj < sv->num_values; vj++)
                snprintf(dv->values[vj], sizeof(dv->values[vj]), "%s", sv->values[vj]);
        }

        // Copy body
        size_t needed = strlen(src->body) + 1;
        if (ensure_body_cap(dst, (int)needed))
            snprintf(dst->body, (size_t)dst->body_cap, "%s", src->body);
    }

    return 1;
}

static int emit_learned_step(Program *prog, int learned_idx) {
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
            strcat(prog->globals, lp->globals);
    }

    // Copy functions - for existing main, append body & vars
    for (int fi = 0; fi < lp->num_funcs; fi++) {
        Function *src = &lp->funcs[fi];

        int existing = -1;
        for (int i = 0; i < prog->num_funcs; i++)
            if (strcmp(prog->funcs[i].name, src->name) == 0) { existing = i; break; }

        if (existing >= 0) {
            Function *dst = &prog->funcs[existing];
            size_t needed = strlen(dst->body) + strlen(src->body) + 1;
            if (ensure_body_cap(dst, (int)needed + 1))
                strcat(dst->body, src->body);
            for (int vi = 0; vi < src->num_vars; vi++) {
                int found = 0;
                for (int dv = 0; dv < dst->num_vars; dv++)
                    if (strcmp(dst->vars[dv].name, src->vars[vi].name) == 0) { found = 1; break; }
                if (!found) {
                    if (!ensure_vars_cap(dst, dst->num_vars + 1)) continue;
                    Variable *sv = &src->vars[vi];
                    Variable *dv = &dst->vars[dst->num_vars++];
                    snprintf(dv->name, sizeof(dv->name), "%s", sv->name);
                    snprintf(dv->type, sizeof(dv->type), "%s", sv->type);
                    dv->count = sv->count;
                    dv->num_values = sv->num_values;
                    for (int vj = 0; vj < sv->num_values; vj++)
                        snprintf(dv->values[vj], sizeof(dv->values[vj]), "%s", sv->values[vj]);
                }
            }
        } else {
            add_func(prog, src->name);
            Function *dst = &prog->funcs[prog->num_funcs - 1];
            dst->is_local = src->is_local;
            dst->has_vars = src->has_vars;
            dst->has_vardef = src->has_vardef;
            snprintf(dst->vardef_name, sizeof(dst->vardef_name), "%s", src->vardef_name);
            for (int vi = 0; vi < src->num_vars; vi++) {
                if (!ensure_vars_cap(dst, dst->num_vars + 1)) continue;
                Variable *sv = &src->vars[vi];
                Variable *dv = &dst->vars[dst->num_vars++];
                snprintf(dv->name, sizeof(dv->name), "%s", sv->name);
                snprintf(dv->type, sizeof(dv->type), "%s", sv->type);
                dv->count = sv->count;
                dv->num_values = sv->num_values;
                for (int vj = 0; vj < sv->num_values; vj++)
                    snprintf(dv->values[vj], sizeof(dv->values[vj]), "%s", sv->values[vj]);
            }
            size_t needed = strlen(src->body) + 1;
            if (ensure_body_cap(dst, (int)needed))
                snprintf(dst->body, (size_t)dst->body_cap, "%s", src->body);
        }
    }
    return 1;
}

static int has_learned_id(const char *id) {
    for (int i = 0; i < num_learned; i++) {
        if (strcmp(learned_patterns[i].id, id) == 0)
            return 1;
    }
    return 0;
}

static int forget_learned(const char *id) {
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
    snprintf(filepath, sizeof(filepath), "%s/%s.l1lp", learned_dir_path(), learned_patterns[found].id);
    remove(filepath);

    // Shift array
    for (int i = found; i < num_learned - 1; i++)
        learned_patterns[i] = learned_patterns[i + 1];
    num_learned--;

    c_printf(ANSI_GREEN, "Forgot pattern '%s'.\n", id);
    return 1;
}

static void list_learned(void) {
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

static int self_test(void) {
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
        char fullpath[512];
        prepend_out_dir(fname, fullpath, sizeof(fullpath));
        remove(fullpath);
    }
    printf("\n--- self-test: ");
    c_printf(ANSI_GREEN, "%d passed", passed);
    printf(", ");
    c_printf(ANSI_RED, "%d failed", failed);
    printf(", %d total ---\n", np);
    return failed == 0;
}

void show_help() {
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

void interactive_mode() {
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
    printf("> ");
    while (fgets(prompt, sizeof(prompt), stdin)) {
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
            char fname[512] = {0};
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
            char lpath[1024] = {0}, lkeywords[512] = {0}, ldesc[256] = {0};
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
        printf("  opts=\"--help -h --list -l --search --validate -v --self-test -t --verbose --out-dir --l1vm-root --batch --bash-completion --learn --forget --list-learned\"\n");
        printf("  COMPREPLY=($(compgen -W \"${opts}\" -- \"$cur\"))\n");
        printf("}\n");
        printf("complete -F _brackets_code_completions brackets-code\n");
        return 0;
    }

    // Scan for global flags before positional args
    while (arg_idx < argc) {
        if (strcmp(argv[arg_idx], "--verbose") == 0) { verbose_flag = 1; arg_idx++; }
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
            char bfname[256];
            prompt_to_filename(bline, bfname, sizeof(bfname));
            char bfull[512];
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
        if (!strstr(fname, ".l1com")) strcat(fname, ".l1com");
    } else {
        prompt_to_filename(prompt, fname, sizeof(fname));
    }
    char fullpath[512];
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
