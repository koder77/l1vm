/*
 * linter-solver.c - bounded model checking / constraint solving for the
 * L1VM linter.
 *
 * This file is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (info@midnight-coding.de), 2024
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
 * The interval analysis of the main linter is a fast over-approximation.
 * It cannot remember relations such as x + y == 10 or x == y, so it loses
 * many proofs at control flow joins.
 *
 * This module follows the CBMC idea instead:
 *
 *   1. split the source text into parts (functions),
 *   2. build a small structured control flow tree for every part,
 *   3. symbolically execute every feasible path through the tree,
 *   4. collect the mathematical constraints of the path, and
 *   5. let the Z3 solver decide whether a branch, assertion or assumption
 *      can actually be reached / violated.
 *
 * All bit-vector operations use the exact 64 bit L1VM semantics. Paths are
 * bounded (loops are unrolled) and the analysis is conservative: whenever the
 * model is incomplete (unknown calls, unsupported statements, budget or
 * unrolling limits) the affected path is marked "imprecise" and no diagnostic
 * is based on it. That keeps the solver output free of false positives.
 */

#include "../include/global.h"

#ifdef HAVE_Z3

#include <z3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* helpers and state of the main linter */
extern S8 linter_warnings;
extern S2 linenum;
extern U1 file_inside;
extern U1 warn_line_marked (S8 line);
extern void warn_line_mark (S8 line);

extern S2 tokenize_line (U1 *line, U1 tokens[][MAXSTRLEN + 1], S2 max_tokens);
extern S2 find_close_paren (S2 nt, U1 tokens[][MAXSTRLEN + 1], S2 open);
extern S2 find_open_paren (S2 nt, U1 tokens[][MAXSTRLEN + 1], S2 close);
extern U1 is_number_token (U1 *tok);
extern U1 is_ident_start (U1 c);
extern U1 is_ident_char (U1 c);
extern U1 is_operator_suffix (U1 c);
extern U1 token_is_keyword (U1 *tok);
extern U1 getvartype (U1 *type);
extern char *fgets_uni (char *str, int len, FILE *fptr);
extern size_t strlen_safe (const char * str, S8 maxlen);
extern S2 ip_extract_func_start (U1 *line, U1 *name);
extern S2 ip_is_funcend (U1 *line);

#define SV_MAXTOK           128
#define SV_MAX_VARS         1024
#define SV_MAX_CASES        256
#define SV_MAX_UNROLL       8
#define SV_MAX_QUERIES      5000
#define SV_MAX_STEPS        20000
#define SV_MAX_ENV_SLOTS    200000
#define SV_MAX_LIST         1024

#define SV_ESC_NONE         0
#define SV_ESC_BREAK        1
#define SV_ESC_RETURN       2
#define SV_ESC_JUMP         3

/* statement kinds */
#define SVN_STMT            1
#define SVN_ASSERT          2
#define SVN_ASSUME          3
#define SVN_IF              4
#define SVN_DO              5
#define SVN_FOR             6
#define SVN_SWITCH          7
#define SVN_BREAK           8
#define SVN_RETURN          9
#define SVN_JMP             10
#define SVN_LABEL           11

/* parser stop reasons */
#define SVSTOP_ELSE         100
#define SVSTOP_ENDIF        101
#define SVSTOP_WHILE        102
#define SVSTOP_NEXT         103
#define SVSTOP_CASE         104
#define SVSTOP_SWITCHEND    105

struct sv_var
{
    U1 name[MAXSTRLEN + 1];
    U1 type;
    U1 is_const;
    U1 modeled;
};

struct sv_node;

struct sv_case
{
    S2 line;
    struct sv_node *body;
};

struct sv_node
{
    S2 kind;
    S2 line;
    S2 cond_line;
    struct sv_node *next;
    struct sv_node *body;
    struct sv_node *else_body;
    struct sv_case *cases;
    S2 ncases;
};

struct sv_part
{
    U1 name[MAXSTRLEN + 1];
    U1 **lines;
    S8 *line_nums;
    S2 count;
    S2 cap;
    struct sv_var vars[SV_MAX_VARS];
    S2 nvars;
    struct sv_node *body;
};

struct sv_env
{
    Z3_ast *v;
    Z3_ast pc;
    U1 precise;
    U1 esc;
};

struct sv_list
{
    struct sv_env **e;
    S2 n;
    S2 cap;
};

struct sv_ctx
{
    Z3_context z3;
    Z3_solver solver;
    Z3_sort bv64;
    S8 queries;
    S8 steps;
    S8 env_slots;
    S8 fresh;
    U1 truncated;
    S2 unroll;
    S8 cur_line;
    U1 *cur_text;
};

struct sv_val
{
    Z3_ast ast;
    U1 is_bool;
    U1 ok;
};

/* one token buffer is enough: every caller finishes with a line before it
 * recurses into another block. */
static U1 sv_tokens[SV_MAXTOK][MAXSTRLEN + 1];

/* ------------------------------------------------------------------------- */
/* small helpers                                                             */
/* ------------------------------------------------------------------------- */

static U1 sv_is (U1 *a, const char *b)
{
    return (U1) (strcmp ((const char *) a, b) == 0);
}

static void sv_trim (U1 *line, S2 *start)
{
    S2 i = 0;

    while (line[i] == ' ' || line[i] == '\t')
    {
        i++;
    }
    *start = i;
}

static U1 sv_line_is_comment (U1 *line)
{
    S2 p;

    sv_trim (line, &p);
    return (U1) (line[p] == '/' && line[p + 1] == '/');
}

static U1 sv_line_has (U1 *line, const char *what)
{
    return (U1) (strstr ((const char *) line, what) != NULL);
}

static S2 sv_classify (U1 *line, S2 *nt_out)
{
    S2 nt;
    S2 i;
    S2 p;

    if (sv_line_is_comment (line) == 1)
    {
        if (sv_line_has (line, "(assert ") == 1)
        {
            if (nt_out) *nt_out = 0;
            return (SVN_ASSERT);
        }
        if (sv_line_has (line, "(assume ") == 1)
        {
            if (nt_out) *nt_out = 0;
            return (SVN_ASSUME);
        }
        if (nt_out) *nt_out = 0;
        return (0);
    }

    nt = tokenize_line (line, sv_tokens, SV_MAXTOK);
    if (nt_out)
    {
        *nt_out = nt;
    }
    if (nt == 0)
    {
        return (0);
    }

    sv_trim (line, &p);
    if (line[p] == '#')
    {
        return (0);
    }

    for (i = 0; i < nt; i++)
    {
        if (sv_is (sv_tokens[i], "else") == 1) return (SVSTOP_ELSE);
        if (sv_is (sv_tokens[i], "endif") == 1) return (SVSTOP_ENDIF);
        if (sv_is (sv_tokens[i], "while") == 1) return (SVSTOP_WHILE);
        if (sv_is (sv_tokens[i], "next") == 1) return (SVSTOP_NEXT);
        if (sv_is (sv_tokens[i], "switchend") == 1) return (SVSTOP_SWITCHEND);
    }

    /* for-loop is split into "for" "-" "loop" by the tokenizer */
    for (i = 0; i + 2 < nt; i++)
    {
        if (sv_is (sv_tokens[i], "for") == 1 &&
            sv_is (sv_tokens[i + 1], "-") == 1 &&
            sv_is (sv_tokens[i + 2], "loop") == 1)
        {
            return (SVN_FOR);
        }
    }
    for (i = 0; i < nt; i++)
    {
        if (sv_is (sv_tokens[i], "for") == 1) return (SVN_FOR);
    }
    for (i = 0; i < nt; i++)
    {
        if (sv_is (sv_tokens[i], "?") == 1) return (SVSTOP_CASE);
    }
    for (i = 0; i < nt; i++)
    {
        if (sv_is (sv_tokens[i], "switch") == 1) return (SVN_SWITCH);
        if (sv_is (sv_tokens[i], "do") == 1) return (SVN_DO);
        if (sv_is (sv_tokens[i], "if") == 1 ||
            sv_is (sv_tokens[i], "if+") == 1) return (SVN_IF);
        if (sv_is (sv_tokens[i], "break") == 1) return (SVN_BREAK);
        if (sv_is (sv_tokens[i], "return") == 1) return (SVN_RETURN);
        if (sv_is (sv_tokens[i], "jmp") == 1 ||
            sv_is (sv_tokens[i], "jmpi") == 1) return (SVN_JMP);
    }

    /* (label :) */
    if (nt == 4 && sv_is (sv_tokens[2], ":") == 1 &&
        is_ident_start (sv_tokens[1][0]) == 1)
    {
        return (SVN_LABEL);
    }

    /* (:label) is the label definition used by L1VM */
    if (nt == 4 && sv_is (sv_tokens[1], ":") == 1 &&
        is_ident_start (sv_tokens[2][0]) == 1 &&
        token_is_keyword (sv_tokens[2]) == 0)
    {
        return (SVN_LABEL);
    }

    return (SVN_STMT);
}

/* ------------------------------------------------------------------------- */
/* Z3 helpers                                                                */
/* ------------------------------------------------------------------------- */

static Z3_ast sv_num (struct sv_ctx *c, S8 v)
{
    return Z3_mk_int64 (c->z3, (int64_t) v, c->bv64);
}

static Z3_ast sv_true (struct sv_ctx *c)
{
    return Z3_mk_true (c->z3);
}

static Z3_ast sv_and (struct sv_ctx *c, Z3_ast a, Z3_ast b)
{
    if (a == NULL) return b;
    if (b == NULL) return a;
    return Z3_mk_and (c->z3, 2, (Z3_ast[]) { a, b });
}

static Z3_ast sv_or (struct sv_ctx *c, Z3_ast a, Z3_ast b)
{
    if (a == NULL) return b;
    if (b == NULL) return a;
    return Z3_mk_or (c->z3, 2, (Z3_ast[]) { a, b });
}

static Z3_ast sv_not (struct sv_ctx *c, Z3_ast a)
{
    if (a == NULL) return NULL;
    return Z3_mk_not (c->z3, a);
}

static Z3_ast sv_eq (struct sv_ctx *c, Z3_ast a, Z3_ast b)
{
    return Z3_mk_eq (c->z3, a, b);
}

static Z3_ast sv_to_bool (struct sv_ctx *c, struct sv_val v)
{
    if (v.ok == 0 || v.ast == NULL)
    {
        return NULL;
    }
    if (v.is_bool == 1)
    {
        return v.ast;
    }
    return Z3_mk_not (c->z3, Z3_mk_eq (c->z3, v.ast, sv_num (c, 0)));
}

static Z3_ast sv_to_bv (struct sv_ctx *c, struct sv_val v)
{
    if (v.ok == 0 || v.ast == NULL)
    {
        return NULL;
    }
    if (v.is_bool == 0)
    {
        return v.ast;
    }
    return Z3_mk_ite (c->z3, v.ast, sv_num (c, 1), sv_num (c, 0));
}

/* result: 1 = satisfiable, 0 = unsatisfiable, -1 = unknown/budget */
static S2 sv_check (struct sv_ctx *c, Z3_ast pc, Z3_ast extra)
{
    Z3_lbool r;

    if (c->queries >= SV_MAX_QUERIES)
    {
        c->truncated = 1;
        return (-1);
    }
    c->queries++;

    Z3_solver_push (c->z3, c->solver);
    if (pc != NULL)
    {
        Z3_solver_assert (c->z3, c->solver, pc);
    }
    if (extra != NULL)
    {
        Z3_solver_assert (c->z3, c->solver, extra);
    }
    r = Z3_solver_check (c->z3, c->solver);
    Z3_solver_pop (c->z3, c->solver, 1);

    if (r == Z3_L_TRUE) return (1);
    if (r == Z3_L_FALSE) return (0);
    return (-1);
}

/* ------------------------------------------------------------------------- */
/* variable table and environments                                           */
/* ------------------------------------------------------------------------- */

static S2 sv_find_var (struct sv_part *p, U1 *name)
{
    S2 i;

    for (i = 0; i < p->nvars; i++)
    {
        if (sv_is (p->vars[i].name, (const char *) name) == 1)
        {
            return (i);
        }
    }
    return (-1);
}

static S2 sv_add_var (struct sv_part *p, U1 *name, U1 type, U1 is_const)
{
    S2 i;

    i = sv_find_var (p, name);
    if (i >= 0)
    {
        return (i);
    }
    if (p->nvars >= SV_MAX_VARS || name[0] == '\0')
    {
        return (-1);
    }
    strncpy ((char *) p->vars[p->nvars].name, (const char *) name, MAXSTRLEN);
    p->vars[p->nvars].name[MAXSTRLEN] = '\0';
    p->vars[p->nvars].type = type;
    p->vars[p->nvars].is_const = is_const;
    p->vars[p->nvars].modeled = (U1) (type == QUADWORD);
    return (p->nvars++);
}

static Z3_ast sv_fresh (struct sv_ctx *c, struct sv_part *p, U1 *name)
{
    char buf[MAXSTRLEN + 64];

    c->fresh++;
    snprintf (buf, sizeof (buf), "%s@%s!%lld", (const char *) name,
              (const char *) p->name, (long long) c->fresh);
    return Z3_mk_fresh_const (c->z3, buf, c->bv64);
}

static struct sv_env *sv_env_new (struct sv_ctx *c, struct sv_part *p)
{
    struct sv_env *e;

    if (c->env_slots + p->nvars > SV_MAX_ENV_SLOTS)
    {
        c->truncated = 1;
        return (NULL);
    }
    e = (struct sv_env *) calloc (1, sizeof (struct sv_env));
    if (e == NULL)
    {
        c->truncated = 1;
        return (NULL);
    }
    e->v = (Z3_ast *) calloc ((size_t) p->nvars + 1, sizeof (Z3_ast));
    if (e->v == NULL)
    {
        free (e);
        c->truncated = 1;
        return (NULL);
    }
    c->env_slots += p->nvars + 1;
    e->pc = sv_true (c);
    e->precise = 1;
    e->esc = SV_ESC_NONE;
    return (e);
}

static struct sv_env *sv_env_clone (struct sv_ctx *c, struct sv_part *p,
                                    struct sv_env *src)
{
    struct sv_env *e = sv_env_new (c, p);

    if (e == NULL)
    {
        return (NULL);
    }
    if (p->nvars > 0 && src->v != NULL)
    {
        memcpy (e->v, src->v, (size_t) p->nvars * sizeof (Z3_ast));
    }
    e->pc = src->pc;
    e->precise = src->precise;
    e->esc = src->esc;
    return (e);
}

static void sv_list_add (struct sv_ctx *c, struct sv_list *l, struct sv_env *e)
{
    if (e == NULL)
    {
        return;
    }
    if (l->n >= l->cap)
    {
        S2 nc = l->cap == 0 ? 8 : l->cap * 2;
        struct sv_env **ne;

        if (nc > SV_MAX_LIST)
        {
            nc = SV_MAX_LIST;
        }
        if (nc <= l->cap)
        {
            c->truncated = 1;
            return;
        }
        ne = (struct sv_env **) realloc (l->e, (size_t) nc * sizeof (struct sv_env *));
        if (ne == NULL)
        {
            c->truncated = 1;
            return;
        }
        l->e = ne;
        l->cap = nc;
    }
    l->e[l->n] = e;
    l->n++;
}

static void sv_env_havoc_all (struct sv_ctx *c, struct sv_part *p,
                              struct sv_env *e, U1 set_imprecise)
{
    S2 i;

    for (i = 0; i < p->nvars; i++)
    {
        if (p->vars[i].modeled == 1 && p->vars[i].is_const == 0)
        {
            e->v[i] = sv_fresh (c, p, p->vars[i].name);
        }
    }
    if (set_imprecise == 1)
    {
        e->precise = 0;
    }
}

/* ------------------------------------------------------------------------- */
/* expressions                                                               */
/* ------------------------------------------------------------------------- */

static struct sv_val sv_val_unknown (void)
{
    struct sv_val v;

    v.ast = NULL;
    v.is_bool = 0;
    v.ok = 0;
    return (v);
}

static struct sv_val sv_val_bv (Z3_ast a)
{
    struct sv_val v;

    v.ast = a;
    v.is_bool = 0;
    v.ok = (U1) (a != NULL);
    return (v);
}

static struct sv_val sv_val_bool (Z3_ast a)
{
    struct sv_val v;

    v.ast = a;
    v.is_bool = 1;
    v.ok = (U1) (a != NULL);
    return (v);
}

static const char *sv_op_base (U1 *tok)
{
    S2 len;

    len = (S2) strlen ((const char *) tok);
    if (len >= 2 && is_operator_suffix (tok[len - 1]) == 1)
    {
        static char buf[8];

        /* typed operators such as +i, <=i or ==b carry a type suffix */
        if (len - 1 < (S2) sizeof (buf))
        {
            memcpy (buf, tok, (size_t) (len - 1));
            buf[len - 1] = '\0';
            return (buf);
        }
    }
    return ((const char *) tok);
}

static U1 sv_is_operator (U1 *tok)
{
    const char *b = sv_op_base (tok);

    if (strcmp (b, "+") == 0 || strcmp (b, "-") == 0 ||
        strcmp (b, "*") == 0 || strcmp (b, "/") == 0 ||
        strcmp (b, "%") == 0 || strcmp (b, "==") == 0 ||
        strcmp (b, "!=") == 0 || strcmp (b, "<") == 0 ||
        strcmp (b, "<=") == 0 || strcmp (b, ">") == 0 ||
        strcmp (b, ">=") == 0 || strcmp (b, "&") == 0 ||
        strcmp (b, "|") == 0 || strcmp (b, "&&") == 0 ||
        strcmp (b, "||") == 0)
    {
        return (1);
    }
    return (0);
}

static Z3_ast sv_lookup (struct sv_ctx *c, struct sv_part *p,
                         struct sv_env *e, U1 *name, U1 *precise)
{
    S2 i = sv_find_var (p, name);

    if (i < 0)
    {
        *precise = 0;
        return (sv_fresh (c, p, name));
    }
    if (p->vars[i].modeled == 0)
    {
        *precise = 0;
        return (sv_fresh (c, p, name));
    }
    if (e->v[i] == NULL)
    {
        e->v[i] = sv_fresh (c, p, name);
    }
    return (e->v[i]);
}

static struct sv_val sv_eval (struct sv_ctx *c, struct sv_part *p,
                              struct sv_env *e, S2 nt,
                              U1 toks[][MAXSTRLEN + 1],
                              S2 start, S2 end);

static void sv_apply_group (struct sv_ctx *c, struct sv_part *p,
                            struct sv_env *e, S2 nt,
                            U1 toks[][MAXSTRLEN + 1],
                            S2 start, S2 end);

static struct sv_val sv_apply_bin (struct sv_ctx *c, struct sv_part *p,
                                   struct sv_env *e, const char *op,
                                   struct sv_val l, struct sv_val r)
{
    Z3_ast a;
    Z3_ast b;
    S8 forced = 0;

    (void) p;

    if (l.ok == 0 || r.ok == 0)
    {
        return (sv_val_unknown ());
    }
    if (e->precise == 0)
    {
        forced = 0;
    }
    else
    {
        forced = 1;
    }

    if (strcmp (op, "==") == 0 || strcmp (op, "!=") == 0 ||
        strcmp (op, "<") == 0 || strcmp (op, "<=") == 0 ||
        strcmp (op, ">") == 0 || strcmp (op, ">=") == 0)
    {
        if (l.is_bool == 1 && r.is_bool == 1)
        {
            if (strcmp (op, "==") == 0)
                return (sv_val_bool (Z3_mk_iff (c->z3, l.ast, r.ast)));
            if (strcmp (op, "!=") == 0)
                return (sv_val_bool (Z3_mk_xor (c->z3, l.ast, r.ast)));
        }
        a = sv_to_bv (c, l);
        b = sv_to_bv (c, r);
        if (a == NULL || b == NULL)
        {
            return (sv_val_unknown ());
        }
        if (strcmp (op, "==") == 0) return (sv_val_bool (Z3_mk_eq (c->z3, a, b)));
        if (strcmp (op, "!=") == 0) return (sv_val_bool (Z3_mk_not (c->z3, Z3_mk_eq (c->z3, a, b))));
        if (strcmp (op, "<") == 0) return (sv_val_bool (Z3_mk_bvslt (c->z3, a, b)));
        if (strcmp (op, "<=") == 0) return (sv_val_bool (Z3_mk_bvsle (c->z3, a, b)));
        if (strcmp (op, ">") == 0) return (sv_val_bool (Z3_mk_bvsgt (c->z3, a, b)));
        return (sv_val_bool (Z3_mk_bvsge (c->z3, a, b)));
    }

    if (strcmp (op, "&&") == 0 || strcmp (op, "||") == 0)
    {
        a = sv_to_bool (c, l);
        b = sv_to_bool (c, r);
        if (a == NULL || b == NULL)
        {
            return (sv_val_unknown ());
        }
        if (strcmp (op, "&&") == 0)
            return (sv_val_bool (Z3_mk_and (c->z3, 2, (Z3_ast[]) { a, b })));
        return (sv_val_bool (Z3_mk_or (c->z3, 2, (Z3_ast[]) { a, b })));
    }

    a = sv_to_bv (c, l);
    b = sv_to_bv (c, r);
    if (a == NULL || b == NULL)
    {
        return (sv_val_unknown ());
    }

    if (strcmp (op, "+") == 0) return (sv_val_bv (Z3_mk_bvadd (c->z3, a, b)));
    if (strcmp (op, "-") == 0) return (sv_val_bv (Z3_mk_bvsub (c->z3, a, b)));
    if (strcmp (op, "*") == 0) return (sv_val_bv (Z3_mk_bvmul (c->z3, a, b)));
    if (strcmp (op, "&") == 0) return (sv_val_bv (Z3_mk_bvand (c->z3, a, b)));
    if (strcmp (op, "|") == 0) return (sv_val_bv (Z3_mk_bvor (c->z3, a, b)));

    if (strcmp (op, "/") == 0 || strcmp (op, "%") == 0)
    {
        if (forced == 1)
        {
            /* A divisor which is zero on every feasible path is a real bug.
             * The interval analysis only knows local ranges, the solver
             * knows the path constraints. The result is only modeled while
             * the divisor is provably nonzero. */
            Z3_ast zero = Z3_mk_eq (c->z3, b, sv_num (c, 0));
            Z3_ast nz = Z3_mk_not (c->z3, zero);
            S2 sat_nz = sv_check (c, e->pc, nz);

            if (sat_nz == 0)
            {
                if (c->truncated == 0 && warn_line_marked (c->cur_line) == 0)
                {
                    linter_warnings++;
                    warn_line_mark (c->cur_line);
                    printf ("linter error: solver: %s by zero on every feasible path!\n",
                            strcmp (op, "%") == 0 ? "modulo" : "division");
                    printf (" > %s\n", c->cur_text);
                }
                e->precise = 0;
            }
            else
            {
                U1 safe = (U1) (sat_nz == 1 && sv_check (c, e->pc, zero) == 0);

                if (safe == 1)
                {
                    /* INT64_MIN / -1 and INT64_MIN % -1 overflow in C and
                     * L1VM, so the result is not defined either. */
                    Z3_ast ov = Z3_mk_and (c->z3, 2, (Z3_ast[]) {
                        Z3_mk_eq (c->z3, a, sv_num (c, LLONG_MIN)),
                        Z3_mk_eq (c->z3, b, sv_num (c, -1)) });
                    if (sv_check (c, e->pc, ov) != 0)
                    {
                        safe = 0;
                    }
                }
                if (safe == 0)
                {
                    /* A zero divisor or an overflowing division is possible
                     * or unknown, so the value is undefined. */
                    e->precise = 0;
                }
            }
        }
        if (strcmp (op, "/") == 0) return (sv_val_bv (Z3_mk_bvsdiv (c->z3, a, b)));
        return (sv_val_bv (Z3_mk_bvsrem (c->z3, a, b)));
    }

    if (strcmp (op, "<|") == 0 || strcmp (op, ">|") == 0)
    {
        if (forced == 1)
        {
            Z3_ast lo = Z3_mk_bvsge (c->z3, b, sv_num (c, 0));
            Z3_ast hi = Z3_mk_bvsle (c->z3, b, sv_num (c, 63));
            Z3_ast in_range = Z3_mk_and (c->z3, 2, (Z3_ast[]) { lo, hi });
            S2 sat_ok = sv_check (c, e->pc, in_range);

            if (sat_ok == 0)
            {
                if (c->truncated == 0 && warn_line_marked (c->cur_line) == 0)
                {
                    linter_warnings++;
                    warn_line_mark (c->cur_line);
                    printf ("linter error: solver: shift amount is outside [0, 63] on every feasible path!\n");
                    printf (" > %s\n", c->cur_text);
                }
                e->precise = 0;
            }
            else if (sat_ok != 1 ||
                     sv_check (c, e->pc, Z3_mk_not (c->z3, in_range)) != 0)
            {
                /* An out-of-range shift is possible or unknown, so the
                 * shifted value is undefined. */
                e->precise = 0;
            }
        }
        if (strcmp (op, "<|") == 0)
            return (sv_val_bv (Z3_mk_bvshl (c->z3, a, b)));
        return (sv_val_bv (Z3_mk_bvashr (c->z3, a, b)));
    }

    return (sv_val_unknown ());
}

static struct sv_val sv_eval (struct sv_ctx *c, struct sv_part *p,
                              struct sv_env *e, S2 nt,
                              U1 toks[][MAXSTRLEN + 1],
                              S2 start, S2 end)
{
    S2 i;
    S2 op_at = -1;
    S2 op_width = 1;
    S2 r_start;
    S2 r_end;
    S2 depth;
    U1 op_buf[8];
    const char *op;
    struct sv_val l;
    struct sv_val r;

    if (start < 0 || end < 0 || start > end || end >= nt)
    {
        return (sv_val_unknown ());
    }

    /* strip exactly one enclosing group */
    if (sv_is (toks[start], "(") == 1)
    {
        S2 close = find_close_paren (nt, toks, start);
        if (close == end)
        {
            return (sv_eval (c, p, e, nt, toks, start + 1, end - 1));
        }
    }

    /* scan for the last top level operator */
    depth = 0;
    for (i = start; i <= end; i++)
    {
        if (sv_is (toks[i], "(") == 1)
        {
            S2 close = find_close_paren (nt, toks, i);
            if (close < 0 || close > end)
            {
                return (sv_val_unknown ());
            }
            i = close;
            continue;
        }
        if (sv_is (toks[i], "[") == 1)
        {
            /* array subscript: unmodeled */
            return (sv_val_unknown ());
        }
        if (i + 1 <= end && sv_is (toks[i], "<") == 1 &&
            sv_is (toks[i + 1], "|") == 1)
        {
            op_at = i;
            op_width = 2;
            i++;
            continue;
        }
        if (i + 1 <= end && sv_is (toks[i], ">") == 1 &&
            sv_is (toks[i + 1], "|") == 1)
        {
            op_at = i;
            op_width = 2;
            i++;
            continue;
        }
        if (sv_is_operator (toks[i]) == 1)
        {
            op_at = i;
            op_width = 1;
        }
    }
    (void) depth;

    if (op_at < 0)
    {
        if (start == end)
        {
            if (is_number_token (toks[start]) == 1)
            {
                char *endptr = NULL;
                long long v;

                v = strtoll ((const char *) toks[start], &endptr, 10);
                if (endptr != NULL && *endptr == '\0')
                {
                    return (sv_val_bv (sv_num (c, (S8) v)));
                }
                return (sv_val_unknown ());
            }
            if (is_ident_start (toks[start][0]) == 1)
            {
                U1 keep = e->precise;
                Z3_ast a = sv_lookup (c, p, e, toks[start], &e->precise);

                if (keep == 1 && e->precise == 0)
                {
                    /* reading an unmodeled value does not make the path
                     * wrong, but no proof may be based on it. */
                }
                return (sv_val_bv (a));
            }
        }
        return (sv_val_unknown ());
    }

    if (op_width == 2)
    {
        op = sv_is (toks[op_at], "<") == 1 ? "<|" : ">|";
    }
    else
    {
        op = sv_op_base (toks[op_at]);
    }
    strncpy ((char *) op_buf, op, sizeof (op_buf) - 1);
    op_buf[sizeof (op_buf) - 1] = '\0';
    op = (const char *) op_buf;

    if (op_at + op_width - 1 == end)
    {
        /* postfix: left right op */
        r_end = op_at - 1;
        if (r_end < start)
        {
            return (sv_val_unknown ());
        }
        if (sv_is (toks[r_end], ")") == 1)
        {
            r_start = find_open_paren (nt, toks, r_end);
            if (r_start < 0)
            {
                return (sv_val_unknown ());
            }
        }
        else
        {
            r_start = r_end;
        }
        if (r_start <= start)
        {
            return (sv_val_unknown ());
        }
        l = sv_eval (c, p, e, nt, toks, start, r_start - 1);
        r = sv_eval (c, p, e, nt, toks, r_start, r_end);
    }
    else if (op_at > start && op_at + op_width - 1 < end)
    {
        /* infix: left op right */
        l = sv_eval (c, p, e, nt, toks, start, op_at - 1);
        r = sv_eval (c, p, e, nt, toks, op_at + op_width, end);
    }
    else
    {
        return (sv_val_unknown ());
    }

    return (sv_apply_bin (c, p, e, op, l, r));
}

/* find the condition token range of an "(EXPR if)" style line. The actual
 * condition may be a comparison group or a variable which was filled by an
 * assignment group earlier on the same line, for example
 *
 *   (((x y <) f =) f if)
 */
static struct sv_val sv_line_condition (struct sv_ctx *c, struct sv_part *p,
                                        struct sv_env *e, S2 nt,
                                        U1 toks[][MAXSTRLEN + 1],
                                        const char *marker)
{
    S2 m = -1;
    S2 i;

    for (i = 0; i < nt; i++)
    {
        if (sv_is (toks[i], marker) == 1)
        {
            m = i;
        }
    }
    if (m <= 0 && strcmp (marker, "if") == 0)
    {
        for (i = 0; i < nt; i++)
        {
            if (sv_is (toks[i], "if+") == 1)
            {
                m = i;
            }
        }
    }
    if (m <= 0 && strcmp (marker, "while") == 0)
    {
        /* a bare (while) belongs to a do-loop without condition */
    }
    if (m <= 0)
    {
        e->precise = 0;
        return (sv_val_unknown ());
    }

    if (sv_is (toks[m - 1], ")") == 1)
    {
        S2 open = find_open_paren (nt, toks, m - 1);

        if (open >= 0)
        {
            return (sv_eval (c, p, e, nt, toks, open, m - 1));
        }
    }
    else if (is_ident_start (toks[m - 1][0]) == 1)
    {
        /* an assignment group may be written before the condition variable */
        if (m >= 3 && sv_is (toks[m - 2], ")") == 1)
        {
            S2 open = find_open_paren (nt, toks, m - 2);

            if (open >= 0)
            {
                sv_apply_group (c, p, e, nt, toks, open, m - 2);
            }
        }
        {
            U1 keep = e->precise;
            Z3_ast a = sv_lookup (c, p, e, toks[m - 1], &e->precise);
            (void) keep;
            if (a == NULL)
            {
                return (sv_val_unknown ());
            }
            return (sv_val_bv (a));
        }
    }

    return (sv_eval (c, p, e, nt, toks, m - 1, m - 1));
}

/* ------------------------------------------------------------------------- */
/* statements                                                                */
/* ------------------------------------------------------------------------- */

static void sv_report (struct sv_ctx *c, const char *msg)
{
    if (c->truncated == 1)
    {
        /* The model is incomplete, so no proof is trustworthy anymore. */
        return;
    }
    if (warn_line_marked (c->cur_line) == 1)
    {
        return;
    }
    linter_warnings++;
    warn_line_mark (c->cur_line);
    printf ("linter error: solver: %s\n", msg);
    printf (" > %s\n", c->cur_text);
}

static void sv_apply_group (struct sv_ctx *c, struct sv_part *p,
                            struct sv_env *e, S2 nt,
                            U1 toks[][MAXSTRLEN + 1],
                            S2 start, S2 end)
{
    S2 i;
    S2 op = -1;
    S2 dest;
    struct sv_val v;

    if (start < 0 || end < 0 || start > end || end >= nt)
    {
        return;
    }
    if (sv_is (toks[start], "(") == 1)
    {
        S2 close = find_close_paren (nt, toks, start);
        if (close == end)
        {
            start++;
            end--;
        }
    }
    if (start > end)
    {
        return;
    }

    for (i = start; i <= end; i++)
    {
        if (sv_is (toks[i], ":=") == 1 || sv_is (toks[i], "=") == 1)
        {
            op = i;
        }
    }
    if (op < 0 || op <= start)
    {
        return;
    }

    dest = op - 1;
    if (sv_is (toks[dest], "]") == 1)
    {
        /* Array or pointer element write. It may alias any scalar whose
         * address was taken, so later scalar proofs are no longer sound. */
        e->precise = 0;
        return;
    }
    if (is_ident_start (toks[dest][0]) == 0)
    {
        return;
    }

    {
        S2 expr_start = start;
        S2 expr_end = op - 2;
        S2 vi;

        if (expr_end < expr_start)
        {
            /* no right hand side */
            return;
        }
        v = sv_eval (c, p, e, nt, toks, expr_start, expr_end);
        if (v.ok == 0)
        {
            vi = sv_find_var (p, toks[dest]);
            if (vi >= 0 && p->vars[vi].modeled == 1)
            {
                e->v[vi] = sv_fresh (c, p, toks[dest]);
            }
            e->precise = 0;
            return;
        }
        vi = sv_find_var (p, toks[dest]);
        if (vi < 0)
        {
            /* unknown (for example global) destination: not modeled */
            return;
        }
        if (p->vars[vi].is_const == 1)
        {
            /* the main linter reports assignment to constants */
            return;
        }
        if (p->vars[vi].modeled == 0)
        {
            e->precise = 0;
            return;
        }
        {
            Z3_ast bv = sv_to_bv (c, v);

            if (bv == NULL)
            {
                e->v[vi] = sv_fresh (c, p, toks[dest]);
                e->precise = 0;
                return;
            }
            e->v[vi] = bv;
        }
    }
}

/* The shared tokenizer splits hyphenated type names such as "const-int64"
 * into "const", "-i", "nt64". Reassemble the type name and return the index
 * of the size token, or -1 when the line is not a declaration.
 */
static S2 sv_decl_header (S2 nt, U1 toks[][MAXSTRLEN + 1], U1 *type_out)
{
    S2 ti = 2;
    S2 len = 0;

    if (nt < 4 || sv_is (toks[0], "(") == 0 || sv_is (toks[1], "set") == 0)
    {
        return (-1);
    }
    type_out[0] = '\0';
    while (ti < nt && is_number_token (toks[ti]) == 0)
    {
        S2 tlen = (S2) strlen ((const char *) toks[ti]);

        if (len + tlen >= MAXSTRLEN)
        {
            return (-1);
        }
        strcpy ((char *) type_out + len, (const char *) toks[ti]);
        len += tlen;
        ti++;
    }
    if (ti >= nt || is_number_token (toks[ti]) == 0)
    {
        return (-1);
    }
    return (ti);
}

static void sv_apply_stmt (struct sv_ctx *c, struct sv_part *p,
                           struct sv_env *e, S2 line_idx)
{
    U1 *line = p->lines[line_idx];
    S2 nt;
    S2 i;

    nt = tokenize_line (line, sv_tokens, SV_MAXTOK);
    if (nt == 0)
    {
        return;
    }
    c->cur_line = p->line_nums[line_idx];
    c->cur_text = line;

    /* declaration */
    if (nt > 3 && sv_is (sv_tokens[1], "set") == 1)
    {
        U1 type[MAXSTRLEN + 1];
        S2 size_idx;
        S2 name_idx;
        S2 vi;
        S8 size = 1;

        size_idx = sv_decl_header (nt, sv_tokens, type);
        if (size_idx < 0)
        {
            e->precise = 0;
            return;
        }
        name_idx = size_idx + 1;
        if (name_idx >= nt || is_ident_start (sv_tokens[name_idx][0]) == 0)
        {
            /* sv_scan_decls pre-registers every declaration, so the
             * environment is already sized for the variable table. A
             * declaration which is not in that table (for example a
             * syntactically odd line) is left unmodelled. */
            e->precise = 0;
            return;
        }
        if (is_number_token (sv_tokens[size_idx]) == 1)
        {
            size = (S8) strtoll ((const char *) sv_tokens[size_idx], NULL, 10);
        }
        vi = sv_find_var (p, sv_tokens[name_idx]);
        if (vi < 0)
        {
            e->precise = 0;
            return;
        }
        if (size > 1 || strstr ((const char *) type, "string") != NULL)
        {
            p->vars[vi].modeled = 0;
            e->precise = 0;
            return;
        }
        if (name_idx + 1 <= nt - 2)
        {
            struct sv_val v = sv_eval (c, p, e, nt, sv_tokens, name_idx + 1, nt - 2);
            if (v.ok == 0)
            {
                e->v[vi] = sv_fresh (c, p, sv_tokens[name_idx]);
                e->precise = 0;
                return;
            }
            e->v[vi] = sv_to_bv (c, v);
            if (e->v[vi] == NULL)
            {
                e->v[vi] = sv_fresh (c, p, sv_tokens[name_idx]);
                e->precise = 0;
            }
        }
        else
        {
            e->v[vi] = sv_fresh (c, p, sv_tokens[name_idx]);
        }
        return;
    }

    /* stpop family: the popped values are unconstrained inputs */
    for (i = 0; i < nt; i++)
    {
        if (sv_is (sv_tokens[i], "stpop") == 1 ||
            sv_is (sv_tokens[i], "stpopi") == 1 ||
            sv_is (sv_tokens[i], "stpopd") == 1 ||
            sv_is (sv_tokens[i], "stpopb") == 1)
        {
            S2 k;

            for (k = 1; k < i; k++)
            {
                S2 vi;

                if (is_ident_start (sv_tokens[k][0]) == 1 &&
                    token_is_keyword (sv_tokens[k]) == 0)
                {
                    vi = sv_find_var (p, sv_tokens[k]);
                    if (vi >= 0 && p->vars[vi].modeled == 1)
                    {
                        e->v[vi] = sv_fresh (c, p, sv_tokens[k]);
                    }
                }
            }
            return;
        }
    }

    /* integer intrinsics: unknown effects (exit terminates the path) */
    for (i = 0; i < nt; i++)
    {
        if (sv_is (sv_tokens[i], "intr0") == 1 ||
            sv_is (sv_tokens[i], "intr1") == 1)
        {
            if (i >= 2 && is_number_token (sv_tokens[1]) == 1)
            {
                S2 op = (S2) atoi ((const char *) sv_tokens[1]);
                if (op == 255)
                {
                    e->esc = SV_ESC_RETURN;
                }
            }
            sv_env_havoc_all (c, p, e, 1);
            return;
        }
    }

    /* assignment (including "(a b +) x := ..." and "(x 1 =)") */
    {
        S2 op = -1;

        for (i = 0; i < nt; i++)
        {
            if (sv_is (sv_tokens[i], ":=") == 1 ||
                sv_is (sv_tokens[i], "=") == 1)
            {
                op = i;
            }
        }
        if (op > 1)
        {
            sv_apply_group (c, p, e, nt, sv_tokens, 0, nt - 1);
            return;
        }
    }

    /* pointer instruction: destination becomes an unknown value */
    for (i = 1; i + 1 < nt; i++)
    {
        if (sv_is (sv_tokens[i], "pointer") == 1)
        {
            S2 vi = sv_find_var (p, sv_tokens[i - 1]);

            if (vi >= 0 && p->vars[vi].modeled == 1 &&
                p->vars[vi].is_const == 0)
            {
                e->v[vi] = sv_fresh (c, p, sv_tokens[i - 1]);
            }
            return;
        }
    }

    /* function / library call */
    {
        S2 colon = -1;
        U1 *callee = NULL;
        U1 is_call = 0;

        (void) colon;

        for (i = 0; i < nt; i++)
        {
            if (sv_is (sv_tokens[i], "!") == 1 ||
                sv_is (sv_tokens[i], "call") == 1)
            {
                is_call = 1;
            }
            if (sv_is (sv_tokens[i], ":") == 1 && i + 1 < nt &&
                is_ident_start (sv_tokens[i + 1][0]) == 1)
            {
                colon = i;
                callee = sv_tokens[i + 1];
            }
        }
        if (is_call == 1 && callee != NULL)
        {
            const char *nm = (const char *) callee;

            if (strstr (nm, "print") != NULL || strcmp (nm, "exit") == 0 ||
                strcmp (nm, "check_pointer") == 0 ||
                strcmp (nm, "pointer_type") == 0 ||
                strcmp (nm, "get_size") == 0)
            {
                return;
            }
            /* A call may change any non-const variable. Havocing turns the
             * state into a sound over-approximation, but not into a precise
             * model, so no proof is based on it. */
            sv_env_havoc_all (c, p, e, 1);
            return;
        }
    }

    /* standalone push instructions have no effect on scalar values */
    for (i = 0; i < nt; i++)
    {
        if (sv_is (sv_tokens[i], "stpush") == 1 ||
            sv_is (sv_tokens[i], "stpushi") == 1 ||
            sv_is (sv_tokens[i], "stpushd") == 1 ||
            sv_is (sv_tokens[i], "stpushb") == 1 ||
            sv_is (sv_tokens[i], "loadreg") == 1 ||
            sv_is (sv_tokens[i], "rts") == 1)
        {
            return;
        }
    }

    /* old style calls such as "print_s (str)" and helper calls without the
     * colon notation. Anything unknown may write memory. */
    if (nt > 2 && is_ident_start (sv_tokens[1][0]) == 1 &&
        token_is_keyword (sv_tokens[1]) == 0)
    {
        if (strstr ((const char *) sv_tokens[1], "print") != NULL)
        {
            return;
        }
        e->precise = 0;
    }
}

/* ------------------------------------------------------------------------- */
/* annotation expressions                                                    */
/* ------------------------------------------------------------------------- */

static struct sv_val sv_annotation (struct sv_ctx *c, struct sv_part *p,
                                    struct sv_env *e, S2 line_idx,
                                    const char *keyword)
{
    U1 *line = p->lines[line_idx];
    S2 nt;
    S2 i;
    S2 start;
    S2 end;

    nt = tokenize_line (line, sv_tokens, SV_MAXTOK);
    for (i = 0; i < nt; i++)
    {
        if (sv_is (sv_tokens[i], keyword) == 1)
        {
            break;
        }
    }
    if (i >= nt)
    {
        return (sv_val_unknown ());
    }
    start = i + 1;
    end = nt - 1;
    if (start > end)
    {
        return (sv_val_unknown ());
    }
    if (sv_is (sv_tokens[end], ")") == 1)
    {
        S2 open = find_open_paren (nt, sv_tokens, end);
        if (open == i - 1)
        {
            /* drop the closing ')' of the outer "(assert ...)" group */
            end--;
        }
    }
    if (start > end)
    {
        return (sv_val_unknown ());
    }
    return (sv_eval (c, p, e, nt, sv_tokens, start, end));
}

/* ------------------------------------------------------------------------- */
/* control flow tree                                                         */
/* ------------------------------------------------------------------------- */

static struct sv_node *sv_new_node (S2 kind, S2 line)
{
    struct sv_node *n = (struct sv_node *) calloc (1, sizeof (struct sv_node));

    if (n == NULL)
    {
        return (NULL);
    }
    n->kind = kind;
    n->line = line;
    n->cond_line = -1;
    return (n);
}

static struct sv_node *sv_parse_block (struct sv_ctx *c, struct sv_part *p,
                                       S2 *pos, S2 stops)
{
    struct sv_node *first = NULL;
    struct sv_node *last = NULL;

    (void) stops;
    while (*pos < p->count)
    {
        S2 kind = sv_classify (p->lines[*pos], NULL);
        struct sv_node *n;

        if (kind == SVSTOP_ELSE || kind == SVSTOP_ENDIF ||
            kind == SVSTOP_WHILE || kind == SVSTOP_NEXT ||
            kind == SVSTOP_CASE || kind == SVSTOP_SWITCHEND)
        {
            break;
        }
        if (kind == 0 || (kind >= SVSTOP_ELSE && kind <= SVSTOP_SWITCHEND))
        {
            (*pos)++;
            continue;
        }

        switch (kind)
        {
            case SVN_IF:
            {
                S2 here = *pos;

                n = sv_new_node (SVN_IF, here);
                if (n == NULL) { c->truncated = 1; return (first); }
                (*pos)++;
                n->body = sv_parse_block (c, p, pos, SVSTOP_ELSE | SVSTOP_ENDIF);
                if (*pos < p->count &&
                    sv_classify (p->lines[*pos], NULL) == SVSTOP_ELSE)
                {
                    (*pos)++;
                    n->else_body = sv_parse_block (c, p, pos, SVSTOP_ENDIF);
                }
                if (*pos < p->count &&
                    sv_classify (p->lines[*pos], NULL) == SVSTOP_ENDIF)
                {
                    (*pos)++;
                }
                break;
            }
            case SVN_DO:
            {
                S2 here = *pos;

                n = sv_new_node (SVN_DO, here);
                if (n == NULL) { c->truncated = 1; return (first); }
                (*pos)++;
                n->body = sv_parse_block (c, p, pos, SVSTOP_WHILE);
                if (*pos < p->count &&
                    sv_classify (p->lines[*pos], NULL) == SVSTOP_WHILE)
                {
                    n->cond_line = *pos;
                    (*pos)++;
                }
                break;
            }
            case SVN_FOR:
            {
                S2 here = *pos;

                /* the for-loop marker */
                if (sv_line_has (p->lines[here], "for-loop") == 1 ||
                    sv_line_has (p->lines[here], "for - loop") == 1 ||
                    (here + 2 < p->count &&
                     sv_line_has (p->lines[here], "(for - loop)") == 1))
                {
                    n = sv_new_node (SVN_FOR, here);
                    if (n == NULL) { c->truncated = 1; return (first); }
                    (*pos)++;
                    if (*pos < p->count &&
                        sv_classify (p->lines[*pos], NULL) == SVN_FOR)
                    {
                        n->cond_line = *pos;
                        (*pos)++;
                    }
                    n->body = sv_parse_block (c, p, pos, SVSTOP_NEXT);
                    if (*pos < p->count &&
                        sv_classify (p->lines[*pos], NULL) == SVSTOP_NEXT)
                    {
                        (*pos)++;
                    }
                }
                else
                {
                    /* a condition line without a marker: ordinary statement */
                    n = sv_new_node (SVN_STMT, here);
                    if (n == NULL) { c->truncated = 1; return (first); }
                    (*pos)++;
                }
                break;
            }
            case SVN_SWITCH:
            {
                S2 here = *pos;

                n = sv_new_node (SVN_SWITCH, here);
                if (n == NULL) { c->truncated = 1; return (first); }
                (*pos)++;
                n->cases = (struct sv_case *) calloc (SV_MAX_CASES, sizeof (struct sv_case));
                if (n->cases == NULL) { c->truncated = 1; return (first); }
                while (*pos < p->count)
                {
                    S2 cls = sv_classify (p->lines[*pos], NULL);

                    if (cls == SVSTOP_SWITCHEND)
                    {
                        (*pos)++;
                        break;
                    }
                    if (cls == SVSTOP_CASE)
                    {
                        if (n->ncases >= SV_MAX_CASES)
                        {
                            c->truncated = 1;
                            (*pos)++;
                            continue;
                        }
                        n->cases[n->ncases].line = *pos;
                        (*pos)++;
                        n->cases[n->ncases].body = sv_parse_block (c, p, pos,
                                                                   SVSTOP_CASE | SVSTOP_SWITCHEND);
                        n->ncases++;
                    }
                    else
                    {
                        (*pos)++;
                    }
                }
                break;
            }
            default:
                n = sv_new_node (kind, *pos);
                if (n == NULL) { c->truncated = 1; return (first); }
                (*pos)++;
                break;
        }

        if (n != NULL)
        {
            if (first == NULL)
            {
                first = n;
            }
            else
            {
                last->next = n;
            }
            last = n;
        }
    }
    return (first);
}

/* ------------------------------------------------------------------------- */
/* symbolic execution                                                        */
/* ------------------------------------------------------------------------- */

static struct sv_list sv_exec_block (struct sv_ctx *c, struct sv_part *p,
                                     struct sv_node *first, struct sv_list in);

static U1 sv_branch_possible (struct sv_ctx *c, struct sv_env *e,
                              Z3_ast cond, U1 want_true)
{
    Z3_ast q = want_true == 1 ? cond : sv_not (c, cond);
    Z3_ast pc = sv_and (c, e->pc, q);
    S2 r = sv_check (c, pc, NULL);

    if (r == 0)
    {
        return (0);
    }
    return (1);
}

static void sv_collect_paths (struct sv_ctx *c, struct sv_list *body_out,
                              struct sv_list *cont, struct sv_list *out)
{
    S2 i;

    for (i = 0; i < body_out->n; i++)
    {
        struct sv_env *e = body_out->e[i];

        if (e->esc == SV_ESC_NONE)
        {
            sv_list_add (c, cont, e);
        }
        else
        {
            sv_list_add (c, out, e);
        }
    }
    free (body_out->e);
    body_out->e = NULL;
    body_out->n = 0;
    body_out->cap = 0;
}

static void sv_exec_if (struct sv_ctx *c, struct sv_part *p,
                        struct sv_node *n, struct sv_env *e,
                        struct sv_list *cont, struct sv_list *out)
{
    S2 nt;
    struct sv_val v;
    Z3_ast cond = NULL;
    U1 ok = 0;
    U1 then_ok = 1;
    U1 else_ok = 1;

    nt = tokenize_line (p->lines[n->line], sv_tokens, SV_MAXTOK);
    c->cur_line = p->line_nums[n->line];
    c->cur_text = p->lines[n->line];
    v = sv_line_condition (c, p, e, nt, sv_tokens, "if");
    if (v.ok == 1)
    {
        cond = sv_to_bool (c, v);
        ok = (U1) (cond != NULL);
    }
    if (ok == 0)
    {
        e->precise = 0;
    }

    if (ok == 1 && e->precise == 1 && c->truncated == 0)
    {
        then_ok = sv_branch_possible (c, e, cond, 1);
        else_ok = sv_branch_possible (c, e, cond, 0);

        if (then_ok == 0 && else_ok == 1)
        {
            sv_report (c, "condition is always false, the then branch is dead code");
        }
        else if (then_ok == 1 && else_ok == 0)
        {
            sv_report (c, "condition is always true, the else branch is dead code");
        }
    }

    if (then_ok == 0 && else_ok == 0)
    {
        return;
    }

    /* then branch */
    if (then_ok == 1)
    {
        struct sv_env *te = sv_env_clone (c, p, e);
        struct sv_list branch;
        struct sv_list body_out;

        if (te != NULL)
        {
            if (ok == 1)
            {
                te->pc = sv_and (c, e->pc, cond);
            }
            else
            {
                te->precise = 0;
            }
            te->esc = SV_ESC_NONE;
            memset (&branch, 0, sizeof (branch));
            sv_list_add (c, &branch, te);
            body_out = sv_exec_block (c, p, n->body, branch);
            sv_collect_paths (c, &body_out, cont, out);
        }
    }

    /* else branch */
    if (else_ok == 1)
    {
        struct sv_env *ee = sv_env_clone (c, p, e);
        struct sv_list branch;
        struct sv_list body_out;

        if (ee != NULL)
        {
            if (ok == 1)
            {
                ee->pc = sv_and (c, e->pc, sv_not (c, cond));
            }
            else
            {
                ee->precise = 0;
            }
            ee->esc = SV_ESC_NONE;
            memset (&branch, 0, sizeof (branch));
            sv_list_add (c, &branch, ee);
            body_out = sv_exec_block (c, p, n->else_body, branch);
            sv_collect_paths (c, &body_out, cont, out);
        }
    }
}

static U1 sv_loop_condition (struct sv_ctx *c, struct sv_part *p,
                             struct sv_node *n, struct sv_env *e,
                             Z3_ast *cond_out)
{
    S2 nt;
    struct sv_val v;
    Z3_ast cond = NULL;
    U1 ok = 0;

    *cond_out = NULL;
    if (n->cond_line < 0 || n->cond_line >= p->count)
    {
        e->precise = 0;
        return (0);
    }
    nt = tokenize_line (p->lines[n->cond_line], sv_tokens, SV_MAXTOK);
    c->cur_line = p->line_nums[n->cond_line];
    c->cur_text = p->lines[n->cond_line];
    v = sv_line_condition (c, p, e, nt, sv_tokens,
                           n->kind == SVN_DO ? "while" : "for");
    if (v.ok == 1)
    {
        cond = sv_to_bool (c, v);
        ok = (U1) (cond != NULL);
    }
    if (ok == 0)
    {
        e->precise = 0;
        return (0);
    }
    *cond_out = cond;
    return (1);
}

static void sv_exec_loop (struct sv_ctx *c, struct sv_part *p,
                          struct sv_node *n, struct sv_env *e,
                          struct sv_list *cont, struct sv_list *out)
{
    struct sv_list current;
    struct sv_list final;
    S2 iter;

    memset (&current, 0, sizeof (current));
    memset (&final, 0, sizeof (final));
    {
        struct sv_env *copy = sv_env_clone (c, p, e);

        if (copy == NULL)
        {
            return;
        }
        sv_list_add (c, &current, copy);
    }

    for (iter = 0; iter < c->unroll && current.n > 0 && c->truncated == 0; iter++)
    {
        struct sv_list body_out;
        struct sv_list next;
        S2 i;

        memset (&next, 0, sizeof (next));

        if (n->kind == SVN_FOR)
        {
            struct sv_list body_in;

            memset (&body_in, 0, sizeof (body_in));

            /* A for-loop checks the condition before the body. */
            for (i = 0; i < current.n; i++)
            {
                struct sv_env *be = current.e[i];
                Z3_ast cond = NULL;

                if (be->esc != SV_ESC_NONE)
                {
                    sv_list_add (c, &final, be);
                    continue;
                }
                if (sv_loop_condition (c, p, n, be, &cond) == 0 ||
                    be->precise == 0 || c->truncated == 1)
                {
                    struct sv_env *cp = sv_env_clone (c, p, be);

                    if (cp != NULL)
                    {
                        cp->precise = 0;
                        sv_list_add (c, &final, cp);
                    }
                    continue;
                }
                {
                    U1 t = sv_branch_possible (c, be, cond, 1);
                    U1 f = sv_branch_possible (c, be, cond, 0);

                    if (iter == 0 && t == 0 && f == 1 && be->precise == 1)
                    {
                        sv_report (c, "loop condition is always false, the loop body is dead code");
                    }
                    if (t == 1)
                    {
                        struct sv_env *tc = sv_env_clone (c, p, be);

                        if (tc != NULL)
                        {
                            tc->pc = sv_and (c, be->pc, cond);
                            sv_list_add (c, &body_in, tc);
                        }
                    }
                    if (f == 1)
                    {
                        struct sv_env *fc = sv_env_clone (c, p, be);

                        if (fc != NULL)
                        {
                            fc->pc = sv_and (c, be->pc, sv_not (c, cond));
                            sv_list_add (c, &final, fc);
                        }
                    }
                }
            }
            free (current.e);
            current = body_in;
            if (current.n == 0)
            {
                break;
            }
        }

        body_out = sv_exec_block (c, p, n->body, current);
        memset (&current, 0, sizeof (current));

        for (i = 0; i < body_out.n; i++)
        {
            struct sv_env *be = body_out.e[i];

            if (be->esc == SV_ESC_BREAK)
            {
                be->esc = SV_ESC_NONE;
                sv_list_add (c, &final, be);
            }
            else if (be->esc != SV_ESC_NONE)
            {
                sv_list_add (c, &final, be);
            }
            else if (n->kind == SVN_DO)
            {
                Z3_ast cond = NULL;

                /* A do-loop checks the condition after the body. */
                if (sv_loop_condition (c, p, n, be, &cond) == 0 ||
                    be->precise == 0 || c->truncated == 1)
                {
                    struct sv_env *cp = sv_env_clone (c, p, be);

                    if (cp != NULL)
                    {
                        cp->precise = 0;
                        sv_list_add (c, &final, cp);
                    }
                }
                else
                {
                    U1 t = sv_branch_possible (c, be, cond, 1);
                    U1 f = sv_branch_possible (c, be, cond, 0);

                    if (t == 1)
                    {
                        struct sv_env *tc = sv_env_clone (c, p, be);

                        if (tc != NULL)
                        {
                            tc->pc = sv_and (c, be->pc, cond);
                            sv_list_add (c, &next, tc);
                        }
                    }
                    if (f == 1)
                    {
                        struct sv_env *fc = sv_env_clone (c, p, be);

                        if (fc != NULL)
                        {
                            fc->pc = sv_and (c, be->pc, sv_not (c, cond));
                            sv_list_add (c, &final, fc);
                        }
                    }
                }
            }
            else
            {
                sv_list_add (c, &next, be);
            }
        }
        free (body_out.e);
        current = next;
    }

    /* After the unroll bound the remaining state is unknown. */
    {
        S2 i;

        for (i = 0; i < current.n; i++)
        {
            sv_env_havoc_all (c, p, current.e[i], 1);
            sv_list_add (c, &final, current.e[i]);
        }
        free (current.e);
    }

    /* split break/return/jump results from normal continuation */
    {
        S2 i;

        for (i = 0; i < final.n; i++)
        {
            if (final.e[i]->esc == SV_ESC_NONE)
            {
                sv_list_add (c, cont, final.e[i]);
            }
            else
            {
                sv_list_add (c, out, final.e[i]);
            }
        }
        free (final.e);
    }
}

static void sv_exec_switch (struct sv_ctx *c, struct sv_part *p,
                            struct sv_node *n, struct sv_env *e,
                            struct sv_list *cont, struct sv_list *out)
{
    struct sv_val subject;
    S2 nt;
    S2 ci;
    U1 subject_ok = 0;
    Z3_ast subject_ast = NULL;
    Z3_ast prev = NULL;

    nt = tokenize_line (p->lines[n->cases[0].line], sv_tokens, SV_MAXTOK);
    c->cur_line = p->line_nums[n->cases[0].line];
    c->cur_text = p->lines[n->cases[0].line];
    subject = sv_eval (c, p, e, nt, sv_tokens, 1, 1);
    if (subject.ok == 1)
    {
        subject_ast = sv_to_bv (c, subject);
        subject_ok = (U1) (subject_ast != NULL);
    }
    if (subject_ok == 0)
    {
        e->precise = 0;
        sv_list_add (c, cont, e);
        return;
    }

    for (ci = 0; ci < n->ncases; ci++)
    {
        S2 nt2 = tokenize_line (p->lines[n->cases[ci].line], sv_tokens, SV_MAXTOK);
        struct sv_val cv;
        Z3_ast val = NULL;
        Z3_ast guard;
        Z3_ast entry_pc;
        struct sv_list cur;
        S2 j;

        c->cur_line = p->line_nums[n->cases[ci].line];
        c->cur_text = p->lines[n->cases[ci].line];
        cv = sv_eval (c, p, e, nt2, sv_tokens, 2, 2);
        if (cv.ok == 1)
        {
            val = sv_to_bv (c, cv);
        }
        if (val == NULL)
        {
            /* unknown case value: cannot model this switch precisely */
            e->precise = 0;
            continue;
        }
        guard = sv_eq (c, subject_ast, val);
        if (prev != NULL)
        {
            guard = sv_and (c, guard, sv_not (c, prev));
        }
        prev = sv_or (c, prev, sv_eq (c, subject_ast, val));
        entry_pc = sv_and (c, e->pc, guard);

        if (sv_check (c, entry_pc, NULL) == 0)
        {
            continue;
        }

        memset (&cur, 0, sizeof (cur));
        {
            struct sv_env *ce = sv_env_clone (c, p, e);
            if (ce == NULL) return;
            ce->pc = entry_pc;
            sv_list_add (c, &cur, ce);
        }

        for (j = ci; j < n->ncases; j++)
        {
            struct sv_list body_out;
            struct sv_list next;
            S2 k;

            body_out = sv_exec_block (c, p, n->cases[j].body, cur);
            memset (&next, 0, sizeof (next));
            for (k = 0; k < body_out.n; k++)
            {
                struct sv_env *be = body_out.e[k];

                if (be->esc == SV_ESC_BREAK)
                {
                    be->esc = SV_ESC_NONE;
                    sv_list_add (c, cont, be);
                }
                else if (be->esc != SV_ESC_NONE)
                {
                    sv_list_add (c, out, be);
                }
                else
                {
                    sv_list_add (c, &next, be);
                }
            }
            free (body_out.e);
            cur = next;
            if (cur.n == 0)
            {
                break;
            }
        }
        /* fall through to the code after the switch */
        {
            S2 k;
            for (k = 0; k < cur.n; k++)
            {
                sv_list_add (c, cont, cur.e[k]);
            }
            free (cur.e);
        }
    }

    /* no case matched */
    if (prev != NULL)
    {
        Z3_ast nomatch = sv_and (c, e->pc, sv_not (c, prev));
        if (sv_check (c, nomatch, NULL) != 0)
        {
            struct sv_env *ne = sv_env_clone (c, p, e);
            if (ne != NULL)
            {
                ne->pc = nomatch;
                sv_list_add (c, cont, ne);
            }
        }
    }
    else
    {
        sv_list_add (c, cont, e);
    }
}

static struct sv_list sv_exec_block (struct sv_ctx *c, struct sv_part *p,
                                     struct sv_node *first, struct sv_list in)
{
    struct sv_list out;
    struct sv_list cur;
    struct sv_node *n;

    memset (&out, 0, sizeof (out));
    cur = in;

    for (n = first; n != NULL; n = n->next)
    {
        struct sv_list next;
        S2 i;

        if (cur.n == 0)
        {
            break;
        }
        memset (&next, 0, sizeof (next));

        for (i = 0; i < cur.n; i++)
        {
            struct sv_env *e = cur.e[i];

            if (++c->steps > SV_MAX_STEPS)
            {
                c->truncated = 1;
                sv_list_add (c, &out, e);
                continue;
            }

            switch (n->kind)
            {
                case SVN_STMT:
                    c->cur_line = p->line_nums[n->line];
                    c->cur_text = p->lines[n->line];
                    sv_apply_stmt (c, p, e, n->line);
                    if (e->esc == SV_ESC_NONE) sv_list_add (c, &next, e);
                    else sv_list_add (c, &out, e);
                    break;

                case SVN_ASSERT:
                {
                    struct sv_val v;
                    Z3_ast a;

                    c->cur_line = p->line_nums[n->line];
                    c->cur_text = p->lines[n->line];
                    v = sv_annotation (c, p, e, n->line, "assert");
                    a = sv_to_bool (c, v);
                    if (a != NULL && e->precise == 1 && c->truncated == 0)
                    {
                        Z3_ast fail = sv_and (c, e->pc, sv_not (c, a));
                        S2 r = sv_check (c, fail, NULL);

                        if (r == 1)
                        {
                            S2 never = sv_check (c, e->pc, a);
                            if (never == 0)
                            {
                                sv_report (c, "assertion is always false at this point");
                            }
                        }
                        else if (r == -1)
                        {
                            e->precise = 0;
                        }
                    }
                    else if (a == NULL)
                    {
                        e->precise = 0;
                    }
                    sv_list_add (c, &next, e);
                    break;
                }

                case SVN_ASSUME:
                {
                    struct sv_val v;
                    Z3_ast a;

                    c->cur_line = p->line_nums[n->line];
                    c->cur_text = p->lines[n->line];
                    v = sv_annotation (c, p, e, n->line, "assume");
                    a = sv_to_bool (c, v);
                    if (a != NULL)
                    {
                        Z3_ast hold = sv_and (c, e->pc, a);
                        S2 r = sv_check (c, hold, NULL);

                        if (r == 0 && e->precise == 1 && c->truncated == 0)
                        {
                            sv_report (c, "assumption can never hold at this point");
                        }
                        e->pc = hold;
                        if (r == 0 && e->precise == 1)
                        {
                            /* the path is infeasible, do not continue it */
                            break;
                        }
                    }
                    else
                    {
                        e->precise = 0;
                    }
                    sv_list_add (c, &next, e);
                    break;
                }

                case SVN_IF:
                    sv_exec_if (c, p, n, e, &next, &out);
                    break;

                case SVN_DO:
                case SVN_FOR:
                    sv_exec_loop (c, p, n, e, &next, &out);
                    break;

                case SVN_SWITCH:
                    if (n->ncases == 0)
                    {
                        sv_list_add (c, &next, e);
                    }
                    else
                    {
                        sv_exec_switch (c, p, n, e, &next, &out);
                    }
                    break;

                case SVN_BREAK:
                    e->esc = SV_ESC_BREAK;
                    sv_list_add (c, &out, e);
                    break;

                case SVN_RETURN:
                    e->esc = SV_ESC_RETURN;
                    sv_list_add (c, &out, e);
                    break;

                case SVN_JMP:
                    /* An unconditional jump leaves this block. A conditional
                     * "jmpi" may also fall through; keep that path, but it is
                     * imprecise because the jump state is not propagated.
                     * Labels below are marked imprecise as well, so a missing
                     * predecessor can never make a later check look provable. */
                    if (sv_line_has (p->lines[n->line], "jmpi") == 1)
                    {
                        e->precise = 0;
                        sv_list_add (c, &next, e);
                    }
                    else
                    {
                        e->esc = SV_ESC_JUMP;
                        sv_list_add (c, &out, e);
                    }
                    break;

                case SVN_LABEL:
                    /* A label can be the target of a forward or backward
                     * jump. The jump predecessor states are not merged here,
                     * so code after a label must not produce solver proofs. */
                    e->precise = 0;
                    sv_list_add (c, &next, e);
                    break;

                default:
                    sv_list_add (c, &next, e);
                    break;
            }
        }
        free (cur.e);
        cur = next;
    }

    /* whatever is still active falls out of the block */
    {
        S2 i;
        for (i = 0; i < cur.n; i++)
        {
            sv_list_add (c, &out, cur.e[i]);
        }
        free (cur.e);
    }
    return (out);
}

/* ------------------------------------------------------------------------- */
/* parts, declarations and file handling                                     */
/* ------------------------------------------------------------------------- */

static void sv_part_add_line (struct sv_ctx *c, struct sv_part *p,
                              U1 *line, S8 line_no)
{
    U1 **nl;
    S8 *nn;
    S2 cap;

    if (p->count >= p->cap)
    {
        cap = p->cap == 0 ? 64 : p->cap * 2;
        nl = (U1 **) realloc (p->lines, (size_t) cap * sizeof (U1 *));
        nn = (S8 *) realloc (p->line_nums, (size_t) cap * sizeof (S8));
        if (nl == NULL || nn == NULL)
        {
            free (nl);
            free (nn);
            c->truncated = 1;
            return;
        }
        p->lines = nl;
        p->line_nums = nn;
        p->cap = cap;
    }
    p->lines[p->count] = (U1 *) strdup ((const char *) line);
    if (p->lines[p->count] == NULL)
    {
        c->truncated = 1;
        return;
    }
    p->line_nums[p->count] = line_no;
    p->count++;
}

static U1 sv_is_include_start (U1 *line)
{
    S2 p;

    sv_trim (line, &p);
    return (U1) (strncmp ((const char *) &line[p], "FILE:", 5) == 0);
}

static U1 sv_is_include_end (U1 *line)
{
    S2 p;

    sv_trim (line, &p);
    return (U1) (strncmp ((const char *) &line[p], "FILE END", 8) == 0);
}

static S2 sv_read_parts (struct sv_ctx *c, const char *fname,
                         struct sv_part **parts_out)
{
    FILE *f;
    U1 rbuf[MAXLINELEN + 1];
    U1 buf[MAXLINELEN + 1];
    char *read;
    struct sv_part *parts = NULL;
    S2 nparts = 0;
    S2 cap = 0;
    S2 cur = -1;
    U1 inside_include = 0;
    S8 line_no = 0;

    f = fopen (fname, "r");
    if (f == NULL)
    {
        return (0);
    }

    while (1)
    {
        U1 name[MAXSTRLEN + 1];

        read = fgets_uni ((char *) rbuf, MAXLINELEN, f);
        if (read == NULL)
        {
            break;
        }
        if (inside_include == 0)
        {
            line_no++;
        }
        strcpy ((char *) buf, (const char *) rbuf);
        {
            S2 i = 0;
            while (buf[i] != '\0')
            {
                if (buf[i] == '\t') buf[i] = ' ';
                i++;
            }
        }

        if (sv_is_include_start (buf) == 1)
        {
            inside_include = 1;
            continue;
        }
        if (sv_is_include_end (buf) == 1)
        {
            inside_include = 0;
            continue;
        }
        if (inside_include == 1)
        {
            continue;
        }

        if (ip_is_funcend (buf) == 1)
        {
            cur = -1;
            continue;
        }
        if (ip_extract_func_start (buf, name) == 1)
        {
            if (nparts >= cap)
            {
                S2 nc = cap == 0 ? 16 : cap * 2;
                struct sv_part *np = (struct sv_part *) realloc (parts, (size_t) nc * sizeof (struct sv_part));
                if (np == NULL)
                {
                    break;
                }
                memset (&np[cap], 0, (size_t) (nc - cap) * sizeof (struct sv_part));
                parts = np;
                cap = nc;
            }
            memset (&parts[nparts], 0, sizeof (struct sv_part));
            strncpy ((char *) parts[nparts].name, (const char *) name, MAXSTRLEN);
            parts[nparts].name[MAXSTRLEN] = '\0';
            cur = nparts;
            nparts++;
            continue;
        }
        if (cur >= 0)
        {
            sv_part_add_line (c, &parts[cur], buf, line_no);
        }
    }
    fclose (f);
    *parts_out = parts;
    return (nparts);
}

static void sv_scan_decls (struct sv_part *p)
{
    S2 li;

    for (li = 0; li < p->count; li++)
    {
        S2 nt;

        if (sv_line_is_comment (p->lines[li]) == 1)
        {
            continue;
        }
        nt = tokenize_line (p->lines[li], sv_tokens, SV_MAXTOK);
        if (nt > 3 && sv_is (sv_tokens[1], "set") == 1)
        {
            U1 type[MAXSTRLEN + 1];
            S2 size_idx = sv_decl_header (nt, sv_tokens, type);
            S2 name_idx = size_idx + 1;

            if (size_idx < 0 || name_idx >= nt ||
                is_ident_start (sv_tokens[name_idx][0]) == 0)
            {
                continue;
            }
            {
                U1 vartype = getvartype (type);
                U1 is_const = (U1) (strncmp ((const char *) type, "const-", 6) == 0);
                S8 size = 1;

                if (is_number_token (sv_tokens[size_idx]) == 1)
                {
                    size = (S8) strtoll ((const char *) sv_tokens[size_idx], NULL, 10);
                }
                {
                    S2 vi = sv_add_var (p, sv_tokens[name_idx], vartype, is_const);
                    if (vi >= 0 && (size > 1 ||
                        strstr ((const char *) type, "string") != NULL))
                    {
                        p->vars[vi].modeled = 0;
                    }
                }
            }
        }
    }
}

static void sv_free_node (struct sv_node *n)
{
    while (n != NULL)
    {
        struct sv_node *next = n->next;
        S2 i;

        sv_free_node (n->body);
        sv_free_node (n->else_body);
        for (i = 0; i < n->ncases; i++)
        {
            sv_free_node (n->cases[i].body);
        }
        free (n->cases);
        free (n);
        n = next;
    }
}

static void sv_free_part (struct sv_part *p)
{
    S2 i;

    for (i = 0; i < p->count; i++)
    {
        free (p->lines[i]);
    }
    free (p->lines);
    free (p->line_nums);
    sv_free_node (p->body);
}

/* ------------------------------------------------------------------------- */
/* public entry point                                                        */
/* ------------------------------------------------------------------------- */

void linter_solver_file (const char *fname)
{
    struct sv_ctx c;
    struct sv_part *parts = NULL;
    S2 nparts;
    S2 i;

    memset (&c, 0, sizeof (c));
    c.unroll = SV_MAX_UNROLL;

    Z3_config cfg = Z3_mk_config ();
    Z3_set_param_value (cfg, "model", "false");
    c.z3 = Z3_mk_context (cfg);
    Z3_del_config (cfg);
    if (c.z3 == NULL)
    {
        return;
    }
    c.solver = Z3_mk_solver (c.z3);
    c.bv64 = Z3_mk_bv_sort (c.z3, 64);

    nparts = sv_read_parts (&c, fname, &parts);
    for (i = 0; i < nparts && c.truncated == 0; i++)
    {
        struct sv_part *p = &parts[i];
        struct sv_env *e;
        struct sv_list in;
        struct sv_list out;
        S2 pos = 0;
        S2 k;

        sv_scan_decls (p);
        p->body = sv_parse_block (&c, p, &pos, 0);
        if (p->body == NULL)
        {
            continue;
        }

        e = sv_env_new (&c, p);
        if (e == NULL)
        {
            break;
        }
        for (k = 0; k < p->nvars; k++)
        {
            if (p->vars[k].modeled == 1)
            {
                e->v[k] = sv_fresh (&c, p, p->vars[k].name);
            }
        }
        memset (&in, 0, sizeof (in));
        sv_list_add (&c, &in, e);
        out = sv_exec_block (&c, p, p->body, in);
        free (out.e);
    }

    for (i = 0; i < nparts; i++)
    {
        sv_free_part (&parts[i]);
    }
    free (parts);
    Z3_del_context (c.z3);
}

#else

void linter_solver_file (const char *fname)
{
    (void) fname;
}

#endif
