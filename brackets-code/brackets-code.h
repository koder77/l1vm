/*
 * This file brackets-code.h is part of L1vm.
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

#ifndef BRACKETS_CODE_H
#define BRACKETS_CODE_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <math.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <spawn.h>

#define VERSION_TXT "0.6.7"

#define MAX_LINE 4096
#define MAX_VARS 48
#define MAX_FUNCS 24
#define MAX_PROMPT 8192
#define MAX_CODE 65536
#define MAX_OUTPUT 4096
#define MAX_VALUES 16

#define EMBED_DIM 32
#define VOCAB_SIZE 72
#define TEMPERATURE 0.8
#define MAX_STEPS 32
#define NUM_EMITTERS 166

#define MAX_LEARNED 4096
#define LEARNED_DIR ".brackets-code/learned"

#define MAX_EXAMPLES 512
#define MAX_TOP_K 10
#define EXAMPLE_DIR "l1vm-example-code"
#define EXAMPLE_SUBDIRS 3
#define EXAMPLE_EXTS 3

#define MAX_NUMS 8
#define INIT_VARS_CAP 8
#define INIT_BODY_CAP 4096
#define INIT_FUNCS_CAP 4
#define INIT_INCLUDES_CAP 4

#define ANSI_RESET   "\033[0m"
#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_BOLD    "\033[1m"

#ifdef HAVE_READLINE
#define Function L1vmFunction
#include <readline/readline.h>
#include <readline/history.h>
#undef Function
#endif

#define c_printf(color, ...) do { if (use_color) { printf(color); printf(__VA_ARGS__); printf(ANSI_RESET); } else printf(__VA_ARGS__); } while(0)

#define SNPRINTF_CHECK(buf, sz, ...) ({ \
    int _snr = snprintf(buf, sz, __VA_ARGS__); \
    if (_snr < 0 || _snr >= (int)(sz)) \
        fprintf(stderr, "snprintf trunc/error at %s:%d\n", __FILE__, __LINE__); \
    _snr; \
})

// ==================== DATA STRUCTURES ====================

typedef struct {
    char name[256];
    char type[32];
    int count;
    char values[MAX_VALUES][256];
    int num_values;
} Variable;

typedef struct {
    char name[256];
    int is_local;
    int has_vars;
    int has_vardef;
    char vardef_name[256];
    Variable *vars;
    int num_vars;
    int vars_cap;
    char *body;
    int body_cap;
} Function;

typedef struct {
    char filename[256];
    Function *funcs;
    int num_funcs;
    int funcs_cap;
    char (*includes)[256];
    int num_includes;
    int includes_cap;
    char (*includes_post)[256];
    int num_includes_post;
    int includes_post_cap;
    char globals[MAX_CODE];
} Program;

typedef struct {
    char id[64];
    char keywords[512];
    char description[256];
    char source_path[1024];
    int is_learned;
    int num_includes;
    char (*includes)[256];
    int includes_cap;
    int num_funcs;
    Function *funcs;
    int funcs_cap;
    char globals[MAX_CODE];
} LearnedPattern;

typedef struct {
    int has_input;
    int input_count;
    int count_value;
    int has_literals;
    int literals[MAX_NUMS];
    double double_literals[MAX_NUMS];
    int num_literals;
    int has_operation;
    char op[16];
    int has_add;
    int has_sub;
    int has_mul;
    int has_div;
    int has_algorithm;
    char algorithm[32];
    int algo_param;
    int has_loop;
    int loop_start, loop_end;
    int has_condition;
    int has_output;
    int has_sort;
    int has_power;
    int has_max;
    int has_min;
    int has_descending;
    int has_gcd;
    int has_countdown;
    int has_mult_table;
    int has_guess;
    int has_random;
    int has_hello_name;
    int has_time;
    int has_pointer;
    int has_struct;
    int has_hex_binary;
    int has_shell_args;
    int has_array;
    int has_function;
    int has_average;
    int has_fizzbuzz;
    int has_even_odd;
    int has_primes;
    int has_sum;
    int has_factorial;
    int has_fibonacci;
    int has_median;
    int median_count;
    int has_string_cat;
    int has_string_compare;
    int has_array_assign;
    int has_array_reverse;
    int has_array_find;
    int has_sum_range;
    int sum_range_n;
    int has_print_even;
    int print_even_n;
    int has_find_max;
    int find_max_count;
    int has_fib_seq;
    int fib_seq_n;
    int has_input_sort;
    int input_sort_count;
    int has_input_fact;
    int has_countdown_from;
    int countdown_start;
    int has_read_file;
    int has_write_file;
    int has_array_vmath;
    int has_string_to_num;
    int has_timer;
    int has_array_min_max;
    int has_bool_demo;
    int has_print_var;
    int has_bit_check;
    int has_leap_year;
    int has_temp_convert;
    int has_circle_area;
    int has_fann_create;
    int has_fann_train;
    int has_fann_run;
    int has_palindrome;
    int has_lcm;
    int has_collatz;
    int has_sum_of_digits;
    int has_reverse_string;
    int has_armstrong;
    int has_perfect_number;
    int has_count_vowels;
    int has_anagram_check;
    int has_string_to_upper;
    int has_string_to_lower;
    int has_caesar_cipher;
    int has_palindrome_string;
    int has_bubble_sort;
    int has_binary_search;
    int has_square_root;
    int has_prime_factorization;
    int has_standard_deviation;
    int has_compound_interest;
    int has_decimal_to_binary;
    int has_dice_roll;
    int has_double_math;
    int has_double_circle_area;
    int has_double_average;
    int has_double_compound_interest;
    int has_double_pythagoras;
    int has_double_temp_convert;
    int has_double_sqrt;
    int has_double_power;
    int has_double_volume_sphere;
    int has_double_discount;
    int has_double_simple_interest;
    int has_double_bmi;
    int has_double_standard_deviation;
    int has_double_kinetic_energy;
    int has_string_length;
    int has_stack;
    int has_queue;
    int has_insertion_sort;
    int has_calculator;
    int has_unit_converter;
    int has_rock_paper_scissors;
    int has_pyramid;
    int has_temp_converter_menu;
    int has_sort_stats;
    int has_string_analyzer;
    int has_number_analyzer;
    int has_filter_numbers;
    int has_random_generator;
    int has_math_menu;
    int has_quiz_game;
    int has_bmi_calculator;
    int has_statistics_suite;
    int has_linked_list;
    int has_binary_search_tree;
    int has_tree_traversal;
    int has_graph_bfs_dfs;
    int has_n_queens;
    int has_sudoku;
    int has_levenshtein;
    int has_maze_generator;
    int has_maze_solver;
    int has_monte_carlo;
    int has_matrix_mul;
    int has_matrix_transpose;
    int has_numerical_integration;
    int has_complex_numbers;
    int has_linear_regression;
    int has_base_converter;
    int has_freq_analysis;
    int has_shuffle;
    int has_weighted_random;
    int has_ascii_table;
    int has_bignum_math;
    int has_password_card;
    int has_chess_problem;
    int has_shell_repl;
    int has_webserver;
    int has_sdl_window;
    int has_sdl_button;
    int has_thread;
    int has_scheduler;
    int has_shell_exec;
    int has_json;
    int has_crypto;
    int has_bluetooth_ble;
    int has_serial_rs232;
    int has_gpio;
    int has_gps;
    int has_timer_date;
    int has_sdl_sound;
    int has_sdl_joystick;
    int has_sdl_mouse;
    int has_fractal;
    int has_cluster_3x1;
    int has_reload;
    int has_coordinate_grid;
    int has_turmite;
    int has_crossword;
    int has_linter;
    int has_hello_world;
    int has_string_find;
    int has_string_split;
    int has_switch_demo;
    int has_type_convert;
    int has_iterative_factorial;
    int has_random_walk;
    int has_bar_chart;
    int has_hanoi_tower;
    int has_ascii_art;
    int has_number_to_words;
    int has_temperature_table;
    int has_loop_demo;
    int has_array_access;
    int has_array_write;
    int has_array_iterate;
    int has_array_sum;
    int has_array_average;
    int has_double_array_access;
    int has_double_array_write;
    int has_double_array_iterate;
    int has_double_array_sum;
    int has_double_array_min_max;
    int has_double_array_reverse;
    int has_double_array_average;
    int suppress_output;
    char result_var[64];
    int skip_input;
    char prompt[MAX_PROMPT];
    char inherit_var[256];
    int inherit_count;
    char inherit_var_names[64][256];
    char inherit_var_types[64][64];
    int inherit_var_counts[64];
    int num_inherit_vars;
    int extra_emitters[32];
    int num_extra_emitters;
    char op_seq[32][64];
    int op_seq_literals[32];
    int num_ops;
    char type[16];
    char title[256];
} TaskProfile;

typedef struct {
    char word[256];
    char canonical[256];
} Synonym;

typedef struct {
    char word[32];
    float embed[EMBED_DIM];
} WordEmbedding;

typedef struct {
    char filename[1024];
    char stem[512];
    float embedding[EMBED_DIM];
    float score;
} ExampleDoc;

typedef struct {
    char *keywords;
    char *desc;
    void (*gen)(Program*, const char*);
    const char *dsl_keyword;  // if set, use gen_from_dsl_keyword instead of gen
} Template;

// ==================== GLOBAL VARIABLES ====================

extern int validate_flag;
extern int retry_seed;
extern int verbose_flag;
extern int dry_run_flag;
extern int dataflow_quiet_mode;
extern char out_dir[512];
extern char l1vm_root[512];
extern int use_color;

extern LearnedPattern learned_patterns[MAX_LEARNED];
extern int num_learned;
extern int learned_loaded;

extern WordEmbedding word_embeddings[VOCAB_SIZE];
extern float attention_weights[NUM_EMITTERS];
extern const char *EMITTER_NAMES[NUM_EMITTERS];
extern int vs_boost_tokens[64];
extern int vs_boost_count;
extern float idf_weights[VOCAB_SIZE];

extern const char *vocab[VOCAB_SIZE];
extern ExampleDoc example_docs[MAX_EXAMPLES];
extern int num_examples;
extern int examples_indexed;

extern Template templates[];
extern int num_templates;

// ==================== FUNCTION DECLARATIONS ====================

// Utility
void trim(char *s);
void to_lowercase(char *s);

// Program management
int init_program(Program *prog);
void free_program(Program *prog);
void add_include(Program *prog, const char *inc);
void add_include_post(Program *prog, const char *inc);
void add_func(Program *prog, const char *name);
void add_var_to_func(Function *f, const char *type, const char *name, int count, const char **values, int num_values);
void func_append(Function *f, const char *line);
void func_vardef(Function *f, const char *scope);
int init_function(Function *f);
int ensure_vars_cap(Function *f, int needed);
int ensure_body_cap(Function *f, int needed);
int ensure_funcs_cap(Function **pfuncs, int *cap, int needed);
int ensure_includes_cap(char (**pinc)[256], int *cap, int needed);
void free_function(Function *f);

// Synonyms
void load_synonyms(void);
const char* resolve_synonym(const char *word);
int has_word_fuzzy(const char *text, const char *keyword);
void expand_query(const char *query, char *expanded, int max_len);

// Prompt parsing
int prompt_to_filename(const char *prompt, char *out, int max);
int parse_task(const char *prompt, TaskProfile *task);
int split_prompt_steps(const char *prompt, char steps[MAX_STEPS][MAX_PROMPT]);
int has_sequential_pattern(const char *text);

// Embeddings & vector search
void init_embeddings(void);
int tokenize(const char *text, int *tokens, int max_tokens);
void embed_text(const char *text, float *out);
float cosine_sim(const float *a, const float *b);
void index_examples(void);
int search_examples(const char *query, int top_k, int *indices, float *scores);

// Emitter selection
int llm_select_emitter(const char *prompt, TaskProfile *task);
int has_word(const char *prompt, const char *word);
int str_contains_word(const char *str, const char *word);

// Code generation
int smart_generate(Program *prog, const char *prompt, char *desc, int desc_size);
int generate_from_task(Program *prog, TaskProfile *task, int last_step);
int match_template(const char *prompt, int *best_score);
int generate_code(const char *prompt, const char *filename);
int validate_code(const char *filename);

// Learned patterns
int init_learned_pattern(LearnedPattern *lp);
void free_learned_pattern(LearnedPattern *lp);
char* learned_dir_path(char *buf, int bufsize);
void ensure_learned_dir(void);
int learn_from_file(const char *path, const char *keywords, const char *description);
int save_learned_pattern(LearnedPattern *lp);
void load_learned_patterns(void);
int match_learned_pattern(const char *prompt, int *best_score);
int emit_learned_pattern(Program *prog, int learned_idx);
int emit_learned_step(Program *prog, int learned_idx);
int has_learned_id(const char *id);
int forget_learned(const char *id);
void list_learned(void);

// Emitters
void emit_math(Program *prog, Function *f, const char *type, const char *op, const int *vals, int n, int last_step);
void emit_input_loop(Program *prog, Function *f, int count, const char *type, const char *op);
void emit_for_sum(Program *prog, Function *f, int n);
void emit_print_even(Program *prog, Function *f, int n);
void emit_input_find_max(Program *prog, Function *f, int count);
void emit_countdown_from(Program *prog, Function *f, int start);
void emit_fib_seq(Program *prog, Function *f, int n);
void emit_input_sort(Program *prog, Function *f, int count, int skip_input, int descending, const char *type);
void emit_median(Program *prog, Function *f, int count, int skip_input);
void emit_string_cat(Program *prog, Function *f);
void emit_string_compare(Program *prog, Function *f);
void emit_array_assign(Program *prog, Function *f);
void emit_array_reverse(Program *prog, Function *f, int skip_input, int array_count);
void emit_array_find(Program *prog, Function *f, int skip_input);
void emit_input_factorial(Program *prog, Function *f);
void emit_array_vmath(Program *prog, Function *f, int skip_input);
void emit_read_file(Program *prog, Function *f);
void emit_write_file(Program *prog, Function *f);
void emit_string_to_num(Program *prog, Function *f);
void emit_timer(Program *prog, Function *f);
void emit_factorial(Program *prog, Function *f);
void emit_fizzbuzz(Program *prog, Function *f);
void emit_primes(Program *prog, Function *f, int max_val);
void emit_even_odd(Program *prog, Function *f);
void emit_power(Program *prog, Function *f);
void emit_multiplication_table(Program *prog, Function *f);
void emit_guess_number(Program *prog, Function *f);
void emit_gcd(Program *prog, Function *f);
void emit_random_number(Program *prog, Function *f);
void emit_hello_name(Program *prog, Function *f);
void emit_array_min_max(Program *prog, Function *f);
void emit_bool_demo(Program *prog, Function *f);
void emit_bit_check(Program *prog, Function *f);
void emit_leap_year(Program *prog, Function *f);
void emit_temp_convert(Program *prog, Function *f);
void emit_circle_area(Program *prog, Function *f);
void emit_average(Program *prog, Function *f, int skip_input);
void emit_selection_sort(Program *prog, Function *f, int count, int skip_input);
void emit_fann_create(Program *prog, Function *f);
void emit_fann_train(Program *prog, Function *f);
void emit_fann_run(Program *prog, Function *f);
void emit_palindrome(Program *prog, Function *f);
void emit_lcm(Program *prog, Function *f);
void emit_collatz(Program *prog, Function *f);
void emit_sum_of_digits(Program *prog, Function *f);
void emit_function(Program *prog, Function *f);
void emit_reverse_string(Program *prog, Function *f);
void emit_armstrong(Program *prog, Function *f);
void emit_perfect_number(Program *prog, Function *f);
void emit_count_vowels(Program *prog, Function *f);
void emit_anagram_check(Program *prog, Function *f);
void emit_string_to_upper(Program *prog, Function *f);
void emit_string_to_lower(Program *prog, Function *f);
void emit_caesar_cipher(Program *prog, Function *f);
void emit_palindrome_string(Program *prog, Function *f);
void emit_binary_search(Program *prog, Function *f);
void emit_square_root(Program *prog, Function *f);
void emit_prime_factorization(Program *prog, Function *f);
void emit_standard_deviation(Program *prog, Function *f, int skip_input);
void emit_compound_interest(Program *prog, Function *f);
void emit_decimal_to_binary(Program *prog, Function *f);
void emit_dice_roll(Program *prog, Function *f);
void emit_double_math(Program *prog, Function *f, const char *op);
void emit_double_circle_area(Program *prog, Function *f);
void emit_double_average(Program *prog, Function *f, int skip_input);
void emit_double_compound_interest(Program *prog, Function *f);
void emit_double_pythagoras(Program *prog, Function *f);
void emit_double_temp_convert(Program *prog, Function *f);
void emit_double_sqrt(Program *prog, Function *f);
void emit_double_power(Program *prog, Function *f);
void emit_double_volume_sphere(Program *prog, Function *f);
void emit_double_discount(Program *prog, Function *f);
void emit_double_simple_interest(Program *prog, Function *f);
void emit_double_bmi(Program *prog, Function *f);
void emit_double_standard_deviation(Program *prog, Function *f, int skip_input);
void emit_double_kinetic_energy(Program *prog, Function *f);
void emit_add(Program *prog, Function *f);
void emit_sub(Program *prog, Function *f);
void emit_mul(Program *prog, Function *f);
void emit_div(Program *prog, Function *f);
void emit_double_add(Program *prog, Function *f);
void emit_double_sub(Program *prog, Function *f);
void emit_double_mul(Program *prog, Function *f);
void emit_double_div(Program *prog, Function *f);
void emit_hello_world(Program *prog, Function *f);
void emit_string_find(Program *prog, Function *f);
void emit_string_split(Program *prog, Function *f);
void emit_switch_demo(Program *prog, Function *f);
void emit_type_convert(Program *prog, Function *f);
void emit_iterative_factorial(Program *prog, Function *f);
void emit_random_walk(Program *prog, Function *f);
void emit_bar_chart(Program *prog, Function *f);
void emit_hanoi_tower(Program *prog, Function *f);
void emit_ascii_art(Program *prog, Function *f);
void emit_number_to_words(Program *prog, Function *f);
void emit_temperature_table(Program *prog, Function *f);
void emit_loop_demo(Program *prog, Function *f);
void emit_pointer_demo(Program *prog, Function *f);
void emit_struct_demo(Program *prog, Function *f);
void emit_hex_binary(Program *prog, Function *f);
void emit_shell_args(Program *prog, Function *f);
void emit_time_demo(Program *prog, Function *f);
void emit_string_length(Program *prog, Function *f);
void emit_stack(Program *prog, Function *f);
void emit_queue(Program *prog, Function *f);
void emit_insertion_sort(Program *prog, Function *f, int count, int skip_input);
void emit_calculator(Program *prog, Function *f);
void emit_unit_converter(Program *prog, Function *f);
void emit_rock_paper_scissors(Program *prog, Function *f);
void emit_pyramid(Program *prog, Function *f);
void emit_temp_converter_menu(Program *prog, Function *f);
void emit_sort_stats(Program *prog, Function *f);
void emit_string_analyzer(Program *prog, Function *f);
void emit_number_analyzer(Program *prog, Function *f);
void emit_filter_numbers(Program *prog, Function *f);
void emit_random_generator(Program *prog, Function *f);
void emit_math_menu(Program *prog, Function *f);
void emit_quiz_game(Program *prog, Function *f);
void emit_bmi_calculator(Program *prog, Function *f);
void emit_statistics_suite(Program *prog, Function *f);
void emit_linked_list(Program *prog, Function *f);
void emit_binary_search_tree(Program *prog, Function *f);
void emit_tree_traversal(Program *prog, Function *f);
void emit_graph_bfs_dfs(Program *prog, Function *f);
void emit_n_queens(Program *prog, Function *f);
void emit_sudoku(Program *prog, Function *f);
void emit_levenshtein_distance(Program *prog, Function *f);
void emit_maze_generator(Program *prog, Function *f);
void emit_maze_solver(Program *prog, Function *f);
void emit_monte_carlo_pi(Program *prog, Function *f);
void emit_matrix_multiplication(Program *prog, Function *f);
void emit_matrix_transpose(Program *prog, Function *f);
void emit_numerical_integration(Program *prog, Function *f);
void emit_complex_numbers(Program *prog, Function *f);
void emit_linear_regression(Program *prog, Function *f);
void emit_base_converter(Program *prog, Function *f);
void emit_freq_analysis(Program *prog, Function *f);
void emit_shuffle(Program *prog, Function *f);
void emit_weighted_random(Program *prog, Function *f);
void emit_ascii_table(Program *prog, Function *f);
void emit_bignum_math(Program *prog, Function *f);
void emit_password_card(Program *prog, Function *f);
void emit_chess_problem(Program *prog, Function *f);
void emit_shell_repl(Program *prog, Function *f);

// Template generators
void gen_from_dsl_keyword(Program *prog, const char *dsl_keyword);
int dsl_apply_to_func(Program *prog, Function *f, const char *keyword);

// CLI
int self_test(void);
void show_help(void);
void interactive_mode(void);
void prepend_out_dir(const char *fname, char *buf, int bufsize);
void write_program(Program *prog, const char *filename);
int is_question(const char *prompt);
void answer_question(const char *prompt);

#endif
