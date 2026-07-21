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
#include <stdint.h>
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

#define PATH_BUF_SIZE 512
#define WORD_BUF_SIZE 256
#define FULLPATH_BUF_SIZE 1024
#define DEFAULT_ELEMENT_COUNT 5
#define EMBED_DIM 32
#define VOCAB_SIZE 72
#define TEMPERATURE 0.8
#define MAX_STEPS 32
#define NUM_EMITTERS 166
#define MAX_LEARNED 4096
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
#define LEARNED_DIR ".brackets-code/learned"

#define ANSI_RESET   "\033[0m"
#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_YELLOW  "\033[33m"
#define ANSI_CYAN    "\033[36m"
#define ANSI_BOLD    "\033[1m"

#define c_printf(color, ...) do { if (use_color) { printf(color); printf(__VA_ARGS__); printf(ANSI_RESET); } else printf(__VA_ARGS__); } while(0)

#define SNPRINTF_CHECK(buf, size, ...) snprintf(buf, size, __VA_ARGS__)

#define L1vmFunctionf L1vmFunction *f
#define L1vmFunctionfn L1vmFunction *fn
#define L1vmFunctionfcur L1vmFunction *fcur

#define PATTERN_MAX_KEYWORDS 4
#define PATTERN_MAX_EXCLUDES 4

typedef struct {
    char word[40];
    char canonical[40];
} Synonym;

typedef struct {
    char name[64];
    char type[32];
    int count;
    int num_values;
    char values[MAX_VALUES][64];
} Variable;

typedef struct {
    char name[64];
    Variable *vars;
    int vars_cap;
    int num_vars;
    char *body;
    int body_cap;
    int body_num;
    int is_local;
    int has_vars;
    int has_vardef;
    char vardef_name[64];
    char scope[64];
} L1vmFunction;

typedef struct {
    L1vmFunction *funcs;
    int funcs_cap;
    int num_funcs;
    char (*includes)[256];
    int includes_cap;
    int num_includes;
    char (*includes_post)[256];
    int includes_post_cap;
    int num_includes_post;
    char globals[MAX_CODE];
    char filename[512];
} Program;

typedef struct {
    char id[64];
    char source_path[512];
    char keywords[1024];
    char description[512];
    char (*includes)[256];
    int includes_cap;
    int num_includes;
    L1vmFunction *funcs;
    int funcs_cap;
    int num_funcs;
    char globals[MAX_CODE];
    int is_learned;
    float confidence;
    int match_count;
} LearnedPattern;

#define TF_WORDS 4

typedef struct {
    char prompt[MAX_PROMPT];
    char type[32];
    char title[MAX_PROMPT];
    uint64_t flags[TF_WORDS];
    int literals[MAX_NUMS];
    int num_literals;
    double double_literals[MAX_NUMS];
    int input_count;
    char op[64];
    int num_ops;
    char op_seq[MAX_NUMS][64];
    int op_seq_literals[MAX_NUMS];
    int sum_range_n;
    int print_even_n;
    int find_max_count;
    int fib_seq_n;
    int countdown_start;
    int input_sort_count;
    int median_count;
    char inherit_var[256];
    int inherit_count;
    int skip_input;
    int suppress_output;
    char inherit_var_names[64][WORD_BUF_SIZE];
    char inherit_var_types[64][64];
    int inherit_var_counts[64];
    int num_inherit_vars;
    int num_extra_emitters;
    int extra_emitters[32];
} TaskProfile;

typedef enum {
    FLAG_literals = 0,
    FLAG_input,
    FLAG_output,
    FLAG_operation,
    FLAG_add,
    FLAG_sub,
    FLAG_mul,
    FLAG_div,
    FLAG_loop,
    FLAG_condition,
    FLAG_sort,
    FLAG_descending,
    FLAG_power,
    FLAG_max,
    FLAG_min,
    FLAG_countdown,
    FLAG_countdown_from,
    FLAG_guess,
    FLAG_random,
    FLAG_pointer,
    FLAG_struct,
    FLAG_function,
    FLAG_hex_binary,
    FLAG_average,
    FLAG_fizzbuzz,
    FLAG_primes,
    FLAG_sum,
    FLAG_sum_range,
    FLAG_factorial,
    FLAG_fibonacci,
    FLAG_fib_seq,
    FLAG_median,
    FLAG_string_compare,
    FLAG_timer,
    FLAG_even_odd,
    FLAG_bignum_math,
    FLAG_gcd,
    FLAG_mult_table,
    FLAG_string_cat,
    FLAG_read_file,
    FLAG_write_file,
    FLAG_string_to_num,
    FLAG_bool_demo,
    FLAG_bit_check,
    FLAG_leap_year,
    FLAG_temp_convert,
    FLAG_temp_converter_menu,
    FLAG_circle_area,
    FLAG_palindrome,
    FLAG_lcm,
    FLAG_collatz,
    FLAG_sum_of_digits,
    FLAG_reverse_string,
    FLAG_armstrong,
    FLAG_perfect_number,
    FLAG_count_vowels,
    FLAG_anagram_check,
    FLAG_string_to_upper,
    FLAG_string_to_lower,
    FLAG_caesar_cipher,
    FLAG_palindrome_string,
    FLAG_bubble_sort,
    FLAG_binary_search,
    FLAG_square_root,
    FLAG_prime_factorization,
    FLAG_compound_interest,
    FLAG_decimal_to_binary,
    FLAG_dice_roll,
    FLAG_double_math,
    FLAG_double_circle_area,
    FLAG_double_average,
    FLAG_double_compound_interest,
    FLAG_double_pythagoras,
    FLAG_double_temp_convert,
    FLAG_double_sqrt,
    FLAG_double_power,
    FLAG_double_volume_sphere,
    FLAG_double_discount,
    FLAG_double_simple_interest,
    FLAG_double_bmi,
    FLAG_double_kinetic_energy,
    FLAG_double_standard_deviation,
    FLAG_print_var,
    FLAG_print_even,
    FLAG_find_max,
    FLAG_input_sort,
    FLAG_input_fact,
    FLAG_array_assign,
    FLAG_array_reverse,
    FLAG_array_find,
    FLAG_array_access,
    FLAG_array_write,
    FLAG_array_vmath,
    FLAG_array_min_max,
    FLAG_string_find,
    FLAG_string_split,
    FLAG_switch_demo,
    FLAG_type_convert,
    FLAG_iterative_factorial,
    FLAG_random_walk,
    FLAG_bar_chart,
    FLAG_hanoi_tower,
    FLAG_ascii_art,
    FLAG_number_to_words,
    FLAG_temperature_table,
    FLAG_loop_demo,
    FLAG_matrix_mul,
    FLAG_matrix_transpose,
    FLAG_fann_create,
    FLAG_fann_train,
    FLAG_fann_run,
    FLAG_insertion_sort,
    FLAG_standard_deviation,
    FLAG_filter_numbers,
    FLAG_random_generator,
    FLAG_ascii_table,
    FLAG_password_card,
    FLAG_calculator,
    FLAG_unit_converter,
    FLAG_sort_stats,
    FLAG_string_analyzer,
    FLAG_number_analyzer,
    FLAG_math_menu,
    FLAG_quiz_game,
    FLAG_bmi_calculator,
    FLAG_base_converter,
    FLAG_freq_analysis,
    FLAG_shuffle,
    FLAG_weighted_random,
    FLAG_chess_problem,
    FLAG_shell_repl,
    FLAG_webserver,
    FLAG_sdl_window,
    FLAG_sdl_button,
    FLAG_thread,
    FLAG_scheduler,
    FLAG_shell_exec,
    FLAG_json,
    FLAG_crypto,
    FLAG_bluetooth_ble,
    FLAG_serial_rs232,
    FLAG_gpio,
    FLAG_gps,
    FLAG_timer_date,
    FLAG_sdl_sound,
    FLAG_sdl_joystick,
    FLAG_sdl_mouse,
    FLAG_fractal,
    FLAG_cluster_3x1,
    FLAG_reload,
    FLAG_coordinate_grid,
    FLAG_turmite,
    FLAG_crossword,
    FLAG_linter,
    FLAG_linked_list,
    FLAG_binary_search_tree,
    FLAG_tree_traversal,
    FLAG_graph_bfs_dfs,
    FLAG_n_queens,
    FLAG_sudoku,
    FLAG_levenshtein,
    FLAG_maze_generator,
    FLAG_maze_solver,
    FLAG_monte_carlo,
    FLAG_complex_numbers,
    FLAG_linear_regression,
    FLAG_numerical_integration,
    FLAG_hello_name,
    FLAG_hello_world,
    FLAG_algorithm,
    FLAG_double_array_access,
    FLAG_double_array_write,
    FLAG_double_array_iterate,
    FLAG_double_array_sum,
    FLAG_double_array_min_max,
    FLAG_double_array_reverse,
    FLAG_double_array_average,
    FLAG_array_iterate,
    FLAG_array_sum,
    FLAG_array_average,
    FLAG_shell_args,
    FLAG_pyramid,
    FLAG_statistics_suite,
    FLAG_string_length,
    FLAG_time,
    FLAG_stack,
    FLAG_queue,
    FLAG_rock_paper_scissors
} FlagType;

#define TF_ISSET(task, flag) ((task)->flags[(flag) / 64] & (1ULL << ((flag) % 64)))
#define TF_SET(task, flag)   ((task)->flags[(flag) / 64] |= (1ULL << ((flag) % 64)))
#define TF_CLR(task, flag)   ((task)->flags[(flag) / 64] &= ~(1ULL << ((flag) % 64)))

typedef struct {
    const char *any_of[PATTERN_MAX_KEYWORDS];
    const char *must_have[PATTERN_MAX_KEYWORDS];
    const char *excludes[PATTERN_MAX_EXCLUDES];
    int flag;
    int clear_algorithm;
} PatternTableEntry;

typedef struct {
    char filename[512];
    char stem[512];
    float embedding[EMBED_DIM];
    float score;
} ExampleDoc;

typedef struct {
    float embed[EMBED_DIM];
} WordEmbedding;

typedef struct {
    const char *keywords;
    const char *desc;
    void (*gen)(Program *, const char *);
    const char *dsl_keyword;
} Template;

extern WordEmbedding word_embeddings[VOCAB_SIZE];
extern ExampleDoc example_docs[MAX_EXAMPLES];
extern int num_examples;
extern int use_color;
extern int verbose_flag;
extern int dry_run_flag;
extern int validate_flag;
extern char out_dir[512];
extern char l1vm_root[512];
extern int vs_boost_tokens[64];
extern int vs_boost_count;

extern LearnedPattern learned_patterns[MAX_LEARNED];
extern int num_learned;
extern int learned_loaded;

extern Template templates[];
extern int num_templates;

int ensure_vars_cap(L1vmFunction *f, int needed);
int ensure_body_cap(L1vmFunction *f, int needed);
int ensure_funcs_cap(L1vmFunction **pfuncs, int *cap, int needed);
int ensure_includes_cap(char (**pinc)[256], int *cap, int needed);
int str_contains_word(const char *str, const char *word);
int save_learned_pattern(LearnedPattern *lp);
int init_function(L1vmFunction *f);
void free_function(L1vmFunction *f);
int init_program(Program *prog);
void free_program(Program *prog);
int init_learned_pattern(LearnedPattern *lp);
void free_learned_pattern(LearnedPattern *lp);
void trim(char *s);
void to_lowercase(char *s);
void add_include(Program *prog, const char *inc);
void add_include_post(Program *prog, const char *inc);
void add_func(Program *prog, const char *name);
void add_var_to_func(L1vmFunction *f, const char *type, const char *name, int count, const char **values, int num_values);
void func_append(L1vmFunction *f, const char *line);
void func_vardef(L1vmFunction *f, const char *scope);
int prompt_to_filename(const char *prompt, char *out, int max);
int is_question(const char *prompt);
void write_program(Program *prog, const char *filename);

int parse_task(const char *prompt, TaskProfile *task);
int generate_from_task(Program *prog, TaskProfile *task, int last_step);

int match_learned_pattern(const char *prompt, int *best_score);
int emit_learned_pattern(Program *prog, int learned_idx);
int emit_learned_step(Program *prog, int learned_idx);
int has_learned_id(const char *id);
void load_learned_patterns(void);
void ensure_learned_dir(void);
int forget_learned(const char *id);
void list_learned(void);
char* learned_dir_path(char *buf, int bufsize);
int learn_from_file(const char *path, const char *keywords, const char *description);

void init_embeddings(void);
void index_examples(void);
int search_examples(const char *query, int top_k, int *indices, float *scores);
void embed_text(const char *text, float *out);
float cosine_sim(const float *a, const float *b);
int llm_select_emitter(const char *prompt, TaskProfile *task);
void expand_query(const char *query, char *expanded, int max_len);
int tokenize(const char *text, int *tokens, int max_tokens);

int split_prompt_steps(const char *prompt, char steps[MAX_STEPS][MAX_PROMPT]);
int has_sequential_pattern(const char *prompt);
void load_synonyms(void);
void answer_question(const char *prompt);
int validate_code(const char *filename);
int self_test(void);

const char* resolve_synonym(const char *word);
int has_word(const char *prompt, const char *word);
int has_word_fuzzy(const char *text, const char *keyword);

extern int retry_seed;

#endif
