/*
 * This file intr.h is part of L1vm.
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
 * intr - cooperative Ctrl+C (SIGINT) handling.
 *
 * The signal handler ONLY sets a flag (async-signal-safe). Blocking code
 * checks the flag when a syscall returns EINTR and aborts gracefully, so
 * the heap is never left mid-`malloc` (which is what a siglongjmp from a
 * signal handler would corrupt). The REPL clears the flag after printing
 * "(interrupted)" and restarting the LSP.
 */

#ifndef INTR_H
#define INTR_H

#include <signal.h>

extern volatile sig_atomic_t g_intr_request;

#endif