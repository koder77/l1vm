/*
 * This file l1vm-func.c is part of L1vm.
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

// Antigravity created

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "../include/global.h"

#define MAX_CFUNCS 1000
#define LINE_BUF_SIZE 2048

struct cfunc {
    char name[128];
    char pop_list[256];
    char push_list[256];
};

struct cfunc cfuncs[MAX_CFUNCS];
int cfunc_count = 0;

// Helper to trim leading/trailing whitespace
void trim(char *str) {
    int start = 0;
    while (str[start] && isspace((unsigned char)str[start])) {
        start++;
    }
    if (start > 0) {
        memmove(str, str + start, strlen(str + start) + 1);
    }
    int len = strlen(str);
    while (len > 0 && isspace((unsigned char)str[len - 1])) {
        str[--len] = '\0';
    }
}

// Helper to replace commas with spaces and clean up double spaces
void remove_commas(char *str) {
    char temp[512] = "";
    int t = 0;
    int prev_was_space = 0;
    for (int i = 0; str[i]; i++) {
        char c = str[i];
        if (c == ',') {
            c = ' ';
        }
        if (c == ' ') {
            if (!prev_was_space && t > 0) {
                temp[t++] = ' ';
                prev_was_space = 1;
            }
        } else {
            temp[t++] = c;
            prev_was_space = 0;
        }
    }
    // Trim trailing space if any
    while (t > 0 && temp[t - 1] == ' ') {
        t--;
    }
    temp[t] = '\0';
    strcpy(str, temp);
}

// Parse a line from the C-functions lint file
void parse_cfunc_line(const char *line) {
    if (cfunc_count >= MAX_CFUNCS) return;

    const char *paren1 = strchr(line, '(');
    if (!paren1) return;

    // Extract name (everything before the first '(')
    const char *name_end = paren1;
    int name_len = name_end - line;
    if (name_len >= 128) name_len = 127;
    strncpy(cfuncs[cfunc_count].name, line, name_len);
    cfuncs[cfunc_count].name[name_len] = '\0';
    trim(cfuncs[cfunc_count].name);

    // Extract pop_list (inside the first parentheses)
    const char *paren1_end = strchr(paren1, ')');
    if (!paren1_end) return;
    int pop_len = paren1_end - (paren1 + 1);
    if (pop_len >= 256) pop_len = 255;
    strncpy(cfuncs[cfunc_count].pop_list, paren1 + 1, pop_len);
    cfuncs[cfunc_count].pop_list[pop_len] = '\0';
    trim(cfuncs[cfunc_count].pop_list);
    remove_commas(cfuncs[cfunc_count].pop_list);

    // Extract push_list (inside the second parentheses)
    const char *paren2 = strchr(paren1_end + 1, '(');
    if (!paren2) return;
    const char *paren2_end = strchr(paren2, ')');
    if (!paren2_end) return;
    int push_len = paren2_end - (paren2 + 1);
    if (push_len >= 256) push_len = 255;
    strncpy(cfuncs[cfunc_count].push_list, paren2 + 1, push_len);
    cfuncs[cfunc_count].push_list[push_len] = '\0';
    trim(cfuncs[cfunc_count].push_list);
    remove_commas(cfuncs[cfunc_count].push_list);

    cfunc_count++;
}

// Find a parsed C-function by name
struct cfunc* find_cfunc(const char *name) {
    for (int i = 0; i < cfunc_count; i++) {
        if (strcmp(cfuncs[i].name, name) == 0) {
            return &cfuncs[i];
        }
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    printf ("%s %s\n", argv[0], VM_VERSION_STR);

    if (argc < 4) {
        fprintf(stderr, "Usage: %s <header.l1h> <cfunc_output.txt> <output_header.l1h>\n", argv[0]);
        return 1;
    }

    const char *header_path = argv[1];
    const char *cfunc_path = argv[2];
    const char *output_path = argv[3];

    // 1. Read and parse C-functions lint file
    FILE *cf_file = fopen(cfunc_path, "r");
    if (!cf_file) {
        fprintf(stderr, "Error opening C-functions file '%s': ", cfunc_path);
        perror("");
        return 1;
    }

    char line_buf[LINE_BUF_SIZE];
    while (fgets(line_buf, sizeof(line_buf), cf_file)) {
        trim(line_buf);
        if (strlen(line_buf) > 0) {
            parse_cfunc_line(line_buf);
        }
    }
    fclose(cf_file);

    // 2. Process the L1h header file line by line and insert the annotations
    FILE *in = fopen(header_path, "r");
    if (!in) {
        fprintf(stderr, "Error opening input header file '%s': ", header_path);
        perror("");
        return 1;
    }

    FILE *out = fopen(output_path, "w");
    if (!out) {
        fprintf(stderr, "Error opening output header file '%s': ", output_path);
        perror("");
        fclose(in);
        return 1;
    }

    while (fgets(line_buf, sizeof(line_buf), in)) {
        // Check if this line defines a wrapper function: (NAME func)
        char *func_ptr = strstr(line_buf, " func)");
        if (func_ptr) {
            // Find the matching opening parenthesis '(' backward
            char *paren_ptr = func_ptr;
            while (paren_ptr > line_buf && *paren_ptr != '(') {
                paren_ptr--;
            }

            if (*paren_ptr == '(') {
                // Extract wrapper function name
                char wrapper_name[128];
                int name_len = func_ptr - (paren_ptr + 1);
                if (name_len >= 128) name_len = 127;
                strncpy(wrapper_name, paren_ptr + 1, name_len);
                wrapper_name[name_len] = '\0';
                trim(wrapper_name);

                // Look up in C-functions
                struct cfunc *cf = find_cfunc(wrapper_name);
                if (cf) {
                    // Write only the annotations (without original lines and without leading indentation spaces)
                    fprintf(out, "// %s\n", cf->name);

                    fprintf(out, "// (func args %s %s)\n", cf->name, cf->pop_list);
                    fprintf(out, "// (return args %s %s)\n\n", cf->name, cf->push_list);
                    printf("Generated comments for: %s -> args: %s, return: %s\n", cf->name, cf->pop_list, cf->push_list);
                }
            }
        }
    }

    fclose(in);
    fclose(out);
    return 0;
}
