/*
 * This file l1vm-cfunc.c is part of L1vm.
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

// Gemini 3.5 flash created

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Helper to check if a char is a valid C identifier character
int is_ident_char(char c) {
    return isalnum((unsigned char)c) || c == '_';
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <source_file> <output_file>\n", argv[0]);
        return 1;
    }

    const char *source_file = argv[1];
    const char *output_file = argv[2];

    FILE *in = fopen(source_file, "r");
    if (!in) {
        fprintf(stderr, "Error opening source file '%s': ", source_file);
        perror("");
        return 1;
    }

    // Determine file size and read the whole file into memory
    fseek(in, 0, SEEK_END);
    long size = ftell(in);
    fseek(in, 0, SEEK_SET);

    char *buf = malloc(size + 1);
    if (!buf) {
        perror("Memory allocation failed");
        fclose(in);
        return 1;
    }

    size_t read_bytes = fread(buf, 1, size, in);
    buf[read_bytes] = '\0';
    fclose(in);

    FILE *out = fopen(output_file, "w");
    if (!out) {
        fprintf(stderr, "Error opening output file '%s': ", output_file);
        perror("");
        free(buf);
        return 1;
    }

    // The signature we are looking for
    const char *sig = "(U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)";
    char *ptr = buf;

    while ((ptr = strstr(ptr, sig)) != NULL) {
        // Find the function name by scanning backward
        char *name_end = ptr;
        while (name_end > buf && isspace((unsigned char)*(name_end - 1))) {
            name_end--;
        }
        
        char *name_start = name_end;
        while (name_start > buf && is_ident_char(*(name_start - 1))) {
            name_start--;
        }

        int name_len = name_end - name_start;
        char *func_name = malloc(name_len + 1);
        if (!func_name) {
            perror("Memory allocation failed for function name");
            break;
        }
        strncpy(func_name, name_start, name_len);
        func_name[name_len] = '\0';

        // Find function body starting brace
        char *body_start = strchr(ptr, '{');
        if (!body_start) {
            free(func_name);
            ptr += strlen(sig);
            continue;
        }

        // Find closing brace of the function body by tracking nested braces
        char *body_end = body_start;
        int brace_count = 0;
        while (*body_end) {
            if (*body_end == '{') {
                brace_count++;
            } else if (*body_end == '}') {
                brace_count--;
                if (brace_count == 0) {
                    break;
                }
            }
            body_end++;
        }

        if (brace_count != 0) {
            fprintf(stderr, "Warning: Unbalanced braces in function %s\n", func_name);
            free(func_name);
            ptr += strlen(sig);
            continue;
        }

        // Extract function body string
        int body_len = body_end - body_start + 1;
        char *body = malloc(body_len + 1);
        if (!body) {
            perror("Memory allocation failed for function body");
            free(func_name);
            break;
        }
        strncpy(body, body_start, body_len);
        body[body_len] = '\0';

        // Gather all pops in order of occurrence
        char pop_list[512] = "";
        char *p = body;
        int pop_count = 0;
        while (*p) {
            const char *type = NULL;
            int len = 0;
            if (strncmp(p, "stpopi", 6) == 0) {
                type = "int64";
                len = 6;
            } else if (strncmp(p, "stpopd", 6) == 0) {
                type = "double";
                len = 6;
            } else if (strncmp(p, "stpopb", 6) == 0) {
                type = "byte";
                len = 6;
            }
            
            if (type) {
                if (pop_count > 0) {
                    strcat(pop_list, ", ");
                }
                strcat(pop_list, type);
                pop_count++;
                p += len;
            } else {
                p++;
            }
        }
        if (pop_count == 0) {
            strcpy(pop_list, "none");
        }

        // Gather all pushes in order of occurrence
        char push_list[512] = "";
        p = body;
        int push_count = 0;
        while (*p) {
            const char *type = NULL;
            int len = 0;
            if (strncmp(p, "stpushi", 7) == 0) {
                type = "int64";
                len = 7;
            } else if (strncmp(p, "stpushd", 7) == 0) {
                type = "double";
                len = 7;
            } else if (strncmp(p, "stpushb", 7) == 0) {
                type = "byte";
                len = 7;
            }
            
            if (type) {
                if (push_count > 0) {
                    strcat(push_list, ", ");
                }
                strcat(push_list, type);
                push_count++;
                p += len;
            } else {
                p++;
            }
        }
        if (push_count == 0) {
            strcpy(push_list, "none");
        }

        // Output formatting to file
        fprintf(out, "%s (%s) (%s)\n", func_name, pop_list, push_list);
        printf("Parsed: %s (%s) (%s)\n", func_name, pop_list, push_list);

        free(body);
        free(func_name);

        ptr += strlen(sig);
    }

    fclose(out);
    free(buf);
    return 0;
}
