/*
 * This file main.h is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2021
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

 #ifndef _VM_MAIN_H_
 #define _VM_MAIN_H_

 // show host system type on compile time ======================================
 #if __linux__
 	#pragma message ("Linux host detected!")
 #endif
 #if _WIN32
 	#pragma message ("Windows host detected!")

    #undef DIVISIONCHECK
	#undef CPU_SET_AFFINITY
	#define DIVISIONCHECK 0
	#define CPU_SET_AFFINITY 0
 #endif

#if __MACH__
    #pragma message ("macOS host detected!")

	#undef DIVISIONCHECK
	#undef CPU_SET_AFFINITY
	#define DIVISIONCHECK 0
	#define CPU_SET_AFFINITY 0
#endif

 // show math limits settings on compile time
 #if MATH_LIMITS
 	#pragma message ("MATH LIMITS ON")
 #endif
 #if MATH_LIMITS_DOUBLE_FULL
 	#pragma message ("MATH LIMITS DOUBLE FULL ON")
 #endif

 // show alignment on compile time
 #if DO_ALIGNMENT
 	#pragma message ("ALIGNMENT FOR 64 bit ON")
 #endif
 // ============================================================================

 // time functions
 struct tm *tm;

 // timer interrupt stuff
 #if TIMER_USE
 	struct timeval  timer_start, timer_end;
 	F8 timer_double ALIGN;
 	S8 timer_int ALIGN;
 #endif

 #if JIT_COMPILER
 U1 JIT_initialized = 0;
 S8 JIT_code_ind ALIGN = -1;
 struct JIT_code *JIT_code = NULL;

 int jit_compiler (U1 *code, U1 *data, S8 *jumpoffs, S8 *regi, F8 *regd, U1 *sp, U1 *sp_top, U1 *sp_bottom, S8 start, S8 end, struct JIT_code *JIT_code, S8 JIT_code_ind, S8 code_size);
 int run_jit (S8 code, struct JIT_code *JIT_code);
 int free_jit_code (struct JIT_code *JIT_code);
 void get_jit_compiler_type (void);
 char *fgets_uni (char *str, int len, FILE *fptr);
 size_t strlen_safe (const char * str, S8  maxlen);
 #endif

#define EXE_NEXT(); ep = ep + eoffs; goto *jumpt[code[ep]];
#define PRINT_EPOS(); printf ("epos: %lli\n\n", ep);

//#define EXE_NEXT(); ep = ep + eoffs; printf ("next opcode: %i\n", code[ep]); goto *jumpt[code[ep]];

// protos
S2 load_object (U1 *name, S2 load_code_only);

void free_modules (void);
size_t strlen_safe (const char * str, S8 maxlen);

// code_datasize.c
void show_code_data_size (S8 codesize, S8 datasize);

S2 memory_bounds (S8 start, S8 offset_access);
S2 memory_size (S8 start);
S2 pointer_check (S8 start, S8 pointer_type);
S2 pointer_type (S8 start);
S2 set_immutable_string (S8 string_pointer);

#endif
