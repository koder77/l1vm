/*
 * This file tools.h is part of L1vm.
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
 * brackets-llm - agent tools (read_file / write_file / edit_file / list_files)
 */

#ifndef BRACKETS_TOOLS_H
#define BRACKETS_TOOLS_H

/* Build the JSON "tools" array describing the available functions.
 * Returns a heap-allocated JSON string. Caller frees. */
char *tools_definitions_json(void);

/* Execute a tool call. `name` is the function name, `args_json` the raw
 * JSON arguments string (borrowed). *out receives a heap result string
 * describing the outcome (both success and error cases). Returns 0 if the
 * tool ran, -1 on internal failure (message still in *out). */
int tool_run(const char *name, const char *args_json, char **out);

/* Expand a leading "~" to $HOME, as the tools do. Returns a heap-allocated
 * path (strdup of the input if there is no leading "~"). Caller frees. */
char *tool_expand_path(const char *p);

/* Canonical absolute form of `path`: expands ~, resolves "." and ".." and
 * prepends the working directory for relative paths. Symlinks are resolved
 * when the path exists. Returns a heap-allocated string. Caller frees. */
char *tool_canon_path(const char *path);

/* 1 when `path` refers to the working directory itself or something inside
 * it; 0 when it points outside (including ".." or absolute paths). */
int tool_path_inside_cwd(const char *path);

/* Warn + ask the user for permission to access a path outside the working
 * directory. Never prompts on piped/scripted input (always allows it then).
 * Returns 1 to allow, 0 to deny. */
int tool_ask_permission(const char *path);

#endif
