/*
 * This file l1vmform.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2026
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

// l1vmform - Brackets code formatter

#include "../include/global.h"

#define KEYWORDS 15

// protos
char *fgets_uni (char *str, int len, FILE *fptr);
size_t strlen_safe (const char * str, S8 maxlen);
S2 searchstr (U1 *str, U1 *srchstr, S2 start, S2 end, U1 case_sens);


struct keywords
{
    U1 key [256];
    S2 tab;
};

struct keywords keywords[KEYWORDS] =
{
    { "func)", 4 },
    { "(funcend)", -4 },
    { "object)", 4 },
    { "(objectend)", -4},
    { "if", 4 },
    { "(else)", -4},
    { "(endif)", -4},
    { "for", 4},
    { "(next)", -4},
    { "(do)", 4},
    { "while", -4},
    { "(switch)", 4},
    { "(break)", -4},
    { "?)", -4},
    { "(switchend)", -4}
};

S8 curr_pos = 0;

void strip_leading_spaces(U1 *input, U1 *output)
{
    S8 i = 0, j = 0, len;

    len = strlen_safe((const char *)input, MAXSTRLEN);
    if (len == 0)
    {
        if (output) output[0] = '\0';
        return;
    }

    while (i < len && (input[i] == 32 || input[i] == 9))
    {
        i++;
    }

    while (i < len)
    {
        output[j] = input[i];
        i++;
        j++;
    }

    output[j] = '\0';
}

int is_separator(char c)
{
    // If it's not a-z, A-Z, 0-9 or an underscore, it's a separator for our keywords
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_') {
        return 0;
    }
    return 1;
}

S2 format_line(U1 *input)
{
    S8 i, n;
    U1 new_line[MAXSTRLEN] = {0};
    S8 indent_step = 0;
    U1 is_else = 0;

    // 1. Temporary string for searching (to ignore comments)
    U1 search_buf[MAXSTRLEN];
    strcpy((char*)search_buf, (char*)input);
    char *comment_ptr = strstr((char*)search_buf, "//");
    if (comment_ptr) *comment_ptr = '\0'; // Cut off comments for the search

    // 2. Identify the structural keyword in the line
    for (i = 0; i < KEYWORDS; i++)
    {
        // Use the cleaned search_buf to find keywords
        S8 pos = searchstr(search_buf, keywords[i].key, 0, 0, TRUE);
        if (pos >= 0)
        {
            size_t klen = strlen((char*)keywords[i].key);

            // Boundary check: must be a standalone word
            if ((pos == 0 || is_separator(search_buf[pos - 1])) &&
                (is_separator(search_buf[pos + klen]) || search_buf[pos + klen] == '\0'))
            {
                indent_step = keywords[i].tab;

                // Flag if it's an 'else' to handle its special double-move
                if (strstr((char*)keywords[i].key, "else")) {
                    is_else = 1;
                }
                break; // One structural keyword per line is enough
            }
        }
    }

    // 3. IMMEDIATE Shift Left (for endif, else, next, etc.)
    if (indent_step < 0)
    {
        curr_pos += indent_step;
    }

    // Safety: Indentation must not be negative
    if (curr_pos < 0) curr_pos = 0;

    // 4. Construct the indentation spaces
    for (n = 0; n < curr_pos; n++)
    {
        if (n < MAXSTRLEN - 1) new_line[n] = ' ';
    }
    new_line[n] = '\0';

    // 5. Safety check for buffer overflow
    if ((strlen_safe((char*)new_line, MAXSTRLEN) + strlen_safe((char*)input, MAXSTRLEN)) >= MAXSTRLEN)
    {
        printf("ERROR: String overflow in format_line!\n");
        return (1);
    }

    // Merge indentation with the original input (which still has its comments)
    strcat((char *)new_line, (const char *)input);
    strcpy((char *)input, (const char *)new_line);

    // 6. Shift Right for the NEXT line (for if, for, do, and restoring after else)
    if (indent_step > 0 || is_else)
    {
        curr_pos += 4;
    }

    return (0);
}

S2 cat (FILE *in, FILE *out)
{
   int ch;

   while ((ch = fgetc(in)) != EOF) {
       if (fputc(ch, out) == EOF)
       {
           return (1);
       }
   }

   return (0);
}

int main (int ac, char const *av[])
{   FILE *finptr = NULL, *foutptr = NULL;

    U1 readbuf[MAXSTRLEN + 1] = "";
    U1 convbuf[MAXSTRLEN + 1] = "";

    U1 parse = 1;
    char *read = NULL;
    S8 convbuf_len = 0;

    if (ac < 3)
    {
        printf ("l1vmform <input code> <output code>\n");
        exit (1);
    }

    // open fles
    // open input file
	finptr = fopen (av[1], "r");
	if (finptr == NULL)
	{
		// error can't open input file
		printf ("ERROR: can't open input file: '%s' !\n", av[1]);
		exit (1);
	}

	foutptr = fopen (av[2], "w");
	if (foutptr == NULL)
	{
		// error can't open input file
		printf ("ERROR: can't open output file: '%s' !\n", av[2]);
		fclose (finptr);
		exit (1);
	}

    while (parse == 1)
    {
        read = fgets_uni ((char *) readbuf, MAXSTRLEN, finptr);
        if (read != NULL)
        {
            strip_leading_spaces (readbuf, convbuf);
            if (format_line (convbuf) != 0)
            {
                printf ("error: can't parse line!\n");
                if (finptr) fclose (finptr);
                if (foutptr) fclose (foutptr);
                exit (1);
            }

            convbuf_len = strlen_safe ((char *) convbuf, MAXSTRLEN);
            if (fwrite (convbuf, sizeof (U1), convbuf_len, foutptr) == 0)
            {
                printf ("error: can’t write to outputfile: '%s' !\n", av[2]);
                if (finptr) fclose (finptr);
                if (foutptr) fclose (foutptr);
                exit (1);
            }
        }
        else
        {
            parse = 0;
        }
    }

    if (finptr) fclose (finptr);
    if (foutptr) fclose (foutptr);

    exit (0);
}
