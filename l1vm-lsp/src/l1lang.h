/*
 * This file l1lang.h is part of L1vm.
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
 * l1vm-lsp - Brackets language analysis
 * Document model, tokenizer, symbol tables, diagnostics and LSP feature helpers
 */

#ifndef L1VM_LANG_H
#define L1VM_LANG_H

#include "json.h"

/* ---------------- settings ---------------- */

#define L1_L1COM_AUTO 0
#define L1_L1COM_ON 1
#define L1_L1COM_OFF 2

typedef struct {
    int l1com_enabled;     /* L1_L1COM_* */
    char *l1com_path;      /* NULL => "l1com" */
    char *include_dirs;    /* '\n' separated list, NULL if none */
    int static_diag;       /* enable built-in static analysis */
    int missing_tilde_hint;
} L1Settings;

extern L1Settings l1_settings;

/* ---------------- document model ---------------- */

typedef struct { char *text; int nbytes; } L1Line;

typedef struct {
    char *name;
    char *type;
    char *size;
    char *scope;
    int line, col, end_col;   /* 0-based line, UTF-16 columns */
    int is_const;
    int is_array;
} L1Var;

typedef struct {
    char *name;
    char *parent;            /* object name for methods, else NULL */
    char *signature;         /* from "// (func args name ...)" comment */
    char *ret_sig;
    int line, col, end_col;
    int end_line;            /* -1 while unclosed */
    int is_object;
} L1Func;

typedef struct {
    char *name;
    int line, col, end_col;
    int end_line;
} L1Obj;

typedef struct {
    char *name;
    int line, col, end_col;
} L1Label;

typedef enum { U_VAR, U_FUNC, U_LABEL, U_MACRO } L1UseKind;

typedef struct {
    L1UseKind kind;
    char *name;              /* resolved symbol name */
    char *full;              /* as written */
    int line, col, len;      /* UTF-16 */
    int ref;                 /* index into vars/funcs/labels/macros, or -1 */
    int is_def;
    int is_builtin;
    int is_lib;              /* from a library file */
    int is_call;             /* used in a "name!" invocation */
} L1Usage;

typedef struct { char *name; char *file; int line, col; } L1Inc;
typedef struct { char *name; char *args; int line, col; } L1Macro;

typedef enum { L1SEV_ERROR = 1, L1SEV_WARNING, L1SEV_INFO, L1SEV_HINT } L1Sev;

typedef struct {
    L1Sev sev;
    char *msg;
    char *source;            /* e.g. "l1vm" or "l1com" */
    int line, col, end_line, end_col;
} L1Diag;

/* token kinds */
enum {
    TK_EOF = 0,
    TK_WORD,     /* identifier / keyword / type */
    TK_NUMBER,
    TK_STRING,
    TK_COMMENT,
    TK_LABEL,    /* ":name" or ":method->object" */
    TK_PREPROC,  /* #include / #var / ... */
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_LBRACKET, TK_RBRACKET,
    TK_OP,       /* operators, assignment, punctuation like = + - ! */
    TK_COMMA, TK_SEMI, TK_OTHER
};

/* token flags */
enum {
    TF_DECL_VAR = 1,   /* name token of a set declaration */
    TF_DECL_FUNC = 2,  /* name token of a (name func) or (name object) */
    TF_DECL_LABEL = 4, /* label definition token */
    TF_BUILTIN = 8     /* matches a builtin library function */
};

typedef struct {
    int kind;
    int line;
    int bstart, blen;      /* byte offsets in the line */
    int start, len;        /* UTF-16 offsets */
    int flags;
    char *text;
} L1Tok;

/* simple vectors */
#define L1_VEC(T) typedef struct { T *data; int len, cap; } L1Vec_##T

L1_VEC(L1Var);
L1_VEC(L1Func);
L1_VEC(L1Obj);
L1_VEC(L1Label);
L1_VEC(L1Usage);
L1_VEC(L1Inc);
L1_VEC(L1Macro);
L1_VEC(L1Diag);
L1_VEC(L1Tok);
typedef struct { char **data; int len, cap; } L1Vec_str;

typedef struct {
    char *uri;
    char *path;
    int version;
    char *text;
    L1Line *lines;
    int nlines;

    L1Vec_L1Var vars;
    L1Vec_L1Func funcs;
    L1Vec_L1Obj objs;
    L1Vec_L1Label labels;
    L1Vec_L1Usage usages;
    L1Vec_L1Inc includes;
    L1Vec_L1Macro macros;
    L1Vec_L1Diag diags;
    L1Vec_L1Tok toks;

    int has_main;
    int is_header;
    int compiler_diags;    /* diags came from l1com */
    int parse_error;       /* an internal parse error occurred */
    int inside_asm;        /* between (asm...asmend) while analyzing */
} L1Doc;

/* ---------------- builtins ---------------- */

typedef struct {
    const char *name;
    const char *lib;
    const char *args;
    const char *doc;
} L1Builtin;

const L1Builtin *l1_find_builtin(const char *name);
const L1Builtin *l1_builtins(void);

/* ---------------- doc lifecycle ---------------- */

L1Doc *l1_doc_new(const char *uri, const char *path, const char *text, int version);
void l1_doc_set_text(L1Doc *d, const char *text, int version);
void l1_doc_free(L1Doc *d);
void l1_doc_analyze(L1Doc *d);
void l1_doc_diagnostics(L1Doc *d);
void l1_doc_run_compiler(L1Doc *d);

/* ---------------- utf16 <-> byte ---------------- */

int l1_utf8_to_utf16(const char *s, int byte_off);
int l1_utf16_to_utf8(const char *s, int col);
int l1_is_word_char(unsigned char c);

/* ---------------- LSP features (return JSON) ---------------- */

JVal *l1_completion(L1Doc *d, int line, int col);
JVal *l1_hover(L1Doc *d, int line, int col);
JVal *l1_definition(L1Doc *d, int line, int col);
JVal *l1_references(L1Doc *d, int line, int col);
JVal *l1_document_symbols(L1Doc *d);
JVal *l1_folding(L1Doc *d);
JVal *l1_highlight(L1Doc *d, int line, int col);
JVal *l1_signature(L1Doc *d, int line, int col);
JVal *l1_semantic_tokens(L1Doc *d);

/* ---------------- helpers ---------------- */

const char *l1_type_doc(const char *type);
int l1_keyword_doc(const char *kw, SB *out);
const char *l1_keyword_snippet(const char *kw);

#endif
