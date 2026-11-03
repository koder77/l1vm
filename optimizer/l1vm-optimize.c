/*
 * This file l1vm-optimize.c is part of L1vm.
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
 * l1vm-optimize - an assembly level optimizer for L1VM.
 * Developed with the help of AI.
 *
 * Usage:
 *   l1vm-optimize [-O0|-O1|-O2|-O4|-O5|-O6|-O7] <assembly> <optimized-assembly>
 *
 * The ".l1asm" suffix is added automatically to both file names.
 * "-O1" is the default.  "-O0" disables all optimizations.
 *
 * Optimizations:
 *
 *  - "inclsijmpi" / "decgrijmpi" superopcodes for counted loops.
 *
 *    A bottom tested register loop
 *
 *        :loop
 *            ...
 *            addi one, counter, counter
 *            lsi  counter, limit, flag
 *            jmpi flag, :loop
 *
 *    is replaced by
 *
 *        :loop
 *            ...
 *            inclsijmpi counter, limit, :loop
 *
 *    The decreasing variant uses "subi"/"gri" and "decgrijmpi".
 *
 *  - The usual compiler generated top tested "for"/"while" loop:
 *
 *        :for_0
 *            loada counter, 0, 1
 *            loada limit,   0, 2
 *            lsi 1, 2, 3
 *            load f, 0, 4
 *            pullqw 3, 4, 0
 *            loada f, 0, 1
 *            jmpi 1, :for2_0
 *            jmp :forend_0
 *        :for2_0
 *            ... loop body ...
 *            loada counter, 0, 1
 *            loada one,     0, 2
 *            addi 1, 2, 3
 *            load counter, 0, 4
 *            pullqw 3, 4, 0
 *            jmp :for_0
 *        :forend_0
 *
 *    is rotated into a bottom tested loop which uses the superopcode.
 *    The loop counter is written back to memory at the body entry and
 *    on loop exit, so the memory based variable keeps its value.
 *
 *  - Redundant self moves ("movi 1, 1") are removed.
 *  - An unconditional jump to the directly following label is removed.
 *
 * With "-O2" the optimizer also promotes scalar variables of non-main
 * functions into registers. Variables which the compiler scoped to a
 * function with the "name@function" convention are only visible inside
 * that function, so their memory image does not have to be kept current:
 *
 *        loada counter@func, 0, 1        movi 10, 1
 *        ...                       ->    ...
 *        load counter@func, 0, 2         movi 3, 10
 *        pullqw 3, 2, 0
 *
 * If every read is preceded by a write on all control flow paths the
 * variable is kept in its register only.  Otherwise the value is loaded
 * once on function entry and written back before every "rts".
 *
 * With "-O4" the optimizer also recognizes compiler locals which use the
 * plain "namefunction" suffix, for example "numsquare_int" inside
 * ":square_int".  If a local is always stored from and read into the same
 * register, that register is reused, so the memory accesses can be removed
 * completely:
 *
 *        load numsquare_int, 0, 2        (removed)
 *        pullqw 1, 2, 0
 *        muli 1, 1, 2                    muli 1, 1, 2
 *        load squaresquare_int, 0, 3     (removed)
 *        pullqw 2, 3, 0
 *        loada squaresquare_int, 0, 2    (removed)
 *
 * Register reuse is limited to straight line code without calls; otherwise
 * a fresh register is used as in "-O2".
 *
 * "-O4" also promotes plain globals which are referenced by only one
 * function.  Such a variable cannot be observed by another function, so it
 * is handled like a function local:
 *
 *        max, ind, num and friends in ":3x1"
 *
 *        loada ind, 0, 1        (loop header)
 *        ...
 *        loada num, 0, 2
 *        ...
 *
 * become register accesses.  A value which is read before it is written is
 * loaded once on function entry and written back before every "rts".  A
 * read only global can be cached in a fresh register inside every function
 * which reads it more than once or on a loop path, even if several
 * functions read the same constant.
 *
 * With "-O5" the optimizer also inlines small leaf functions which use the
 * normal compiler calling convention.  For example, the call
 *
 *        stpushi 3
 *        jsr :square_int
 *        stpopi 4
 *
 * with the function body
 *
 *        :square_int
 *        loada zero, 0, 0
 *        stpopi 1
 *        muli 1, 1, 2
 *        stpushi 2
 *        rts
 *
 * becomes
 *
 *        movi 3, 7
 *        muli 7, 7, 9
 *        movi 9, 4
 *
 * The argument push, the "jsr", the result pop, the callee argument pop and
 * the callee result push all disappear.  Only functions with a single
 * straight line exit, explicit leading argument pops, only integer/double
 * arguments and at most 32 body instructions, 16 arguments and 4 call sites
 * are inlined.  The
 * inlined body gets fresh registers, so the caller register saves and restores
 * can be removed as well when their complete block is found.  An unused
 * initial "loada zero, 0, 0" is not copied.
 *
 * With "-O6" the optimizer also unrolls small rotated loops which use the
 * "inclsijmpi" / "decgrijmpi" superopcodes.
 *
 * "-O7" enables all of the above with more aggressive limits (96 body
 * instructions and 16 call sites for the inliner, unroll factor 8) and adds
 * a constant propagation and folding pass, forward copy propagation inside
 * straight line blocks and liveness based dead code elimination.
 *
 * All transformations are conservative: a loop is only changed if the
 * complete canonical pattern is found and the temporaries used by the
 * transformation are provably dead.
 */

#define _GNU_SOURCE 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>

#include "../include/global.h"
#include "../include/opcodes.h"

#define OPT_LINE_MAX 4096
#define OPT_ARG_MAX 512
#define OPT_MAX_ARGS 4
#define OPT_DATA_TOKENS 16

#define INLINE_MAX_BODY 32
#define INLINE_MAX_CALLS 4
#define UNROLL_MAX_FACTOR 4
#define INLINE_MAX_ARGS 16
#define INLINE_MAX_PUSHES 64
#define INLINE_SCAN_MAX 512

typedef enum
{
    LINE_BLANK = 0,
    LINE_COMMENT,
    LINE_DIRECTIVE,
    LINE_LABEL,
    LINE_INSTR
} LineKind;

typedef struct
{
    char *text;                                 /* original line without newline */
    LineKind kind;

    char op[OPT_ARG_MAX];                       /* opcode, directive or label */
    int argc;
    char args[OPT_MAX_ARGS][OPT_ARG_MAX];

    int active;                                 /* 0: line is removed */
    char *pre;                                  /* text printed before this line */
    char *replace;                              /* replacement text (may be multiline) */
    char *post;                                 /* text printed after this line */
} Line;

typedef struct
{
    char name[OPT_ARG_MAX];
    int known;
    long long value;
    int order;
} DataConst;

typedef struct
{
    char name[OPT_ARG_MAX];
    char type;                                  /* B, W, D, Q or F (double) */
    long long count;
    int ambiguous;
} DataSymbol;

static Line *lines = NULL;
static int nlines = 0;
static int line_cap = 0;

static DataConst *consts = NULL;
static int nconsts = 0;
static int const_cap = 0;

static DataSymbol *symbols = NULL;
static int nsymbols = 0;
static int symbol_cap = 0;

static int opcode_count = 0;
static int opt_level = 1;

/* -O7 relaxes the conservative inliner and unroller limits. */
static int inline_max_body = INLINE_MAX_BODY;
static int inline_max_calls = INLINE_MAX_CALLS;
static int unroll_max_factor = UNROLL_MAX_FACTOR;

/* ------------------------------------------------------------------------- */
/* small helpers                                                             */
/* ------------------------------------------------------------------------- */

static void die (const char *msg)
{
    fprintf (stderr, "l1vm-optimize: %s\n", msg);
    exit (1);
}

static void *xmalloc (size_t size)
{
    void *ptr = malloc (size);

    if (ptr == NULL)
    {
        die ("out of memory");
    }
    return (ptr);
}

static void *xrealloc (void *ptr, size_t size)
{
    void *newptr = realloc (ptr, size);

    if (newptr == NULL)
    {
        die ("out of memory");
    }
    return (newptr);
}

static char *xstrdup (const char *str)
{
    size_t len = strlen (str);
    char *copy = (char *) xmalloc (len + 1);

    memcpy (copy, str, len + 1);
    return (copy);
}

static char *trim (char *str)
{
    char *end;

    while (*str != '\0' && isspace ((unsigned char) *str))
    {
        str++;
    }

    end = str + strlen (str);
    while (end > str && isspace ((unsigned char) end[-1]))
    {
        end--;
    }
    *end = '\0';
    return (str);
}

/* append "extra" as a new line to *dest */
static void append_extra (char **dest, const char *extra)
{
    size_t oldlen;
    size_t extralen;
    char *newstr;

    if (extra == NULL || extra[0] == '\0')
    {
        return;
    }

    oldlen = (*dest == NULL) ? 0 : strlen (*dest);
    extralen = strlen (extra);
    newstr = (char *) xmalloc (oldlen + extralen + 2);

    if (*dest != NULL)
    {
        memcpy (newstr, *dest, oldlen);
    }
    if (oldlen > 0)
    {
        newstr[oldlen] = '\n';
        oldlen++;
    }
    memcpy (newstr + oldlen, extra, extralen + 1);
    free (*dest);
    *dest = newstr;
}

/* ------------------------------------------------------------------------- */
/* line parsing                                                              */
/* ------------------------------------------------------------------------- */

/*
 * Find a "//" comment outside of a quoted string.
 * Returns the position of the comment or -1.
 */
static int find_comment (const char *str)
{
    int in_quote = 0;
    int i = 0;

    while (str[i] != '\0')
    {
        if (str[i] == '"')
        {
            in_quote = ! in_quote;
        }
        else if (! in_quote && str[i] == '/' && str[i + 1] == '/')
        {
            return (i);
        }
        i++;
    }
    return (-1);
}

/*
 * Split a line into tokens. Commas and white space separate tokens.
 * Quoted strings are kept as one token including the quotes.
 */
static int tokenize (const char *str, char tokens[][OPT_ARG_MAX], int max_tokens)
{
    int count = 0;
    int pos = 0;

    while (str[pos] != '\0' && count < max_tokens)
    {
        while (str[pos] != '\0' &&
               (isspace ((unsigned char) str[pos]) || str[pos] == ','))
        {
            pos++;
        }

        if (str[pos] == '\0')
        {
            break;
        }

        if (str[pos] == '"')
        {
            int t = 0;

            tokens[count][t] = str[pos];
            t++;
            pos++;

            while (str[pos] != '\0' && t < OPT_ARG_MAX - 1)
            {
                tokens[count][t] = str[pos];
                t++;
                if (str[pos] == '"')
                {
                    pos++;
                    break;
                }
                pos++;
            }
            tokens[count][t] = '\0';
            count++;
        }
        else
        {
            int t = 0;

            while (str[pos] != '\0' &&
                   ! isspace ((unsigned char) str[pos]) &&
                   str[pos] != ',')
            {
                if (t < OPT_ARG_MAX - 1)
                {
                    tokens[count][t] = str[pos];
                    t++;
                }
                pos++;
            }
            tokens[count][t] = '\0';
            count++;
        }
    }
    return (count);
}

static void parse_line (Line *line)
{
    char buf[OPT_LINE_MAX];
    char *ptr;
    char tokens[OPT_MAX_ARGS + 1][OPT_ARG_MAX];
    int count;
    int comment_pos;

    line->argc = 0;
    line->op[0] = '\0';
    line->kind = LINE_BLANK;

    if (strlen (line->text) >= sizeof (buf))
    {
        die ("input line too long");
    }
    strcpy (buf, line->text);

    ptr = trim (buf);
    if (ptr[0] == '\0')
    {
        line->kind = LINE_BLANK;
        return;
    }

    if (ptr[0] == '/' && ptr[1] == '/')
    {
        line->kind = LINE_COMMENT;
        return;
    }

    comment_pos = find_comment (ptr);
    if (comment_pos >= 0)
    {
        ptr[comment_pos] = '\0';
        ptr = trim (ptr);
        if (ptr[0] == '\0')
        {
            line->kind = LINE_COMMENT;
            return;
        }
    }

    if (ptr[0] == '.')
    {
        count = tokenize (ptr, tokens, OPT_MAX_ARGS + 1);
        if (count < 1)
        {
            line->kind = LINE_BLANK;
            return;
        }
        line->kind = LINE_DIRECTIVE;
        strncpy (line->op, tokens[0], OPT_ARG_MAX - 1);
        line->op[OPT_ARG_MAX - 1] = '\0';
        return;
    }

    if (ptr[0] == ':')
    {
        count = tokenize (ptr, tokens, 2);
        if (count < 1)
        {
            line->kind = LINE_BLANK;
            return;
        }
        line->kind = LINE_LABEL;
        strncpy (line->op, tokens[0], OPT_ARG_MAX - 1);
        line->op[OPT_ARG_MAX - 1] = '\0';
        return;
    }

    count = tokenize (ptr, tokens, OPT_MAX_ARGS + 1);
    if (count < 1)
    {
        line->kind = LINE_BLANK;
        return;
    }

    line->kind = LINE_INSTR;
    strncpy (line->op, tokens[0], OPT_ARG_MAX - 1);
    line->op[OPT_ARG_MAX - 1] = '\0';

    line->argc = count - 1;
    if (line->argc > OPT_MAX_ARGS)
    {
        line->argc = OPT_MAX_ARGS;
    }

    {
        int i;

        for (i = 0; i < line->argc; i++)
        {
            strncpy (line->args[i], tokens[i + 1], OPT_ARG_MAX - 1);
            line->args[i][OPT_ARG_MAX - 1] = '\0';
        }
    }
}

static void append_parsed_line (Line **arr, int *count, int *cap,
                                const char *text)
{
    Line *line;

    if (*count >= *cap)
    {
        *cap = (*cap == 0) ? 1024 : *cap * 2;
        *arr = (Line *) xrealloc (*arr, (size_t) *cap * sizeof (Line));
    }

    line = &(*arr)[*count];
    memset (line, 0, sizeof (Line));
    line->text = xstrdup (text);
    line->active = 1;
    parse_line (line);
    (*count)++;
}

static void add_line (const char *text)
{
    append_parsed_line (&lines, &nlines, &line_cap, text);
}

/* ------------------------------------------------------------------------- */
/* data constants                                                            */
/* ------------------------------------------------------------------------- */

static int parse_number_token (const char *token, long long *value)
{
    char buf[OPT_ARG_MAX];
    char *ptr;
    char *endptr;
    int base = 10;
    long long num;

    if (token == NULL || token[0] == '\0' || token[0] == '"')
    {
        return (0);
    }

    strncpy (buf, token, OPT_ARG_MAX - 1);
    buf[OPT_ARG_MAX - 1] = '\0';
    ptr = trim (buf);

    if (ptr[0] == '\0' || ptr[0] == '"')
    {
        return (0);
    }

    /* skip a type prefix like "I", "Q", "F", ... */
    if ((ptr[0] == 'I' || ptr[0] == 'F' || ptr[0] == 'W' ||
         ptr[0] == 'D' || ptr[0] == 'Q' || ptr[0] == 'B') &&
        (isdigit ((unsigned char) ptr[1]) || ptr[1] == '-' || ptr[1] == '+'))
    {
        ptr++;
    }

    if (ptr[0] == '&')
    {
        base = 16;
        ptr++;
    }
    else if (ptr[0] == '$')
    {
        base = 2;
        ptr++;
    }

    if (ptr[0] == '\0')
    {
        return (0);
    }

    errno = 0;
    num = strtoll (ptr, &endptr, base);
    if (endptr == ptr || errno == ERANGE)
    {
        return (0);
    }

    /* allow a trailing type suffix like "160Q" */
    while (*endptr != '\0')
    {
        if (*endptr == 'I' || *endptr == 'F' || *endptr == 'W' ||
            *endptr == 'D' || *endptr == 'Q' || *endptr == 'B' ||
            isspace ((unsigned char) *endptr))
        {
            endptr++;
        }
        else
        {
            return (0);
        }
    }

    *value = num;
    return (1);
}

static int compare_consts (const void *a, const void *b)
{
    const DataConst *ca = (const DataConst *) a;
    const DataConst *cb = (const DataConst *) b;
    int cmp = strcmp (ca->name, cb->name);

    if (cmp != 0)
    {
        return (cmp);
    }
    return (ca->order - cb->order);
}

static void add_const (const char *name)
{
    int i;

    for (i = 0; i < nconsts; i++)
    {
        if (strcmp (consts[i].name, name) == 0)
        {
            /* the assembler uses the first definition */
            return;
        }
    }

    if (nconsts >= const_cap)
    {
        const_cap = (const_cap == 0) ? 256 : const_cap * 2;
        consts = (DataConst *) xrealloc (consts,
                                         (size_t) const_cap * sizeof (DataConst));
    }

    memset (&consts[nconsts], 0, sizeof (DataConst));
    strncpy (consts[nconsts].name, name, OPT_ARG_MAX - 1);
    consts[nconsts].name[OPT_ARG_MAX - 1] = '\0';
    consts[nconsts].known = 0;
    consts[nconsts].order = nconsts;
    nconsts++;
}

static DataConst *find_const_linear (const char *name)
{
    int i;

    for (i = 0; i < nconsts; i++)
    {
        if (strcmp (consts[i].name, name) == 0)
        {
            return (&consts[i]);
        }
    }
    return (NULL);
}

static DataConst *find_const (const char *name)
{
    int low = 0;
    int high = nconsts - 1;

    while (low <= high)
    {
        int mid = low + (high - low) / 2;
        int cmp = strcmp (name, consts[mid].name);

        if (cmp < 0)
        {
            high = mid - 1;
        }
        else if (cmp > 0)
        {
            low = mid + 1;
        }
        else
        {
            /* the lowest "order" entry wins */
            while (mid > 0 && strcmp (consts[mid - 1].name, name) == 0)
            {
                mid--;
            }
            return (&consts[mid]);
        }
    }
    return (NULL);
}

static int data_const_value (const char *name, long long *value)
{
    DataConst *entry = find_const (name);

    if (entry != NULL && entry->known)
    {
        *value = entry->value;
        return (1);
    }
    return (0);
}

static void collect_data_constants (void)
{
    int in_data = 0;
    int i;
    char current_name[OPT_ARG_MAX] = "";
    int have_current = 0;

    for (i = 0; i < nlines; i++)
    {
        Line *line = &lines[i];
        char tokens[OPT_DATA_TOKENS][OPT_ARG_MAX];
        int count;

        if (line->kind == LINE_DIRECTIVE)
        {
            if (strcmp (line->op, ".data") == 0)
            {
                in_data = 1;
            }
            else if (strcmp (line->op, ".dend") == 0 ||
                     strcmp (line->op, ".code") == 0 ||
                     strcmp (line->op, ".cend") == 0)
            {
                in_data = 0;
            }
            continue;
        }

        if (! in_data || line->kind == LINE_BLANK ||
            line->kind == LINE_COMMENT || line->kind == LINE_LABEL)
        {
            continue;
        }

        count = tokenize (line->text, tokens, OPT_DATA_TOKENS);
        if (count < 2)
        {
            continue;
        }

        if (strcmp (tokens[0], "@") == 0)
        {
            long long value;

            if (have_current && count >= 3)
            {
                DataConst *entry = find_const_linear (current_name);

                if (entry != NULL && ! entry->known &&
                    parse_number_token (tokens[2], &value))
                {
                    entry->known = 1;
                    entry->value = value;
                }
            }
            continue;
        }

        if (count >= 3)
        {
            int j;

            for (j = 0; j < nconsts; j++)
            {
                if (strcmp (consts[j].name, tokens[2]) == 0)
                {
                    break;
                }
            }
            if (j == nconsts)
            {
                add_const (tokens[2]);
            }

            strncpy (current_name, tokens[2], OPT_ARG_MAX - 1);
            current_name[OPT_ARG_MAX - 1] = '\0';
            have_current = 1;
        }
    }

    qsort (consts, (size_t) nconsts, sizeof (DataConst), compare_consts);
}

/* ------------------------------------------------------------------------- */
/* data symbols (used by the register promotion)                             */
/* ------------------------------------------------------------------------- */

static void add_symbol (const char *name, char type, long long count)
{
    int i;

    for (i = 0; i < nsymbols; i++)
    {
        if (strcmp (symbols[i].name, name) == 0)
        {
            /* duplicate definition: only optimize it if it is unambiguous */
            if (symbols[i].type != type || symbols[i].count != count)
            {
                symbols[i].ambiguous = 1;
            }
            return;
        }
    }

    if (nsymbols >= symbol_cap)
    {
        symbol_cap = (symbol_cap == 0) ? 256 : symbol_cap * 2;
        symbols = (DataSymbol *) xrealloc (symbols,
                                           (size_t) symbol_cap * sizeof (DataSymbol));
    }

    memset (&symbols[nsymbols], 0, sizeof (DataSymbol));
    strncpy (symbols[nsymbols].name, name, OPT_ARG_MAX - 1);
    symbols[nsymbols].name[OPT_ARG_MAX - 1] = '\0';
    symbols[nsymbols].type = type;
    symbols[nsymbols].count = count;
    nsymbols++;
}

static int compare_symbols (const void *a, const void *b)
{
    const DataSymbol *sa = (const DataSymbol *) a;
    const DataSymbol *sb = (const DataSymbol *) b;

    return (strcmp (sa->name, sb->name));
}

static void collect_data_symbols (void)
{
    int in_data = 0;
    int i;

    for (i = 0; i < nlines; i++)
    {
        Line *line = &lines[i];
        char tokens[OPT_DATA_TOKENS][OPT_ARG_MAX];
        int count;

        if (line->kind == LINE_DIRECTIVE)
        {
            if (strcmp (line->op, ".data") == 0)
            {
                in_data = 1;
            }
            else if (strcmp (line->op, ".dend") == 0 ||
                     strcmp (line->op, ".code") == 0 ||
                     strcmp (line->op, ".cend") == 0)
            {
                in_data = 0;
            }
            continue;
        }

        if (! in_data || line->kind == LINE_BLANK ||
            line->kind == LINE_COMMENT || line->kind == LINE_LABEL)
        {
            continue;
        }

        count = tokenize (line->text, tokens, OPT_DATA_TOKENS);
        if (count < 3 || strlen (tokens[0]) != 1)
        {
            continue;
        }

        if (tokens[0][0] != 'B' && tokens[0][0] != 'W' &&
            tokens[0][0] != 'D' && tokens[0][0] != 'Q' &&
            tokens[0][0] != 'F')
        {
            continue;
        }

        {
            char *endptr;
            long long size;

            errno = 0;
            size = strtoll (tokens[1], &endptr, 10);
            if (endptr == tokens[1] || *endptr != '\0' || size < 0)
            {
                continue;
            }
            add_symbol (tokens[2], tokens[0][0], size);
        }
    }

    qsort (symbols, (size_t) nsymbols, sizeof (DataSymbol), compare_symbols);
}

/* ------------------------------------------------------------------------- */
/* register helpers                                                          */
/* ------------------------------------------------------------------------- */

static int opcode_index (const char *name)
{
    int i;

    for (i = 0; i < opcode_count; i++)
    {
        if (strcmp ((const char *) opcode[i].op, name) == 0)
        {
            return (i);
        }
    }
    return (-1);
}

static int opcode_type (int index, int arg)
{
    if (index < 0 || arg < 0 || arg >= 4)
    {
        return (-1);
    }
    return (opcode[index].type[arg]);
}

static int parse_register_token (const char *token)
{
    char buf[OPT_ARG_MAX];
    char *ptr;
    char *endptr;
    int base = 10;
    long long num;

    if (token == NULL || token[0] == '\0' || token[0] == '"')
    {
        return (-1);
    }

    strncpy (buf, token, OPT_ARG_MAX - 1);
    buf[OPT_ARG_MAX - 1] = '\0';
    ptr = trim (buf);

    if (ptr[0] == '\0')
    {
        return (-1);
    }

    /* register names may be written as "I3", "Q3", ... */
    if ((ptr[0] == 'I' || ptr[0] == 'F' || ptr[0] == 'W' ||
         ptr[0] == 'D' || ptr[0] == 'Q' || ptr[0] == 'B') &&
        (isdigit ((unsigned char) ptr[1]) || ptr[1] == '-' || ptr[1] == '+'))
    {
        ptr++;
    }

    if (ptr[0] == '&')
    {
        base = 16;
        ptr++;
    }
    else if (ptr[0] == '$')
    {
        base = 2;
        ptr++;
    }

    if (ptr[0] == '\0')
    {
        return (-1);
    }

    errno = 0;
    num = strtoll (ptr, &endptr, base);
    if (endptr == ptr || errno == ERANGE)
    {
        return (-1);
    }

    while (*endptr != '\0')
    {
        if (*endptr == 'I' || *endptr == 'F' || *endptr == 'W' ||
            *endptr == 'D' || *endptr == 'Q' || *endptr == 'B' ||
            isspace ((unsigned char) *endptr))
        {
            endptr++;
        }
        else
        {
            return (-1);
        }
    }

    if (num < 0 || num >= MAXREG)
    {
        return (-1);
    }
    return ((int) num);
}

static int reg_match (const char *a, const char *b)
{
    int ra;
    int rb;

    if (strcmp (a, b) == 0)
    {
        return (1);
    }

    ra = parse_register_token (a);
    rb = parse_register_token (b);
    return (ra >= 0 && rb >= 0 && ra == rb);
}

static int op_is (const Line *line, const char *name)
{
    return (strcmp (line->op, name) == 0);
}

static int is_push_op (const char *op)
{
    return (strcmp (op, "pushb") == 0 || strcmp (op, "pushw") == 0 ||
            strcmp (op, "pushdw") == 0 || strcmp (op, "pushqw") == 0 ||
            strcmp (op, "pushd") == 0);
}

static int is_pull_op (const char *op)
{
    return (strcmp (op, "pullb") == 0 || strcmp (op, "pullw") == 0 ||
            strcmp (op, "pulldw") == 0 || strcmp (op, "pullqw") == 0 ||
            strcmp (op, "pulld") == 0);
}

static int is_stpush_op (const char *op)
{
    return (strcmp (op, "stpushb") == 0 || strcmp (op, "stpushi") == 0 ||
            strcmp (op, "stpushd") == 0);
}

static int is_stpop_op (const char *op)
{
    return (strcmp (op, "stpopb") == 0 || strcmp (op, "stpopi") == 0 ||
            strcmp (op, "stpopd") == 0);
}

static int is_jump_op (const char *op)
{
    return (strcmp (op, "jmp") == 0 || strcmp (op, "jmpi") == 0 ||
            strcmp (op, "jmpa") == 0 || strcmp (op, "jsr") == 0 ||
            strcmp (op, "jsra") == 0 || strcmp (op, "rts") == 0);
}

static int instr_arg_reads (const Line *line, int arg)
{
    const char *op = line->op;

    if (strcmp (op, "movi") == 0 || strcmp (op, "movd") == 0 ||
        strcmp (op, "noti") == 0)
    {
        return (arg == 0);
    }

    if (strcmp (op, "loadl") == 0)
    {
        return (0);
    }

    if (is_push_op (op))
    {
        return (arg == 0 || arg == 1);
    }

    if (is_pull_op (op))
    {
        return (arg == 0 || arg == 1 || arg == 2);
    }

    if (is_stpush_op (op))
    {
        return (arg == 0);
    }

    if (is_stpop_op (op))
    {
        return (0);
    }

    if (strcmp (op, "loada") == 0 || strcmp (op, "load") == 0 ||
        strcmp (op, "loadd") == 0)
    {
        return (0);
    }

    if (strcmp (op, "intr0") == 0 || strcmp (op, "intr1") == 0)
    {
        return (1);
    }

    if (strcmp (op, "jmpi") == 0 || strcmp (op, "jmpa") == 0 ||
        strcmp (op, "jsra") == 0)
    {
        return (arg == 0);
    }

    if (strcmp (op, "inclsijmpi") == 0 || strcmp (op, "decgrijmpi") == 0)
    {
        return (arg == 0 || arg == 1);
    }

    if (is_jump_op (op))
    {
        return (0);
    }

    /* arithmetic and compare opcodes: destination is the last argument */
    return (arg != 2);
}

static int instr_arg_writes (const Line *line, int arg)
{
    const char *op = line->op;

    if (strcmp (op, "movi") == 0 || strcmp (op, "movd") == 0 ||
        strcmp (op, "noti") == 0)
    {
        return (arg == 1);
    }

    if (strcmp (op, "loadl") == 0)
    {
        return (arg == 1);
    }

    if (is_push_op (op))
    {
        return (arg == 2);
    }

    if (is_pull_op (op))
    {
        return (0);
    }

    if (is_stpush_op (op))
    {
        return (0);
    }

    if (is_stpop_op (op))
    {
        return (arg == 0);
    }

    if (strcmp (op, "loada") == 0 || strcmp (op, "load") == 0 ||
        strcmp (op, "loadd") == 0)
    {
        return (arg == 2);
    }

    if (strcmp (op, "intr0") == 0 || strcmp (op, "intr1") == 0)
    {
        return (0);
    }

    if (strcmp (op, "inclsijmpi") == 0 || strcmp (op, "decgrijmpi") == 0)
    {
        return (arg == 0);
    }

    if (is_jump_op (op))
    {
        return (0);
    }

    return (arg == 2);
}

static int instr_arg_is_register (const Line *line, int arg)
{
    int index = opcode_index (line->op);
    int type;

    /* the first argument of an interrupt is the interrupt number */
    if ((strcmp (line->op, "intr0") == 0 ||
         strcmp (line->op, "intr1") == 0) && arg == 0)
    {
        return (0);
    }

    if (index < 0)
    {
        return (parse_register_token (line->args[arg]) >= 0);
    }

    type = opcode_type (index, arg);
    return (type == I_REG || type == D_REG || type == ALL);
}

static int instr_reads_reg (const Line *line, int reg)
{
    int arg;

    if (line->kind != LINE_INSTR)
    {
        return (0);
    }

    for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
    {
        if (instr_arg_is_register (line, arg) &&
            instr_arg_reads (line, arg) &&
            parse_register_token (line->args[arg]) == reg)
        {
            return (1);
        }
    }
    return (0);
}

static int instr_writes_reg (const Line *line, int reg)
{
    int arg;

    if (line->kind != LINE_INSTR)
    {
        return (0);
    }

    for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
    {
        if (instr_arg_is_register (line, arg) &&
            instr_arg_writes (line, arg) &&
            parse_register_token (line->args[arg]) == reg)
        {
            return (1);
        }
    }
    return (0);
}

/*
 * Integer and double registers use separate banks, so address register
 * liveness must ignore instructions which only touch the double bank.
 */
static int instr_arg_is_ireg (const Line *line, int arg)
{
    int index = opcode_index (line->op);
    int type;

    if ((strcmp (line->op, "intr0") == 0 ||
         strcmp (line->op, "intr1") == 0) && arg == 0)
    {
        return (0);
    }

    if (index < 0)
    {
        return (parse_register_token (line->args[arg]) >= 0);
    }

    type = opcode_type (index, arg);
    return (type == I_REG || type == ALL);
}

static int instr_reads_ireg (const Line *line, int reg)
{
    int arg;

    if (line->kind != LINE_INSTR)
    {
        return (0);
    }

    for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
    {
        if (instr_arg_is_ireg (line, arg) &&
            instr_arg_reads (line, arg) &&
            parse_register_token (line->args[arg]) == reg)
        {
            return (1);
        }
    }
    return (0);
}

static int instr_writes_ireg (const Line *line, int reg)
{
    int arg;

    if (line->kind != LINE_INSTR)
    {
        return (0);
    }

    for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
    {
        if (instr_arg_is_ireg (line, arg) &&
            instr_arg_writes (line, arg) &&
            parse_register_token (line->args[arg]) == reg)
        {
            return (1);
        }
    }
    return (0);
}

static int instr_arg_is_dreg (const Line *line, int arg)
{
    int index = opcode_index (line->op);
    int type;

    if ((strcmp (line->op, "intr0") == 0 ||
         strcmp (line->op, "intr1") == 0) && arg == 0)
    {
        return (0);
    }

    if (index < 0)
    {
        return (parse_register_token (line->args[arg]) >= 0);
    }

    type = opcode_type (index, arg);
    return (type == D_REG || type == ALL);
}

static int instr_writes_dreg (const Line *line, int reg)
{
    int arg;

    if (line->kind != LINE_INSTR)
    {
        return (0);
    }

    for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
    {
        if (instr_arg_is_dreg (line, arg) &&
            instr_arg_writes (line, arg) &&
            parse_register_token (line->args[arg]) == reg)
        {
            return (1);
        }
    }
    return (0);
}

static int instr_arg_is_label (const Line *line, int arg)
{
    int index = opcode_index (line->op);

    if (index < 0)
    {
        return (0);
    }
    return (opcode_type (index, arg) == LABEL);
}

/* ------------------------------------------------------------------------- */
/* navigation helpers                                                        */
/* ------------------------------------------------------------------------- */

static int next_line_any (int from)
{
    int i;

    for (i = from + 1; i < nlines; i++)
    {
        if (! lines[i].active)
        {
            continue;
        }
        if (lines[i].kind == LINE_BLANK || lines[i].kind == LINE_COMMENT)
        {
            continue;
        }
        return (i);
    }
    return (-1);
}

static int next_instr_here (int from)
{
    int i;

    for (i = from + 1; i < nlines; i++)
    {
        if (! lines[i].active)
        {
            continue;
        }
        if (lines[i].kind == LINE_BLANK || lines[i].kind == LINE_COMMENT)
        {
            continue;
        }
        if (lines[i].kind != LINE_INSTR)
        {
            return (-1);
        }
        return (i);
    }
    return (-1);
}

static int prev_instr_here (int from)
{
    int i;

    for (i = from - 1; i >= 0; i--)
    {
        if (! lines[i].active)
        {
            continue;
        }
        if (lines[i].kind == LINE_BLANK || lines[i].kind == LINE_COMMENT)
        {
            continue;
        }
        if (lines[i].kind != LINE_INSTR)
        {
            return (-1);
        }
        return (i);
    }
    return (-1);
}

static int find_label (const char *label)
{
    int i;

    for (i = 0; i < nlines; i++)
    {
        if (lines[i].active && lines[i].kind == LINE_LABEL &&
            strcmp (lines[i].op, label) == 0)
        {
            return (i);
        }
    }
    return (-1);
}

static int count_label_refs (const char *label)
{
    int count = 0;
    int i;

    for (i = 0; i < nlines; i++)
    {
        int arg;

        if (! lines[i].active || lines[i].kind != LINE_INSTR)
        {
            continue;
        }

        for (arg = 0; arg < lines[i].argc && arg < OPT_MAX_ARGS; arg++)
        {
            if (instr_arg_is_label (&lines[i], arg) &&
                strcmp (lines[i].args[arg], label) == 0)
            {
                count++;
            }
        }
    }
    return (count);
}

static int has_indirect_label_ref (const char *label)
{
    int i;

    for (i = 0; i < nlines; i++)
    {
        int arg;

        if (! lines[i].active || lines[i].kind != LINE_INSTR)
        {
            continue;
        }

        if (! (op_is (&lines[i], "loadl") || op_is (&lines[i], "jsr")))
        {
            continue;
        }

        for (arg = 0; arg < lines[i].argc && arg < OPT_MAX_ARGS; arg++)
        {
            if (instr_arg_is_label (&lines[i], arg) &&
                strcmp (lines[i].args[arg], label) == 0)
            {
                return (1);
            }
        }
    }
    return (0);
}

/* returns 1 if "reg" is read before it is written in the given range */
static int reg_read_before_write (int start, int end, int reg)
{
    int seen_write = 0;
    int i;

    for (i = start; i <= end; i++)
    {
        if (! lines[i].active || lines[i].kind != LINE_INSTR)
        {
            continue;
        }

        if (instr_reads_reg (&lines[i], reg) && ! seen_write)
        {
            return (1);
        }

        if (instr_writes_reg (&lines[i], reg))
        {
            seen_write = 1;
        }
    }
    return (0);
}

/* integer register variant of reg_read_before_write() */
static int reg_read_before_write_ireg (int start, int end, int reg)
{
    int seen_write = 0;
    int i;

    for (i = start; i <= end; i++)
    {
        if (! lines[i].active || lines[i].kind != LINE_INSTR)
        {
            continue;
        }

        if (instr_reads_ireg (&lines[i], reg) && ! seen_write)
        {
            return (1);
        }

        if (instr_writes_ireg (&lines[i], reg))
        {
            seen_write = 1;
        }
    }
    return (0);
}

/* returns 1 if the data variable address is loaded in the given range */
static int data_addr_used (int start, int end, const char *name)
{
    int i;

    for (i = start; i <= end; i++)
    {
        Line *line;

        if (! lines[i].active || lines[i].kind != LINE_INSTR)
        {
            continue;
        }

        line = &lines[i];

        if (op_is (line, "load") &&
            line->argc >= 1 && strcmp (line->args[0], name) == 0)
        {
            return (1);
        }
    }
    return (0);
}

/* returns 1 if the data variable value is read before a store to it */
static int data_read_before_store (int start, int end, const char *name)
{
    int seen_store = 0;
    int i;

    for (i = start; i <= end; i++)
    {
        Line *line;

        if (! lines[i].active || lines[i].kind != LINE_INSTR)
        {
            continue;
        }

        line = &lines[i];

        if (op_is (line, "load") &&
            line->argc >= 1 && strcmp (line->args[0], name) == 0)
        {
            seen_store = 1;
        }

        if ((op_is (line, "loada") || op_is (line, "loadd")) &&
            line->argc >= 1 && strcmp (line->args[0], name) == 0 &&
            ! seen_store)
        {
            return (1);
        }
    }
    return (0);
}

/* ------------------------------------------------------------------------- */
/* constant tracking for the simple register loop optimization               */
/* ------------------------------------------------------------------------- */

static int reg_known[MAXREG];
static long long reg_value[MAXREG];

static void regs_invalidate_all (void)
{
    memset (reg_known, 0, sizeof (reg_known));
}

static void regs_set (const char *reg, long long value)
{
    int r = parse_register_token (reg);

    if (r >= 0)
    {
        reg_known[r] = 1;
        reg_value[r] = value;
    }
}

static void regs_unknown (const char *reg)
{
    int r = parse_register_token (reg);

    if (r >= 0)
    {
        reg_known[r] = 0;
    }
}

static int regs_get (const char *reg, long long *value)
{
    int r = parse_register_token (reg);

    if (r >= 0 && reg_known[r])
    {
        *value = reg_value[r];
        return (1);
    }
    return (0);
}

static int intr0_readonly (const Line *line)
{
    long long interrupt;

    /*
     * The print and delay interrupts don't write integer registers.
     */
    if (line->argc >= 1 && parse_number_token (line->args[0], &interrupt) &&
        (interrupt == 4 || interrupt == 5 || interrupt == 6 ||
         interrupt == 7 || interrupt == 8 || interrupt == 14))
    {
        return (1);
    }
    return (0);
}

/* update the tracked register constants after an instruction */
static void update_regs (const Line *line)
{
    if (line->kind != LINE_INSTR)
    {
        return;
    }

    if (strcmp (line->op, "loada") == 0)
    {
        long long value;

        if (line->argc >= 3)
        {
            long long offset;

            if (parse_number_token (line->args[1], &offset) && offset == 0 &&
                data_const_value (line->args[0], &value))
            {
                regs_set (line->args[2], value);
            }
            else
            {
                regs_unknown (line->args[2]);
            }
        }
        return;
    }

    if (strcmp (line->op, "movi") == 0)
    {
        long long value;

        if (line->argc >= 2)
        {
            if (regs_get (line->args[0], &value))
            {
                regs_set (line->args[1], value);
            }
            else
            {
                regs_unknown (line->args[1]);
            }
        }
        return;
    }

    if (strcmp (line->op, "addi") == 0 || strcmp (line->op, "subi") == 0)
    {
        long long a;
        long long b;

        if (line->argc >= 3 && regs_get (line->args[0], &a) &&
            regs_get (line->args[1], &b))
        {
            if (strcmp (line->op, "addi") == 0)
            {
                regs_set (line->args[2], a + b);
            }
            else
            {
                regs_set (line->args[2], a - b);
            }
        }
        else if (line->argc >= 3)
        {
            regs_unknown (line->args[2]);
        }
        return;
    }

    if (strcmp (line->op, "intr0") == 0)
    {
        if (intr0_readonly (line))
        {
            return;
        }
        regs_invalidate_all ();
        return;
    }

    if (is_jump_op (line->op) || strcmp (line->op, "intr1") == 0)
    {
        regs_invalidate_all ();
        return;
    }

    if (strcmp (line->op, "inclsijmpi") == 0 ||
        strcmp (line->op, "decgrijmpi") == 0)
    {
        if (line->argc >= 1)
        {
            regs_unknown (line->args[0]);
        }
        return;
    }

    /* invalidate every written register */
    {
        int arg;

        for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
        {
            if (instr_arg_is_register (line, arg) &&
                instr_arg_writes (line, arg))
            {
                regs_unknown (line->args[arg]);
            }
        }
    }
}

/* find the instruction that references a label, or -1 */
static int find_label_ref_index (const char *label)
{
    int i;

    for (i = 0; i < nlines; i++)
    {
        int arg;

        if (! lines[i].active || lines[i].kind != LINE_INSTR)
        {
            continue;
        }

        for (arg = 0; arg < lines[i].argc && arg < OPT_MAX_ARGS; arg++)
        {
            if (instr_arg_is_label (&lines[i], arg) &&
                strcmp (lines[i].args[arg], label) == 0)
            {
                return (i);
            }
        }
    }
    return (-1);
}

/*
 * Look backwards for the last definition of "reg" and return its constant
 * value. Only simple "loada constant" definitions are accepted. Labels are
 * crossed only if they have exactly one reference and that reference cannot
 * skip the definition.
 */
static int find_known_def_before (int before, int reg, long long *value)
{
    int def_index = -1;
    int i;

    for (i = before - 1; i >= 0; i--)
    {
        Line *line = &lines[i];
        long long offset;
        long long const_value;

        if (! line->active || line->kind == LINE_BLANK ||
            line->kind == LINE_COMMENT)
        {
            continue;
        }

        if (line->kind == LINE_DIRECTIVE)
        {
            return (0);
        }

        if (line->kind == LINE_LABEL)
        {
            continue;
        }

        if (line->kind != LINE_INSTR)
        {
            continue;
        }

        if (strcmp (line->op, "intr0") == 0)
        {
            if (intr0_readonly (line))
            {
                continue;
            }
            return (0);
        }

        if (strcmp (line->op, "intr1") == 0 || is_jump_op (line->op) ||
            strcmp (line->op, "jsr") == 0 || strcmp (line->op, "rts") == 0 ||
            strcmp (line->op, "loadl") == 0)
        {
            return (0);
        }

        if (instr_writes_reg (line, reg))
        {
            if (! op_is (line, "loada") || line->argc < 3 ||
                ! parse_number_token (line->args[1], &offset) || offset != 0 ||
                ! data_const_value (line->args[0], &const_value))
            {
                return (0);
            }

            *value = const_value;
            def_index = i;
            break;
        }
    }

    if (def_index < 0)
    {
        return (0);
    }

    /* labels between the definition and the use must not skip it */
    for (i = def_index + 1; i < before; i++)
    {
        Line *line = &lines[i];

        if (! line->active || line->kind != LINE_LABEL)
        {
            continue;
        }

        if (has_indirect_label_ref (line->op) ||
            count_label_refs (line->op) != 1)
        {
            return (0);
        }

        if (find_label_ref_index (line->op) < def_index)
        {
            return (0);
        }
    }

    return (1);
}

static int try_simple_loop (int index)
{
    Line *inc = &lines[index];
    int cmp_index;
    int jmp_index;
    Line *cmp;
    Line *jmp;
    long long one_value;
    int label_index;
    int cond_reg;
    int counter_reg;
    int limit_reg;
    int i;
    char replacement[OPT_ARG_MAX * 4];

    if (! (op_is (inc, "addi") || op_is (inc, "subi")) || inc->argc < 3)
    {
        return (0);
    }

    if (! reg_match (inc->args[0], inc->args[2]))
    {
        return (0);
    }

    if (! regs_get (inc->args[1], &one_value))
    {
        int inc_reg = parse_register_token (inc->args[1]);

        if (inc_reg < 0 ||
            ! find_known_def_before (index, inc_reg, &one_value))
        {
            return (0);
        }
    }

    if (one_value != 1)
    {
        return (0);
    }

    cmp_index = next_instr_here (index);
    if (cmp_index < 0)
    {
        return (0);
    }
    cmp = &lines[cmp_index];

    if (op_is (inc, "addi"))
    {
        if (! op_is (cmp, "lsi") || cmp->argc < 3)
        {
            return (0);
        }
    }
    else
    {
        if (! op_is (cmp, "gri") || cmp->argc < 3)
        {
            return (0);
        }
    }

    if (! reg_match (cmp->args[0], inc->args[2]))
    {
        return (0);
    }

    jmp_index = next_instr_here (cmp_index);
    if (jmp_index < 0)
    {
        return (0);
    }
    jmp = &lines[jmp_index];

    if (! op_is (jmp, "jmpi") || jmp->argc < 2)
    {
        return (0);
    }

    if (! reg_match (jmp->args[0], cmp->args[2]))
    {
        return (0);
    }

    label_index = find_label (jmp->args[1]);
    if (label_index < 0 || label_index >= index)
    {
        return (0);
    }

    cond_reg = parse_register_token (cmp->args[2]);
    counter_reg = parse_register_token (cmp->args[0]);
    limit_reg = parse_register_token (cmp->args[1]);

    if (cond_reg < 0 || cond_reg == counter_reg || cond_reg == limit_reg)
    {
        return (0);
    }

    /* the condition register must not be read in the loop body */
    for (i = label_index + 1; i < index; i++)
    {
        if (lines[i].active && lines[i].kind == LINE_INSTR &&
            instr_reads_reg (&lines[i], cond_reg))
        {
            return (0);
        }
    }

    /* and it must not be read before it is overwritten after the loop */
    for (i = jmp_index + 1; i < nlines; i++)
    {
        if (! lines[i].active || lines[i].kind != LINE_INSTR)
        {
            continue;
        }

        if (instr_reads_reg (&lines[i], cond_reg))
        {
            return (0);
        }
        if (instr_writes_reg (&lines[i], cond_reg))
        {
            break;
        }
    }

    snprintf (replacement, sizeof (replacement), "%s %s, %s, %s",
              op_is (inc, "addi") ? "inclsijmpi" : "decgrijmpi",
              cmp->args[0], cmp->args[1], jmp->args[1]);

    free (inc->replace);
    inc->replace = xstrdup (replacement);
    lines[cmp_index].active = 0;
    lines[jmp_index].active = 0;

    regs_unknown (inc->args[0]);
    regs_unknown (cmp->args[2]);

    return (1);
}

static void optimize_simple_loops (void)
{
    int in_code = 0;
    int i;

    regs_invalidate_all ();

    for (i = 0; i < nlines; i++)
    {
        Line *line = &lines[i];

        if (! line->active)
        {
            continue;
        }

        if (line->kind == LINE_DIRECTIVE)
        {
            if (strcmp (line->op, ".code") == 0)
            {
                in_code = 1;
            }
            else if (strcmp (line->op, ".cend") == 0)
            {
                in_code = 0;
            }
            regs_invalidate_all ();
            continue;
        }

        if (line->kind == LINE_LABEL)
        {
            regs_invalidate_all ();
            continue;
        }

        if (! in_code || line->kind != LINE_INSTR)
        {
            continue;
        }

        if (try_simple_loop (i))
        {
            regs_invalidate_all ();
        }
        else
        {
            update_regs (line);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* rotated compiler for/while loops                                          */
/* ------------------------------------------------------------------------- */

static int used_regs[MAXREG];
static int scratch_reg = -1;
static int scratch2_reg = -1;
static char scratch_name[16];
static char scratch2_name[16];
static int last_loop_end = -1;

static void mark_reg_value (const char *token)
{
    int reg = parse_register_token (token);

    if (reg >= 0)
    {
        used_regs[reg] = 1;
    }
}

static void compute_used_regs (void)
{
    int i;

    memset (used_regs, 0, sizeof (used_regs));

    for (i = 0; i < nlines; i++)
    {
        Line *line = &lines[i];
        int arg;

        if (! line->active || line->kind != LINE_INSTR)
        {
            continue;
        }

        for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
        {
            if (instr_arg_is_register (line, arg))
            {
                mark_reg_value (line->args[arg]);
            }
        }
    }

    scratch_reg = -1;
    scratch2_reg = -1;
}

/* reserve two unused registers for the rotated loop */
static int alloc_scratch (void)
{
    int i;

    scratch_reg = -1;
    scratch2_reg = -1;

    for (i = 0; i < MAXREG; i++)
    {
        if (! used_regs[i])
        {
            if (scratch_reg < 0)
            {
                scratch_reg = i;
            }
            else
            {
                scratch2_reg = i;
                break;
            }
        }
    }

    if (scratch_reg < 0 || scratch2_reg < 0)
    {
        return (0);
    }

    used_regs[scratch_reg] = 1;
    used_regs[scratch2_reg] = 1;

    snprintf (scratch_name, sizeof (scratch_name), "%d", scratch_reg);
    snprintf (scratch2_name, sizeof (scratch2_name), "%d", scratch2_reg);
    return (1);
}

/*
 * Match a top tested loop at a label and rotate it into a bottom tested
 * loop which uses the inclsijmpi/decgrijmpi superopcode.
 */
static int try_rotated_loop (int label_index)
{
    Line *cond_label = &lines[label_index];
    int load_var_i;
    int load_lim_i;
    int cmp_i;
    int jmpi_i = -1;
    int spill = 0;
    int f_load_i = -1;
    int f_store_i = -1;
    int f_reload_i = -1;
    int false_jmp_i;
    int body_i;
    int end_i;
    int back_jmp_i;
    int store_i;
    int addr_i;
    int arith_i;
    int one_i;
    int var_i;
    int inc;

    Line *load_var;
    Line *load_lim;
    Line *cmp;
    Line *jmpi;
    Line *store;
    Line *addr;
    Line *arith;
    Line *one;
    Line *var;

    long long offset;
    long long one_value;
    int rc;
    int rl;
    int rcond;
    int rj = -1;
    int rt = -1;
    int rc2;
    int rone;
    int rd;
    int ra;

    char body_label[OPT_ARG_MAX];
    char end_label[OPT_ARG_MAX];
    char f_name[OPT_ARG_MAX];
    char replacement[OPT_ARG_MAX * 16];
    char prologue[OPT_ARG_MAX * 3];

    if (cond_label->kind != LINE_LABEL)
    {
        return (0);
    }

    load_var_i = next_instr_here (label_index);
    if (load_var_i < 0)
    {
        return (0);
    }
    load_var = &lines[load_var_i];
    if (! op_is (load_var, "loada") || load_var->argc < 3)
    {
        return (0);
    }
    if (! parse_number_token (load_var->args[1], &offset) || offset != 0)
    {
        return (0);
    }

    load_lim_i = next_instr_here (load_var_i);
    if (load_lim_i < 0)
    {
        return (0);
    }
    load_lim = &lines[load_lim_i];
    if (! op_is (load_lim, "loada") || load_lim->argc < 3)
    {
        return (0);
    }
    if (! parse_number_token (load_lim->args[1], &offset) || offset != 0)
    {
        return (0);
    }

    cmp_i = next_instr_here (load_lim_i);
    if (cmp_i < 0)
    {
        return (0);
    }
    cmp = &lines[cmp_i];
    if (! (op_is (cmp, "lsi") || op_is (cmp, "gri")) || cmp->argc < 3)
    {
        return (0);
    }

    if (! reg_match (cmp->args[0], load_var->args[2]) ||
        ! reg_match (cmp->args[1], load_lim->args[2]))
    {
        return (0);
    }

    inc = op_is (cmp, "lsi") ? 1 : 0;
    rcond = parse_register_token (cmp->args[2]);
    rc = parse_register_token (load_var->args[2]);
    rl = parse_register_token (load_lim->args[2]);

    if (rcond < 0 || rc < 0 || rl < 0)
    {
        return (0);
    }
    if (rc == rl || rc == rcond || rl == rcond)
    {
        return (0);
    }
    if (strcmp (load_var->args[0], load_lim->args[0]) == 0)
    {
        return (0);
    }

    /* condition spill to a memory variable "f" (optional) */
    {
        int next = next_instr_here (cmp_i);

        if (next < 0)
        {
            return (0);
        }

        if (op_is (&lines[next], "jmpi"))
        {
            jmpi_i = next;
        }
        else
        {
            if (! op_is (&lines[next], "load"))
            {
                return (0);
            }
            f_load_i = next;
            if (! parse_number_token (lines[f_load_i].args[1], &offset) ||
                offset != 0)
            {
                return (0);
            }

            f_store_i = next_instr_here (f_load_i);
            if (f_store_i < 0 || ! op_is (&lines[f_store_i], "pullqw") ||
                lines[f_store_i].argc < 3)
            {
                return (0);
            }
            if (! reg_match (lines[f_store_i].args[0], cmp->args[2]) ||
                ! reg_match (lines[f_store_i].args[1], lines[f_load_i].args[2]))
            {
                return (0);
            }
            if (! parse_number_token (lines[f_store_i].args[2], &offset) ||
                offset != 0)
            {
                return (0);
            }

            f_reload_i = next_instr_here (f_store_i);
            if (f_reload_i < 0 || ! op_is (&lines[f_reload_i], "loada") ||
                lines[f_reload_i].argc < 3)
            {
                return (0);
            }
            if (strcmp (lines[f_reload_i].args[0], lines[f_load_i].args[0]) != 0)
            {
                return (0);
            }
            if (! parse_number_token (lines[f_reload_i].args[1], &offset) ||
                offset != 0)
            {
                return (0);
            }

            jmpi_i = next_instr_here (f_reload_i);
            if (jmpi_i < 0 || ! op_is (&lines[jmpi_i], "jmpi"))
            {
                return (0);
            }

            strncpy (f_name, lines[f_load_i].args[0], OPT_ARG_MAX - 1);
            f_name[OPT_ARG_MAX - 1] = '\0';
            rt = parse_register_token (lines[f_load_i].args[2]);
            rj = parse_register_token (lines[f_reload_i].args[2]);
            spill = 1;
        }
    }

    jmpi = &lines[jmpi_i];
    if (jmpi->argc < 2)
    {
        return (0);
    }
    if (! reg_match (jmpi->args[0], spill ? lines[f_reload_i].args[2] : cmp->args[2]))
    {
        return (0);
    }

    strncpy (body_label, jmpi->args[1], OPT_ARG_MAX - 1);
    body_label[OPT_ARG_MAX - 1] = '\0';

    false_jmp_i = next_instr_here (jmpi_i);
    if (false_jmp_i < 0 || ! op_is (&lines[false_jmp_i], "jmp"))
    {
        return (0);
    }
    if (lines[false_jmp_i].argc < 1)
    {
        return (0);
    }
    strncpy (end_label, lines[false_jmp_i].args[0], OPT_ARG_MAX - 1);
    end_label[OPT_ARG_MAX - 1] = '\0';

    body_i = next_line_any (false_jmp_i);
    if (body_i < 0 || lines[body_i].kind != LINE_LABEL ||
        strcmp (lines[body_i].op, body_label) != 0)
    {
        return (0);
    }

    end_i = find_label (end_label);
    if (end_i < 0 || end_i <= body_i)
    {
        return (0);
    }

    /* the body label must only be used by the condition jump */
    if (count_label_refs (body_label) != 1)
    {
        return (0);
    }

    /* no computed jump or call into the condition label */
    if (has_indirect_label_ref (body_label) || has_indirect_label_ref (cond_label->op))
    {
        return (0);
    }

    /* loop back edge must be the last instruction before the end label */
    back_jmp_i = prev_instr_here (end_i);
    if (back_jmp_i < 0 || ! op_is (&lines[back_jmp_i], "jmp") ||
        lines[back_jmp_i].argc < 1 ||
        strcmp (lines[back_jmp_i].args[0], cond_label->op) != 0)
    {
        return (0);
    }
    store_i = prev_instr_here (back_jmp_i);
    addr_i = store_i >= 0 ? prev_instr_here (store_i) : -1;
    arith_i = addr_i >= 0 ? prev_instr_here (addr_i) : -1;
    one_i = arith_i >= 0 ? prev_instr_here (arith_i) : -1;
    var_i = one_i >= 0 ? prev_instr_here (one_i) : -1;

    if (var_i < 0 || var_i <= body_i)
    {
        return (0);
    }

    var = &lines[var_i];
    one = &lines[one_i];
    arith = &lines[arith_i];
    addr = &lines[addr_i];
    store = &lines[store_i];

    if (! op_is (var, "loada") || var->argc < 3)
    {
        return (0);
    }
    if (! parse_number_token (var->args[1], &offset) || offset != 0)
    {
        return (0);
    }
    if (strcmp (var->args[0], load_var->args[0]) != 0)
    {
        return (0);
    }

    if (! op_is (one, "loada") || one->argc < 3)
    {
        return (0);
    }
    if (! parse_number_token (one->args[1], &offset) || offset != 0)
    {
        return (0);
    }
    if (! data_const_value (one->args[0], &one_value) || one_value != 1)
    {
        return (0);
    }

    if (inc)
    {
        if (! op_is (arith, "addi") || arith->argc < 3)
        {
            return (0);
        }
    }
    else
    {
        if (! op_is (arith, "subi") || arith->argc < 3)
        {
            return (0);
        }
    }

    if (! reg_match (arith->args[0], var->args[2]) ||
        ! reg_match (arith->args[1], one->args[2]))
    {
        return (0);
    }

    if (! op_is (addr, "load") || addr->argc < 3)
    {
        return (0);
    }
    if (! parse_number_token (addr->args[1], &offset) || offset != 0)
    {
        return (0);
    }
    if (strcmp (addr->args[0], load_var->args[0]) != 0)
    {
        return (0);
    }

    if (! op_is (store, "pullqw") || store->argc < 3)
    {
        return (0);
    }
    if (! reg_match (store->args[0], arith->args[2]) ||
        ! reg_match (store->args[1], addr->args[2]))
    {
        return (0);
    }
    if (! parse_number_token (store->args[2], &offset) || offset != 0)
    {
        return (0);
    }

    rc2 = parse_register_token (var->args[2]);
    rone = parse_register_token (one->args[2]);
    rd = parse_register_token (arith->args[2]);
    ra = parse_register_token (addr->args[2]);

    if (rc2 < 0 || rone < 0 || rd < 0 || ra < 0)
    {
        return (0);
    }

    /* temporaries of the removed bottom must be written before they are read
       in the loop body */
    if (reg_read_before_write (body_i + 1, var_i - 1, rc) ||
        reg_read_before_write (body_i + 1, var_i - 1, rl) ||
        reg_read_before_write (body_i + 1, var_i - 1, rc2) ||
        reg_read_before_write (body_i + 1, var_i - 1, rone) ||
        reg_read_before_write (body_i + 1, var_i - 1, rd) ||
        reg_read_before_write (body_i + 1, var_i - 1, ra))
    {
        return (0);
    }

    /* the body must not store to the loop counter through its address */
    if (data_addr_used (body_i + 1, var_i - 1, load_var->args[0]))
    {
        return (0);
    }

    if (spill)
    {
        if (reg_read_before_write (body_i + 1, var_i - 1, rt) ||
            reg_read_before_write (body_i + 1, var_i - 1, rj) ||
            reg_read_before_write (body_i + 1, var_i - 1, rcond) ||
            data_read_before_store (body_i + 1, var_i - 1, f_name))
        {
            return (0);
        }
    }

    if (! alloc_scratch ())
    {
        return (0);
    }

    /* use an unused register as the loop counter in the rotated loop */
    {
        char cmp_replacement[OPT_ARG_MAX * 4];
        char init_copy[OPT_ARG_MAX * 2];

        snprintf (cmp_replacement, sizeof (cmp_replacement), "%s %s, %s, %s",
                  inc ? "lsi" : "gri", scratch_name, load_lim->args[2],
                  cmp->args[2]);
        free (lines[cmp_i].replace);
        lines[cmp_i].replace = xstrdup (cmp_replacement);

        snprintf (init_copy, sizeof (init_copy), "movi %s, %s",
                  load_var->args[2], scratch_name);
        append_extra (&lines[load_var_i].post, init_copy);
    }

    /* build the new body prologue: store the current counter value */
    snprintf (prologue, sizeof (prologue),
              "load %s, 0, %s\npullqw %s, %s, 0",
              load_var->args[0], scratch2_name, scratch_name, scratch2_name);

    /* build the new loop bottom */
    {
        size_t pos = 0;
        int n;

        n = snprintf (replacement + pos, sizeof (replacement) - pos,
                      "loada %s, 0, %s\n%s %s, %s, %s",
                      load_lim->args[0], load_lim->args[2],
                      inc ? "inclsijmpi" : "decgrijmpi",
                      scratch_name, load_lim->args[2], body_label);
        if (n < 0)
        {
            return (0);
        }
        pos += (size_t) n;

        /* on exit the condition is false, like in the original loop */
        n = snprintf (replacement + pos, sizeof (replacement) - pos,
                      "\n%s %s, %s, %s",
                      inc ? "lsi" : "gri", scratch_name,
                      load_lim->args[2], cmp->args[2]);
        if (n < 0)
        {
            return (0);
        }
        pos += (size_t) n;

        if (spill)
        {
            n = snprintf (replacement + pos, sizeof (replacement) - pos,
                          "\nload %s, 0, %s\npullqw %s, %s, 0",
                          f_name, scratch2_name, cmp->args[2], scratch2_name);
            if (n < 0)
            {
                return (0);
            }
            pos += (size_t) n;
        }

        n = snprintf (replacement + pos, sizeof (replacement) - pos,
                      "\nload %s, 0, %s\npullqw %s, %s, 0\njmp %s",
                      load_var->args[0], scratch2_name,
                      scratch_name, scratch2_name, end_label);
        if (n < 0)
        {
            return (0);
        }
    }

    append_extra (&lines[body_i].post, prologue);

    free (var->replace);
    var->replace = xstrdup (replacement);

    lines[one_i].active = 0;
    lines[arith_i].active = 0;
    lines[addr_i].active = 0;
    lines[store_i].active = 0;
    lines[back_jmp_i].active = 0;

    last_loop_end = end_i;

    return (1);
}

static void optimize_rotated_loops (void)
{
    int in_code = 0;
    int i;
    int active_count = 0;
    int active_end[MAXREG / 2];
    int active_r1[MAXREG / 2];
    int active_r2[MAXREG / 2];

    for (i = 0; i < nlines; i++)
    {
        Line *line = &lines[i];
        int j = 0;

        while (j < active_count)
        {
            if (active_end[j] <= i)
            {
                used_regs[active_r1[j]] = 0;
                used_regs[active_r2[j]] = 0;

                active_count--;
                active_end[j] = active_end[active_count];
                active_r1[j] = active_r1[active_count];
                active_r2[j] = active_r2[active_count];
            }
            else
            {
                j++;
            }
        }

        if (! line->active)
        {
            continue;
        }

        if (line->kind == LINE_DIRECTIVE)
        {
            if (strcmp (line->op, ".code") == 0)
            {
                in_code = 1;
            }
            else if (strcmp (line->op, ".cend") == 0)
            {
                in_code = 0;
            }
            continue;
        }

        if (in_code && line->kind == LINE_LABEL)
        {
            if (try_rotated_loop (i) && active_count < MAXREG / 2)
            {
                active_end[active_count] = last_loop_end;
                active_r1[active_count] = scratch_reg;
                active_r2[active_count] = scratch2_reg;
                active_count++;
            }
        }
    }
}

/* ------------------------------------------------------------------------- */
/* -O2/-O4: register promotion of function local scalars                     */
/* ------------------------------------------------------------------------- */

#define PROMO_READ     1
#define PROMO_STORE    2

typedef struct
{
    int line;                                   /* loada/loadd or load line */
    int store_line;                             /* pullqw/pulld line, -1 for a read */
    int kind;
    int value_reg;                              /* destination/source register */
    int addr_reg;                               /* address register of a store */
} PromoRef;

typedef struct
{
    int symbol;                                 /* index into symbols[] */
    int reg;                                    /* assigned register, -1 if none */
    int owner;                                  /* function label line */
    int owner_end;                              /* first line after the function */
    int is_double;
    int reads;
    int writes;
    int rts_count;
    int full;                                   /* value lives only in the register */
    int entry_load;
    int exit_store;
    int dead;                                   /* no reads: remove all stores */
    int benefit;
    int enabled;
    PromoRef *refs;
    int nrefs;
} PromoPlan;

typedef struct
{
    int *edges;
    int nedges;
    int cap;
    int unsafe;                                 /* computed call: don't optimize */
} FuncNode;

static int *func_starts = NULL;
static int nfunc_starts = 0;
static FuncNode *func_nodes = NULL;
static int func_nodes_count = 0;
static int *func_skip = NULL;

static int compare_int (const void *a, const void *b)
{
    int ia = *(const int *) a;
    int ib = *(const int *) b;

    return (ia < ib) ? -1 : (ia > ib);
}

static void add_function_start (int index)
{
    int i;

    if (index < 0)
    {
        return;
    }

    for (i = 0; i < nfunc_starts; i++)
    {
        if (func_starts[i] == index)
        {
            return;
        }
    }

    func_starts = (int *) xrealloc (func_starts,
                                    (size_t) (nfunc_starts + 1) * sizeof (int));
    func_starts[nfunc_starts] = index;
    nfunc_starts++;
}

static int function_ordinal (int line_index)
{
    int i;

    for (i = 0; i < nfunc_starts; i++)
    {
        if (func_starts[i] == line_index)
        {
            return (i);
        }
    }
    return (-1);
}

static int function_region_end (int start)
{
    int i;

    for (i = 0; i < nfunc_starts; i++)
    {
        if (func_starts[i] == start)
        {
            return ((i + 1 < nfunc_starts) ? func_starts[i + 1] : nlines);
        }
    }
    return (nlines);
}

/*
 * Function entries are "main", every label used by jsr or loadl and every
 * label which has compiler generated local variables ("name@function").
 */
static void build_function_starts (void)
{
    int i;

    free (func_starts);
    func_starts = NULL;
    nfunc_starts = 0;

    add_function_start (find_label (":main"));

    for (i = 0; i < nlines; i++)
    {
        Line *line = &lines[i];
        int arg;

        if (! line->active || line->kind != LINE_INSTR)
        {
            continue;
        }

        if (! (op_is (line, "jsr") || op_is (line, "loadl")))
        {
            continue;
        }

        for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
        {
            if (instr_arg_is_label (line, arg))
            {
                add_function_start (find_label (line->args[arg]));
            }
        }
    }

    for (i = 0; i < nsymbols; i++)
    {
        DataSymbol *sym = &symbols[i];
        char label[OPT_ARG_MAX + 2];
        char *at;

        if ((sym->type != 'Q' && sym->type != 'F') || sym->count != 1 ||
            sym->ambiguous)
        {
            continue;
        }

        at = strrchr (sym->name, '@');
        if (at == NULL || at[1] == '\0' || strcmp (at + 1, "main") == 0)
        {
            continue;
        }

        snprintf (label, sizeof (label), ":%s", at + 1);
        add_function_start (find_label (label));
    }

    if (nfunc_starts > 1)
    {
        qsort (func_starts, (size_t) nfunc_starts, sizeof (int), compare_int);
    }
}

/*
 * The compiler also supports locals which are simply named with the function
 * name as a suffix ("numsquare_int" inside ":square_int") instead of using
 * "name@function".  Find the longest matching function label.  A non-empty
 * prefix is required, so a global variable which happens to have the same
 * name as a function is not treated as local.
 */
static int function_label_for_suffix (const char *name)
{
    int best = -1;
    size_t best_len = 0;
    size_t name_len = strlen (name);
    int i;

    for (i = 0; i < nfunc_starts; i++)
    {
        Line *line = &lines[func_starts[i]];
        const char *label = line->op;
        size_t label_len;

        if (label[0] == ':')
        {
            label++;
        }
        label_len = strlen (label);
        if (label_len == 0 || name_len <= label_len ||
            strcmp (label, "main") == 0)
        {
            continue;
        }
        if (strcmp (name + name_len - label_len, label) != 0)
        {
            continue;
        }
        if (label_len > best_len)
        {
            best = func_starts[i];
            best_len = label_len;
        }
    }
    return (best);
}

static void build_call_graph (void)
{
    int i;

    if (func_nodes != NULL)
    {
        for (i = 0; i < func_nodes_count; i++)
        {
            free (func_nodes[i].edges);
        }
        free (func_nodes);
    }

    func_nodes = (FuncNode *) xmalloc ((size_t) nfunc_starts * sizeof (FuncNode));
    memset (func_nodes, 0, (size_t) nfunc_starts * sizeof (FuncNode));
    func_nodes_count = nfunc_starts;

    for (i = 0; i < nfunc_starts; i++)
    {
        int end = function_region_end (func_starts[i]);
        int j;

        for (j = func_starts[i]; j < end; j++)
        {
            Line *line = &lines[j];
            int arg;

            if (! line->active || line->kind != LINE_INSTR)
            {
                continue;
            }

            if (op_is (line, "jsra") || op_is (line, "loadl") ||
                op_is (line, "jmpa"))
            {
                func_nodes[i].unsafe = 1;
            }

            if (! op_is (line, "jsr"))
            {
                continue;
            }

            for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
            {
                if (instr_arg_is_label (line, arg))
                {
                    int target = function_ordinal (find_label (line->args[arg]));

                    if (target >= 0)
                    {
                        FuncNode *node = &func_nodes[i];

                        if (node->nedges >= node->cap)
                        {
                            node->cap = (node->cap == 0) ? 4 : node->cap * 2;
                            node->edges = (int *) xrealloc (node->edges,
                                                            (size_t) node->cap * sizeof (int));
                        }
                        node->edges[node->nedges] = target;
                        node->nedges++;
                    }
                    else
                    {
                        /* a call to code we can't see may do anything */
                        func_nodes[i].unsafe = 1;
                    }
                }
            }
        }
    }
}

static int function_reaches (int from, int current, int *visited)
{
    int i;

    if (current == from)
    {
        return (1);
    }
    if (visited[current])
    {
        return (0);
    }
    visited[current] = 1;

    for (i = 0; i < func_nodes[current].nedges; i++)
    {
        if (function_reaches (from, func_nodes[current].edges[i], visited))
        {
            return (1);
        }
    }
    return (0);
}

/*
 * A function which is called recursively can't keep its locals in registers,
 * because the recursive invocation would overwrite them. Computed calls are
 * treated as potentially recursive.
 */
static void compute_recursive_functions (void)
{
    int i;
    int changed;

    free (func_skip);
    func_skip = (int *) xmalloc ((size_t) nfunc_starts * sizeof (int));
    memset (func_skip, 0, (size_t) nfunc_starts * sizeof (int));

    /* functions with computed calls can run arbitrary code */
    for (i = 0; i < nfunc_starts; i++)
    {
        if (func_nodes[i].unsafe)
        {
            func_skip[i] = 1;
        }
    }

    /*
     * A caller of an unsafe function can be re-entered by that function
     * through a computed call, so it must be treated as unsafe too.
     */
    do
    {
        changed = 0;
        for (i = 0; i < nfunc_starts; i++)
        {
            int j;

            if (func_skip[i])
            {
                continue;
            }
            for (j = 0; j < func_nodes[i].nedges; j++)
            {
                if (func_skip[func_nodes[i].edges[j]])
                {
                    func_skip[i] = 1;
                    changed = 1;
                    break;
                }
            }
        }
    } while (changed);

    for (i = 0; i < nfunc_starts; i++)
    {
        int *visited;
        int j;

        if (func_skip[i])
        {
            continue;
        }

        visited = (int *) xmalloc ((size_t) nfunc_starts * sizeof (int));
        memset (visited, 0, (size_t) nfunc_starts * sizeof (int));

        for (j = 0; j < func_nodes[i].nedges; j++)
        {
            if (function_reaches (i, func_nodes[i].edges[j], visited))
            {
                func_skip[i] = 1;
                break;
            }
        }
        free (visited);
    }
}

static int instr_arg_is_data (const Line *line, int arg)
{
    int index = opcode_index (line->op);

    if (index < 0)
    {
        return (0);
    }
    return (opcode_type (index, arg) == DATA);
}

/*
 * Return the function index which contains the given line, or -1 if the line
 * is not inside a known function.
 */
static int function_index_for_line (int line_index)
{
    int result = -1;
    int i;

    for (i = 0; i < nfunc_starts; i++)
    {
        if (func_starts[i] <= line_index)
        {
            result = i;
        }
        else
        {
            break;
        }
    }
    return (result);
}

/*
 * Find the only function which references a plain global variable.  A global
 * which is used by exactly one function can be treated like a function local:
 * no other function can observe its memory image.
 */
static int symbol_function_owner (int sym_index)
{
    DataSymbol *sym = &symbols[sym_index];
    int owner = -1;
    int in_code = 0;
    int i;

    for (i = 0; i < nlines; i++)
    {
        Line *line = &lines[i];
        int arg;

        if (! line->active)
        {
            continue;
        }

        if (line->kind == LINE_DIRECTIVE)
        {
            if (strcmp (line->op, ".code") == 0)
            {
                in_code = 1;
            }
            else if (strcmp (line->op, ".cend") == 0)
            {
                in_code = 0;
            }
            continue;
        }

        if (! in_code || line->kind != LINE_INSTR)
        {
            continue;
        }

        for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
        {
            int ord;
            int start;

            if (strcmp (line->args[arg], sym->name) != 0)
            {
                continue;
            }
            if (! instr_arg_is_data (line, arg))
            {
                return (-1);
            }

            ord = function_index_for_line (i);
            if (ord < 0)
            {
                return (-1);
            }
            start = func_starts[ord];
            if (owner < 0)
            {
                owner = start;
            }
            else if (owner != start)
            {
                return (-1);
            }
        }
    }
    return (owner);
}

static int is_name_char (char c)
{
    return (isalnum ((unsigned char) c) || c == '_' || c == '@');
}

static int text_mentions_name (const char *text, const char *name)
{
    size_t len = strlen (name);
    const char *ptr = text;

    while ((ptr = strstr (ptr, name)) != NULL)
    {
        char before = (ptr == text) ? ' ' : ptr[-1];
        char after = ptr[len];

        if (! is_name_char (before) && ! is_name_char (after))
        {
            return (1);
        }
        ptr++;
    }
    return (0);
}

/*
 * A read only global can be kept in a register inside every function which
 * reads it: its value never changes, so each function may load its own copy.
 * Only plain "loada"/"loadd" reads are accepted.  Taking the address, storing
 * or passing the variable to another instruction would make this unsafe.
 */
static int symbol_is_readonly (int sym_index)
{
    DataSymbol *sym = &symbols[sym_index];
    int in_code = 0;
    int refs = 0;
    int i;

    for (i = 0; i < nlines; i++)
    {
        Line *line = &lines[i];
        int arg;

        if (! line->active)
        {
            continue;
        }

        if ((line->pre != NULL && text_mentions_name (line->pre, sym->name)) ||
            (line->replace != NULL && text_mentions_name (line->replace, sym->name)) ||
            (line->post != NULL && text_mentions_name (line->post, sym->name)))
        {
            return (0);
        }

        if (line->kind == LINE_DIRECTIVE)
        {
            if (strcmp (line->op, ".code") == 0)
            {
                in_code = 1;
            }
            else if (strcmp (line->op, ".cend") == 0)
            {
                in_code = 0;
            }
            continue;
        }

        if (! in_code || line->kind != LINE_INSTR)
        {
            continue;
        }

        for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
        {
            long long offset;
            int reg;

            if (strcmp (line->args[arg], sym->name) != 0)
            {
                continue;
            }
            if (! instr_arg_is_data (line, arg))
            {
                return (0);
            }
            if (! ((sym->type == 'Q' && op_is (line, "loada")) ||
                   (sym->type == 'F' && op_is (line, "loadd"))))
            {
                return (0);
            }
            if (line->argc < 3 ||
                ! parse_number_token (line->args[1], &offset) || offset != 0)
            {
                return (0);
            }
            reg = parse_register_token (line->args[2]);
            if (reg < 0)
            {
                return (0);
            }
            refs++;
        }
    }
    return (refs > 0);
}

/*
 * The loop mask marks every line which belongs to a backward branch region,
 * so a value read just once but on a hot loop path is still worth keeping in
 * a register.
 */
static int *loop_mask = NULL;

static int jump_target_line (const Line *line)
{
    if (op_is (line, "jmp") && line->argc >= 1)
    {
        return (find_label (line->args[0]));
    }
    if (op_is (line, "jmpi") && line->argc >= 2)
    {
        return (find_label (line->args[1]));
    }
    if ((op_is (line, "inclsijmpi") || op_is (line, "decgrijmpi")) &&
        line->argc >= 3)
    {
        return (find_label (line->args[2]));
    }
    return (-1);
}

static void build_loop_mask (void)
{
    int i;

    free (loop_mask);
    loop_mask = NULL;

    if (nlines <= 0)
    {
        return;
    }

    loop_mask = (int *) xmalloc ((size_t) nlines * sizeof (int));
    memset (loop_mask, 0, (size_t) nlines * sizeof (int));

    for (i = 0; i < nlines; i++)
    {
        Line *line = &lines[i];
        int target;

        if (! line->active || line->kind != LINE_INSTR)
        {
            continue;
        }

        target = jump_target_line (line);
        if (target >= 0 && target < i)
        {
            int j;

            for (j = target; j < i; j++)
            {
                loop_mask[j] = 1;
            }
        }
    }
}

static int line_in_loop (int line)
{
    return (loop_mask != NULL && line >= 0 && line < nlines && loop_mask[line]);
}

/*
 * Function starts are only known for called functions.  Some assembler files
 * contain additional, uncalled functions after the end of a called function.
 * They are not part of that function, so stop at its first "rts" when
 * promoting globals.  This keeps a load at the real function entry.
 */
static int promotion_function_end (int owner, int owner_end)
{
    int i;

    for (i = owner; i < owner_end && i < nlines; i++)
    {
        Line *line = &lines[i];

        if (line->active && line->kind == LINE_INSTR && op_is (line, "rts"))
        {
            return (i + 1);
        }
    }
    return (owner_end);
}

static int refs_in_loop (const PromoRef *refs, int nrefs)
{
    int i;

    for (i = 0; i < nrefs; i++)
    {
        if (line_in_loop (refs[i].line))
        {
            return (1);
        }
    }
    return (0);
}

/*
 * Collect the simple reads of a read only variable inside one function.
 */
static int collect_readonly_refs (int sym_index, int owner, int owner_end,
                                  PromoRef **refs_out, int *nrefs_out)
{
    DataSymbol *sym = &symbols[sym_index];
    PromoRef *refs = NULL;
    int nrefs = 0;
    int ref_cap = 0;
    int end = promotion_function_end (owner, owner_end);
    int i;

    for (i = owner; i < end && i < nlines; i++)
    {
        Line *line = &lines[i];
        int arg;

        if (! line->active || line->kind != LINE_INSTR)
        {
            continue;
        }

        for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
        {
            long long offset;
            int value_reg;

            if (strcmp (line->args[arg], sym->name) != 0)
            {
                continue;
            }
            if (! instr_arg_is_data (line, arg) ||
                ! ((sym->type == 'Q' && op_is (line, "loada")) ||
                   (sym->type == 'F' && op_is (line, "loadd"))) ||
                line->argc < 3 ||
                ! parse_number_token (line->args[1], &offset) || offset != 0)
            {
                free (refs);
                return (0);
            }

            value_reg = parse_register_token (line->args[2]);
            if (value_reg < 0)
            {
                free (refs);
                return (0);
            }

            if (nrefs >= ref_cap)
            {
                ref_cap = (ref_cap == 0) ? 8 : ref_cap * 2;
                refs = (PromoRef *) xrealloc (refs,
                                              (size_t) ref_cap * sizeof (PromoRef));
            }
            refs[nrefs].line = i;
            refs[nrefs].store_line = -1;
            refs[nrefs].kind = PROMO_READ;
            refs[nrefs].value_reg = value_reg;
            refs[nrefs].addr_reg = -1;
            nrefs++;
        }
    }

    *refs_out = refs;
    *nrefs_out = nrefs;
    return (nrefs > 0);
}

/*
 * Find the scalar store which uses the address loaded by "load name, 0, addr".
 * The compiler may compute the value to store between the address load and
 * the store, so the store is not necessarily the next instruction.  The
 * address register must not be touched on the way and must not be read
 * again after the store.
 */
static int find_address_store (int load_i, int owner_end, int addr_reg,
                               const char *store_op, int is_double)
{
    int i;
    int steps = 0;

    for (i = load_i + 1; i < owner_end; i++)
    {
        Line *line = &lines[i];

        if (! line->active)
        {
            continue;
        }
        if (line->kind == LINE_BLANK || line->kind == LINE_COMMENT)
        {
            continue;
        }
        if (line->kind != LINE_INSTR)
        {
            return (-1);
        }

        if (steps++ > 64)
        {
            return (-1);
        }

        /* control flow or a call can change the address register */
        if (op_is (line, "jmp") || op_is (line, "jmpi") ||
            op_is (line, "jmpa") || op_is (line, "jsr") ||
            op_is (line, "jsra") || op_is (line, "rts") ||
            op_is (line, "intr0") || op_is (line, "intr1") ||
            op_is (line, "inclsijmpi") || op_is (line, "decgrijmpi"))
        {
            return (-1);
        }

        if (instr_reads_ireg (line, addr_reg))
        {
            long long offset;
            int value_reg;

            if (! op_is (line, store_op) || line->argc < 3 ||
                ! reg_match (line->args[1], lines[load_i].args[2]) ||
                ! parse_number_token (line->args[2], &offset) || offset != 0)
            {
                return (-1);
            }

            value_reg = parse_register_token (line->args[0]);
            /* double values and addresses live in separate register banks */
            if (value_reg < 0 || (! is_double && value_reg == addr_reg) ||
                line->replace != NULL || line->post != NULL)
            {
                return (-1);
            }
            return (i);
        }

        if (instr_writes_ireg (line, addr_reg))
        {
            return (-1);
        }
    }
    return (-1);
}

/*
 * Collect and classify all uses of a candidate variable.  Only value reads
 * ("loada"/"loadd") and scalar stores ("load" + "pullqw"/"pulld") are
 * accepted.  Anything else, like taking the address of the variable, makes
 * the variable unsuitable for register promotion.
 */
static int classify_symbol (int sym_index, int owner, int owner_end,
                            PromoRef **refs_out, int *nrefs_out,
                            int *reads_out, int *writes_out, int *rts_out)
{
    DataSymbol *sym = &symbols[sym_index];
    PromoRef *refs = NULL;
    int nrefs = 0;
    int ref_cap = 0;
    int reads = 0;
    int writes = 0;
    int rts_count = 0;
    int in_code = 0;
    int i;

    for (i = 0; i < nlines; i++)
    {
        Line *line = &lines[i];
        int arg;

        if (! line->active)
        {
            continue;
        }

        if (line->kind == LINE_DIRECTIVE)
        {
            if (strcmp (line->op, ".code") == 0)
            {
                in_code = 1;
            }
            else if (strcmp (line->op, ".cend") == 0)
            {
                in_code = 0;
            }
            continue;
        }

        /* text created by earlier passes must not use this variable */
        if ((line->replace != NULL && text_mentions_name (line->replace, sym->name)) ||
            (line->post != NULL && text_mentions_name (line->post, sym->name)))
        {
            goto fail;
        }

        if (! in_code || line->kind != LINE_INSTR)
        {
            continue;
        }

        if (op_is (line, "rts") && i >= owner && i < owner_end)
        {
            rts_count++;
        }

        for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
        {
            if (strcmp (line->args[arg], sym->name) != 0)
            {
                continue;
            }

            if (! instr_arg_is_data (line, arg))
            {
                goto fail;
            }
            if (i < owner || i >= owner_end)
            {
                goto fail;
            }
            if (line->replace != NULL || line->post != NULL)
            {
                goto fail;
            }

            if ((op_is (line, "loada") && sym->type == 'Q') ||
                (op_is (line, "loadd") && sym->type == 'F'))
            {
                long long offset;
                int value_reg;

                if (line->argc < 3 ||
                    ! parse_number_token (line->args[1], &offset) || offset != 0)
                {
                    goto fail;
                }

                value_reg = parse_register_token (line->args[2]);
                if (value_reg < 0)
                {
                    goto fail;
                }

                if (nrefs >= ref_cap)
                {
                    ref_cap = (ref_cap == 0) ? 8 : ref_cap * 2;
                    refs = (PromoRef *) xrealloc (refs,
                                                  (size_t) ref_cap * sizeof (PromoRef));
                }
                refs[nrefs].line = i;
                refs[nrefs].store_line = -1;
                refs[nrefs].kind = PROMO_READ;
                refs[nrefs].value_reg = value_reg;
                refs[nrefs].addr_reg = -1;
                nrefs++;
                reads++;
            }
            else if (op_is (line, "load"))
            {
                long long offset;
                int store_i;
                int addr_reg;
                int value_reg;
                Line *store;

                if (line->argc < 3 ||
                    ! parse_number_token (line->args[1], &offset) || offset != 0)
                {
                    goto fail;
                }

                store_i = find_address_store (i, owner_end,
                                              parse_register_token (line->args[2]),
                                              (sym->type == 'Q') ? "pullqw" : "pulld",
                                              (sym->type == 'F'));
                if (store_i < 0)
                {
                    goto fail;
                }
                store = &lines[store_i];

                addr_reg = parse_register_token (line->args[2]);
                value_reg = parse_register_token (store->args[0]);
                if (addr_reg < 0 || value_reg < 0)
                {
                    goto fail;
                }

                /* the address register must be dead after the store */
                if (reg_read_before_write_ireg (store_i + 1, owner_end - 1,
                                                addr_reg))
                {
                    goto fail;
                }

                if (nrefs >= ref_cap)
                {
                    ref_cap = (ref_cap == 0) ? 8 : ref_cap * 2;
                    refs = (PromoRef *) xrealloc (refs,
                                                  (size_t) ref_cap * sizeof (PromoRef));
                }
                refs[nrefs].line = i;
                refs[nrefs].store_line = store_i;
                refs[nrefs].kind = PROMO_STORE;
                refs[nrefs].value_reg = value_reg;
                refs[nrefs].addr_reg = addr_reg;
                nrefs++;
                writes++;
            }
            else
            {
                goto fail;
            }
        }
    }

    *refs_out = refs;
    *nrefs_out = nrefs;
    *reads_out = reads;
    *writes_out = writes;
    *rts_out = rts_count;
    return (1);

fail:
    free (refs);
    return (0);
}

/*
 * Return the possible successors of a line inside [start, end).  The return
 * value is the number of successors, or -1 for an unknown computed jump.
 */
static int promo_successors (int i, int start, int end, int out[2])
{
    Line *line = &lines[i];
    int target;

    if (! line->active || line->kind != LINE_INSTR)
    {
        out[0] = i + 1;
        return ((i + 1 < end) ? 1 : 0);
    }

    if (op_is (line, "rts"))
    {
        return (0);
    }
    if (op_is (line, "jmpa"))
    {
        return (-1);
    }
    if (op_is (line, "jmp"))
    {
        target = (line->argc >= 1) ? find_label (line->args[0]) : -1;
        if (target < start || target >= end)
        {
            return (0);
        }
        out[0] = target;
        return (1);
    }
    if (op_is (line, "jmpi"))
    {
        int n = 0;

        target = (line->argc >= 2) ? find_label (line->args[1]) : -1;
        if (target >= start && target < end)
        {
            out[n++] = target;
        }
        if (i + 1 < end)
        {
            out[n++] = i + 1;
        }
        return (n);
    }
    if (op_is (line, "inclsijmpi") || op_is (line, "decgrijmpi"))
    {
        int n = 0;

        target = (line->argc >= 3) ? find_label (line->args[2]) : -1;
        if (target >= start && target < end)
        {
            out[n++] = target;
        }
        if (i + 1 < end)
        {
            out[n++] = i + 1;
        }
        return (n);
    }

    out[0] = i + 1;
    return ((i + 1 < end) ? 1 : 0);
}

/*
 * Check that every read of the variable is preceded by a write on all
 * control flow paths.  If so, neither an entry load nor exit stores are
 * needed and the variable can live in its register only.
 */
static int promo_must_define (int start, int end, PromoRef *refs, int nrefs)
{
    int n = end - start;
    int *in;
    int *out;
    int *is_store;
    int *pred_head;
    int *edge_to;
    int *edge_next;
    int edge_count = 0;
    int edge_cap;
    int *queue;
    int *queued;
    int qhead = 0;
    int qtail = 0;
    int qcap;
    int i;
    int ok = 1;

    if (n <= 0)
    {
        return (0);
    }

    in = (int *) xmalloc ((size_t) n * sizeof (int));
    out = (int *) xmalloc ((size_t) n * sizeof (int));
    is_store = (int *) xmalloc ((size_t) n * sizeof (int));
    pred_head = (int *) xmalloc ((size_t) n * sizeof (int));
    queued = (int *) xmalloc ((size_t) n * sizeof (int));
    qcap = n * 2 + 16;
    queue = (int *) xmalloc ((size_t) qcap * sizeof (int));
    edge_cap = n * 2 + 16;
    edge_to = (int *) xmalloc ((size_t) edge_cap * sizeof (int));
    edge_next = (int *) xmalloc ((size_t) edge_cap * sizeof (int));

    memset (is_store, 0, (size_t) n * sizeof (int));
    memset (queued, 0, (size_t) n * sizeof (int));

    for (i = 0; i < n; i++)
    {
        in[i] = 1;
        out[i] = 1;
        pred_head[i] = -1;
    }

    for (i = 0; i < nrefs; i++)
    {
        if (refs[i].kind == PROMO_STORE)
        {
            int rel = refs[i].store_line - start;

            if (rel >= 0 && rel < n)
            {
                is_store[rel] = 1;
            }
        }
    }

    for (i = start; i < end; i++)
    {
        int succ[2];
        int count = promo_successors (i, start, end, succ);
        int j;

        if (count < 0)
        {
            ok = 0;
            goto done;
        }

        for (j = 0; j < count; j++)
        {
            int to = succ[j];

            if (to < start || to >= end)
            {
                continue;
            }
            if (edge_count >= edge_cap)
            {
                edge_cap *= 2;
                edge_to = (int *) xrealloc (edge_to,
                                            (size_t) edge_cap * sizeof (int));
                edge_next = (int *) xrealloc (edge_next,
                                              (size_t) edge_cap * sizeof (int));
            }
            edge_to[edge_count] = i - start;
            edge_next[edge_count] = pred_head[to - start];
            pred_head[to - start] = edge_count;
            edge_count++;
        }
    }

    for (i = 0; i < n; i++)
    {
        queued[i] = 1;
        queue[qtail++] = i;
    }
    in[0] = 0;

    while (qhead < qtail)
    {
        int rel = queue[qhead++];
        int new_in = (rel == 0) ? 0 : 1;
        int new_out;
        int edge;

        queued[rel] = 0;

        for (edge = pred_head[rel]; edge >= 0; edge = edge_next[edge])
        {
            new_in &= out[edge_to[edge]];
        }

        new_out = new_in || is_store[rel];

        if (new_in != in[rel] || new_out != out[rel])
        {
            int succ[2];
            int count;
            int j;

            in[rel] = new_in;
            out[rel] = new_out;

            count = promo_successors (start + rel, start, end, succ);
            for (j = 0; j < count; j++)
            {
                int to = succ[j];

                if (to < start || to >= end)
                {
                    continue;
                }
                if (! queued[to - start])
                {
                    if (qtail >= qcap)
                    {
                        qcap *= 2;
                        queue = (int *) xrealloc (queue,
                                                  (size_t) qcap * sizeof (int));
                    }
                    queued[to - start] = 1;
                    queue[qtail++] = to - start;
                }
            }
        }
    }

    for (i = 0; i < nrefs; i++)
    {
        if (refs[i].kind == PROMO_READ)
        {
            int rel = refs[i].line - start;

            if (rel < 0 || rel >= n || ! in[rel])
            {
                ok = 0;
                break;
            }
        }
    }

done:
    free (in);
    free (out);
    free (is_store);
    free (pred_head);
    free (edge_to);
    free (edge_next);
    free (queue);
    free (queued);
    return (ok);
}

static int compare_plans (const void *a, const void *b)
{
    const PromoPlan *pa = (const PromoPlan *) a;
    const PromoPlan *pb = (const PromoPlan *) b;

    return (pb->benefit - pa->benefit);
}

/* find a register which is not used anywhere in the program, skip r0 */
static int promo_alloc_reg (void)
{
    int i;

    for (i = 1; i < MAXREG; i++)
    {
        if (! used_regs[i])
        {
            used_regs[i] = 1;
            return (i);
        }
    }
    return (-1);
}

static int promo_ref_line (const PromoPlan *plan, int line)
{
    int i;

    for (i = 0; i < plan->nrefs; i++)
    {
        if (plan->refs[i].line == line ||
            plan->refs[i].store_line == line)
        {
            return (1);
        }
    }
    return (0);
}

/*
 * If all stores and reads already use the same register, the memory image
 * of the variable is only a copy of that register.  Keep the value in the
 * register and remove the memory accesses entirely.  This is only safe for
 * straight line code without calls: otherwise the original register may be
 * clobbered between a store and a read.  If the check fails the variable is
 * promoted to a fresh, globally unused register instead.
 */
static int promo_natural_reg (PromoPlan *plan, const int *natural_used)
{
    int r;
    int first_store;
    int last_read = -1;
    int i;

    if (! plan->full || plan->dead || plan->entry_load || plan->exit_store ||
        plan->nrefs < 2 || plan->refs[0].kind != PROMO_STORE)
    {
        return (-1);
    }

    r = plan->refs[0].value_reg;
    if (r <= 0 || natural_used[r])
    {
        return (-1);
    }

    first_store = plan->refs[0].store_line;
    if (first_store < 0 || first_store >= nlines)
    {
        return (-1);
    }

    for (i = 0; i < plan->nrefs; i++)
    {
        PromoRef *ref = &plan->refs[i];

        if (ref->value_reg != r || ref->line < 0 || ref->line >= nlines)
        {
            return (-1);
        }
        if (lines[ref->line].pre != NULL ||
            lines[ref->line].replace != NULL ||
            lines[ref->line].post != NULL)
        {
            return (-1);
        }

        if (ref->kind == PROMO_STORE)
        {
            if (ref->store_line < 0 || ref->store_line >= nlines ||
                lines[ref->store_line].pre != NULL ||
                lines[ref->store_line].replace != NULL ||
                lines[ref->store_line].post != NULL)
            {
                return (-1);
            }
        }
        else if (ref->kind == PROMO_READ)
        {
            if (ref->line > last_read)
            {
                last_read = ref->line;
            }
        }
    }

    if (last_read < 0 || first_store >= last_read)
    {
        return (-1);
    }

    for (i = first_store + 1; i < last_read; i++)
    {
        Line *line = &lines[i];

        if (! line->active || promo_ref_line (plan, i))
        {
            continue;
        }
        if (line->kind == LINE_BLANK || line->kind == LINE_COMMENT)
        {
            continue;
        }
        if (line->kind != LINE_INSTR ||
            line->pre != NULL || line->replace != NULL || line->post != NULL)
        {
            return (-1);
        }

        /* unknown instructions, calls and control flow can clobber r */
        if (opcode_index (line->op) < 0 || is_jump_op (line->op) ||
            op_is (line, "intr0") || op_is (line, "intr1") ||
            op_is (line, "loadl") ||
            op_is (line, "inclsijmpi") || op_is (line, "decgrijmpi"))
        {
            return (-1);
        }

        if (plan->is_double)
        {
            if (instr_writes_dreg (line, r))
            {
                return (-1);
            }
        }
        else if (instr_writes_ireg (line, r))
        {
            return (-1);
        }
    }

    return (r);
}

static void mark_text_registers (const char *text)
{
    char buf[OPT_LINE_MAX];
    const char *start = text;

    while (*start != '\0')
    {
        const char *newline = strchr (start, '\n');
        size_t len = (newline != NULL) ? (size_t) (newline - start) : strlen (start);
        Line tmp;
        int arg;

        if (len >= sizeof (buf))
        {
            len = sizeof (buf) - 1;
        }
        memcpy (buf, start, len);
        buf[len] = '\0';

        memset (&tmp, 0, sizeof (tmp));
        tmp.text = buf;
        parse_line (&tmp);

        if (tmp.kind == LINE_INSTR)
        {
            for (arg = 0; arg < tmp.argc && arg < OPT_MAX_ARGS; arg++)
            {
                if (instr_arg_is_register (&tmp, arg))
                {
                    mark_reg_value (tmp.args[arg]);
                }
            }
        }

        if (newline == NULL)
        {
            break;
        }
        start = newline + 1;
    }
}

/* recompute the registers used by the final text of the current passes */
static void recompute_used_regs (void)
{
    int i;

    memset (used_regs, 0, sizeof (used_regs));

    for (i = 0; i < nlines; i++)
    {
        Line *line = &lines[i];
        int arg;

        if (! line->active)
        {
            continue;
        }

        if (line->kind == LINE_INSTR)
        {
            for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
            {
                if (instr_arg_is_register (line, arg))
                {
                    mark_reg_value (line->args[arg]);
                }
            }
        }

        if (line->pre != NULL)
        {
            mark_text_registers (line->pre);
        }
        if (line->replace != NULL)
        {
            mark_text_registers (line->replace);
        }
        if (line->post != NULL)
        {
            mark_text_registers (line->post);
        }
    }
}

static void apply_promo_plan (PromoPlan *plan, int addr_tmp)
{
    DataSymbol *sym = &symbols[plan->symbol];
    char text[OPT_ARG_MAX * 2];
    int i;

    for (i = 0; i < plan->nrefs; i++)
    {
        PromoRef *ref = &plan->refs[i];
        Line *load = &lines[ref->line];

        if (plan->dead)
        {
            load->active = 0;
            lines[ref->store_line].active = 0;
            continue;
        }

        if (ref->kind == PROMO_READ)
        {
            if (ref->value_reg == plan->reg)
            {
                load->active = 0;
            }
            else
            {
                snprintf (text, sizeof (text), "%s %d, %s",
                          plan->is_double ? "movd" : "movi",
                          plan->reg, load->args[2]);
                free (load->replace);
                load->replace = xstrdup (text);
            }
        }
        else if (ref->kind == PROMO_STORE)
        {
            Line *store = &lines[ref->store_line];

            if (ref->value_reg == plan->reg)
            {
                load->active = 0;
                store->active = 0;
            }
            else
            {
                /*
                 * The value is only available after the instructions which
                 * compute the store source.  Replace the store itself, not
                 * the address load, so the assignment keeps its position.
                 */
                snprintf (text, sizeof (text), "%s %s, %d",
                          plan->is_double ? "movd" : "movi",
                          store->args[0], plan->reg);
                free (store->replace);
                store->replace = xstrdup (text);
                load->active = 0;
            }
        }
    }

    if (plan->dead)
    {
        return;
    }

    if (plan->entry_load)
    {
        snprintf (text, sizeof (text), "%s %s, 0, %d",
                  plan->is_double ? "loadd" : "loada", sym->name, plan->reg);
        append_extra (&lines[plan->owner].post, text);
    }

    if (plan->exit_store && addr_tmp >= 0)
    {
        char store_text[OPT_ARG_MAX * 3];

        snprintf (store_text, sizeof (store_text),
                  "load %s, 0, %d\n%s %d, %d, 0",
                  sym->name, addr_tmp,
                  plan->is_double ? "pulld" : "pullqw",
                  plan->reg, addr_tmp);

        for (i = plan->owner; i < plan->owner_end && i < nlines; i++)
        {
            if (lines[i].active && lines[i].kind == LINE_INSTR &&
                op_is (&lines[i], "rts"))
            {
                append_extra (&lines[i].pre, store_text);
            }
        }
    }
}

static void promote_function_locals (int level)
{
    PromoPlan *plans = NULL;
    int nplans = 0;
    int plan_cap = 0;
    int addr_tmp = -1;
    int need_addr_tmp = 0;
    int aggressive = (level >= 4);
    int natural_used[MAXREG];
    int i;

    build_function_starts ();
    if (nfunc_starts == 0)
    {
        return;
    }

    build_call_graph ();
    compute_recursive_functions ();
    recompute_used_regs ();
    build_loop_mask ();
    memset (natural_used, 0, sizeof (natural_used));

    for (i = 0; i < nsymbols; i++)
    {
        DataSymbol *sym = &symbols[i];
        PromoRef *refs = NULL;
        PromoPlan plan;
        char label[OPT_ARG_MAX + 2];
        char *at;
        int nrefs = 0;
        int reads = 0;
        int writes = 0;
        int rts_count = 0;
        int owner;
        int owner_ord;
        int owner_end;
        int global_owner = 0;

        if ((sym->type != 'Q' && sym->type != 'F') || sym->count != 1 ||
            sym->ambiguous)
        {
            continue;
        }

        /*
         * A read only variable can be loaded once per function into a fresh
         * register, even if several functions read it.  Only promote it when
         * it is read more than once or on a loop path, so a single cold load
         * is left alone.
         */
        if (aggressive && symbol_is_readonly (i))
        {
            int f;

            for (f = 0; f < nfunc_starts; f++)
            {
                int fstart = func_starts[f];
                int fend = function_region_end (fstart);
                PromoRef *rrefs = NULL;
                int rnrefs = 0;
                int in_loop;

                if (! collect_readonly_refs (i, fstart, fend, &rrefs, &rnrefs))
                {
                    free (rrefs);
                    continue;
                }

                in_loop = refs_in_loop (rrefs, rnrefs);
                if (rnrefs < 2 && ! in_loop)
                {
                    free (rrefs);
                    continue;
                }

                memset (&plan, 0, sizeof (plan));
                plan.symbol = i;
                plan.owner = fstart;
                plan.owner_end = fend;
                plan.is_double = (sym->type == 'F');
                plan.reads = rnrefs;
                plan.writes = 0;
                plan.refs = rrefs;
                plan.nrefs = rnrefs;
                plan.reg = -1;
                plan.enabled = 1;
                plan.full = 1;
                plan.entry_load = 1;
                plan.benefit = rnrefs * 2 + (in_loop ? 8 : 0);

                if (nplans >= plan_cap)
                {
                    plan_cap = (plan_cap == 0) ? 32 : plan_cap * 2;
                    plans = (PromoPlan *) xrealloc (plans,
                                                    (size_t) plan_cap * sizeof (PromoPlan));
                }
                plans[nplans] = plan;
                nplans++;
            }
            continue;
        }

        at = strrchr (sym->name, '@');
        if (at != NULL)
        {
            if (at[1] == '\0' || strcmp (at + 1, "main") == 0)
            {
                continue;
            }

            snprintf (label, sizeof (label), ":%s", at + 1);
            owner = find_label (label);
        }
        else if (aggressive)
        {
            owner = function_label_for_suffix (sym->name);
            if (owner < 0)
            {
                owner = symbol_function_owner (i);
                global_owner = (owner >= 0);
            }
        }
        else
        {
            continue;
        }

        if (owner < 0)
        {
            continue;
        }

        owner_ord = function_ordinal (owner);
        if (owner_ord < 0 || func_skip[owner_ord])
        {
            continue;
        }

        owner_end = function_region_end (owner);
        if (global_owner)
        {
            owner_end = promotion_function_end (owner, owner_end);
        }

        if (! classify_symbol (i, owner, owner_end, &refs, &nrefs,
                               &reads, &writes, &rts_count))
        {
            continue;
        }

        if (writes == 0)
        {
            free (refs);
            continue;
        }

        memset (&plan, 0, sizeof (plan));
        plan.symbol = i;
        plan.owner = owner;
        plan.owner_end = owner_end;
        plan.is_double = (sym->type == 'F');
        plan.reads = reads;
        plan.writes = writes;
        plan.rts_count = rts_count;
        plan.refs = refs;
        plan.nrefs = nrefs;
        plan.reg = -1;
        plan.enabled = 1;

        if (reads == 0)
        {
            /* only stores: the values are never read, remove them */
            plan.dead = 1;
            plan.full = 1;
            plan.benefit = writes * 2 + 1;
        }
        else if (promo_must_define (owner, owner_end, refs, nrefs))
        {
            /* every read is preceded by a write: keep it in the register */
            plan.full = 1;
            plan.benefit = writes * 3 + reads;
        }
        else
        {
            int gain;

            if (rts_count <= 0)
            {
                free (refs);
                continue;
            }

            gain = writes - 1 - 2 * rts_count;
            if (gain <= 0)
            {
                /*
                 * A global which lives in a single function is still worth
                 * promoting when it is touched on a loop path: the entry
                 * load and exit store are then paid only once per call.
                 */
                if (! global_owner || ! refs_in_loop (refs, nrefs))
                {
                    free (refs);
                    continue;
                }
                gain = 1;
            }

            plan.entry_load = 1;
            plan.exit_store = 1;
            plan.benefit = gain;
            need_addr_tmp = 1;
        }

        if (nplans >= plan_cap)
        {
            plan_cap = (plan_cap == 0) ? 32 : plan_cap * 2;
            plans = (PromoPlan *) xrealloc (plans,
                                            (size_t) plan_cap * sizeof (PromoPlan));
        }
        plans[nplans] = plan;
        nplans++;
    }

    if (nplans == 0)
    {
        free (loop_mask);
        loop_mask = NULL;
        return;
    }

    if (need_addr_tmp)
    {
        addr_tmp = promo_alloc_reg ();
        if (addr_tmp < 0)
        {
            for (i = 0; i < nplans; i++)
            {
                if (plans[i].exit_store)
                {
                    plans[i].enabled = 0;
                }
            }
        }
    }

    qsort (plans, (size_t) nplans, sizeof (PromoPlan), compare_plans);

    /*
     * At -O4 prefer the register which already holds the value.  This makes
     * the original store/read pair disappear without introducing moves.
     */
    if (aggressive)
    {
        for (i = 0; i < nplans; i++)
        {
            int reg;

            if (! plans[i].enabled || plans[i].dead)
            {
                continue;
            }

            reg = promo_natural_reg (&plans[i], natural_used);
            if (reg >= 0)
            {
                plans[i].reg = reg;
                natural_used[reg] = 1;
                used_regs[reg] = 1;
            }
        }
    }

    for (i = 0; i < nplans; i++)
    {
        if (! plans[i].enabled || plans[i].dead || plans[i].reg >= 0)
        {
            continue;
        }

        plans[i].reg = promo_alloc_reg ();
        if (plans[i].reg < 0)
        {
            plans[i].enabled = 0;
        }
    }

    for (i = 0; i < nplans; i++)
    {
        if (plans[i].enabled)
        {
            apply_promo_plan (&plans[i], addr_tmp);
        }
        free (plans[i].refs);
    }

    free (plans);
    free (loop_mask);
    loop_mask = NULL;
}

/* ------------------------------------------------------------------------- */
/* -O5: inline small leaf functions                                          */
/* ------------------------------------------------------------------------- */

/*
 * Earlier passes attach replacement lines to an existing line with the
 * pre/replace/post fields instead of rewriting the line array.  The inliner
 * works on real instruction lines, so expand these fields first.  This keeps
 * the output identical while letting promoted locals be inlined as well.
 */
static void append_text_block (Line **arr, int *count, int *cap,
                               const char *text)
{
    const char *start = text;

    while (start != NULL)
    {
        const char *end = strchr (start, '\n');
        char buf[OPT_LINE_MAX];
        size_t len = (end != NULL) ? (size_t) (end - start) : strlen (start);

        if (len >= sizeof (buf))
        {
            die ("replacement line too long");
        }
        memcpy (buf, start, len);
        buf[len] = '\0';
        append_parsed_line (arr, count, cap, buf);

        start = (end != NULL) ? end + 1 : NULL;
    }
}

static int materialize_transformations (void)
{
    Line *newlines = NULL;
    int newcount = 0;
    int newcap = 0;
    int changed = 0;
    int i;

    for (i = 0; i < nlines; i++)
    {
        Line *line = &lines[i];

        if (line->active &&
            (line->pre != NULL || line->replace != NULL ||
             line->post != NULL))
        {
            changed = 1;
            break;
        }
    }

    if (! changed)
    {
        return (0);
    }

    for (i = 0; i < nlines; i++)
    {
        Line *line = &lines[i];

        if (! line->active)
        {
            continue;
        }

        if (line->pre != NULL)
        {
            append_text_block (&newlines, &newcount, &newcap, line->pre);
        }

        if (line->replace != NULL)
        {
            append_text_block (&newlines, &newcount, &newcap, line->replace);
        }
        else
        {
            append_parsed_line (&newlines, &newcount, &newcap, line->text);
        }

        if (line->post != NULL)
        {
            append_text_block (&newlines, &newcount, &newcap, line->post);
        }
    }

    for (i = 0; i < nlines; i++)
    {
        free (lines[i].text);
        free (lines[i].pre);
        free (lines[i].replace);
        free (lines[i].post);
    }
    free (lines);

    lines = newlines;
    nlines = newcount;
    line_cap = newcap;
    return (1);
}

typedef struct
{
    int start;
    int end;
    int return_line;                            /* push of the return value */
    char return_type;                           /* 0, 'Q' or 'F' */
    int return_reg;
    int pops[INLINE_MAX_ARGS];                  /* argument pops in body order */
    int npops;
    int body_count;                             /* instructions excluding rts */
} InlineFunc;

/*
 * A function is only a candidate if its body is straight line code with a
 * single exit.  The compiler passes arguments with leading "stpop" and the
 * return value with a "stpush" directly before "rts".  Anything more complex
 * is left alone.
 */
static int inline_is_prefix_op (const Line *line)
{
    return (op_is (line, "stpopi") || op_is (line, "stpopd") ||
            op_is (line, "load") || op_is (line, "loada") ||
            op_is (line, "loadd") || op_is (line, "pullb") ||
            op_is (line, "pullw") || op_is (line, "pulldw") ||
            op_is (line, "pullqw") || op_is (line, "pulld") ||
            op_is (line, "movi") || op_is (line, "movd"));
}

static int inline_analyze_function (int start, InlineFunc *info)
{
    int idx[INLINE_SCAN_MAX];
    int nidx = 0;
    int pos;
    int prefix_end;
    int i;

    memset (info, 0, sizeof (*info));
    info->start = start;
    info->end = function_region_end (start);
    info->return_line = -1;

    if (start < 0 || start >= nlines ||
        lines[start].kind != LINE_LABEL ||
        lines[start].pre != NULL || lines[start].replace != NULL ||
        lines[start].post != NULL)
    {
        return (0);
    }

    for (i = start + 1; i < info->end; i++)
    {
        Line *line = &lines[i];

        if (! line->active)
        {
            continue;
        }
        if (line->kind == LINE_BLANK || line->kind == LINE_COMMENT)
        {
            continue;
        }
        if (line->kind == LINE_DIRECTIVE && strcmp (line->op, ".cend") == 0)
        {
            info->end = i;
            break;
        }
        if (line->kind != LINE_INSTR)
        {
            return (0);
        }
        if (line->pre != NULL || line->replace != NULL || line->post != NULL)
        {
            return (0);
        }
        if (opcode_index (line->op) < 0)
        {
            return (0);
        }

        /*
         * Interrupt calls use the "ALL" register type, so the integer and
         * double register banks can't be rewritten safely.  Computed calls
         * and control flow are too complex for this conservative inliner.
         */
        if (op_is (line, "intr0") || op_is (line, "intr1") ||
            op_is (line, "loadl") || op_is (line, "jsr") ||
            op_is (line, "jsra") || op_is (line, "jmp") ||
            op_is (line, "jmpi") || op_is (line, "jmpa") ||
            op_is (line, "inclsijmpi") || op_is (line, "decgrijmpi"))
        {
            return (0);
        }

        /* byte and word stack values are not part of the supported ABI */
        if ((is_stpush_op (line->op) || is_stpop_op (line->op)) &&
            ! op_is (line, "stpushi") && ! op_is (line, "stpushd") &&
            ! op_is (line, "stpopi") && ! op_is (line, "stpopd"))
        {
            return (0);
        }

        if (nidx >= INLINE_SCAN_MAX)
        {
            return (0);
        }
        idx[nidx++] = i;
    }

    /*
     * The compiler starts every function with "loada zero, 0, 0".  -O4 may
     * prepend "loadd" entry loads for promoted double locals, so skip those
     * and require that the conventional data address load is still present.
     */
    pos = 0;
    while (pos < nidx && op_is (&lines[idx[pos]], "loadd"))
    {
        pos++;
    }
    if (pos >= nidx || ! op_is (&lines[idx[pos]], "loada"))
    {
        return (0);
    }

    /* find the trailing run of "rts" (the compiler emits up to two) */
    pos = nidx - 1;
    while (pos >= 0 && op_is (&lines[idx[pos]], "rts"))
    {
        pos--;
    }
    if (pos == nidx - 1)
    {
        return (0);
    }

    for (i = 0; i <= pos; i++)
    {
        if (op_is (&lines[idx[i]], "rts"))
        {
            return (0);
        }
    }

    if (pos >= 0 && (op_is (&lines[idx[pos]], "stpushi") ||
                     op_is (&lines[idx[pos]], "stpushd")))
    {
        info->return_line = idx[pos];
        info->return_type = op_is (&lines[idx[pos]], "stpushd") ? 'F' : 'Q';
        info->return_reg = parse_register_token (lines[idx[pos]].args[0]);
        if (info->return_reg < 0)
        {
            return (0);
        }
        pos--;
    }

    info->body_count = pos + 1;
    if (info->body_count <= 0 || info->body_count > inline_max_body)
    {
        return (0);
    }

    /*
     * Argument pops must be in the leading prologue.  Locals which were
     * promoted by -O4 may insert moves between the pops, so allow the usual
     * load/store/move instructions here.
     */
    prefix_end = 0;
    while (prefix_end <= pos &&
           inline_is_prefix_op (&lines[idx[prefix_end]]))
    {
        Line *line = &lines[idx[prefix_end]];

        if (op_is (line, "stpopi") || op_is (line, "stpopd"))
        {
            if (info->npops >= INLINE_MAX_ARGS)
            {
                return (0);
            }
            info->pops[info->npops] = idx[prefix_end];
            info->npops++;
        }
        prefix_end++;
    }

    for (i = prefix_end; i <= pos; i++)
    {
        if (is_stpop_op (lines[idx[i]].op))
        {
            return (0);
        }
    }

    for (i = 0; i <= pos; i++)
    {
        if (is_stpush_op (lines[idx[i]].op))
        {
            return (0);
        }
    }

    return (1);
}

static int inline_count_calls (int target)
{
    const char *label = lines[target].op;
    int count = 0;
    int i;

    for (i = 0; i < nlines; i++)
    {
        Line *line = &lines[i];
        int arg;

        if (! line->active || line->kind != LINE_INSTR ||
            line->replace != NULL || ! op_is (line, "jsr"))
        {
            continue;
        }

        for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
        {
            if (instr_arg_is_label (line, arg) &&
                strcmp (line->args[arg], label) == 0)
            {
                count++;
                break;
            }
        }
    }
    return (count);
}

static void inline_caller_bounds (int line, int *start, int *end)
{
    int i;

    *start = 0;
    *end = nlines;

    for (i = 0; i < nfunc_starts; i++)
    {
        if (func_starts[i] <= line)
        {
            *start = func_starts[i];
        }
        else
        {
            *end = func_starts[i];
            break;
        }
    }
}

static int inline_next_code (int from, int end)
{
    int i;

    for (i = from + 1; i < end; i++)
    {
        if (! lines[i].active)
        {
            continue;
        }
        if (lines[i].kind == LINE_BLANK || lines[i].kind == LINE_COMMENT)
        {
            continue;
        }
        return (i);
    }
    return (-1);
}

/*
 * Collect the compiler's data stack pushes which belong to the call block.
 * The scan stops at control flow, labels and stack pops, so pushes from an
 * earlier statement are not mixed in.
 */
static int inline_collect_pushes (int jsr, int caller_start,
                                  int *push_lines, char *push_types,
                                  int *count_out)
{
    int count = 0;
    int i;
    int j;

    for (i = jsr - 1; i >= caller_start; i--)
    {
        Line *line = &lines[i];

        if (! line->active)
        {
            continue;
        }
        if (line->kind == LINE_BLANK || line->kind == LINE_COMMENT)
        {
            continue;
        }
        if (line->kind != LINE_INSTR)
        {
            break;
        }
        if (line->replace != NULL || line->pre != NULL || line->post != NULL)
        {
            break;
        }

        if (op_is (line, "stpushi") || op_is (line, "stpushd"))
        {
            if (count >= INLINE_MAX_PUSHES)
            {
                return (0);
            }
            push_lines[count] = i;
            push_types[count] = op_is (line, "stpushd") ? 'D' : 'I';
            count++;
            continue;
        }
        if (is_stpush_op (line->op))
        {
            return (0);
        }
        if (is_stpop_op (line->op))
        {
            break;
        }
        if (is_jump_op (line->op) ||
            op_is (line, "inclsijmpi") || op_is (line, "decgrijmpi") ||
            op_is (line, "intr0") || op_is (line, "intr1") ||
            op_is (line, "loadl"))
        {
            break;
        }
    }

    /* the scan ran backwards, restore source order */
    for (i = 0, j = count - 1; i < j; i++, j--)
    {
        int tmp_line = push_lines[i];
        char tmp_type = push_types[i];

        push_lines[i] = push_lines[j];
        push_types[i] = push_types[j];
        push_lines[j] = tmp_line;
        push_types[j] = tmp_type;
    }

    *count_out = count;
    return (1);
}

/*
 * Find the matching restore block.  The compiler restores doubles first and
 * then integers, both in decreasing register order.  A block is only used if
 * it is contiguous and matches every saved register.
 */
static int inline_find_restore (int start, int end,
                                const int *save_lines,
                                const char *save_types, int nsaves,
                                int *restore_lines)
{
    int i;

    if (nsaves == 0)
    {
        return (1);
    }

    for (i = start; i < end; )
    {
        Line *line = &lines[i];
        int run[INLINE_MAX_PUSHES];
        int nrun = 0;
        int j;

        if (! line->active)
        {
            i++;
            continue;
        }
        if (line->kind == LINE_BLANK || line->kind == LINE_COMMENT)
        {
            i++;
            continue;
        }
        if (line->kind != LINE_INSTR ||
            line->replace != NULL || line->pre != NULL || line->post != NULL)
        {
            break;
        }
        if (is_jump_op (line->op) ||
            op_is (line, "inclsijmpi") || op_is (line, "decgrijmpi") ||
            op_is (line, "intr0") || op_is (line, "intr1") ||
            op_is (line, "loadl"))
        {
            break;
        }

        if (! is_stpop_op (line->op))
        {
            i++;
            continue;
        }

        j = i;
        while (j < end)
        {
            Line *run_line = &lines[j];

            if (! run_line->active)
            {
                j++;
                continue;
            }
            if (run_line->kind == LINE_BLANK || run_line->kind == LINE_COMMENT)
            {
                j++;
                continue;
            }
            if (run_line->kind != LINE_INSTR ||
                run_line->replace != NULL ||
                run_line->pre != NULL || run_line->post != NULL ||
                ! is_stpop_op (run_line->op))
            {
                break;
            }
            if (nrun >= INLINE_MAX_PUSHES)
            {
                return (0);
            }
            run[nrun++] = j;
            j++;
        }

        if (nrun == nsaves)
        {
            int ok = 1;
            int k;

            for (k = 0; k < nsaves; k++)
            {
                Line *save = &lines[save_lines[nsaves - 1 - k]];
                Line *restore = &lines[run[k]];

                if ((save_types[nsaves - 1 - k] == 'I' &&
                     ! op_is (restore, "stpopi")) ||
                    (save_types[nsaves - 1 - k] == 'D' &&
                     ! op_is (restore, "stpopd")) ||
                    ! reg_match (save->args[0], restore->args[0]))
                {
                    ok = 0;
                    break;
                }
            }

            if (ok)
            {
                for (k = 0; k < nsaves; k++)
                {
                    restore_lines[k] = run[k];
                }
                return (1);
            }
        }

        i = (j > i) ? j : i + 1;
    }
    return (0);
}

static int inline_line_removable (int index)
{
    Line *line = &lines[index];

    return (line->active && line->kind == LINE_INSTR &&
            line->pre == NULL && line->replace == NULL &&
            line->post == NULL);
}

/*
 * Removing a caller register save is only safe if the saved register is not
 * written before its matching restore.  The inlined body uses fresh registers,
 * so only the surrounding caller code is checked here.
 */
static int inline_saved_regs_untouched (int first_save, int last_restore,
                                        int jsr, int result_line,
                                        const int *save_lines,
                                        const char *save_types, int nsaves,
                                        const int *restore_lines)
{
    int i;

    for (i = 0; i < nsaves; i++)
    {
        int reg = parse_register_token (lines[save_lines[i]].args[0]);
        int k;

        if (reg < 0)
        {
            return (0);
        }

        for (k = first_save + 1; k < last_restore; k++)
        {
            Line *line = &lines[k];
            int skip = 0;
            int j;

            if (k == jsr || k == result_line)
            {
                continue;
            }
            for (j = 0; j < nsaves; j++)
            {
                if (k == save_lines[j] || k == restore_lines[j])
                {
                    skip = 1;
                    break;
                }
            }
            if (skip || ! line->active || line->kind != LINE_INSTR)
            {
                continue;
            }

            if (save_types[i] == 'I')
            {
                if (instr_writes_ireg (line, reg))
                {
                    return (0);
                }
            }
            else if (instr_writes_dreg (line, reg))
            {
                return (0);
            }
        }
    }
    return (1);
}

static int inline_args_match (const InlineFunc *info,
                              const char *push_types, int first_arg)
{
    int i;

    for (i = 0; i < info->npops; i++)
    {
        int argpos = first_arg + info->npops - 1 - i;
        Line *pop = &lines[info->pops[i]];
        char want = op_is (pop, "stpopd") ? 'D' : 'I';

        if (argpos < first_arg || push_types[argpos] != want)
        {
            return (0);
        }
    }
    return (1);
}

/*
 * Removing a push is only safe if its register still holds the pushed value
 * at the call site.  It also rejects interrupts between argument evaluation
 * and the call, because they may use the data stack.
 */
static int inline_arg_regs_stable (int jsr, const int *push_lines,
                                   const char *push_types, int first_arg,
                                   int nargs)
{
    int i;

    for (i = 0; i < nargs; i++)
    {
        int push_i = push_lines[first_arg + i];
        int reg = parse_register_token (lines[push_i].args[0]);
        int k;

        if (reg < 0)
        {
            return (0);
        }

        for (k = push_i + 1; k < jsr; k++)
        {
            Line *line = &lines[k];

            if (! line->active)
            {
                continue;
            }
            if (line->kind == LINE_BLANK || line->kind == LINE_COMMENT)
            {
                continue;
            }
            if (line->kind != LINE_INSTR ||
                line->replace != NULL || line->pre != NULL ||
                line->post != NULL)
            {
                return (0);
            }
            if (op_is (line, "intr0") || op_is (line, "intr1"))
            {
                return (0);
            }
            if (push_types[first_arg + i] == 'I')
            {
                if (instr_writes_ireg (line, reg))
                {
                    return (0);
                }
            }
            else if (instr_writes_dreg (line, reg))
            {
                return (0);
            }
        }
    }
    return (1);
}

/*
 * The compiler starts every function with "loada zero, 0, 0".  If the body
 * never reads register 0 before writing it, that initial load can be dropped
 * from the inlined copy.
 */
static int inline_initial_load_unneeded (const InlineFunc *info)
{
    int defined = 0;
    int first = 1;
    int i;

    for (i = info->start + 1; i < info->end; i++)
    {
        Line *line = &lines[i];

        if (! line->active)
        {
            continue;
        }
        if (line->kind == LINE_BLANK || line->kind == LINE_COMMENT)
        {
            continue;
        }
        if (line->kind != LINE_INSTR)
        {
            return (0);
        }

        if (first)
        {
            int dest;

            first = 0;
            if (! op_is (line, "loada"))
            {
                return (0);
            }
            dest = parse_register_token (line->args[2]);
            if (dest != 0)
            {
                return (0);
            }
            continue;
        }

        if (instr_reads_ireg (line, 0) && ! defined)
        {
            return (0);
        }
        if (instr_writes_ireg (line, 0))
        {
            defined = 1;
        }
    }
    return (1);
}

/* allocate fresh registers for every register used by the inlined body */
static int inline_map_registers (const InlineFunc *info, int *map_i,
                                 int *map_d)
{
    int i;
    int arg;

    for (i = 0; i < MAXREG; i++)
    {
        map_i[i] = -1;
        map_d[i] = -1;
    }

    for (i = info->start + 1; i < info->end; i++)
    {
        Line *line = &lines[i];

        if (! line->active)
        {
            continue;
        }
        if (line->kind == LINE_BLANK || line->kind == LINE_COMMENT)
        {
            continue;
        }
        if (line->kind != LINE_INSTR)
        {
            return (0);
        }
        if (line->pre != NULL || line->replace != NULL || line->post != NULL)
        {
            return (0);
        }
        if (op_is (line, "rts"))
        {
            continue;
        }

        for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
        {
            int is_i = instr_arg_is_ireg (line, arg);
            int is_d = instr_arg_is_dreg (line, arg);
            int reg;

            if (is_i && is_d)
            {
                return (0);
            }
            if (! is_i && ! is_d)
            {
                if (instr_arg_is_register (line, arg))
                {
                    return (0);
                }
                continue;
            }

            reg = parse_register_token (line->args[arg]);
            if (reg < 0)
            {
                return (0);
            }

            if (is_i)
            {
                if (map_i[reg] < 0)
                {
                    map_i[reg] = promo_alloc_reg ();
                    if (map_i[reg] < 0)
                    {
                        return (0);
                    }
                }
            }
            else if (map_d[reg] < 0)
            {
                map_d[reg] = promo_alloc_reg ();
                if (map_d[reg] < 0)
                {
                    return (0);
                }
            }
        }
    }
    return (1);
}

static int inline_format_line (const Line *line, const int *map_i,
                               const int *map_d, char *buf, size_t bufsize)
{
    int pos;
    int arg;

    pos = snprintf (buf, bufsize, "%s", line->op);
    if (pos < 0 || (size_t) pos >= bufsize)
    {
        return (0);
    }

    for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
    {
        const char *value = line->args[arg];
        char number[32];
        int is_i = instr_arg_is_ireg (line, arg);
        int is_d = instr_arg_is_dreg (line, arg);
        int written;

        if (is_i && is_d)
        {
            return (0);
        }
        if (is_i)
        {
            int reg = parse_register_token (line->args[arg]);

            if (reg < 0 || map_i[reg] < 0)
            {
                return (0);
            }
            snprintf (number, sizeof (number), "%d", map_i[reg]);
            value = number;
        }
        else if (is_d)
        {
            int reg = parse_register_token (line->args[arg]);

            if (reg < 0 || map_d[reg] < 0)
            {
                return (0);
            }
            snprintf (number, sizeof (number), "%d", map_d[reg]);
            value = number;
        }
        else if (instr_arg_is_register (line, arg))
        {
            return (0);
        }

        written = snprintf (buf + pos, bufsize - (size_t) pos, "%s%s",
                            (arg == 0) ? " " : ", ", value);
        if (written < 0 || (size_t) (pos + written) >= bufsize)
        {
            return (0);
        }
        pos += written;
    }
    return (1);
}

static int inline_build_body (const InlineFunc *info, const int *map_i,
                              const int *map_d, const int *push_lines,
                              int first_arg, int result_dest,
                              char **body_out)
{
    char *body = NULL;
    int pop_pos = 0;
    int first = 1;
    int skip_initial_load = inline_initial_load_unneeded (info);
    int i;

    for (i = info->start + 1; i < info->end; i++)
    {
        Line *line = &lines[i];

        if (! line->active)
        {
            continue;
        }
        if (line->kind == LINE_BLANK || line->kind == LINE_COMMENT)
        {
            continue;
        }
        if (line->kind != LINE_INSTR)
        {
            free (body);
            return (0);
        }
        if (op_is (line, "rts") || i == info->return_line)
        {
            continue;
        }
        if (first)
        {
            first = 0;
            if (skip_initial_load)
            {
                continue;
            }
        }

        if (pop_pos < info->npops && i == info->pops[pop_pos])
        {
            int argpos = first_arg + info->npops - 1 - pop_pos;
            Line *push;
            int src;
            int dest;
            const char *move_op;
            char text[OPT_ARG_MAX * 2];

            if (argpos < first_arg)
            {
                free (body);
                return (0);
            }
            push = &lines[push_lines[argpos]];
            src = parse_register_token (push->args[0]);
            if (src < 0)
            {
                free (body);
                return (0);
            }

            if (op_is (line, "stpopi"))
            {
                dest = map_i[parse_register_token (line->args[0])];
                move_op = "movi";
            }
            else if (op_is (line, "stpopd"))
            {
                dest = map_d[parse_register_token (line->args[0])];
                move_op = "movd";
            }
            else
            {
                free (body);
                return (0);
            }

            if (dest < 0)
            {
                free (body);
                return (0);
            }

            snprintf (text, sizeof (text), "%s %d, %d", move_op, src, dest);
            append_extra (&body, text);
            pop_pos++;
            continue;
        }

        if (is_stpush_op (line->op) || is_stpop_op (line->op))
        {
            free (body);
            return (0);
        }

        {
            char text[OPT_LINE_MAX];

            if (! inline_format_line (line, map_i, map_d,
                                      text, sizeof (text)))
            {
                free (body);
                return (0);
            }
            append_extra (&body, text);
        }
    }

    if (pop_pos != info->npops)
    {
        free (body);
        return (0);
    }

    if (info->return_line >= 0)
    {
        char text[OPT_ARG_MAX * 2];
        int src;

        if (info->return_type == 'F')
        {
            src = map_d[info->return_reg];
        }
        else
        {
            src = map_i[info->return_reg];
        }
        if (src < 0 || result_dest < 0)
        {
            free (body);
            return (0);
        }

        snprintf (text, sizeof (text), "%s %d, %d",
                  (info->return_type == 'F') ? "movd" : "movi",
                  src, result_dest);
        append_extra (&body, text);
    }

    *body_out = body;
    return (1);
}

static int inline_call_site (int jsr)
{
    Line *call = &lines[jsr];
    InlineFunc info;
    int target;
    int target_ord;
    int caller_start;
    int caller_end;
    int push_lines[INLINE_MAX_PUSHES];
    char push_types[INLINE_MAX_PUSHES];
    int npushes;
    int first_arg;
    int nargs;
    int result_line = -1;
    int result_dest = -1;
    int save_count;
    int restore_lines[INLINE_MAX_PUSHES];
    int remove_saves = 0;
    int map_i[MAXREG];
    int map_d[MAXREG];
    char *body = NULL;
    int i;

    if (! call->active || call->kind != LINE_INSTR ||
        call->pre != NULL || call->replace != NULL || call->post != NULL ||
        ! op_is (call, "jsr") || call->argc < 1)
    {
        return (0);
    }

    target = find_label (call->args[0]);
    if (target < 0 || strcmp (lines[target].op, ":main") == 0)
    {
        return (0);
    }

    target_ord = function_ordinal (target);
    if (target_ord < 0 || func_skip == NULL || func_skip[target_ord] ||
        inline_count_calls (target) > inline_max_calls)
    {
        return (0);
    }

    if (! inline_analyze_function (target, &info) || info.npops > INLINE_MAX_ARGS)
    {
        return (0);
    }

    inline_caller_bounds (jsr, &caller_start, &caller_end);
    if (! inline_collect_pushes (jsr, caller_start, push_lines, push_types,
                                 &npushes))
    {
        return (0);
    }

    nargs = info.npops;
    if (npushes < nargs)
    {
        return (0);
    }
    first_arg = npushes - nargs;

    if (! inline_args_match (&info, push_types, first_arg) ||
        ! inline_arg_regs_stable (jsr, push_lines, push_types,
                                  first_arg, nargs))
    {
        return (0);
    }

    for (i = first_arg; i < npushes; i++)
    {
        if (! inline_line_removable (push_lines[i]))
        {
            return (0);
        }
    }

    if (info.return_type != 0)
    {
        int after = inline_next_code (jsr, caller_end);

        if (after < 0 || ! inline_line_removable (after))
        {
            return (0);
        }
        if (info.return_type == 'Q')
        {
            if (! op_is (&lines[after], "stpopi"))
            {
                return (0);
            }
        }
        else if (! op_is (&lines[after], "stpopd"))
        {
            return (0);
        }

        result_line = after;
        result_dest = parse_register_token (lines[after].args[0]);
        if (result_dest < 0)
        {
            return (0);
        }
    }

    /*
     * Saving the live caller registers is optional.  If the complete block
     * can be matched, both the pushes and the restores are removed.  If not,
     * they are left alone and remain balanced.
     */
    save_count = first_arg;
    if (save_count > 0)
    {
        int all_removable = 1;
        int scan_start;

        for (i = 0; i < save_count; i++)
        {
            if (! inline_line_removable (push_lines[i]))
            {
                all_removable = 0;
                break;
            }
        }

        if (all_removable)
        {
            scan_start = (result_line >= 0) ? result_line + 1 : jsr + 1;
            if (inline_find_restore (scan_start, caller_end,
                                     push_lines, push_types, save_count,
                                     restore_lines))
            {
                int last_restore = restore_lines[0];
                int k;

                for (k = 1; k < save_count; k++)
                {
                    if (restore_lines[k] > last_restore)
                    {
                        last_restore = restore_lines[k];
                    }
                }

                if (inline_saved_regs_untouched (push_lines[0], last_restore,
                                                 jsr, result_line,
                                                 push_lines, push_types,
                                                 save_count, restore_lines))
                {
                    remove_saves = 1;
                }
            }
        }

        /*
         * If the result is written into a saved register, the original
         * restore would overwrite it later.  Keep that save/restore pair.
         */
        if (remove_saves && result_line >= 0)
        {
            for (i = 0; i < save_count; i++)
            {
                int same_bank =
                    (push_types[i] == 'I' && info.return_type == 'Q') ||
                    (push_types[i] == 'D' && info.return_type == 'F');

                if (same_bank &&
                    parse_register_token (lines[push_lines[i]].args[0]) ==
                    result_dest)
                {
                    remove_saves = 0;
                    break;
                }
            }
        }
    }

    if (! inline_map_registers (&info, map_i, map_d))
    {
        return (0);
    }

    if (! inline_build_body (&info, map_i, map_d, push_lines, first_arg,
                             result_dest, &body))
    {
        return (0);
    }

    if (body == NULL)
    {
        body = xstrdup ("");
    }

    call->replace = body;

    for (i = first_arg; i < npushes; i++)
    {
        lines[push_lines[i]].active = 0;
    }

    if (result_line >= 0)
    {
        lines[result_line].active = 0;
    }

    if (remove_saves)
    {
        for (i = 0; i < save_count; i++)
        {
            lines[push_lines[i]].active = 0;
            lines[restore_lines[i]].active = 0;
        }
    }

    return (1);
}

static void inline_functions (void)
{
    int i;

    if (materialize_transformations ())
    {
        build_function_starts ();
        build_call_graph ();
        compute_recursive_functions ();
    }

    if (nfunc_starts == 0 || func_skip == NULL)
    {
        return;
    }

    recompute_used_regs ();

    for (i = 0; i < nlines; i++)
    {
        if (lines[i].active && lines[i].kind == LINE_INSTR &&
            op_is (&lines[i], "jsr"))
        {
            inline_call_site (i);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* -O6: loop unrolling                                                       */
/* ------------------------------------------------------------------------- */

/*
 * The unroll pass runs after -O5, which materialised every pre/replace/post
 * transformation into plain instruction lines.  So the lines it copies are
 * guaranteed to have no attached text.
 */
static int unroll_line_clean (int index)
{
    Line *line = &lines[index];

    return (line->active && line->kind == LINE_INSTR);
}

/*
 * The loop body must be pure straight line for unrolling to be safe: every
 * line a known instruction and no control flow, calls or interrupts.
 */
static int unroll_body_straight (int start, int end)
{
    int i;

    for (i = start; i < end; i++)
    {
        Line *line = &lines[i];

        if (! unroll_line_clean (i))
        {
            return (0);
        }
        if (opcode_index (line->op) < 0)
        {
            return (0);
        }
        if (is_jump_op (line->op) || op_is (line, "intr0") ||
            op_is (line, "intr1") || op_is (line, "inclsijmpi") ||
            op_is (line, "decgrijmpi"))
        {
            return (0);
        }
    }
    return (1);
}

/*
 * Count the distinct integer registers used by the body (excluding r0 which
 * is reserved for data addresses) and return them so the unroller can allocate
 * the factor-1 extra copies.  r0 is excluded because the superop keeps the
 * loop address register.
 */
static int unroll_distinct_regs (int start, int end, int *use_i)
{
    int i;
    int arg;

    memset (use_i, 0, sizeof (int) * MAXREG);

    for (i = start; i < end; i++)
    {
        Line *line = &lines[i];

        for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
        {
            if (instr_arg_is_ireg (line, arg) &&
                ! instr_arg_is_label (line, arg) &&
                ! instr_arg_is_data (line, arg))
            {
                int reg = parse_register_token (line->args[arg]);

                if (reg >= 0 && reg != 0)
                {
                    use_i[reg] = 1;
                }
            }
        }
    }
    return (1);
}

static void unroll_map_regs (const int *use_i, int *map_i)
{
    int i;

    memset (map_i, -1, sizeof (int) * MAXREG);

    for (i = 0; i < MAXREG; i++)
    {
        if (use_i[i])
        {
            map_i[i] = promo_alloc_reg ();

            if (map_i[i] < 0)
            {
                memset (map_i, -1, sizeof (int) * MAXREG);
                return;
            }
        }
    }
}

static void unroll_map_dregs (int start, int end, int *map_d)
{
    int i;
    int use_d[MAXREG];
    int arg;

    memset (use_d, 0, sizeof (int) * MAXREG);
    memset (map_d, -1, sizeof (int) * MAXREG);

    for (i = start; i < end; i++)
    {
        Line *line = &lines[i];

        for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
        {
            if (instr_arg_is_dreg (line, arg) &&
                ! instr_arg_is_label (line, arg) &&
                ! instr_arg_is_data (line, arg))
            {
                int reg = parse_register_token (line->args[arg]);

                if (reg >= 0 && reg != 0 && ! use_d[reg])
                {
                    use_d[reg] = 1;
                    if (map_d[reg] < 0)
                    {
                        map_d[reg] = promo_alloc_reg ();
                        if (map_d[reg] < 0)
                        {
                            memset (map_d, -1, sizeof (int) * MAXREG);
                            i = start;
                            return;
                        }
                    }
                }
            }
        }
    }
}

static int unroll_format_body (int start, int end, int factor,
                               const int *map_i, const int *map_d,
                               char *buf, size_t bufsize)
{
    int pos;
    int i;
    int f;
    int arg;
    int is_i;
    int is_d;
    int reg;
    int newreg;

    pos = 0;

    for (f = 0; f < factor; f++)
    {
        for (i = start; i < end; i++)
        {
            Line *line = &lines[i];

            if (! unroll_line_clean (i))
            {
                return (0);
            }

            pos += snprintf (buf + pos, bufsize - (size_t) pos, "%s", line->op);
            if (pos < 0 || (size_t) pos >= bufsize)
            {
                return (0);
            }

            for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
            {
                is_i = instr_arg_is_ireg (line, arg);
                is_d = instr_arg_is_dreg (line, arg);
                reg = parse_register_token (line->args[arg]);

                if (instr_arg_is_label (line, arg))
                {
                    pos += snprintf (buf + pos, bufsize - (size_t) pos, " %s",
                                     line->args[arg]);
                }
                else if (is_i)
                {
                    newreg = map_i[reg];
                    if (newreg < 0)
                    {
                        return (0);
                    }
                    pos += snprintf (buf + pos, bufsize - (size_t) pos,
                                     ", %d", newreg);
                }
                else if (is_d)
                {
                    newreg = map_d[reg];
                    if (newreg < 0)
                    {
                        return (0);
                    }
                    pos += snprintf (buf + pos, bufsize - (size_t) pos,
                                     ", %d", newreg);
                }
                else if (instr_arg_is_register (line, arg))
                {
                    return (0);
                }
                else
                {
                    pos += snprintf (buf + pos, bufsize - (size_t) pos, " %s",
                                     line->args[arg]);
                }

                if (pos < 0 || (size_t) pos >= bufsize)
                {
                    return (0);
                }
            }
            pos += snprintf (buf + pos, bufsize - (size_t) pos, "\n");
            if (pos < 0 || (size_t) pos >= bufsize)
            {
                return (0);
            }
        }
    }
    return (1);
}

/*
 * Find the "var" line (whose replace text contains the inclsijmpi/decgrijmpi
 * superop) and the ":end" label of the rotated loop rooted at label_index.
 *
 * The rotated loop produced by try_rotated_loop() has this layout:
 *
 *   :label        (label_index)
 *     <body instructions>
 *     <var line>  replace = "loada lim, 0, lim_reg\ninclsijmpi counter, lim_reg, :label\n<cond>\n[spill]\nload var, 0, s2\npullqw counter, s2, 0\njmp :end"
 *     <back-edge jmp :label  (inactive)>
 *   :end
 *
 * Return 0 on success with *bottom_i and *end_i set; -1 otherwise.
 */
static int locate_unroll_loop (int label_index, int *bottom_i, int *end_i)
{
    int i;
    int in_code = 1;

    for (i = label_index + 1; i < nlines; i++)
    {
        Line *line = &lines[i];

        if (! line->active)
        {
            continue;
        }
        if (line->kind == LINE_DIRECTIVE)
        {
            if (strcmp (line->op, ".code") == 0)
            {
                in_code = 1;
            }
            else if (strcmp (line->op, ".cend") == 0)
            {
                in_code = 0;
            }
            continue;
        }
        if (! in_code)
        {
            continue;
        }
        if (line->kind == LINE_LABEL)
        {
            /* hitting another label before the superop means this label
               is not the root of a superop loop */
            return (-1);
        }
        if (line->kind == LINE_INSTR && line->replace != NULL)
        {
            if (strstr (line->replace, "inclsijmpi") != NULL ||
                strstr (line->replace, "decgrijmpi") != NULL)
            {
                /* the superop's jump target must be the label at label_index
                   (the body label).  The superop text is
                   "inclsijmpi <counter>, <lim_reg>, :label" -- the label
                   is the last arg on the line.  We confirm it matches. */
                {
                    char *p = line->replace;
                    char *superop;
                    char *superop_line_start;
                    char *superop_line_end;
                    char *arg3;

                    superop = strstr (p, "inclsijmpi");
                    if (superop == NULL)
                    {
                        superop = strstr (p, "decgrijmpi");
                    }
                    if (superop == NULL)
                    {
                        continue;
                    }

                    /* walk back to the start of the superop's line */
                    superop_line_start = superop;
                    while (superop_line_start > p && *superop_line_start != '\n')
                    {
                        superop_line_start--;
                    }

                    /* walk forward to the end of the superop's line */
                    superop_line_end = superop;
                    while (*superop_line_end != '\n' && *superop_line_end != '\0')
                    {
                        superop_line_end++;
                    }

                    /* arg3 is everything after the second comma on this line,
                       trimmed. */
                    arg3 = superop;
                    while (*arg3 && *arg3 != ',')
                    {
                        arg3++;
                    }
                    while (*arg3 == ' ' || *arg3 == '\t')
                    {
                        arg3++;
                    }
                    while (*arg3 && *arg3 != ',')
                    {
                        arg3++;
                    }
                    while (*arg3 == ' ' || *arg3 == '\t')
                    {
                        arg3++;
                    }

                    {
                        char label_buf[128];
                        int n = 0;

                        while (*arg3 && *arg3 != '\n' && *arg3 != '\0' && n < 127)
                        {
                            if (*arg3 == ' ' || *arg3 == '\t')
                            {
                                break;
                            }
                            label_buf[n++] = *arg3;
                            arg3++;
                        }
                        label_buf[n] = '\0';

                        if (strcmp (label_buf, lines[label_index].op) == 0)
                        {
                            *bottom_i = i;
                            /* find the :end label after this line */
                            for (i = i + 1; i < nlines; i++)
                            {
                                if (! lines[i].active)
                                {
                                    continue;
                                }
                                if (lines[i].kind == LINE_LABEL)
                                {
                                    *end_i = i;
                                    return (0);
                                }
                                if (lines[i].kind == LINE_DIRECTIVE &&
                                    strcmp (lines[i].op, ".cend") == 0)
                                {
                                    return (-1);
                                }
                            }
                            return (-1);
                        }
                    }
                }
            }
        }
    }
    return (-1);
}

/*
 * Unroll a single rotated loop rooted at label_index.
 *
 * The body (between the body label and the superop line) is duplicated
 * factor times and emitted before the original bottom text in
 * bottom_i->replace.  The original body lines are then marked inactive.
 *
 * The counter register (first arg of the superop) is NOT remapped in the
 * body copies: the superop writes to it once per iteration and the body
 * reads it (factor times per iteration), always seeing the same value
 * until the next superop updates it.  This is correct for loop counters.
 *
 * Safety: the body must not write to the counter register (otherwise the
 * write would be applied factor times).
 *
 * Return 1 on success, 0 otherwise.
 */
static int try_unroll_loop (int label_index)
{
    int bottom_i;
    int end_i;
    int body_start;
    int body_end;
    int j;
    int first_instr;
    int last_instr;
    int counter_reg;
    int use_i[MAXREG];
    int map_i[MAXREG];
    int map_d[MAXREG];
    char buf[262144];
    int ret;

    if ((ret = locate_unroll_loop (label_index, &bottom_i, &end_i)) != 0)
    {
        return (0);
    }
    (void) end_i;

    {
        Line *bottom = &lines[bottom_i];
        char *p;
        char *superop;
        int n;
        char counter_tok[32];
        char lim_tok[32];
        char label_tok[128];
        int tpos;
        int tpos2;
        int n2;
        char *start2;
        char *token_end;
        char *token_end2;
        int n3;
        char *start3;

        if (bottom->replace == NULL)
        {
            return (0);
        }
        p = bottom->replace;

        superop = strstr (p, "inclsijmpi");
        if (superop == NULL)
        {
            superop = strstr (p, "decgrijmpi");
        }
        if (superop == NULL)
        {
            return (0);
        }

        n = (strncmp (superop, "inclsijmpi", 12) == 0) ? 12 : 12;
        p = superop + n;

        token_end = p;
        while (*token_end && *token_end != ',' && *token_end != '\n')
        {
            token_end++;
        }
        n = (int) (token_end - p);
        tpos = 0;
        while (p[tpos] == ' ' || p[tpos] == '\t')
        {
            tpos++;
        }
        n -= tpos;
        if (n < 0 || n >= (int) sizeof (counter_tok))
        {
            return (0);
        }
        memcpy (counter_tok, p + tpos, n);
        counter_tok[n] = '\0';

        token_end2 = token_end;
        while (*token_end2 == ',' || *token_end2 == ' ' || *token_end2 == '\t')
        {
            token_end2++;
        }
        start2 = token_end2;
        token_end2 = start2;
        while (*token_end2 && *token_end2 != ',' && *token_end2 != '\n')
        {
            token_end2++;
        }
        n2 = (int) (token_end2 - start2);
        tpos2 = 0;
        while (start2[tpos2] == ' ' || start2[tpos2] == '\t')
        {
            tpos2++;
        }
        n2 -= tpos2;
        if (n2 < 0 || n2 >= (int) sizeof (lim_tok))
        {
            return (0);
        }
        memcpy (lim_tok, start2 + tpos2, n2);
        lim_tok[n2] = '\0';

        token_end2 += n2 + tpos2;
        while (*token_end2 == ',' || *token_end2 == ' ' || *token_end2 == '\t')
        {
            token_end2++;
        }
        start3 = token_end2;
        n3 = 0;
        while (*start3 && *start3 != '\n' && *start3 != ',' && n3 < 127)
        {
            if (*start3 == ' ' || *start3 == '\t')
            {
                break;
            }
            label_tok[n3++] = *start3;
            start3++;
        }
        label_tok[n3] = '\0';

        if (strcmp (label_tok, lines[label_index].op) != 0)
        {
            return (0);
        }

        counter_reg = parse_register_token (counter_tok);
        if (counter_reg < 0)
        {
            return (0);
        }

        (void) lim_tok;
    }

    body_start = label_index + 1;
    body_end = bottom_i;

    first_instr = -1;
    last_instr = -1;
    for (j = body_start; j < body_end; j++)
    {
        if (lines[j].active && lines[j].kind == LINE_INSTR)
        {
            if (first_instr < 0)
            {
                first_instr = j;
            }
            last_instr = j;
        }
    }
    if (first_instr < 0)
    {
        return (0);
    }
    if (! unroll_body_straight (first_instr, last_instr + 1))
    {
        return (0);
    }
    body_start = first_instr;
    body_end = last_instr + 1;

    for (j = body_start; j < body_end; j++)
    {
        if (instr_writes_ireg (&lines[j], counter_reg))
        {
            return (0);
        }
    }

    unroll_distinct_regs (body_start, body_end, use_i);

    unroll_map_dregs (body_start, body_end, map_d);
    unroll_map_regs (use_i, map_i);

    if (map_i[counter_reg] >= 0)
    {
        map_i[counter_reg] = -1;
    }

    if (! unroll_format_body (body_start, body_end, unroll_max_factor,
                               map_i, map_d, buf, sizeof (buf)))
    {
        return (0);
    }

    {
        Line *bottom = &lines[bottom_i];
        size_t body_len = strlen (buf);
        size_t bottom_len = strlen (bottom->replace);
        char *new_replace;
        int i2;

        new_replace = (char *) xmalloc (body_len + bottom_len + 2);
        if (new_replace == NULL)
        {
            return (0);
        }
        memcpy (new_replace, buf, body_len);
        memcpy (new_replace + body_len, bottom->replace, bottom_len + 1);
        new_replace[body_len + bottom_len + 1] = '\0';
        free (bottom->replace);
        bottom->replace = new_replace;
        (void) i2;
    }

    for (j = body_start; j < body_end; j++)
    {
        lines[j].active = 0;
    }

    recompute_used_regs ();

    return (1);
}

/*
 * -O6: main entry point.  Walks all labels in the code section and tries
 * to unroll each rotated loop (body labels referenced exactly once by the
 * superop).
 */
static void optimize_loop_unroll (void)
{
    int in_code = 0;
    int i;

    for (i = 0; i < nlines; i++)
    {
        Line *line = &lines[i];

        if (! line->active)
        {
            continue;
        }

        if (line->kind == LINE_DIRECTIVE)
        {
            if (strcmp (line->op, ".code") == 0)
            {
                in_code = 1;
            }
            else if (strcmp (line->op, ".cend") == 0)
            {
                in_code = 0;
            }
            continue;
        }

        if (! in_code || line->kind != LINE_LABEL)
        {
            continue;
        }

        /* a body label is referenced exactly once (by the superop's
           implicit jump).  Only such labels root a unrollable loop. */
        if (count_label_refs (line->op) == 1)
        {
            try_unroll_loop (i);
        }
    }
}

/* ------------------------------------------------------------------------- */
/* -O7: aggressive constant folding, copy propagation and dead code removal  */
/* ------------------------------------------------------------------------- */

#define AGGR_REGSET_WORDS ((MAXREG + 63) / 64)

typedef struct
{
    uint64_t word[AGGR_REGSET_WORDS];
} AggrRegSet;

static void aggr_set_clear (AggrRegSet *set)
{
    memset (set, 0, sizeof (*set));
}

static void aggr_set_add (AggrRegSet *set, int reg)
{
    if (reg >= 0 && reg < MAXREG)
    {
        set->word[reg >> 6] |= ((uint64_t) 1) << (reg & 63);
    }
}

static int aggr_set_has (const AggrRegSet *set, int reg)
{
    if (reg < 0 || reg >= MAXREG)
    {
        return (0);
    }
    return ((set->word[reg >> 6] & (((uint64_t) 1) << (reg & 63))) != 0);
}

static void aggr_set_or (AggrRegSet *dest, const AggrRegSet *src)
{
    int i;

    for (i = 0; i < AGGR_REGSET_WORDS; i++)
    {
        dest->word[i] |= src->word[i];
    }
}

static int aggr_set_equal (const AggrRegSet *a, const AggrRegSet *b)
{
    return (memcmp (a->word, b->word, sizeof (a->word)) == 0);
}

static void aggr_set_all (AggrRegSet *set)
{
    int i;

    aggr_set_clear (set);
    for (i = 0; i < MAXREG; i++)
    {
        aggr_set_add (set, i);
    }
}

static int aggr_opaque (const Line *line)
{
    const char *op = line->op;

    if (opcode_index (op) < 0)
    {
        return (1);
    }
    if (op_is (line, "intr0") || op_is (line, "intr1") ||
        op_is (line, "jsr") || op_is (line, "jsra") ||
        op_is (line, "jmpa"))
    {
        return (1);
    }
    return (0);
}

static void aggr_rewrite_line (Line *line)
{
    char buf[OPT_LINE_MAX];
    size_t pos;
    int arg;

    pos = (size_t) snprintf (buf, sizeof (buf), "%s", line->op);
    for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
    {
        int n = snprintf (buf + pos, sizeof (buf) - pos,
                          (arg == 0) ? " %s" : ", %s", line->args[arg]);

        if (n < 0 || pos + (size_t) n >= sizeof (buf))
        {
            die ("aggressive rewrite line too long");
        }
        pos += (size_t) n;
    }

    free (line->replace);
    line->replace = xstrdup (buf);
}

/*
 * Replace an instruction with "movi <src>, <dest>".  "dest" is copied first
 * because both arguments live in the same Line structure.
 */
static void aggr_rewrite_move (Line *line, const char *src)
{
    char dest[OPT_ARG_MAX];
    char source[OPT_ARG_MAX];

    strcpy (dest, line->args[2]);
    strcpy (source, src);

    strcpy (line->op, "movi");
    line->argc = 2;
    strcpy (line->args[0], source);
    strcpy (line->args[1], dest);

    aggr_rewrite_line (line);
}

static int aggr_find_constant_reg (const unsigned char *known,
                                   const long long *value, long long wanted)
{
    int reg;

    for (reg = 0; reg < MAXREG; reg++)
    {
        if (known[reg] && value[reg] == wanted)
        {
            return (reg);
        }
    }
    return (-1);
}

static int aggr_fold_constants (const char *op, long long a, long long b,
                                long long *result)
{
    if (strcmp (op, "addi") == 0)
    {
        *result = (long long) ((unsigned long long) a +
                               (unsigned long long) b);
        return (1);
    }
    if (strcmp (op, "subi") == 0)
    {
        *result = (long long) ((unsigned long long) a -
                               (unsigned long long) b);
        return (1);
    }
    if (strcmp (op, "muli") == 0)
    {
        *result = (long long) ((unsigned long long) a *
                               (unsigned long long) b);
        return (1);
    }
    if (strcmp (op, "divi") == 0 || strcmp (op, "modi") == 0)
    {
        if (b == 0 || (a == LLONG_MIN && b == -1))
        {
            return (0);
        }
        *result = (strcmp (op, "divi") == 0) ? (a / b) : (a % b);
        return (1);
    }
    if (strcmp (op, "andi") == 0)
    {
        *result = (a != 0 && b != 0) ? 1 : 0;
        return (1);
    }
    if (strcmp (op, "ori") == 0)
    {
        *result = (a != 0 || b != 0) ? 1 : 0;
        return (1);
    }
    if (strcmp (op, "bandi") == 0)
    {
        *result = a & b;
        return (1);
    }
    if (strcmp (op, "bori") == 0)
    {
        *result = a | b;
        return (1);
    }
    if (strcmp (op, "bxori") == 0)
    {
        *result = a ^ b;
        return (1);
    }
    if (strcmp (op, "smuli") == 0)
    {
        if (b < 0 || b >= 64)
        {
            return (0);
        }
        *result = (long long) ((unsigned long long) a << (unsigned int) b);
        return (1);
    }
    if (strcmp (op, "eqi") == 0)
    {
        *result = (a == b) ? 1 : 0;
        return (1);
    }
    if (strcmp (op, "neqi") == 0)
    {
        *result = (a != b) ? 1 : 0;
        return (1);
    }
    if (strcmp (op, "gri") == 0)
    {
        *result = (a > b) ? 1 : 0;
        return (1);
    }
    if (strcmp (op, "lsi") == 0)
    {
        *result = (a < b) ? 1 : 0;
        return (1);
    }
    if (strcmp (op, "greqi") == 0)
    {
        *result = (a >= b) ? 1 : 0;
        return (1);
    }
    if (strcmp (op, "lseqi") == 0)
    {
        *result = (a <= b) ? 1 : 0;
        return (1);
    }
    return (0);
}

static int aggr_symbol_index (const char *name)
{
    int low = 0;
    int high = nsymbols - 1;

    while (low <= high)
    {
        int mid = low + (high - low) / 2;
        int cmp = strcmp (name, symbols[mid].name);

        if (cmp == 0)
        {
            return (mid);
        }
        if (cmp < 0)
        {
            high = mid - 1;
        }
        else
        {
            low = mid + 1;
        }
    }
    return (-1);
}

static int aggressive_constant_fold (void)
{
    unsigned char known[MAXREG];
    long long value[MAXREG];
    int *readonly = NULL;
    int in_code = 0;
    int changed = 0;
    int i;

    if (nsymbols > 0)
    {
        readonly = (int *) xmalloc ((size_t) nsymbols * sizeof (int));
        for (i = 0; i < nsymbols; i++)
        {
            readonly[i] = symbol_is_readonly (i);
        }
    }

    memset (known, 0, sizeof (known));
    memset (value, 0, sizeof (value));

    for (i = 0; i < nlines; i++)
    {
        Line *line = &lines[i];
        const char *op;
        int a_arg;
        int b_arg;
        int d_arg;
        int d_reg;
        int a_known;
        int b_known;
        long long a_value = 0;
        long long b_value = 0;

        if (! line->active)
        {
            continue;
        }

        if (line->kind == LINE_DIRECTIVE)
        {
            if (strcmp (line->op, ".code") == 0)
            {
                in_code = 1;
            }
            else if (strcmp (line->op, ".cend") == 0)
            {
                in_code = 0;
            }
            memset (known, 0, sizeof (known));
            continue;
        }

        if (! in_code)
        {
            continue;
        }

        if (line->kind == LINE_BLANK || line->kind == LINE_COMMENT)
        {
            continue;
        }

        if (line->kind == LINE_LABEL)
        {
            memset (known, 0, sizeof (known));
            continue;
        }

        if (line->kind != LINE_INSTR || line->pre != NULL ||
            line->post != NULL)
        {
            memset (known, 0, sizeof (known));
            continue;
        }

        /*
         * The print and delay interrupts are known not to write integer
         * registers, so constants can flow across them.
         */
        if (op_is (line, "intr0") && intr0_readonly (line))
        {
            continue;
        }

        if (aggr_opaque (line))
        {
            memset (known, 0, sizeof (known));
            continue;
        }

        op = line->op;

        if (strcmp (op, "movi") == 0 && line->argc >= 2)
        {
            int src = parse_register_token (line->args[0]);
            int dst = parse_register_token (line->args[1]);

            if (dst >= 0)
            {
                if (src >= 0 && known[src])
                {
                    known[dst] = 1;
                    value[dst] = value[src];
                }
                else
                {
                    known[dst] = 0;
                }
            }
            continue;
        }

        if (strcmp (op, "loada") == 0 && line->argc >= 3)
        {
            long long offset;
            long long constant;
            int dst = parse_register_token (line->args[2]);
            int sym = aggr_symbol_index (line->args[0]);

            if (dst >= 0)
            {
                if (parse_number_token (line->args[1], &offset) && offset == 0 &&
                    sym >= 0 && ! symbols[sym].ambiguous &&
                    readonly != NULL && readonly[sym] &&
                    data_const_value (line->args[0], &constant))
                {
                    known[dst] = 1;
                    value[dst] = constant;
                }
                else
                {
                    known[dst] = 0;
                }
            }
            continue;
        }

        if (strcmp (op, "loadd") == 0 || strcmp (op, "load") == 0 ||
            strcmp (op, "loadl") == 0)
        {
            int arg;

            for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
            {
                if (instr_arg_is_ireg (line, arg) &&
                    instr_arg_writes (line, arg))
                {
                    int reg = parse_register_token (line->args[arg]);

                    if (reg >= 0)
                    {
                        known[reg] = 0;
                    }
                }
            }
            continue;
        }

        if (strcmp (op, "jmpi") == 0 && line->argc >= 2)
        {
            int cond = parse_register_token (line->args[0]);

            if (cond >= 0 && known[cond])
            {
                if (value[cond] == 0)
                {
                    line->active = 0;
                }
                else
                {
                    char label[OPT_ARG_MAX];

                    strcpy (label, line->args[1]);
                    strcpy (line->op, "jmp");
                    line->argc = 1;
                    strcpy (line->args[0], label);
                    aggr_rewrite_line (line);
                }
                changed = 1;
            }
            memset (known, 0, sizeof (known));
            continue;
        }

        if (strcmp (op, "jmp") == 0 || strcmp (op, "rts") == 0 ||
            strcmp (op, "inclsijmpi") == 0 ||
            strcmp (op, "decgrijmpi") == 0)
        {
            memset (known, 0, sizeof (known));
            continue;
        }

        if (strcmp (op, "noti") == 0 && line->argc >= 2)
        {
            a_arg = 0;
            b_arg = -1;
            d_arg = 1;
        }
        else if (line->argc >= 3)
        {
            a_arg = 0;
            b_arg = 1;
            d_arg = 2;
        }
        else
        {
            int arg;

            for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
            {
                if (instr_arg_is_ireg (line, arg) &&
                    instr_arg_writes (line, arg))
                {
                    int reg = parse_register_token (line->args[arg]);

                    if (reg >= 0)
                    {
                        known[reg] = 0;
                    }
                }
            }
            continue;
        }

        d_reg = parse_register_token (line->args[d_arg]);
        if (d_reg < 0 || ! instr_arg_is_ireg (line, d_arg) ||
            ! instr_arg_writes (line, d_arg))
        {
            int arg;

            for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
            {
                if (instr_arg_is_ireg (line, arg) &&
                    instr_arg_writes (line, arg))
                {
                    int reg = parse_register_token (line->args[arg]);

                    if (reg >= 0)
                    {
                        known[reg] = 0;
                    }
                }
            }
            continue;
        }

        {
            int ar = parse_register_token (line->args[a_arg]);
            int br = (b_arg >= 0) ? parse_register_token (line->args[b_arg]) : -1;

            a_known = (ar >= 0 && known[ar]);
            b_known = (b_arg < 0) ? 1 : (br >= 0 && known[br]);
            if (a_known)
            {
                a_value = value[ar];
            }
            if (b_arg >= 0 && b_known)
            {
                b_value = value[br];
            }
        }

        /* fold a fully constant operation into a constant register */
        if (a_known && b_known)
        {
            long long result;

            if (strcmp (op, "noti") == 0)
            {
                result = (a_value == 0) ? 1 : 0;
            }
            else if (! aggr_fold_constants (op, a_value, b_value, &result))
            {
                result = 0;
                goto identity_or_unknown;
            }

            {
                int src = aggr_find_constant_reg (known, value, result);

                if (src >= 0)
                {
                    char dest[OPT_ARG_MAX];

                    strcpy (dest, line->args[d_arg]);
                    strcpy (line->op, "movi");
                    line->argc = 2;
                    snprintf (line->args[0], OPT_ARG_MAX, "%d", src);
                    strcpy (line->args[1], dest);
                    aggr_rewrite_line (line);
                    known[d_reg] = 1;
                    value[d_reg] = result;
                    changed = 1;
                    continue;
                }
            }
        }

identity_or_unknown:
        /* algebraic identities which avoid a full constant materialisation */
        {
            const char *copy_src = NULL;
            char one_buf[16];

            if (strcmp (op, "addi") == 0)
            {
                if (b_known && b_value == 0)
                {
                    copy_src = line->args[0];
                }
                else if (a_known && a_value == 0)
                {
                    copy_src = line->args[1];
                }
            }
            else if (strcmp (op, "subi") == 0)
            {
                if (b_known && b_value == 0)
                {
                    copy_src = line->args[0];
                }
            }
            else if (strcmp (op, "muli") == 0)
            {
                if (a_known && a_value == 0)
                {
                    copy_src = line->args[0];
                }
                else if (b_known && b_value == 0)
                {
                    copy_src = line->args[1];
                }
                else if (b_known && b_value == 1)
                {
                    copy_src = line->args[0];
                }
                else if (a_known && a_value == 1)
                {
                    copy_src = line->args[1];
                }
            }
            else if (strcmp (op, "divi") == 0)
            {
                if (b_known && b_value == 1)
                {
                    copy_src = line->args[0];
                }
                else if (a_known && a_value == 0 && b_known && b_value != 0)
                {
                    copy_src = line->args[0];
                }
            }
            else if (strcmp (op, "modi") == 0)
            {
                if (b_known && b_value == 1)
                {
                    int zero = aggr_find_constant_reg (known, value, 0);

                    if (zero >= 0)
                    {
                        snprintf (one_buf, sizeof (one_buf), "%d", zero);
                        copy_src = one_buf;
                    }
                }
                else if (a_known && a_value == 0 && b_known && b_value != 0)
                {
                    copy_src = line->args[0];
                }
            }
            else if (strcmp (op, "andi") == 0 || strcmp (op, "bandi") == 0)
            {
                if (a_known && a_value == 0)
                {
                    copy_src = line->args[0];
                }
                else if (b_known && b_value == 0)
                {
                    copy_src = line->args[1];
                }
                else if (strcmp (op, "bandi") == 0 &&
                         b_known && b_value == -1)
                {
                    copy_src = line->args[0];
                }
                else if (strcmp (op, "bandi") == 0 &&
                         a_known && a_value == -1)
                {
                    copy_src = line->args[1];
                }
            }
            else if (strcmp (op, "ori") == 0)
            {
                if (a_known && a_value != 0)
                {
                    int one = aggr_find_constant_reg (known, value, 1);

                    if (one >= 0)
                    {
                        snprintf (one_buf, sizeof (one_buf), "%d", one);
                        copy_src = one_buf;
                    }
                }
                else if (b_known && b_value != 0)
                {
                    int one = aggr_find_constant_reg (known, value, 1);

                    if (one >= 0)
                    {
                        snprintf (one_buf, sizeof (one_buf), "%d", one);
                        copy_src = one_buf;
                    }
                }
            }
            else if (strcmp (op, "bori") == 0 || strcmp (op, "bxori") == 0)
            {
                if (a_known && a_value == 0)
                {
                    copy_src = line->args[1];
                }
                else if (b_known && b_value == 0)
                {
                    copy_src = line->args[0];
                }
                else if (strcmp (op, "bori") == 0 &&
                         b_known && b_value == -1)
                {
                    copy_src = line->args[1];
                }
                else if (strcmp (op, "bori") == 0 &&
                         a_known && a_value == -1)
                {
                    copy_src = line->args[0];
                }
            }
            else if (strcmp (op, "smuli") == 0)
            {
                if (b_known && b_value == 0)
                {
                    copy_src = line->args[0];
                }
            }

            if (copy_src != NULL)
            {
                int src_reg = parse_register_token (copy_src);

                aggr_rewrite_move (line, copy_src);
                if (src_reg >= 0 && known[src_reg])
                {
                    known[d_reg] = 1;
                    value[d_reg] = value[src_reg];
                }
                else
                {
                    known[d_reg] = 0;
                }
                changed = 1;
                continue;
            }
        }

        /* unknown result: invalidate the destination */
        known[d_reg] = 0;
    }

    free (readonly);
    return (changed);
}

/*
 * Propagate "movi src, dst" forward inside one straight line block.  Uses of
 * "dst" are rewritten to "src" until either register is written or control
 * flow starts.  The dead move is removed later by the liveness pass.
 */
static int aggressive_copy_propagate (void)
{
    int changed = 0;
    int i;

    for (i = 0; i < nlines; i++)
    {
        Line *line = &lines[i];
        int src;
        int dst;
        int j;

        if (! line->active || line->kind != LINE_INSTR ||
            line->pre != NULL || line->post != NULL ||
            ! op_is (line, "movi") || line->argc < 2)
        {
            continue;
        }

        src = parse_register_token (line->args[0]);
        dst = parse_register_token (line->args[1]);
        if (src < 0 || dst < 0 || src == dst)
        {
            continue;
        }

        for (j = i + 1; j < nlines; j++)
        {
            Line *next = &lines[j];
            int arg;
            int line_changed = 0;
            int writes_src;
            int writes_dst;
            int control_flow;
            int no_replace;

            if (! next->active)
            {
                continue;
            }
            if (next->kind == LINE_BLANK || next->kind == LINE_COMMENT)
            {
                continue;
            }
            if (next->kind != LINE_INSTR || next->pre != NULL ||
                next->post != NULL)
            {
                break;
            }
            if (! (op_is (next, "intr0") && intr0_readonly (next)) &&
                aggr_opaque (next))
            {
                break;
            }

            writes_src = instr_writes_ireg (next, src);
            writes_dst = instr_writes_ireg (next, dst);
            control_flow = (is_jump_op (next->op) ||
                            op_is (next, "inclsijmpi") ||
                            op_is (next, "decgrijmpi"));

            /*
             * The loop superopcodes increment/decrement before comparing the
             * counter with the limit.  Rewriting the limit to the counter
             * register would therefore change the loop condition.
             */
            no_replace = (op_is (next, "inclsijmpi") ||
                          op_is (next, "decgrijmpi"));

            /*
             * Interrupt arguments are typed "ALL" and may refer to the
             * double register bank, so their numeric tokens must not be
             * rewritten as if they were integer registers.
             */
            if (! no_replace && ! op_is (next, "intr0") && ! op_is (next, "intr1"))
            {
                for (arg = 0; arg < next->argc && arg < OPT_MAX_ARGS; arg++)
                {
                    if (instr_arg_is_ireg (next, arg) &&
                        instr_arg_reads (next, arg) &&
                        ! instr_arg_writes (next, arg) &&
                        parse_register_token (next->args[arg]) == dst)
                    {
                        snprintf (next->args[arg], OPT_ARG_MAX, "%d", src);
                        line_changed = 1;
                    }
                }
            }

            if (line_changed)
            {
                aggr_rewrite_line (next);
                changed = 1;
            }

            if (control_flow)
            {
                break;
            }
            if (writes_dst)
            {
                break;
            }
            if (writes_src)
            {
                break;
            }
        }
    }
    return (changed);
}

static int aggr_line_pure (const Line *line)
{
    static const char *pure_ops[] =
    {
        "movi", "movd",
        "load", "loadl",
        "addi", "subi", "muli", "smuli",
        "andi", "ori", "bandi", "bori", "bxori", "noti",
        "eqi", "neqi", "gri", "lsi", "greqi", "lseqi",
        "addd", "subd", "muld",
        "eqd", "neqd", "grd", "lsd", "greqd", "lseqd",
        NULL
    };
    int i;

    for (i = 0; pure_ops[i] != NULL; i++)
    {
        if (strcmp (line->op, pure_ops[i]) == 0)
        {
            return (1);
        }
    }
    return (0);
}

static int aggressive_dead_code (void)
{
    AggrRegSet *uses = NULL;
    AggrRegSet *defs = NULL;
    AggrRegSet *live_in = NULL;
    AggrRegSet *live_out = NULL;
    int *succ1 = NULL;
    int *succ2 = NULL;
    int *fall = NULL;
    int code_start = -1;
    int code_end = -1;
    int changed;
    int removed = 0;
    int i;
    int k;

    for (i = 0; i < nlines; i++)
    {
        if (! lines[i].active || lines[i].kind != LINE_DIRECTIVE)
        {
            continue;
        }
        if (strcmp (lines[i].op, ".code") == 0)
        {
            code_start = i + 1;
            break;
        }
    }
    for (i = (code_start < 0) ? nlines - 1 : nlines - 1; i >= 0; i--)
    {
        if (! lines[i].active || lines[i].kind != LINE_DIRECTIVE)
        {
            continue;
        }
        if (strcmp (lines[i].op, ".cend") == 0 && i >= code_start)
        {
            code_end = i;
            break;
        }
    }
    if (code_start < 0 || code_end <= code_start)
    {
        return (0);
    }

    uses = (AggrRegSet *) xmalloc ((size_t) nlines * sizeof (AggrRegSet));
    defs = (AggrRegSet *) xmalloc ((size_t) nlines * sizeof (AggrRegSet));
    live_in = (AggrRegSet *) xmalloc ((size_t) nlines * sizeof (AggrRegSet));
    live_out = (AggrRegSet *) xmalloc ((size_t) nlines * sizeof (AggrRegSet));
    succ1 = (int *) xmalloc ((size_t) nlines * sizeof (int));
    succ2 = (int *) xmalloc ((size_t) nlines * sizeof (int));
    fall = (int *) xmalloc ((size_t) nlines * sizeof (int));

    for (i = 0; i < nlines; i++)
    {
        aggr_set_clear (&uses[i]);
        aggr_set_clear (&defs[i]);
        aggr_set_clear (&live_in[i]);
        aggr_set_clear (&live_out[i]);
        succ1[i] = -1;
        succ2[i] = -1;
        fall[i] = -1;
    }

    {
        int next = -1;

        for (i = code_end - 1; i >= code_start; i--)
        {
            fall[i] = next;
            if (lines[i].active && lines[i].kind != LINE_BLANK &&
                lines[i].kind != LINE_COMMENT)
            {
                next = i;
            }
        }
    }

    for (i = code_start; i < code_end; i++)
    {
        Line *line = &lines[i];
        int arg;

        if (! line->active || line->kind == LINE_BLANK ||
            line->kind == LINE_COMMENT)
        {
            continue;
        }

        if (line->kind == LINE_LABEL)
        {
            succ1[i] = fall[i];
            continue;
        }
        if (line->kind == LINE_DIRECTIVE)
        {
            succ1[i] = fall[i];
            continue;
        }
        if (line->kind != LINE_INSTR)
        {
            succ1[i] = fall[i];
            continue;
        }

        if (aggr_opaque (line))
        {
            aggr_set_all (&uses[i]);
        }
        else
        {
            for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
            {
                if (instr_arg_is_ireg (line, arg))
                {
                    int reg = parse_register_token (line->args[arg]);

                    if (reg >= 0)
                    {
                        if (instr_arg_reads (line, arg))
                        {
                            aggr_set_add (&uses[i], reg);
                        }
                        if (instr_arg_writes (line, arg))
                        {
                            aggr_set_add (&defs[i], reg);
                        }
                    }
                }
            }
        }

        if (op_is (line, "jmp") && line->argc >= 1)
        {
            int target = find_label (line->args[0]);

            succ1[i] = (target >= 0) ? fall[target] : -1;
        }
        else if (op_is (line, "jmpi") && line->argc >= 2)
        {
            int target = find_label (line->args[1]);

            succ1[i] = (target >= 0) ? fall[target] : -1;
            succ2[i] = fall[i];
        }
        else if ((op_is (line, "inclsijmpi") ||
                  op_is (line, "decgrijmpi")) && line->argc >= 3)
        {
            int target = find_label (line->args[2]);

            succ1[i] = (target >= 0) ? fall[target] : -1;
            succ2[i] = fall[i];
        }
        else if (op_is (line, "rts") || op_is (line, "jmpa") ||
                 op_is (line, "jsra"))
        {
            succ1[i] = -1;
            succ2[i] = -1;
        }
        else
        {
            succ1[i] = fall[i];
        }
    }

    changed = 1;
    while (changed)
    {
        changed = 0;

        for (i = code_end - 1; i >= code_start; i--)
        {
            AggrRegSet out;
            AggrRegSet in;

            if (! lines[i].active || lines[i].kind == LINE_BLANK ||
                lines[i].kind == LINE_COMMENT)
            {
                continue;
            }

            aggr_set_clear (&out);
            if (succ1[i] >= 0 && succ1[i] < nlines)
            {
                aggr_set_or (&out, &live_in[succ1[i]]);
            }
            if (succ2[i] >= 0 && succ2[i] < nlines)
            {
                aggr_set_or (&out, &live_in[succ2[i]]);
            }

            in = uses[i];
            for (k = 0; k < AGGR_REGSET_WORDS; k++)
            {
                in.word[k] |= out.word[k] & ~defs[i].word[k];
            }

            if (! aggr_set_equal (&in, &live_in[i]) ||
                ! aggr_set_equal (&out, &live_out[i]))
            {
                live_in[i] = in;
                live_out[i] = out;
                changed = 1;
            }
        }
    }

    for (i = code_start; i < code_end; i++)
    {
        Line *line = &lines[i];
        int arg;
        int writes = 0;
        int live = 0;
        int has_double = 0;

        if (! line->active || line->kind != LINE_INSTR ||
            ! aggr_line_pure (line))
        {
            continue;
        }

        for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
        {
            if (instr_arg_is_dreg (line, arg) && instr_arg_writes (line, arg))
            {
                has_double = 1;
                break;
            }
        }
        if (has_double)
        {
            continue;
        }

        for (arg = 0; arg < line->argc && arg < OPT_MAX_ARGS; arg++)
        {
            if (instr_arg_is_ireg (line, arg) &&
                instr_arg_writes (line, arg))
            {
                int reg = parse_register_token (line->args[arg]);

                if (reg >= 0)
                {
                    writes = 1;
                    if (aggr_set_has (&live_out[i], reg))
                    {
                        live = 1;
                    }
                }
            }
        }

        if (writes && ! live)
        {
            line->active = 0;
            removed++;
        }
    }

    free (uses);
    free (defs);
    free (live_in);
    free (live_out);
    free (succ1);
    free (succ2);
    free (fall);
    return (removed);
}

static void optimize_aggressive (void)
{
    int round;

    if (materialize_transformations ())
    {
        build_function_starts ();
        build_call_graph ();
        compute_recursive_functions ();
        recompute_used_regs ();
    }

    for (round = 0; round < 3; round++)
    {
        int changed = 0;

        changed |= aggressive_constant_fold ();
        changed |= aggressive_copy_propagate ();
        changed |= (aggressive_dead_code () > 0);

        if (! changed)
        {
            break;
        }
    }

    recompute_used_regs ();
}

/* ------------------------------------------------------------------------- */
/* peephole                                                                  */
/* ------------------------------------------------------------------------- */

static void peephole (void)
{
    int in_code = 0;
    int i;

    for (i = 0; i < nlines; i++)
    {
        Line *line = &lines[i];

        if (! line->active)
        {
            continue;
        }

        if (line->kind == LINE_DIRECTIVE)
        {
            if (strcmp (line->op, ".code") == 0)
            {
                in_code = 1;
            }
            else if (strcmp (line->op, ".cend") == 0)
            {
                in_code = 0;
            }
            continue;
        }

        if (! in_code || line->kind != LINE_INSTR)
        {
            continue;
        }

        if ((op_is (line, "movi") || op_is (line, "movd")) &&
            line->argc >= 2 && reg_match (line->args[0], line->args[1]))
        {
            line->active = 0;
            continue;
        }

        if (op_is (line, "jmp") && line->argc >= 1)
        {
            int next = next_line_any (i);

            if (next >= 0 && lines[next].kind == LINE_LABEL &&
                strcmp (lines[next].op, line->args[0]) == 0)
            {
                line->active = 0;
            }
        }
    }
}

/* ------------------------------------------------------------------------- */
/* output                                                                    */
/* ------------------------------------------------------------------------- */

static void write_output (const char *filename)
{
    FILE *file;
    int i;

    file = fopen (filename, "w");
    if (file == NULL)
    {
        fprintf (stderr, "l1vm-optimize: can't open output file '%s': %s\n",
                 filename, strerror (errno));
        exit (1);
    }

    for (i = 0; i < nlines; i++)
    {
        Line *line = &lines[i];

        if (! line->active)
        {
            continue;
        }

        if (line->pre != NULL)
        {
            if (fprintf (file, "%s\n", line->pre) < 0)
            {
                fprintf (stderr, "l1vm-optimize: can't write output file!\n");
                fclose (file);
                exit (1);
            }
        }

        if (line->replace != NULL)
        {
            if (fprintf (file, "%s\n", line->replace) < 0)
            {
                fprintf (stderr, "l1vm-optimize: can't write output file!\n");
                fclose (file);
                exit (1);
            }
        }
        else
        {
            if (fprintf (file, "%s\n", line->text) < 0)
            {
                fprintf (stderr, "l1vm-optimize: can't write output file!\n");
                fclose (file);
                exit (1);
            }
        }

        if (line->post != NULL)
        {
            if (fprintf (file, "%s\n", line->post) < 0)
            {
                fprintf (stderr, "l1vm-optimize: can't write output file!\n");
                fclose (file);
                exit (1);
            }
        }
    }

    fclose (file);
}

/* ------------------------------------------------------------------------- */
/* file names and main                                                       */
/* ------------------------------------------------------------------------- */

static void check_file_ending (char *name)
{
    size_t len = strlen (name);

    if (len >= 6 && strcmp (name + len - 6, ".l1asm") == 0)
    {
        name[len - 6] = '\0';
    }
}

static void read_input (const char *filename)
{
    FILE *file;
    char *line = NULL;
    size_t cap = 0;
    ssize_t len;

    file = fopen (filename, "r");
    if (file == NULL)
    {
        fprintf (stderr, "l1vm-optimize: can't open assembly file '%s': %s\n",
                 filename, strerror (errno));
        exit (1);
    }

    while ((len = getline (&line, &cap, file)) != -1)
    {
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        {
            line[len - 1] = '\0';
            len--;
        }
        add_line (line);
    }

    free (line);
    fclose (file);
}

static void show_info (void)
{
    printf ("l1vm-optimize [-O0|-O1|-O2|-O4|-O5|-O6|-O7] <assembly> <optimized-assembly>\n\n");
    printf ("optimizes the L1VM assembly file '<assembly>.l1asm' and writes\n");
    printf ("the result to '<optimized-assembly>.l1asm'.\n\n");
    printf ("  -O0  no optimization\n");
    printf ("  -O1  loop and peephole optimizations (default)\n");
    printf ("  -O2  additionally promote function local variables to registers\n");
    printf ("  -O4  additionally promote plain-suffix locals, single-function globals\n");
    printf ("       and read only globals to registers\n");
    printf ("  -O5  additionally inline small leaf functions\n");
    printf ("  -O6  all from -O4 and -O5 plus loop unrolling\n");
    printf ("  -O7  all from -O6 plus aggressive inlining and unrolling, constant\n");
    printf ("       folding, copy propagation and dead code elimination\n");
}

int main (int ac, char *av[])
{
    char input_name[OPT_LINE_MAX];
    char output_name[OPT_LINE_MAX];
    int arg = 1;

    opcode_count = (int) (sizeof (opcode) / sizeof (opcode[0]));

    if (ac == 2 && (strcmp (av[1], "--help") == 0 ||
                    strcmp (av[1], "-?") == 0 ||
                    strcmp (av[1], "-h") == 0))
    {
        show_info ();
        return (0);
    }

    if (arg < ac && strncmp (av[arg], "-O", 2) == 0)
    {
        if (strcmp (av[arg], "-O0") == 0)
        {
            opt_level = 0;
        }
        else if (strcmp (av[arg], "-O1") == 0 || strcmp (av[arg], "-O") == 0)
        {
            opt_level = 1;
        }
        else if (strcmp (av[arg], "-O2") == 0)
        {
            opt_level = 2;
        }
        else if (strcmp (av[arg], "-O4") == 0)
        {
            opt_level = 4;
        }
        else if (strcmp (av[arg], "-O5") == 0)
        {
            opt_level = 5;
        }
        else if (strcmp (av[arg], "-O6") == 0)
        {
            opt_level = 6;
        }
        else if (strcmp (av[arg], "-O7") == 0)
        {
            opt_level = 7;
        }
        else
        {
            show_info ();
            return (1);
        }
        arg++;
    }

    if (opt_level >= 7)
    {
        inline_max_body = 96;
        inline_max_calls = 16;
        unroll_max_factor = 8;
    }

    if (ac - arg != 2)
    {
        show_info ();
        return (1);
    }

    if (strlen (av[arg]) >= sizeof (input_name) - 7 ||
        strlen (av[arg + 1]) >= sizeof (output_name) - 7)
    {
        die ("file name too long");
    }

    strcpy (input_name, av[arg]);
    strcpy (output_name, av[arg + 1]);
    check_file_ending (input_name);
    check_file_ending (output_name);
    strcat (input_name, ".l1asm");
    strcat (output_name, ".l1asm");

    if (strcmp (input_name, output_name) == 0)
    {
        die ("input and output file are identical");
    }

    read_input (input_name);
    collect_data_constants ();
    collect_data_symbols ();
    compute_used_regs ();

    if (opt_level >= 1)
    {
        optimize_simple_loops ();
        optimize_rotated_loops ();
    }

    if (opt_level >= 2)
    {
        promote_function_locals (opt_level);
    }

    if (opt_level >= 5)
    {
        inline_functions ();
    }

    if (opt_level >= 6)
    {
        optimize_loop_unroll ();
    }

    if (opt_level >= 7)
    {
        optimize_aggressive ();
    }

    if (opt_level >= 1)
    {
        peephole ();
    }

    write_output (output_name);

    printf ("optimized assembly: '%s' -> '%s'\n", input_name, output_name);
    return (0);
}
