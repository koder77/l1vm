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
