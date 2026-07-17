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


/* ── TaskProfile bit-flag constants ──────────────────────────────────── */

enum {
    FLAG_input = 0,
    FLAG_literals = 1,
    FLAG_operation = 2,
    FLAG_add = 3,
    FLAG_sub = 4,
    FLAG_mul = 5,
    FLAG_div = 6,
    FLAG_algorithm = 7,
    FLAG_loop = 8,
    FLAG_condition = 9,
    FLAG_output = 10,
    FLAG_sort = 11,
    FLAG_power = 12,
    FLAG_max = 13,
    FLAG_min = 14,
    FLAG_descending = 15,
    FLAG_gcd = 16,
    FLAG_countdown = 17,
    FLAG_mult_table = 18,
    FLAG_guess = 19,
    FLAG_random = 20,
    FLAG_hello_name = 21,
    FLAG_time = 22,
    FLAG_pointer = 23,
    FLAG_struct = 24,
    FLAG_hex_binary = 25,
    FLAG_shell_args = 26,
    FLAG_array = 27,
    FLAG_function = 28,
    FLAG_average = 29,
    FLAG_fizzbuzz = 30,
    FLAG_even_odd = 31,
    FLAG_primes = 32,
    FLAG_sum = 33,
    FLAG_factorial = 34,
    FLAG_fibonacci = 35,
    FLAG_median = 36,
    FLAG_string_cat = 37,
    FLAG_string_compare = 38,
    FLAG_array_assign = 39,
    FLAG_array_reverse = 40,
    FLAG_array_find = 41,
    FLAG_sum_range = 42,
    FLAG_print_even = 43,
    FLAG_find_max = 44,
    FLAG_fib_seq = 45,
    FLAG_input_sort = 46,
    FLAG_input_fact = 47,
    FLAG_countdown_from = 48,
    FLAG_read_file = 49,
    FLAG_write_file = 50,
    FLAG_array_vmath = 51,
    FLAG_string_to_num = 52,
    FLAG_timer = 53,
    FLAG_array_min_max = 54,
    FLAG_bool_demo = 55,
    FLAG_print_var = 56,
    FLAG_bit_check = 57,
    FLAG_leap_year = 58,
    FLAG_temp_convert = 59,
    FLAG_circle_area = 60,
    FLAG_fann_create = 61,
    FLAG_fann_train = 62,
    FLAG_fann_run = 63,
    FLAG_palindrome = 64,
    FLAG_lcm = 65,
    FLAG_collatz = 66,
    FLAG_sum_of_digits = 67,
    FLAG_reverse_string = 68,
    FLAG_armstrong = 69,
    FLAG_perfect_number = 70,
    FLAG_count_vowels = 71,
    FLAG_anagram_check = 72,
    FLAG_string_to_upper = 73,
    FLAG_string_to_lower = 74,
    FLAG_caesar_cipher = 75,
    FLAG_palindrome_string = 76,
    FLAG_bubble_sort = 77,
    FLAG_binary_search = 78,
    FLAG_square_root = 79,
    FLAG_prime_factorization = 80,
    FLAG_standard_deviation = 81,
    FLAG_compound_interest = 82,
    FLAG_decimal_to_binary = 83,
    FLAG_dice_roll = 84,
    FLAG_double_math = 85,
    FLAG_double_circle_area = 86,
    FLAG_double_average = 87,
    FLAG_double_compound_interest = 88,
    FLAG_double_pythagoras = 89,
    FLAG_double_temp_convert = 90,
    FLAG_double_sqrt = 91,
    FLAG_double_power = 92,
    FLAG_double_volume_sphere = 93,
    FLAG_double_discount = 94,
    FLAG_double_simple_interest = 95,
    FLAG_double_bmi = 96,
    FLAG_double_standard_deviation = 97,
    FLAG_double_kinetic_energy = 98,
    FLAG_string_length = 99,
    FLAG_stack = 100,
    FLAG_queue = 101,
    FLAG_insertion_sort = 102,
    FLAG_calculator = 103,
    FLAG_unit_converter = 104,
    FLAG_rock_paper_scissors = 105,
    FLAG_pyramid = 106,
    FLAG_temp_converter_menu = 107,
    FLAG_sort_stats = 108,
    FLAG_string_analyzer = 109,
    FLAG_number_analyzer = 110,
    FLAG_filter_numbers = 111,
    FLAG_random_generator = 112,
    FLAG_math_menu = 113,
    FLAG_quiz_game = 114,
    FLAG_bmi_calculator = 115,
    FLAG_statistics_suite = 116,
    FLAG_linked_list = 117,
    FLAG_binary_search_tree = 118,
    FLAG_tree_traversal = 119,
    FLAG_graph_bfs_dfs = 120,
    FLAG_n_queens = 121,
    FLAG_sudoku = 122,
    FLAG_levenshtein = 123,
    FLAG_maze_generator = 124,
    FLAG_maze_solver = 125,
    FLAG_monte_carlo = 126,
    FLAG_matrix_mul = 127,
    FLAG_matrix_transpose = 128,
    FLAG_numerical_integration = 129,
    FLAG_complex_numbers = 130,
    FLAG_linear_regression = 131,
    FLAG_base_converter = 132,
    FLAG_freq_analysis = 133,
    FLAG_shuffle = 134,
    FLAG_weighted_random = 135,
    FLAG_ascii_table = 136,
    FLAG_bignum_math = 137,
    FLAG_password_card = 138,
    FLAG_chess_problem = 139,
    FLAG_shell_repl = 140,
    FLAG_webserver = 141,
    FLAG_sdl_window = 142,
    FLAG_sdl_button = 143,
    FLAG_thread = 144,
    FLAG_scheduler = 145,
    FLAG_shell_exec = 146,
    FLAG_json = 147,
    FLAG_crypto = 148,
    FLAG_bluetooth_ble = 149,
    FLAG_serial_rs232 = 150,
    FLAG_gpio = 151,
    FLAG_gps = 152,
    FLAG_timer_date = 153,
    FLAG_sdl_sound = 154,
    FLAG_sdl_joystick = 155,
    FLAG_sdl_mouse = 156,
    FLAG_fractal = 157,
    FLAG_cluster_3x1 = 158,
    FLAG_reload = 159,
    FLAG_coordinate_grid = 160,
    FLAG_turmite = 161,
    FLAG_crossword = 162,
    FLAG_linter = 163,
    FLAG_hello_world = 164,
    FLAG_string_find = 165,
    FLAG_string_split = 166,
    FLAG_switch_demo = 167,
    FLAG_type_convert = 168,
    FLAG_iterative_factorial = 169,
    FLAG_random_walk = 170,
    FLAG_bar_chart = 171,
    FLAG_hanoi_tower = 172,
    FLAG_ascii_art = 173,
    FLAG_number_to_words = 174,
    FLAG_temperature_table = 175,
    FLAG_loop_demo = 176,
    FLAG_array_access = 177,
    FLAG_array_write = 178,
    FLAG_array_iterate = 179,
    FLAG_array_sum = 180,
    FLAG_array_average = 181,
    FLAG_double_array_access = 182,
    FLAG_double_array_write = 183,
    FLAG_double_array_iterate = 184,
    FLAG_double_array_sum = 185,
    FLAG_double_array_min_max = 186,
    FLAG_double_array_reverse = 187,
    FLAG_double_array_average = 188,
    FLAG_COUNT = 189
};

#define TF_WORDS   ((FLAG_COUNT + 63) / 64)
#define TF_SET(tf, n)    ((tf)->flags[(n) / 64] |=  (1ULL << ((n) % 64)))
#define TF_CLR(tf, n)    ((tf)->flags[(n) / 64] &= ~(1ULL << ((n) % 64)))
#define TF_ISSET(tf, n)  ((tf)->flags[(n) / 64] &  (1ULL << ((n) % 64)))
#define TF_CLEAR(tf)     (memset((tf)->flags, 0, sizeof((tf)->flags)))

#define PATTERN_MAX_KEYWORDS 4
#define PATTERN_MAX_EXCLUDES 4

typedef struct {
    const char *any_of[PATTERN_MAX_KEYWORDS];
    const char *must_have[PATTERN_MAX_KEYWORDS];
    const char *excludes[PATTERN_MAX_EXCLUDES];
    int flag;
    int clear_algorithm;
} PatternTableEntry;


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
    uint64_t flags[TF_WORDS]; /* bit-flags (FLAG_xxx enum) */
    int input_count;
    int count_value;
    int literals[MAX_NUMS];
    double double_literals[MAX_NUMS];
    int num_literals;
    char op[16];
    char algorithm[32];
    int algo_param;
    int loop_start, loop_end;
    int median_count;
    int sum_range_n;
    int print_even_n;
    int find_max_count;
    int fib_seq_n;
    int input_sort_count;
    int countdown_start;
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
