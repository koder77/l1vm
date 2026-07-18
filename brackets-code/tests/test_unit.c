/*
 * test_unit.c - Unit tests for brackets-code utility functions
 * Compile: clang-19 -Wall -Wextra -o test_unit test_unit.c ../brackets-code.c ../dsl.c -lm
 * Run: ./test_unit
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Include the main header for type definitions and function declarations */
#include "../brackets-code.h"
#include "../dsl.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { \
    tests_run++; \
    printf("  TEST %d: %s ... ", tests_run, name); \
    fflush(stdout); \
} while(0)

#define PASS() do { tests_passed++; printf("OK\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)
#define ASSERT(cond) do { \
    if (!(cond)) { FAIL(#cond); return; } \
} while(0)

/* ── trim tests ─────────────────────────────────────────────────── */

static void test_trim_empty(void) {
    TEST("trim empty string");
    char s[] = "";
    trim(s);
    ASSERT(strcmp(s, "") == 0);
    PASS();
}

static void test_trim_whitespace(void) {
    TEST("trim leading/trailing whitespace");
    char s[] = "  hello world  ";
    trim(s);
    ASSERT(strcmp(s, "hello world") == 0);
    PASS();
}

static void test_trim_tabs(void) {
    TEST("trim tabs and newlines");
    char s[] = "\t\nhello\n\t";
    trim(s);
    ASSERT(strcmp(s, "hello") == 0);
    PASS();
}

static void test_trim_no_whitespace(void) {
    TEST("trim with no whitespace");
    char s[] = "hello";
    trim(s);
    ASSERT(strcmp(s, "hello") == 0);
    PASS();
}

/* ── to_lowercase tests ────────────────────────────────────────── */

static void test_lowercase_basic(void) {
    TEST("lowercase basic");
    char s[] = "HELLO World";
    to_lowercase(s);
    ASSERT(strcmp(s, "hello world") == 0);
    PASS();
}

static void test_lowercase_empty(void) {
    TEST("lowercase empty string");
    char s[] = "";
    to_lowercase(s);
    ASSERT(strcmp(s, "") == 0);
    PASS();
}

/* ── str_contains_word tests ───────────────────────────────────── */

static void test_contains_word_exact(void) {
    TEST("str_contains_word exact match");
    ASSERT(str_contains_word("hello world", "hello") == 1);
    PASS();
}

static void test_contains_word_no_match(void) {
    TEST("str_contains_word no match");
    ASSERT(str_contains_word("hello world", "xyz") == 0);
    PASS();
}

static void test_contains_word_partial(void) {
    TEST("str_contains_word partial word");
    ASSERT(str_contains_word("hello world", "hell") == 0);
    PASS();
}

static void test_contains_word_case_insensitive(void) {
    TEST("str_contains_word case insensitive");
    ASSERT(str_contains_word("Hello World", "hello") == 1);
    PASS();
}

/* ── prompt_to_filename tests ──────────────────────────────────── */

static void test_filename_basic(void) {
    TEST("prompt_to_filename basic");
    char out[256];
    prompt_to_filename("hello world", out, sizeof(out));
    ASSERT(strcmp(out, "hello_world.l1com") == 0);
    PASS();
}

static void test_filename_special_chars(void) {
    TEST("prompt_to_filename special chars");
    char out[256];
    prompt_to_filename("hello! @world#", out, sizeof(out));
    ASSERT(strcmp(out, "hello___world.l1com") == 0);
    PASS();
}

static void test_filename_leading_special(void) {
    TEST("prompt_to_filename leading special chars");
    char out[256];
    prompt_to_filename("!!!hello", out, sizeof(out));
    ASSERT(strcmp(out, "hello.l1com") == 0);
    PASS();
}

/* ── is_question tests ─────────────────────────────────────────── */

static void test_is_question_yes(void) {
    TEST("is_question with question mark");
    ASSERT(is_question("hello?") == 1);
    PASS();
}

static void test_is_question_what(void) {
    TEST("is_question with 'what'");
    ASSERT(is_question("what is this") == 1);
    PASS();
}

static void test_is_question_wie(void) {
    TEST("is_question with 'wie'");
    ASSERT(is_question("wie geht es") == 1);
    PASS();
}

static void test_is_question_no(void) {
    TEST("is_question with statement");
    ASSERT(is_question("hello world") == 0);
    PASS();
}

/* ── DSL token type tests ──────────────────────────────────────── */

static void test_dsl_token_int64(void) {
    TEST("dsl_token_type_from_string int64");
    ASSERT(dsl_token_type_from_string("int64") == DSL_TOKEN_INT64);
    PASS();
}

static void test_dsl_token_double(void) {
    TEST("dsl_token_type_from_string double");
    ASSERT(dsl_token_type_from_string("double") == DSL_TOKEN_DOUBLE);
    PASS();
}

static void test_dsl_token_string(void) {
    TEST("dsl_token_type_from_string string");
    ASSERT(dsl_token_type_from_string("string") == DSL_TOKEN_STRING);
    PASS();
}

static void test_dsl_token_const_int64(void) {
    TEST("dsl_token_type_from_string const-int64");
    ASSERT(dsl_token_type_from_string("const-int64") == DSL_TOKEN_CONST_INT64);
    PASS();
}

static void test_dsl_token_unknown(void) {
    TEST("dsl_token_type_from_string unknown");
    ASSERT(dsl_token_type_from_string("foobar") == DSL_TOKEN_UNKNOWN);
    PASS();
}

static void test_dsl_token_to_string(void) {
    TEST("dsl_token_type_to_string round-trip");
    ASSERT(strcmp(dsl_token_type_to_string(DSL_TOKEN_INT64), "int64") == 0);
    ASSERT(strcmp(dsl_token_type_to_string(DSL_TOKEN_DOUBLE), "double") == 0);
    ASSERT(strcmp(dsl_token_type_to_string(DSL_TOKEN_STRING), "string") == 0);
    PASS();
}

/* ── Program init/free tests ───────────────────────────────────── */

static void test_program_init_free(void) {
    TEST("init_program and free_program");
    Program prog;
    ASSERT(init_program(&prog) == 1);
    ASSERT(prog.num_funcs == 0);
    ASSERT(prog.num_includes == 0);
    ASSERT(prog.funcs != NULL);
    ASSERT(prog.includes != NULL);
    ASSERT(prog.includes_post != NULL);
    free_program(&prog);
    ASSERT(prog.funcs == NULL);
    ASSERT(prog.includes == NULL);
    PASS();
}

/* ── Function tests ────────────────────────────────────────────── */

static void test_add_func(void) {
    TEST("add_func");
    Program prog;
    init_program(&prog);
    add_func(&prog, "main");
    ASSERT(prog.num_funcs == 1);
    ASSERT(strcmp(prog.funcs[0].name, "main") == 0);
    free_program(&prog);
    PASS();
}

static void test_add_include(void) {
    TEST("add_include");
    Program prog;
    init_program(&prog);
    add_include(&prog, "intr-func.l1h");
    ASSERT(prog.num_includes == 1);
    ASSERT(strcmp(prog.includes[0], "intr-func.l1h") == 0);
    /* Add duplicate - should not increase count */
    add_include(&prog, "intr-func.l1h");
    ASSERT(prog.num_includes == 1);
    free_program(&prog);
    PASS();
}

static void test_add_var_to_func(void) {
    TEST("add_var_to_func");
    Program prog;
    init_program(&prog);
    add_func(&prog, "main");
    Function *f = &prog.funcs[0];
    const char *vals[] = {"42"};
    add_var_to_func(f, "int64", "x", 1, vals, 1);
    ASSERT(f->num_vars == 1);
    ASSERT(strcmp(f->vars[0].name, "x") == 0);
    ASSERT(strcmp(f->vars[0].type, "int64") == 0);
    ASSERT(strcmp(f->vars[0].values[0], "42") == 0);
    free_program(&prog);
    PASS();
}

static void test_func_append(void) {
    TEST("func_append");
    Function f;
    init_function(&f);
    func_append(&f, "\t(x :=)");
    ASSERT(strstr(f.body, "(x :=)") != NULL);
    free_function(&f);
    PASS();
}

/* ── has_word tests ────────────────────────────────────────────── */

static void test_has_word_basic(void) {
    TEST("has_word basic");
    ASSERT(has_word("hello world", "hello") == 1);
    PASS();
}

static void test_has_word_not_found(void) {
    TEST("has_word not found");
    ASSERT(has_word("hello world", "xyz") == 0);
    PASS();
}

/* ── has_sequential_pattern tests ─────────────────────────────── */

static void test_seq_numbered_steps(void) {
    TEST("has_sequential_pattern numbered steps");
    ASSERT(has_sequential_pattern("1) do this 2) do that") == 1);
    PASS();
}

static void test_seq_bullet_points(void) {
    TEST("has_sequential_pattern bullet points");
    ASSERT(has_sequential_pattern("- first task\n- second task") == 1);
    PASS();
}

static void test_seq_english_adverbs(void) {
    TEST("has_sequential_pattern English adverbs (first...then)");
    ASSERT(has_sequential_pattern("first calculate sum then print result") == 1);
    PASS();
}

static void test_seq_german_adverbs(void) {
    TEST("has_sequential_pattern German adverbs (zuerst...dann)");
    ASSERT(has_sequential_pattern("zuerst sortiere dann drucke ab") == 1);
    PASS();
}

static void test_seq_compound_und_dann(void) {
    TEST("has_sequential_pattern compound 'und dann'");
    ASSERT(has_sequential_pattern("sort 5 numbers und dann reverse array") == 1);
    PASS();
}

static void test_seq_compound_und_danach(void) {
    TEST("has_sequential_pattern compound 'und danach'");
    ASSERT(has_sequential_pattern("sort 5 numbers und danach sort descending") == 1);
    PASS();
}

static void test_seq_compound_and_then(void) {
    TEST("has_sequential_pattern compound 'and then'");
    ASSERT(has_sequential_pattern("calculate factorial and then print result") == 1);
    PASS();
}

static void test_seq_compound_und_anschlussend(void) {
    TEST("has_sequential_pattern compound 'und anschließend'");
    ASSERT(has_sequential_pattern("zähle bis 10 und anschließend faktorielle") == 1);
    PASS();
}

static void test_seq_single_word_no_trigger(void) {
    TEST("has_sequential_pattern single word does not trigger");
    ASSERT(has_sequential_pattern("sort 5 numbers") == 0);
    PASS();
}

static void test_seq_empty_string(void) {
    TEST("has_sequential_pattern empty string");
    ASSERT(has_sequential_pattern("") == 0);
    PASS();
}

/* ── Main ──────────────────────────────────────────────────────── */

int main(void) {
    printf("========================================\n");
    printf("  brackets-code unit tests\n");
    printf("========================================\n\n");

    printf("trim tests:\n");
    test_trim_empty();
    test_trim_whitespace();
    test_trim_tabs();
    test_trim_no_whitespace();

    printf("\nto_lowercase tests:\n");
    test_lowercase_basic();
    test_lowercase_empty();

    printf("\nstr_contains_word tests:\n");
    test_contains_word_exact();
    test_contains_word_no_match();
    test_contains_word_partial();
    test_contains_word_case_insensitive();

    printf("\nprompt_to_filename tests:\n");
    test_filename_basic();
    test_filename_special_chars();
    test_filename_leading_special();

    printf("\nis_question tests:\n");
    test_is_question_yes();
    test_is_question_what();
    test_is_question_wie();
    test_is_question_no();

    printf("\nDSL token type tests:\n");
    test_dsl_token_int64();
    test_dsl_token_double();
    test_dsl_token_string();
    test_dsl_token_const_int64();
    test_dsl_token_unknown();
    test_dsl_token_to_string();

    printf("\nProgram management tests:\n");
    test_program_init_free();
    test_add_func();
    test_add_include();
    test_add_var_to_func();
    test_func_append();

    printf("\nhas_word tests:\n");
    test_has_word_basic();
    test_has_word_not_found();

    printf("\nhas_sequential_pattern tests:\n");
    test_seq_numbered_steps();
    test_seq_bullet_points();
    test_seq_english_adverbs();
    test_seq_german_adverbs();
    test_seq_compound_und_dann();
    test_seq_compound_und_danach();
    test_seq_compound_and_then();
    test_seq_compound_und_anschlussend();
    test_seq_single_word_no_trigger();
    test_seq_empty_string();

    printf("\n========================================\n");
    printf("  Results: %d/%d passed\n", tests_passed, tests_run);
    printf("========================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
