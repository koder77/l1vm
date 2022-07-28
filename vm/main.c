/*
 * This file main.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2017
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

//  l1vm RISC VM
//
//

#include "jit.h"
#include "../include/global.h"
#include "../include/stack.h"
#include "main.h"


// include/home.h 
char *get_home (void);

S8 data_size ALIGN;
S8 code_size ALIGN;

// see global.h user settings on top
S8 max_code_size ALIGN = MAX_CODE_SIZE;
S8 max_data_size ALIGN = MAX_DATA_SIZE;

S8 data_mem_size ALIGN;
S8 stack_size ALIGN = STACKSIZE;		// stack size added to data size when dumped to object file

// code
U1 *code = NULL;

// data
U1 *data = NULL;


S8 code_ind ALIGN;
S8 data_ind ALIGN;
S8 modules_ind ALIGN = -1;    // no module loaded = -1

S8 cpu_ind ALIGN = 0;

S8 max_cpu ALIGN = MAXCPUCORES;    // number of threads that can be runned

U1 silent_run = 0;				// switch startup and status messages of: "-q" flag on shell

typedef U1* (*dll_func)(U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);

struct module
{
    U1 name[512];
	U1 used;

#if __linux__
    void *lptr;
#endif

#if _WIN32
    HINSTANCE lptr;
#endif

    dll_func func[MODULES_MAXFUNC];
};

struct module modules[MODULES];

struct data_info data_info[MAXDATAINFO];
S8 data_info_ind ALIGN = -1;

// pthreads data mutex
pthread_mutex_t data_mutex;

// mutexes as defined in settings.h
pthread_mutex_t global_mutex[MAX_MUTEXES];

// wait loop mutex 
pthread_mutex_t wait_loop;
U1 run_loop = 1;

// return code of main thread
S8 retcode ALIGN = 0;

// shell arguments
U1 shell_args[MAXSHELLARGS][MAXSHELLARGLEN];
S4 shell_args_ind = -1;

struct threaddata *threaddata;

#if JIT_COMPILER
S2 alloc_jit_code ()
{
	JIT_code = (struct JIT_code*) calloc (MAXJITCODE, sizeof (struct JIT_code));
	if (JIT_code == NULL)
	{
		printf ("FATAL ERROR: can't allocate JIT_code structure!\n");
		return (1);
	}
	return (0);
}
#endif

S2 load_module (U1 *name, S8 ind)
{
	U1 linux_lib_suffix[] = ".so";
	U1 macos_lib_suffix[] = ".so";
	U1 libname[MAXSTRLEN];

	if (strlen_safe ((const char*) name, MAXLINELEN) > MAXLINELEN - 5)
	{
		printf ("load_module: ERROR library name overflow!\n");
		return (1);
	}

#if __linux__ || _WIN32 
	strcpy ((char *) libname, (const char *) name);
	strcat ((char *) libname, (const char *) linux_lib_suffix);
#endif

#if __MACH__
	strcpy ((char *) libname, (const char *) name);
	strcat ((char *) libname, (const char *) macos_lib_suffix);
#endif

#if __linux__
    modules[ind].lptr = dlopen ((const char *) libname, RTLD_LAZY);
    if (!modules[ind].lptr)
	{
        printf ("error load module %s!\n", (const char *) libname);
        return (1);
    }
#endif

#if _WIN32
    modules[ind].lptr = LoadLibrary ((const char *) libname);
    if (! modules[ind].lptr)
    {
        printf ("error load module %s!\n", (const char *) libname);
        return (1);
    }
#endif

    strcpy ((char *) modules[ind].name, (const char *) libname);
	modules[ind].used = 1;

	// print module name:
	if (silent_run == 0)
	{
		printf ("module: %lli %s loaded\n", ind, libname);
  	}
    return (0);
}

void free_module (S8 ind)
{
	if (modules[ind].used != 0)
	{
		#if __linux__
   			dlclose (modules[ind].lptr);
		#endif

		#if _WIN32
    		FreeLibrary (modules[ind].lptr);
		#endif

		if (silent_run == 0)
		{
			printf ("free_module: %lli %s\n", ind, modules[ind].name);
		}

		// mark as free
    	strcpy ((char *) modules[ind].name, "");
		modules[ind].used = 0;		// mark as free!
	}
}

S2 set_module_func (S8 ind, S8 func_ind, U1 *func_name)
{
#if __linux__
	dlerror ();

    // load the symbols (handle to function)
    modules[ind].func[func_ind] = dlsym (modules[ind].lptr, (const char *) func_name);
    const char* dlsym_error = dlerror ();
    if (dlsym_error)
	{
        printf ("error set module %s, function: '%s'!\n", modules[ind].name, func_name);
		printf ("%s\n", dlsym_error);
        return (1);
    }
    return (0);
#endif

#if _WIN32
    modules[ind].func[func_ind] = GetProcAddress (modules[ind].lptr, (const char *) func_name);
    if (! modules[ind].func[func_ind])
    {
        printf ("error set module %s, function: '%s'!\n", modules[ind].name, func_name);
        return (1);
    }
    return (0);
#endif
}

U1 *call_module_func (S8 ind, S8 func_ind, U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    return (*modules[ind].func[func_ind])(sp, sp_top, sp_bottom, data);
}

void cleanup (void)
{
	#if JIT_COMPILER
		free_jit_code (JIT_code, JIT_code_ind);
		if (JIT_code) free (JIT_code);
	#endif

    free_modules ();
	if (data) free (data);
    if (code) free (code);
	if (threaddata) free (threaddata);
}

U1 double_state (F8 num)
{
	S2 state;
	U1 flag = 0;
	state = fpclassify (num);
	if (state == FP_INFINITE || state == FP_NAN || state == FP_SUBNORMAL)
	{
		// ERROR!!
		flag = 1;
	}
	return (flag);
}

void loop_stop (void)
{
	// stop pthreads wait loop
	pthread_mutex_lock (&wait_loop);
	run_loop = 0;
	pthread_mutex_unlock (&wait_loop);

	// printf ("DEBUG: loop_stop!\n");
}

S2 run (void *arg)
{
	S8 cpu_core ALIGN = (S8) arg;
	S8 i ALIGN;
	U1 eoffs;                  	// offset to next opcode
	S8 regi[MAXREG];   		  	// integer registers
	F8 regd[MAXREG];			// double registers
	S8 arg1 ALIGN;
	S8 arg2 ALIGN;
	S8 arg3 ALIGN;
	S8 arg4 ALIGN;				// opcode arguments

	S8 ep ALIGN = 0; 			// execution pointer in code segment
	S8 startpos ALIGN;

	U1 overflow = 0;			// MATH_LIMITS calculation overflow flag

	U1 *sp;  					// stack pointer
	U1 *sp_top;    				// stack pointer start address
	U1 *sp_bottom;				// stack bottom
	U1 *srcptr, *dstptr;

	U1 *bptr;

	// jump call stack for jsr, jsra
	S8 jumpstack[MAXSUBJUMPS];
	S8 jumpstack_ind ALIGN = -1;		// empty

	// threads
	S8 new_cpu ALIGN;
	S8 cpus_free ALIGN;

    // thread attach to CPU core
	#if CPU_SET_AFFINITY
	cpu_set_t cpuset;
	#endif

	// for data input
	U1 input_str[MAXINPUT];

	// for time functions
	time_t secs;

	// jumpoffsets
	S8 *jumpoffs ALIGN;
	S8 offset ALIGN;
	jumpoffs = (S8 *) calloc (code_size, sizeof (S8));
	if (jumpoffs == NULL)
	{
		printf ("ERROR: can't allocate %lli bytes for jumpoffsets!\n", code_size);
		loop_stop ();
		pthread_exit ((void *) 1);
	}

	sp_top = threaddata[cpu_core].sp_top_thread;
	sp_bottom = threaddata[cpu_core].sp_bottom_thread;
	sp = threaddata[cpu_core].sp_thread;

	if (silent_run == 0)
	{
		printf ("%lli stack size: %lli\n", cpu_core, stack_size);
		printf ("%lli sp top: %lli\n", cpu_core, (S8) sp_top);
		printf ("%lli sp bottom: %lli\n", cpu_core, (S8) sp_bottom);
		printf ("%lli sp: %lli\n", cpu_core, (S8) sp);

		printf ("%lli sp caller top: %lli\n", cpu_core, (S8) threaddata[cpu_core].sp_top);
		printf ("%lli sp caller bottom: %lli\n", cpu_core, (S8) threaddata[cpu_core].sp_bottom);
	}

	startpos = threaddata[cpu_core].ep_startpos;
	if (threaddata[cpu_core].sp != threaddata[cpu_core].sp_top)
	{
		// something on mother thread stack, copy it

		srcptr = threaddata[cpu_core].sp_top;
		dstptr = threaddata[cpu_core].sp_top_thread;

		while (srcptr >= threaddata[cpu_core].sp)
		{
			// printf ("dstptr stack: %lli\n", (S8) dstptr);
			*dstptr-- = *srcptr--;
		}
	}

	cpu_ind = cpu_core;

	// jumptable for indirect threading execution
	static void *jumpt[] =
	{
		&&pushb, &&pushw, &&pushdw, &&pushqw, &&pushd,
		&&pullb, &&pullw, &&pulldw, &&pullqw, &&pulld,
		&&addi, &&subi, &&muli, &&divi,
		&&addd, &&subd, &&muld, &&divd,
		&&smuli, &&sdivi,
		&&andi, &&ori, &&bandi, &&bori, &&bxori, &&modi,
		&&eqi, &&neqi, &&gri, &&lsi, &&greqi, &&lseqi,
		&&eqd, &&neqd, &&grd, &&lsd, &&greqd, &&lseqd,
		&&jmp, &&jmpi,
		&&stpushb, &&stpopb, &&stpushi, &&stpopi, &&stpushd, &&stpopd,
		&&loada, &&loadd,
		&&intr0, &&intr1, &&inclsijmpi, &&decgrijmpi,
		&&movi, &&movd, &&loadl, &&jmpa,
		&&jsr, &&jsra, &&rts, &&load,
        &&noti
	};

	//printf ("setting jump offset table...\n");

	// setup jump offset table
	for (i = 16; i < code_size; i = i + offset)
	{
		//printf ("opcode: %i\n", code[i]);
		offset = 0;
		if (code[i] <= LSEQD)
		{
			offset = 4;
		}
		if (code[i] == JMP)
		{
			bptr = (U1 *) &arg1;

			*bptr = code[i + 1];
			bptr++;
			*bptr = code[i + 2];
			bptr++;
			*bptr = code[i + 3];
			bptr++;
			*bptr = code[i + 4];
			bptr++;
			*bptr = code[i + 5];
			bptr++;
			*bptr = code[i + 6];
			bptr++;
			*bptr = code[i + 7];
			bptr++;
			*bptr = code[i + 8];

			jumpoffs[i] = arg1;
			offset = 9;
		}

		if (code[i] == JMPI)
		{
			bptr = (U1 *) &arg1;

			*bptr = code[i + 2];
			bptr++;
			*bptr = code[i + 3];
			bptr++;
			*bptr = code[i + 4];
			bptr++;
			*bptr = code[i + 5];
			bptr++;
			*bptr = code[i + 6];
			bptr++;
			*bptr = code[i + 7];
			bptr++;
			*bptr = code[i + 8];
			bptr++;
			*bptr = code[i + 9];

			jumpoffs[i] = arg1;
			offset = 10;
		}

		if (code[i] == INCLSIJMPI || code[i] == DECGRIJMPI)
		{
			bptr = (U1 *) &arg1;

			*bptr = code[i + 3];
			bptr++;
			*bptr = code[i + 4];
			bptr++;
			*bptr = code[i + 5];
			bptr++;
			*bptr = code[i + 6];
			bptr++;
			*bptr = code[i + 7];
			bptr++;
			*bptr = code[i + 8];
			bptr++;
			*bptr = code[i + 9];
			bptr++;
			*bptr = code[i + 10];

			jumpoffs[i] = arg1;
			offset = 11;
		}

		if (code[i] == JSR)
		{
			bptr = (U1 *) &arg1;

			*bptr = code[i + 1];
			bptr++;
			*bptr = code[i + 2];
			bptr++;
			*bptr = code[i + 3];
			bptr++;
			*bptr = code[i + 4];
			bptr++;
			*bptr = code[i + 5];
			bptr++;
			*bptr = code[i + 6];
			bptr++;
			*bptr = code[i + 7];
			bptr++;
			*bptr = code[i + 8];

			jumpoffs[i] = arg1;
			offset = 9;
		}

		if (offset == 0)
		{
			// not set yet, do it!

			switch (code[i])
			{
				case STPUSHB:
				case STPOPB:
				case STPUSHI:
				case STPOPI:
				case STPUSHD:
				case STPOPD:
					offset = 2;
					break;

				case LOADA:
				case LOADD:
					offset = 18;
					break;

				case INTR0:
				case INTR1:
					offset = 5;
					break;

				case MOVI:
				case MOVD:
					offset = 3;
					break;

				case LOADL:
					offset = 10;
					break;

				case JMPA:
					offset = 2;
					break;

				case JSRA:
					offset = 2;
					break;

				case RTS:
					offset = 1;
					break;

				case LOAD:
					offset = 18;
					break;

                case NOTI:
                    offset = 3;
                    break;
			}
		}
		if (offset == 0)
		{
			printf ("FATAL error: setting jump offset failed! opcode: %i\n", code[i]);
			free (jumpoffs);
			loop_stop ();
			pthread_exit ((void *) 1);
		}

		if (i >= code_size) break;
	}

	// debug
#if DEBUG
	printf ("code DUMP:\n");
	for (i = 0; i < code_size; i++)
	{
		printf ("code %lli: %02x\n", i, code[i]);
	}
	printf ("DUMP END\n");
#endif

	// init registers
	for (i = 0; i < 256; i++)
	{
		regi[i] = 0;
		regd[i] = 0.0;
	}

	if (silent_run == 0)
	{
		printf ("CPU %lli ready\n", cpu_ind);
		show_code_data_size (code_size, data_mem_size);
		printf ("ep: %lli\n\n", startpos);
	}
#if DEBUG
	printf ("stack pointer sp: %lli\n", (S8) sp);
#endif

	ep = startpos; eoffs = 0;

	EXE_NEXT();

	// arg2 = data offset
	pushb:
	#if DEBUG
	printf ("%lli PUSHB\n", cpu_core);
	#endif
	arg1 = regi[code[ep + 1]];
	arg2 = regi[code[ep + 2]];
	arg3 = code[ep + 3];

	#if BOUNDSCHECK
	if (memory_bounds (arg1, arg2) != 0)
	{
		PRINT_EPOS();
		free (jumpoffs);
		loop_stop ();
        pthread_exit ((void *) 1);
	}
	#endif

	regi[arg3] = 0;		// set to zero, before loading data
	regi[arg3] = data[arg1 + arg2];

	eoffs = 4;
	EXE_NEXT();

	pushw:
	#if DEBUG
	printf ("%lli PUSHW\n", cpu_core);
	#endif
	arg1 = regi[code[ep + 1]];
	arg2 = regi[code[ep + 2]];
	arg3 = code[ep + 3];

	#if BOUNDSCHECK
	if (memory_bounds (arg1, arg2) != 0)
	{
		PRINT_EPOS();
		free (jumpoffs);
		loop_stop ();
        pthread_exit ((void *) 1);
	}
	#endif

	regi[arg3] = 0;		// set to zero, before loading data
	bptr = (U1 *) &regi[arg3];

	*bptr = data[arg1 + arg2];
	bptr++;
	*bptr = data[arg1 + arg2 + 1];

	eoffs = 4;
	EXE_NEXT();

	pushdw:
	#if DEBUG
	printf ("%lli PUSHDW\n", cpu_core);
	#endif
	arg1 = regi[code[ep + 1]];
	arg2 = regi[code[ep + 2]];
	arg3 = code[ep + 3];

	#if BOUNDSCHECK
	if (memory_bounds (arg1, arg2) != 0)
	{
		PRINT_EPOS();
		free (jumpoffs);
		loop_stop ();
        pthread_exit ((void *) 1);
	}
	#endif

	regi[arg3] = 0;		// set to zero, before loading data
	bptr = (U1 *) &regi[arg3];

	*bptr = data[arg1 + arg2];
	bptr++;
	*bptr = data[arg1 + arg2 + 1];
	bptr++;
	*bptr = data[arg1 + arg2 + 2];
	bptr++;
	*bptr = data[arg1 + arg2 + 3];

	eoffs = 4;
	EXE_NEXT();

	pushqw:
	#if DEBUG
	printf ("%lli PUSHQW\n", cpu_core);
	#endif
	arg1 = regi[code[ep + 1]];
	arg2 = regi[code[ep + 2]];
	arg3 = code[ep + 3];

	#if BOUNDSCHECK
	if (memory_bounds (arg1, arg2) != 0)
	{
		PRINT_EPOS();
		free (jumpoffs);
		loop_stop ();
        pthread_exit ((void *) 1);
	}
	#endif

	bptr = (U1 *) &regi[arg3];

	*bptr = data[arg1 + arg2];
	bptr++;
	*bptr = data[arg1 + arg2 + 1];
	bptr++;
	*bptr = data[arg1 + arg2 + 2];
	bptr++;
	*bptr = data[arg1 + arg2 + 3];
	bptr++;
	*bptr = data[arg1 + arg2 + 4];
	bptr++;
	*bptr = data[arg1 + arg2 + 5];
	bptr++;
	*bptr = data[arg1 + arg2 + 6];
	bptr++;
	*bptr = data[arg1 + arg2 + 7];

	eoffs = 4;
	EXE_NEXT();

	pushd:
	#if DEBUG
	printf ("%lli PUSHD\n", cpu_core);
	#endif
	arg1 = regi[code[ep + 1]];
	arg2 = regi[code[ep + 2]];
	arg3 = code[ep + 3];

	#if BOUNDSCHECK
	if (memory_bounds (arg1, arg2) != 0)
	{
		PRINT_EPOS();
		free (jumpoffs);
		loop_stop ();
        pthread_exit ((void *) 1);
	}
	#endif

	bptr = (U1 *) &regd[arg3];

	*bptr = data[arg1 + arg2];
	bptr++;
	*bptr = data[arg1 + arg2 + 1];
	bptr++;
	*bptr = data[arg1 + arg2 + 2];
	bptr++;
	*bptr = data[arg1 + arg2 + 3];
	bptr++;
	*bptr = data[arg1 + arg2 + 4];
	bptr++;
	*bptr = data[arg1 + arg2 + 5];
	bptr++;
	*bptr = data[arg1 + arg2 + 6];
	bptr++;
	*bptr = data[arg1 + arg2 + 7];

	eoffs = 4;
	EXE_NEXT();


	pullb:
	#if DEBUG
	printf ("%lli PULLB\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = regi[code[ep + 2]];
	arg3 = regi[code[ep + 3]];

	#if BOUNDSCHECK
	if (memory_bounds (arg2, arg3) != 0)
	{
		PRINT_EPOS();
		free (jumpoffs);
		loop_stop ();
        pthread_exit ((void *) 1);
	}
	#endif

	data[arg2 + arg3] = regi[arg1];

	eoffs = 4;
	EXE_NEXT();

	pullw:
	#if DEBUG
	printf ("%lli PULLW\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = regi[code[ep + 2]];
	arg3 = regi[code[ep + 3]];

	#if BOUNDSCHECK
	if (memory_bounds (arg2, arg3) != 0)
	{
		PRINT_EPOS();
		free (jumpoffs);
		loop_stop ();
        pthread_exit ((void *) 1);
	}
	#endif

	bptr = (U1 *) &regi[arg1];

	data[arg2 + arg3] = *bptr;
	bptr++;
	data[arg2 + arg3 + 1] = *bptr;

	eoffs = 4;
	EXE_NEXT();

	pulldw:
	#if DEBUG
	printf ("%lli PULLDW\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = regi[code[ep + 2]];
	arg3 = regi[code[ep + 3]];

	#if BOUNDSCHECK
	if (memory_bounds (arg2, arg3) != 0)
	{
		PRINT_EPOS();
		free (jumpoffs);
		loop_stop ();
        pthread_exit ((void *) 1);
	}
	#endif

	bptr = (U1 *) &regi[arg1];

	data[arg2 + arg3] = *bptr;
	bptr++;
	data[arg2 + arg3 + 1] = *bptr;
	bptr++;
	data[arg2 + arg3 + 2] = *bptr;
	bptr++;
	data[arg2 + arg3 + 3] = *bptr;

	eoffs = 4;
	EXE_NEXT();

	pullqw:
	#if DEBUG
	printf ("%lli PULLQW\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = regi[code[ep + 2]];
	arg3 = regi[code[ep + 3]];

	#if BOUNDSCHECK
	if (memory_bounds (arg2, arg3) != 0)
	{
		PRINT_EPOS();
		free (jumpoffs);
		loop_stop ();
        pthread_exit ((void *) 1);
	}
	#endif

	bptr = (U1 *) &regi[arg1];

	data[arg2 + arg3] = *bptr;
	bptr++;
	data[arg2 + arg3 + 1] = *bptr;
	bptr++;
	data[arg2 + arg3 + 2] = *bptr;
	bptr++;
	data[arg2 + arg3 + 3] = *bptr;
	bptr++;
	data[arg2 + arg3 + 4] = *bptr;
	bptr++;
	data[arg2 + arg3 + 5] = *bptr;
	bptr++;
	data[arg2 + arg3 + 6] = *bptr;
	bptr++;
	data[arg2 + arg3 + 7] = *bptr;

	eoffs = 4;
	EXE_NEXT();

	pulld:
	#if DEBUG
	printf ("%lli PULLD\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = regi[code[ep + 2]];
	arg3 = regi[code[ep + 3]];

	#if BOUNDSCHECK
	if (memory_bounds (arg2, arg3) != 0)
	{
		PRINT_EPOS();
		free (jumpoffs);
		loop_stop ();
        pthread_exit ((void *) 1);
	}
	#endif

	bptr = (U1 *) &regd[arg1];

	data[arg2 + arg3] = *bptr;
	bptr++;
	data[arg2 + arg3 + 1] = *bptr;
	bptr++;
	data[arg2 + arg3 + 2] = *bptr;
	bptr++;
	data[arg2 + arg3 + 3] = *bptr;
	bptr++;
	data[arg2 + arg3 + 4] = *bptr;
	bptr++;
	data[arg2 + arg3 + 5] = *bptr;
	bptr++;
	data[arg2 + arg3 + 6] = *bptr;
	bptr++;
	data[arg2 + arg3 + 7] = *bptr;

	eoffs = 4;
	EXE_NEXT();


	addi:
	#if DEBUG
	printf ("%lli ADDI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	#if MATH_LIMITS
		if (__builtin_saddll_overflow (regi[arg1], regi[arg2], &regi[arg3]))
		{
			overflow = 1;
 			printf ("ERROR: overflow at addi!\n");
			PRINT_EPOS();
		}
		else
		{
			 overflow = 0;
		}
	#else
		regi[arg3] = regi[arg1] + regi[arg2];
	#endif

	eoffs = 4;
	EXE_NEXT();

	subi:
	#if DEBUG
	printf ("%lli SUBI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	#if MATH_LIMITS
		if (__builtin_ssubll_overflow (regi[arg1], regi[arg2], &regi[arg3]))
		{
			overflow = 1;
 			printf ("ERROR: overflow at subi!\n");
			PRINT_EPOS();
		}
		else
		{
			 overflow = 0;
		}
	#else
		regi[arg3] = regi[arg1] - regi[arg2];
	#endif

	eoffs = 4;
	EXE_NEXT();

	muli:
	#if DEBUG
	printf ("%lli MULI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	#if MATH_LIMITS
		if (__builtin_smulll_overflow (regi[arg1], regi[arg2], &regi[arg3]))
		{
			overflow = 1;
 			printf ("ERROR: overflow at muli!\n");
			PRINT_EPOS();
		}
		else
		{
			 overflow = 0;
		}
	#else
		regi[arg3] = regi[arg1] * regi[arg2];
	#endif

	eoffs = 4;
	EXE_NEXT();

	divi:
	#if DEBUG
	printf ("%lli DIVI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

    #if DIVISIONCHECK
    if (iszero (regi[arg2]))
    {
        printf ("FATAL ERROR: division by zero!\n");
		PRINT_EPOS();
		free (jumpoffs);
		loop_stop ();
		pthread_exit ((void *) 1);
    }
    #endif

	regi[arg3] = regi[arg1] / regi[arg2];

	eoffs = 4;
	EXE_NEXT();

	addd:
	#if DEBUG
	printf ("%lli ADDD\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	#if MATH_LIMITS
		overflow = 0;
	#endif
	#if MATH_LIMITS_DOUBLE_FULL
		if (double_state (regd[arg1]) == 1)
		{
			overflow = 1;
			printf ("ERROR: overflow at addd!\n");
			PRINT_EPOS();
		}

		if (double_state (regd[arg2]) == 1)
		{
			overflow = 1;
			printf ("ERROR: overflow at addd!\n");
			PRINT_EPOS();
		}
	#endif

	regd[arg3] = regd[arg1] + regd[arg2];

	#if MATH_LIMITS
		if (double_state (regd[arg3]) == 1)
		{
			overflow = 1;
			printf ("ERROR: overflow at addd!\n");
			PRINT_EPOS();
		}
	#endif

	eoffs = 4;
	EXE_NEXT();

	subd:
	#if DEBUG
	printf ("%lli SUBD\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	#if MATH_LIMITS
		overflow = 0;
	#endif
	#if MATH_LIMITS_DOUBLE_FULL
		if (double_state (regd[arg1]) == 1)
		{
			overflow = 1;
			printf ("ERROR: overflow at subd!\n");
			PRINT_EPOS();
		}

		if (double_state (regd[arg2]) == 1)
		{
			overflow = 1;
			printf ("ERROR: overflow at subd!\n");
			PRINT_EPOS();
		}
	#endif

	regd[arg3] = regd[arg1] - regd[arg2];

	#if MATH_LIMITS
		if (double_state (regd[arg3]) == 1)
		{
			overflow = 1;
			printf ("ERROR: overflow at subd!\n");
			PRINT_EPOS();
		}
	#endif

	eoffs = 4;
	EXE_NEXT();

	muld:
	#if DEBUG
	printf ("%lli MULD\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	#if MATH_LIMITS
		overflow = 0;
	#endif
	#if MATH_LIMITS_DOUBLE_FULL
		if (double_state (regd[arg1]) == 1)
		{
			overflow = 1;
			printf ("ERROR: overflow at muld!\n");
			PRINT_EPOS();
		}

		if (double_state (regd[arg2]) == 1)
		{
			overflow = 1;
			printf ("ERROR: overflow at muld!\n");
			PRINT_EPOS();
		}
	#endif

	regd[arg3] = regd[arg1] * regd[arg2];

	#if MATH_LIMITS
		if (double_state (regd[arg3]) == 1)
		{
			overflow = 1;
			printf ("ERROR: overflow at muld!\n");
			PRINT_EPOS();
		}
	#endif

	eoffs = 4;
	EXE_NEXT();

	divd:
	#if DEBUG
	printf ("%lli DIVD\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

    #if DIVISIONCHECK
    if (iszero (regd[arg2]))
    {
        printf ("FATAL ERROR: division by zero!\n");
		PRINT_EPOS();
		free (jumpoffs);
		loop_stop ();
		pthread_exit ((void *) 1);
    }
    #endif

	#if MATH_LIMITS
		overflow = 0;
	#endif
	#if MATH_LIMITS_DOUBLE_FULL
		if (double_state (regd[arg1]) == 1)
		{
			overflow = 1;
			printf ("ERROR: overflow at divd!\n");
			PRINT_EPOS();
		}

		if (double_state (regd[arg2]) == 1)
		{
			overflow = 1;
			printf ("ERROR: overflow at divd!\n");
			PRINT_EPOS();
		}
	#endif

	regd[arg3] = regd[arg1] / regd[arg2];

	#if MATH_LIMITS
	if (double_state (regd[arg3]) == 1)
		{
			overflow = 1;
			printf ("ERROR: overflow at divd!\n");
			PRINT_EPOS();
		}
	#endif

	eoffs = 4;
	EXE_NEXT();

	smuli:
	#if DEBUG
	printf ("%lli SMULI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	regi[arg3] = regi[arg1] << regi[arg2];

	eoffs = 4;
	EXE_NEXT();

	sdivi:
	#if DEBUG
	printf ("%lli SDIVI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	regi[arg3] = regi[arg1] >> regi[arg2];

	eoffs = 4;
	EXE_NEXT();

	andi:
	#if DEBUG
	printf ("%lli ANDI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	regi[arg3] = regi[arg1] && regi[arg2];

	eoffs = 4;
	EXE_NEXT();

	ori:
	#if DEBUG
	printf ("%lli ORI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	regi[arg3] = regi[arg1] || regi[arg2];

	eoffs = 4;
	EXE_NEXT();


	bandi:
	#if DEBUG
	printf ("%lli BANDI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	regi[arg3] = regi[arg1] & regi[arg2];

	eoffs = 4;
	EXE_NEXT();

	bori:
	#if DEBUG
	printf ("%lli BORI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	regi[arg3] = regi[arg1] | regi[arg2];

	eoffs = 4;
	EXE_NEXT();

	bxori:
	#if DEBUG
	printf ("%lli BXORI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	regi[arg3] = regi[arg1] ^ regi[arg2];

	eoffs = 4;
	EXE_NEXT();

	modi:
	#if DEBUG
	printf ("%lli MODI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	regi[arg3] = regi[arg1] % regi[arg2];

	eoffs = 4;
	EXE_NEXT();


	eqi:
	#if DEBUG
	printf ("%lli EQI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	regi[arg3] = regi[arg1] == regi[arg2];

	eoffs = 4;
	EXE_NEXT();

	neqi:
	#if DEBUG
	printf ("%lli NEQI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	regi[arg3] = regi[arg1] != regi[arg2];

	eoffs = 4;
	EXE_NEXT();

	gri:
	#if DEBUG
	printf ("%lli GRI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	regi[arg3] = regi[arg1] > regi[arg2];

	eoffs = 4;
	EXE_NEXT();

	lsi:
	#if DEBUG
	printf ("%lli LSI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	regi[arg3] = regi[arg1] < regi[arg2];

	eoffs = 4;
	EXE_NEXT();

	greqi:
	#if DEBUG
	printf ("%lli GREQI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	regi[arg3] = regi[arg1] >= regi[arg2];

	eoffs = 4;
	EXE_NEXT();

	lseqi:
	#if DEBUG
	printf ("%lli LSEQI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	regi[arg3] = regi[arg1] <= regi[arg2];

	eoffs = 4;
	EXE_NEXT();


	eqd:
	#if DEBUG
	printf ("%lli EQD\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	regi[arg3] = regd[arg1] == regd[arg2];

	eoffs = 4;
	EXE_NEXT();

	neqd:
	#if DEBUG
	printf ("%lli NEQD\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	regi[arg3] = regd[arg1] != regd[arg2];

	eoffs = 4;
	EXE_NEXT();

	grd:
	#if DEBUG
	printf ("%lli GRD\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	regi[arg3] = regd[arg1] > regd[arg2];

	eoffs = 4;
	EXE_NEXT();

	lsd:
	#if DEBUG
	printf ("%lli LSD\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	regi[arg3] = regd[arg1] < regd[arg2];

	eoffs = 4;
	EXE_NEXT();

	greqd:
	#if DEBUG
	printf ("%lli GREQD\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	regi[arg3] = regd[arg1] >= regd[arg2];

	eoffs = 4;
	EXE_NEXT();

	lseqd:
	#if DEBUG
	printf ("%lli LSEQD\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	regi[arg3] = regd[arg1] <= regd[arg2];

	eoffs = 4;
	EXE_NEXT();


	jmp:
	#if DEBUG
	printf ("%lli JMP\n", cpu_core);
	#endif
	ep = jumpoffs[ep];
	eoffs = 0;

	EXE_NEXT();

	jmpi:
	#if DEBUG
	printf ("%lli JMPI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];

	arg2 = jumpoffs[ep];

	if (regi[arg1] != 0)
	{
		eoffs = 0;
		ep = arg2;
		#if DEBUG
		printf ("%lli JUMP TO %lli\n", cpu_core, ep);
		#endif
		EXE_NEXT();
	}

	eoffs = 10;

	EXE_NEXT();


	stpushb:
	#if DEBUG
	printf("%lli STPUSHB\n", cpu_core);
	#endif
	arg1 = code[ep + 1];

	sp = stpushb (regi[arg1], sp, sp_bottom);
	if (sp == NULL)
	{
		PRINT_EPOS();
		free (jumpoffs);
		loop_stop ();
		pthread_exit ((void *) 1);
	}
	eoffs = 2;

	EXE_NEXT();

	stpopb:
	#if DEBUG
	printf("%lli STPOPB\n", cpu_core);
	#endif
	arg1 = code[ep + 1];

	sp = stpopb ((U1 *) &regi[arg1], sp, sp_top);
	if (sp == NULL)
	{
		PRINT_EPOS();
		free (jumpoffs);
		loop_stop ();
		pthread_exit ((void *) 1);
	}
	eoffs = 2;

	EXE_NEXT();

	stpushi:
	#if DEBUG
	printf("%lli STPUSHI\n", cpu_core);
	#endif

	arg1 = code[ep + 1];

	sp = stpushi (regi[arg1], sp, sp_bottom);
	if (sp == NULL)
	{
		PRINT_EPOS();
		free (jumpoffs);
		loop_stop ();
		pthread_exit ((void *) 1);
	}
	eoffs = 2;

	EXE_NEXT();

	stpopi:
	#if DEBUG
	printf("%lli STPOPI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];

	sp = stpopi ((U1 *) &regi[arg1], sp, sp_top);
	if (sp == NULL)
	{
		PRINT_EPOS();
		free (jumpoffs);
		loop_stop ();
		pthread_exit ((void *) 1);
	}
	eoffs = 2;

	EXE_NEXT();

	stpushd:
	#if DEBUG
	printf("%lli STPUSHD\n", cpu_core);
	#endif
	arg1 = code[ep + 1];

	sp = stpushd (regd[arg1], sp, sp_bottom);
	if (sp == NULL)
	{
		PRINT_EPOS();
		free (jumpoffs);
		loop_stop ();
		pthread_exit ((void *) 1);
	}
	eoffs = 2;

	EXE_NEXT();

	stpopd:
	#if DEBUG
	printf("%lli STPOPD\n", cpu_core);
	#endif
	arg1 = code[ep + 1];

	sp = stpopd ((U1 *) &regd[arg1], sp, sp_top);
	if (sp == NULL)
	{
		PRINT_EPOS();
		free (jumpoffs);
		loop_stop ();
		pthread_exit ((void *) 1);
	}
	eoffs = 2;

	EXE_NEXT();

	loada:
	#if DEBUG
	printf ("%lli LOADA\n", cpu_core);
	#endif
	// data
	bptr = (U1 *) &arg1;

	*bptr = code[ep + 1];
	bptr++;
	*bptr = code[ep + 2];
	bptr++;
	*bptr = code[ep + 3];
	bptr++;
	*bptr = code[ep + 4];
	bptr++;
	*bptr = code[ep + 5];
	bptr++;
	*bptr = code[ep + 6];
	bptr++;
	*bptr = code[ep + 7];
	bptr++;
	*bptr = code[ep + 8];

	// offset

	//printf ("arg1: %li\n", arg1);

	bptr = (U1 *) &arg2;

	*bptr = code[ep + 9];
	bptr++;
	*bptr = code[ep + 10];
	bptr++;
	*bptr = code[ep + 11];
	bptr++;
	*bptr = code[ep + 12];
	bptr++;
	*bptr = code[ep + 13];
	bptr++;
	*bptr = code[ep + 14];
	bptr++;
	*bptr = code[ep + 15];
	bptr++;
	*bptr = code[ep + 16];

	//printf ("arg2: %li\n", arg2);

	#if BOUNDSCHECK
	if (memory_bounds (arg1, arg2) != 0)
	{
		PRINT_EPOS();
		free (jumpoffs);
		loop_stop ();
		pthread_exit ((void *) 1);
	}
	#endif

	arg3 = code[ep + 17];

	bptr = (U1 *) &regi[arg3];

	*bptr = data[arg1 + arg2];
	bptr++;
	*bptr = data[arg1 + arg2 + 1];
	bptr++;
	*bptr = data[arg1 + arg2 + 2];
	bptr++;
	*bptr = data[arg1 + arg2 + 3];
	bptr++;
	*bptr = data[arg1 + arg2 + 4];
	bptr++;
	*bptr = data[arg1 + arg2 + 5];
	bptr++;
	*bptr = data[arg1 + arg2 + 6];
	bptr++;
	*bptr = data[arg1 + arg2 + 7];

	eoffs = 18;
	EXE_NEXT();

	loadd:
	#if DEBUG
	printf ("%lli LOADD\n", cpu_core);
	#endif
	// data
	bptr = (U1 *) &arg1;

	*bptr = code[ep + 1];
	bptr++;
	*bptr = code[ep + 2];
	bptr++;
	*bptr = code[ep + 3];
	bptr++;
	*bptr = code[ep + 4];
	bptr++;
	*bptr = code[ep + 5];
	bptr++;
	*bptr = code[ep + 6];
	bptr++;
	*bptr = code[ep + 7];
	bptr++;
	*bptr = code[ep + 8];

	// offset
	bptr = (U1 *) &arg2;

	*bptr = code[ep + 9];
	bptr++;
	*bptr = code[ep + 10];
	bptr++;
	*bptr = code[ep + 11];
	bptr++;
	*bptr = code[ep + 12];
	bptr++;
	*bptr = code[ep + 13];
	bptr++;
	*bptr = code[ep + 14];
	bptr++;
	*bptr = code[ep + 15];
	bptr++;
	*bptr = code[ep + 16];

	#if BOUNDSCHECK
	if (memory_bounds (arg1, arg2) != 0)
	{
		PRINT_EPOS();
		free (jumpoffs);
		loop_stop ();
		pthread_exit ((void *) 1);
	}
	#endif

	arg3 = code[ep + 17];

	bptr = (U1 *) &regd[arg3];

	*bptr = data[arg1 + arg2];
	bptr++;
	*bptr = data[arg1 + arg2 + 1];
	bptr++;
	*bptr = data[arg1 + arg2 + 2];
	bptr++;
	*bptr = data[arg1 + arg2 + 3];
	bptr++;
	*bptr = data[arg1 + arg2 + 4];
	bptr++;
	*bptr = data[arg1 + arg2 + 5];
	bptr++;
	*bptr = data[arg1 + arg2 + 6];
	bptr++;
	*bptr = data[arg1 + arg2 + 7];

	eoffs = 18;
	EXE_NEXT();

	intr0:

	arg1 = code[ep + 1];
	#if DEBUG
	printf ("%lli INTR0: %lli\n", cpu_core, arg1);
	#endif
	switch (arg1)
	{
		case 0:
			//printf ("LOADMODULE\n");
			arg2 = code[ep + 2];
			arg3 = code[ep + 3];

			if (load_module ((U1 *) &data[regi[arg2]], regi[arg3]) != 0)
			{
				printf ("EXIT!\n");
				free (jumpoffs);
				loop_stop ();
				pthread_exit ((void *) 1);
			}
			eoffs = 5;
			break;

		case 1:
			//printf ("FREEMODULE\n");
			arg2 = code[ep + 2];

			free_module (regi[arg2]);
			eoffs = 5;
			break;

		case 2:
			//printf ("SETMODULEFUNC\n");
			arg2 = code[ep + 2];
			arg3 = code[ep + 3];
			arg4 = code[ep + 4];

			if (set_module_func (regi[arg2], regi[arg3], (U1 *) &data[regi[arg4]]) != 0)
			{
				printf ("EXIT!\n");
				free (jumpoffs);
				loop_stop ();
				pthread_exit ((void *) 1);
			}
			eoffs = 5;
			break;

		case 3:
			//printf ("CALLMODULEFUNC\n");
			arg2 = code[ep + 2];
			arg3 = code[ep + 3];

			sp = call_module_func (regi[arg2], regi[arg3], (U1 *) sp, sp_top, sp_bottom, (U1 *) data);
			eoffs = 5;
			if (sp == NULL)
			{
				// ERROR -> EXIT
				retcode = 1;
				free (jumpoffs);
				pthread_mutex_lock (&data_mutex);
				threaddata[cpu_core].status = STOP;
				pthread_mutex_unlock (&data_mutex);
				loop_stop ();
				pthread_exit ((void *) retcode);
			}
			break;

		case 4:
			//printf ("PRINTI\n");
			arg2 = code[ep + 2];
			printf ("%lli", regi[arg2]);
			eoffs = 5;
			break;

		case 5:
			//printf ("PRINTD\n");
			arg2 = code[ep + 2];
			printf ("%.10lf", regd[arg2]);
			eoffs = 5;
			break;

		case 6:
			//printf ("PRINTSTR\n");
			arg2 = code[ep + 2];
			printf ("%s", (char *) &data[regi[arg2]]);
			eoffs = 5;
			break;

		case 7:
			//printf ("PRINTNEWLINE\n");
			printf ("\n");
			eoffs = 5;
			break;

		case 8:
			if (silent_run == 0)
			{
				printf ("DELAY\n");
			}
			arg2 = code[ep + 2];
			usleep (regi[arg2] * 1000);
			#if DEBUG
				printf ("delay: %lli\n", regi[arg2]);
			#endif
			eoffs = 5;
			break;

		case 9:
			//printf ("INPUTI\n");
			arg2 = code[ep + 2];
			input_str[0] = '\0';
			if (fgets ((char *) input_str, MAXINPUT - 1, stdin) != NULL)
			{
				regi[arg2] = 0;
				sscanf ((const char *) input_str, "%lli", &regi[arg2]);
			}
			else
			{
				printf ("input integer: can't read!\n");
				PRINT_EPOS();
			}
			eoffs = 5;
			break;

		case 10:
			//printf ("INPUTD\n");
			arg2 = code[ep + 2];
			input_str[0] = '\0';
			if (fgets ((char *) input_str, MAXINPUT - 1, stdin) != NULL)
			{
				regd[arg2] = 0.0;
				sscanf ((const char *) input_str, "%lf", &regd[arg2]);
			}
			else
			{
				printf ("input double: can't read!\n");
				PRINT_EPOS();
			}
			eoffs = 5;
			break;

		case 11:
			//printf ("INPUTS\n");
			arg2 = code[ep + 2];
			arg3 = code[ep + 3];

			{
				U1 ch;
				S8 i = 0;

				while (1)
				{
					if (i < regi[arg2])
					{
						ch = getchar ();
						if (memory_bounds (regi[arg3], i) != 0)
						{
							// ERROR string variable overflow!
							printf ("ERROR: input: string variable overflow!\n");
							PRINT_EPOS();
							free (jumpoffs);
							loop_stop ();
							pthread_exit ((void *) 1);
						}
						data[regi[arg3] + i] = ch;
						if (ch == 10)
						{
							if (i == 0)
							{
								data[regi[arg3]] = '\0';
								break;
							}
							else
							{
								data[regi[arg3] + i] = '\0';
								break;
							}
						}
						i++;
					}
					else
					{
						break;
					}
				}
			}
			eoffs = 5;
			break;

		case 12:
			//printf ("SHELLARGSNUM\n");
			arg2 = code[ep + 2];
			regi[arg2] = shell_args_ind + 1;
			eoffs = 5;
			break;

		case 13:
			//printf ("GETSHELLARG\n");
			{
			S8 shell_arg_len ALIGN = 0;
			arg2 = code[ep + 2];
			arg3 = code[ep + 3];
			if (regi[arg2] > shell_args_ind)
			{
				printf ("ERROR: shell argument index out of range!\n");
				PRINT_EPOS();
				free (jumpoffs);
				loop_stop ();
				pthread_exit ((void *) 1);
			}

			// check target string size
			shell_arg_len = strlen_safe ((const char *) shell_args[regi[arg2]], MAXLINELEN) + 1;

			if (memory_bounds (regi[arg3], shell_arg_len) != 0)
			{
				printf ("ERROR: string overflow: shell argument %lli can't be saved in variable!\n", regi[arg2]);
				PRINT_EPOS();
				free (jumpoffs);
				loop_stop ();
        		pthread_exit ((void *) 1);
			}

			snprintf ((char *) &data[regi[arg3]], shell_arg_len, "%s", (const char *) shell_args[regi[arg2]]);
			eoffs = 5;
			}
			break;

		case 14:
			//printf("SHOWSTACKPOINTER\n");
			printf ("stack pointer sp: %lli\n", (S8) sp);
			if (sp == sp_top)
			{
				printf ("stack is empty!\n\n");
			}
			else
			{
				printf ("stack has data!\n\n");
			}
			eoffs = 5;
			break;

        case 15:
            // return number of CPU cores available
            arg2 = code[ep + 2];
            regi[arg2] = max_cpu;
            eoffs = 5;
            break;

        case 16:
            // return endianess of host machine
            arg2 = code[ep + 2];
#if MACHINE_BIG_ENDIAN
            regi[arg2] = 1;
#else
            regi[arg2] = 0;
#endif
            eoffs = 5;
            break;

		case 17:
			// return current time
			arg2 = code[ep + 2];
			arg3 = code[ep + 3];
			arg4 = code[ep + 4];

			time (&secs);
			tm = localtime (&secs);
			regi[arg2] = tm->tm_hour;
			regi[arg3] = tm->tm_min;
			regi[arg4] = tm->tm_sec;
			eoffs = 5;
			break;

		case 18:
			// return date
			arg2 = code[ep + 2];
			arg3 = code[ep + 3];
			arg4 = code[ep + 4];

			time (&secs);
			tm = localtime (&secs);
			regi[arg2] = tm->tm_year + 1900;
			regi[arg3] = tm->tm_mon + 1;
			regi[arg4] = tm->tm_mday;
			eoffs = 5;
			break;

		case 19:
			// return weekday since Sunday
			// Sunday = 0
			arg2 = code[ep + 2];

			time (&secs);
			tm = localtime (&secs);
			regi[arg2] = tm->tm_wday;
			eoffs = 5;
			break;

		case 20:
			//printf ("PRINTI format\n");
			arg2 = code[ep + 2];
			arg3 = code[ep + 3];

			printf ((char *) &data[regi[arg3]], regi[arg2]);
			eoffs = 5;
			break;

		case 21:
			//printf ("PRINTD format\n");
			arg2 = code[ep + 2];
			arg3 = code[ep + 3];

			printf ((char *) &data[regi[arg3]], regd[arg2]);
			eoffs = 5;
			break;

		case 22:
			//printf ("PRINTI as int16\n");
			arg2 = code[ep + 2];

			printf ("%d", (S2) regi[arg2]);
			eoffs = 5;
			break;

		case 23:
			//printf ("PRINTI as int32\n");
			arg2 = code[ep + 2];

			printf ("%i", (S4) regi[arg2]);
			eoffs = 5;
			break;

#if TIMER_USE
		case 24:
			gettimeofday (&timer_start, NULL);
			eoffs = 5;
			break;

		case 25:
			arg2 = code[ep + 2];
			gettimeofday (&timer_end, NULL);

			timer_double = (double) (timer_end.tv_usec - timer_start.tv_usec) / 1000000 + (double) (timer_end.tv_sec - timer_start.tv_sec);
			timer_double = timer_double * 1000.0; 	// get ms
			printf ("TIMER ms: %.10lf\n", timer_double);
			timer_int = ceil (timer_double);
			regi[arg2] = timer_int;
			eoffs = 5;
			break;
#else
		case 24:
			printf ("FATAL ERROR: no start timer!\n");
			PRINT_EPOS();
			free (jumpoffs);
			loop_stop ();
			pthread_exit ((void *) 1);
			break;

		case 25:
			printf ("FATAL ERROR: no end timer!\n");
			PRINT_EPOS();
			free (jumpoffs);
			loop_stop ();
			pthread_exit ((void *) 1);
			break;
#endif

		case 26:
			// check if stack is empty, if not then give an error message and exit!!!
			if (sp != sp_top)
			{
				printf ("ERROR: stack has data! Stack should be empty!\n");
				PRINT_EPOS();
				free (jumpoffs);
				loop_stop ();
				pthread_exit ((void *) 1);
			}
			eoffs = 5;
			break;

		case 27:
			// print out debug note
			printf ("INTR0: 27 DEBUG marking\n")
			PRINT_EPOS();
			eoffs = 5;
			break;

		case 28:
			// check if int variable value is in legal range
			arg2 = code[ep + 2];
			arg3 = code[ep + 3];	// min range
			arg4 = code[ep + 4];	// max range

			if (regi[arg2] < regi[arg3] || regi[arg2] > regi[arg4])
			{
				printf ("ERROR: int variable value in illegal range!\n");
				PRINT_EPOS();
			}
			eoffs = 5;
			break;

		case 29:
			// check if double variable value is in legal range
			arg2 = code[ep + 2];
			arg3 = code[ep + 3];	// min range
			arg4 = code[ep + 4];	// max range

			if (regd[arg2] < regd[arg3] || regd[arg2] > regd[arg4])
			{
				printf ("ERROR: double variable value in illegal range!\n");
				PRINT_EPOS();
			}
			eoffs = 5;
			break;

		case 251:
			// set overflow on double reg
			arg2 = code[ep + 2];
			overflow = 0;
			if (double_state (regd[arg2]) == 1)
			{
				overflow = 1;
			}
			eoffs = 5;
			break;

		case 252:
			// get overflow flag
			arg2 = code[ep + 2];
			regi[arg2] = overflow;
			eoffs = 5;
			break;

#if JIT_COMPILER
        case 253:
        // run JIT compiler
			if (JIT_initialized == 0)
			{
				if (alloc_jit_code () != 0)
				{
					printf ("FATAL ERROR: JIT compiler: can't alloc memory!\n");
					PRINT_EPOS();
	                free (jumpoffs);
					loop_stop ();
	            	pthread_exit ((void *) 1);
				}

				JIT_initialized = 1;	// JIT compiler ready to use!
			}

            arg2 = code[ep + 2];
            arg3 = code[ep + 3];

			if (jit_compiler ((U1 *) code, (U1 *) data, (S8 *) jumpoffs, (S8 *) &regi, (F8 *) &regd, (U1 *) sp, sp_top, sp_bottom, regi[arg2], regi[arg3], JIT_code, JIT_code_ind, code_size) != 0)
            {
                printf ("FATAL ERROR: JIT compiler: can't compile!\n");
				PRINT_EPOS();
                free (jumpoffs);
				loop_stop ();
            	pthread_exit ((void *) 1);
            }

            eoffs = 5;
            break;

        case 254:
            arg2 = code[ep + 2];
            // printf ("intr0: 254: RUN JIT CODE: %i\n", arg2);
			if (run_jit (regi[arg2], JIT_code, JIT_code_ind) == 1)
			{
				// ERROR can't run JIT-code 
				PRINT_EPOS();
                free (jumpoffs);
				loop_stop ();
            	pthread_exit ((void *) 1);
			}

            eoffs = 5;
            break;
#else
		case 253:
			printf ("FATAL ERROR: no JIT compiler: can't compile!\n");
			PRINT_EPOS();
			free (jumpoffs);
			loop_stop ();
			pthread_exit ((void *) 1);
			break;

		case 254:
			printf ("FATAL ERROR: no JIT compiler: can't execute!\n");
			PRINT_EPOS();
			free (jumpoffs);
			loop_stop ();
			pthread_exit ((void *) 1);
			break;
#endif


		case 255:
			if (silent_run == 0)
			{
				printf ("EXIT\n");
			}
			arg2 = code[ep + 2];
			retcode = regi[arg2];
			free (jumpoffs);
			pthread_mutex_lock (&data_mutex);
			threaddata[cpu_core].status = STOP;
			pthread_mutex_unlock (&data_mutex);
			loop_stop ();
			pthread_exit ((void *) retcode);
			break;

		default:
			printf ("FATAL ERROR: INTR0: %lli does not exist!\n", arg1);
			PRINT_EPOS();
			free (jumpoffs);
			loop_stop ();
			pthread_exit ((void *) 1);
	}
	EXE_NEXT();

	intr1:
	#if DEBUG
	printf("%lli INTR1\n", cpu_core);
	#endif
	// special interrupt
	arg1 = code[ep + 1];

	switch (arg1)
	{
		case 0:
			// run new CPU instance
			arg2 = code[ep + 2];
			arg2 = regi[arg2];

            // search for a free CPU core
            // if none free found set new_cpu to -1, to indicate all CPU cores are used!!
            new_cpu = -1;
            pthread_mutex_lock (&data_mutex);
            for (i = 0; i < max_cpu; i++)
            {
                if (threaddata[i].status == STOP)
                {
                    new_cpu = i;
                    break;
                }
            }
	        pthread_mutex_unlock (&data_mutex);
			if (new_cpu == -1)
			{
				// maximum of CPU cores used, no new core possible

				printf ("ERROR: can't start new CPU core!\n");
				PRINT_EPOS();
				free (jumpoffs);
				loop_stop ();
				pthread_exit ((void *) 1);
			}

            // run new CPU core
            // set threaddata

			printf ("current CPU: %lli, starts new CPU: %lli\n", cpu_core, new_cpu);
			pthread_mutex_lock (&data_mutex);
			threaddata[new_cpu].sp = sp;
			threaddata[new_cpu].sp_top = sp_top;
			threaddata[new_cpu].sp_bottom = sp_bottom;

			threaddata[new_cpu].sp_top_thread = sp_top + (new_cpu * stack_size);
			threaddata[new_cpu].sp_bottom_thread = sp_bottom + (new_cpu * stack_size);
			threaddata[new_cpu].sp_thread = threaddata[new_cpu].sp_top_thread - (sp_top - sp);
			threaddata[new_cpu].ep_startpos = arg2;
			pthread_mutex_unlock (&data_mutex);

            // create new POSIX thread

			if (pthread_create (&threaddata[new_cpu].id, NULL, (void *) run, (void*) new_cpu) != 0)
			{
				printf ("ERROR: can't start new thread!\n");
				PRINT_EPOS();
				free (jumpoffs);
				loop_stop ();
				pthread_exit ((void *) 1);
			}


            #if CPU_SET_AFFINITY
            // LOCK thread to CPU core

            CPU_ZERO (&cpuset);
            CPU_SET (new_cpu, &cpuset);

            if (pthread_setaffinity_np (threaddata[new_cpu].id, sizeof(cpu_set_t), &cpuset) != 0)
            {
                    printf ("ERROR: setting pthread affinity of thread: %lli\n", new_cpu);
					PRINT_EPOS();
            }
            #endif

			pthread_mutex_lock (&data_mutex);
			threaddata[new_cpu].status = RUNNING;
			pthread_mutex_unlock (&data_mutex);
			eoffs = 5;
			break;

		case 1:
			// join threads
			printf ("JOINING THREADS...\n");
			U1 wait = 1, running;
			while (wait == 1)
			{
				running = 0;
				pthread_mutex_lock (&data_mutex);
				for (i = 1; i < max_cpu; i++)
				{
					if (threaddata[i].status == RUNNING)
					{
						// printf ("CPU: %lli running\n", i);
						running = 1;
					}
				}
				pthread_mutex_unlock (&data_mutex);
				if (running == 0)
				{
					// no child threads running, joining done
					wait = 0;
				}

				usleep (200);
			}

			eoffs = 5;
			break;

		case 2:
			// lock data_mutex
			pthread_mutex_lock (&data_mutex);
			eoffs = 5;
			break;

		case 3:
			// unlock data_mutex
			pthread_mutex_unlock (&data_mutex);
			eoffs = 5;
			break;

		case 4:
			// return number of current CPU core
			arg2 = code[ep + 2];
			regi[arg2] = cpu_core;

			eoffs = 5;
			break;

		case 5:
			// return number of free CPU cores
			// search for a free CPU core
			// if none free found set cpus_free to 0, to indicate all CPU cores are used!!

			cpus_free = 0;
			pthread_mutex_lock (&data_mutex);
			for (i = 0; i < max_cpu; i++)
			{
				if (threaddata[i].status == STOP)
				{
					cpus_free++;
				}
			}
			pthread_mutex_unlock (&data_mutex);

			arg2 = code[ep + 2];
			regi[arg2] = cpus_free;

			eoffs = 5;
			break;

		case 6:
			// set global mutex 
			arg2 = code[ep + 2];
			if (regi[arg2] >= 0 && regi[arg2] < MAX_MUTEXES)
			{
				pthread_mutex_lock (&global_mutex[regi[arg2]]);
			}
			else 
			{
				printf ("interrupt 1: set global mutex index overflow!\n");
				PRINT_EPOS();
				free (jumpoffs);
				loop_stop ();
				pthread_exit ((void *) 1);
			}

			eoffs = 5;
			break;
			
		case 7:
			// unset global mutex 
			arg2 = code[ep + 2];
			if (regi[arg2] >= 0 && regi[arg2] < MAX_MUTEXES)
			{
				pthread_mutex_unlock (&global_mutex[regi[arg2]]);
			}
			else 
			{
				printf ("interrupt 1: unset global mutex index overflow!\n");
				PRINT_EPOS();
				free (jumpoffs);
				loop_stop ();
				pthread_exit ((void *) 1);
			}

			eoffs = 5;
			break;

			
		case 255:
			printf ("thread EXIT\n");
			arg2 = code[ep + 2];
			retcode = regi[arg2];
			pthread_mutex_lock (&data_mutex);
			threaddata[cpu_core].status = STOP;
			pthread_mutex_unlock (&data_mutex);
			// loop_stop ();
			pthread_exit ((void *) retcode);
			break;

		default:
			printf ("FATAL ERROR: INTR1: %lli does not exist!\n", arg1);
			PRINT_EPOS();
			free (jumpoffs);
			loop_stop ();
			pthread_exit ((void *) 1);
	}
	EXE_NEXT();

	//  superopcodes for counter loops
	inclsijmpi:
	#if DEBUG
	printf ("%lli INCLSIJMPI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];

	arg3 = jumpoffs[ep];

	//printf ("jump to: %li\n", arg3);

	regi[arg1]++;
	if (regi[arg1] < regi[arg2])
	{
		eoffs = 0;
		ep = arg3;
		EXE_NEXT();
	}

	eoffs = 11;

	EXE_NEXT();

	decgrijmpi:
	#if DEBUG
	printf ("%lli DECGRIJMPI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];

	arg3 = jumpoffs[ep];

	regi[arg1]--;
	if (regi[arg1] > regi[arg2])
	{
		eoffs = 0;
		ep = arg3;
		EXE_NEXT();
	}

	eoffs = 11;

	EXE_NEXT();

	movi:
	#if DEBUG
	printf ("%lli MOVI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];

	regi[arg2] = regi[arg1];

	eoffs = 3;
	EXE_NEXT();

	movd:
	#if DEBUG
	printf ("%lli MOVD\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];

	regd[arg2] = regd[arg1];

	eoffs = 3;
	EXE_NEXT();

	loadl:
	#if DEBUG
	printf ("%lli LOADL\n", cpu_core);
	#endif
	// data
	bptr = (U1 *) &arg1;
	arg2 = code[ep + 9];

	*bptr = code[ep + 1];
	bptr++;
	*bptr = code[ep + 2];
	bptr++;
	*bptr = code[ep + 3];
	bptr++;
	*bptr = code[ep + 4];
	bptr++;
	*bptr = code[ep + 5];
	bptr++;
	*bptr = code[ep + 6];
	bptr++;
	*bptr = code[ep + 7];
	bptr++;
	*bptr = code[ep + 8];

	regi[arg2] = arg1;

	eoffs = 10;
	EXE_NEXT();

	jmpa:
	#if DEBUG
	printf ("%lli JMPA\n", cpu_core);
	#endif
	arg1 = code[ep + 1];

	eoffs = 0;
	ep = regi[arg1];
	#if DEBUG
	printf ("%lli JUMP TO %lli\n", cpu_core, ep);
	#endif
	EXE_NEXT();

	jsr:
	#if DEBUG
	printf ("%lli JSR\n", cpu_core);
	#endif
	arg1 = jumpoffs[ep];

	if (jumpstack_ind == MAXSUBJUMPS - 1)
	{
		printf ("ERROR: jumpstack full, no more jsr!\n");
		PRINT_EPOS();
		free (jumpoffs);
		loop_stop ();
		pthread_exit ((void *) 1);
	}

	jumpstack_ind++;
	jumpstack[jumpstack_ind] = ep + 9;

	eoffs = 0;
	ep = arg1;

	EXE_NEXT();

	jsra:
	#if DEBUG
	printf ("%lli JSRA\n", cpu_core);
	#endif

	arg1 = code[ep + 1];

	#if DEBUG
	printf ("%lli JUMP TO %lli\n", cpu_core, ep);
	#endif

	if (jumpstack_ind == MAXSUBJUMPS - 1)
	{
		printf ("ERROR: jumpstack full, no more jsr!\n");
		PRINT_EPOS();
		free (jumpoffs);
		loop_stop ();
		pthread_exit ((void *) 1);
	}

	jumpstack_ind++;
	jumpstack[jumpstack_ind] = ep + 2;

	eoffs = 0;
	ep = regi[arg1];

	EXE_NEXT();

	rts:
	#if DEBUG
	printf ("%lli RTS\n", cpu_core);
	#endif

	ep = jumpstack[jumpstack_ind];
	eoffs = 0;
	jumpstack_ind--;

	EXE_NEXT();

	load:
	#if DEBUG
	printf ("%lli LOAD\n", cpu_core);
	#endif

	// data
	bptr = (U1 *) &arg1;

	*bptr = code[ep + 1];
	bptr++;
	*bptr = code[ep + 2];
	bptr++;
	*bptr = code[ep + 3];
	bptr++;
	*bptr = code[ep + 4];
	bptr++;
	*bptr = code[ep + 5];
	bptr++;
	*bptr = code[ep + 6];
	bptr++;
	*bptr = code[ep + 7];
	bptr++;
	*bptr = code[ep + 8];

	// offset

	bptr = (U1 *) &arg2;

	*bptr = code[ep + 9];
	bptr++;
	*bptr = code[ep + 10];
	bptr++;
	*bptr = code[ep + 11];
	bptr++;
	*bptr = code[ep + 12];
	bptr++;
	*bptr = code[ep + 13];
	bptr++;
	*bptr = code[ep + 14];
	bptr++;
	*bptr = code[ep + 15];
	bptr++;
	*bptr = code[ep + 16];

	//printf ("arg2: %li\n", arg2);

	arg3 = code[ep + 17];

	regi[arg3] = arg1 + arg2;

	eoffs = 18;
	EXE_NEXT();


    noti:
	#if DEBUG
	printf ("%lli NOTI\n", cpu_core);
	#endif
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];

	regi[arg2] = ! regi[arg1];

	eoffs = 3;
	EXE_NEXT();
}

void break_handler (void)
{
	/* break - handling
	 *
	 * if user answer is 'y', the engine will shutdown
	 *
	 */

	U1 answ[2];

	printf ("\nexe: break,  exit now (y/n)? ");
	scanf ("%1s", answ);

	if (strcmp ((const char *) answ, "y") == 0 || strcmp ((const char *) answ, "Y") == 0)
	{
		cleanup ();
		exit (1);
	}
}

void init_modules (void)
{
    S8 i ALIGN;

    for (i = 0; i < MODULES; i++)
    {
        strcpy ((char *) modules[i].name, "");
		modules[i].used = 0;
    }
}

void free_modules (void)
{
    S8 i ALIGN;

    for (i = MODULES - 1; i >= 0; i--)
    {
        if (modules[i].used != 0)
        {
			if (silent_run == 0)
			{
            	printf ("free_modules: module %lli %s free.\n", i, modules[i].name);
			}
            free_module (i);
        }
    }
}

void show_info (void)
{
	printf ("l1vm <program> [-C cpu_cores] [-S stacksize] [-q] [-p run priority (-20 - 19)] <-args> <cmd args>\n");
	printf ("-C cores : set maximum of threads that can be run\n");
	printf ("-S stacksize : set the stack size\n");
	printf ("-q : quiet run, don't show welcome messages\n");
	printf ("-p : set run priority, -20 = highest, 19 = lowest priority\n\n");
	printf ("program arguments for the program must be set by '-args':\n");
	printf ("l1vm programname -args foo bar\n");
	printf ("%s", VM_VERSION_STR);
	printf ("%s\n", COPYRIGHT_STR);
}

S2 run_main_loop_thread (void *arg)
{
	U1 wait = 1;
	S8 delay ALIGN = 500;
	U1 do_stop = 0;

	// printf ("DEBUG: run_main_loop_thread...\n");

	while (wait)
	{
		// printf ("DEBUG: run_main_loop_thread loop...\n");
		usleep (delay * 1000);
		pthread_mutex_lock (&wait_loop);
		do_stop = run_loop;
		pthread_mutex_unlock (&wait_loop);
		if (do_stop == 0)
		{
			usleep (500 * 1000); // delay
			// printf ("run_main_loop_thread: EXIT!\n");
			threaddata[1].status = STOP;
			pthread_exit ((void *) 0);
		}
	}
	threaddata[1].status = STOP;
	pthread_exit ((void *) 0);
}

void show_file_root_path (void)
{
	char *home;

	#if SANDBOX
	home = get_home ();
	printf ("sandbox root = %s%s\n", home, SANDBOX_ROOT);
	#else 
	printf ("no sandbox root set!\n");
	#endif
}

void show_run_info (void)
{
	// do compilation time sense check on integer 64 bit and double 64 bit type!!
	S8 size_int64 ALIGN;
	S8 size_double64 ALIGN;
	size_int64 = sizeof (S8);
	if (size_int64 != 8)
	{
		printf ("FATAL compiler ERROR: size of S8 not 8 bytes (64 bit!): %lli bytes only!!\n", size_int64);
		cleanup ();
		exit (1);
	}

	size_double64 = sizeof (F8);
	if (size_double64 != 8)
	{
		printf ("FATAL compiler ERROR: size of F8 not 8 bytes (64 bit!): %lli bytes only!!\n", size_double64);
		cleanup ();
		exit (1);
	}

	printf ("l1vm - %s -%s\n", VM_VERSION_STR, COPYRIGHT_STR);
	printf (">>> lightning speed <<<\n");
	printf ("CPU cores: %lli (STATIC)\n", max_cpu);

	printf ("internal type check: S8 = %lli bytes, F8 = %lli bytes. All OK!\n", size_int64, size_double64);

	#if defined(__clang__)
		printf ("C compiler: clang version %s\n", __clang_version__);
	#elif defined(__GNUC__) || defined(__GNUG__)
		printf ("C compiler: gcc version %s\n", __VERSION__);
	#endif

	printf ("build on: %s\n", __DATE__);

	show_file_root_path ();

	#if JIT_COMPILER
	    printf ("JIT-compiler: ready to load module.\n");
	#endif

	#if MATH_LIMITS
		printf (">> math overflow check << ");
	#endif

	#if BOUNDSCHECK
		printf (">> boundscheck << ");
	#endif

	#if DIVISIONCHECK
		printf (">> divisioncheck << ");
	#endif

	#if STACK_CHECK
		printf (">> stackcheck <<");
	#endif

	printf ("\n");
	printf ("machine: ");
	#if MACHINE_BIG_ENDIAN
		printf ("big endianess\n");
	#else
		printf ("little endianess\n");
	#endif
}

int main (int ac, char *av[])
{
	S8 i ALIGN;
	S8 arglen ALIGN;
	S8 strind ALIGN;
	S8 avind ALIGN;
	U1 cmd_args = 0;		// switched to one, if arguments follow

	pthread_t id;

	S8 new_cpu ALIGN;

	// process priority on Linux
    S4 run_priority = 0;        // -20 = highest priority, 19 = lowest priority


	// printf ("DEBUG: ac: %i\n", ac);

    if (ac > 1)
    {
        for (i = 1; i < ac; i++)
        {
            arglen = strlen_safe (av[i], MAXLINELEN);

			// printf ("DEBUG: arg: '%s'\n", av[i]);

			if (cmd_args == 1)
			{
				// printf ("got shellarg: '%s'\n", av[i]);

					if (shell_args_ind < MAXSHELLARGS - 1)
					{
						shell_args_ind++;
						if (strlen_safe (av[i], MAXSHELLARGLEN - 1) < MAXSHELLARGLEN -1)
						{
							strcpy ((char *) shell_args[shell_args_ind], av[i]);
						}
						else
						{
							printf ("ERROR: shell argument: '%s' too long!\n", av[i]);
							cleanup ();
							exit (1);
						}

						// printf ("arg: %i: '%s'\n", shell_args_ind, av[i]);
					}
			}
			else
			{
				if (arglen >= 2)
				{
					// get VM specific args
					if (av[i][0] == '-' || (av[i][0] == '-' && av[i][1] == '-'))
					{
						if (arglen == 2)
						{
							if (av[i][0] == '-' && av[i][1] == 'q')
							{
								silent_run = 1;
							}

							if (av[i][0] == '-' && av[i][1] == 'C')
							{
								if (i < ac - 1)
				                {
									// set max cpu cores flag...
									max_cpu = atoi (av[i + 1]);
									if (max_cpu == 0)
									{
										printf ("ERROR: max_cpu less than 1 core!\n");
										cleanup ();
										exit (1);
									}
									printf ("max_cpu: cores set to: %lli\n", max_cpu);
								}
							}

							if (av[i][0] == '-' && av[i][1] == 'S')
							{
								if (i < ac - 1)
				                {
									// set max stack size flag...
									stack_size = atoi (av[i + 1]);
									if (stack_size == 0)
									{
										printf ("ERROR: stack size is 0!\n");
										cleanup ();
										exit (1);
									}
									printf ("stack_size: stack size set to %lli\n", stack_size);
								}
							}

							if (av[i][0] == '-' && av[i][1] == '?')
							{
								// user needs help, show arguments info and exit
								show_info ();
								cleanup ();
								exit (1);
							}

							if (av[i][0] == '-' && av[i][1] == 'p')
							{
								// set VM run priority
								run_priority = atoi (av[i + 1]);
								if (run_priority < -20 || run_priority > 19)
								{
									printf ("Run priority out of legal range! Set to default 0!\n");
									run_priority = 0;
								}
							}
						}
            			if (arglen > 2)
            			{
                			if (av[i][0] == '-' && av[i][1] == 'M')
							{
								// try load module (shared library)
								modules_ind++;
                    			if (modules_ind < MODULES)
                    			{
                    				strind = 0; avind = 2;
                    				for (avind = 2; avind < arglen; avind++)
                    				{
                        				modules[modules_ind].name[strind] = av[i][avind];
                        				strind++;
                    				}
                        			modules[modules_ind].name[strind] = '\0';

                    				if (load_module (modules[modules_ind].name, modules_ind) == 0)
                    				{
                        				printf ("module: %s loaded\n", modules[modules_ind].name);
                    				}
                    				else
                    				{
                        				printf ("EXIT!\n");
										cleanup ();
                        				exit (1);
                    				}
                				}
                			}
                			else
							{
								if (arglen >= 4)
								{
                    				if (arglen == 6)
									{
										if (strcmp (av[i], "--help") == 0)
										{
											// user needs help, show arguments info and exit
											show_info ();
											cleanup ();
											exit (1);
										}
									}
								}
							}
	            		}
					}
				}
			}
			// get normal shell arg
			if (arglen > 0)
			{
				if (strcmp (av[i], "-args") == 0)
				{
					// printf ("args follow: %lli: '%s\n", av[i]);
					cmd_args = 1;
				}
			}
        }
    }
    else
	{
		show_info ();
		show_run_info ();
		cleanup ();
		exit (1);
	}

	// set run priority
	nice (run_priority);

	threaddata = (struct threaddata *) calloc (max_cpu, sizeof (struct threaddata));
	if (threaddata == NULL)
	{
		printf ("ERROR: can't allocate threaddata!\n");
		cleanup ();
		exit (1);
	}

	if (silent_run == 0)
	{
		show_run_info ();
	}
    if (load_object ((U1 *) av[1]))
    {
		cleanup ();
        exit (1);
    }

    init_modules ();
	signal (SIGINT, (void *) break_handler);

	// set all higher threads as STOPPED = unused
	for (i = 1; i < max_cpu; i++)
	{
		threaddata[i].status = STOP;
	}
	threaddata[0].status = RUNNING;		// main thread will run

	new_cpu = 0;

	threaddata[new_cpu].sp = (U1 *) &data + (data_mem_size - ((max_cpu - 1) * stack_size) - 1);
	threaddata[new_cpu].sp_top = threaddata[new_cpu].sp;
	threaddata[new_cpu].sp_bottom = threaddata[new_cpu].sp_top - stack_size + 1;

	threaddata[new_cpu].sp_thread = threaddata[new_cpu].sp + (new_cpu * stack_size);
	threaddata[new_cpu].sp_top_thread = threaddata[new_cpu].sp_top + (new_cpu * stack_size);
	threaddata[new_cpu].sp_bottom_thread = threaddata[new_cpu].sp_bottom + (new_cpu * stack_size);
	threaddata[new_cpu].ep_startpos = 16;

	// on macOS use wait loop thread, because the SDL module must be run from main thread!!!
	#if __MACH__
	new_cpu++;
    if (pthread_create (&id, NULL, (void *) run_main_loop_thread, (void *) new_cpu) != 0)
	{
		printf ("ERROR: can't start main loop thread!\n");
		cleanup ();
		exit (1);
	}
	threaddata[1].status = RUNNING;

	// start main thread
	run (0);
	#else
	if (pthread_create (&id, NULL, (void *) run, (void *) new_cpu) != 0)
	{
		printf ("ERROR: can't start main thread!\n");
		cleanup ();
		exit (1);
	}
	#endif

    pthread_join (id, NULL);
	cleanup ();
	exit (retcode);
}
