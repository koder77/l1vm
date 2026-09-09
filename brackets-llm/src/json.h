/*
 * This file json.h is part of L1vm.
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
 * l1vm-lsp - minimal JSON value model, parser and emitter
 * (no external dependencies, needed for JSON-RPC / LSP)
 */

#ifndef L1VM_JSON_H
#define L1VM_JSON_H

#include "sb.h"

typedef enum { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ } JType;

typedef struct JVal JVal;
struct JVal {
    JType type;
    union {
        int b;
        double num;
        char *str;
        struct { JVal **items; size_t len, cap; } arr;
        struct { char **keys; JVal **vals; size_t len, cap; } obj;
    } u;
};

JVal *j_new(JType t);
void j_free(JVal *v);
JVal *j_parse(const char *s);

JVal *j_str_new(const char *s);
JVal *j_num_new(double n);
JVal *j_bool_new(int b);
JVal *j_arr_new(void);
JVal *j_obj_new(void);

JVal *j_obj_set(JVal *o, const char *key, JVal *val);
JVal *j_arr_push(JVal *a, JVal *item);
const JVal *j_get(const JVal *o, const char *key);
const JVal *j_at(const JVal *a, size_t i);
size_t j_len(const JVal *a);
const char *j_str(const JVal *v);
double j_num(const JVal *v);
int j_bool(const JVal *v);
int j_is(const JVal *v, JType t);

void j_emit(const JVal *v, SB *out);

#endif
