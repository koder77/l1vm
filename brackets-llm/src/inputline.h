/*
 * This file inputline.h is part of L1vm.
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
 * When libreadline is available at build time and stdin is a terminal,
 * readline gives line editing and command history. Otherwise the input is
 * read with plain fgets, so piped/scripted input keeps working unchanged.
 */

#ifndef INPUTLINE_H
#define INPUTLINE_H

/* Read one line of input. On a terminal with libreadline, readline(3) is
 * used (the prompt is passed through verbatim and the line is added to the
 * history when non-empty); otherwise a plain fgets fallback is used.
 * Returns a heap-allocated NUL-terminated line WITHOUT the trailing
 * newline, or NULL on EOF/error. The caller must free() it. */
char *input_line(const char *prompt);

/* Ask a yes/no question. `prompt` is printed before reading the answer.
 * Returns 1 for y/Y, 0 for n/N or an empty line/EOF. When stdin is not a
 * terminal (piped/scripted input), the prompt is skipped and `def` is
 * returned so automation is never interrupted. */
int input_confirm(const char *prompt, int def);

/* Release readline state (history, line state, terminal) before the
 * process exits. Harmless when readline was never used. The one-time
 * keymap/function-map/terminfo caches inside libreadline/libtinfo have no
 * release API and are reclaimed by the OS at exit. */
void input_shutdown(void);

#endif