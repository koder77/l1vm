/*
 * This file l1lang.c is part of L1vm.
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
 */

#define _POSIX_C_SOURCE 200809L

#include "l1lang.h"

#include <ctype.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

/* ==================== settings ==================== */

L1Settings l1_settings = {
    .l1com_enabled = L1_L1COM_AUTO,
    .l1com_path = NULL,
    .include_dirs = NULL,
    .static_diag = 1,
    .missing_tilde_hint = 1,
};

/* ==================== vector helpers ==================== */

#define L1_VEC_GROW(v, TYPE)                                            \
    do {                                                               \
        if ((v).len >= (v).cap) {                                       \
            int ncap = (v).cap ? (v).cap * 2 : 32;                      \
            TYPE *nd = realloc((v).data, (size_t)ncap * sizeof(TYPE));  \
            if (nd) { (v).data = nd; (v).cap = ncap; }                  \
            else break;                                                 \
        }                                                               \
    } while (0)

#define VEC_PUSH(v, item, TYPE)                                         \
    do {                                                               \
        L1_VEC_GROW(v, TYPE);                                           \
        if ((v).len < (v).cap) (v).data[(v).len++] = (item);           \
    } while (0)

/* ==================== keyword / type tables ==================== */
static const char *const l1_keywords[] = {
    "func", "funcend", "object", "objectend", "set",
    "if", "if+", "else", "endif", "do", "while", "for", "for-loop",
    "next", "switch", "switchend", "break",
    "return", "stpush", "stpop", "stpushi", "stpopi", "stpushd",
    "stpopd", "stpushb", "stpopb", "call", "pointer", "cast",
    "jmp", "jmpi", "jsr", "jsra", "jmpa", "loadreg", "savereg",
    "reset-reg", "thread", "join", "threadexit",
    "unsafe", "unsafe-end", "optimize-if", "optimize-if-off",
    "no-var-pull-on", "no-var-pull-off",
    "variable-immutable", "variable-mutable",
    "variable-local-on", "variable-local-off",
    "variable-local-only-on", "variable-local-only-off",
    "pure-off", "forbid-unsafe", "linter",
    "contracts-on", "contracts-off", "precondition",
    "precondition-end", "postcondition", "postcondition-end",
    "varname-ascii", "nested-code-on", "nested-code-off",
    "data-local-alloc", "data-local", "data-global",
    "data_func_local_alloc", "data_func_local", "data_func_global",
    "data_func_free", "vcpu", "vcpu_RUN", "vcpu_JOIN", "vcpu_EXIT",
    "vcpu_SCHEDULERMAX", "get_cpu", "free_cpus",
    "range", "?", "m=", "ASM", "ASM_END",
    NULL
};

static const char *const l1_types[] = {
    "bool", "byte", "int16", "int32", "int64", "double", "string",
    "const-bool", "const-byte", "const-int16", "const-int32",
    "const-int64", "const-double", "const-string",
    "mut-bool", "mut-byte", "mut-int16", "mut-int32",
    "mut-int64", "mut-double", "mut-string",
    NULL
};

static int str_in_list(const char *const *list, const char *s)
{
    int i;
    for (i = 0; list[i]; i++)
        if (strcmp(list[i], s) == 0)
            return 1;
    return 0;
}

int l1_is_keyword(const char *s) { return str_in_list(l1_keywords, s); }
int l1_is_type(const char *s) { return str_in_list(l1_types, s); }

/* ==================== builtins ==================== */

static const L1Builtin l1_builtin_table[] = {
    /* intr-func.l1h */
    { "print_s", "intr-func.l1h", "variable~", "Print string variable~." },
    { "print_i", "intr-func.l1h", "variable~", "Print int64 variable~." },
    { "print_d", "intr-func.l1h", "variable~", "Print double variable~." },
    { "print_n", "intr-func.l1h", "", "Print a newline." },
    { "print_lns", "intr-func.l1h", "variable~", "Print string variable~ and newline." },
    { "print_lni", "intr-func.l1h", "variable~", "Print int64 variable~ and newline." },
    { "print_lnd", "intr-func.l1h", "variable~", "Print double variable~ and newline." },
    { "printf_d", "intr-func.l1h", "format~ value~", "Print a double using a printf-style format string." },
    { "input_i", "intr-func.l1h", "variable~", "Read an int64 from stdin into variable~." },
    { "input_d", "intr-func.l1h", "variable~", "Read a double from stdin into variable~." },
    { "input_s", "intr-func.l1h", "maxlen~ variable~", "Read a string from stdin (max. maxlen~ chars) into variable~." },
    { "exit", "intr-func.l1h", "code~", "Exit the program with exit code~." },
    { "time", "intr-func.l1h", "", "Push hour, minute, second on the stack (sec on top)." },
    { "date", "intr-func.l1h", "", "Push year, month, day on the stack (day on top)." },
    { "start_timer", "intr-func.l1h", "", "Start the internal timer." },
    { "stop_timer", "intr-func.l1h", "time~", "Stop the timer and push elapsed milliseconds." },
    { "detime", "intr-func.l1h", "ms~", "Delay/sleep for ms~ milliseconds." },
    { "epochms", "intr-func.l1h", "", "Push milliseconds since the epoch." },
    { "shell_args", "intr-func.l1h", "count~", "Get the shell argument count." },
    { "get_shell_arg", "intr-func.l1h", "i~ variable~", "Get shell argument i~." },
    { "get_host_cpu", "host.l1h", "variable~", "Get the CPU type." },
    { "get_host_os", "host.l1h", "variable~", "Get the OS type." },
    /* file-lib.l1h */
    { "file_init", "file-lib.l1h", "max_handles~ count~", "Initialize the file module. Push 0 on success." },
    { "file_open", "file-lib.l1h", "mode~ filenameaddr~", "Open a file. Push the file handle (negative = error)." },
    { "file_close", "file-lib.l1h", "handle~", "Close a file handle." },
    { "file_put_string", "file-lib.l1h", "handle~ stringaddr~", "Write a string to the file." },
    { "file_get_string", "file-lib.l1h", "handle~ bufferaddr~ len~", "Read a string from the file." },
    { "file_put_int64", "file-lib.l1h", "handle~ value~", "Write an int64 to the file." },
    { "file_get_int64", "file-lib.l1h", "handle~", "Read an int64 from the file." },
    { "file_put_double", "file-lib.l1h", "handle~ value~", "Write a double to the file." },
    { "file_get_double", "file-lib.l1h", "handle~", "Read a double from the file." },
    { "file_putc", "file-lib.l1h", "handle~ byte~", "Write a single byte to the file." },
    { "file_getc", "file-lib.l1h", "handle~", "Read a single byte from the file." },
    { "file_seek", "file-lib.l1h", "handle~ offset~ whence~", "Seek in the file." },
    { "file_flush", "file-lib.l1h", "handle~", "Flush the file buffer." },
    { "file_free_mem", "file-lib.l1h", "", "Free the file module memory." },
    /* string.l1h */
    { "string_init", "string.l1h", "", "Initialize the string module." },
    { "string_cat", "string.l1h", "srcaddr~ destaddr~", "Append src string to dest string." },
    { "string_len", "string.l1h", "straddr~", "Push the string length." },
    { "string_copy", "string.l1h", "srcaddr~ destaddr~", "Copy src string to dest string." },
    { "string_compare", "string.l1h", "str1addr~ str2addr~", "Compare strings. Push 0 if equal." },
    { "string_left", "string.l1h", "srcaddr~ destaddr~ num~", "Take the left num~ characters." },
    { "string_right", "string.l1h", "srcaddr~ destaddr~ num~", "Take the right num~ characters." },
    { "string_mid", "string.l1h", "srcaddr~ destaddr~ pos~", "Take the character at position pos~." },
    { "string_search", "string.l1h", "straddr~ searchaddr~", "Find a substring. Push position (-1 if not found)." },
    { "string_int64tostring", "string.l1h", "num~ straddr~ buflen~", "Convert an int64 to a string." },
    { "string_doubletostring", "string.l1h", "num~ straddr~ buflen~", "Convert a double to a string." },
    { "string_bytetohexstring", "string.l1h", "num~ straddr~ buflen~", "Convert a byte to a hex string." },
    { "string_replace", "string.l1h", "input~ search~ replace~ output~", "Replace all occurrences of search~ with replace~." },
    { "string_regex", "string.l1h", "str~ regex~", "Regex match." },
    { "string_verify", "string.l1h", "str~ verify~", "Verify the characters of a string." },
    { "string_padd", "string.l1h", "dest~ len~ padchar~", "Pad a string to len~ with padchar~." },
    { "string_parse_json", "string.l1h", "json~ key~ value~ maxlen~", "Parse JSON, extract the value for key~." },
    { "get_env", "string.l1h", "envname~", "Get an environment variable. Push its value." },
    { "set_env", "string.l1h", "name~ value~", "Set an environment variable." },
    { "string_to_array", "string.l1h", "srcaddr~ destaddr~ index~ len~ size~", "Copy a string to a byte array." },
    { "array_to_string", "string.l1h", "srcaddr~ destaddr~ index~ len~ size~", "Copy a byte array to a string." },
    { "free_mod", "string.l1h", "", "Free the module memory." },
    /* mem-lib.l1h */
    { "mem_init", "mem-lib.l1h", "max~ count~", "Initialize the memory module. Push 0 on success." },
    { "alloc_byte", "mem-lib.l1h", "size~", "Allocate a byte array. Push the address (negative = error)." },
    { "alloc_int16", "mem-lib.l1h", "size~", "Allocate an int16 array. Push the address." },
    { "alloc_int32", "mem-lib.l1h", "size~", "Allocate an int32 array. Push the address." },
    { "alloc_int64", "mem-lib.l1h", "size~", "Allocate an int64 array. Push the address." },
    { "alloc_double", "mem-lib.l1h", "size~", "Allocate a double array. Push the address." },
    { "dealloc", "mem-lib.l1h", "addr~", "Free allocated memory." },
    { "int_to_array", "mem-lib.l1h", "addr~ index~ value~", "Write an int64 into the array." },
    { "double_to_array", "mem-lib.l1h", "addr~ index~ value~", "Write a double into the array." },
    { "array_to_int", "mem-lib.l1h", "addr~ index~", "Read an int64 from the array. Push the value." },
    { "array_to_double", "mem-lib.l1h", "addr~ index~", "Read a double from the array. Push the value." },
    { "free_mem", "mem-lib.l1h", "", "Free the memory module." },
    { "alloc_int64_vect", "mem-lib.l1h", "size~", "Allocate an int64 vector." },
    { "alloc_double_vect", "mem-lib.l1h", "size~", "Allocate a double vector." },
    { "int_to_vect", "mem-lib.l1h", "addr~ index~ value~", "Insert an int64 into a vector." },
    { "vect_to_int", "mem-lib.l1h", "addr~ index~", "Read an int64 from a vector." },
    { "vect_erase", "mem-lib.l1h", "addr~", "Erase a vector element." },
    { "get_vect_size", "mem-lib.l1h", "addr~", "Push the vector size." },
    /* mem-obj-lib.l1h */
    { "mem_obj_init", "mem-obj-lib.l1h", "max~ count~", "Initialize the memory object module." },
    { "alloc_obj_memobj", "mem-obj-lib.l1h", "vars~ memsize~", "Allocate a memory object. Push the address." },
    { "save_obj_memobj", "mem-obj-lib.l1h", "values... vars~ zero~ addr~", "Save values into a memory object." },
    { "load_obj_string_memobj", "mem-obj-lib.l1h", "index~ addr~ dest~", "Load a string from a memory object." },
    { "load_obj_memobj", "mem-obj-lib.l1h", "index1~ index2~ addr~", "Load values from a memory object." },
    { "free_obj_memobj", "mem-obj-lib.l1h", "addr~", "Free a memory object." },
    /* math-lib.l1h */
    { "math_init", "math-lib.l1h", "", "Initialize the math module." },
    { "math_randinit", "math-lib.l1h", "seed~", "Seed the random number generator." },
    { "math_randint", "math-lib.l1h", "", "Push a random int64." },
    { "math_not", "math-lib.l1h", "value~", "Logical NOT." },
    { "math_degree_to_rad", "math-lib.l1h", "angle~", "Convert degrees to radians." },
    { "math_sindouble", "math-lib.l1h", "rad~", "Sine of an angle in radians." },
    /* process-lib.l1h */
    { "process_init", "process-lib.l1h", "", "Initialize the process module." },
    { "run_shell", "process-lib.l1h", "command~", "Run a shell command. Push the return value." },
    /* net-lib.l1h */
    { "net_init", "net-lib.l1h", "mod~ count~", "Initialize the network module." },
    { "get_hostbyname", "net-lib.l1h", "ipaddr~ bufaddr~", "Resolve a hostname." },
    { "open_server_socket", "net-lib.l1h", "bufaddr~ port~", "Open a server socket." },
    { "open_accept_server", "net-lib.l1h", "socket~", "Accept a connection." },
    { "socket_read_string", "net-lib.l1h", "socket~ buf~ size~", "Read a string from a socket." },
    { "close_server_socket", "net-lib.l1h", "socket~", "Close a server socket." },
    /* sdl-lib.l1h */
    { "sdl_open_screen", "sdl-lib.l1h", "zero~ w~ h~ bit~ zero~", "Open the SDL screen." },
    { "sdl_font_ttf", "sdl-lib.l1h", "fontaddr~ size~", "Load a TTF font." },
    { "sdl_rectangle_fill", "sdl-lib.l1h", "zero~ zero~ w~ h~ r~ g~ b~ a~", "Fill a rectangle." },
    { "set_gadget_button", "sdl-lib.l1h", "x~ y~ text~ status~", "Set a button gadget." },
    { "set_gadget_string", "sdl-lib.l1h", "x~ y~ text~ value~ status~", "Set a string gadget." },
    { "sdl_update", "sdl-lib.l1h", "", "Update the screen." },
    { "sdl_text_ttf", "sdl-lib.l1h", "x~ y~ r~ g~ b~ straddr~", "Draw text." },
    /* crypto / json / messages */
    { "crypto_init", "crypto.l1h", "", "Initialize the crypto module." },
    { "encrypt", "crypto.l1h", "in~ size~ out~ key~ nonce~ gen~ mode~", "Encrypt/decrypt data." },
    { "parse_json", "json.l1h", "", "Initialize JSON parsing." },
    { "create_json", "json.l1h", "values... entries~ json~", "Create JSON." },
    { "msg_init", "messages-lib.l1h", "", "Initialize the messages module." },
    { "msg_put_message", "messages-lib.l1h", "addr~ message~ lock~", "Send a message." },
    { "msg_wait_message", "messages-lib.l1h", "addr~ message~ lock~", "Receive a message (blocking)." },
    /* misc-macros.l1h */
    { "pull_int64_var", "misc-macros.l1h", "variable~", "Pull an int64 from the stack into variable~." },
    { "pull_double_var", "misc-macros.l1h", "variable~", "Pull a double from the stack into variable~." },
    { "pull_byte_var", "misc-macros.l1h", "variable~", "Pull a byte from the stack into variable~." },
    { "inc", "misc-macros.l1h", "variable~", "Increment variable~ by 1." },
    { "dec", "misc-macros.l1h", "variable~", "Decrement variable~ by 1." },
    { NULL, NULL, NULL, NULL }
};

const L1Builtin *l1_builtins(void) { return l1_builtin_table; }

const L1Builtin *l1_find_builtin(const char *name)
{
    int i;
    if (!name)
        return NULL;
    for (i = 0; l1_builtin_table[i].name; i++)
        if (strcmp(l1_builtin_table[i].name, name) == 0)
            return &l1_builtin_table[i];
    return NULL;
}

/* ==================== utf16 <-> utf8 ==================== */

int l1_utf8_to_utf16(const char *s, int byte_off)
{
    int col = 0, i = 0;
    if (!s)
        return 0;
    while (i < byte_off) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80)
            i += 1;
        else if ((c & 0xE0) == 0xC0)
            i += 2;
        else if ((c & 0xF0) == 0xE0)
            i += 3;
        else if ((c & 0xF8) == 0xF0) {
            i += 4;
            col++;          /* supplementary char = 2 UTF-16 units */
        } else
            i += 1;
        col++;
    }
    return col;
}

int l1_utf16_to_utf8(const char *s, int col)
{
    int i = 0, c = 0;
    if (!s)
        return 0;
    while (c < col && s[i]) {
        unsigned char ch = (unsigned char)s[i];
        if (ch < 0x80)
            i += 1;
        else if ((ch & 0xE0) == 0xC0)
            i += 2;
        else if ((ch & 0xF0) == 0xE0)
            i += 3;
        else if ((ch & 0xF8) == 0xF0) {
            i += 4;
            c++;            /* counts 2 UTF-16 units */
        } else
            i += 1;
        c++;
    }
    return i;
}

int l1_is_word_char(unsigned char c)
{
    return isalnum(c) || c == '_' || c == '~' || c == '.' ||
           c == '$' || c == '\\';
}

static int l1_is_label_char(unsigned char c)
{
    return l1_is_word_char(c) || c == '-' || c == '>';
}

/* ==================== tokenizer ==================== */

static void tok_push(L1Doc *d, int kind, int line, int bstart, int blen,
                     int start, int flags)
{
    L1Tok t;
    memset(&t, 0, sizeof(t));
    t.kind = kind;
    t.line = line;
    t.bstart = bstart;
    t.blen = blen;
    t.start = start;
    t.len = l1_utf8_to_utf16(d->lines[line].text + bstart, blen);
    t.flags = flags;
    if (kind == TK_WORD || kind == TK_LABEL || kind == TK_PREPROC)
        t.text = strndup(d->lines[line].text + bstart, (size_t)blen);
    VEC_PUSH(d->toks, t, L1Tok);
}

static void l1_tokenize_line(L1Doc *d, int li)
{
    const char *s = d->lines[li].text;
    int nb = d->lines[li].nbytes;
    int i = 0;

    while (i < nb) {
        unsigned char c = (unsigned char)s[i];

        /* whitespace */
        if (c == ' ' || c == '\t' || c == '\r') {
            i++;
            continue;
        }
        /* comment */
        if (c == '/' && i + 1 < nb && s[i + 1] == '/') {
            tok_push(d, TK_COMMENT, li, i, nb - i, l1_utf8_to_utf16(s, i), 0);
            return;
        }
        /* string */
        if (c == '"') {
            int j = i + 1;
            while (j < nb && s[j] != '"')
                j++;
            if (j < nb)
                j++;
            tok_push(d, TK_STRING, li, i, j - i, l1_utf8_to_utf16(s, i), 0);
            i = j;
            continue;
        }
        /* number (digit first) */
        if (isdigit(c)) {
            int j = i;
            while (j < nb && (isdigit((unsigned char)s[j]) || s[j] == '.'))
                j++;
            /* optional trailing base suffix letters: 42Q, 23_const -> kept in word */
            while (j < nb && (isalpha((unsigned char)s[j])))
                j++;
            tok_push(d, TK_NUMBER, li, i, j - i, l1_utf8_to_utf16(s, i), 0);
            i = j;
            continue;
        }
        /* label */
        if (c == ':') {
            int j = i + 1;
            while (j < nb && l1_is_label_char((unsigned char)s[j]))
                j++;
            if (j == i + 1) {
                tok_push(d, TK_OP, li, i, 1, l1_utf8_to_utf16(s, i), 0);
                i++;
                continue;
            }
            tok_push(d, TK_LABEL, li, i, j - i, l1_utf8_to_utf16(s, i), 0);
            i = j;
            continue;
        }
        /* preprocessor */
        if (c == '#') {
            int j = i + 1;
            while (j < nb && (isalnum((unsigned char)s[j]) || s[j] == '_'))
                j++;
            tok_push(d, TK_PREPROC, li, i, j - i, l1_utf8_to_utf16(s, i), 0);
            i = j;
            continue;
        }
        /* word */
        if (l1_is_word_char(c)) {
            int j = i;
            while (j < nb && l1_is_word_char((unsigned char)s[j]))
                j++;
            tok_push(d, TK_WORD, li, i, j - i, l1_utf8_to_utf16(s, i), 0);
            i = j;
            continue;
        }
        /* multi-char operators */
        if (i + 1 < nb) {
            const char *m2 = NULL;
            switch (c) {
            case '=': if (s[i + 1] == '=') m2 = "=="; break;
            case '!': if (s[i + 1] == '=') m2 = "!="; break;
            case '<':
                if (s[i + 1] == '=') m2 = "<=";
                else if (s[i + 1] == '<') m2 = "<<";
                else if (s[i + 1] == 'd') m2 = "<d";
                break;
            case '>':
                if (s[i + 1] == '=') m2 = ">=";
                else if (s[i + 1] == '>') m2 = ">>";
                else if (s[i + 1] == 'd') m2 = ">d";
                break;
            case '&': if (s[i + 1] == '&') m2 = "&&"; break;
            case '|': if (s[i + 1] == '|') m2 = "||"; break;
            case ':': if (s[i + 1] == '=') m2 = ":="; break;
            case 'm': if (s[i + 1] == '=') m2 = "m="; break;
            case '-': if (s[i + 1] == '>') m2 = "->"; break;
            default: break;
            }
            if (m2) {
                tok_push(d, TK_OP, li, i, 2, l1_utf8_to_utf16(s, i), 0);
                i += 2;
                continue;
            }
        }
        /* 3-char double operators +d -d *d /d ==d etc. */
        if (i + 2 < nb) {
            char three[4] = { s[i], s[i + 1], s[i + 2], 0 };
            static const char *const t3[] = { "+d", "-d", "*d", "/d",
                                              "==d", "!=d", "<=d", ">=d",
                                              "<d", ">d", NULL };
            int k;
            int found = 0;
            for (k = 0; t3[k]; k++) {
                if (strcmp(three, t3[k]) == 0) {
                    found = 1;
                    break;
                }
            }
            if (found) {
                tok_push(d, TK_OP, li, i, 3, l1_utf8_to_utf16(s, i), 0);
                i += 3;
                continue;
            }
        }
        /* single char */
        switch (c) {
        case '(': tok_push(d, TK_LPAREN, li, i, 1, l1_utf8_to_utf16(s, i), 0); break;
        case ')': tok_push(d, TK_RPAREN, li, i, 1, l1_utf8_to_utf16(s, i), 0); break;
        case '{': tok_push(d, TK_LBRACE, li, i, 1, l1_utf8_to_utf16(s, i), 0); break;
        case '}': tok_push(d, TK_RBRACE, li, i, 1, l1_utf8_to_utf16(s, i), 0); break;
        case '[': tok_push(d, TK_LBRACKET, li, i, 1, l1_utf8_to_utf16(s, i), 0); break;
        case ']': tok_push(d, TK_RBRACKET, li, i, 1, l1_utf8_to_utf16(s, i), 0); break;
        case ',': tok_push(d, TK_COMMA, li, i, 1, l1_utf8_to_utf16(s, i), 0); break;
        case ';': tok_push(d, TK_SEMI, li, i, 1, l1_utf8_to_utf16(s, i), 0); break;
        default:
            tok_push(d, TK_OP, li, i, 1, l1_utf8_to_utf16(s, i), 0);
            break;
        }
        i++;
    }
}

/* ==================== small text helpers ==================== */

static int l1_is_preprocessor_directive(const char *p)
{
    static const char *const kw[] = { "include", "var", "func", "define" };
    size_t k;
    if (*p != '#')
        return 0;
    p++;
    for (k = 0; k < sizeof(kw) / sizeof(kw[0]); k++) {
        size_t n = strlen(kw[k]);
        if (strncmp(p, kw[k], n) == 0 &&
            !isalnum((unsigned char)p[n]))
            return 1;
    }
    return 0;
}

static void strip_comment(const char *line, char *out, size_t outsz)
{
    int i = 0, in_str = 0;
    size_t o = 0;
    while (line[i]) {
        char c = line[i];
        if (in_str) {
            out[o++] = c;
            if (c == '"')
                in_str = 0;
        } else {
            if (c == '"') {
                in_str = 1;
                out[o++] = c;
            } else if (c == '/' && line[i + 1] == '/') {
                break;
            } else if (c == '#') {
                if (o > 0) {
                    size_t k;
                    int at_line_start = 1;
                    for (k = 0; k < o; k++)
                        if (out[k] != ' ' && out[k] != '\t') {
                            at_line_start = 0;
                            break;
                        }
                    if (!at_line_start) {
                        break;
                    }
                }
                if (l1_is_preprocessor_directive(line + i))
                    out[o++] = c;
                else
                    break;
            } else {
                out[o++] = c;
            }
        }
        i++;
        if (o + 1 >= outsz)
            break;
    }
    while (o > 0 && (out[o - 1] == ' ' || out[o - 1] == '\t'))
        o--;
    out[o] = '\0';
}

static char *l1_trim(char *s)
{
    char *e;
    while (*s == ' ' || *s == '\t')
        s++;
    e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r'))
        e--;
    *e = '\0';
    return s;
}

/* extract a whitespace separated token, returns new string */
static char *next_tok(const char **pp)
{
    const char *p = *pp;
    const char *start;
    size_t n;
    while (*p == ' ' || *p == '\t')
        p++;
    if (*p == '\0') {
        *pp = p;
        return strdup("");
    }
    start = p;
    while (*p && *p != ' ' && *p != '\t')
        p++;
    n = (size_t)(p - start);
    *pp = p;
    return strndup(start, n);
}

/* ==================== set declaration parsing ==================== */

/*
 * Parse a "(set TYPE SIZE NAME ...)" or "{set ...}" declaration.
 * src must be the comment-stripped line. Returns 0 and fills out vars
 * (caller frees) or 1 if not a set declaration / malformed.
 */
static int parse_set_decl(const char *src, char **ptype, char **psize,
                          char **pname, int *name_byte_start)
{
    const char *p = src;
    char *tok;

    /* skip leading parens/braces */
    while (*p == '(' || *p == '{' || *p == ' ' || *p == '\t')
        p++;

    tok = next_tok(&p);
    if (strcmp(tok, "set") != 0) {
        free(tok);
        return 1;
    }
    free(tok);

    *ptype = next_tok(&p);
    *psize = next_tok(&p);
    *pname = next_tok(&p);
    *name_byte_start = (int)(p - src) - (int)strlen(*pname);

    if (**ptype == '\0' || **psize == '\0' || **pname == '\0') {
        free(*ptype); *ptype = NULL;
        free(*psize); *psize = NULL;
        free(*pname); *pname = NULL;
        return 1;
    }
    return 0;
}

static int l1_type_ok(const char *type)
{
    return l1_is_type(type);
}

/* ==================== scope tracking ==================== */

typedef struct {
    char name[256];
    char parent[256];
    int kind;           /* 0 func, 1 object */
    int sym_index;      /* index into funcs/objs */
    int start_line;
} L1Block;

static L1Block l1_blockstack[512];
static int l1_nblocks;

static const char *l1_current_scope(void)
{
    if (l1_nblocks > 0)
        return l1_blockstack[l1_nblocks - 1].name;
    return "";
}

/* enclosing function scope for a line */
static const char *l1_scope_at(const L1Doc *d, int line)
{
    static char buf[512];
    int i;
    for (i = 0; i < d->funcs.len; i++) {
        int end = d->funcs.data[i].end_line;
        if (line >= d->funcs.data[i].line && (end == -1 || line <= end))
            return d->funcs.data[i].name;
    }
    for (i = 0; i < d->objs.len; i++) {
        int end = d->objs.data[i].end_line;
        if (line >= d->objs.data[i].line && (end == -1 || line <= end))
            return d->objs.data[i].name;
    }
    buf[0] = '\0';
    return buf;
}

/* is var with given scope visible from current scope? */
static int l1_scope_visible(const char *var_scope, const char *cur_scope,
                            const L1Doc *d)
{
    (void)d;
    if (strcmp(var_scope, "") == 0)      /* file level */
        return 1;
    if (strcmp(var_scope, "lib") == 0)   /* from library includes */
        return 1;
    if (strcmp(var_scope, cur_scope) == 0)
        return 1;
    if (strcmp(var_scope, "main") == 0)  /* globals in main */
        return 1;
    return 0;
}

static int l1_find_var(const L1Doc *d, const char *name, const char *scope)
{
    int i;
    for (i = 0; i < d->vars.len; i++) {
        const L1Var *v = &d->vars.data[i];
        if (strcmp(v->name, name) == 0 && l1_scope_visible(v->scope, scope, d))
            return i;
    }
    return -1;
}

static int l1_find_func(const L1Doc *d, const char *name)
{
    int i;
    for (i = 0; i < d->funcs.len; i++)
        if (strcmp(d->funcs.data[i].name, name) == 0 &&
            !d->funcs.data[i].parent)
            return i;
    return -1;
}

static int l1_find_method(const L1Doc *d, const char *name, const char *parent)
{
    int i;
    for (i = 0; i < d->funcs.len; i++)
        if (d->funcs.data[i].parent &&
            strcmp(d->funcs.data[i].name, name) == 0 &&
            strcmp(d->funcs.data[i].parent, parent) == 0)
            return i;
    return -1;
}

static int l1_find_label(const L1Doc *d, const char *name)
{
    int i;
    for (i = 0; i < d->labels.len; i++)
        if (strcmp(d->labels.data[i].name, name) == 0)
            return i;
    return -1;
}

static int l1_find_macro(const L1Doc *d, const char *name)
{
    int i;
    for (i = 0; i < d->macros.len; i++)
        if (strcmp(d->macros.data[i].name, name) == 0)
            return i;
    return -1;
}

/* ==================== diagnostics helpers ==================== */

static void diag_add(L1Doc *d, L1Sev sev, const char *msg, int line, int col,
                     int end_line, int end_col, const char *source)
{
    L1Diag g;
    memset(&g, 0, sizeof(g));
    g.sev = sev;
    g.msg = strdup(msg);
    g.source = strdup(source ? source : "l1vm-lsp");
    g.line = line;
    g.col = col;
    g.end_line = end_line < 0 ? line : end_line;
    g.end_col = end_col;
    VEC_PUSH(d->diags, g, L1Diag);
}

static void diag_line(L1Doc *d, L1Sev sev, const char *msg, int line,
                      const char *source)
{
    int nc = 0;
    if (line >= 0 && line < d->nlines)
        nc = l1_utf8_to_utf16(d->lines[line].text, d->lines[line].nbytes);
    diag_add(d, sev, msg, line < 0 ? 0 : line, 0, line, nc, source);
}

/* line is content if it has non-comment, non-whitespace text */
static int l1_is_content_line(const L1Doc *d, int li)
{
    const char *s = d->lines[li].text;
    char buf[2048];
    strip_comment(s, buf, sizeof(buf));
    return *l1_trim(buf) != '\0';
}

static int l1_next_content_line(const L1Doc *d, int after)
{
    int i;
    for (i = after + 1; i < d->nlines; i++)
        if (l1_is_content_line(d, i))
            return i;
    return -1;
}

/* ==================== analyze: pass 1 structure ==================== */

static void l1_analyze_line(L1Doc *d, int li)
{
    char buf[2048];
    char clean[2048];
    const char *line = d->lines[li].text;
    char *p;
    int i;

    strip_comment(line, clean, sizeof(clean));
    snprintf(buf, sizeof(buf), "%s", clean);
    p = l1_trim(buf);

    /* preprocessor directives */
    if (*p == '#') {
        if (strncmp(p, "#include", 8) == 0) {
            L1Inc inc;
            char *fn = strchr(p, '<');
            char *tmp = NULL;
            memset(&inc, 0, sizeof(inc));
            if (fn) {
                char *e = strchr(fn, '>');
                if (e) {
                    *e = '\0';
                    tmp = strdup(fn + 1);
                }
            } else {
                fn = strchr(p, '"');
                if (fn) {
                    char *e = strrchr(fn, '"');
                    if (e) {
                        *e = '\0';
                        tmp = strdup(fn + 1);
                    }
                }
            }
            inc.name = tmp ? tmp : strdup("");
            inc.file = strdup(tmp ? tmp : "");
            inc.line = li;
            inc.col = 0;
            VEC_PUSH(d->includes, inc, L1Inc);
        } else if (strncmp(p, "#func", 5) == 0) {
            /* #func name (ARGS) :{...} */
            char *nm = p + 5;
            char *args = NULL;
            char *paren = strchr(nm, '(');
            L1Macro m;
            while (*nm == ' ' || *nm == '\t')
                nm++;
            if (paren) {
                char *e = strchr(paren, ')');
                if (e) {
                    args = strndup(paren + 1, (size_t)(e - paren - 1));
                    *paren = '\0';
                }
            }
            memset(&m, 0, sizeof(m));
            m.name = strdup(nm);
            m.args = args ? args : strdup("");
            m.line = li;
            m.col = 0;
            VEC_PUSH(d->macros, m, L1Macro);
        } else if (strncmp(p, "#var", 4) == 0) {
            /* scope marker: #var ~ name */
        } else if (strncmp(p, "#define", 7) == 0) {
            char *nm = p + 7;
            L1Macro m;
            while (*nm == ' ' || *nm == '\t')
                nm++;
            {
                char *sp = nm;
                while (*sp && *sp != ' ' && *sp != '\t')
                    sp++;
                memset(&m, 0, sizeof(m));
                m.name = strndup(nm, (size_t)(sp - nm));
                m.args = strdup("");
                m.line = li;
                m.col = 0;
                VEC_PUSH(d->macros, m, L1Macro);
            }
        }
        return;
    }

    /* ASM block markers */
    if (strcmp(p, "(ASM)") == 0) {
        d->inside_asm = 1;
        return;
    }
    if (strcmp(p, "(ASM_END)") == 0) {
        d->inside_asm = 0;
        return;
    }

    /* function / object definition: (name func) / (name object) */
    if (strncmp(p, "(", 1) == 0) {
        char *inner = strchr(p, '(');
        char *inner_end;
        char name[512];
        char kw[64];
        int nlen = 0;
        name[0] = '\0';
        kw[0] = '\0';

        inner++;                 /* skip ( */
        while (*inner == ' ' || *inner == '\t')
            inner++;
        inner_end = inner;
        while (*inner_end && *inner_end != ' ' && *inner_end != ')' &&
               *inner_end != '(')
            inner_end++;
        nlen = (int)(inner_end - inner);
        if (nlen > 0 && nlen < 512) {
            memcpy(name, inner, (size_t)nlen);
            name[nlen] = '\0';
            {
                char *k = inner_end;
                while (*k == ' ' || *k == '\t')
                    k++;
                if (strncmp(k, "func", 4) == 0 &&
                    (k[4] == ')' || k[4] == ' ' || k[4] == '\0')) {
                    snprintf(kw, sizeof(kw), "func");
                } else if (strncmp(k, "object", 6) == 0) {
                    snprintf(kw, sizeof(kw), "object");
                } else {
                    kw[0] = '\0';
                }
            }

            if (strcmp(kw, "func") == 0 && name[0] != ':') {
                L1Func f;
                memset(&f, 0, sizeof(f));
                f.name = strdup(name);
                f.line = li;
                f.col = 0;
                f.end_line = -1;
                f.is_object = 0;
                if (l1_nblocks > 0 &&
                    l1_blockstack[l1_nblocks - 1].kind == 1)
                    f.parent = strdup(l1_blockstack[l1_nblocks - 1].name);
                else
                    f.parent = NULL;
                VEC_PUSH(d->funcs, f, L1Func);

                if (strcmp(name, "main") == 0)
                    d->has_main = 1;

                {
                    L1Block b;
                    memset(&b, 0, sizeof(b));
                    snprintf(b.name, sizeof(b.name), "%.255s", name);
                    snprintf(b.parent, sizeof(b.parent), "%.255s",
                             f.parent ? f.parent : "");
                    b.kind = 0;
                    b.sym_index = d->funcs.len - 1;
                    b.start_line = li;
                    if (l1_nblocks < 512)
                        l1_blockstack[l1_nblocks++] = b;
                }

                /* mark name token */
                for (i = 0; i < d->toks.len; i++) {
                    L1Tok *t = &d->toks.data[i];
                    if (t->line == li && t->kind == TK_WORD &&
                        t->text && strcmp(t->text, name) == 0) {
                        t->flags |= TF_DECL_FUNC;
                        d->funcs.data[d->funcs.len - 1].col = t->start;
                        d->funcs.data[d->funcs.len - 1].end_col =
                            t->start + t->len;
                        break;
                    }
                }

                /* check #var directive on the next content line */
                {
                    int nx = l1_next_content_line(d, li);
                    if (nx != -1) {
                        char nb[2048];
                        char nc[2048];
                        strip_comment(d->lines[nx].text, nc, sizeof(nc));
                        snprintf(nb, sizeof(nb), "%s", nc);
                        if (strncmp(l1_trim(nb), "#var", 4) != 0) {
                            char msg[512];
                            snprintf(msg, sizeof(msg),
                                     "function '%.150s' is missing the "
                                     "#var ~ %.150s directive", name, name);
                            diag_line(d, L1SEV_WARNING, msg, li, NULL);
                        }
                    }
                }
            } else if (strcmp(kw, "object") == 0 && name[0] != ':') {
                L1Obj o;
                memset(&o, 0, sizeof(o));
                o.name = strdup(name);
                o.line = li;
                o.end_line = -1;
                VEC_PUSH(d->objs, o, L1Obj);

                {
                    L1Block b;
                    memset(&b, 0, sizeof(b));
                    snprintf(b.name, sizeof(b.name), "%.255s", name);
                    b.parent[0] = '\0';
                    b.kind = 1;
                    b.sym_index = d->objs.len - 1;
                    b.start_line = li;
                    if (l1_nblocks < 512)
                        l1_blockstack[l1_nblocks++] = b;
                }

                for (i = 0; i < d->toks.len; i++) {
                    L1Tok *t = &d->toks.data[i];
                    if (t->line == li && t->kind == TK_WORD &&
                        t->text && strcmp(t->text, name) == 0) {
                        t->flags |= TF_DECL_FUNC;   /* class marker */
                        d->objs.data[d->objs.len - 1].col = t->start;
                        d->objs.data[d->objs.len - 1].end_col =
                            t->start + t->len;
                        break;
                    }
                }
            }
        }
        /* only (name func) / (name object) are handled here; other
         * ( ... ) lines fall through to the closing-marker / label checks */
        if (kw[0] != '\0')
            return;
    }

    /* closing markers */
    if (strcmp(p, "(funcend)") == 0 || strcmp(p, "(objectend)") == 0 ||
        strcmp(p, "(func-end)") == 0) {
        if (l1_nblocks > 0) {
            L1Block *b = &l1_blockstack[l1_nblocks - 1];
            if (b->kind == 0 && b->sym_index < d->funcs.len)
                d->funcs.data[b->sym_index].end_line = li;
            if (b->kind == 1 && b->sym_index < d->objs.len)
                d->objs.data[b->sym_index].end_line = li;
            l1_nblocks--;
        }
        return;
    }

    /* label definition: (name) alone, or :name alone in ASM */
    {
        int tokcount = 0;
        int lab_idx = -1;
        for (i = 0; i < d->toks.len; i++) {
            if (d->toks.data[i].line != li)
                continue;
            tokcount++;
            if (d->toks.data[i].kind == TK_LABEL)
                lab_idx = i;
        }
        if (lab_idx != -1 && tokcount == 3) {
            L1Tok *lt = &d->toks.data[lab_idx];
            int prev = lab_idx - 1;
            int next = lab_idx + 1;
            if (prev >= 0 && d->toks.data[prev].line == li &&
                d->toks.data[prev].kind == TK_LPAREN &&
                next < d->toks.len && d->toks.data[next].line == li &&
                d->toks.data[next].kind == TK_RPAREN) {
                L1Label lab;
                memset(&lab, 0, sizeof(lab));
                lab.name = strdup(lt->text + 1);
                lab.line = li;
                lab.col = lt->start + 1;
                lab.end_col = lt->start + lt->len;
                VEC_PUSH(d->labels, lab, L1Label);
                lt->flags |= TF_DECL_LABEL;
                return;
            }
        }
        /* ASM label definition */
        if (d->inside_asm && *p == ':' && strchr(p, ' ') == NULL) {
            L1Label lab;
            int k;
            int found = 0;
            for (i = 0; i < d->toks.len; i++)
                if (d->toks.data[i].line == li &&
                    d->toks.data[i].kind == TK_LABEL) {
                    k = i;
                    found = 1;
                    break;
                }
            if (found) {
                L1Tok *lt = &d->toks.data[k];
                memset(&lab, 0, sizeof(lab));
                lab.name = strdup(lt->text + 1);
                lab.line = li;
                lab.col = lt->start + 1;
                lab.end_col = lt->start + lt->len;
                VEC_PUSH(d->labels, lab, L1Label);
                lt->flags |= TF_DECL_LABEL;
            }
            return;
        }
    }

    /* set declaration */
    {
        char *ptype = NULL, *psize = NULL, *pname = NULL;
        int nbs = 0;
        if (parse_set_decl(clean, &ptype, &psize, &pname, &nbs) == 0 &&
            l1_type_ok(ptype)) {
            L1Var v;
            int j;
            const char *scope = l1_current_scope();
            memset(&v, 0, sizeof(v));
            v.name = strdup(pname);
            v.type = strdup(ptype);
            v.size = strdup(psize);
            v.scope = strdup(scope);
            v.line = li;
            v.col = 0;
            v.end_col = 0;
            v.is_const = (strncmp(ptype, "const-", 6) == 0);
            v.is_array = (strcmp(psize, "1") != 0 && strcmp(psize, "s") != 0);
            VEC_PUSH(d->vars, v, L1Var);

            /* mark name token */
            for (j = 0; j < d->toks.len; j++) {
                L1Tok *t = &d->toks.data[j];
                if (t->line == li && t->kind == TK_WORD &&
                    t->text && strcmp(t->text, pname) == 0) {
                    t->flags |= TF_DECL_VAR;
                    d->vars.data[d->vars.len - 1].col = t->start;
                    d->vars.data[d->vars.len - 1].end_col = t->start + t->len;
                    break;
                }
            }
        }
        free(ptype);
        free(psize);
        free(pname);
    }
}

/* ==================== analyze: pass 2 usages ==================== */

static void l1_analyze_usage(L1Doc *d, int li)
{
    int i;
    const char *scope = l1_scope_at(d, li);

    for (i = 0; i < d->toks.len; i++) {
        L1Tok *t = &d->toks.data[i];
        L1Usage u;
        int len;
        if (t->line != li)
            continue;

        memset(&u, 0, sizeof(u));
        u.line = li;
        u.col = t->start;
        u.len = t->len;
        u.ref = -1;

        if (t->kind == TK_LABEL) {
            const char *nm = t->text + 1;
            char *psep = strstr(nm, "->");
            char namebuf[256];
            char objbuf[256];
            int is_method = 0;
            const L1Builtin *bf;
            L1Tok *nt = (i + 1 < d->toks.len) ? &d->toks.data[i + 1] : NULL;
            int is_call = 0;

            if (t->flags & TF_DECL_LABEL)
                continue;

            if (psep) {
                snprintf(namebuf, sizeof(namebuf), "%.*s",
                         (int)(psep - nm), nm);
                snprintf(objbuf, sizeof(objbuf), "%s", psep + 2);
                is_method = 1;
            } else {
                snprintf(namebuf, sizeof(namebuf), "%s", nm);
                objbuf[0] = '\0';
            }

            is_call = nt && nt->line == li &&
                      ((nt->kind == TK_OP &&
                        (strcmp(nt->text ? nt->text : "", "!") == 0 ||
                         strcmp(nt->text ? nt->text : "", "*") == 0)) ||
                       (nt->kind == TK_WORD && nt->text &&
                        strcmp(nt->text, "call") == 0));

            if (is_method) {
                int fidx = l1_find_method(d, namebuf, objbuf);
                u.kind = U_FUNC;
                u.name = strdup(t->text + 1);
                u.full = strdup(t->text + 1);
                if (fidx != -1) {
                    u.ref = fidx;
                }
            } else {
                bf = l1_find_builtin(namebuf);
                u.full = strdup(namebuf);
                if (bf) {
                    u.kind = U_FUNC;
                    u.name = strdup(namebuf);
                    u.is_builtin = 1;
                    t->flags |= TF_BUILTIN;
                } else {
                    int fidx = l1_find_func(d, namebuf);
                    int midx = l1_find_macro(d, namebuf);
                    int lidx = l1_find_label(d, namebuf);
                    if (fidx != -1) {
                        u.kind = U_FUNC;
                        u.name = strdup(namebuf);
                        u.ref = fidx;
                    } else if (midx != -1) {
                        u.kind = U_MACRO;
                        u.name = strdup(namebuf);
                        u.ref = midx;
                    } else if (lidx != -1) {
                        u.kind = U_LABEL;
                        u.name = strdup(namebuf);
                        u.ref = lidx;
                    } else {
                        u.kind = U_FUNC;
                        u.name = strdup(namebuf);
                        u.is_call = 0;
                        /* unresolved: record is_call to pick severity later */
                        u.is_call = is_call ? 1 : 0;
                    }
                }
            }
            (void)len;
            VEC_PUSH(d->usages, u, L1Usage);
            continue;
        }

        if (t->kind == TK_WORD && t->text) {
            const char *w = t->text;
            int wlen = (int)strlen(w);

            /* standalone ~ scope marker ("#var ~ name") is not a variable */
            if (wlen == 1 && w[0] == '~')
                continue;

            if (t->flags & TF_DECL_VAR) {
                u.kind = U_VAR;
                u.name = strdup(w);
                u.full = strdup(w);
                u.is_def = 1;
                u.ref = l1_find_var(d, w, scope);
                VEC_PUSH(d->usages, u, L1Usage);
                continue;
            }

            /* variable usage: ends with ~ or is a declared var */
            if (w[wlen - 1] == '~') {
                char base[512];
                int vidx;
                u.kind = U_VAR;
                u.name = strdup(w);
                u.full = strdup(w);
                vidx = l1_find_var(d, w, scope);
                if (vidx == -1 && wlen > 6 &&
                    strcmp(w + wlen - 6, "addr~") == 0) {
                    /* address pseudo variable: foo~addr~ -> foo~ */
                    snprintf(base, sizeof(base), "%.*s~", wlen - 6, w);
                    vidx = l1_find_var(d, base, scope);
                    if (vidx == -1) {
                        /* or the tilde-less global form: foo~addr~ -> foo */
                        snprintf(base, sizeof(base), "%.*s", wlen - 6, w);
                        vidx = l1_find_var(d, base, scope);
                    }
                } else if (vidx == -1) {
                    /* global declared without ~ suffix (e.g. zero) */
                    snprintf(base, sizeof(base), "%.*s", wlen - 1, w);
                    vidx = l1_find_var(d, base, scope);
                }
                if (vidx != -1)
                    u.ref = vidx;
                VEC_PUSH(d->usages, u, L1Usage);
                continue;
            }

            /* declared constant without ~ (e.g. zero, or lib constants) */
            {
                int vidx = l1_find_var(d, w, scope);
                if (vidx != -1) {
                    u.kind = U_VAR;
                    u.name = strdup(w);
                    u.full = strdup(w);
                    u.ref = vidx;
                    VEC_PUSH(d->usages, u, L1Usage);
                    continue;
                }
            }
        }
    }
}

/* ==================== diagnostics ==================== */

static int l1_count_parens(const char *s, int *empty_bracket)
{
    int open = 0, close = 0, i, in_str = 0;
    int prev_nonspace = 0;
    int prev_char = 0;
    *empty_bracket = 0;
    for (i = 0; s[i]; i++) {
        char c = s[i];
        if (in_str) {
            if (c == '"')
                in_str = 0;
            prev_char = c;
            continue;
        }
        if (c == '"') {
            in_str = 1;
            prev_char = c;
            continue;
        }
        if (c == '/' && s[i + 1] == '/')
            break;
        if (c == '(') {
            open++;
            prev_nonspace = 1;
            prev_char = c;
        } else if (c == ')') {
            close++;
            if (prev_nonspace && (prev_char == '(' || prev_char == 0))
                *empty_bracket = 1;
            prev_char = c;
        } else if (c == ' ' || c == '\t') {
            prev_nonspace = 0;
            prev_char = c;
        } else {
            prev_nonspace = 1;
            prev_char = c;
        }
    }
    return open - close;
}

static int l1_count_braces(const char *s)
{
    int b = 0, i, in_str = 0;
    for (i = 0; s[i]; i++) {
        char c = s[i];
        if (in_str) {
            if (c == '"')
                in_str = 0;
            continue;
        }
        if (c == '"') {
            in_str = 1;
            continue;
        }
        if (c == '/' && s[i + 1] == '/')
            break;
        if (c == '{')
            b++;
        else if (c == '}')
            b--;
    }
    return b;
}

static int l1_count_brackets(const char *s)
{
    int b = 0, i, in_str = 0;
    for (i = 0; s[i]; i++) {
        char c = s[i];
        if (in_str) {
            if (c == '"')
                in_str = 0;
            continue;
        }
        if (c == '"') {
            in_str = 1;
            continue;
        }
        if (c == '/' && s[i + 1] == '/')
            break;
        if (c == '[')
            b++;
        else if (c == ']')
            b--;
    }
    return b;
}

static void l1_check_numeric_range(L1Doc *d, const char *type,
                                   const char *val, int line)
{
    char *end;
    long long v;
    int bad = 0;
    if (!val || *val == '\0')
        return;
    if (*val == '&') {
        v = strtoll(val + 1, &end, 16);
        if (*end != '\0' && *end != ')' && *end != ' ')
            return;
    } else if (strncmp(val, "0x", 2) == 0) {
        v = strtoll(val + 2, &end, 16);
        if (*end != '\0' && *end != ')' && *end != ' ')
            return;
    } else {
        v = strtoll(val, &end, 10);
        if (*end != '\0' && *end != ')' && *end != ' ')
            return;
    }
    (void)bad;
    if (strcmp(type, "byte") == 0 || strcmp(type, "const-byte") == 0 ||
        strcmp(type, "mut-byte") == 0) {
        if (v < 0 || v > 255) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "byte value %lld out of range (0-255)", v);
            diag_line(d, L1SEV_ERROR, msg, line, NULL);
        }
    } else if (strcmp(type, "bool") == 0 || strcmp(type, "const-bool") == 0 ||
               strcmp(type, "mut-bool") == 0) {
        if (v != 0 && v != 1) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "boolean value must be 0 (false) or 1 (true)");
            diag_line(d, L1SEV_ERROR, msg, line, NULL);
        }
    } else if (strcmp(type, "int16") == 0 ||
               strcmp(type, "const-int16") == 0 ||
               strcmp(type, "mut-int16") == 0) {
        if (v < -32768 || v > 32767) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "int16 value %lld out of range (-32768-32767)", v);
            diag_line(d, L1SEV_ERROR, msg, line, NULL);
        }
    } else if (strcmp(type, "int32") == 0 ||
               strcmp(type, "const-int32") == 0 ||
               strcmp(type, "mut-int32") == 0) {
        if (v < -2147483648LL || v > 2147483647LL) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "int32 value %lld out of range", v);
            diag_line(d, L1SEV_ERROR, msg, line, NULL);
        }
    }
}

static void l1_static_diags(L1Doc *d);

void l1_doc_diagnostics(L1Doc *d)
{
    int i;

    if (d->compiler_diags)
        return;

    /* clear */
    for (i = 0; i < d->diags.len; i++) {
        free(d->diags.data[i].msg);
        free(d->diags.data[i].source);
    }
    d->diags.len = 0;

    if (!l1_settings.static_diag)
        return;

    l1_static_diags(d);
}

/* compare a token against a literal; for operator tokens (text == NULL)
 * the raw source bytes are compared */
static int l1_tok_is(const L1Doc *d, const L1Tok *t, int kind,
                     const char *txt)
{
    size_t n;
    if (t->kind != kind)
        return 0;
    if (t->text)
        return strcmp(t->text, txt) == 0;
    n = strlen(txt);
    if ((size_t)t->blen != n)
        return 0;
    return strncmp(d->lines[t->line].text + t->bstart, txt, n) == 0;
}

/* true when the token at idx is preceded by "-" and a word, i.e. it is the
 * tail of a hyphenated directive such as "optimize-if" / "optimize-if-off" */
static int l1_tok_preceded_by_dash(const L1Doc *d, const L1Tok *toks,
                                   int idx)
{
    if (idx < 2)
        return 0;
    if (toks[idx - 1].kind != TK_OP || toks[idx - 1].blen != 1)
        return 0;
    if (d->lines[toks[idx - 1].line].text[toks[idx - 1].bstart] != '-')
        return 0;
    return toks[idx - 2].kind == TK_WORD;
}

/* built-in static analysis, pushes into d->diags */
static void l1_static_diags(L1Doc *d)
{
    int i;
    char msg[512];

    /* per line structural checks */
    for (i = 0; i < d->nlines; i++) {
        const char *s = d->lines[i].text;
        char buf[2048];
        int empty_bracket = 0;
        int diff;

        strip_comment(s, buf, sizeof(buf));

        diff = l1_count_parens(buf, &empty_bracket);
        if (diff != 0) {
            char m[256];
            snprintf(m, sizeof(m), "%s parenthesis",
                     diff > 0 ? "unclosed" : "too many closing");
            diag_add(d, L1SEV_ERROR, m, i, 0, i,
                     l1_utf8_to_utf16(buf, (int)strlen(buf)), NULL);
        }
        if (empty_bracket) {
            diag_add(d, L1SEV_ERROR, "empty brackets () not allowed", i, 0,
                     i, l1_utf8_to_utf16(buf, (int)strlen(buf)), NULL);
        }
        if (l1_count_braces(buf) != 0) {
            diag_add(d, L1SEV_ERROR, "unbalanced { } braces", i, 0, i,
                     l1_utf8_to_utf16(buf, (int)strlen(buf)), NULL);
        }
        if (l1_count_brackets(buf) != 0) {
            diag_add(d, L1SEV_WARNING, "unbalanced [ ] brackets", i, 0, i,
                     l1_utf8_to_utf16(buf, (int)strlen(buf)), NULL);
        }
    }

    /* duplicate functions */
    for (i = 0; i < d->funcs.len; i++) {
        int j;
        for (j = i + 1; j < d->funcs.len; j++) {
            if (strcmp(d->funcs.data[i].name, d->funcs.data[j].name) == 0 &&
                ((!d->funcs.data[i].parent && !d->funcs.data[j].parent))) {
                snprintf(msg, sizeof(msg),
                         "function '%s' already defined", d->funcs.data[j].name);
                diag_line(d, L1SEV_ERROR, msg, d->funcs.data[j].line, NULL);
            }
        }
    }

    /* duplicate objects */
    for (i = 0; i < d->objs.len; i++) {
        int j;
        for (j = i + 1; j < d->objs.len; j++) {
            if (strcmp(d->objs.data[i].name, d->objs.data[j].name) == 0) {
                snprintf(msg, sizeof(msg),
                         "object '%s' already defined", d->objs.data[j].name);
                diag_line(d, L1SEV_ERROR, msg, d->objs.data[j].line, NULL);
            }
        }
    }

    /* duplicate labels */
    for (i = 0; i < d->labels.len; i++) {
        int j;
        for (j = i + 1; j < d->labels.len; j++) {
            if (strcmp(d->labels.data[i].name, d->labels.data[j].name) == 0) {
                snprintf(msg, sizeof(msg),
                         "label '%s' already defined", d->labels.data[j].name);
                diag_line(d, L1SEV_ERROR, msg, d->labels.data[j].line, NULL);
            }
        }
    }

    /* duplicate variables in the same scope */
    for (i = 0; i < d->vars.len; i++) {
        int j;
        for (j = i + 1; j < d->vars.len; j++) {
            if (strcmp(d->vars.data[i].name, d->vars.data[j].name) == 0 &&
                strcmp(d->vars.data[i].scope, d->vars.data[j].scope) == 0) {
                snprintf(msg, sizeof(msg),
                         "variable '%s' already defined",
                         d->vars.data[j].name);
                diag_line(d, L1SEV_ERROR, msg, d->vars.data[j].line, NULL);
            }
        }
    }

    /* unclosed blocks */
    for (i = 0; i < d->funcs.len; i++) {
        if (d->funcs.data[i].end_line == -1) {
            snprintf(msg, sizeof(msg),
                     "function '%s' is missing (funcend)",
                     d->funcs.data[i].name);
            diag_line(d, L1SEV_ERROR, msg, d->funcs.data[i].line, NULL);
        }
    }
    for (i = 0; i < d->objs.len; i++) {
        if (d->objs.data[i].end_line == -1) {
            snprintf(msg, sizeof(msg),
                     "object '%s' is missing (objectend)",
                     d->objs.data[i].name);
            diag_line(d, L1SEV_ERROR, msg, d->objs.data[i].line, NULL);
        }
    }

    /* control-flow block matching: for/next, if/if+/else/endif,
     * do/while, switch/switchend */
    {
        struct { int line; int is_ifplus; int has_else; } ifstk[256];
        int ifsp = 0;
        int forstk[256];
        int forsp = 0;
        int dostk[256];
        int dosp = 0;
        int swstk[256];
        int swsp = 0;
        for (i = 0; i < d->toks.len; i++) {
            L1Tok *t = &d->toks.data[i];
            if (t->kind != TK_WORD || !t->text)
                continue;
            if (strcmp(t->text, "for") == 0) {
                if (l1_tok_preceded_by_dash(d, d->toks.data, i))
                    continue;
                /* the word "for" inside a "(for-loop)" marker is not a
                 * separate loop: the following "(((cond) f =) f for)"
                 * expression is the actual loop start */
                {
                    char lc[2048];
                    strip_comment(d->lines[t->line].text, lc, sizeof(lc));
                    if (strstr(lc, "(for-loop)") != NULL)
                        continue;
                }
                if (forsp < 256)
                    forstk[forsp++] = t->line;
            } else if (strcmp(t->text, "next") == 0) {
                if (forsp > 0)
                    forsp--;
            } else if (strcmp(t->text, "if") == 0) {
                /* "if+" tokenizes as the word "if" followed by "+" */
                int is_ifplus;
                if (l1_tok_preceded_by_dash(d, d->toks.data, i))
                    continue;
                is_ifplus =
                    (i + 1 < d->toks.len &&
                     l1_tok_is(d, &d->toks.data[i + 1], TK_OP, "+"));
                if (is_ifplus)
                    i++;
                if (ifsp < 256) {
                    ifstk[ifsp].line = t->line;
                    ifstk[ifsp].is_ifplus = is_ifplus;
                    ifstk[ifsp].has_else = 0;
                    ifsp++;
                }
            } else if (strcmp(t->text, "else") == 0) {
                if (ifsp > 0)
                    ifstk[ifsp - 1].has_else = 1;
            } else if (strcmp(t->text, "endif") == 0) {
                if (ifsp > 0) {
                    ifsp--;
                    if (ifstk[ifsp].is_ifplus && !ifstk[ifsp].has_else) {
                        snprintf(msg, sizeof(msg), "if+ without else");
                        diag_line(d, L1SEV_ERROR, msg, ifstk[ifsp].line, NULL);
                    }
                }
            } else if (strcmp(t->text, "do") == 0) {
                if (l1_tok_preceded_by_dash(d, d->toks.data, i))
                    continue;
                if (dosp < 256)
                    dostk[dosp++] = t->line;
            } else if (strcmp(t->text, "while") == 0) {
                if (l1_tok_preceded_by_dash(d, d->toks.data, i))
                    continue;
                if (dosp > 0)
                    dosp--;
            } else if (strcmp(t->text, "switch") == 0) {
                /* "(switch-end)" tokenizes as "switch" "-" "end" */
                int is_end;
                if (l1_tok_preceded_by_dash(d, d->toks.data, i))
                    continue;
                is_end =
                    (i + 2 < d->toks.len &&
                     l1_tok_is(d, &d->toks.data[i + 1], TK_OP, "-") &&
                     d->toks.data[i + 2].kind == TK_WORD &&
                     strcmp(d->toks.data[i + 2].text, "end") == 0);
                if (is_end) {
                    if (swsp > 0)
                        swsp--;
                    i += 2;
                } else if (swsp < 256) {
                    swstk[swsp++] = t->line;
                }
            } else if (strcmp(t->text, "switchend") == 0) {
                if (swsp > 0)
                    swsp--;
            }
        }
        while (forsp > 0) {
            snprintf(msg, sizeof(msg), "for without next");
            diag_line(d, L1SEV_ERROR, msg, forstk[--forsp], NULL);
        }
        while (dosp > 0) {
            snprintf(msg, sizeof(msg), "do without while");
            diag_line(d, L1SEV_ERROR, msg, dostk[--dosp], NULL);
        }
        while (swsp > 0) {
            snprintf(msg, sizeof(msg), "switch without switchend");
            diag_line(d, L1SEV_ERROR, msg, swstk[--swsp], NULL);
        }
        while (ifsp > 0) {
            ifsp--;
            if (ifstk[ifsp].is_ifplus) {
                if (!ifstk[ifsp].has_else)
                    snprintf(msg, sizeof(msg), "if+ without else and endif");
                else
                    snprintf(msg, sizeof(msg), "if+ without endif");
            } else {
                snprintf(msg, sizeof(msg), "if without endif");
            }
            diag_line(d, L1SEV_ERROR, msg, ifstk[ifsp].line, NULL);
        }
    }

    /* precondition / postcondition contract blocks: each (precondition)
     * must have a matching (precondition-end), and likewise for
     * (postcondition)/(postcondition-end), with actual code between */
    {
        int prestk[256];
        int presp = 0;
        int poststk[256];
        int postsp = 0;
        for (i = 0; i < d->toks.len; i++) {
            L1Tok *t = &d->toks.data[i];
            int is_end;
            if (t->kind != TK_WORD || !t->text)
                continue;
            if (strcmp(t->text, "precondition") == 0) {
                /* "(precondition-end)" tokenizes as "precondition" "-"
                 * "end" */
                if (l1_tok_preceded_by_dash(d, d->toks.data, i))
                    continue;
                is_end =
                    (i + 2 < d->toks.len &&
                     l1_tok_is(d, &d->toks.data[i + 1], TK_OP, "-") &&
                     d->toks.data[i + 2].kind == TK_WORD &&
                     strcmp(d->toks.data[i + 2].text, "end") == 0);
                if (is_end) {
                    if (presp > 0) {
                        int start = prestk[--presp];
                        int li;
                        int has_code = 0;
                        for (li = start + 1; li < t->line; li++)
                            if (l1_is_content_line(d, li)) {
                                has_code = 1;
                                break;
                            }
                        if (!has_code) {
                            snprintf(msg, sizeof(msg),
                                     "empty precondition block: code required "
                                     "between (precondition) and "
                                     "(precondition-end)");
                            diag_line(d, L1SEV_ERROR, msg, start, NULL);
                        }
                    }
                    i += 2;
                } else if (presp < 256) {
                    prestk[presp++] = t->line;
                }
            } else if (strcmp(t->text, "postcondition") == 0) {
                if (l1_tok_preceded_by_dash(d, d->toks.data, i))
                    continue;
                is_end =
                    (i + 2 < d->toks.len &&
                     l1_tok_is(d, &d->toks.data[i + 1], TK_OP, "-") &&
                     d->toks.data[i + 2].kind == TK_WORD &&
                     strcmp(d->toks.data[i + 2].text, "end") == 0);
                if (is_end) {
                    if (postsp > 0) {
                        int start = poststk[--postsp];
                        int li;
                        int has_code = 0;
                        for (li = start + 1; li < t->line; li++)
                            if (l1_is_content_line(d, li)) {
                                has_code = 1;
                                break;
                            }
                        if (!has_code) {
                            snprintf(msg, sizeof(msg),
                                     "empty postcondition block: code required "
                                     "between (postcondition) and "
                                     "(postcondition-end)");
                            diag_line(d, L1SEV_ERROR, msg, start, NULL);
                        }
                    }
                    i += 2;
                } else if (postsp < 256) {
                    poststk[postsp++] = t->line;
                }
            }
        }
        while (presp > 0) {
            snprintf(msg, sizeof(msg), "precondition without precondition-end");
            diag_line(d, L1SEV_ERROR, msg, prestk[--presp], NULL);
        }
        while (postsp > 0) {
            snprintf(msg, sizeof(msg),
                     "postcondition without postcondition-end");
            diag_line(d, L1SEV_ERROR, msg, poststk[--postsp], NULL);
        }
    }

    /* missing main */
    if (!d->has_main && !d->is_header && d->nlines > 0) {
        int has_content = 0;
        for (i = 0; i < d->nlines; i++)
            if (l1_is_content_line(d, i)) {
                has_content = 1;
                break;
            }
        if (has_content) {
            diag_add(d, L1SEV_ERROR,
                     "missing (main func) definition - the main function "
                     "is the required entry point",
                     0, 0, 0,
                     l1_utf8_to_utf16(d->lines[0].text,
                                      d->lines[0].nbytes),
                     NULL);
        }
    }

    /* set declarations checks */
    for (i = 0; i < d->vars.len; i++) {
        const L1Var *v = &d->vars.data[i];
        size_t nl = strlen(v->name);
        char valbuf[2048];
        char clean[2048];

        /* missing ~ suffix hint */
        if (l1_settings.missing_tilde_hint && nl > 0 &&
            v->name[nl - 1] != '~' && strcmp(v->name, "zero") != 0 &&
            strcmp(v->scope, "lib") != 0) {
            snprintf(msg, sizeof(msg),
                     "variable '%s' is declared without the '~' suffix",
                     v->name);
            diag_line(d, L1SEV_HINT, msg, v->line, NULL);
        }

        /* bool variables must begin with 'B' */
        if ((strcmp(v->type, "bool") == 0 ||
             strcmp(v->type, "const-bool") == 0 ||
             strcmp(v->type, "mut-bool") == 0) && v->name[0] != 'B') {
            snprintf(msg, sizeof(msg),
                     "boolean variable '%s' must begin with 'B'", v->name);
            diag_line(d, L1SEV_WARNING, msg, v->line, NULL);
        }

        /* numeric range */
        if (v->line >= 0 && v->line < d->nlines) {
            strip_comment(d->lines[v->line].text, clean, sizeof(clean));
            snprintf(valbuf, sizeof(valbuf), "%s", clean);
            {
                const char *pp = l1_trim(valbuf);
                /* skip: set type size name */
                pp = strstr(pp, "set");
                if (pp) {
                    char *t1;
                    int k;
                    for (k = 0; k < 4; k++) {
                        t1 = next_tok(&pp);
                        free(t1);
                    }
                    t1 = next_tok(&pp);
                    l1_check_numeric_range(d, v->type, t1, v->line);
                    free(t1);
                }
            }
        }
    }

    /* usages */
    for (i = 0; i < d->usages.len; i++) {
        const L1Usage *u = &d->usages.data[i];
        int sev;
        char *src = NULL;

        if (u->ref != -1 || u->is_builtin)
            continue;

        if (u->kind == U_VAR) {
            snprintf(msg, sizeof(msg), "unknown variable '%s'", u->full);
            sev = L1SEV_WARNING;
        } else if (u->kind == U_FUNC) {
            if (u->is_call)
                snprintf(msg, sizeof(msg),
                         "undefined function or label '%s'", u->name);
            else
                snprintf(msg, sizeof(msg),
                         "undefined label '%s'", u->name);
            sev = u->is_call ? L1SEV_ERROR : L1SEV_WARNING;
        } else if (u->kind == U_LABEL) {
            snprintf(msg, sizeof(msg), "undefined label '%s'", u->name);
            sev = L1SEV_WARNING;
        } else {
            continue;
        }
        (void)sev;
        (void)src;
        diag_add(d, sev, msg, u->line, u->col, u->line, u->col + u->len, NULL);
    }
}

/* ==================== analyze driver ==================== */

static void l1_vec_clear_all(L1Doc *d)
{
    int i;
    for (i = 0; i < d->vars.len; i++) {
        free(d->vars.data[i].name);
        free(d->vars.data[i].type);
        free(d->vars.data[i].size);
        free(d->vars.data[i].scope);
    }
    d->vars.len = 0;
    for (i = 0; i < d->funcs.len; i++) {
        free(d->funcs.data[i].name);
        free(d->funcs.data[i].parent);
        free(d->funcs.data[i].signature);
        free(d->funcs.data[i].ret_sig);
    }
    d->funcs.len = 0;
    for (i = 0; i < d->objs.len; i++) {
        free(d->objs.data[i].name);
    }
    d->objs.len = 0;
    for (i = 0; i < d->labels.len; i++)
        free(d->labels.data[i].name);
    d->labels.len = 0;
    for (i = 0; i < d->usages.len; i++) {
        free(d->usages.data[i].name);
        free(d->usages.data[i].full);
    }
    d->usages.len = 0;
    for (i = 0; i < d->includes.len; i++) {
        free(d->includes.data[i].name);
        free(d->includes.data[i].file);
    }
    d->includes.len = 0;
    for (i = 0; i < d->macros.len; i++) {
        free(d->macros.data[i].name);
        free(d->macros.data[i].args);
    }
    d->macros.len = 0;
    for (i = 0; i < d->toks.len; i++)
        free(d->toks.data[i].text);
    d->toks.len = 0;
}

/* signature comments: "// (func args name int64 int64)" */
static void l1_parse_signature_comments(L1Doc *d)
{
    int i, j;
    for (i = 0; i < d->nlines; i++) {
        const char *s = d->lines[i].text;
        const char *p = strstr(s, "//");
        char nbuf[256];
        char kw[32];
        if (!p)
            continue;
        p += 2;
        while (*p == ' ')
            p++;
        if (*p != '(')
            continue;
        p++;
        if (sscanf(p, "%31s %255s", kw, nbuf) != 2)
            continue;
        if (strcmp(kw, "func") == 0 || strcmp(kw, "return") == 0) {
            for (j = 0; j < d->funcs.len; j++) {
                if (strcmp(d->funcs.data[j].name, nbuf) == 0) {
                    if (strcmp(kw, "func") == 0 && !d->funcs.data[j].signature)
                        d->funcs.data[j].signature =
                            strdup(s + (int)(p - s) + (int)strlen(kw) + 1);
                    else if (strcmp(kw, "return") == 0 &&
                             !d->funcs.data[j].ret_sig)
                        d->funcs.data[j].ret_sig =
                            strdup(s + (int)(p - s) + (int)strlen(kw) + 1);
                    break;
                }
            }
        }
    }
}

/* ==================== library include files ==================== */

static void l1_load_includes(L1Doc *d);

void l1_doc_analyze(L1Doc *d)
{
    int li;
    int i;

    if (!d->text)
        return;

    l1_vec_clear_all(d);
    l1_nblocks = 0;
    d->has_main = 0;
    d->inside_asm = 0;
    d->compiler_diags = 0;
    d->parse_error = 0;

    /* tokenize */
    for (li = 0; li < d->nlines; li++)
        l1_tokenize_line(d, li);

    /* pass 1: structure + declarations */
    for (li = 0; li < d->nlines; li++)
        l1_analyze_line(d, li);

    /* signature comments */
    l1_parse_signature_comments(d);

    /* pass 2: usages */
    for (li = 0; li < d->nlines; li++)
        l1_analyze_usage(d, li);

    /* diagnostics */
    l1_doc_diagnostics(d);
    (void)i;

    /* re-apply library include symbols (uses the persistent lib cache) */
    l1_load_includes(d);
}

/* ==================== library include files ==================== */

typedef struct LibCache LibCache;
struct LibCache {
    char *path;
    L1Vec_L1Var vars;
    L1Vec_L1Macro macros;
    L1Vec_L1Label labels;
    LibCache *next;
};

static LibCache *l1_lib_cache = NULL;

static LibCache *l1_lib_cache_find(const char *path)
{
    LibCache *c;
    for (c = l1_lib_cache; c; c = c->next)
        if (strcmp(c->path, path) == 0)
            return c;
    return NULL;
}

static void l1_parse_lib_vars(const char *path, L1Vec_L1Var *vars,
                              L1Vec_L1Macro *macros, L1Vec_L1Label *labels)
{
    FILE *f = fopen(path, "r");
    char line[4096];
    int li = 0;
    if (!f)
        return;
    while (fgets(line, sizeof(line), f)) {
        char clean[4096];
        char buf[4096];
        char *p;
        li++;
        strip_comment(line, clean, sizeof(clean));
        snprintf(buf, sizeof(buf), "%s", clean);
        p = l1_trim(buf);
        if (*p == '\0')
            continue;
        if (*p == '#') {
            if (strncmp(p, "#func", 5) == 0) {
                char *nm = p + 5;
                char *args = NULL;
                char *paren = strchr(nm, '(');
                L1Macro m;
                while (*nm == ' ' || *nm == '\t')
                    nm++;
                if (paren) {
                    char *e = strchr(paren, ')');
                    if (e) {
                        args = strndup(paren + 1, (size_t)(e - paren - 1));
                        *paren = '\0';
                    }
                }
                memset(&m, 0, sizeof(m));
                m.name = strdup(nm);
                m.args = args ? args : strdup("");
                m.line = li - 1;
                m.col = 0;
                VEC_PUSH(*macros, m, L1Macro);
            } else if (strncmp(p, "#define", 7) == 0) {
                char *nm = p + 7;
                char *sp;
                L1Macro m;
                while (*nm == ' ' || *nm == '\t')
                    nm++;
                sp = nm;
                while (*sp && *sp != ' ' && *sp != '\t')
                    sp++;
                memset(&m, 0, sizeof(m));
                m.name = strndup(nm, (size_t)(sp - nm));
                m.args = strdup("");
                m.line = li - 1;
                m.col = 0;
                VEC_PUSH(*macros, m, L1Macro);
            }
            continue;
        }
        /* label def */
        if (p[0] == '(' && p[1] == ':') {
            char *e = strchr(p, ')');
            if (e && e == p + strlen(p) - 1) {
                L1Label lab;
                memset(&lab, 0, sizeof(lab));
                lab.name = strndup(p + 2, (size_t)(e - p - 2));
                lab.line = li - 1;
                lab.col = 1;
                lab.end_col = 1 + (int)strlen(lab.name);
                VEC_PUSH(*labels, lab, L1Label);
            }
            continue;
        }
        /* set decl */
        {
            char *ptype = NULL, *psize = NULL, *pname = NULL;
            int nbs = 0;
            if (parse_set_decl(clean, &ptype, &psize, &pname, &nbs) == 0 &&
                l1_type_ok(ptype)) {
                L1Var v;
                memset(&v, 0, sizeof(v));
                v.name = strdup(pname);
                v.type = strdup(ptype);
                v.size = strdup(psize);
                v.scope = strdup("lib");
                v.line = li - 1;
                v.col = 0;
                v.end_col = 0;
                v.is_const = (strncmp(ptype, "const-", 6) == 0);
                v.is_array = (strcmp(psize, "1") != 0 &&
                              strcmp(psize, "s") != 0);
                VEC_PUSH(*vars, v, L1Var);
            }
            free(ptype);
            free(psize);
            free(pname);
        }
    }
    fclose(f);
}

static char *l1_search_lib(const L1Doc *d, const char *name)
{
    char buf[4096];
    const char *dirs[16];
    int ndirs = 0;
    int i;
    const char *env;

    /* dir of document */
    if (d->path) {
        char *slash = strrchr(d->path, '/');
        if (slash) {
            snprintf(buf, sizeof(buf), "%.*s/%s",
                     (int)(slash - d->path), d->path, name);
            if (access(buf, R_OK) == 0)
                return strdup(buf);
        }
    }
    /* configured include dirs */
    if (l1_settings.include_dirs) {
        char *copy = strdup(l1_settings.include_dirs);
        char *tok = strtok(copy, "\n");
        while (tok && ndirs < 16) {
            dirs[ndirs++] = tok;
            tok = strtok(NULL, "\n");
        }
        for (i = 0; i < ndirs; i++) {
            snprintf(buf, sizeof(buf), "%s/%s", dirs[i], name);
            if (access(buf, R_OK) == 0) {
                free(copy);
                return strdup(buf);
            }
        }
        free(copy);
    }
    /* L1VM_INCLUDE_DIRS env var */
    env = getenv("L1VM_INCLUDE_DIRS");
    if (env) {
        char *copy = strdup(env);
        char *tok = strtok(copy, ":");
        while (tok && ndirs < 16) {
            dirs[ndirs++] = tok;
            tok = strtok(NULL, ":");
        }
        for (i = 0; i < ndirs; i++) {
            snprintf(buf, sizeof(buf), "%s/%s", dirs[i], name);
            if (access(buf, R_OK) == 0) {
                free(copy);
                return strdup(buf);
            }
        }
        free(copy);
    }
    return NULL;
}

static void l1_load_includes(L1Doc *d)
{
    int i;
    for (i = 0; i < d->includes.len; i++) {
        const char *name = d->includes.data[i].name;
        char *path;
        LibCache *c;
        int j;
        if (!name || *name == '\0')
            continue;
        path = l1_search_lib(d, name);
        if (!path)
            continue;
        c = l1_lib_cache_find(path);
        if (!c) {
            c = calloc(1, sizeof(LibCache));
            if (!c) {
                free(path);
                continue;
            }
            c->path = path;
            l1_parse_lib_vars(path, &c->vars, &c->macros, &c->labels);
            c->next = l1_lib_cache;
            l1_lib_cache = c;
        } else {
            free(path);
        }
        for (j = 0; j < c->vars.len; j++) {
            L1Var nv;
            memset(&nv, 0, sizeof(nv));
            nv.name = strdup(c->vars.data[j].name);
            nv.type = strdup(c->vars.data[j].type);
            nv.size = strdup(c->vars.data[j].size);
            nv.scope = strdup(c->vars.data[j].scope);
            nv.line = c->vars.data[j].line;
            nv.col = c->vars.data[j].col;
            nv.end_col = c->vars.data[j].end_col;
            nv.is_const = c->vars.data[j].is_const;
            nv.is_array = c->vars.data[j].is_array;
            VEC_PUSH(d->vars, nv, L1Var);
        }
        for (j = 0; j < c->macros.len; j++) {
            L1Macro nm;
            memset(&nm, 0, sizeof(nm));
            nm.name = strdup(c->macros.data[j].name);
            nm.args = strdup(c->macros.data[j].args);
            nm.line = c->macros.data[j].line;
            nm.col = c->macros.data[j].col;
            VEC_PUSH(d->macros, nm, L1Macro);
        }
        for (j = 0; j < c->labels.len; j++) {
            L1Label nl;
            memset(&nl, 0, sizeof(nl));
            nl.name = strdup(c->labels.data[j].name);
            nl.line = c->labels.data[j].line;
            nl.col = c->labels.data[j].col;
            nl.end_col = c->labels.data[j].end_col;
            VEC_PUSH(d->labels, nl, L1Label);
        }
    }
}

/* ==================== l1com integration ==================== */

static char *l1_file_dir(const char *path)
{
    char *slash = strrchr(path, '/');
    if (!slash)
        return strdup(".");
    if (slash == path)
        return strdup("/");
    return strndup(path, (size_t)(slash - path));
}

static void l1_strip_ansi(const char *in, char *out, size_t outsz)
{
    size_t o = 0;
    while (*in && o + 1 < outsz) {
        if (*in == '\033' && in[1] == '[') {
            in += 2;
            while (*in && !((*in >= 'A' && *in <= 'Z') ||
                            (*in >= 'a' && *in <= 'z')))
                in++;
            if (*in)
                in++;
        } else {
            out[o++] = *in++;
        }
    }
    out[o] = '\0';
}

static void l1_shell_escape(char *out, size_t outsz, const char *in)
{
    size_t o = 0;
    const char *p;
    for (p = in; *p && o + 1 < outsz; p++) {
        if (*p == '\'') {
            if (o + 4 >= outsz)
                break;
            out[o++] = '\'';
            out[o++] = '\\';
            out[o++] = '\'';
            out[o++] = '\'';
        } else {
            out[o++] = *p;
        }
    }
    out[o] = '\0';
}

/* detect l1pre-only directives (#include, #define, #func) in the buffer */
static int l1_has_l1pre_directives(const char *text)
{
    const char *p = text;
    if (!text)
        return 0;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        char buf[2048];
        char *t;
        size_t n = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
        memcpy(buf, p, n);
        buf[n] = '\0';
        t = l1_trim(buf);
        if (*t == '#' &&
            (strncmp(t, "#include", 8) == 0 ||
             strncmp(t, "#define", 7) == 0 ||
             strncmp(t, "#func", 5) == 0))
            return 1;
        p += len;
        if (*p)
            p++;
    }
    return 0;
}

/* normalized line content used to align source lines onto the expanded file */
static void l1_line_signature(const char *text, char *out, size_t outsz)
{
    char buf[2048];
    size_t o = 0;
    int i;
    int last_ws = 0;
    strip_comment(text, buf, sizeof(buf));
    for (i = 0; buf[i] && o + 1 < outsz; i++) {
        char c = buf[i];
        if (c == ' ' || c == '\t') {
            if (!last_ws && o > 0) {
                out[o++] = ' ';
                last_ws = 1;
            }
        } else {
            out[o++] = c;
            last_ws = 0;
        }
    }
    while (o > 0 && out[o - 1] == ' ')
        o--;
    out[o] = '\0';
}

/* best-effort map from l1com line numbers on the l1pre-expanded file back
 * to original source lines. Returns array indexed by (l1com_line - 1) with
 * source line numbers (0-based), or NULL. */
static int *l1_build_expand_map(const L1Doc *d, const char *exp_text,
                                int *map_sz)
{
    char **exps = NULL;
    int *lnum = NULL;
    int *src_idx = NULL;
    int *map = NULL;
    int nexp = 0, i, j, k, maxl = 0;

    *map_sz = 0;
    if (!exp_text || !*exp_text || d->nlines <= 0)
        return NULL;
    {
        SB b;
        const char *p = exp_text;
        sb_init(&b);
        while (*p) {
            sb_reset(&b);
            while (*p && *p != '\n')
                sb_addc(&b, *p++);
            if (*p)
                p++;
            exps = realloc(exps, (size_t)(nexp + 1) * sizeof(char *));
            if (!exps)
                goto cleanup;
            exps[nexp] = strdup(sb_cstr(&b));
            nexp++;
        }
        sb_free(&b);
    }
    if (nexp == 0)
        goto cleanup;
    lnum = calloc((size_t)nexp, sizeof(int));
    src_idx = calloc((size_t)nexp, sizeof(int));
    if (!lnum || !src_idx)
        goto cleanup;
    {
        int file_inside = 0, linenum = 0;
        for (i = 0; i < nexp; i++) {
            if (!file_inside) {
                linenum++;
                if (strncmp(exps[i], "FILE:", 5) == 0)
                    file_inside = 1;
                lnum[i] = linenum;
            } else {
                lnum[i] = -1;
                if (strncmp(exps[i], "FILE END", 8) == 0)
                    file_inside = 0;
            }
            if (linenum > maxl)
                maxl = linenum;
        }
    }
    j = 0;
    for (i = 0; i < d->nlines && j < nexp; i++) {
        char sig[2048], esig[2048];
        int found = -1;
        l1_line_signature(d->lines[i].text, sig, sizeof(sig));
        for (k = j; k < nexp && k - j < 4096; k++) {
            l1_line_signature(exps[k], esig, sizeof(esig));
            if (strcmp(esig, sig) == 0) {
                found = k;
                break;
            }
        }
        if (found != -1) {
            for (; j < found; j++)
                src_idx[j] = i > 0 ? i - 1 : i;
            src_idx[found] = i;
            j = found + 1;
        } else if (j < nexp) {
            src_idx[j] = i;
            j++;
        }
    }
    for (; j < nexp; j++)
        src_idx[j] = d->nlines - 1;
    if (maxl > 0) {
        map = calloc((size_t)maxl, sizeof(int));
        if (map) {
            *map_sz = maxl;
            for (i = 0; i < nexp; i++)
                if (lnum[i] >= 1 && lnum[i] - 1 < maxl)
                    map[lnum[i] - 1] = src_idx[i];
        }
    }
cleanup:
    free(lnum);
    free(src_idx);
    for (i = 0; i < nexp; i++)
        free(exps[i]);
    free(exps);
    return map;
}

void l1_doc_run_compiler(L1Doc *d)
{
    char cmd[65536];
    char l1com[4096];
    char esc_l1com[8192];
    char esc_dir[8192];
    char esc_path[8192];
    char tmpl[8192];
    char srcpath[8200];
    char prepath[8240];
    FILE *fp;
    char line[4096];
    SB out;
    int l1com_missing = 0;
    int used_temp = 0;
    int need_pre = 0;
    int pre_ok = 0;
    int *linemap = NULL;
    int linemap_sz = 0;
    const char *path = d->path;
    const char *src;

    if (l1_settings.l1com_enabled == L1_L1COM_OFF)
        return;
    if (!path || !d->text)
        return;
    need_pre = l1_has_l1pre_directives(d->text);

    /* the buffer may differ from the file on disk (or not be saved at
     * all): write the current text to a temp file next to the real file
     * so line numbers and relative includes stay correct */
    {
        char *dd = l1_file_dir(path);
        int tfd;
        size_t tlen = strlen(d->text);
        snprintf(tmpl, sizeof(tmpl), "%s/.l1vm-lsp-XXXXXX", dd);
        free(dd);
        tfd = mkstemp(tmpl);
        if (tfd < 0)
            return;
        {
            ssize_t w = write(tfd, d->text, tlen);
            (void)w;
        }
        close(tfd);
        snprintf(srcpath, sizeof(srcpath), "%s.l1com", tmpl);
        rename(tmpl, srcpath);
        src = srcpath;
        used_temp = 1;
    }

    snprintf(l1com, sizeof(l1com), "%s",
             l1_settings.l1com_path ? l1_settings.l1com_path : "l1com");
    l1_shell_escape(esc_l1com, sizeof(esc_l1com), l1com);
    {
        char *dd = l1_file_dir(path);
        l1_shell_escape(esc_dir, sizeof(esc_dir), dd);
        free(dd);
    }
    l1_shell_escape(esc_path, sizeof(esc_path), src);

    snprintf(cmd, sizeof(cmd), "cd '%s' && %s '%s' 2>&1",
             esc_dir, esc_l1com, esc_path);

    if (need_pre) {
        /* the buffer uses the l1pre preprocessor (#include, #define,
         * #func): l1com cannot read it directly, so expand first and
         * compile the result */
        char esc_pre[8240];
        snprintf(prepath, sizeof(prepath), "%s.pre.l1com", tmpl);
        l1_shell_escape(esc_pre, sizeof(esc_pre), prepath);
        {
            SB incs;
            const char *dirs = l1_settings.include_dirs;
            const char *envd = getenv("L1VM_INCLUDE_DIRS");
            sb_init(&incs);
            if (dirs) {
                const char *q = dirs;
                while (*q) {
                    const char *nl = strchr(q, '\n');
                    size_t n = nl ? (size_t)(nl - q) : strlen(q);
                    char b[8192];
                    size_t m = n < sizeof(b) - 2 ? n : sizeof(b) - 2;
                    size_t bl;
                    memcpy(b, q, m);
                    b[m] = '\0';
                    bl = strlen(b);
                    if (bl > 0) {
                        char eb[8192];
                        if (b[bl - 1] != '/') {
                            b[bl] = '/';
                            b[bl + 1] = '\0';
                        }
                        l1_shell_escape(eb, sizeof(eb), b);
                        sb_add(&incs, " '");
                        sb_add(&incs, eb);
                        sb_addc(&incs, '\'');
                    }
                    q += n;
                    if (*q)
                        q++;
                }
            }
            if (envd) {
                const char *q = envd;
                while (*q) {
                    const char *nl = strchr(q, ':');
                    size_t n = nl ? (size_t)(nl - q) : strlen(q);
                    char b[8192];
                    size_t m = n < sizeof(b) - 2 ? n : sizeof(b) - 2;
                    size_t bl;
                    memcpy(b, q, m);
                    b[m] = '\0';
                    bl = strlen(b);
                    if (bl > 0) {
                        char eb[8192];
                        if (b[bl - 1] != '/') {
                            b[bl] = '/';
                            b[bl + 1] = '\0';
                        }
                        l1_shell_escape(eb, sizeof(eb), b);
                        sb_add(&incs, " '");
                        sb_add(&incs, eb);
                        sb_addc(&incs, '\'');
                    }
                    q += n;
                    if (*q)
                        q++;
                }
            }
            snprintf(cmd, sizeof(cmd), "cd '%s' && l1pre '%s' '%s'%s 2>&1",
                     esc_dir, esc_path, esc_pre, sb_cstr(&incs));
            sb_free(&incs);
        }
        sb_init(&out);
        fp = popen(cmd, "r");
        if (fp) {
            while (fgets(line, sizeof(line), fp))
                sb_add(&out, line);
            {
                int st = pclose(fp);
                pre_ok = (st != -1 && WIFEXITED(st) &&
                          WEXITSTATUS(st) == 0);
            }
        } else {
            sb_free(&out);
            goto cleanup;
        }
        if (pre_ok) {
            /* build source<->expanded line map and compile the output */
            {
                FILE *ef = fopen(prepath, "r");
                if (ef) {
                    SB et;
                    char rl[4096];
                    sb_init(&et);
                    while (fgets(rl, sizeof(rl), ef))
                        sb_add(&et, rl);
                    fclose(ef);
                    linemap = l1_build_expand_map(d, sb_cstr(&et),
                                                  &linemap_sz);
                    sb_free(&et);
                }
            }
            {
                char epre[8240];
                snprintf(epre, sizeof(epre), "%s.pre", tmpl);
                l1_shell_escape(esc_path, sizeof(esc_path), epre);
            }
            sb_reset(&out);
            snprintf(cmd, sizeof(cmd), "cd '%s' && %s '%s' 2>&1",
                     esc_dir, esc_l1com, esc_path);
            fp = popen(cmd, "r");
            if (fp) {
                while (fgets(line, sizeof(line), fp))
                    sb_add(&out, line);
                pclose(fp);
            } else {
                sb_free(&out);
                goto cleanup;
            }
        } else if (strstr(sb_cstr(&out), "not found") ||
                   strstr(sb_cstr(&out), "No such file")) {
            /* l1pre binary missing: fall through like a missing l1com */
            l1com_missing = 1;
        }
    } else {
        sb_init(&out);
        fp = popen(cmd, "r");
        if (fp) {
            while (fgets(line, sizeof(line), fp))
                sb_add(&out, line);
            pclose(fp);
        } else {
            sb_free(&out);
            goto cleanup;
        }
    }

    if (strstr(sb_cstr(&out), "not found") ||
        strstr(sb_cstr(&out), "No such file"))
        l1com_missing = 1;

    if (!l1com_missing) {
        /* parse compiler output into a local vector */
        L1Vec_L1Diag comp;
        const char *buf = sb_cstr(&out);
        const char *p = buf;
        L1Diag g;
        int found = 0;

        memset(&comp, 0, sizeof(comp));
        while (*p) {
            const char *nl = strchr(p, '\n');
            int linelen = nl ? (int)(nl - p) : (int)strlen(p);
            char lb[4096];
            char lb2[4096];
            int ln;
            int is_err = 0, is_warn = 0;
            const char *msgstart;
            memcpy(lb, p, (size_t)linelen);
            lb[linelen] = '\0';
            l1_strip_ansi(lb, lb2, sizeof(lb2));

            if (sscanf(lb2, "error: line %d:", &ln) == 1 ||
                sscanf(lb2, "error: line: %d", &ln) == 1)
                is_err = 1;
            else if (sscanf(lb2, "warning: line %d:", &ln) == 1 ||
                     sscanf(lb2, "warning: line: %d", &ln) == 1)
                is_warn = 1;
            else {
                /* l1com prefixes some messages, e.g. "checkdef: error: line 7:" */
                const char *mm = strstr(lb2, "error: line");
                if (mm && (sscanf(mm, "error: line %d:", &ln) == 1 ||
                           sscanf(mm, "error: line: %d", &ln) == 1))
                    is_err = 1;
                else {
                    mm = strstr(lb2, "warning: line");
                    if (mm && (sscanf(mm, "warning: line %d:", &ln) == 1 ||
                               sscanf(mm, "warning: line: %d", &ln) == 1))
                        is_warn = 1;
                }
            }

            if (is_err || is_warn) {
                const char *mm = strstr(lb2, is_err ? "error: line"
                                                    : "warning: line");
                if (mm) {
                    const char *q;
                    const char *colon;
                    q = mm + 11;
                    while (*q == ' ' || *q == ':')
                        q++;
                    while (*q >= '0' && *q <= '9')
                        q++;
                    colon = q;
                    while (*colon == ' ' || *colon == ':')
                        colon++;
                    msgstart = (colon != q) ? colon : lb2;
                } else {
                    msgstart = lb2;
                }
                while (*msgstart == ' ')
                    msgstart++;
                memset(&g, 0, sizeof(g));
                g.sev = is_err ? L1SEV_ERROR : L1SEV_WARNING;
                g.msg = strdup(msgstart);
                g.source = strdup("l1com");
                g.line = ln > 0 && linemap && ln - 1 < linemap_sz
                             ? linemap[ln - 1] : (ln > 0 ? ln - 1 : 0);
                g.col = 0;
                g.end_line = g.line;
                g.end_col = 1;
                VEC_PUSH(comp, g, L1Diag);
                found = 1;
            } else if (strncmp(lb2, "error:", 6) == 0 ||
                       strncmp(lb2, "ERROR:", 6) == 0) {
                memset(&g, 0, sizeof(g));
                g.sev = L1SEV_ERROR;
                g.msg = strdup(lb2);
                g.source = strdup("l1com");
                g.line = 0;
                g.col = 0;
                g.end_line = 0;
                g.end_col = 1;
                VEC_PUSH(comp, g, L1Diag);
                found = 1;
            }
            if (!nl)
                break;
            p = nl + 1;
        }

        if (found) {
            /* merge compiler diagnostics into the static diagnostics
             * instead of replacing them, skipping exact duplicates
             * (same line + message) */
            for (int k = 0; k < comp.len; k++) {
                int dup = 0;
                for (int m = 0; m < d->diags.len; m++) {
                    if (d->diags.data[m].line == comp.data[k].line &&
                        strcmp(d->diags.data[m].msg,
                               comp.data[k].msg) == 0) {
                        dup = 1;
                        break;
                    }
                }
                if (!dup)
                    VEC_PUSH(d->diags, comp.data[k], L1Diag);
                else {
                    free(comp.data[k].msg);
                    free(comp.data[k].source);
                }
            }
            free(comp.data);
            d->compiler_diags = 1;
        } else {
            /* compiler reported nothing: keep static diagnostics */
            for (int k = 0; k < comp.len; k++) {
                free(comp.data[k].msg);
                free(comp.data[k].source);
            }
            free(comp.data);
        }
    }

cleanup:
    sb_free(&out);
    free(linemap);
    if (used_temp) {
        char b[8240];
        unlink(src);
        unlink(prepath);
        snprintf(b, sizeof(b), "%s.l1asm", tmpl);
        unlink(b);
        snprintf(b, sizeof(b), "%s.l1dbg", tmpl);
        unlink(b);
        snprintf(b, sizeof(b), "%s.l1obj", tmpl);
        unlink(b);
        snprintf(b, sizeof(b), "%s.pre.l1asm", tmpl);
        unlink(b);
        snprintf(b, sizeof(b), "%s.pre.l1dbg", tmpl);
        unlink(b);
        snprintf(b, sizeof(b), "%s.pre.l1obj", tmpl);
        unlink(b);
    }
}

/* ==================== doc lifecycle ==================== */

static char *l1_uri_to_path(const char *uri)
{
    SB b;
    const char *p;
    if (!uri)
        return NULL;
    if (strncmp(uri, "file://", 7) != 0)
        return strdup(uri);
    p = uri + 7;
    if (strncmp(p, "localhost", 9) == 0)
        p += 9;
    sb_init(&b);
    while (*p) {
        if (*p == '%' && p[1] && p[2]) {
            char hex[3] = { p[1], p[2], 0 };
            long v = strtol(hex, NULL, 16);
            sb_addc(&b, (char)v);
            p += 3;
        } else {
            sb_addc(&b, *p);
            p++;
        }
    }
    {
        char *res = strdup(sb_cstr(&b));
        sb_free(&b);
        return res;
    }
}

L1Doc *l1_doc_new(const char *uri, const char *path, const char *text,
                  int version)
{
    L1Doc *d = calloc(1, sizeof(L1Doc));
    if (!d)
        return NULL;
    d->uri = strdup(uri ? uri : "");
    d->path = path ? strdup(path) : l1_uri_to_path(uri);
    l1_doc_set_text(d, text, version);
    return d;
}

void l1_doc_set_text(L1Doc *d, const char *text, int version)
{
    int i;
    if (!d)
        return;
    free(d->text);
    d->text = text ? strdup(text) : strdup("");
    d->version = version;

    for (i = 0; i < d->nlines; i++)
        free(d->lines[i].text);
    free(d->lines);
    d->lines = NULL;
    d->nlines = 0;

    {
        const char *p = d->text;
        int n = 0;
        if (*p)
            n = 1;
        while (*p) {
            if (*p == '\n')
                n++;
            p++;
        }
        d->lines = calloc((size_t)n, sizeof(L1Line));
        if (!d->lines)
            return;
        p = d->text;
        while (*p) {
            const char *start = p;
            int len = 0;
            while (*p && *p != '\n')
                p++;
            len = (int)(p - start);
            d->lines[d->nlines].text = strndup(start, (size_t)len);
            d->lines[d->nlines].nbytes = len;
            if (*p == '\n')
                p++;
            d->nlines++;
        }
    }

    {
        const char *ext = d->path ? strrchr(d->path, '.') : NULL;
        d->is_header = ext && strcmp(ext, ".l1h") == 0;
    }

    l1_doc_analyze(d);
}

void l1_doc_free(L1Doc *d)
{
    int i;
    if (!d)
        return;
    free(d->uri);
    free(d->path);
    free(d->text);
    for (i = 0; i < d->nlines; i++)
        free(d->lines[i].text);
    free(d->lines);
    l1_vec_clear_all(d);
    free(d->vars.data);
    free(d->funcs.data);
    free(d->objs.data);
    free(d->labels.data);
    free(d->usages.data);
    free(d->includes.data);
    free(d->macros.data);
    free(d->toks.data);
    free(d->diags.data);
    free(d);
}

/* ==================== position helpers ==================== */

static int l1_pos_to_byte(const L1Doc *d, int line, int col)
{
    if (line < 0 || line >= d->nlines)
        return 0;
    return l1_utf16_to_utf8(d->lines[line].text, col);
}

static int l1_tok_at(const L1Doc *d, int line, int byteoff)
{
    int i;
    for (i = 0; i < d->toks.len; i++) {
        const L1Tok *t = &d->toks.data[i];
        if (t->line != line)
            continue;
        if (byteoff >= t->bstart && byteoff <= t->bstart + t->blen)
            return i;
        if (t->bstart > byteoff)
            break;
    }
    return -1;
}

/* word region at cursor (byte offsets in the line) */
static void l1_word_region(const L1Doc *d, int line, int byteoff,
                           int *start, int *end)
{
    const char *s;
    int i;
    *start = byteoff;
    *end = byteoff;
    if (line < 0 || line >= d->nlines)
        return;
    s = d->lines[line].text;
    /* expand to a "completion word": word chars, : # - */
    i = byteoff;
    while (i > 0 &&
           (l1_is_word_char((unsigned char)s[i - 1]) ||
            s[i - 1] == ':' || s[i - 1] == '#' || s[i - 1] == '-'))
        i--;
    *start = i;
    i = byteoff;
    while (s[i] && (l1_is_word_char((unsigned char)s[i]) ||
                    s[i] == ':' || s[i] == '#' || s[i] == '-'))
        i++;
    *end = i;
}

/* ==================== LSP features ==================== */

static JVal *j_range(int sl, int sc, int el, int ec)
{
    JVal *r = j_obj_new();
    JVal *st = j_obj_new();
    JVal *en = j_obj_new();
    j_obj_set(st, "line", j_num_new(sl));
    j_obj_set(st, "character", j_num_new(sc));
    j_obj_set(en, "line", j_num_new(el));
    j_obj_set(en, "character", j_num_new(ec));
    j_obj_set(r, "start", st);
    j_obj_set(r, "end", en);
    return r;
}

static JVal *j_location(const L1Doc *d, int line, int col, int end_col)
{
    JVal *l = j_obj_new();
    j_obj_set(l, "uri", j_str_new(d->uri));
    j_obj_set(l, "range", j_range(line, col, line, end_col));
    return l;
}

/* resolve the symbol under the cursor; returns 0 and fills result, or -1 */
static int l1_resolve_at(const L1Doc *d, int line, int col, int *kind,
                         int *ref)
{
    int byteoff = l1_pos_to_byte(d, line, col);
    int ti = l1_tok_at(d, line, byteoff);
    const L1Tok *t;
    const char *scope;

    *kind = -1;
    *ref = -1;
    if (ti < 0)
        return -1;
    t = &d->toks.data[ti];

    scope = l1_scope_at(d, line);

    if (t->kind == TK_LABEL) {
        const char *nm = t->text + 1;
        const L1Builtin *bf;
        char *psep = strstr(nm, "->");
        if (t->flags & TF_DECL_LABEL) {
            *kind = 2;   /* label */
            *ref = l1_find_label(d, nm);
            return *ref >= 0 ? 0 : -1;
        }
        if (psep) {
            char mbuf[256];
            char obuf[256];
            snprintf(mbuf, sizeof(mbuf), "%.*s", (int)(psep - nm), nm);
            snprintf(obuf, sizeof(obuf), "%s", psep + 2);
            *kind = 1;
            *ref = l1_find_method(d, mbuf, obuf);
            return *ref >= 0 ? 0 : -1;
        }
        bf = l1_find_builtin(nm);
        if (bf) {
            *kind = 3;   /* builtin */
            *ref = -1;
            return 0;
        }
        {
            int fidx = l1_find_func(d, nm);
            if (fidx != -1) {
                *kind = 1;
                *ref = fidx;
                return 0;
            }
            int midx = l1_find_macro(d, nm);
            if (midx != -1) {
                *kind = 4;
                *ref = midx;
                return 0;
            }
            int lidx = l1_find_label(d, nm);
            if (lidx != -1) {
                *kind = 2;
                *ref = lidx;
                return 0;
            }
        }
        return -1;
    }

    if (t->kind == TK_WORD && t->text) {
        const char *w = t->text;
        int wlen = (int)strlen(w);
        if (l1_is_type(w)) {
            *kind = 5;   /* type */
            *ref = -1;
            return 0;
        }
        if (l1_is_keyword(w)) {
            *kind = 6;   /* keyword */
            *ref = -1;
            return 0;
        }
        if (w[wlen - 1] == '~') {
            int vidx = l1_find_var(d, w, scope);
            *kind = 0;
            if (vidx == -1 && wlen > 6 &&
                strcmp(w + wlen - 6, "addr~") == 0) {
                char base[512];
                snprintf(base, sizeof(base), "%.*s~", wlen - 6, w);
                vidx = l1_find_var(d, base, scope);
            }
            *ref = vidx;
            return vidx >= 0 ? 0 : -1;
        }
        {
            int vidx = l1_find_var(d, w, scope);
            if (vidx != -1) {
                *kind = 0;
                *ref = vidx;
                return 0;
            }
        }
        {
            int fidx = l1_find_func(d, w);
            if (fidx != -1) {
                *kind = 1;
                *ref = fidx;
                return 0;
            }
        }
    }
    return -1;
}

JVal *l1_definition(L1Doc *d, int line, int col)
{
    int kind, ref;
    if (l1_resolve_at(d, line, col, &kind, &ref) != 0)
        return NULL;
    switch (kind) {
    case 0:
        if (ref < 0)
            return NULL;
        return j_location(d, d->vars.data[ref].line,
                          d->vars.data[ref].col,
                          d->vars.data[ref].end_col);
    case 1:
        if (ref < 0)
            return NULL;
        return j_location(d, d->funcs.data[ref].line,
                          d->funcs.data[ref].col,
                          d->funcs.data[ref].end_col);
    case 2:
        if (ref < 0)
            return NULL;
        return j_location(d, d->labels.data[ref].line,
                          d->labels.data[ref].col,
                          d->labels.data[ref].end_col);
    case 4:
        if (ref < 0)
            return NULL;
        return j_location(d, d->macros.data[ref].line,
                          d->macros.data[ref].col,
                          d->macros.data[ref].col +
                          (int)strlen(d->macros.data[ref].name));
    default:
        return NULL;
    }
}

JVal *l1_references(L1Doc *d, int line, int col)
{
    int kind, ref;
    JVal *res = j_arr_new();
    int i;
    char *name = NULL;
    int is_method = 0;

    if (l1_resolve_at(d, line, col, &kind, &ref) != 0)
        return res;

    /* determine the canonical name to search for */
    {
        int byteoff = l1_pos_to_byte(d, line, col);
        int ti = l1_tok_at(d, line, byteoff);
        const L1Tok *t = ti >= 0 ? &d->toks.data[ti] : NULL;
        if (t && t->kind == TK_LABEL) {
            name = strdup(t->text + 1);
            if (strstr(name, "->"))
                is_method = 1;
        } else if (t && t->kind == TK_WORD && t->text) {
            name = strdup(t->text);
        }
    }
    (void)is_method;
    if (!name)
        return res;

    if (kind == 0 && ref >= 0) {
        const L1Var *v = &d->vars.data[ref];
        /* definition */
        j_arr_push(res, j_location(d, v->line, v->col, v->end_col));
        for (i = 0; i < d->usages.len; i++) {
            if (d->usages.data[i].kind == U_VAR &&
                strcmp(d->usages.data[i].name, v->name) == 0) {
                if (d->usages.data[i].is_def)
                    continue;
                j_arr_push(res, j_location(d, d->usages.data[i].line,
                                           d->usages.data[i].col,
                                           d->usages.data[i].col +
                                           d->usages.data[i].len));
            }
        }
    } else if (kind == 1 && ref >= 0) {
        const L1Func *f = &d->funcs.data[ref];
        j_arr_push(res, j_location(d, f->line, f->col, f->end_col));
        for (i = 0; i < d->usages.len; i++) {
            if (d->usages.data[i].kind == U_FUNC &&
                strcmp(d->usages.data[i].name, name) == 0) {
                if (d->usages.data[i].is_def)
                    continue;
                j_arr_push(res, j_location(d, d->usages.data[i].line,
                                           d->usages.data[i].col,
                                           d->usages.data[i].col +
                                           d->usages.data[i].len));
            }
        }
    } else if (kind == 2 && ref >= 0) {
        const L1Label *l = &d->labels.data[ref];
        j_arr_push(res, j_location(d, l->line, l->col, l->end_col));
        for (i = 0; i < d->usages.len; i++) {
            if (d->usages.data[i].kind == U_LABEL &&
                strcmp(d->usages.data[i].name, name) == 0) {
                j_arr_push(res, j_location(d, d->usages.data[i].line,
                                           d->usages.data[i].col,
                                           d->usages.data[i].col +
                                           d->usages.data[i].len));
            }
        }
        for (i = 0; i < d->usages.len; i++) {
            if (d->usages.data[i].kind == U_FUNC &&
                strcmp(d->usages.data[i].name, name) == 0) {
                j_arr_push(res, j_location(d, d->usages.data[i].line,
                                           d->usages.data[i].col,
                                           d->usages.data[i].col +
                                           d->usages.data[i].len));
            }
        }
    }
    (void)is_method;
    free(name);
    return res;
}

JVal *l1_highlight(L1Doc *d, int line, int col)
{
    JVal *res = j_arr_new();
    int kind, ref;
    char *name = NULL;

    if (l1_resolve_at(d, line, col, &kind, &ref) != 0)
        return res;
    {
        int byteoff = l1_pos_to_byte(d, line, col);
        int ti = l1_tok_at(d, line, byteoff);
        const L1Tok *t = ti >= 0 ? &d->toks.data[ti] : NULL;
        if (t && t->kind == TK_LABEL)
            name = strdup(t->text + 1);
        else if (t && t->kind == TK_WORD && t->text)
            name = strdup(t->text);
    }
    if (!name)
        return res;

    {
        int i;
        for (i = 0; i < d->toks.len; i++) {
            const L1Tok *t = &d->toks.data[i];
            if (t->kind != TK_WORD && t->kind != TK_LABEL)
                continue;
            if (!t->text)
                continue;
            if (t->kind == TK_LABEL) {
                if (strcmp(t->text + 1, name) != 0)
                    continue;
            } else {
                if (strcmp(t->text, name) != 0)
                    continue;
            }
            {
                JVal *h = j_obj_new();
                int k = (t->flags & TF_DECL_VAR) ? 3 : 1; /* write:3 */
                j_obj_set(h, "range", j_range(t->line, t->start,
                                              t->line, t->start + t->len));
                j_obj_set(h, "kind", j_num_new(k));
                j_arr_push(res, h);
            }
        }
    }
    free(name);
    return res;
}

/* ---------- hover ---------- */

JVal *l1_hover(L1Doc *d, int line, int col)
{
    int kind, ref;
    int byteoff = l1_pos_to_byte(d, line, col);
    int ti = l1_tok_at(d, line, byteoff);
    SB md;
    const L1Tok *t;

    if (ti < 0)
        return NULL;
    t = &d->toks.data[ti];

    sb_init(&md);

    if (t->kind == TK_COMMENT) {
        sb_add(&md, "Brackets comment");
    } else if (t->kind == TK_STRING) {
        sb_add(&md, "String literal");
    } else if (t->kind == TK_NUMBER) {
        sb_add(&md, "Number literal");
    } else if (t->kind == TK_PREPROC) {
        sb_add(&md, "**Preprocessor directive** `");
        sb_add(&md, t->text ? t->text : "");
        sb_add(&md, "`");
        if (t->text && strcmp(t->text, "#include") == 0)
            sb_add(&md, "\n\nInclude a library header `*.l1h`.");
    } else if (t->kind == TK_LABEL) {
        const char *nm = t->text + 1;
        const L1Builtin *bf = l1_find_builtin(nm);
        char *psep = strstr(nm, "->");
        if (bf) {
            sb_add(&md, "**Library function** `");
            sb_add(&md, bf->name);
            sb_add(&md, "`\n\nLibrary: `");
            sb_add(&md, bf->lib);
            sb_add(&md, "`\n\n");
            sb_add(&md, bf->doc);
            if (bf->args[0]) {
                sb_add(&md, "\n\nArguments: `");
                sb_add(&md, bf->args);
                sb_add(&md, "`");
            }
        } else if (psep) {
            char mbuf[256];
            char obuf[256];
            int m;
            snprintf(mbuf, sizeof(mbuf), "%.*s", (int)(psep - nm), nm);
            snprintf(obuf, sizeof(obuf), "%s", psep + 2);
            m = l1_find_method(d, mbuf, obuf);
            sb_add(&md, "**Method** `");
            sb_add(&md, nm);
            sb_add(&md, "`\n\nObject: `");
            sb_add(&md, obuf);
            sb_add(&md, "`");
            if (m >= 0) {
                sb_printf(&md, "\n\nDefined at line %d.",
                          d->funcs.data[m].line + 1);
                if (d->funcs.data[m].signature) {
                    sb_add(&md, "\n\nSignature: `");
                    sb_add(&md, d->funcs.data[m].signature);
                    sb_add(&md, "`");
                }
            }
        } else {
            int fidx = l1_find_func(d, nm);
            int midx = l1_find_macro(d, nm);
            int lidx = l1_find_label(d, nm);
            if (fidx >= 0) {
                sb_add(&md, "**Function** `");
                sb_add(&md, d->funcs.data[fidx].name);
                sb_add(&md, "`\n\n```brackets\n(");
                sb_add(&md, d->funcs.data[fidx].name);
                sb_add(&md, " func)\n```\n\nDefined at line ");
                sb_printf(&md, "%d.", d->funcs.data[fidx].line + 1);
                if (d->funcs.data[fidx].signature) {
                    sb_add(&md, "\n\nSignature: `");
                    sb_add(&md, d->funcs.data[fidx].signature);
                    sb_add(&md, "`");
                }
                if (d->funcs.data[fidx].ret_sig) {
                    sb_add(&md, "\n\nReturns: `");
                    sb_add(&md, d->funcs.data[fidx].ret_sig);
                    sb_add(&md, "`");
                }
            } else if (midx >= 0) {
                sb_add(&md, "**Preprocessor macro** `");
                sb_add(&md, d->macros.data[midx].name);
                sb_add(&md, "`\n\nArgs: `");
                sb_add(&md, d->macros.data[midx].args);
                sb_add(&md, "`\n\nDefined at line ");
                sb_printf(&md, "%d.", d->macros.data[midx].line + 1);
            } else if (lidx >= 0) {
                sb_add(&md, "**Label** `");
                sb_add(&md, d->labels.data[lidx].name);
                sb_add(&md, "`\n\nDefined at line ");
                sb_printf(&md, "%d.", d->labels.data[lidx].line + 1);
            } else {
                sb_add(&md, "Unknown symbol `");
                sb_add(&md, nm);
                sb_add(&md, "`");
            }
        }
    } else if (t->kind == TK_WORD && t->text) {
        const char *w = t->text;
        if (l1_resolve_at(d, line, col, &kind, &ref) != 0) {
            sb_add(&md, "Identifier `");
            sb_add(&md, w);
            sb_add(&md, "`");
        } else {
            switch (kind) {
            case 0:
                if (ref >= 0) {
                    const L1Var *v = &d->vars.data[ref];
                    sb_add(&md, "**Variable** `");
                    sb_add(&md, v->name);
                    sb_add(&md, "`\n\n- type: `");
                    sb_add(&md, v->type);
                    sb_add(&md, "`\n- size: `");
                    sb_add(&md, v->size);
                    sb_add(&md, "`\n- scope: `");
                    sb_add(&md, v->scope);
                    sb_add(&md, "`");
                    if (v->is_const)
                        sb_add(&md, "\n- **const**");
                    if (v->is_array)
                        sb_add(&md, "\n- array");
                    sb_add(&md, "\n\nDeclared at line ");
                    sb_printf(&md, "%d.", v->line + 1);
                }
                break;
            case 1:
                if (ref >= 0) {
                    const L1Func *f = &d->funcs.data[ref];
                    sb_add(&md, "**Function** `");
                    sb_add(&md, f->name);
                    sb_add(&md, "`\n\nDefined at line ");
                    sb_printf(&md, "%d.", f->line + 1);
                    if (f->signature) {
                        sb_add(&md, "\n\nSignature: `");
                        sb_add(&md, f->signature);
                        sb_add(&md, "`");
                    }
                }
                break;
            case 5: {
                const char *td = l1_type_doc(w);
                sb_add(&md, "**Type** `");
                sb_add(&md, w);
                sb_add(&md, "`\n\n");
                sb_add(&md, td);
                break;
            }
            case 6: {
                SB kd;
                sb_init(&kd);
                if (l1_keyword_doc(w, &kd)) {
                    sb_add(&md, "**Keyword** `");
                    sb_add(&md, w);
                    sb_add(&md, "`\n\n");
                    sb_add(&md, sb_cstr(&kd));
                } else {
                    sb_add(&md, "**Keyword** `");
                    sb_add(&md, w);
                    sb_add(&md, "`");
                }
                sb_free(&kd);
                break;
            }
            default:
                sb_add(&md, "Identifier `");
                sb_add(&md, w);
                sb_add(&md, "`");
                break;
            }
        }
    } else if (t->kind == TK_OP) {
        const char *op = t->text ? t->text : "";
        sb_add(&md, "**Operator** `");
        sb_add(&md, op);
        sb_add(&md, "`");
        if (strcmp(op, "=") == 0 || strcmp(op, ":=") == 0)
            sb_add(&md, "\n\nAssignment.");
    } else {
        sb_free(&md);
        return NULL;
    }

    {
        JVal *contents = j_obj_new();
        JVal *r = j_obj_new();
        j_obj_set(contents, "kind", j_str_new("markdown"));
        j_obj_set(contents, "value", j_str_new(sb_cstr(&md)));
        j_obj_set(r, "contents", contents);
        sb_free(&md);
        return r;
    }
}

/* ---------- completion ---------- */

static int l1_str_startswith(const char *s, const char *pre)
{
    return strncmp(s, pre, strlen(pre)) == 0;
}

JVal *l1_completion(L1Doc *d, int line, int col)
{
    JVal *items = j_arr_new();
    int byteoff = l1_pos_to_byte(d, line, col);
    int wstart, wend;
    char prefix[512];
    int plen;
    int i;
    const char *scope;

    l1_word_region(d, line, byteoff, &wstart, &wend);
    if (wend - wstart >= 511)
        wend = wstart + 511;
    plen = wend - wstart;
    if (plen > 0)
        memcpy(prefix, d->lines[line].text + wstart, (size_t)plen);
    prefix[plen] = '\0';

    scope = l1_scope_at(d, line);

    /* ---------- : prefix -> functions / labels / builtins ---------- */
    if (prefix[0] == ':') {
        const char *sub = prefix + 1;
        for (i = 0; l1_builtin_table[i].name; i++) {
            const L1Builtin *b = &l1_builtin_table[i];
            if (!l1_str_startswith(b->name, sub))
                continue;
            {
                JVal *it = j_obj_new();
                JVal *doc = j_obj_new();
                SB dd;
                sb_init(&dd);
                sb_printf(&dd, "`%s` (library `%s`)\n\n%s",
                          b->name, b->lib, b->doc);
                if (b->args[0]) {
                    sb_add(&dd, "\n\nArguments: `");
                    sb_add(&dd, b->args);
                    sb_add(&dd, "`");
                }
                j_obj_set(it, "label", j_str_new(b->name));
                j_obj_set(it, "kind", j_num_new(3));   /* Function */
                j_obj_set(it, "detail", j_str_new(b->lib));
                j_obj_set(doc, "kind", j_str_new("markdown"));
                j_obj_set(doc, "value", j_str_new(sb_cstr(&dd)));
                j_obj_set(it, "documentation", doc);
                j_arr_push(items, it);
                sb_free(&dd);
            }
        }
        for (i = 0; i < d->funcs.len; i++) {
            const L1Func *f = &d->funcs.data[i];
            char label[512];
            int is_method = f->parent != NULL;
            if (is_method)
                snprintf(label, sizeof(label), "%s->%s", f->name, f->parent);
            else
                snprintf(label, sizeof(label), "%s", f->name);
            if (!l1_str_startswith(label, sub))
                continue;
            {
                JVal *it = j_obj_new();
                JVal *doc = j_obj_new();
                SB dd;
                sb_init(&dd);
                sb_add(&dd, "**");
                if (is_method)
                    sb_add(&dd, "Method");
                else
                    sb_add(&dd, "Function");
                sb_add(&dd, "** `");
                sb_add(&dd, f->name);
                sb_add(&dd, "`\n\nDefined at line ");
                sb_printf(&dd, "%d.", f->line + 1);
                if (f->signature) {
                    sb_add(&dd, "\n\nSignature: `");
                    sb_add(&dd, f->signature);
                    sb_add(&dd, "`");
                }
                j_obj_set(it, "label", j_str_new(label));
                j_obj_set(it, "kind", j_num_new(is_method ? 2 : 3));
                j_obj_set(it, "detail",
                          j_str_new(is_method ? "object method" : "function"));
                j_obj_set(doc, "kind", j_str_new("markdown"));
                j_obj_set(doc, "value", j_str_new(sb_cstr(&dd)));
                j_obj_set(it, "documentation", doc);
                j_arr_push(items, it);
                sb_free(&dd);
            }
        }
        for (i = 0; i < d->labels.len; i++) {
            const L1Label *l = &d->labels.data[i];
            if (!l1_str_startswith(l->name, sub))
                continue;
            {
                JVal *it = j_obj_new();
                j_obj_set(it, "label", j_str_new(l->name));
                j_obj_set(it, "kind", j_num_new(18));  /* Reference */
                j_obj_set(it, "detail", j_str_new("label"));
                j_arr_push(items, it);
            }
        }
        for (i = 0; i < d->macros.len; i++) {
            const L1Macro *m = &d->macros.data[i];
            if (!l1_str_startswith(m->name, sub))
                continue;
            {
                JVal *it = j_obj_new();
                JVal *doc = j_obj_new();
                SB dd;
                sb_init(&dd);
                sb_add(&dd, "**Macro** `");
                sb_add(&dd, m->name);
                sb_add(&dd, "`\n\nArgs: `");
                sb_add(&dd, m->args);
                sb_add(&dd, "`");
                j_obj_set(it, "label", j_str_new(m->name));
                j_obj_set(it, "kind", j_num_new(3));
                j_obj_set(it, "detail", j_str_new("preprocessor macro"));
                j_obj_set(doc, "kind", j_str_new("markdown"));
                j_obj_set(doc, "value", j_str_new(sb_cstr(&dd)));
                j_obj_set(it, "documentation", doc);
                j_arr_push(items, it);
                sb_free(&dd);
            }
        }
        return items;
    }

    /* ---------- # prefix -> preprocessor ---------- */
    if (prefix[0] == '#') {
        static const char *const dirs[] = {
            "include", "var", "func", "define", NULL
        };
        for (i = 0; dirs[i]; i++) {
            char label[64];
            snprintf(label, sizeof(label), "#%s", dirs[i]);
            if (!l1_str_startswith(label, prefix))
                continue;
            {
                JVal *it = j_obj_new();
                j_obj_set(it, "label", j_str_new(label));
                j_obj_set(it, "kind", j_num_new(14));  /* Keyword */
                j_obj_set(it, "detail", j_str_new("preprocessor directive"));
                j_arr_push(items, it);
            }
        }
        return items;
    }

    /* ---------- keyword / type / variable completion ---------- */
    for (i = 0; l1_keywords[i]; i++) {
        const char *kw = l1_keywords[i];
        if (!l1_str_startswith(kw, prefix))
            continue;
        {
            JVal *it = j_obj_new();
            JVal *doc = j_obj_new();
            SB dd;
            sb_init(&dd);
            if (!l1_keyword_doc(kw, &dd)) {
                sb_reset(&dd);
                sb_add(&dd, "Brackets keyword.");
            }
            j_obj_set(it, "label", j_str_new(kw));
            j_obj_set(it, "kind", j_num_new(14));   /* Keyword */
            j_obj_set(it, "detail", j_str_new("keyword"));
            {
                const char *snp = l1_keyword_snippet(kw);
                if (snp) {
                    j_obj_set(it, "insertTextFormat", j_num_new(2)); /* Snippet */
                    j_obj_set(it, "insertText", j_str_new(snp));
                }
            }
            j_obj_set(doc, "kind", j_str_new("markdown"));
            j_obj_set(doc, "value", j_str_new(sb_cstr(&dd)));
            j_obj_set(it, "documentation", doc);
            j_arr_push(items, it);
            sb_free(&dd);
        }
    }
    for (i = 0; l1_types[i]; i++) {
        const char *ty = l1_types[i];
        if (!l1_str_startswith(ty, prefix))
            continue;
        {
            JVal *it = j_obj_new();
            JVal *doc = j_obj_new();
            j_obj_set(it, "label", j_str_new(ty));
            j_obj_set(it, "kind", j_num_new(14));
            j_obj_set(it, "detail", j_str_new("data type"));
            j_obj_set(doc, "kind", j_str_new("markdown"));
            j_obj_set(doc, "value", j_str_new(l1_type_doc(ty)));
            j_obj_set(it, "documentation", doc);
            j_arr_push(items, it);
        }
    }
    for (i = 0; i < d->vars.len; i++) {
        const L1Var *v = &d->vars.data[i];
        if (!l1_scope_visible(v->scope, scope, d))
            continue;
        if (!l1_str_startswith(v->name, prefix))
            continue;
        {
            JVal *it = j_obj_new();
            JVal *doc = j_obj_new();
            SB dd;
            sb_init(&dd);
            sb_printf(&dd, "type `%s`, size `%s`, scope `%s`",
                      v->type, v->size, v->scope);
            if (v->is_const)
                sb_add(&dd, ", const");
            j_obj_set(it, "label", j_str_new(v->name));
            j_obj_set(it, "kind", j_num_new(v->is_const ? 21 : 6));
            j_obj_set(it, "detail", j_str_new(v->type));
            j_obj_set(doc, "kind", j_str_new("markdown"));
            j_obj_set(doc, "value", j_str_new(sb_cstr(&dd)));
            j_obj_set(it, "documentation", doc);
            j_arr_push(items, it);
            sb_free(&dd);
        }
    }
    for (i = 0; i < d->macros.len; i++) {
        const L1Macro *m = &d->macros.data[i];
        if (!l1_str_startswith(m->name, prefix))
            continue;
        {
            JVal *it = j_obj_new();
            j_obj_set(it, "label", j_str_new(m->name));
            j_obj_set(it, "kind", j_num_new(3));
            j_obj_set(it, "detail", j_str_new("macro"));
            j_arr_push(items, it);
        }
    }
    return items;
}

/* ---------- document symbols ---------- */

JVal *l1_document_symbols(L1Doc *d)
{
    JVal *top = j_arr_new();
    int i;

    /* functions and objects at top level */
    for (i = 0; i < d->funcs.len; i++) {
        const L1Func *f = &d->funcs.data[i];
        JVal *s = j_obj_new();
        JVal *children = j_arr_new();
        int j;
        if (f->parent)
            continue;
        j_obj_set(s, "name", j_str_new(f->name));
        j_obj_set(s, "kind", j_num_new(12));   /* Function */
        j_obj_set(s, "range",
                  j_range(f->line, 0,
                          f->end_line >= 0 ? f->end_line : f->line,
                          f->end_line >= 0 ? 1 : f->end_col));
        j_obj_set(s, "selectionRange",
                  j_range(f->line, f->col, f->line, f->end_col));
        for (j = 0; j < d->vars.len; j++) {
            const L1Var *v = &d->vars.data[j];
            if (strcmp(v->scope, f->name) != 0)
                continue;
            {
                JVal *vs = j_obj_new();
                j_obj_set(vs, "name", j_str_new(v->name));
                j_obj_set(vs, "kind", j_num_new(v->is_const ? 14 : 13));
                j_obj_set(vs, "range",
                          j_range(v->line, v->col, v->line, v->end_col));
                j_obj_set(vs, "selectionRange",
                          j_range(v->line, v->col, v->line, v->end_col));
                j_arr_push(children, vs);
            }
        }
        j_obj_set(s, "children", children);
        j_arr_push(top, s);
    }

    /* objects with methods and vars */
    for (i = 0; i < d->objs.len; i++) {
        const L1Obj *o = &d->objs.data[i];
        JVal *s = j_obj_new();
        JVal *children = j_arr_new();
        int j;
        j_obj_set(s, "name", j_str_new(o->name));
        j_obj_set(s, "kind", j_num_new(5));   /* Class */
        j_obj_set(s, "range",
                  j_range(o->line, 0,
                          o->end_line >= 0 ? o->end_line : o->line,
                          o->end_line >= 0 ? 1 : o->end_col));
        j_obj_set(s, "selectionRange",
                  j_range(o->line, o->col, o->line, o->end_col));
        for (j = 0; j < d->funcs.len; j++) {
            const L1Func *f = &d->funcs.data[j];
            if (!f->parent || strcmp(f->parent, o->name) != 0)
                continue;
            {
                JVal *fs = j_obj_new();
                j_obj_set(fs, "name", j_str_new(f->name));
                j_obj_set(fs, "kind", j_num_new(6));   /* Method */
                j_obj_set(fs, "range",
                          j_range(f->line, 0,
                                  f->end_line >= 0 ? f->end_line : f->line,
                                  f->end_line >= 0 ? 1 : f->end_col));
                j_obj_set(fs, "selectionRange",
                          j_range(f->line, f->col, f->line, f->end_col));
                j_arr_push(children, fs);
            }
        }
        for (j = 0; j < d->vars.len; j++) {
            const L1Var *v = &d->vars.data[j];
            if (strcmp(v->scope, o->name) != 0)
                continue;
            {
                JVal *vs = j_obj_new();
                j_obj_set(vs, "name", j_str_new(v->name));
                j_obj_set(vs, "kind", j_num_new(v->is_const ? 14 : 13));
                j_obj_set(vs, "range",
                          j_range(v->line, v->col, v->line, v->end_col));
                j_obj_set(vs, "selectionRange",
                          j_range(v->line, v->col, v->line, v->end_col));
                j_arr_push(children, vs);
            }
        }
        j_obj_set(s, "children", children);
        j_arr_push(top, s);
    }

    /* file level variables (scope "") */
    for (i = 0; i < d->vars.len; i++) {
        const L1Var *v = &d->vars.data[i];
        if (strcmp(v->scope, "") != 0)
            continue;
        {
            JVal *vs = j_obj_new();
            j_obj_set(vs, "name", j_str_new(v->name));
            j_obj_set(vs, "kind", j_num_new(v->is_const ? 14 : 13));
            j_obj_set(vs, "range",
                      j_range(v->line, v->col, v->line, v->end_col));
            j_obj_set(vs, "selectionRange",
                      j_range(v->line, v->col, v->line, v->end_col));
            j_arr_push(top, vs);
        }
    }
    return top;
}

/* ---------- folding ---------- */

JVal *l1_folding(L1Doc *d)
{
    JVal *res = j_arr_new();
    int i;
    for (i = 0; i < d->funcs.len; i++) {
        const L1Func *f = &d->funcs.data[i];
        if (f->end_line > f->line) {
            JVal *fr = j_obj_new();
            j_obj_set(fr, "startLine", j_num_new(f->line));
            j_obj_set(fr, "endLine", j_num_new(f->end_line - 1));
            j_arr_push(res, fr);
        }
    }
    for (i = 0; i < d->objs.len; i++) {
        const L1Obj *o = &d->objs.data[i];
        if (o->end_line > o->line) {
            JVal *fr = j_obj_new();
            j_obj_set(fr, "startLine", j_num_new(o->line));
            j_obj_set(fr, "endLine", j_num_new(o->end_line - 1));
            j_arr_push(res, fr);
        }
    }
    /* comment blocks */
    for (i = 0; i < d->nlines; i++) {
        const char *s = d->lines[i].text;
        char *c = strstr(s, "//");
        if (c) {
            int j = i + 1;
            while (j < d->nlines && strstr(d->lines[j].text, "//"))
                j++;
            if (j - i >= 2) {
                JVal *fr = j_obj_new();
                j_obj_set(fr, "startLine", j_num_new(i));
                j_obj_set(fr, "endLine", j_num_new(j - 1));
                j_arr_push(res, fr);
                i = j;
            }
        }
    }
    return res;
}

/* ---------- signature help ---------- */

JVal *l1_signature(L1Doc *d, int line, int col)
{
    int i;
    const char *scope = l1_scope_at(d, line);
    const L1Tok *label_tok = NULL;
    int byteoff = l1_pos_to_byte(d, line, col);

    /* find nearest label token at or before cursor on this line */
    for (i = 0; i < d->toks.len; i++) {
        const L1Tok *t = &d->toks.data[i];
        if (t->line != line)
            continue;
        if (t->kind != TK_LABEL)
            continue;
        if (t->bstart > byteoff)
            break;
        label_tok = t;
    }
    if (!label_tok)
        return NULL;

    {
        JVal *res = j_obj_new();
        JVal *sigs = j_arr_new();
        JVal *sig = j_obj_new();
        JVal *params = j_arr_new();
        JVal *labelv;
        const char *nm = label_tok->text + 1;
        const L1Builtin *bf = l1_find_builtin(nm);
        char siglabel[512];
        int argcount = 0;
        (void)scope;

        if (bf) {
            if (bf->args[0]) {
                snprintf(siglabel, sizeof(siglabel), "%s(%s)", bf->name,
                         bf->args);
                /* count args */
                {
                    const char *p2;
                    for (p2 = bf->args; *p2; p2++)
                        if (*p2 == ' ')
                            argcount++;
                    if (bf->args[0])
                        argcount++;
                }
                {
                    char args_copy[512];
                    char *save = NULL;
                    char *tok;
                    snprintf(args_copy, sizeof(args_copy), "%s", bf->args);
                    tok = strtok_r(args_copy, " ", &save);
                    while (tok) {
                        j_arr_push(params, j_str_new(tok));
                        tok = strtok_r(NULL, " ", &save);
                    }
                }
            } else {
                snprintf(siglabel, sizeof(siglabel), "%s()", bf->name);
            }
            j_obj_set(sig, "label", j_str_new(siglabel));
            {
                JVal *doc = j_obj_new();
                SB dd;
                sb_init(&dd);
                sb_printf(&dd, "`%s` (library `%s`)\n\n%s",
                          bf->name, bf->lib, bf->doc);
                j_obj_set(doc, "kind", j_str_new("markdown"));
                j_obj_set(doc, "value", j_str_new(sb_cstr(&dd)));
                j_obj_set(sig, "documentation", doc);
                sb_free(&dd);
            }
        } else {
            int fidx = l1_find_func(d, nm);
            if (fidx >= 0) {
                snprintf(siglabel, sizeof(siglabel), "%s()", nm);
                j_obj_set(sig, "label", j_str_new(siglabel));
                if (d->funcs.data[fidx].signature) {
                    JVal *doc = j_obj_new();
                    SB dd;
                    sb_init(&dd);
                    sb_add(&dd, d->funcs.data[fidx].signature);
                    j_obj_set(doc, "kind", j_str_new("markdown"));
                    j_obj_set(doc, "value", j_str_new(sb_cstr(&dd)));
                    j_obj_set(sig, "documentation", doc);
                    sb_free(&dd);
                    /* try to extract parameter labels */
                    {
                        const char *p2 = d->funcs.data[fidx].signature;
                        const char *sp = strchr(p2, ' ');
                        while (sp) {
                            const char *e = sp + 1;
                            while (*e == ' ')
                                e++;
                            if (*e == '\0')
                                break;
                            {
                                const char *e2 = e;
                                while (*e2 && *e2 != ' ')
                                    e2++;
                                j_arr_push(params,
                                           j_str_new(strndup(e, (size_t)(e2 - e))));
                            }
                            sp = strchr(e, ' ');
                        }
                    }
                }
            } else {
                snprintf(siglabel, sizeof(siglabel), "%s", nm);
                j_obj_set(sig, "label", j_str_new(siglabel));
            }
        }
        labelv = j_obj_new();
        j_obj_set(labelv, "label", j_str_new(siglabel));
        (void)labelv;
        j_obj_set(sig, "parameters", params);
        j_arr_push(sigs, sig);
        j_obj_set(res, "signatures", sigs);
        j_obj_set(res, "activeSignature", j_num_new(0));
        j_obj_set(res, "activeParameter", j_num_new(argcount - 1));
        return res;
    }
}

/* ---------- semantic tokens ---------- */

static int l1_st_token_type(const L1Doc *d, const L1Tok *t)
{
    /* legend: 0 keyword, 1 type, 2 function, 3 variable, 4 constant,
       5 label, 6 macro, 7 comment, 8 string, 9 number, 10 operator,
       11 preprocessor, 12 namespace, 13 class */
    switch (t->kind) {
    case TK_COMMENT:  return 7;
    case TK_STRING:   return 8;
    case TK_NUMBER:   return 9;
    case TK_PREPROC:  return 11;
    case TK_LABEL:    return 5;
    case TK_LPAREN: case TK_RPAREN: case TK_LBRACE: case TK_RBRACE:
    case TK_LBRACKET: case TK_RBRACKET: case TK_COMMA: case TK_SEMI:
    case TK_OP:       return 10;
    case TK_WORD:
        break;
    default:
        return -1;
    }
    if (!t->text)
        return -1;
    if (l1_is_keyword(t->text))
        return 0;
    if (l1_is_type(t->text))
        return 1;
    if (t->flags & TF_DECL_FUNC) {
        /* function or class (object) */
        const char *s = d->lines[t->line].text;
        return strstr(s, "object") ? 13 : 2;
    }
    if (t->flags & TF_DECL_VAR) {
        return d->vars.len && d->vars.data[d->vars.len - 1].is_const ? 4 : 3;
    }
    if (t->text[strlen(t->text) - 1] == '~')
        return 3;
    if (l1_is_keyword(t->text) == 0) {
        if (l1_find_func(d, t->text) >= 0 || l1_find_macro(d, t->text) >= 0)
            return 2;
    }
    return -1;
}

static int l1_st_modifiers(const L1Doc *d, const L1Tok *t, int type)
{
    int m = 0;
    /* legend: 0 declaration, 1 readonly, 2 defaultLibrary */
    if (t->flags & (TF_DECL_VAR | TF_DECL_FUNC | TF_DECL_LABEL))
        m |= 1;                       /* declaration */
    if (type == 5 && (t->flags & TF_BUILTIN))
        m |= 4;                       /* defaultLibrary */
    if (type == 5)
        m |= 2;                       /* labels are read-only targets */
    if (type == 3 && d->vars.len &&
        d->vars.data[d->vars.len - 1].is_const)
        m |= 2;
    return m;
}

JVal *l1_semantic_tokens(L1Doc *d)
{
    JVal *res = j_arr_new();
    int prev_line = 0, prev_start = 0;
    int i;
    for (i = 0; i < d->toks.len; i++) {
        const L1Tok *t = &d->toks.data[i];
        int type = l1_st_token_type(d, t);
        int mods;
        int delta_line, delta_start;
        if (type < 0)
            continue;
        mods = l1_st_modifiers(d, t, type);
        delta_line = t->line - prev_line;
        delta_start = (delta_line == 0) ? (t->start - prev_start) : t->start;
        j_arr_push(res, j_num_new(delta_line));
        j_arr_push(res, j_num_new(delta_start));
        j_arr_push(res, j_num_new(t->len));
        j_arr_push(res, j_num_new(type));
        j_arr_push(res, j_num_new(mods));
        prev_line = t->line;
        prev_start = t->start;
    }
    return res;
}

/* ---------- type / keyword docs ---------- */

const char *l1_type_doc(const char *type)
{
    if (strcmp(type, "bool") == 0 || strcmp(type, "const-bool") == 0 ||
        strcmp(type, "mut-bool") == 0)
        return "bool (8 bytes, stored as int64: 0 = false, 1 = true)";
    if (strcmp(type, "byte") == 0 || strcmp(type, "const-byte") == 0 ||
        strcmp(type, "mut-byte") == 0)
        return "byte (1 byte, unsigned 8-bit integer, 0 to 255)";
    if (strcmp(type, "int16") == 0 || strcmp(type, "const-int16") == 0 ||
        strcmp(type, "mut-int16") == 0)
        return "int16 (2 bytes, signed 16-bit integer, -32768 to 32767)";
    if (strcmp(type, "int32") == 0 || strcmp(type, "const-int32") == 0 ||
        strcmp(type, "mut-int32") == 0)
        return "int32 (4 bytes, signed 32-bit integer)";
    if (strcmp(type, "int64") == 0 || strcmp(type, "const-int64") == 0 ||
        strcmp(type, "mut-int64") == 0)
        return "int64 (8 bytes, signed 64-bit integer) - most common type";
    if (strcmp(type, "double") == 0 || strcmp(type, "const-double") == 0 ||
        strcmp(type, "mut-double") == 0)
        return "double (8 bytes, IEEE 754 64-bit floating point)";
    if (strcmp(type, "string") == 0 || strcmp(type, "const-string") == 0 ||
        strcmp(type, "mut-string") == 0)
        return "string (variable length, null-terminated character string)";
    return "Brackets data type";
}

int l1_keyword_doc(const char *kw, SB *out)
{
    static const struct { const char *kw; const char *doc; const char *snippet; } docs[] = {
        { "func", "Start a function definition: `(name func)` ... `(funcend)`.", NULL },
        { "funcend", "End a function definition.", NULL },
        { "object", "Start an object definition: `(name object)` ... `(objectend)`.", NULL },
        { "objectend", "End an object definition.", NULL },
        { "set", "Declare a variable: `(set TYPE SIZE name~ VALUE)`. All variables use the `~` suffix.", NULL },
        { "if", "Conditional: `(((cond) f~ =) f~ if)` ... `(endif)`. Note: `else` requires `if+`.", "(((cond) f~ =) f~ if)\n\t${1:body}\n(endif)\n$0" },
        { "if+", "Conditional with else: `(((cond) f~ =) f~ if+)` ... `(else)` ... `(endif)`.", "(((cond) f~ =) f~ if+)\n\t${1:if-body}\n(else)\n\t${2:else-body}\n(endif)\n$0" },
        { "else", "Else branch of `if+`.", "(else)" },
        { "endif", "End of an if block.", "(endif)" },
        { "do", "Start a do-while loop body, executed at least once; closed by `(((cond) f~ =) f~ while)`.", "(do)\n\t${1:body}\n((( ${2:cond} ) f=) f while)\n$0" },
        { "while", "Do-while condition: `(((cond) f~ =) f~ while)`.", "(((cond) f~ =) f~ while)" },
        { "for", "For loop condition (Brackets 3.2.0+ needs no `(for-loop)`): `(((cond) f~ =) f~ for)` ... `(next)`.", "(((cond) f~ =) f~ for)\n\t${1:body}\n(next)\n$0" },
        { "for-loop", "Mark the start of a for loop: `(for-loop)`.", "(for-loop)" },
        { "next", "Jump back to the `(for-loop)` start.", "(next)" },
        { "switch", "Start a switch statement.", NULL },
        { "switchend", "End a switch statement.", NULL },
        { "break", "Break out of a switch case.", NULL },
        { "return", "Return from the current function.", NULL },
        { "stpush", "Push a value onto the stack (auto-detect type).", NULL },
        { "stpop", "Pop a value from the stack into a variable.", NULL },
        { "stpushi", "Push an int64 onto the stack.", NULL },
        { "stpopi", "Pop an int64 from the stack into a variable.", NULL },
        { "stpushd", "Push a double onto the stack.", NULL },
        { "stpopd", "Pop a double from the stack into a variable.", NULL },
        { "stpushb", "Push a byte onto the stack.", NULL },
        { "stpopb", "Pop a byte from the stack into a variable.", NULL },
        { "call", "Call a function by label (use `!` instead).", NULL },
        { "jmp", "Unconditional jump to a label.", NULL },
        { "jmpi", "Conditional jump: `(f~ :label jmpi)` jumps if f~ != 0.", NULL },
        { "pointer", "Get the address of a variable: `(variable~ Pvar~ pointer)`.", NULL },
        { "cast", "Cast a value between types: `(cast src~ dest~ =)`.", NULL },
        { "thread", "Start a thread at a label.", NULL },
        { "join", "Wait for all threads to finish.", NULL },
        { "threadexit", "Exit the current thread with a return code.", NULL },
        { "reset-reg", "Reset all internal registers to zero.", NULL },
        { "ASM", "Start an inline assembly block.", NULL },
        { "ASM_END", "End an inline assembly block.", NULL },
        { "range", "Compile-time range check: `(x~ x_min~ x_max~ range)`.", NULL },
        { "unsafe", "Enter unsafe mode (memory bounds checking off).", NULL },
        { "unsafe-end", "Exit unsafe mode.", NULL },
        { "linter", "Enable linter mode.", NULL },
        { "?", "Switch case: `(value~ constant~ ?)`.", NULL },
        { NULL, NULL, NULL }
    };
    int i;
    for (i = 0; docs[i].kw; i++) {
        if (strcmp(docs[i].kw, kw) == 0) {
            sb_add(out, docs[i].doc);
            return 1;
        }
    }
    return 0;
}

const char *l1_keyword_snippet(const char *kw)
{
    static const struct { const char *kw; const char *snippet; } snips[] = {
        { "if", "(((cond) f~ =) f~ if)\n\t${1:body}\n(endif)\n$0" },
        { "if+", "(((cond) f~ =) f~ if+)\n\t${1:if-body}\n(else)\n\t${2:else-body}\n(endif)\n$0" },
        { "else", "(else)" },
        { "endif", "(endif)" },
        { "do", "(do)\n\t${1:body}\n((( ${2:cond} ) f =) f while)\n$0" },
        { "while", "(((cond) f~ =) f~ while)" },
        { "for", "(((cond) f~ =) f~ for)\n\t${1:body}\n(next)\n$0" },
        { "for-loop", "(for-loop)" },
        { "next", "(next)" },
        { NULL, NULL }
    };
    int i;
    for (i = 0; snips[i].kw; i++)
        if (strcmp(snips[i].kw, kw) == 0)
            return snips[i].snippet;
    return NULL;
}
