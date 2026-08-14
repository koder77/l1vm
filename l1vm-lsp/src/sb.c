/*
 * This file sb.c is part of L1vm.
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


#include "sb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sb_init(SB *b)
{
    b->buf = NULL;
    b->len = 0;
    b->cap = 0;
}

void sb_free(SB *b)
{
    free(b->buf);
    b->buf = NULL;
    b->len = b->cap = 0;
}

void sb_reset(SB *b)
{
    b->len = 0;
    if (b->buf)
        b->buf[0] = '\0';
}

static void sb_ensure(SB *b, size_t extra)
{
    if (b->len + extra + 1 > b->cap) {
        size_t ncap = b->cap ? b->cap * 2 : 128;
        char *nb;
        while (ncap < b->len + extra + 1)
            ncap *= 2;
        nb = realloc(b->buf, ncap);
        if (!nb)
            return;
        b->buf = nb;
        b->cap = ncap;
    }
}

void sb_addn(SB *b, const char *s, size_t n)
{
    if (!s || n == 0)
        return;
    sb_ensure(b, n);
    if (b->len + n + 1 > b->cap)
        return;
    memcpy(b->buf + b->len, s, n);
    b->len += n;
    b->buf[b->len] = '\0';
}

void sb_add(SB *b, const char *s)
{
    if (s)
        sb_addn(b, s, strlen(s));
}

void sb_addc(SB *b, char c)
{
    sb_ensure(b, 1);
    if (b->len + 2 > b->cap)
        return;
    b->buf[b->len++] = c;
    b->buf[b->len] = '\0';
}

void sb_printf(SB *b, const char *fmt, ...)
{
    va_list ap;
    int need;

    va_start(ap, fmt);
    need = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (need < 0)
        return;
    sb_ensure(b, (size_t)need);
    if (b->len + (size_t)need + 1 > b->cap)
        return;
    va_start(ap, fmt);
    vsnprintf(b->buf + b->len, b->cap - b->len, fmt, ap);
    va_end(ap);
    b->len += (size_t)need;
}

void sb_quote_json(SB *b, const char *s, size_t n)
{
    size_t i;
    sb_addc(b, '"');
    for (i = 0; i < n; i++) {
        unsigned char c = (unsigned char)s[i];
        switch (c) {
        case '"':  sb_add(b, "\\\""); break;
        case '\\': sb_add(b, "\\\\"); break;
        case '\b': sb_add(b, "\\b"); break;
        case '\f': sb_add(b, "\\f"); break;
        case '\n': sb_add(b, "\\n"); break;
        case '\r': sb_add(b, "\\r"); break;
        case '\t': sb_add(b, "\\t"); break;
        default:
            if (c < 0x20)
                sb_printf(b, "\\u%04x", c);
            else
                sb_addc(b, (char)c);
        }
    }
    sb_addc(b, '"');
}
