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

/* format truncation warnings are benign: buffers sized for real-world usage */
#pragma GCC diagnostic ignored "-Wformat-truncation"

#define MAX_LINE 4096
#define MAX_VARS 48
#define MAX_FUNCS 24
#define MAX_PROMPT 8192
#define MAX_CODE 65536
#define MAX_OUTPUT 4096
#define MAX_VALUES 16

int validate_flag = 0;
static const char *current_prompt = "";

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
    Variable vars[MAX_VARS];
    int num_vars;
    char body[MAX_CODE];
} Function;

typedef struct {
    char filename[256];
    Function funcs[MAX_FUNCS];
    int num_funcs;
    char includes[MAX_FUNCS][256];
    int num_includes;
    char includes_post[MAX_FUNCS][256];
    int num_includes_post;
    char globals[MAX_CODE];
} Program;

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
    snprintf(prog->includes[prog->num_includes++], sizeof(prog->includes[0]), "%s", inc);
}

void add_include_post(Program *prog, const char *inc) {
    for (int i = 0; i < prog->num_includes_post; i++)
        if (strcmp(prog->includes_post[i], inc) == 0) return;
    snprintf(prog->includes_post[prog->num_includes_post++], sizeof(prog->includes_post[0]), "%s", inc);
}

void add_func(Program *prog, const char *name) {
    for (int i = 0; i < prog->num_funcs; i++)
        if (strcmp(prog->funcs[i].name, name) == 0) return;
    Function *f = &prog->funcs[prog->num_funcs++];
    snprintf(f->name, sizeof(f->name), "%s", name);
    f->is_local = 0;
    f->has_vars = 0;
    f->has_vardef = 0;
    f->num_vars = 0;
    f->body[0] = '\0';
}

void add_var_to_func(Function *f, const char *type, const char *name, int count, const char **values, int num_values) {
    // Skip if variable with same name already exists (prevents duplicate declarations)
    for (int i = 0; i < f->num_vars; i++)
        if (strcmp(f->vars[i].name, name) == 0) return;
    Variable *v = &f->vars[f->num_vars++];
    snprintf(v->name, sizeof(v->name), "%s", name);
    snprintf(v->type, sizeof(v->type), "%s", type);
    v->count = count;
    v->num_values = num_values;
    for (int i = 0; i < num_values && i < MAX_VALUES; i++)
        snprintf(v->values[i], sizeof(v->values[i]), "%s", values[i]);
}

void func_append(Function *f, const char *line) {
    if (strlen(f->body) + strlen(line) + 2 < MAX_CODE) {
        strcat(f->body, line);
        strcat(f->body, "\n");
    }
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
        printf("  (Px [ offset ] val :=) - read value at pointer+offset into val\n");
        printf("  (val Px [ offset ] :=) - write val to pointer+offset\n");
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
        printf("  (arr [ realind ] val :=)            - read element\n");
        printf("  (val arr [ realind ] :=)            - write element\n");
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
        printf("Brackets (L1VM) is a stack-based virtual machine and assembly language.\n");
        printf("Code is written in .l1com files using S-expressions (parentheses).\n");
        printf("Key concepts: (func), (set), (if), (for-loop), pointer access, local scoping via #var ~\n");
    } else {
        printf("I'm a Brackets (L1VM) code generator. Ask me about:\n");
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
};
static const int NUM_SYNONYMS = sizeof(SYNONYM_TABLE) / sizeof(SYNONYM_TABLE[0]);

static const char* resolve_synonym(const char *word) {
    for (int i = 0; i < NUM_SYNONYMS; i++)
        if (strcmp(word, SYNONYM_TABLE[i].word) == 0)
            return SYNONYM_TABLE[i].canonical;
    return NULL;
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
    static char buf[64];
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
    int has_bit_check;
    int has_leap_year;
    int has_temp_convert;
    int has_circle_area;
    int skip_input;
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
                if (strcmp(prefix, "int") == 0 || strcmp(prefix, "const") == 0) {
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
static void emit_input_sort(Program *prog, Function *f, int count, int skip_input);
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

// ==================== TINY LLM INFERENCE ENGINE ====================

#define EMBED_DIM 32
#define VOCAB_SIZE 64
#define TEMPERATURE 0.8
#define MAX_STEPS 8
#define NUM_EMITTERS 34

typedef struct {
    char word[32];
    float embed[EMBED_DIM];
} WordEmbedding;

static WordEmbedding word_embeddings[VOCAB_SIZE];
static float attention_weights[NUM_EMITTERS];
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
    char filename[512];
    char stem[256];
    float embedding[EMBED_DIM];
    float score;
} ExampleDoc;

static ExampleDoc example_docs[MAX_EXAMPLES];
static int num_examples = 0;
static int examples_indexed = 0;

static void index_examples(void);
static int search_examples(const char *query, int top_k, int *indices, float *scores);

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
    "start", "stop", "run"
};

static void init_embeddings(void) {
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

    for (int ti = 0; ti < num_tokens && ti < 32; ti++) {
        int tok_id = tokens[ti];
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
        || has_word(text, "execution") || has_word(text, "ausführ") || has_word(text, "lauf");
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
                        char merged_step[MAX_PROMPT];
                        snprintf(merged_step, MAX_PROMPT, "%s %s", steps[i-1], steps[i]);
                        trim(merged_step);
                        snprintf(steps[i-1], MAX_PROMPT, "%s", merged_step);
                        for (int j = i; j < num_steps - 1; j++) snprintf(steps[j], MAX_PROMPT, "%s", steps[j+1]);
                        num_steps--; merged = 1; break;
                    } else if (i == 0 && num_steps > 1) {
                        char merged_step[MAX_PROMPT];
                        snprintf(merged_step, MAX_PROMPT, "%s %s", steps[0], steps[1]);
                        trim(merged_step);
                        snprintf(steps[0], MAX_PROMPT, "%s", merged_step);
                        for (int j = 1; j < num_steps - 1; j++) snprintf(steps[j], MAX_PROMPT, "%s", steps[j+1]);
                        num_steps--; merged = 1; break;
                    }
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
    func_append(f, "\t(sum :print_i !)");
    func_append(f, "\t(:print_n !)");
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

static void emit_input_sort(Program *prog, Function *f, int count, int skip_input) {
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
        snprintf(ln, sizeof(ln), "\t(((i count <) f :=) f for)");
        func_append(f, ln);
        func_append(f, "\t\t(input_prompt :print_s !)");
        func_append(f, "\t\t(i * int64_size realind :=)");
        func_append(f, "\t\t(a :input_i !)");
        func_append(f, "\t\t(a arr [ realind ] :=)");
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
    snprintf(ln, sizeof(ln), "\t(((j count one - <) f :=) f for)");
    func_append(f, ln);
    func_append(f, "\t\t\t(j * int64_size realind :=)");
    func_append(f, "\t\t\t((j + one) * int64_size realind2 :=)");
    func_append(f, "\t\t\t(arr [ realind ] a :=)");
    func_append(f, "\t\t\t(arr [ realind2 ] b :=)");
    func_append(f, "\t\t\t(((a b >) f :=) f if)");
    func_append(f, "\t\t\t\t(b arr [ realind ] :=)");
    func_append(f, "\t\t\t\t(a arr [ realind2 ] :=)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t\t(j + one j :=)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t// print sorted");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    snprintf(ln, sizeof(ln), "\t(((i count <) f :=) f for)");
    func_append(f, ln);
    func_append(f, "\t\t(i * int64_size realind :=)");
    func_append(f, "\t\t(arr [ realind ] a :=)");
    func_append(f, "\t\t(a :print_i !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
}

static void emit_median(Program *prog, Function *f, int count, int skip_input) {
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
        func_append(f, "\t\t(a arr [ realind ] :=)");
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
    snprintf(ln, sizeof(ln), "\t(((j count one - <) f :=) f for)");
    func_append(f, ln);
    func_append(f, "\t\t\t(j * int64_size realind :=)");
    func_append(f, "\t\t\t((j + one) * int64_size realind2 :=)");
    func_append(f, "\t\t\t(arr [ realind ] a :=)");
    func_append(f, "\t\t\t(arr [ realind2 ] b :=)");
    func_append(f, "\t\t\t(((a b >) f :=) f if)");
    func_append(f, "\t\t\t\t(b arr [ realind ] :=)");
    func_append(f, "\t\t\t\t(a arr [ realind2 ] :=)");
    func_append(f, "\t\t\t(endif)");
    func_append(f, "\t\t\t(j + one j :=)");
    func_append(f, "\t\t(next)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t// median at midpoint");
    func_append(f, "\t(count / two mid :=)");
    snprintf(ln, sizeof(ln), "\t(mid * int64_size realind :=)");
    func_append(f, ln);
    func_append(f, "\t(arr [ realind ] a :=)");
    func_append(f, "\t(a :print_i !)");
    func_append(f, "\t(:print_n !)");
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
    func_append(f, "\t(index * int64_size realind :=)");
    func_append(f, "\t(value arr [ realind ] :=)");
    func_append(f, "\t// print array");
    func_append(f, "\t(zero index :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((index 5 <) f :=) f for)");
    func_append(f, "\t\t(index * int64_size realind :=)");
    func_append(f, "\t\t(arr [ realind ] value :=)");
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
        func_append(f, "\t\t(a + zero a :=)");
        func_append(f, "\t\t(a arr [ realind ] :=)");
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
    func_append(f, "\t\t(arr [ realind2 ] a :=)");
    func_append(f, "\t\t(a + zero a :=)");
    func_append(f, "\t\t(a reverse [ realind ] :=)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t// copy array");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i count <) f :=) f for)");
    func_append(f, "\t\t(i * int64_size realind :=)");
    func_append(f, "\t\t(reverse [ realind ] a :=)");
    func_append(f, "\t\t(a + zero a :=)");
    func_append(f, "\t\t(a arr [ realind ] :=)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t// print reversed");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i count <) f :=) f for)");
    func_append(f, "\t\t(i * int64_size realind :=)");
    func_append(f, "\t\t(arr [ realind ] a :=)");
    func_append(f, "\t\t(a :print_i !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
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
        func_append(f, "\t\t(a arr [ realind ] :=)");
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
    func_append(f, "\t\t(arr [ realind ] a :=)");
    func_append(f, "\t\t(((a key ==) f :=) f if)");
    func_append(f, "\t\t\t(one found :=)");
    func_append(f, "\t\t\t(:found_break jmp)");
    func_append(f, "\t\t(endif)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(:found_break)");
    func_append(f, "\t(((found one ==) f :=) f if+)");
    func_append(f, "\t\t(i :print_i !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t(else)");
    func_append(f, "\t\t(not_str :print_s !)");
    func_append(f, "\t\t(:print_n !)");
    func_append(f, "\t(endif)");
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
    func_append(f, "\t(fac :print_i !)");
    func_append(f, "\t(:print_n !)");
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
        func_append(f, "\t\t(val arr [ realind ] :=)");
        func_append(f, "\t\t(i + one i :=)");
        func_append(f, "\t(next)");
    }
    func_append(f, "\t// compute stats");
    func_append(f, "\t(zero i :=)");
    func_append(f, "\t(arr [ 0 ] mn :=)");
    func_append(f, "\t(arr [ 0 ] mx :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i count <) f :=) f for)");
    func_append(f, "\t\t(i * int64_size realind :=)");
    func_append(f, "\t\t(arr [ realind ] val :=)");
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
    func_append(f, "\t(mn :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t// max");
    func_append(f, "\t(mx :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t// avg");
    func_append(f, "\t(avg :print_i !)");
    func_append(f, "\t(:print_n !)");
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
    const char *ov[] = {"1"};
    const char *fv[] = {"5"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "int64", "num", 1, fv, 1);
    add_var_to_func(f, "int64", "fac", 1, ov, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    func_append(f, "\t// calculate factorial");
    func_append(f, "\t(one i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i num <=) f :=) f for)");
    func_append(f, "\t\t(fac i * fac :=)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(fac :print_i !)");
    func_append(f, "\t(:print_n !)");
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
    const char *ov[] = {"1"};
    const char *bv[] = {"2"};
    const char *ev[] = {"10"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    add_var_to_func(f, "int64", "base", 1, bv, 1);
    add_var_to_func(f, "int64", "exp", 1, ev, 1);
    add_var_to_func(f, "int64", "ret", 1, zv, 1);
    add_var_to_func(f, "int64", "i", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    func_append(f, "\t// power: base^exp");
    func_append(f, "\t(base ret :=)");
    func_append(f, "\t(one i :=)");
    func_append(f, "\t(for-loop)");
    func_append(f, "\t(((i exp <) f :=) f for)");
    func_append(f, "\t\t(ret base * ret :=)");
    func_append(f, "\t\t(i + one i :=)");
    func_append(f, "\t(next)");
    func_append(f, "\t(ret :print_i !)");
    func_append(f, "\t(:print_n !)");
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
    add_var_to_func(f, "int64", "mod", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    func_append(f, "\t// Euclidean algorithm");
    func_append(f, "\t(do)");
    func_append(f, "\t\t(a b % mod :=)");
    func_append(f, "\t\t(b a :=)");
    func_append(f, "\t\t(mod b :=)");
    func_append(f, "\t(((b zero >) f :=) f while)");
    func_append(f, "\t(a :print_i !)");
    func_append(f, "\t(:print_n !)");
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
    func_append(f, "\t(min_str :print_s !)");
    func_append(f, "\t(mn :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(max_str :print_s !)");
    func_append(f, "\t(mx :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t(avg_str :print_s !)");
    func_append(f, "\t(avg :print_i !)");
    func_append(f, "\t(:print_n !)");
    add_include_post(prog, "math-lib-vect.l1h");
}

static void emit_bool_demo(Program *prog, Function *f) {
    add_include(prog, "intr-func.l1h");
    const char *zv[] = {"0"};
    add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
    const char *ov[] = {"1"};
    add_var_to_func(f, "const-int64", "one", 1, ov, 1);
    const char *av[] = {"42"};
    add_var_to_func(f, "int64", "a", 1, av, 1);
    add_var_to_func(f, "int64", "b", 1, zv, 1);
    add_var_to_func(f, "int64", "f", 1, zv, 1);
    add_var_to_func(f, "bool", "Bool", 1, (const char *[]){"true"}, 1);
    const char *ps[] = {"\"Bool = \""};
    add_var_to_func(f, "const-string", "bool_str", 8, ps, 1);
    func_append(f, "\t(a b :=)");
    func_append(f, "\t(((b a ==) f :=) f if+)");
    func_append(f, "\t\t(true Bool :=)");
    func_append(f, "\t(else)");
    func_append(f, "\t\t(false Bool :=)");
    func_append(f, "\t(endif)");
    func_append(f, "\t(bool_str :print_s !)");
    func_append(f, "\t(Bool :print_i !)");
    func_append(f, "\t(:print_n !)");
    add_include_post(prog, "bool.l1h");
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

    if (has_word(buf, "power") || has_word(buf, "potenz") || has_word(buf, "exponent")
        || has_word(buf, "square") || has_word(buf, "quadrat") || has_word(buf, "cube"))
        task->has_power = 1;

    if (has_word(buf, "max") || has_word(buf, "größt") || has_word(buf, "largest")
        || has_word(buf, "greatest") || has_word(buf, "highest"))
        task->has_max = 1;

    if (has_word(buf, "gcd") || has_word(buf, "ggt") || has_word(buf, "gcm"))
        task->has_gcd = 1;

    if (has_word(buf, "countdown") || has_word(buf, "count down"))
        task->has_countdown = 1;

    if (has_word(buf, "table") || has_word(buf, "einmaleins") || has_word(buf, "multiplication"))
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

    if (has_word(buf, "string") || has_word(buf, "zeichen") || has_word(buf, "text")
        || has_word(buf, "wort"))
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

    if (has_word(buf, "countdown") && task->has_literals) {
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

    if (has_word(buf, "bool")) {
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
        || task->has_array_min_max || task->has_bool_demo || task->has_bit_check
        || task->has_leap_year || task->has_temp_convert || task->has_circle_area;

    snprintf(task->title, sizeof(task->title), "%s", prompt);
    return has_any;
}

// ==================== PLAN-BASED GENERATOR ====================

static int generate_from_task(Program *prog, TaskProfile *task, int last_step) {
    (void)last_step;
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

    // Dispatch to emitter based on task profile
    if (task->has_sum_range) {
        int n = task->sum_range_n > 0 ? task->sum_range_n : 100;
        emit_for_sum(prog, f, n);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_print_even) {
        int n = task->print_even_n > 0 ? task->print_even_n : 100;
        emit_print_even(prog, f, n);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_find_max) {
        int c = task->find_max_count > 0 ? task->find_max_count : 5;
        emit_input_find_max(prog, f, c);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_fib_seq) {
        int n = task->fib_seq_n > 0 ? task->fib_seq_n : 10;
        emit_fib_seq(prog, f, n);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_countdown_from) {
        int s = task->countdown_start > 0 ? task->countdown_start : 10;
        emit_countdown_from(prog, f, s);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_input_sort) {
        int c = task->input_sort_count > 0 ? task->input_sort_count : 5;
        emit_input_sort(prog, f, c, task->skip_input);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_median) {
        int c = task->median_count > 0 ? task->median_count : 5;
        emit_median(prog, f, c, task->skip_input);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_input_fact) {
        emit_input_factorial(prog, f);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_string_cat) {
        emit_string_cat(prog, f);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_string_compare) {
        emit_string_compare(prog, f);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_array_assign) {
        emit_array_assign(prog, f);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_array_reverse) {
        emit_array_reverse(prog, f, task->skip_input);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_array_find) {
        emit_array_find(prog, f, task->skip_input);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_array_vmath) {
        emit_array_vmath(prog, f, task->skip_input);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_read_file) {
        emit_read_file(prog, f);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_write_file) {
        emit_write_file(prog, f);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_string_to_num) {
        emit_string_to_num(prog, f);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_timer) {
        emit_timer(prog, f);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_factorial && !task->has_input) {
        emit_factorial(prog, f);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_fizzbuzz) {
        emit_fizzbuzz(prog, f);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_primes) {
        emit_primes(prog, f);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_even_odd && !task->has_print_even) {
        emit_even_odd(prog, f);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_power) {
        emit_power(prog, f);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_mult_table) {
        emit_multiplication_table(prog, f);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_guess) {
        emit_guess_number(prog, f);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_gcd) {
        emit_gcd(prog, f);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_hello_name) {
        emit_hello_name(prog, f);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_random) {
        emit_random_number(prog, f);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_array_min_max) {
        emit_array_min_max(prog, f);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_bool_demo) {
        emit_bool_demo(prog, f);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_bit_check) {
        emit_bit_check(prog, f);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_loop && task->has_literals && task->has_sum) {
        int n = task->num_literals > 0 ? task->literals[0] : 100;
        emit_for_sum(prog, f, n);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_operation && task->has_literals) {
        int n = task->num_literals > 2 ? 3 : (task->num_literals < 2 ? 2 : task->num_literals);
        int vals[3] = {10, 3, 5};
        if (task->num_literals >= 1) vals[0] = task->literals[0];
        if (task->num_literals >= 2) vals[1] = task->literals[1];
        if (task->num_literals >= 3) vals[2] = task->literals[2];
        emit_math(prog, f, task->type, task->op, vals, n, 1);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
        return 1;
    }
    if (task->has_leap_year) {
        emit_leap_year(prog, f);
        return 1;
    }
    if (task->has_temp_convert) {
        emit_temp_convert(prog, f);
        return 1;
    }
    if (task->has_circle_area) {
        emit_circle_area(prog, f);
        return 1;
    }
    if (task->has_input && !task->has_operation) {
        int c = task->input_count > 0 ? task->input_count : 5;
        emit_input_loop(prog, f, c, task->type, 0);
        const char *zv[] = {"0"};
        add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
        func_append(f, "\t(zero :exit !)");
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

    // Multi-step support
    char steps[MAX_STEPS][MAX_PROMPT];
    int num_steps = split_prompt_steps(prompt, steps);

    if (num_steps > 1) {
        for (int i = 0; i < num_steps; i++) {
            TaskProfile task;
            memset(&task, 0, sizeof(task));
            if (parse_task(steps[i], &task)) {
                task.skip_input = (i > 0);
                generate_from_task(prog, &task, (i == num_steps - 1));
            }
        }
        // ensure exit if not already added by last step
        Function *f = NULL;
        for (int i = 0; i < prog->num_funcs; i++)
            if (strcmp(prog->funcs[i].name, "main") == 0) { f = &prog->funcs[i]; break; }
        if (f && !strstr(f->body, ":exit !")) {
            const char *zv[] = {"0"};
            add_var_to_func(f, "const-int64", "zero", 1, zv, 1);
            func_append(f, "\t(zero :exit !)");
        }
        snprintf(desc, desc_size, "multi-step generation (%d steps)", num_steps);
        return 1;
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
    func_append(f, "\t\t(arr [ realind ] num :=)");
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
    func_append(f, "\t\t\t(arr [ realind ] a :=)");
    func_append(f, "\t\t\t(arr [ realind2 ] b :=)");
    func_append(f, "\t\t\t(((a b >) f :=) f if)");
    func_append(f, "\t\t\t\t(b arr [ realind ] :=)");
    func_append(f, "\t\t\t\t(a arr [ realind2 ] :=)");
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
    func_append(f, "\t\t(arr [ realind ] a :=)");
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
    func_append(f, "\t(Px [ zero ] val :=)");
    func_append(f, "\t(val :print_i !)");
    func_append(f, "\t(:print_n !)");
    func_append(f, "\t// change value via pointer");
    func_append(f, "\t(100 Px [ zero ] :=)");
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

static int validate_code(const char *filename) {
    char cmd[1024];
    char ppname[512];
    const char *dot = strrchr(filename, '.');
    if (dot) {
        int len = dot - filename;
        snprintf(ppname, sizeof(ppname), "%.*s_pp.l1com", len, filename);
    } else {
        snprintf(ppname, sizeof(ppname), "%s_pp.l1com", filename);
    }
    const char *include_dir = getenv("L1VM_INCLUDE");
    if (!include_dir) include_dir = "/home/stefan/l1vm/include/";

    snprintf(cmd, sizeof(cmd), "l1pre \"%s\" \"%s\" \"%s\" 2>&1", filename, ppname, include_dir);
    int ret = system(cmd);
    if (ret != 0) {
        printf("Validation: l1pre FAILED (exit code %d)\n", ret);
        remove(ppname);
        return 0;
    }

    char compname[512];
    snprintf(compname, sizeof(compname), "%.*s_pp", (int)(dot ? dot - filename : (int)strlen(filename)), filename);
    snprintf(cmd, sizeof(cmd), "l1com \"%s\" 2>&1", compname);
    ret = system(cmd);
    if (ret == 0) {
        printf("Validation: OK\n");
    } else {
        printf("Validation: FAILED (exit code %d)\n", ret);
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

            char content[2048] = {0};
            FILE *f = fopen(fullpath, "r");
            if (f) {
                char line[512];
                int lines_read = 0;
                while (fgets(line, sizeof(line), f) && lines_read < 5) {
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

    float q_embed[EMBED_DIM];
    embed_text(query, q_embed);

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

int generate_code(const char *prompt, const char *filename) {
    int is_q = is_question(prompt);
    if (is_q)
        answer_question(prompt);

    Program *prog = calloc(1, sizeof(Program));
    if (!prog) { fprintf(stderr, "Out of memory\n"); return 0; }
    snprintf(prog->filename, sizeof(prog->filename), "%s", filename);
    reset_temp();
    func_counter = 0;

    char desc[256] = {0};
    if (smart_generate(prog, prompt, desc, sizeof(desc))) {
        if (is_q) printf("Code example written to: %s\n", filename);
        else printf("Smart generated (plan): %s\n", desc);
        write_program(prog, filename);
        free(prog);
        return 1;
    }

    int score;
    int idx = match_template(prompt, &score);

    if (idx >= 0) {
        if (is_q) printf("Code example written to: %s\n", filename);
        else printf("Matched template: %s (score: %d)\n", templates[idx].desc, score);
        templates[idx].gen(prog, prompt);
        write_program(prog, filename);
        free(prog);
        return 1;
    }

    if (is_q) {
        free(prog);
        return 1;
    }

    printf("No template matched for: '%s'. Generating default hello world.\n", prompt);
    gen_hello_world(prog, prompt);
    write_program(prog, filename);
    free(prog);
    return 0;
}

void show_help() {
    printf("\nBrackets Code Generator for Brackets (L1VM) Language\n");
    printf("============================================================\n");
    printf("Usage:\n");
    printf("  Interactive mode:  brackets-code\n");
    printf("  One-shot mode:     brackets-code \"<prompt>\" [filename]\n");
    printf("  With validation:   brackets-code --validate \"<prompt>\" [filename]\n");
    printf("  Vector search:     brackets-code --search \"<query>\"\n");
    printf("  List templates:    brackets-code --list\n\n");
    printf("Available templates:\n");
    for (int i = 0; i < num_templates; i++) {
        printf("  - %s: %s\n", templates[i].keywords, templates[i].desc);
    }
    printf("\nSpecial commands in interactive mode:\n");
    printf("  /help      - Show this help\n");
    printf("  /list      - List all templates\n");
    printf("  /search <q>- Vector search example code\n");
    printf("  /exit      - Exit\n");
    printf("  /save <fn> - Save last generated code to file\n");
    printf("\n");
}

void interactive_mode() {
    char prompt[MAX_PROMPT];
    char last_fname[256] = {0};
    int has_last = 0;

    printf("Brackets Code Generator\n");
    printf("Type /help for help, /exit to quit.\n");
    printf("> ");

    while (fgets(prompt, sizeof(prompt), stdin)) {
        trim(prompt);
        if (strlen(prompt) == 0) { printf("> "); continue; }

        if (strcmp(prompt, "/exit") == 0 || strcmp(prompt, "/quit") == 0) break;

        if (strcmp(prompt, "/help") == 0 || strcmp(prompt, "/?") == 0) {
            show_help();
            printf("> ");
            continue;
        }

        if (strcmp(prompt, "/list") == 0) {
            printf("Available templates:\n");
            for (int i = 0; i < num_templates; i++)
                printf("  %-25s %s\n", templates[i].keywords, templates[i].desc);
            printf("> ");
            continue;
        }

        if (strncmp(prompt, "/save", 5) == 0) {
            char fname[256] = {0};
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
            printf("> ");
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
            printf("> ");
            continue;
        }

        char fname[256];
        prompt_to_filename(prompt, fname, sizeof(fname));
        generate_code(prompt, fname);
        snprintf(last_fname, sizeof(last_fname), "%s", fname);
        has_last = 1;
        printf("> ");
    }
    printf("\nBye!\n");
}

int main(int argc, char *argv[]) {
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
    if (strcmp(argv[arg_idx], "--validate") == 0 || strcmp(argv[arg_idx], "-v") == 0) {
        validate_flag = 1;
        arg_idx++;
        if (arg_idx >= argc) {
            fprintf(stderr, "Usage: %s [--validate] <prompt> [filename]\n", argv[0]);
            return 1;
        }
    }

    const char *prompt = argv[arg_idx++];
    char fname[256];

    if (arg_idx < argc) {
        snprintf(fname, sizeof(fname), "%s", argv[arg_idx]);
        if (!strstr(fname, ".l1com")) strcat(fname, ".l1com");
    } else {
        prompt_to_filename(prompt, fname, sizeof(fname));
    }

    generate_code(prompt, fname);
    if (validate_flag) validate_code(fname);
    return 0;
}
