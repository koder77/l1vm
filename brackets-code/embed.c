/*
 * This file embed.c is part of L1vm.
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

#include "brackets-code.h"

// ==================== TINY LLM INFERENCE ENGINE ====================

#define EMBED_DIM 32
#define VOCAB_SIZE 72
#define TEMPERATURE 0.8
#define MAX_STEPS 32
WordEmbedding word_embeddings[VOCAB_SIZE];
float attention_weights[NUM_EMITTERS];
const char *EMITTER_NAMES[NUM_EMITTERS] = {"math","input_loop","loop","for_sum","print_even","find_max","countdown","fib_seq","input_sort","median","string_cat","string_compare","array_assign","array_reverse","array_find","input_fact","array_vmath","read_file","write_file","string_to_num","timer","factorial","fizzbuzz","primes","even_odd","power","mult_table","guess","gcd","hello_name","random","array_min_max","bool_demo","bit_check","fann_create","fann_train","fann_run","average","selection_sort","palindrome","lcm","collatz","sum_of_digits","reverse_string","armstrong","perfect_number","count_vowels","anagram_check","string_to_upper","string_to_lower","caesar_cipher","palindrome_string","bubble_sort","binary_search","square_root","prime_factorization","standard_deviation","compound_interest","decimal_to_binary","dice_roll","double_math","double_circle_area","double_average","double_compound_interest","double_pythagoras","double_temp_convert","double_sqrt","function","string_length","stack","queue","insertion_sort","calculator","unit_converter","rock_paper_scissors","pyramid","temp_converter_menu","sort_stats","string_analyzer","number_analyzer","filter_numbers","random_generator","math_menu","quiz_game","bmi_calculator","statistics_suite","linked_list","binary_search_tree","tree_traversal","graph_bfs_dfs","n_queens","sudoku","levenshtein_distance","maze_generator","maze_solver","monte_carlo_pi","matrix_multiplication","matrix_transpose","numerical_integration","complex_numbers","linear_regression","base_converter","freq_analysis","shuffle","weighted_random","ascii_table","bignum_math","password_card","chess_problem","shell_repl","webserver","sdl_window","sdl_button","thread","scheduler","shell_exec","json","crypto","bluetooth_ble","serial_rs232","gpio","gps","timer_date","sdl_sound","sdl_joystick","sdl_mouse","fractal","cluster_3x1","reload","coordinate_grid","turmite","crossword","linter","double_power","double_volume_sphere","double_discount","double_simple_interest","double_bmi","double_standard_deviation","double_kinetic_energy","add","sub","mul","div","double_add","double_sub","double_mul","double_div",
"hello_world", "string_find", "string_split", "switch_demo",
"type_convert", "iterative_factorial", "random_walk", "bar_chart",
"hanoi_tower", "ascii_art", "number_to_words", "temperature_table",
"loop_demo", "pointer_demo", "struct_demo", "hex_binary",
"shell_args", "time_demo"};
/* compile-time assert: NUM_EMITTERS must match actual array count */
typedef int EMITTER_COUNT_CHECK[(sizeof(EMITTER_NAMES)/sizeof(EMITTER_NAMES[0])) == NUM_EMITTERS ? 1 : -1];
int vs_boost_tokens[64];
int vs_boost_count = 0;

// Vector search data structures
#define MAX_EXAMPLES 512
#define MAX_TOP_K 10
#define EXAMPLE_DIR "l1vm-example-code"
#define EXAMPLE_SUBDIRS 3
const char *example_subdirs[EXAMPLE_SUBDIRS] = {"prog", "include", "lib"};
static const char *example_exts[] = {".l1com", ".l1h", ".l1asm"};
#define EXAMPLE_EXTS 3

ExampleDoc example_docs[MAX_EXAMPLES];
int num_examples = 0;
int examples_indexed = 0;

void index_examples(void);
int search_examples(const char *query, int top_k, int *indices, float *scores);

unsigned long hash_word(const char *s) {
    unsigned long hash = 5381;
    int c;
    while ((c = *s++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

float idf_weights[VOCAB_SIZE];

// Vocabulary token IDs
#define TOK_SUM 0
#define TOK_ADD 1
#define TOK_SUB 2
#define TOK_MUL 3
#define TOK_DIV 4
#define TOK_MOD 5
#define TOK_INPUT 6
#define TOK_PRINT 7
#define TOK_LOOP 8
#define TOK_FOR 9
#define TOK_WHILE 10
#define TOK_IF 11
#define TOK_ARRAY 12
#define TOK_SORT 13
#define TOK_MAX 14
#define TOK_MIN 15
#define TOK_AVERAGE 16
#define TOK_MEDIAN 17
#define TOK_COUNTDOWN 18
#define TOK_FIBONACCI 19
#define TOK_FACTORIAL 20
#define TOK_PRIME 21
#define TOK_FIZZBUZZ 22
#define TOK_POWER 23
#define TOK_GCD 24
#define TOK_TIME 25
#define TOK_POINTER 26
#define TOK_STRUCT 27
#define TOK_STRING 28
#define TOK_CONCAT 29
#define TOK_COMPARE 30
#define TOK_REVERSE 31
#define TOK_FIND 32
#define TOK_ASSIGN 33
#define TOK_FILE 34
#define TOK_READ 35
#define TOK_WRITE 36
#define TOK_CONVERT 37
#define TOK_TIMER 38
#define TOK_SHELL 39
#define TOK_HELLO 40
#define TOK_FUNCTION 41
#define TOK_EVEN 42
#define TOK_ODD 43
#define TOK_GUESS 44
#define TOK_TABLE 45
#define TOK_HEX 46
#define TOK_BINARY 47
#define TOK_RANGE 48
#define TOK_EXIT 49
#define TOK_ZERO 50
#define TOK_ONE 51
#define TOK_TWO 52
#define TOK_NUM 53
#define TOK_VALUE 54
#define TOK_DATA 55
#define TOK_CODE 56
#define TOK_GENERATE 57
#define TOK_CREATE 58
#define TOK_MAKE 59
#define TOK_SHOW 60
#define TOK_START 61
#define TOK_STOP 62
#define TOK_RUN 63
#define TOK_FANN 64
#define TOK_TRAIN 65
#define TOK_NEURAL 66
#define TOK_NETWORK 67
#define TOK_MODEL 68
#define TOK_LAYER 69
#define TOK_LEARN 70
#define TOK_PREDICT 71

const char *vocab[VOCAB_SIZE] = {
    "sum", "add", "sub", "mul", "div", "mod", "input", "print", "loop", "for",
    "while", "if", "array", "sort", "max", "min", "average", "median", "countdown",
    "fibonacci", "factorial", "prime", "fizzbuzz", "power", "gcd", "time", "pointer",
    "struct", "string", "concat", "compare", "reverse", "find", "assign", "file",
    "read", "write", "convert", "timer", "shell", "hello", "function", "even",
    "odd", "guess", "table", "hex", "binary", "range", "exit", "zero", "one",
    "two", "num", "value", "data", "code", "generate", "create", "make", "show",
    "start", "stop", "run",
    "fann", "train", "neural", "network", "model", "layer", "learn", "predict"
};

int embeddings_initialized = 0;

void init_embeddings(void) {
    if (embeddings_initialized) return;
    embeddings_initialized = 1;
    for (int i = 0; i < VOCAB_SIZE; i++) {
        const char *w = vocab[i];
        int len = strlen(w);
        // Build embedding from character trigrams so similar words get similar vectors
        int ngram_count = 0;
        for (int ci = 0; ci + 2 < len; ci++) {
            char trigram[4] = {w[ci], w[ci+1], w[ci+2], '\0'};
            unsigned long th = hash_word(trigram);
            srand(th);
            for (int j = 0; j < EMBED_DIM; j++)
                word_embeddings[i].embed[j] += ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
            ngram_count++;
        }
        // Fallback for short words: use bigrams and character-position hash
        if (ngram_count == 0) {
            for (int ci = 0; ci + 1 < len; ci++) {
                char bigram[3] = {w[ci], w[ci+1], '\0'};
                unsigned long th = hash_word(bigram);
                srand(th);
                for (int j = 0; j < EMBED_DIM; j++)
                    word_embeddings[i].embed[j] += ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
                ngram_count++;
            }
        }
        // Single-char fallback
        if (ngram_count == 0) {
            unsigned long th = hash_word(w);
            srand(th);
            for (int j = 0; j < EMBED_DIM; j++)
                word_embeddings[i].embed[j] = ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
            ngram_count = 1;
        }
        // Average
        float inv = 1.0f / ngram_count;
        float sum = 0;
        for (int j = 0; j < EMBED_DIM; j++) {
            word_embeddings[i].embed[j] *= inv;
            sum += word_embeddings[i].embed[j] * word_embeddings[i].embed[j];
        }
        float norm = sqrtf(sum);
        if (norm > 0)
            for (int j = 0; j < EMBED_DIM; j++) word_embeddings[i].embed[j] /= norm;
    }
    for (int i = 0; i < VOCAB_SIZE; i++) idf_weights[i] = 1.0f;
}

int tokenize(const char *text, int *tokens, int max_tokens) {
    char buf[MAX_PROMPT];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", text);
    to_lowercase(buf);
    int count = 0;
    char *p = buf;
    while (*p && count < max_tokens) {
        while (*p && !isalpha(*p)) p++;
        if (!*p) break;
        char *start = p;
        while (*p && isalpha(*p)) p++;
        char saved = *p;
        *p = '\0';
        int found = -1;
        for (int i = 0; i < VOCAB_SIZE; i++) {
            if (strcmp(start, vocab[i]) == 0) { found = i; break; }
        }
        if (found < 0) {
            const char *syn = resolve_synonym(start);
            if (syn) {
                for (int i = 0; i < VOCAB_SIZE; i++) {
                    if (strcmp(syn, vocab[i]) == 0) { found = i; break; }
                }
            }
        }
        if (found >= 0 && count < max_tokens) tokens[count++] = found;
        *p = saved;
    }
    return count;
}

void softmax(float *x, int n) {
    if (n <= 0) return;
    float max = x[0], sum = 0;
    for (int i = 1; i < n; i++) if (x[i] > max) max = x[i];
    for (int i = 0; i < n; i++) { x[i] = expf(x[i] - max); sum += x[i]; }
    for (int i = 0; i < n; i++) x[i] /= sum;
}

void apply_token_boosts(float *scores, const int *tokens, int count,
                                float fann_34, float fann_35, float fann_36,
                                float train_34, float train_35,
                                float predict_36,
                                float p, float s,
                                float power_0, float power_25)
{
    for (int ti = 0; ti < count && ti < 32; ti++) {
        int tok_id = tokens[ti];
        if (tok_id == TOK_FANN || tok_id == TOK_NEURAL || tok_id == TOK_NETWORK) {
            scores[34] += fann_34; scores[35] += fann_35; scores[36] += fann_36; }
        if (tok_id == TOK_TRAIN || tok_id == TOK_LEARN) { scores[34] += train_34; scores[35] += train_35; }
        if (tok_id == TOK_PREDICT) { scores[36] += predict_36; }
        if (tok_id == TOK_SUM || tok_id == TOK_DIV) scores[0] += p;
        if (tok_id == TOK_MOD || tok_id == TOK_ADD) scores[1] += p;
        if (tok_id == TOK_LOOP || tok_id == TOK_FOR) scores[2] += p;
        if (tok_id == TOK_MEDIAN) scores[9] += s;
        if (tok_id == TOK_FIBONACCI) scores[7] += s;
        if (tok_id == TOK_FACTORIAL) scores[15] += s;
        if (tok_id == TOK_SORT) scores[8] += s;
        if (tok_id == TOK_MAX) scores[5] += s;
        if (tok_id == TOK_MIN) scores[16] += p;
        if (tok_id == TOK_STRING) { scores[10] += s; scores[11] += s; }
        if (tok_id == TOK_FIND) scores[13] += s;
        if (tok_id == TOK_ASSIGN) scores[14] += s;
        if (tok_id == TOK_WRITE || tok_id == TOK_READ) { scores[17] += s; scores[18] += s; }
        if (tok_id == TOK_CONVERT) scores[19] += s;
        if (tok_id == TOK_TIMER) scores[20] += s;
        if (tok_id == TOK_STRUCT) scores[6] += s;
        if (tok_id == TOK_POWER) scores[0] += power_0;
        if (tok_id == TOK_FACTORIAL) scores[21] += s;
        if (tok_id == TOK_FIZZBUZZ) scores[22] += s;
        if (tok_id == TOK_PRIME) scores[23] += s;
        if (tok_id == TOK_EVEN || tok_id == TOK_ODD) scores[24] += s;
        if (tok_id == TOK_POWER) scores[25] += power_25;
        if (tok_id == TOK_HEX) scores[26] += s;
        if (tok_id == TOK_GUESS) scores[27] += s;
        if (tok_id == TOK_GCD) scores[28] += s;
        if (tok_id == TOK_HELLO) scores[29] += s;
        if (tok_id == TOK_NUM) scores[30] += p;
    }
}

int llm_select_emitter(const char *prompt, TaskProfile *task) {
    int tokens[64], num_tokens = tokenize(prompt, tokens, 64);

    float emitter_scores[NUM_EMITTERS];
    for (int i = 0; i < NUM_EMITTERS; i++) emitter_scores[i] = 0;

    if (task->has_operation && task->has_literals) emitter_scores[0] = 1.5f;
    if (task->has_input && !task->has_operation) emitter_scores[1] = 1.5f;
    if (task->has_input && task->has_operation) emitter_scores[1] += 1.0f;
    if (task->has_loop && !task->has_operation && !task->has_input) emitter_scores[2] = 1.5f;
    if (task->has_sum_range) emitter_scores[3] = 2.0f;
    if (task->has_print_even) emitter_scores[4] = 2.0f;
    if (task->has_find_max) emitter_scores[5] = 2.0f;
    if (task->has_countdown_from) emitter_scores[6] = 2.0f;
    if (task->has_fib_seq) emitter_scores[7] = 2.0f;
    if (task->has_input_sort) emitter_scores[8] = 2.0f;
    if (task->has_median) emitter_scores[9] = 2.0f;
    if (task->has_string_cat) emitter_scores[10] = 2.0f;
    if (task->has_string_compare) emitter_scores[11] = 2.0f;
    if (task->has_array_assign) emitter_scores[12] = 2.0f;
    if (task->has_array_reverse) emitter_scores[13] = 2.0f;
    if (task->has_array_find) emitter_scores[14] = 2.0f;
    if (task->has_array_access) emitter_scores[14] = 2.0f;
    if (task->has_array_write) emitter_scores[12] = 2.0f;
    if (task->has_input_fact) emitter_scores[15] = 2.0f;
    if (task->has_array_vmath) emitter_scores[16] = 2.0f;
    if (task->has_read_file) emitter_scores[17] = 2.0f;
    if (task->has_write_file) emitter_scores[18] = 2.0f;
    if (task->has_string_to_num) emitter_scores[19] = 2.0f;
    if (task->has_timer) emitter_scores[20] = 2.0f;
    if (task->has_factorial && !task->has_input) emitter_scores[21] = 2.0f;
    if (task->has_fizzbuzz) emitter_scores[22] = 2.0f;
    if (task->has_primes) emitter_scores[23] = 2.0f;
    if (task->has_even_odd && !task->has_print_even) emitter_scores[24] = 2.0f;
    if (task->has_power) emitter_scores[25] = 2.0f;
    if (task->has_mult_table) emitter_scores[26] = 2.0f;
    if (task->has_guess) emitter_scores[27] = 2.0f;
    if (task->has_gcd) emitter_scores[28] = 2.0f;
    if (task->has_hello_name) emitter_scores[29] = 2.0f;
    if (task->has_random) emitter_scores[30] = 2.0f;
    if (task->has_array_min_max) emitter_scores[31] = 2.0f;
    if (task->has_bool_demo) emitter_scores[32] = 2.0f;
    if (task->has_bit_check) emitter_scores[33] = 2.0f;
    if (task->has_fann_create && task->has_fann_train) emitter_scores[35] = 2.5f;
    if (task->has_fann_create && !task->has_fann_train) emitter_scores[34] = 2.0f;
    if (task->has_fann_run) emitter_scores[36] = 2.0f;
    if (task->has_average && task->has_input) emitter_scores[37] = 2.0f;
    if (task->has_sort && !task->has_input) emitter_scores[38] = 2.0f;
    if (task->has_palindrome) emitter_scores[39] = 2.0f;
    if (task->has_lcm) emitter_scores[40] = 2.0f;
    if (task->has_collatz) emitter_scores[41] = 2.0f;
    if (task->has_sum_of_digits) emitter_scores[42] = 2.0f;
    if (task->has_reverse_string) emitter_scores[43] = 2.0f;
    if (task->has_armstrong) emitter_scores[44] = 2.0f;
    if (task->has_perfect_number) emitter_scores[45] = 2.0f;
    if (task->has_count_vowels) emitter_scores[46] = 2.0f;
    if (task->has_anagram_check) emitter_scores[47] = 2.0f;
    if (task->has_string_to_upper) emitter_scores[48] = 2.0f;
    if (task->has_string_to_lower) emitter_scores[49] = 2.0f;
    if (task->has_caesar_cipher) emitter_scores[50] = 2.0f;
    if (task->has_palindrome_string) emitter_scores[51] = 2.0f;
    if (task->has_bubble_sort) emitter_scores[52] = 2.0f;
    if (task->has_binary_search) emitter_scores[53] = 2.0f;
    if (task->has_square_root) emitter_scores[54] = 2.0f;
    if (task->has_prime_factorization) emitter_scores[55] = 2.0f;
    if (task->has_standard_deviation) emitter_scores[56] = 2.0f;
    if (task->has_compound_interest) emitter_scores[57] = 2.0f;
    if (task->has_decimal_to_binary) emitter_scores[58] = 2.0f;
    if (task->has_dice_roll) emitter_scores[59] = 2.0f;
    if (task->has_double_math) emitter_scores[60] = 2.0f;
    if (task->has_double_circle_area) emitter_scores[61] = 2.0f;
    if (task->has_double_average) emitter_scores[62] = 2.0f;
    if (task->has_double_compound_interest) emitter_scores[63] = 2.0f;
    if (task->has_double_pythagoras) emitter_scores[64] = 2.0f;
    if (task->has_double_temp_convert) emitter_scores[65] = 2.0f;
    if (task->has_double_sqrt) emitter_scores[66] = 2.0f;
    if (task->has_function) emitter_scores[67] = 2.0f;
    if (task->has_string_length) emitter_scores[68] = 2.0f;
    if (task->has_stack) emitter_scores[69] = 2.0f;
    if (task->has_queue) emitter_scores[70] = 2.0f;
    if (task->has_insertion_sort) emitter_scores[71] = 2.0f;
    if (task->has_calculator) emitter_scores[72] = 2.0f;
    if (task->has_unit_converter) emitter_scores[73] = 2.0f;
    if (task->has_rock_paper_scissors) emitter_scores[74] = 2.0f;
    if (task->has_pyramid) emitter_scores[75] = 2.0f;
    if (task->has_temp_converter_menu) emitter_scores[76] = 2.0f;
    if (task->has_sort_stats) emitter_scores[77] = 2.0f;
    if (task->has_string_analyzer) emitter_scores[78] = 2.0f;
    if (task->has_number_analyzer) emitter_scores[79] = 2.0f;
    if (task->has_filter_numbers) emitter_scores[80] = 2.0f;
    if (task->has_random_generator) emitter_scores[81] = 2.0f;
    if (task->has_math_menu) emitter_scores[82] = 2.0f;
    if (task->has_quiz_game) emitter_scores[83] = 2.0f;
    if (task->has_bmi_calculator) emitter_scores[84] = 2.0f;
    if (task->has_statistics_suite) emitter_scores[85] = 2.0f;
    if (task->has_linked_list) emitter_scores[86] = 2.0f;
    if (task->has_binary_search_tree) emitter_scores[87] = 2.0f;
    if (task->has_tree_traversal) emitter_scores[88] = 2.0f;
    if (task->has_graph_bfs_dfs) emitter_scores[89] = 2.0f;
    if (task->has_n_queens) emitter_scores[90] = 2.0f;
    if (task->has_sudoku) emitter_scores[91] = 2.0f;
    if (task->has_levenshtein) emitter_scores[92] = 2.0f;
    if (task->has_maze_generator) emitter_scores[93] = 2.0f;
    if (task->has_maze_solver) emitter_scores[94] = 2.0f;
    if (task->has_monte_carlo) emitter_scores[95] = 2.0f;
    if (task->has_matrix_mul) emitter_scores[96] = 2.0f;
    if (task->has_matrix_transpose) emitter_scores[97] = 2.0f;
    if (task->has_numerical_integration) emitter_scores[98] = 2.0f;
    if (task->has_complex_numbers) emitter_scores[99] = 2.0f;
    if (task->has_linear_regression) emitter_scores[100] = 2.0f;
    if (task->has_base_converter) emitter_scores[101] = 2.0f;
    if (task->has_freq_analysis) emitter_scores[102] = 2.0f;
    if (task->has_shuffle) emitter_scores[103] = 2.0f;
    if (task->has_weighted_random) emitter_scores[104] = 2.0f;
    if (task->has_ascii_table) emitter_scores[105] = 3.0f;
    if (task->has_bignum_math) emitter_scores[106] = 2.5f;
    if (task->has_password_card) emitter_scores[107] = 2.5f;
    if (task->has_chess_problem) emitter_scores[108] = 2.5f;
    if (task->has_shell_repl) emitter_scores[109] = 2.5f;
    if (task->has_webserver) emitter_scores[110] = 2.5f;
    if (task->has_sdl_window) emitter_scores[111] = 2.5f;
    if (task->has_sdl_button) emitter_scores[112] = 2.5f;
    if (task->has_thread) emitter_scores[113] = 2.0f;
    if (task->has_scheduler) emitter_scores[114] = 2.0f;
    if (task->has_shell_exec) emitter_scores[115] = 2.5f;
    if (task->has_json) emitter_scores[116] = 2.5f;
    if (task->has_crypto) emitter_scores[117] = 2.5f;
    if (task->has_bluetooth_ble) emitter_scores[118] = 2.5f;
    if (task->has_serial_rs232) emitter_scores[119] = 2.5f;
    if (task->has_gpio) emitter_scores[120] = 2.5f;
    if (task->has_gps) emitter_scores[121] = 2.5f;
    if (task->has_timer_date) emitter_scores[122] = 2.0f;
    if (task->has_sdl_sound) emitter_scores[123] = 2.5f;
    if (task->has_sdl_joystick) emitter_scores[124] = 2.5f;
    if (task->has_sdl_mouse) emitter_scores[125] = 2.5f;
    if (task->has_fractal) emitter_scores[126] = 2.5f;
    if (task->has_cluster_3x1) emitter_scores[127] = 2.5f;
    if (task->has_reload) emitter_scores[128] = 2.0f;
    if (task->has_coordinate_grid) emitter_scores[129] = 2.0f;
    if (task->has_turmite) emitter_scores[130] = 2.5f;
    if (task->has_crossword) emitter_scores[131] = 2.5f;
    if (task->has_linter) emitter_scores[132] = 2.0f;
    if (task->has_double_power) emitter_scores[133] = 2.0f;
    if (task->has_double_volume_sphere) emitter_scores[134] = 2.0f;
    if (task->has_double_discount) emitter_scores[135] = 2.0f;
    if (task->has_double_simple_interest) emitter_scores[136] = 2.0f;
    if (task->has_double_bmi) emitter_scores[137] = 2.0f;
    if (task->has_double_standard_deviation) emitter_scores[138] = 2.0f;
    if (task->has_double_kinetic_energy) emitter_scores[139] = 2.0f;
    if (task->has_add && task->has_literals && !task->has_input) { emitter_scores[140] = 2.0f; }
    if (task->has_sub && task->has_literals && !task->has_input) { emitter_scores[141] = 2.0f; }
    if (task->has_mul && task->has_literals && !task->has_input) { emitter_scores[142] = 2.0f; }
    if (task->has_div && task->has_literals && !task->has_input) { emitter_scores[143] = 2.0f; }
    if (task->has_add && task->has_literals && !task->has_input && strcmp(task->type, "double") == 0) { emitter_scores[144] = 2.5f; }
    if (task->has_sub && task->has_literals && !task->has_input && strcmp(task->type, "double") == 0) { emitter_scores[145] = 2.5f; }
    if (task->has_mul && task->has_literals && !task->has_input && strcmp(task->type, "double") == 0) { emitter_scores[146] = 2.5f; }
    if (task->has_div && task->has_literals && !task->has_input && strcmp(task->type, "double") == 0) { emitter_scores[147] = 2.5f; }

    if (task->has_hello_world) { emitter_scores[148] = 2.0f; }
    if (task->has_string_find) { emitter_scores[149] = 2.0f; }
    if (task->has_string_split) { emitter_scores[150] = 2.0f; }
    if (task->has_switch_demo) { emitter_scores[151] = 2.0f; }
    if (task->has_type_convert) { emitter_scores[152] = 2.0f; }
    if (task->has_iterative_factorial) { emitter_scores[153] = 2.0f; }
    if (task->has_random_walk) { emitter_scores[154] = 2.0f; }
    if (task->has_bar_chart) { emitter_scores[155] = 2.0f; }
    if (task->has_hanoi_tower) { emitter_scores[156] = 2.0f; }
    if (task->has_ascii_art) { emitter_scores[157] = 2.0f; }
    if (task->has_number_to_words) { emitter_scores[158] = 2.0f; }
    if (task->has_temperature_table) { emitter_scores[159] = 2.0f; }
    if (task->has_loop_demo) { emitter_scores[160] = 2.0f; }
    if (task->has_pointer) { emitter_scores[161] = 2.0f; }
    if (task->has_struct) { emitter_scores[162] = 2.0f; }
    if (task->has_hex_binary) { emitter_scores[163] = 2.0f; }
    if (task->has_shell_args) { emitter_scores[164] = 2.0f; }
    if (task->has_time) { emitter_scores[165] = 2.0f; }

    // Phase 2a: prompt token boosts
    apply_token_boosts(emitter_scores, tokens, num_tokens,
        1.0f, 1.5f, 1.0f,
        0.5f, 1.5f,
        1.5f,
        0.5f, 0.8f,
        0.3f, 0.5f);

    // Phase 2b: Vector search boost tokens from matched examples
    apply_token_boosts(emitter_scores, vs_boost_tokens, vs_boost_count,
        0.5f, 0.7f, 0.5f,
        0.3f, 0.7f,
        0.7f,
        0.3f, 0.4f,
        0.2f, 0.3f);

    float temp = TEMPERATURE;
    for (int i = 0; i < NUM_EMITTERS; i++) emitter_scores[i] /= temp;
    softmax(emitter_scores, NUM_EMITTERS);
    for (int i = 0; i < NUM_EMITTERS; i++) attention_weights[i] = emitter_scores[i];

    // Rank emitters by score, storing top alternatives for retry
    int ranked[NUM_EMITTERS];
    for (int i = 0; i < NUM_EMITTERS; i++) ranked[i] = i;
    // Simple bubble sort (NUM_EMITTERS is small, 133)
    for (int i = 0; i < NUM_EMITTERS - 1; i++) {
        for (int j = 0; j < NUM_EMITTERS - i - 1; j++) {
            if (emitter_scores[ranked[j]] < emitter_scores[ranked[j + 1]]) {
                int tmp = ranked[j]; ranked[j] = ranked[j + 1]; ranked[j + 1] = tmp;
            }
        }
    }

    int best = 0;
    float best_p = emitter_scores[0];
    for (int i = 1; i < NUM_EMITTERS; i++) {
        if (emitter_scores[i] > best_p) { best_p = emitter_scores[i]; best = i; }
    }

    // Use retry_seed to select alternative emitter (for validation retry)
    if (retry_seed > 0) {
        int alt_idx = retry_seed < NUM_EMITTERS ? retry_seed : NUM_EMITTERS - 1;
        best = ranked[alt_idx];
        best_p = emitter_scores[best];
    }

    if (verbose_flag) {
        printf("\nEmitter scores:\n");
        for (int i = 0; i < NUM_EMITTERS; i++)
            printf("  %2d %-16s %.3f\n", i, EMITTER_NAMES[i], emitter_scores[i]);
        printf("  -> best: %s (%.3f)\n", EMITTER_NAMES[best], best_p);
        if (retry_seed > 0) printf("  (retry %d, using alternative rank %d)\n", retry_seed, retry_seed);
    }

    // Store ranked alternatives in extra_emitters for retry fallback
    // (used when retry_seed changes which primary emitter is selected)
    // Multi-emitter composition: add strongly-scored secondary emitters
    // Dynamic threshold based on prompt length: longer prompts more likely multi-step
    float pfactor = (float)strlen(prompt) / 80.0f;
    if (pfactor > 2.0f) pfactor = 2.0f;
    if (pfactor < 0.5f) pfactor = 0.5f;
    task->num_extra_emitters = 0;
    float threshold = best_p * (0.5f / pfactor);
    float hard_min = 0.2f;
    if (threshold < hard_min) threshold = hard_min;

    // Emitter domain groups: avoid selecting redundant emitters from same domain
    static const char *const sort_domain[] = {"input_sort", "bubble_sort", "selection_sort", "insertion_sort", NULL};
    static const char *const math_domain[] = {"math", "input_loop", "loop", "for_sum", NULL};
    static const char *const string_domain[] = {"string_cat", "reverse_string", "count_vowels", "anagram_check", "string_to_upper", "string_to_lower", "caesar_cipher", "palindrome_string", "string_compare", "string_length", "string_analyzer", NULL};
    static const char *const fib_domain[] = {"fibonacci", "fib_seq", NULL};
    static const char *const fann_domain[] = {"fann_create", "fann_train", "fann_run", NULL};

    // Input-reading emitters: mutual exclusion - if primary reads input, skip other input-readers
    static const char *const input_readers[] = {"math", "input_loop", "for_sum", "print_even",
        "input_sort", "average", "selection_sort", "bubble_sort",
        "standard_deviation", "insertion_sort", NULL};
    int best_reads_input = 0;
    for (int ri = 0; input_readers[ri]; ri++)
        if (strcmp(EMITTER_NAMES[best], input_readers[ri]) == 0) { best_reads_input = 1; break; }

    // Determine which domain the best emitter belongs to
    int in_domain = 0;
    const char *best_name = EMITTER_NAMES[best];
    const char *const *domains[] = {sort_domain, math_domain, string_domain, fib_domain, fann_domain, NULL};
    for (int d = 0; domains[d]; d++) {
        for (int di = 0; domains[d][di]; di++) {
            if (strcmp(best_name, domains[d][di]) == 0) {
                in_domain = 1;
                break;
            }
        }
        if (in_domain) {
            for (int i = 0; i < NUM_EMITTERS && task->num_extra_emitters < 32; i++) {
                if (i == best) continue;
                // Skip emitters in the same domain as the primary
                int same_domain = 0;
                for (int di = 0; domains[d][di]; di++) {
                    if (strcmp(EMITTER_NAMES[i], domains[d][di]) == 0) { same_domain = 1; break; }
                }
                if (same_domain) continue;
                // Skip input-reading emitters if primary also reads input
                if (best_reads_input) {
                    int is_input_reader = 0;
                    for (int ri = 0; input_readers[ri]; ri++)
                        if (strcmp(EMITTER_NAMES[i], input_readers[ri]) == 0) { is_input_reader = 1; break; }
                    if (is_input_reader) continue;
                }
                if (emitter_scores[i] >= threshold && emitter_scores[i] >= 0.5f) {
                    task->extra_emitters[task->num_extra_emitters++] = i;
                }
            }
            break;
        }
    }
    if (!in_domain) {
        // Fallback: original behavior for undomained emitters
        for (int i = 0; i < NUM_EMITTERS && task->num_extra_emitters < 32; i++) {
            if (i == best) continue;
            // Skip input-reading emitters if primary also reads input
            if (best_reads_input) {
                int is_input_reader = 0;
                for (int ri = 0; input_readers[ri]; ri++)
                    if (strcmp(EMITTER_NAMES[i], input_readers[ri]) == 0) { is_input_reader = 1; break; }
                if (is_input_reader) continue;
            }
            if (emitter_scores[i] >= threshold && emitter_scores[i] >= 0.5f) {
                task->extra_emitters[task->num_extra_emitters++] = i;
            }
        }
    }

    if (verbose_flag && task->num_extra_emitters > 0) {
        printf("  extra emitters:");
        for (int ei = 0; ei < task->num_extra_emitters; ei++)
            printf(" %s", EMITTER_NAMES[task->extra_emitters[ei]]);
        printf("\n");
    }

    return best;
}

int has_actionable_keyword(const char *text) {
    // arithmetic
    if (has_word(text, "add") || has_word(text, "sum") || has_word(text, "summe")
        || has_word(text, "plus") || has_word(text, "berechne") || has_word(text, "ermittle")
        || has_word(text, "calculate") || has_word(text, "compute") || has_word(text, "count")
        || has_word(text, "zähle") || has_word(text, "anzahl"))
        return 1;
    if (has_word(text, "sub") || has_word(text, "subtract") || has_word(text, "minus")
        || has_word(text, "difference") || has_word(text, "differenz"))
        return 1;
    if (has_word(text, "mul") || has_word(text, "multiply") || has_word(text, "mal")
        || has_word(text, "times") || has_word(text, "product") || has_word(text, "produkt"))
        return 1;
    if (has_word(text, "div") || has_word(text, "divide") || has_word(text, "geteilt")
        || has_word(text, "quotient"))
        return 1;
    if (has_word(text, "mod") || has_word(text, "modulo") || has_word(text, "remainder")
        || has_word(text, "rest"))
        return 1;
    if (has_word(text, "power") || has_word(text, "potenz") || has_word(text, "exponent")
        || has_word(text, "hoch") || has_word(text, "square") || has_word(text, "quadrat")
        || has_word(text, "cube") || has_word(text, "kubik") || has_word(text, "sqrt")
        || has_word(text, "wurzel") || has_word(text, "root"))
        return 1;

    // comparison
    if (has_word(text, "max") || has_word(text, "größt") || has_word(text, "largest")
        || has_word(text, "greatest") || has_word(text, "groesst") || has_word(text, "maximum")
        || has_word(text, "minimum") || has_word(text, "min") || has_word(text, "kleinste")
        || has_word(text, "smallest") || has_word(text, "compare") || has_word(text, "vergleich"))
        return 1;

    // io
    if (has_word(text, "lies") || has_word(text, "read") || has_word(text, "input")
        || has_word(text, "gib") || has_word(text, "enter") || has_word(text, "erfasse")
        || has_word(text, "collect") || has_word(text, "eingabe") || has_word(text, "einlesen"))
        return 1;
    if (has_word(text, "print") || has_word(text, "druck") || has_word(text, "ausg")
        || has_word(text, "show") || has_word(text, "display") || has_word(text, "zeig")
        || has_word(text, "output") || has_word(text, "schreib") || has_word(text, "write")
        || has_word(text, "ausgeben"))
        return 1;

    // sorting / searching
    if (has_word(text, "sort") || has_word(text, "sortiere") || has_word(text, "bubble")
        || has_word(text, "sorted") || has_word(text, "sortiert") || has_word(text, "order")
        || has_word(text, "ordne") || has_word(text, "selection") || has_word(text, "insertion"))
        return 1;
    if (has_word(text, "find") || has_word(text, "search") || has_word(text, "suche")
        || has_word(text, "lookup") || has_word(text, "filter") || has_word(text, "select")
        || has_word(text, "choose") || has_word(text, "wähle") || has_word(text, "auswählen"))
        return 1;

    // sequence transformation
    if (has_word(text, "reverse") || has_word(text, "umkehr") || has_word(text, "swap")
        || has_word(text, "tausche") || has_word(text, "rotate") || has_word(text, "shift")
        || has_word(text, "verschieb") || has_word(text, "shuffle") || has_word(text, "misch"))
        return 1;

    // aggregate
    if (has_word(text, "average") || has_word(text, "durchschnitt") || has_word(text, "mean")
        || has_word(text, "median") || has_word(text, "standard") || has_word(text, "deviation"))
        return 1;

    // number theory
    if (has_word(text, "factorial") || has_word(text, "fakult") || has_word(text, "fibo")
        || has_word(text, "fibonacci") || has_word(text, "prime") || has_word(text, "prim")
        || has_word(text, "fizzbuzz") || has_word(text, "gcd") || has_word(text, "ggt")
        || has_word(text, "gcm") || has_word(text, "lcm") || has_word(text, "kgv")
        || has_word(text, "collatz") || has_word(text, "palindrome") || has_word(text, "palindrom")
        || has_word(text, "armstrong") || has_word(text, "perfect") || has_word(text, "perfekt"))
        return 1;

    // even/odd / boolean
    if (has_word(text, "even") || has_word(text, "odd") || has_word(text, "gerade")
        || has_word(text, "ungerade") || has_word(text, "leap") || has_word(text, "schalt"))
        return 1;

    // loops / conditions
    if (has_word(text, "for") || has_word(text, "loop") || has_word(text, "schleife")
        || has_word(text, "while") || has_word(text, "switch") || has_word(text, "case")
        || has_word(text, "if") || has_word(text, "bedingung") || has_word(text, "wenn"))
        return 1;

    // data structures
    if (has_word(text, "array") || has_word(text, "feld") || has_word(text, "liste")
        || has_word(text, "list") || has_word(text, "stack") || has_word(text, "queue")
        || has_word(text, "pointer") || has_word(text, "zeiger") || has_word(text, "struct")
        || has_word(text, "tree") || has_word(text, "baum") || has_word(text, "graph")
        || has_word(text, "linked") || has_word(text, "verkettet"))
        return 1;

    // string operations
    if (has_word(text, "string") || has_word(text, "zeichen") || has_word(text, "text")
        || has_word(text, "cat") || has_word(text, "concat") || has_word(text, "vowel")
        || has_word(text, "vokal") || has_word(text, "anagram") || has_word(text, "upper")
        || has_word(text, "lower") || has_word(text, "groß") || has_word(text, "klein")
        || has_word(text, "caesar") || has_word(text, "cipher") || has_word(text, "chiffre")
        || has_word(text, "length") || has_word(text, "länge") || has_word(text, "len"))
        return 1;

    // conversion
    if (has_word(text, "convert") || has_word(text, "parse") || has_word(text, "umwand")
        || has_word(text, "decimal") || has_word(text, "dezimal") || has_word(text, "binary")
        || has_word(text, "binär") || has_word(text, "hex") || has_word(text, "hexadezimal")
        || has_word(text, "celsius") || has_word(text, "fahrenheit") || has_word(text, "temp"))
        return 1;

    // games / simulation
    if (has_word(text, "guess") || has_word(text, "rate") || has_word(text, "raten")
        || has_word(text, "dice") || has_word(text, "würfel") || has_word(text, "wuerfel")
        || has_word(text, "random") || has_word(text, "zufall") || has_word(text, "game")
        || has_word(text, "spiel") || has_word(text, "rock") || has_word(text, "paper")
        || has_word(text, "scissors") || has_word(text, "schere") || has_word(text, "stein"))
        return 1;

    // math utilities
    if (has_word(text, "table") || has_word(text, "einmaleins") || has_word(text, "time")
        || has_word(text, "zeit") || has_word(text, "clock") || has_word(text, "countdown")
        || has_word(text, "function") || has_word(text, "funktion") || has_word(text, "area")
        || has_word(text, "fläche") || has_word(text, "circle") || has_word(text, "kreis")
        || has_word(text, "bmi") || has_word(text, "interest") || has_word(text, "zins")
        || has_word(text, "pyramid") || has_word(text, "pyramide") || has_word(text, "matrix"))
        return 1;

    // file / system
    if (has_word(text, "file") || has_word(text, "datei") || has_word(text, "shell")
        || has_word(text, "argument") || has_word(text, "parameter") || has_word(text, "env")
        || has_word(text, "timer") || has_word(text, "benchmark") || has_word(text, "measure")
        || has_word(text, "mess") || has_word(text, "dauer") || has_word(text, "execution")
        || has_word(text, "ausführ") || has_word(text, "lauf"))
        return 1;

    // hello / misc
    if (has_word(text, "hello") || has_word(text, "hallo") || has_word(text, "hello")
        || has_word(text, "hi") || has_word(text, "name"))
        return 1;

    // general action verbs
    if (has_word(text, "generate") || has_word(text, "create") || has_word(text, "erzeuge")
        || has_word(text, "erstelle") || has_word(text, "bau") || has_word(text, "build")
        || has_word(text, "check") || has_word(text, "prüfe") || has_word(text, "teste")
        || has_word(text, "verify") || has_word(text, "überprüf") || has_word(text, "solve")
        || has_word(text, "löse") || has_word(text, "set") || has_word(text, "initialize")
        || has_word(text, "initialisiere") || has_word(text, "setze") || has_word(text, "analyze")
        || has_word(text, "analyse") || has_word(text, "untersuche") || has_word(text, "determine")
        || has_word(text, "bestimme") || has_word(text, "evaluate") || has_word(text, "auswerten"))
        return 1;

    // nouns as context
    if (has_word(text, "ergebnis") || has_word(text, "result") || has_word(text, "value")
        || has_word(text, "wert") || has_word(text, "number") || has_word(text, "zahl")
        || has_word(text, "element") || has_word(text, "index") || has_word(text, "bis")
        || has_word(text, "to") || has_word(text, "von") || has_word(text, "from")
        || has_word(text, "data") || has_word(text, "daten") || has_word(text, "sequence")
        || has_word(text, "folge") || has_word(text, "reihe") || has_word(text, "series"))
        return 1;

    // boolean
    if (has_word(text, "fann") || has_word(text, "neural") || has_word(text, "network")
        || has_word(text, "train") || has_word(text, "learn") || has_word(text, "predict")
        || has_word(text, "infer"))
        return 1;

    return 0;
}

int has_sequential_pattern(const char *text) {
    // Check if text contains numbered steps like "1)...2)..." or "step 1...step 2..."
    int count_enumerated = 0;
    const char *p = text;
    while (*p && count_enumerated < 2) {
        if ((p[0] >= '1' && p[0] <= '9') && (p[1] == ')' || (p[1] == '.' && p[2] != '.' && !(p[2] >= '0' && p[2] <= '9')))) { count_enumerated++; p += 2; }
        else if ((p[0] == '(') && (p[1] >= '1' && p[1] <= '9') && p[2] == ')') { count_enumerated++; p += 3; }
        else if (strncmp(p, "step ", 5) == 0 && p[5] >= '1' && p[5] <= '9') { count_enumerated++; p += 6; }
        else p++;
    }
    if (count_enumerated >= 2) return 1;

    // Check for bullet points: lines starting with "-", "*", "+"
    int bullet_count = 0;
    const char *bp = text;
    while (*bp) {
        while (*bp && *bp == ' ') bp++;
        if (*bp == '-' || *bp == '*' || *bp == '+') { bullet_count++; if (bullet_count >= 2) return 1; }
        while (*bp && *bp != '\n') bp++;
        if (*bp == '\n') bp++;
    }

    // Check for multi-step sequential adverbs: at least 2 of "first, then, finally"
    // Count multiple occurrences of "then" (e.g. "input numbers then sort then print")
    int seq_count = 0;
    if (has_word(text, "first") || has_word(text, "zuerst") || has_word(text, "erstens")) seq_count++;
    if (has_word(text, "second") || has_word(text, "zweitens") || has_word(text, "zweit")) seq_count++;
    if (has_word(text, "third") || has_word(text, "drittens") || has_word(text, "dritt")) seq_count++;
    if (has_word(text, "fourth") || has_word(text, "viertens") || has_word(text, "viert")) seq_count++;
    if (has_word(text, "fifth") || has_word(text, "fünftens") || has_word(text, "fuenft")) seq_count++;
    if (has_word(text, "finally") || has_word(text, "schließlich") || has_word(text, "abschließend")) seq_count++;
    { const char *tp = text; while ((tp = strstr(tp, "then")) != NULL) { seq_count++; tp++; } }
    { const char *tp = text; while ((tp = strstr(tp, "dann")) != NULL) { seq_count++; tp++; } }
    if (has_word(text, "next") || has_word(text, "als nächstes") || has_word(text, "danach")) seq_count++;
    if (seq_count >= 2) return 1;
    return 0;
}

int split_prompt_steps(const char *prompt, char steps[MAX_STEPS][MAX_PROMPT]) {
    char buf[MAX_PROMPT];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", prompt);
    to_lowercase(buf);
    int num_steps = 0;
    char *remaining = buf;

    // Phase 1: Newline-based splitting (bullet points, numbered lists, line-separated items)
    // Detect if prompt contains explicit line breaks with list markers
    {
        int newline_count = 0;
        char *nlp = buf;
        while (*nlp) { if (*nlp == '\n') newline_count++; nlp++; }
        if (newline_count >= 2) {
            char *line = buf;
            char *next;
            while (line && num_steps < MAX_STEPS) {
                next = strchr(line, '\n');
                if (next) *next = '\0';
                trim(line);
                // Strip leading bullet/number markers
                char *content = line;
                while (*content == ' ' || *content == '\t') content++;
                if (*content == '-' || *content == '*' || *content == '+') { content++; while (*content == ' ') content++; }
                else {
                    while ((*content >= '0' && *content <= '9') || *content == '(' || *content == ')' || *content == '.') content++;
                    while (*content == ' ') content++;
                }
                if (strlen(content) > 0 && has_actionable_keyword(content)) {
                    SNPRINTF_CHECK(steps[num_steps], MAX_PROMPT, "%s", content);
                    num_steps++;
                }
                if (next) { *next = '\n'; line = next + 1; }
                else break;
            }
            if (num_steps >= 2) return num_steps;
            num_steps = 0;
        }
    }

    // Phase 2: Numbered/enumerated steps (high confidence)
    // e.g., "step 1: sort 5 numbers step 2: reverse the array"
    //        "1) sort 5 numbers 2) reverse the array"
    //        "(1) sort 5 numbers (2) reverse the array"
    //        "1. sort 5 numbers 2. reverse the array"
    if (has_sequential_pattern(buf)) {
        // Try bullet point first (dash/asterisk/plus lists)
        {
            char *bp = remaining;
            char bullet_steps[MAX_STEPS][MAX_PROMPT];
            int bs_count = 0;
            while (*bp && bs_count < MAX_STEPS) {
                while (*bp == ' ') bp++;
                if (*bp == '-' || *bp == '*' || *bp == '+') {
                    bp++; while (*bp == ' ') bp++;
                    char *start = bp;
                    while (*bp && *bp != '\n' &&
                           !(*bp == ' ' && (bp[1] == '-' || bp[1] == '*' || bp[1] == '+'))) bp++;
                    char saved = *bp;
                    *bp = '\0';
                    trim(start);
                    if (strlen(start) > 0) {
                        SNPRINTF_CHECK(bullet_steps[bs_count], MAX_PROMPT, "%s", start);
                        bs_count++;
                    }
                    *bp = saved;
                    if (*bp == '\n') bp++;
                } else {
                    while (*bp && *bp != '\n') bp++;
                    if (*bp == '\n') bp++;
                }
            }
            if (bs_count >= 2) {
                for (int bsi = 0; bsi < bs_count; bsi++)
                    SNPRINTF_CHECK(steps[num_steps++], MAX_PROMPT, "%.8191s", bullet_steps[bsi]);
                return num_steps;
            }
        }

        char *p = remaining;
        char current[MAX_PROMPT] = "";
        int collecting = 0;
        while (*p && num_steps < MAX_STEPS - 1) {
            int step_found = 0;
            if ((p[0] >= '1' && p[0] <= '9') && (p[1] == ')' || (p[1] == '.' && p[2] != '.' && !(p[2] >= '0' && p[2] <= '9')))) {
                if (collecting && strlen(current) > 0) {
                    for (char *cp = current + 1; *cp; cp++) {
                        if (((cp[0] >= '1' && cp[0] <= '9') && (cp[1] == ')' || (cp[1] == '.' && cp[2] != '.' && !(cp[2] >= '0' && cp[2] <= '9'))))
                            || (cp[0] == '(' && cp[1] >= '1' && cp[1] <= '9' && cp[2] == ')')
                            || strncmp(cp, "step ", 5) == 0 || strncmp(cp, "schritt ", 8) == 0) { *cp = '\0'; break; }
                    }
                    trim(current);
                    SNPRINTF_CHECK(steps[num_steps], MAX_PROMPT, "%s", current);
                    num_steps++;
                }
                p += 2; while (*p && isspace(*p)) p++;
                SNPRINTF_CHECK(current, sizeof(current), "%s", p);
                collecting = 1;
                step_found = 1;
            } else if ((p[0] == '(') && (p[1] >= '1' && p[1] <= '9') && p[2] == ')') {
                if (collecting && strlen(current) > 0) {
                    for (char *cp = current + 1; *cp; cp++) {
                        if (((cp[0] >= '1' && cp[0] <= '9') && (cp[1] == ')' || (cp[1] == '.' && cp[2] != '.' && !(cp[2] >= '0' && cp[2] <= '9'))))
                            || (cp[0] == '(' && cp[1] >= '1' && cp[1] <= '9' && cp[2] == ')')
                            || strncmp(cp, "step ", 5) == 0 || strncmp(cp, "schritt ", 8) == 0) { *cp = '\0'; break; }
                    }
                    trim(current);
                    SNPRINTF_CHECK(steps[num_steps], MAX_PROMPT, "%s", current);
                    num_steps++;
                }
                p += 3; while (*p && isspace(*p)) p++;
                SNPRINTF_CHECK(current, sizeof(current), "%s", p);
                collecting = 1;
                step_found = 1;
            } else if ((strncmp(p, "step ", 5) == 0 && p[5] >= '1' && p[5] <= '9') || (strncmp(p, "schritt ", 8) == 0 && p[8] >= '1' && p[8] <= '9')) {
                int is_step = (strncmp(p, "step ", 5) == 0);
                int prefix_len = is_step ? 5 : 8;
                if (collecting && strlen(current) > 0) {
                    for (char *cp = current + 1; *cp; cp++) {
                        if (((cp[0] >= '1' && cp[0] <= '9') && (cp[1] == ')' || (cp[1] == '.' && cp[2] != '.' && !(cp[2] >= '0' && cp[2] <= '9'))))
                            || (cp[0] == '(' && cp[1] >= '1' && cp[1] <= '9' && cp[2] == ')')
                            || strncmp(cp, "step ", 5) == 0 || strncmp(cp, "schritt ", 8) == 0) { *cp = '\0'; break; }
                    }
                    trim(current);
                    SNPRINTF_CHECK(steps[num_steps], MAX_PROMPT, "%s", current);
                    num_steps++;
                }
                p += prefix_len; while (*p && isspace(*p)) p++;
                if (*p == ':') p++;
                while (*p && isspace(*p)) p++;
                SNPRINTF_CHECK(current, sizeof(current), "%s", p);
                collecting = 1;
                step_found = 1;
            }
            if (!step_found) { p++; }
            else {
                while (*p && !((p[0] >= '1' && p[0] <= '9' && (p[1] == ')' || (p[1] == '.' && p[2] != '.' && !(p[2] >= '0' && p[2] <= '9'))))
                       || (p[0] == '(' && p[1] >= '1' && p[1] <= '9' && p[2] == ')')
                       || strncmp(p, "step ", 5) == 0 || strncmp(p, "schritt ", 8) == 0)) p++;
            }
        }
        if (collecting && strlen(current) > 0) {
            trim(current);
            SNPRINTF_CHECK(steps[num_steps], MAX_PROMPT, "%s", current);
            num_steps++;
        }
        if (num_steps >= 2) {
            for (int si = 0; si < num_steps; si++) {
                char *s = steps[si];
                while (*s && ((*s >= '0' && *s <= '9') || *s == ')' || *s == '(' || *s == '.')) s++;
                while (*s && isspace(*s)) s++;
                if (s > steps[si]) memmove(steps[si], s, strlen(s) + 1);
            }
            return num_steps;
        }
        num_steps = 0;
        remaining = buf;
    }

    // Phase 3: Conjunction-based splitting with extended pattern set
    struct { const char *pat; int len; } patterns[] = {
        // Long multi-word conjunctions (highest priority)
        {" und dann ", 10}, {" und danach ", 12}, {" und anschließend ", 18},
        {" anschließend ", 14},
        {" als nächstes ", 14}, {" zum Schluss ", 13},
        {" and then ", 10}, {" and finally ", 12},
        {" first ", 7}, {" then ", 6}, {" finally ", 9},
        {" next ", 6}, {" afterwards ", 12}, {" after that ", 12},
        // German sequential
        {" danach ", 8}, {" daraufhin ", 11}, {" zuerst ", 8},
        {" als erstes ", 12}, {" als zweites ", 13}, {" als drittes ", 13},
        {" nun ", 5},
        // English sequential
        {" firstly ", 9}, {" secondly ", 10}, {" thirdly ", 9},
        {" subsequently ", 14}, {" thereafter ", 12},
        // Punctuation (medium confidence)
        {" . ", 3}, {". ", 2}, {"; ", 2}, {";", 1}, {": ", 2},
        // Short conjunctions (lowest priority)
        {" und ", 5}, {" and ", 5}, {", ", 2},
    };
    int npats = sizeof(patterns) / sizeof(patterns[0]);

    while (num_steps < MAX_STEPS - 1) {
        int best_pos = -1, best_len = 0;
        for (int i = 0; i < npats; i++) {
            char *pos = strstr(remaining, patterns[i].pat);
            if (!pos) continue;
            int idx = pos - remaining;
            if (best_pos >= 0 && idx > best_pos) continue;
            if (best_pos >= 0 && idx == best_pos) continue;
            best_pos = idx; best_len = patterns[i].len;
        }
        if (best_pos < 0) break;
        char step[MAX_PROMPT];
        SNPRINTF_CHECK(step, MAX_PROMPT, "%.*s", best_pos, remaining);
        trim(step);
        if (strlen(step) > 0 && has_actionable_keyword(step)) {
            // Avoid splitting on short conjunctions (" und ", " and ", ", ") when
            // both sides are short (< 4 words each), as this is likely a list
            // (e.g. "minimum und maximum") rather than sequential action steps,
            // OR when both sides contain arithmetic operation keywords (e.g.
            // "addiere 3 und multipliziere mit 10" → one compound action).
            int is_short_conj = (best_len <= 5);
            if (is_short_conj) {
                char *rest = remaining + best_pos + best_len;
                // Comma before "and"/"und"/"or"/"oder" is punctuation, not step separator
                if (best_len == 2) {
                    char *rp = rest;
                    while (*rp == ' ') rp++;
                    if (strncmp(rp, "and ", 4) == 0 || strncmp(rp, "und ", 4) == 0
                        || strncmp(rp, "or ", 3) == 0 || strncmp(rp, "oder ", 5) == 0)
                    {
                        char combined[MAX_PROMPT * 2];
                        SNPRINTF_CHECK(combined, sizeof(combined), "%s %s", step, rp);
                        trim(combined);
                        SNPRINTF_CHECK(remaining, MAX_PROMPT, "%.*s", MAX_PROMPT - 1, combined);
                        break;
                    }
                }
                int left_has_op = has_word(step, "add") || has_word(step, "sub") || has_word(step, "mul") || has_word(step, "div")
                    || has_word(step, "addiere") || has_word(step, "subtrahier") || has_word(step, "multiplizier")
                    || has_word(step, "dividier") || has_word(step, "plus") || has_word(step, "minus")
                    || has_word(step, "mal") || has_word(step, "durch");
                int right_has_op = has_word(rest, "add") || has_word(rest, "sub") || has_word(rest, "mul") || has_word(rest, "div")
                    || has_word(rest, "addiere") || has_word(rest, "subtrahier") || has_word(rest, "multiplizier")
                    || has_word(rest, "dividier") || has_word(rest, "plus") || has_word(rest, "minus")
                    || has_word(rest, "mal") || has_word(rest, "durch");
                int sw = 0, rw = 0;
                char tmp[MAX_PROMPT];
                char *saveptr;
                SNPRINTF_CHECK(tmp, sizeof(tmp), "%s", step);
                char *t = strtok_r(tmp, " ", &saveptr);
                while (t) { trim(t); if (strlen(t) > 0) sw++; t = strtok_r(NULL, " ", &saveptr); }
                SNPRINTF_CHECK(tmp, sizeof(tmp), "%s", rest);
                t = strtok_r(tmp, " ", &saveptr);
                while (t) { trim(t); if (strlen(t) > 0) rw++; t = strtok_r(NULL, " ", &saveptr); }
                if ((sw < 4 && rw < 4) || (left_has_op && right_has_op)) {
                    // False split – merge and continue as single step
                    char combined[MAX_PROMPT * 2];
                    SNPRINTF_CHECK(combined, sizeof(combined), "%s %s", step, rest);
                    trim(combined);
                    SNPRINTF_CHECK(remaining, MAX_PROMPT, "%.*s", MAX_PROMPT - 1, combined);
                    break;
                }
            }
            SNPRINTF_CHECK(steps[num_steps], MAX_PROMPT, "%s", step); num_steps++;
            remaining += best_pos + best_len;
        } else if (strlen(step) > 0) {
            char *rest = remaining + best_pos + best_len;
            char combined[MAX_PROMPT * 2];
            SNPRINTF_CHECK(combined, sizeof(combined), "%s %s", step, rest);
            trim(combined);
            SNPRINTF_CHECK(remaining, MAX_PROMPT, "%.*s", MAX_PROMPT - 1, combined);
            break;
        } else {
            remaining += best_pos + best_len;
        }
    }
    trim(remaining);
    if (strlen(remaining) > 0) { SNPRINTF_CHECK(steps[num_steps], MAX_PROMPT, "%s", remaining); num_steps++; }

    if (verbose_flag) {
        fprintf(stderr, "DEBUG split: num_steps=%d steps:", num_steps);
        for (int d = 0; d < num_steps; d++) fprintf(stderr, " [%d]='%s'", d, steps[d]);
        fprintf(stderr, "\n");
    }
    // Phase 4: Merge non-viable steps backward into their predecessor
    if (num_steps > 1) {
        int merged = 1;
        while (merged) {
            merged = 0;
            for (int i = 0; i < num_steps; i++) {
                if (!has_actionable_keyword(steps[i])) {
                    if (i > 0) {
                    char merged_step[MAX_PROMPT * 2];
                    SNPRINTF_CHECK(merged_step, sizeof(merged_step), "%.*s %.*s", MAX_PROMPT - 1, steps[i-1], MAX_PROMPT - 1, steps[i]);
                        trim(merged_step);
                        SNPRINTF_CHECK(steps[i-1], MAX_PROMPT, "%.*s", MAX_PROMPT - 1, merged_step);
                        for (int j = i; j < num_steps - 1; j++) memmove(steps[j], steps[j+1], MAX_PROMPT);
                        num_steps--; merged = 1; break;
                    } else if (num_steps > 1) {
                        char merged_step[MAX_PROMPT * 2];
                        SNPRINTF_CHECK(merged_step, sizeof(merged_step), "%.*s %.*s", MAX_PROMPT - 1, steps[0], MAX_PROMPT - 1, steps[1]);
                        trim(merged_step);
                        SNPRINTF_CHECK(steps[0], MAX_PROMPT, "%.*s", MAX_PROMPT - 1, merged_step);
                        for (int j = 1; j < num_steps - 1; j++) memmove(steps[j], steps[j+1], MAX_PROMPT);
                        num_steps--; merged = 1; break;
                    }
                }
            }
        }
    }

    // Phase 5: Merge "print/display/show the result" with preceding step
    // Only merge when the print step explicitly mentions a result word (not generic pronouns
    // like "them"/"it" which cause cascading merges that collapse multi-step prompts).
    if (num_steps > 1) {
        int merged = 1;
        while (merged) {
            merged = 0;
            for (int i = 1; i < num_steps; i++) {
                if ((has_word(steps[i], "print") || has_word(steps[i], "show")
                     || has_word(steps[i], "display") || has_word(steps[i], "ausgeb")
                     || has_word(steps[i], "zeig") || strstr(steps[i], "gib aus"))
                    && (has_word(steps[i], "median") || has_word(steps[i], "average")
                     || has_word(steps[i], "mean") || has_word(steps[i], "largest")
                     || has_word(steps[i], "smallest") || has_word(steps[i], "greatest")
                     || has_word(steps[i], "sum") || has_word(steps[i], "max")
                     || has_word(steps[i], "min")
                     || has_word(steps[i], "result")
                     || has_word(steps[i], "ergebnis") || has_word(steps[i], "resultat")
                     || has_word(steps[i], "sorted") || has_word(steps[i], "sortiert")
                     || has_word(steps[i], "output") || has_word(steps[i], "ausgabe")
                     || has_word(steps[i], "calculated") || has_word(steps[i], "computed")
                     || has_word(steps[i], "berech") || has_word(steps[i], "ermittelt"))) {
                    char merged_step[MAX_PROMPT * 2];
                    SNPRINTF_CHECK(merged_step, sizeof(merged_step), "%.*s %.*s", MAX_PROMPT - 1, steps[i-1], MAX_PROMPT - 1, steps[i]);
                    trim(merged_step);
                    SNPRINTF_CHECK(steps[i-1], MAX_PROMPT, "%.*s", MAX_PROMPT - 1, merged_step);
                    for (int j = i; j < num_steps - 1; j++) memmove(steps[j], steps[j+1], MAX_PROMPT);
                    num_steps--; merged = 1; break;
                }
            }
        }
    }

    // Phase 6: For long prompts with many "and" / "," splits, merge consecutive
    // steps that belong together (e.g., "read 5 numbers" + "and sort them" → keep separate if both have keywords)
    // This is intentionally a no-op - both have keywords so they stay separate.

    if (num_steps <= 1) { SNPRINTF_CHECK(steps[0], MAX_PROMPT, "%s", prompt); return 1; }
    return num_steps;
}

// ==================== VECTOR SEARCH ====================

float cosine_sim(const float *a, const float *b) {
    float dot = 0, na = 0, nb = 0;
    for (int i = 0; i < EMBED_DIM; i++) {
        dot += a[i] * b[i];
        na += a[i] * a[i];
        nb += b[i] * b[i];
    }
    float denom = sqrtf(na) * sqrtf(nb);
    return (denom == 0) ? 0 : dot / denom;
}

void embed_text(const char *text, float *out) {
    memset(out, 0, sizeof(float) * EMBED_DIM);
    char buf[MAX_PROMPT];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", text);
    to_lowercase(buf);

    float total_weight = 0;
    char *p = buf;
    while (*p) {
        while (*p && !isalpha(*p)) p++;
        if (!*p) break;
        char *start = p;
        while (*p && isalpha(*p)) p++;
        char saved = *p;
        *p = '\0';

        int tok_id = -1;
        for (int i = 0; i < VOCAB_SIZE; i++) {
            if (strcmp(start, vocab[i]) == 0) { tok_id = i; break; }
        }
        if (tok_id < 0) {
            const char *syn = resolve_synonym(start);
            if (syn) {
                for (int i = 0; i < VOCAB_SIZE; i++) {
                    if (strcmp(syn, vocab[i]) == 0) { tok_id = i; break; }
                }
            }
        }

        if (tok_id >= 0) {
            float w = idf_weights[tok_id];
            for (int j = 0; j < EMBED_DIM; j++)
                out[j] += w * word_embeddings[tok_id].embed[j];
            total_weight += w;
        } else {
            int len = strlen(start);
            if (len >= 2) {
                float ngram_embed[EMBED_DIM];
                memset(ngram_embed, 0, sizeof(ngram_embed));
                int ngram_count = 0;
                for (int ci = 0; ci < len - 1; ci++) {
                    char bigram[3] = {start[ci], start[ci+1], '\0'};
                    unsigned long h = hash_word(bigram);
                    srand(h);
                    for (int j = 0; j < EMBED_DIM; j++)
                        ngram_embed[j] += ((float)rand() / (float)RAND_MAX) * 2.0f - 1.0f;
                    ngram_count++;
                }
                if (ngram_count > 0) {
                    float inv = 1.0f / ngram_count;
                    float norm = 0;
                    for (int j = 0; j < EMBED_DIM; j++) {
                        ngram_embed[j] *= inv;
                        norm += ngram_embed[j] * ngram_embed[j];
                    }
                    norm = sqrtf(norm);
                    if (norm > 0) {
                        for (int j = 0; j < EMBED_DIM; j++)
                            out[j] += ngram_embed[j] / norm;
                        total_weight += 1.0f;
                    }
                }
            }
        }

        *p = saved;
    }

    if (total_weight > 0) {
        float inv = 1.0f / total_weight;
        for (int j = 0; j < EMBED_DIM; j++) out[j] *= inv;
    }
}

void filename_stem(const char *path, char *stem) {
    const char *p = strrchr(path, '/');
    p = p ? p + 1 : path;
    char buf[256];
    SNPRINTF_CHECK(buf, sizeof(buf), "%s", p);
    char *dot = strrchr(buf, '.');
    if (dot) *dot = '\0';
    for (char *q = buf; *q; q++)
        if (*q == '-' || *q == '_') *q = ' ';
    SNPRINTF_CHECK(stem, 256, "%s", buf);
}

void index_examples(void) {
    if (examples_indexed) return;
    examples_indexed = 1;
    num_examples = 0;

    for (int s = 0; s < EXAMPLE_SUBDIRS; s++) {
        char dirpath[512];
        SNPRINTF_CHECK(dirpath, sizeof(dirpath), "%s/%s", EXAMPLE_DIR, example_subdirs[s]);
        DIR *d = opendir(dirpath);
        if (!d) continue;

        struct dirent *entry;
        while ((entry = readdir(d)) != NULL && num_examples < MAX_EXAMPLES) {
            const char *ext = strrchr(entry->d_name, '.');
            if (!ext) continue;
            int valid_ext = 0;
            for (int e = 0; e < EXAMPLE_EXTS; e++) {
                if (strcmp(ext, example_exts[e]) == 0) { valid_ext = 1; break; }
            }
            if (!valid_ext) continue;

            char fullpath[1024];
            SNPRINTF_CHECK(fullpath, sizeof(fullpath), "%s/%s", dirpath, entry->d_name);
            SNPRINTF_CHECK(example_docs[num_examples].filename, sizeof(example_docs[num_examples].filename), "%s", fullpath);

            char raw_stem[256];
            filename_stem(entry->d_name, raw_stem);
            SNPRINTF_CHECK(example_docs[num_examples].stem, sizeof(example_docs[num_examples].stem), "%s/%s", example_subdirs[s], raw_stem);

            char content[8192] = {0};
            FILE *f = fopen(fullpath, "r");
            if (f) {
                char line[512];
                int lines_read = 0;
                while (fgets(line, sizeof(line), f) && lines_read < 100 && strlen(content) < 7000) {
                    char *trimmed = line;
                    while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
                    if (trimmed[0] == '/' && trimmed[1] == '/') continue;
                    strncat(content, line, sizeof(content) - strlen(content) - 1);
                    lines_read++;
                }
                fclose(f);
            }
            char combined[10000];
            SNPRINTF_CHECK(combined, sizeof(combined), "%s %s %s %s",
                     example_docs[num_examples].stem,
                     example_docs[num_examples].stem,
                     example_docs[num_examples].stem,
                     content);
            embed_text(combined, example_docs[num_examples].embedding);
            num_examples++;
        }
        closedir(d);
    }

    if (num_examples > 0) {
        int doc_freq[VOCAB_SIZE];
        memset(doc_freq, 0, sizeof(doc_freq));
        for (int i = 0; i < num_examples; i++) {
            int seen[VOCAB_SIZE] = {0};
            char buf[MAX_PROMPT];
            SNPRINTF_CHECK(buf, sizeof(buf), "%s", example_docs[i].stem);
            int tokens[128];
            int n = tokenize(buf, tokens, 128);
            for (int t = 0; t < n; t++) {
                if (!seen[tokens[t]]) {
                    seen[tokens[t]] = 1;
                    doc_freq[tokens[t]]++;
                }
            }
        }
        for (int i = 0; i < VOCAB_SIZE; i++) {
            if (doc_freq[i] > 0)
                idf_weights[i] = logf((float)num_examples / doc_freq[i]) + 1.0f;
            else
                idf_weights[i] = 1.0f;
        }
    }
}

int search_examples(const char *query, int top_k, int *indices, float *scores) {
    if (num_examples == 0) return 0;

    char expanded[MAX_PROMPT];
    expand_query(query, expanded, sizeof(expanded));

    float q_embed[EMBED_DIM];
    embed_text(expanded, q_embed);

    for (int i = 0; i < num_examples; i++)
        example_docs[i].score = cosine_sim(q_embed, example_docs[i].embedding);

    int count = top_k < num_examples ? top_k : num_examples;
    int used[MAX_EXAMPLES] = {0};
    int result = 0;
    for (int k = 0; k < count; k++) {
        int best = -1;
        for (int i = 0; i < num_examples; i++) {
            if (used[i]) continue;
            if (best < 0 || example_docs[i].score > example_docs[best].score)
                best = i;
        }
        if (best >= 0 && example_docs[best].score > 0) {
            indices[k] = best;
            scores[k] = example_docs[best].score;
            used[best] = 1;
            result++;
        }
    }

    return result;
}
