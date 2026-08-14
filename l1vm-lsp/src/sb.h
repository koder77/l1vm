/*
 * l1vm-lsp - Language Server Protocol server for the Brackets language (L1VM)
 * String builder
 */

#ifndef L1VM_SB_H
#define L1VM_SB_H

#include <stddef.h>
#include <stdarg.h>

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
} SB;

void sb_init(SB *b);
void sb_free(SB *b);
void sb_reset(SB *b);
void sb_addn(SB *b, const char *s, size_t n);
void sb_add(SB *b, const char *s);
void sb_addc(SB *b, char c);
void sb_printf(SB *b, const char *fmt, ...);
void sb_quote_json(SB *b, const char *s, size_t n);

static inline const char *sb_cstr(const SB *b) { return b->buf ? b->buf : ""; }
static inline size_t sb_len(const SB *b) { return b->len; }

#endif
