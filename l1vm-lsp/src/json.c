/*
 * This file json.c is part of L1vm.
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


#define _POSIX_C_SOURCE 200809L

#include "json.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

JVal *j_new(JType t)
{
    JVal *v = calloc(1, sizeof(JVal));
    if (!v)
        return NULL;
    v->type = t;
    return v;
}

void j_free(JVal *v)
{
    size_t i;
    if (!v)
        return;
    switch (v->type) {
    case J_STR:
        free(v->u.str);
        break;
    case J_ARR:
        for (i = 0; i < v->u.arr.len; i++)
            j_free(v->u.arr.items[i]);
        free(v->u.arr.items);
        break;
    case J_OBJ:
        for (i = 0; i < v->u.obj.len; i++) {
            free(v->u.obj.keys[i]);
            j_free(v->u.obj.vals[i]);
        }
        free(v->u.obj.keys);
        free(v->u.obj.vals);
        break;
    default:
        break;
    }
    free(v);
}

int j_is(const JVal *v, JType t) { return v && v->type == t; }

const char *j_str(const JVal *v) { return (v && v->type == J_STR) ? v->u.str : NULL; }
double j_num(const JVal *v) { return (v && v->type == J_NUM) ? v->u.num : 0; }
int j_bool(const JVal *v) { return (v && v->type == J_BOOL) ? v->u.b : 0; }
size_t j_len(const JVal *a) { return (a && a->type == J_ARR) ? a->u.arr.len : 0; }

JVal *j_str_new(const char *s)
{
    JVal *v = j_new(J_STR);
    if (v)
        v->u.str = strdup(s ? s : "");
    return v;
}

JVal *j_num_new(double n)
{
    JVal *v = j_new(J_NUM);
    if (v)
        v->u.num = n;
    return v;
}

JVal *j_bool_new(int b)
{
    JVal *v = j_new(J_BOOL);
    if (v)
        v->u.b = !!b;
    return v;
}

JVal *j_arr_new(void) { return j_new(J_ARR); }
JVal *j_obj_new(void) { return j_new(J_OBJ); }

JVal *j_arr_push(JVal *a, JVal *item)
{
    if (!a || !item || a->type != J_ARR)
        return a;
    if (a->u.arr.len == a->u.arr.cap) {
        size_t ncap = a->u.arr.cap ? a->u.arr.cap * 2 : 8;
        JVal **ni = realloc(a->u.arr.items, ncap * sizeof(JVal *));
        if (!ni)
            return a;
        a->u.arr.items = ni;
        a->u.arr.cap = ncap;
    }
    a->u.arr.items[a->u.arr.len++] = item;
    return a;
}

const JVal *j_at(const JVal *a, size_t i)
{
    if (!a || a->type != J_ARR || i >= a->u.arr.len)
        return NULL;
    return a->u.arr.items[i];
}

JVal *j_obj_set(JVal *o, const char *key, JVal *val)
{
    size_t i;
    if (!o || !key || o->type != J_OBJ)
        return o;
    for (i = 0; i < o->u.obj.len; i++) {
        if (strcmp(o->u.obj.keys[i], key) == 0) {
            j_free(o->u.obj.vals[i]);
            o->u.obj.vals[i] = val;
            return o;
        }
    }
    if (o->u.obj.len == o->u.obj.cap) {
        size_t ncap = o->u.obj.cap ? o->u.obj.cap * 2 : 8;
        char **nk = realloc(o->u.obj.keys, ncap * sizeof(char *));
        JVal **nv = realloc(o->u.obj.vals, ncap * sizeof(JVal *));
        if (!nk || !nv)
            return o;
        o->u.obj.keys = nk;
        o->u.obj.vals = nv;
        o->u.obj.cap = ncap;
    }
    o->u.obj.keys[o->u.obj.len] = strdup(key);
    o->u.obj.vals[o->u.obj.len] = val;
    o->u.obj.len++;
    return o;
}

const JVal *j_get(const JVal *o, const char *key)
{
    size_t i;
    if (!o || o->type != J_OBJ || !key)
        return NULL;
    for (i = 0; i < o->u.obj.len; i++)
        if (strcmp(o->u.obj.keys[i], key) == 0)
            return o->u.obj.vals[i];
    return NULL;
}

/* ------------------------------------------------------------------ */
/* parser                                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *s;
    size_t len;
    size_t pos;
    int depth;
} JP;

static JVal *jp_parse_value(JP *p);

static void jp_ws(JP *p)
{
    while (p->pos < p->len && isspace((unsigned char)p->s[p->pos]))
        p->pos++;
}

static JVal *jp_parse_string(JP *p)
{
    SB b;
    char c;
    sb_init(&b);
    p->pos++; /* skip opening quote */
    while (p->pos < p->len) {
        c = p->s[p->pos++];
        if (c == '"') {
            JVal *v = j_str_new(sb_cstr(&b));
            sb_free(&b);
            return v;
        }
        if (c == '\\') {
            char e;
            if (p->pos >= p->len)
                break;
            e = p->s[p->pos++];
            switch (e) {
            case '"':  sb_addc(&b, '"'); break;
            case '\\': sb_addc(&b, '\\'); break;
            case '/':  sb_addc(&b, '/'); break;
            case 'b':  sb_addc(&b, '\b'); break;
            case 'f':  sb_addc(&b, '\f'); break;
            case 'n':  sb_addc(&b, '\n'); break;
            case 'r':  sb_addc(&b, '\r'); break;
            case 't':  sb_addc(&b, '\t'); break;
            case 'u': {
                unsigned cp = 0;
                int i;
                for (i = 0; i < 4; i++) {
                    char h;
                    if (p->pos >= p->len)
                        break;
                    h = p->s[p->pos++];
                    cp <<= 4;
                    if (h >= '0' && h <= '9') cp |= (unsigned)(h - '0');
                    else if (h >= 'a' && h <= 'f') cp |= (unsigned)(h - 'a' + 10);
                    else if (h >= 'A' && h <= 'F') cp |= (unsigned)(h - 'A' + 10);
                }
                if (cp >= 0xD800 && cp <= 0xDBFF &&
                    p->pos + 5 < p->len && p->s[p->pos] == '\\' &&
                    p->s[p->pos + 1] == 'u') {
                    unsigned lo = 0;
                    int j;
                    p->pos += 2;
                    for (j = 0; j < 4; j++) {
                        char h;
                        if (p->pos >= p->len)
                            break;
                        h = p->s[p->pos++];
                        lo <<= 4;
                        if (h >= '0' && h <= '9') lo |= (unsigned)(h - '0');
                        else if (h >= 'a' && h <= 'f') lo |= (unsigned)(h - 'a' + 10);
                        else if (h >= 'A' && h <= 'F') lo |= (unsigned)(h - 'A' + 10);
                    }
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                }
                if (cp < 0x80) sb_addc(&b, (char)cp);
                else if (cp < 0x800) {
                    sb_addc(&b, (char)(0xC0 | (cp >> 6)));
                    sb_addc(&b, (char)(0x80 | (cp & 0x3F)));
                } else if (cp < 0x10000) {
                    sb_addc(&b, (char)(0xE0 | (cp >> 12)));
                    sb_addc(&b, (char)(0x80 | ((cp >> 6) & 0x3F)));
                    sb_addc(&b, (char)(0x80 | (cp & 0x3F)));
                } else {
                    sb_addc(&b, (char)(0xF0 | (cp >> 18)));
                    sb_addc(&b, (char)(0x80 | ((cp >> 12) & 0x3F)));
                    sb_addc(&b, (char)(0x80 | ((cp >> 6) & 0x3F)));
                    sb_addc(&b, (char)(0x80 | (cp & 0x3F)));
                }
                break;
            }
            default:
                sb_addc(&b, e);
                break;
            }
        } else {
            sb_addc(&b, c);
        }
    }
    {
        JVal *v = j_str_new(sb_cstr(&b));
        sb_free(&b);
        return v;
    }
}

static JVal *jp_parse_number(JP *p)
{
    size_t start = p->pos;
    int isdbl = 0;
    char *tmp;
    JVal *v;

    while (p->pos < p->len) {
        char c = p->s[p->pos];
        if (isdigit((unsigned char)c)) {
            p->pos++;
        } else if (c == '-' || c == '+') {
            if (p->pos == start ||
                p->s[p->pos - 1] == 'e' || p->s[p->pos - 1] == 'E')
                p->pos++;
            else
                break;
        } else if (c == '.') {
            isdbl = 1;
            p->pos++;
        } else if (c == 'e' || c == 'E') {
            isdbl = 1;
            p->pos++;
        } else {
            break;
        }
    }
    tmp = malloc(p->pos - start + 1);
    if (!tmp)
        return j_new(J_NUM);
    memcpy(tmp, p->s + start, p->pos - start);
    tmp[p->pos - start] = '\0';
    if (isdbl)
        v = j_num_new(strtod(tmp, NULL));
    else
        v = j_num_new((double)strtoll(tmp, NULL, 10));
    free(tmp);
    return v;
}

static JVal *jp_parse_array(JP *p)
{
    JVal *a = j_arr_new();
    p->pos++;
    for (;;) {
        jp_ws(p);
        if (p->pos >= p->len)
            break;
        if (p->s[p->pos] == ']') {
            p->pos++;
            break;
        }
        if (p->s[p->pos] == ',') {
            p->pos++;
            continue;
        }
        j_arr_push(a, jp_parse_value(p));
    }
    return a;
}

static JVal *jp_parse_object(JP *p)
{
    JVal *o = j_obj_new();
    p->pos++;
    for (;;) {
        JVal *key;
        const char *ks;
        jp_ws(p);
        if (p->pos >= p->len)
            break;
        if (p->s[p->pos] == '}') {
            p->pos++;
            break;
        }
        if (p->s[p->pos] == ',') {
            p->pos++;
            continue;
        }
        if (p->s[p->pos] != '"')
            break;
        key = jp_parse_string(p);
        ks = j_str(key);
        jp_ws(p);
        if (p->pos < p->len && p->s[p->pos] == ':')
            p->pos++;
        jp_ws(p);
        if (ks)
            j_obj_set(o, ks, jp_parse_value(p));
        else
            j_free(key);
        j_free(key);
    }
    return o;
}

static JVal *jp_parse_value(JP *p)
{
    JVal *v = NULL;
    if (p->depth > 512)
        return j_new(J_NULL);
    p->depth++;
    jp_ws(p);
    if (p->pos >= p->len) {
        p->depth--;
        return j_new(J_NULL);
    }
    switch (p->s[p->pos]) {
    case '"':
        v = jp_parse_string(p);
        break;
    case '{':
        v = jp_parse_object(p);
        break;
    case '[':
        v = jp_parse_array(p);
        break;
    case 't':
        if (strncmp(p->s + p->pos, "true", 4) == 0) {
            v = j_bool_new(1);
            p->pos += 4;
        }
        break;
    case 'f':
        if (strncmp(p->s + p->pos, "false", 5) == 0) {
            v = j_bool_new(0);
            p->pos += 5;
        }
        break;
    case 'n':
        if (strncmp(p->s + p->pos, "null", 4) == 0) {
            v = j_new(J_NULL);
            p->pos += 4;
        }
        break;
    default:
        if (p->s[p->pos] == '-' || p->s[p->pos] == '+' ||
            isdigit((unsigned char)p->s[p->pos]))
            v = jp_parse_number(p);
        break;
    }
    p->depth--;
    return v ? v : j_new(J_NULL);
}

JVal *j_parse(const char *s)
{
    JP p;
    if (!s)
        return j_new(J_NULL);
    p.s = s;
    p.len = strlen(s);
    p.pos = 0;
    p.depth = 0;
    jp_ws(&p);
    if (p.len >= 3 && (unsigned char)p.s[0] == 0xEF &&
        (unsigned char)p.s[1] == 0xBB && (unsigned char)p.s[2] == 0xBF)
        p.pos = 3;
    return jp_parse_value(&p);
}

/* ------------------------------------------------------------------ */
/* emitter                                                             */
/* ------------------------------------------------------------------ */

void j_emit(const JVal *v, SB *out)
{
    size_t i;
    if (!v) {
        sb_add(out, "null");
        return;
    }
    switch (v->type) {
    case J_NULL:
        sb_add(out, "null");
        break;
    case J_BOOL:
        sb_add(out, v->u.b ? "true" : "false");
        break;
    case J_NUM: {
        double d = v->u.num;
        if (d == (double)(long long)d && d >= -9.0e18 && d <= 9.0e18)
            sb_printf(out, "%lld", (long long)d);
        else
            sb_printf(out, "%.17g", d);
        break;
    }
    case J_STR:
        sb_quote_json(out, v->u.str, strlen(v->u.str));
        break;
    case J_ARR:
        sb_addc(out, '[');
        for (i = 0; i < v->u.arr.len; i++) {
            if (i)
                sb_addc(out, ',');
            j_emit(v->u.arr.items[i], out);
        }
        sb_addc(out, ']');
        break;
    case J_OBJ:
        sb_addc(out, '{');
        for (i = 0; i < v->u.obj.len; i++) {
            if (i)
                sb_addc(out, ',');
            sb_quote_json(out, v->u.obj.keys[i], strlen(v->u.obj.keys[i]));
            sb_addc(out, ':');
            j_emit(v->u.obj.vals[i], out);
        }
        sb_addc(out, '}');
        break;
    }
}
