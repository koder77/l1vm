/*
 * This file inputline.c is part of L1vm.
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
 * inputline - interactive line input with optional GNU readline.
 *
 * Compiled against GNU readline when HAVE_LIBREADLINE is defined (the
 * Makefile detects it at build time). When readline is not available the
 * code falls back to plain fgets, which also handles piped/scripted stdin.
 */

#include "inputline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_LIBREADLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif

#ifdef HAVE_LIBREADLINE
static int g_readline_used = 0;
#endif

char *input_line(const char *prompt)
{
#ifdef HAVE_LIBREADLINE
    char *r;

    if (isatty(STDIN_FILENO)) {
        r = readline(prompt ? prompt : "");
        if (!r)
            return NULL;
        if (*r)
            add_history(r);
        g_readline_used = 1;
        return r;   /* readline malloc's the line and strips the newline */
    }
#endif
    {
        char *line = malloc(65536);
        if (!line)
            return NULL;
        if (prompt) {
            /* strip readline's prompt-width markers (\001/\002) so they are
             * not echoed literally when readline is not in use; the ANSI
             * color escape stays, so a colored prompt still works */
            const char *p = prompt;
            while (*p) {
                if (*p != '\001' && *p != '\002')
                    fputc(*p, stdout);
                p++;
            }
            fflush(stdout);
        }
        if (!fgets(line, 65536, stdin)) {
            /* A signal (Ctrl+C) can interrupt the underlying read(), leaving
             * stdio's FILE in an error state. Clear it so a later read on
             * stdin still works; otherwise a subsequent fgets would fail
             * immediately and the caller could mistake it for EOF. */
            if (ferror(stdin))
                clearerr(stdin);
            free(line);
            return NULL;
        }
        line[strcspn(line, "\r\n")] = '\0';
        return line;
    }
}

int input_confirm(const char *prompt, int def)
{
    char *line;
    char answer;

    if (!isatty(STDIN_FILENO))
        return def;   /* piped/scripted input: cannot prompt */

    line = input_line(prompt);
    if (!line)
        return def;   /* EOF */
    answer = line[0];
    free(line);
    if (answer == 'y' || answer == 'Y')
        return 1;
    if (answer == 'n' || answer == 'N')
        return 0;
    return def;       /* empty or unrecognized answer */
}

void input_shutdown(void)
{
#ifdef HAVE_LIBREADLINE
    if (!g_readline_used)
        return;
    rl_deprep_terminal();
    rl_clear_history();
    rl_free_line_state();
#endif
}

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */