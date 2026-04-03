/*
 * This file main-scheduler.c is part of L1vm.
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

// This is a new VM core with a scheduler for the CPU
// It should run a lot threads at the same time. So you can run more threads as your CPU has logical cores.
// The run function start is now ok.
// I will now change the opcodes to use the "cpu[cpuc]..." CPU variables
// And then write the scheduler.

#include "jit.h"
#include "../include/global.h"
#include "../include/stack.h"
#include "main.h"


// steps to run in current CPU core
#define SCHEDULER_MAX 1000000
#define SCHEDULER_OFF -1

// include/home.h 
char *get_home (void);

S8 data_size ALIGN;

// see global.h user settings on top
S8 max_code_size ALIGN = MAX_CODE_SIZE;
S8 max_data_size ALIGN = MAX_DATA_SIZE;

S8 data_mem_size ALIGN;
S8 stack_size ALIGN = STACKSIZE;		// stack size added to data size when dumped to object file

// code
U1 *code = NULL;

// data
U1 *data_global = NULL;

// local data max
S8 local_data_max ALIGN = LOCAL_DATA_MAX;

S8 code_size ALIGN;
S8 data_ind ALIGN;
S8 modules_ind ALIGN = -1;    // no module loaded = -1

S8 cpu_ind ALIGN = 0;

S8 max_cpu ALIGN = MAXCPUCORES;    // number of threads that can be runned
S8 max_virtcpu ALIGN = MAXVIRTCPU;  // number of virtual CPUs threads
U1 silent_run = 0;				// switch startup and status messages of: "-q" flag on shell

U1 bytecode_hot_reload = 0;     // used in run  bytecode main function, if set then the new rogram bytecode will be run!

typedef S8 (*dll_func)(U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data);

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
	U1 func_set[MODULES_MAXFUNC];
};

struct module modules[MODULES];

// for memory_bounds () initialization in modules
typedef S8 (*dll_memory_init_func)(struct data_info *data_infoptr, S8 data_info_ind);

struct module_init
{
#if __linux__
    void *lptr;
#endif

#if _WIN32
    HINSTANCE lptr;
#endif

    dll_memory_init_func func;
};

struct module_init module_init;


struct data_info data_info[MAXDATAINFO];
S8 data_info_ind ALIGN = -1;

// pthreads data mutex
pthread_mutex_t data_mutex;

// mutexes as defined in settings.h
pthread_mutex_t global_mutex[MAX_MUTEXES];

// wait loop mutex 
pthread_mutex_t wait_loop;
U1 run_loop = 1;

// cpu mutex
pthread_mutex_t cpu_mutex;

// return code of main thread
S8 retcode ALIGN = 0;

// shell arguments
U1 shell_args[MAXSHELLARGS][MAXSHELLARGLEN];
S4 shell_args_ind = -1;

struct threaddata *threaddata;

struct cpu
{
	U1 status;        // set to RUNNING if core has code to execute or STOP
	S8 scheduler;      // counter to set the task switching by the scheduler, if it is 0 then switch to other running CPU core.
	S8 cpu_core ALIGN;
	S8 i ALIGN;
	U1 eoffs;                  	// offset to next opcode
	S8 regi[MAXREG];   		  	// integer registers
	F8 regd[MAXREG];			// double registers
	S8 arg1 ALIGN;
	S8 arg2 ALIGN;
	S8 arg3 ALIGN;
	S8 arg4 ALIGN;				// opcode arguments

	S8 ep ALIGN; 			// execution pointer in code segment
	S8 startpos ALIGN;

	U1 overflow;			// MATH_LIMITS calculation overflow flag

	U1 *sp;  					// stack pointer
	U1 *sp_top;    				// stack pointer start address
	U1 *sp_bottom;				// stack bottom
	U1 *srcptr, *dstptr;

	U1 *bptr;

	U1 *data;

	// jump call stack for jsr, jsra
	S8 jumpstack[MAXSUBJUMPS];
	S8 jumpstack_ind ALIGN;		// empty

	// threads
	S8 new_cpu ALIGN;
	S8 cpus_free ALIGN;

    // thread attach to CPU core
	#if CPU_SET_AFFINITY
	cpu_set_t cp uset;
	#endif

	// for data input
	U1 input_str[MAXINPUT];

	// for time functions
	time_t secs;

	// for local data interrupt
    S8 local_data_ind ALIGN;

	U1 do_memory_bounds_check;

    // jumpoffsets
	S8 *jumpoffs ALIGN;
	S8 offset ALIGN;

	S8 ins_count ALIGN;
};

#define SCHEXE_NEXT() \
    do { \
			cpu[cpuc].ep += cpu[cpuc].eoffs;	\
			cpu[cpuc].eoffs = 0;				\
			ep = cpu[cpuc].ep;  \
			if (ep >= code_size) {			\
            cpu[cpuc].status = STOP; \
            goto task_scheduler; \
        } \
        if (cpu[cpuc].status == STOP) \
        { \
           goto task_scheduler; \
        } \
        if (cpu[cpuc].scheduler == SCHEDULER_OFF) { \
            goto task_scheduler;                \
        } else { \
           goto task_scheduler; \
        } \
    } while (0)


// CPU handle with the scheduler data
struct cpu *cpu;

#if JIT_COMPILER
S2 alloc_jit_code ()
{
	S8 ind ALIGN;

	JIT_code = (struct JIT_code*) calloc (MAXJITCODE, sizeof (struct JIT_code));
	if (JIT_code == NULL)
	{
		printf ("FATAL ERROR: can't allocate JIT_code structure!\n");
		return (1);
	}

	// set used as free:
	for (ind = 0; ind < MAXJITCODE; ind++)
	{
		JIT_code[ind].used = 0;
	}
	return (0);
}
#endif

/*
 * for osv capstan avoiding .so.so names
void strip_library_name (char *libname)
{
	S2 i;
	S2 liblen;
	S2 first_dot = -1;
	S2 second_dot = -1;

	liblen = strlen_safe (libname, MAXSTRLEN);

	if (liblen > 0)
	{
		//printf ("strip_library_name: '%s'\n", libname);

		for (i = 0; i < liblen; i++)
		{
			if (libname[i] == '.')
			{
				if (first_dot == -1)
				{
					first_dot = i;
				}
				else
				{
					if (second_dot == -1 && first_dot != -1)
					{
						second_dot = i;
					}
				}
			}
	   }

	   if (second_dot != -1)
	   {
		   // strip away second .so: libfoo.so.so
		   libname[second_dot] = '\0';
		   //printf ("strip_library_name: '%s'\n", libname);
	   }
    }
}
*/

S2 load_module (U1 *name, S8 ind)
{
	U1 linux_lib_suffix[] = ".so";
	U1 libname[MAXSTRLEN];
	S8 i ALIGN = 0;

	if (strlen_safe ((const char*) name, MAXLINELEN) > MAXLINELEN - 5)
	{
		printf ("load_module: ERROR library name overflow!\n");
		return (1);
	}

#if __linux__ || _WIN32 
	strcpy ((char *) libname, (const char *) name);
	strcat ((char *) libname, (const char *) linux_lib_suffix);
#endif

// strip_library_name((char *) libname);

// check if modules index is in legal range
if (ind >= MODULES)
{
	printf ("error load module: index out of range!\n");
	return (1);
}

// check if this index already used
if (modules[ind].used == 1)
{
	printf ("error load module %s!, index already in use by: %s.\n", libname, modules[ind].name);
	return (1);
}


// check if module already load before
for (i = 0; i < MODULES; i++)
{
	if (modules[i].used == 0)
	{
		if (strcmp ((const char *) libname, (char *) modules[i].name) == 0)
		{
			printf ("error load module %s!, module already load by: %lli: %s.\n", libname, i, modules[i].name);
			return (1);
		}
	}
 }


#if __linux__
    modules[ind].lptr = dlopen ((const char *) libname, RTLD_LAZY);
    if (! modules[ind].lptr)
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


	// call memory_bounds init function in module
	// load init_memory_bounds function
#if __linux__
	dlerror ();

    // load the symbols (handle to function)
    module_init.func = dlsym (modules[ind].lptr, "init_memory_bounds");
    const char* dlsym_error = dlerror ();
    if (dlsym_error)
	{
        printf ("error set module %s, function: '%s'!\n", modules[ind].name, "init_memory_bounds");
		printf ("%s\n", dlsym_error);
        return (1);
    }
#endif

#if _WIN32
    module_init.func = GetProcAddress (modules[ind].lptr, "init_memory_bounds");
    if (! module_init.func)
    {
        printf ("error set module %s, function: '%s'!\n", modules[ind].name, "init_memory_bounds");
        return (1);
    }
#endif

	// call init_memory_bounds function
	return (*module_init.func)(data_info, data_info_ind);
}

void free_module (S8 ind)
{
	// check if modules index is in legal range
	if (ind >= MODULES)
	{
		printf ("error free module: index out of range!\n");
		return;
	}

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
// check if modules index is in legal range
if (ind >= MODULES)
{
	printf ("error set module func: index out of range!\n");
	return (1);
}

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
	modules[ind].func_set[func_ind] = 1; // module function set flag
    return (0);
#endif

#if _WIN32
    modules[ind].func[func_ind] = GetProcAddress (modules[ind].lptr, (const char *) func_name);
    if (! modules[ind].func[func_ind])
    {
        printf ("error set module %s, function: '%s'!\n", modules[ind].name, func_name);
        return (1);
    }
	modules[ind].func_set[func_ind] = 1; // module function set flag
    return (0);
#endif
}

U1 *call_module_func (S8 ind, S8 func_ind, U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// check if modules index is in legal range
	if (ind >= MODULES)
	{
		printf ("error call module func: index out of range!\n");
		return (NULL);
	}

	// avoid call without loaded module
	if (modules[ind].used == 0)
	{
		printf ("FATAL ERROR: module %lli not loaded!\n", ind);
        return (NULL);
	}
    if (modules[ind].func_set[func_ind] == 0)
	{
		printf ("FATAL ERROR: module %lli func %lli not loaded!\n", ind, func_ind);
		return (NULL);
	}

    return ((U1 *)(*modules[ind].func[func_ind])(sp, sp_top, sp_bottom, data));
}

void clean_code (void)
{
	// erase all code and free memory
	S8 i ALIGN;

	for (i = 0; i < code_size; i++)
	{
		code[i] = 0;
	}

	free (code);
}

void clean_data (void)
{
	// erase all data and free memory
	S8 i ALIGN;

	for (i = 0; i < data_mem_size; i++)
	{
		data_global[i] = 0;
	}

	free (data_global);
}

void clean_threaddata_data (S8 cpu)
{
	S8 i ALIGN;
	S8 j ALIGN;
	S8 data_local_size ALIGN = data_mem_size - (stack_size * max_cpu);

	if (threaddata[cpu].data != NULL)
	{
		for (i = 0; i < data_local_size; i++)
		{
			threaddata[cpu].data[i] = 0;
		}

		free (threaddata[cpu].data);
	}

	if (threaddata[cpu].local_data != NULL)
	{
		for (i = 0; i < local_data_max; i++)
		{
		    if (threaddata[cpu].local_data[i])
			{
				// overwrite memory with zeroes
				for (j = 0; j < data_local_size; j++)
				{
					threaddata[cpu].local_data[i][j] = 0;
				}

				free (threaddata[cpu].local_data[i]);
			}
	    }
		free (threaddata[cpu].local_data);
	}
}

void cleanup (void)
{
	S8 i ALIGN;

	#if JIT_COMPILER
	    if (JIT_code)
		{
			free_jit_code (JIT_code);
			if (JIT_code) free (JIT_code);
		}
	#endif

    free_modules ();
	if (data_global) clean_data ();
    if (code) clean_code ();

	pthread_mutex_lock (&data_mutex);

	if (threaddata)
	{
		for (i = 0; i < max_cpu; i++)
		{
			clean_threaddata_data (i);
		}

		if (threaddata) free (threaddata);
	}

	if (cpu)
	{
		for (i = 0; i < max_virtcpu; i++)
		{
			free_jumpoffs (i);
		}
	    free (cpu);
	}

	pthread_mutex_unlock (&data_mutex);
}

void cleanup_hold_data (void)
{
	// for hot code reload cleanup, don't free data!!!

	S8 i ALIGN;

	#if JIT_COMPILER
	    if (JIT_code)
		{
			free_jit_code (JIT_code);
			if (JIT_code) free (JIT_code);
		}
	#endif

    free_modules ();

	pthread_mutex_lock (&data_mutex);

	if (threaddata)
	{
		for (i = 0; i < max_cpu; i++)
		{
			clean_threaddata_data (i);
		}

		if (threaddata) free (threaddata);
	}
	pthread_mutex_unlock (&data_mutex);
}

U1 double_state (F8 num)
{
	S2 state;
	U1 flag = 0;
	state = fpclassify (num);

	switch (state)
	{
		case FP_INFINITE:
			printf ("\nERROR: number infinite!\n");
			flag = 1;

		case FP_NAN:
			printf ("\nERROR: number not a number!\n");
			flag = 1;

		case FP_SUBNORMAL:
			printf ("\nERROR: number underflow!\n");
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

void free_jumpoffs (S8 ind)
{
	if (cpu[ind].jumpoffs)
	{
		free (cpu[ind].jumpoffs);
		cpu[ind].jumpoffs = NULL;
	}
}

// L1VM normal run function
// EDIT RUN
S2 run (void *arg)
{
	S8 cpuc ALIGN = (S8) arg;
    S8 cpu_core ALIGN = cpuc; // POSIX THREAD CPU core ID, for the POSIX stuff!
	S8 i ALIGN;
	S8 offset ALIGN;
	U1 *bptr;

	S8 ep ALIGN;
	S8 arg1 ALIGN;
	S8 arg2 ALIGN;
	S8 arg3 ALIGN;
	S8 arg4 ALIGN;

	// EDIT NEU
	cpu[cpuc].status = RUNNING;
	cpu[cpuc].scheduler = SCHEDULER_OFF;  // run in single thread mode if new funcion call
	cpu[cpuc].ep = 16;
	cpu[cpuc].eoffs = 0;
	cpu[cpuc].startpos = 0;
	cpu[cpuc].overflow = 0;
	cpu[cpuc].data = data_global;
	cpu[cpuc].jumpstack_ind = -1;
	cpu[cpuc].local_data_ind = -1;
	cpu[cpuc].do_memory_bounds_check = 1;
	cpu[cpuc].ins_count = INSCOUNT_MAX;

	cpu[cpuc].jumpoffs = (S8 *) calloc (code_size, sizeof (S8));
	if (cpu[cpuc].jumpoffs == NULL)
	{
		printf ("ERROR: can't allocate %lli bytes for jumpoffsets!\n", code_size);
		loop_stop ();
		pthread_exit ((void *) 1);
	}

	/*
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

	U1 *data = data_global;

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

	// for local data interrupt
    S8 local_data_ind ALIGN = -1;

	U1 do_memory_bounds_check = 1;

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
    */

	cpu[cpuc].sp_top = threaddata[cpuc].sp_top_thread;
	cpu[cpuc].sp_bottom = threaddata[cpuc].sp_bottom_thread;
	cpu[cpuc].sp = threaddata[cpuc].sp_thread;

	threaddata[cpuc].local_data = (U1 **) calloc (local_data_max, sizeof (U1*));
	if (threaddata[cpuc].local_data == NULL)
	{
		printf ("ERROR: can't allocate %lli elements for local function data!\n", local_data_max);
		free_jumpoffs (cpuc);
		loop_stop ();
		pthread_exit ((void *) 1);
	}

	{
		S8 i ALIGN;
		for (i = 0; i < local_data_max; i++)
		{
			threaddata[cpuc].local_data[i] = NULL;
		}
	}

	if (silent_run == 0)
	{
		printf ("%lli stack size: %lli\n", cpuc, stack_size);
		printf ("%lli sp top: %lli\n", cpuc, (S8) cpu[cpuc].sp_top);
		printf ("%lli sp bottom: %lli\n", cpuc, (S8) cpu[cpuc].sp_bottom);
		printf ("%lli sp: %lli\n", cpuc, (S8) cpu[cpuc].sp);

		printf ("%lli sp caller top: %lli\n", cpuc, (S8) threaddata[cpuc].sp_top);
		printf ("%lli sp caller bottom: %lli\n", cpuc, (S8) threaddata[cpuc].sp_bottom);
	}

	cpu[cpuc].startpos = threaddata[cpuc].ep_startpos;
	if (threaddata[cpuc].sp != threaddata[cpuc].sp_top)
	{
		// something on mother thread stack, copy it

		cpu[cpuc].srcptr = threaddata[cpuc].sp_top;
		cpu[cpuc].dstptr = threaddata[cpuc].sp_top_thread;

		while (cpu[cpuc].srcptr >= threaddata[cpuc].sp)
		{
			// printf ("dstptr stack: %lli\n", (S8) dstptr);
			*cpu[cpuc].dstptr-- = *cpu[cpuc].srcptr--;
		}
	}

	cpu_ind = cpuc;

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

			cpu[cpuc].jumpoffs[i] = arg1;
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

			cpu[cpuc].jumpoffs[i] = arg1;
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

			cpu[cpuc].jumpoffs[i] = arg1;
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

			cpu[cpuc].jumpoffs[i] = arg1;
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
			free_jumpoffs (cpuc);
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
		cpu[cpuc].regi[i] = 0;
		cpu[cpuc].regd[i] = 0.0;
	}

	if (silent_run == 0)
	{
		printf ("CPU %lli ready\n", cpu_ind);
		show_code_data_size (code_size, data_mem_size);
		printf ("ep: %lli\n\n", cpu[cpuc].startpos);
		printf ("Donate at: https://ko-fi.com/P5P2Y3KP\nThank you!\n\n");
	}
#if DEBUG
	printf ("stack pointer sp: %lli\n", (S8) sp);
#endif

	// EDIT OPCODES
	cpu[cpuc].ep = cpu[cpuc].startpos; cpu[cpuc].eoffs = 0;
    ep = cpu[cpuc].ep;

    SCHEXE_NEXT();

	// arg2 = data offset
	pushb:
	#if DEBUG
	printf ("%lli PUSHB\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = cpu[cpuc].regi[code[ep + 1]];
	arg2 = cpu[cpuc].regi[code[ep + 2]];
	arg3 = code[ep + 3];

	#if BOUNDSCHECK
	if (cpu[cpuc].do_memory_bounds_check == 1)
	{
	if (memory_bounds (arg1, arg2) != 0)
	{
		PRINT_EPOS();
		free_jumpoffs (cpuc);
		loop_stop ();
        pthread_exit ((void *) 1);
	}
	}
	#endif

	cpu[cpuc].regi[arg3] = 0;		// set to zero, before loading data
	cpu[cpuc].regi[arg3] = (S8) cpu[cpuc].data[arg1 + arg2];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	pushw:
	#if DEBUG
	printf ("%lli PUSHW\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = cpu[cpuc].regi[code[ep + 1]];
	arg2 = cpu[cpuc].regi[code[ep + 2]];
	arg3 = code[ep + 3];

	#if BOUNDSCHECK
	if (cpu[cpuc].do_memory_bounds_check == 1)
	{
	if (memory_bounds (arg1, arg2) != 0)
	{
		PRINT_EPOS();
		free_jumpoffs (cpuc);
		loop_stop ();
        pthread_exit ((void *) 1);
	}
	}
	#endif

	cpu[cpuc].regi[arg3] = 0;		// set to zero, before loading data
	bptr = (U1 *) &cpu[cpuc].regi[arg3];

	*bptr = cpu[cpuc].data[arg1 + arg2];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 1];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	pushdw:
	#if DEBUG
	printf ("%lli PUSHDW\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = cpu[cpuc].regi[code[ep + 1]];
	arg2 = cpu[cpuc].regi[code[ep + 2]];
	arg3 = code[ep + 3];

	#if BOUNDSCHECK
	if (cpu[cpuc].do_memory_bounds_check == 1)
	{
	if (memory_bounds (arg1, arg2) != 0)
	{
		PRINT_EPOS();
		free_jumpoffs (cpuc);
		loop_stop ();
        pthread_exit ((void *) 1);
	}
	}
	#endif

	cpu[cpuc].regi[arg3] = 0;		// set to zero, before loading data
	bptr = (U1 *) &cpu[cpuc].regi[arg3];

	*bptr = cpu[cpuc].data[arg1 + arg2];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 1];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 2];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 3];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	pushqw:
	#if DEBUG
	printf ("%lli PUSHQW\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = cpu[cpuc].regi[code[ep + 1]];
	arg2 = cpu[cpuc].regi[code[ep + 2]];
	arg3 = code[ep + 3];

	#if BOUNDSCHECK
	if (cpu[cpuc].do_memory_bounds_check == 1)
	{
	if (memory_bounds (arg1, arg2) != 0)
	{
		PRINT_EPOS();
		free_jumpoffs (cpuc);
		loop_stop ();
        pthread_exit ((void *) 1);
	}
	}
	#endif

	bptr = (U1 *) &cpu[cpuc].regi[arg3];

	*bptr = cpu[cpuc].data[arg1 + arg2];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 1];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 2];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 3];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 4];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 5];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 6];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 7];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	pushd:
	#if DEBUG
	printf ("%lli PUSHD\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = cpu[cpuc].regi[code[ep + 1]];
	arg2 = cpu[cpuc].regi[code[ep + 2]];
	arg3 = code[ep + 3];

	#if BOUNDSCHECK
	if (cpu[cpuc].do_memory_bounds_check == 1)
	{
	if (memory_bounds (arg1, arg2) != 0)
	{
		PRINT_EPOS();
		free_jumpoffs (cpuc);
		loop_stop ();
        pthread_exit ((void *) 1);
	}
	}
	#endif

	bptr = (U1 *) &cpu[cpuc].regd[arg3];

	*bptr = cpu[cpuc].data[arg1 + arg2];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 1];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 2];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 3];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 4];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 5];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 6];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 7];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();


	pullb:
	#if DEBUG
	printf ("%lli PULLB\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = cpu[cpuc].regi[code[ep + 2]];
	arg3 = cpu[cpuc].regi[code[ep + 3]];

	#if BOUNDSCHECK
	if (cpu[cpuc].do_memory_bounds_check == 1)
	{
	if (memory_bounds (arg2, arg3) != 0)
	{
		PRINT_EPOS();
		free_jumpoffs (cpuc);
		loop_stop ();
        pthread_exit ((void *) 1);
	}
	}
	#endif

	cpu[cpuc].data[arg2 + arg3] = cpu[cpuc].regi[arg1];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	pullw:
	#if DEBUG
	printf ("%lli PULLW\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = cpu[cpuc].regi[code[ep + 2]];
	arg3 = cpu[cpuc].regi[code[ep + 3]];

	#if BOUNDSCHECK
	if (cpu[cpuc].do_memory_bounds_check == 1)
	{
	if (memory_bounds (arg2, arg3) != 0)
	{
		PRINT_EPOS();
		free_jumpoffs (cpuc);
		loop_stop ();
        pthread_exit ((void *) 1);
	}
	}
	#endif

	bptr = (U1 *) &cpu[cpuc].regi[arg1];

	cpu[cpuc].data[arg2 + arg3] = *bptr;
	bptr++;
	cpu[cpuc].data[arg2 + arg3 + 1] = *bptr;

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	pulldw:
	#if DEBUG
	printf ("%lli PULLDW\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = cpu[cpuc].regi[code[ep + 2]];
	arg3 = cpu[cpuc].regi[code[ep + 3]];

	#if BOUNDSCHECK
	if (cpu[cpuc].do_memory_bounds_check == 1)
	{
	if (memory_bounds (arg2, arg3) != 0)
	{
		PRINT_EPOS();
		free_jumpoffs (cpuc);
		loop_stop ();
        pthread_exit ((void *) 1);
	}
	}
	#endif

	bptr = (U1 *) &cpu[cpuc].regi[arg1];

	cpu[cpuc].data[arg2 + arg3] = *bptr;
	bptr++;
	cpu[cpuc].data[arg2 + arg3 + 1] = *bptr;
	bptr++;
	cpu[cpuc].data[arg2 + arg3 + 2] = *bptr;
	bptr++;
	cpu[cpuc].data[arg2 + arg3 + 3] = *bptr;

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	pullqw:
	#if DEBUG
	printf ("%lli PULLQW\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = cpu[cpuc].regi[code[ep + 2]];
	arg3 = cpu[cpuc].regi[code[ep + 3]];

	#if BOUNDSCHECK
	if (cpu[cpuc].do_memory_bounds_check == 1)
	{
	if (memory_bounds (arg2, arg3) != 0)
	{
		PRINT_EPOS();
		free_jumpoffs (cpuc);
		loop_stop ();
        pthread_exit ((void *) 1);
	}
	}
	#endif

	bptr = (U1 *) &cpu[cpuc].regi[arg1];

	cpu[cpuc].data[arg2 + arg3] = *bptr;
	bptr++;
	cpu[cpuc].data[arg2 + arg3 + 1] = *bptr;
	bptr++;
	cpu[cpuc].data[arg2 + arg3 + 2] = *bptr;
	bptr++;
	cpu[cpuc].data[arg2 + arg3 + 3] = *bptr;
	bptr++;
	cpu[cpuc].data[arg2 + arg3 + 4] = *bptr;
	bptr++;
	cpu[cpuc].data[arg2 + arg3 + 5] = *bptr;
	bptr++;
	cpu[cpuc].data[arg2 + arg3 + 6] = *bptr;
	bptr++;
	cpu[cpuc].data[arg2 + arg3 + 7] = *bptr;

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	pulld:
	#if DEBUG
	printf ("%lli PULLD\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = cpu[cpuc].regi[code[ep + 2]];
	arg3 = cpu[cpuc].regi[code[ep + 3]];

	#if BOUNDSCHECK
	if (cpu[cpuc].do_memory_bounds_check == 1)
	{
	if (memory_bounds (arg2, arg3) != 0)
	{
		PRINT_EPOS();
		free_jumpoffs (cpuc);
		loop_stop ();
        pthread_exit ((void *) 1);
	}
	}
	#endif

	bptr = (U1 *) &cpu[cpuc].regd[arg1];

	cpu[cpuc].data[arg2 + arg3] = *bptr;
	bptr++;
	cpu[cpuc].data[arg2 + arg3 + 1] = *bptr;
	bptr++;
	cpu[cpuc].data[arg2 + arg3 + 2] = *bptr;
	bptr++;
	cpu[cpuc].data[arg2 + arg3 + 3] = *bptr;
	bptr++;
	cpu[cpuc].data[arg2 + arg3 + 4] = *bptr;
	bptr++;
	cpu[cpuc].data[arg2 + arg3 + 5] = *bptr;
	bptr++;
	cpu[cpuc].data[arg2 + arg3 + 6] = *bptr;
	bptr++;
	cpu[cpuc].data[arg2 + arg3 + 7] = *bptr;

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

// EDIT ADDI
	addi:
	#if DEBUG
	printf ("%lli ADDI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	#if MATH_LIMITS_INT
		if (__builtin_saddll_overflow (cpu[cpuc].regi[arg1], cpu[cpuc].regi[arg2], &cpu[cpuc].regi[arg3]))
		{
			cpu[cpuc].overflow = 1;
 			printf ("ERROR: overflow at addi!\n");
			PRINT_EPOS();
		}
		else
		{
			 cpu[cpuc].overflow = 0;
		}
	#else
		cpu[cpuc].regi[arg3] = cpu[cpuc].regi[arg1] + cpu[cpuc].regi[arg2];
	#endif

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	subi:
	#if DEBUG
	printf ("%lli SUBI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	#if MATH_LIMITS_INT
		if (__builtin_ssubll_overflow (cpu[cpuc].regi[arg1], cpu[cpuc].regi[arg2], &cpu[cpuc].regi[arg3]))
		{
			cpu[cpuc].overflow = 1;
 			printf ("ERROR: overflow at subi!\n");
			PRINT_EPOS();
		}
		else
		{
			 cpu[cpuc].overflow = 0;
		}
	#else
		regi[arg3] = regi[arg1] - regi[arg2];
	#endif

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	muli:
	#if DEBUG
	printf ("%lli MULI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	#if MATH_LIMITS_INT
		if (__builtin_smulll_overflow (cpu[cpuc].regi[arg1], cpu[cpuc].regi[arg2], &cpu[cpuc].regi[arg3]))
		{
			cpu[cpuc].overflow = 1;
 			printf ("ERROR: overflow at muli!\n");
			PRINT_EPOS();
		}
		else
		{
			 cpu[cpuc].overflow = 0;
		}
	#else
		regi[arg3] = regi[arg1] * regi[arg2];
	#endif

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	divi:
	#if DEBUG
	printf ("%lli DIVI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

    #if DIVISIONCHECK
    if (iszero (cpu[cpuc].regi[arg2]))
    {
        printf ("FATAL ERROR: division by zero!\n");
		PRINT_EPOS();
		free_jumpoffs (cpuc);
		loop_stop ();
		pthread_exit ((void *) 1);
    }
    #endif

	cpu[cpuc].regi[arg3] = cpu[cpuc].regi[arg1] / cpu[cpuc].regi[arg2];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	addd:
	#if DEBUG
	printf ("%lli ADDD\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	#if MATH_LIMITS
		cpu[cpuc].overflow = 0;
	#endif
	#if MATH_LIMITS_DOUBLE_FULL
		if (double_state (cpu[cpuc].regd[arg1]) == 1)
		{
			cpu[cpuc].overflow = 1;
			printf ("ERROR: overflow at addd!\n");
			PRINT_EPOS();
		}

		if (double_state (cpu[cpuc].regd[arg2]) == 1)
		{
			cpu[cpuc].overflow = 1;
			printf ("ERROR: overflow at addd!\n");
			PRINT_EPOS();
		}
	#endif

	cpu[cpuc].regd[arg3] = cpu[cpuc].regd[arg1] + cpu[cpuc].regd[arg2];

	#if MATH_LIMITS
		if (double_state (cpu[cpuc].regd[arg3]) == 1)
		{
			cpu[cpuc].overflow = 1;
			printf ("ERROR: overflow at addd!\n");
			PRINT_EPOS();
		}
	#endif

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	subd:
	#if DEBUG
	printf ("%lli SUBD\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	#if MATH_LIMITS
		cpu[cpuc].overflow = 0;
	#endif
	#if MATH_LIMITS_DOUBLE_FULL
		if (double_state (cpu[cpuc].regd[arg1]) == 1)
		{
			cpu[cpuc].overflow = 1;
			printf ("ERROR: overflow at subd!\n");
			PRINT_EPOS();
		}

		if (double_state (cpu[cpuc].regd[arg2]) == 1)
		{
			cpu[cpuc].overflow = 1;
			printf ("ERROR: overflow at subd!\n");
			PRINT_EPOS();
		}
	#endif

	cpu[cpuc].regd[arg3] = cpu[cpuc].regd[arg1] - cpu[cpuc].regd[arg2];

	#if MATH_LIMITS
		if (double_state (cpu[cpuc].regd[arg3]) == 1)
		{
			cpu[cpuc].overflow = 1;
			printf ("ERROR: overflow at subd!\n");
			PRINT_EPOS();
		}
	#endif

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	muld:
	#if DEBUG
	printf ("%lli MULD\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	#if MATH_LIMITS
		cpu[cpuc].overflow = 0;
	#endif
	#if MATH_LIMITS_DOUBLE_FULL
		if (double_state (cpu[cpuc].regd[arg1]) == 1)
		{
			cpu[cpuc].overflow = 1;
			printf ("ERROR: overflow at muld!\n");
			PRINT_EPOS();
		}

		if (double_state (cpu[cpuc].regd[arg2]) == 1)
		{
			cpu[cpuc].overflow = 1;
			printf ("ERROR: overflow at muld!\n");
			PRINT_EPOS();
		}
	#endif

	cpu[cpuc].regd[arg3] = cpu[cpuc].regd[arg1] * cpu[cpuc].regd[arg2];

	#if MATH_LIMITS
		if (double_state (cpu[cpuc].regd[arg3]) == 1)
		{
			cpu[cpuc].overflow = 1;
			printf ("ERROR: overflow at muld!\n");
			PRINT_EPOS();
		}
	#endif

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	divd:
	#if DEBUG
	printf ("%lli DIVD\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

    #if DIVISIONCHECK
    if (iszero (cpu[cpuc].regd[arg2]))
    {
        printf ("FATAL ERROR: division by zero!\n");
		PRINT_EPOS();
		free_jumpoffs (cpuc);
		loop_stop ();
		pthread_exit ((void *) 1);
    }
    #endif

	#if MATH_LIMITS
		cpu[cpuc].overflow = 0;
	#endif
	#if MATH_LIMITS_DOUBLE_FULL
		if (double_state (cpu[cpuc].regd[arg1]) == 1)
		{
			cpu[cpuc].overflow = 1;
			printf ("ERROR: overflow at divd!\n");
			PRINT_EPOS();
		}

		if (double_state (cpu[cpuc].regd[arg2]) == 1)
		{
			cpu[cpuc].overflow = 1;
			printf ("ERROR: overflow at divd!\n");
			PRINT_EPOS();
		}
	#endif

	cpu[cpuc].regd[arg3] = cpu[cpuc].regd[arg1] / cpu[cpuc].regd[arg2];

	#if MATH_LIMITS
	if (double_state (cpu[cpuc].regd[arg3]) == 1)
		{
			cpu[cpuc].overflow = 1;
			printf ("ERROR: overflow at divd!\n");
			PRINT_EPOS();
		}
	#endif

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	smuli:
	#if DEBUG
	printf ("%lli SMULI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	cpu[cpuc].regi[arg3] = cpu[cpuc].regi[arg1] << cpu[cpuc].regi[arg2];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	sdivi:
	#if DEBUG
	printf ("%lli SDIVI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	cpu[cpuc].regi[arg3] = cpu[cpuc].regi[arg1] >> cpu[cpuc].regi[arg2];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	andi:
	#if DEBUG
	printf ("%lli ANDI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	cpu[cpuc].regi[arg3] = cpu[cpuc].regi[arg1] && cpu[cpuc].regi[arg2];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	ori:
	#if DEBUG
	printf ("%lli ORI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	cpu[cpuc].regi[arg3] = cpu[cpuc].regi[arg1] || cpu[cpuc].regi[arg2];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();


	bandi:
	#if DEBUG
	printf ("%lli BANDI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	cpu[cpuc].regi[arg3] = cpu[cpuc].regi[arg1] & cpu[cpuc].regi[arg2];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	bori:
	#if DEBUG
	printf ("%lli BORI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	cpu[cpuc].regi[arg3] = cpu[cpuc].regi[arg1] | cpu[cpuc].regi[arg2];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	bxori:
	#if DEBUG
	printf ("%lli BXORI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	cpu[cpuc].regi[arg3] = cpu[cpuc].regi[arg1] ^ cpu[cpuc].regi[arg2];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	modi:
	#if DEBUG
	printf ("%lli MODI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	cpu[cpuc].regi[arg3] = cpu[cpuc].regi[arg1] % cpu[cpuc].regi[arg2];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();


	eqi:
	#if DEBUG
	printf ("%lli EQI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	cpu[cpuc].regi[arg3] = cpu[cpuc].regi[arg1] == cpu[cpuc].regi[arg2];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	neqi:
	#if DEBUG
	printf ("%lli NEQI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	cpu[cpuc].regi[arg3] = cpu[cpuc].regi[arg1] != cpu[cpuc].regi[arg2];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	gri:
	#if DEBUG
	printf ("%lli GRI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	cpu[cpuc].regi[arg3] = cpu[cpuc].regi[arg1] > cpu[cpuc].regi[arg2];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	lsi:
	#if DEBUG
	printf ("%lli LSI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	cpu[cpuc].regi[arg3] = cpu[cpuc].regi[arg1] < cpu[cpuc].regi[arg2];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	greqi:
	#if DEBUG
	printf ("%lli GREQI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	cpu[cpuc].regi[arg3] = cpu[cpuc].regi[arg1] >= cpu[cpuc].regi[arg2];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	lseqi:
	#if DEBUG
	printf ("%lli LSEQI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	cpu[cpuc].regi[arg3] = cpu[cpuc].regi[arg1] <= cpu[cpuc].regi[arg2];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();


	eqd:
	#if DEBUG
	printf ("%lli EQD\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	cpu[cpuc].regi[arg3] = cpu[cpuc].regd[arg1] == cpu[cpuc].regd[arg2];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	neqd:
	#if DEBUG
	printf ("%lli NEQD\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	cpu[cpuc].regi[arg3] = cpu[cpuc].regd[arg1] != cpu[cpuc].regd[arg2];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	grd:
	#if DEBUG
	printf ("%lli GRD\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	cpu[cpuc].regi[arg3] = cpu[cpuc].regd[arg1] > cpu[cpuc].regd[arg2];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	lsd:
	#if DEBUG
	printf ("%lli LSD\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	cpu[cpuc].regi[arg3] = cpu[cpuc].regd[arg1] < cpu[cpuc].regd[arg2];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	greqd:
	#if DEBUG
	printf ("%lli GREQD\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	cpu[cpuc].regi[arg3] = cpu[cpuc].regd[arg1] >= cpu[cpuc].regd[arg2];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();

	lseqd:
	#if DEBUG
	printf ("%lli LSEQD\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];
	arg3 = code[ep + 3];

	cpu[cpuc].regi[arg3] = cpu[cpuc].regd[arg1] <= cpu[cpuc].regd[arg2];

	cpu[cpuc].eoffs = 4;
	SCHEXE_NEXT();


	jmp:
	#if DEBUG
	printf ("%lli JMP\n", cpu_core);
	#endif
	cpu[cpuc].ep = cpu[cpuc].jumpoffs[ep];
	cpu[cpuc].eoffs = 0;

	SCHEXE_NEXT();

	jmpi:
	#if DEBUG
	printf ("%lli JMPI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];

	arg2 = cpu[cpuc].jumpoffs[ep];

	if (cpu[cpuc].regi[arg1] != 0)
	{
		cpu[cpuc].eoffs = 0;
		cpu[cpuc].ep = arg2;
		#if DEBUG
		printf ("%lli JUMP TO %lli\n", cpu_core, cpu[cpuc].ep);
		#endif
		SCHEXE_NEXT();
	}

	cpu[cpuc].eoffs = 10;

	SCHEXE_NEXT();


	stpushb:
	#if DEBUG
	printf("%lli STPUSHB\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];

	cpu[cpuc].sp = stpushb (cpu[cpuc].regi[arg1], cpu[cpuc].sp, cpu[cpuc].sp_bottom);
	if (cpu[cpuc].sp == NULL)
	{
		PRINT_EPOS();
		printf ("stpushb: error can't push!\n");
		free_jumpoffs (cpuc);
		loop_stop ();
		pthread_exit ((void *) 1);
	}
	cpu[cpuc].eoffs = 2;

	SCHEXE_NEXT();

	stpopb:
	#if DEBUG
	printf("%lli STPOPB\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];

	cpu[cpuc].sp = stpopb ((U1 *) &cpu[cpuc].regi[arg1], cpu[cpuc].sp, cpu[cpuc].sp_top);
	if (cpu[cpuc].sp == NULL)
	{
		PRINT_EPOS();
		printf ("stpopb: error can't pop!\n");
		free_jumpoffs (cpuc);
		loop_stop ();
		pthread_exit ((void *) 1);
	}
	cpu[cpuc].eoffs = 2;

	SCHEXE_NEXT();

	stpushi:
	#if DEBUG
	printf("%lli STPUSHI\n", cpu_core);
	#endif
    ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];

	cpu[cpuc].sp = stpushi (cpu[cpuc].regi[arg1], cpu[cpuc].sp, cpu[cpuc].sp_bottom);
	if (cpu[cpuc].sp == NULL)
	{
		PRINT_EPOS();
		printf ("stpushi: error can't push!\n");
		free_jumpoffs (cpuc);
		loop_stop ();
		pthread_exit ((void *) 1);
	}
	cpu[cpuc].eoffs = 2;

	SCHEXE_NEXT();

	stpopi:
	#if DEBUG
	printf("%lli STPOPI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];

	cpu[cpuc].sp = stpopi ((U1 *) &cpu[cpuc].regi[arg1], cpu[cpuc].sp, cpu[cpuc].sp_top);
	if (cpu[cpuc].sp == NULL)
	{
		PRINT_EPOS();
		printf ("stpopi: error can't pop!\n");
		free_jumpoffs (cpuc);
		loop_stop ();
		pthread_exit ((void *) 1);
	}
	cpu[cpuc].eoffs = 2;

	SCHEXE_NEXT();

	stpushd:
	#if DEBUG
	printf("%lli STPUSHD\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];

	cpu[cpuc].sp = stpushd (cpu[cpuc].regd[arg1], cpu[cpuc].sp, cpu[cpuc].sp_bottom);
	if (cpu[cpuc].sp == NULL)
	{
		PRINT_EPOS();
		printf ("stpushd: error can't push!\n");
		free_jumpoffs (cpuc);
		loop_stop ();
		pthread_exit ((void *) 1);
	}
	cpu[cpuc].eoffs = 2;

	SCHEXE_NEXT();

	stpopd:
	#if DEBUG
	printf("%lli STPOPD\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];

	cpu[cpuc].sp = stpopd ((U1 *) &cpu[cpuc].regd[arg1], cpu[cpuc].sp, cpu[cpuc].sp_top);
	if (cpu[cpuc].sp == NULL)
	{
		PRINT_EPOS();
		printf ("stpopd: error can't pop!\n");
		free_jumpoffs (cpuc);
		loop_stop ();
		pthread_exit ((void *) 1);
	}
	cpu[cpuc].eoffs = 2;

	SCHEXE_NEXT();

	loada:
	#if DEBUG
	printf ("%lli LOADA\n", cpu_core);
	#endif
	// data
    ep = cpu[cpuc].ep;
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
	if (cpu[cpuc].do_memory_bounds_check == 1)
	{
	if (memory_bounds (arg1, arg2) != 0)
	{
		PRINT_EPOS();
		free_jumpoffs (cpuc);
		loop_stop ();
		pthread_exit ((void *) 1);
	}
	}
	#endif

	arg3 = code[ep + 17];

	bptr = (U1 *) &cpu[cpuc].regi[arg3];

	*bptr = cpu[cpuc].data[arg1 + arg2];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 1];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 2];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 3];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 4];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 5];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 6];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 7];

	cpu[cpuc].eoffs = 18;
	SCHEXE_NEXT();

	loadd:
	#if DEBUG
	printf ("%lli LOADD\n", cpu_core);
	#endif
	// data
	ep = cpu[cpuc].ep;
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
	if (cpu[cpuc].do_memory_bounds_check == 1)
	{
	if (memory_bounds (arg1, arg2) != 0)
	{
		PRINT_EPOS();
		free_jumpoffs (cpuc);
		loop_stop ();
		pthread_exit ((void *) 1);
	}
	}
	#endif

	arg3 = code[ep + 17];

	bptr = (U1 *) &cpu[cpuc].regd[arg3];

	*bptr = cpu[cpuc].data[arg1 + arg2];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 1];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 2];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 3];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 4];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 5];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 6];
	bptr++;
	*bptr = cpu[cpuc].data[arg1 + arg2 + 7];

	cpu[cpuc].eoffs = 18;
	SCHEXE_NEXT();

	intr0:
    ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	#if DEBUG
	printf ("%lli INTR0: %lli\n", cpu_core, arg1);
	#endif
	switch (arg1)
	{
		case 0:
			//printf ("LOADMODULE\n");
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			arg3 = code[ep + 3];

			if (load_module ((U1 *) &cpu[cpuc].data[cpu[cpuc].regi[arg2]], cpu[cpuc].regi[arg3]) != 0)
			{
				printf ("EXIT!\n");
				free_jumpoffs (cpuc);
				loop_stop ();
				pthread_exit ((void *) 1);
			}
			cpu[cpuc].eoffs = 5;
			break;

		case 1:
			//printf ("FREEMODULE\n");
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];

			free_module (cpu[cpuc].regi[arg2]);
			cpu[cpuc].eoffs = 5;
			break;

		case 2:
			//printf ("SETMODULEFUNC\n");
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			arg3 = code[ep + 3];
			arg4 = code[ep + 4];

			if (set_module_func (cpu[cpuc].regi[arg2], cpu[cpuc].regi[arg3], (U1 *) &cpu[cpuc].data[cpu[cpuc].regi[arg4]]) != 0)
			{
				printf ("EXIT!\n");
				free_jumpoffs (cpuc);
				loop_stop ();
				pthread_exit ((void *) 1);
			}
			cpu[cpuc].eoffs = 5;
			break;

		case 3:
			//printf ("CALLMODULEFUNC\n");
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			arg3 = code[ep + 3];

			cpu[cpuc].sp = call_module_func (cpu[cpuc].regi[arg2], cpu[cpuc].regi[arg3], (U1 *) cpu[cpuc].sp, cpu[cpuc].sp_top, cpu[cpuc].sp_bottom, (U1 *) cpu[cpuc].data);
			cpu[cpuc].eoffs = 5;
			if (cpu[cpuc].sp == NULL)
			{
				// ERROR -> EXIT
				// EDIT THREAD
				retcode = 1;
				free_jumpoffs (cpuc);
				pthread_mutex_lock (&data_mutex);
				threaddata[cpu_core].status = STOP;
				pthread_mutex_unlock (&data_mutex);
				loop_stop ();
				pthread_exit ((void *) retcode);
			}
			break;

		case 4:
			//printf ("PRINTI\n");
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			printf ("%lli", cpu[cpuc].regi[arg2]);
			cpu[cpuc].eoffs = 5;
			break;

		case 5:
			//printf ("PRINTD\n");
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			printf ("%.10lf", cpu[cpuc].regd[arg2]);
			cpu[cpuc].eoffs = 5;
			break;

		case 6:
			//printf ("PRINTSTR\n");
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			printf ("%s", (char *) &cpu[cpuc].data[cpu[cpuc].regi[arg2]]);
			cpu[cpuc].eoffs = 5;
			break;

		case 7:
			//printf ("PRINTNEWLINE\n");
			ep = cpu[cpuc].ep;

			printf ("\n");
			cpu[cpuc].eoffs = 5;
			break;

		case 8:
			ep = cpu[cpuc].ep;

			if (silent_run == 0)
			{
				printf ("DELAY\n");
			}
			arg2 = code[ep + 2];
			usleep (cpu[cpuc].regi[arg2] * 1000);
			#if DEBUG
				printf ("delay: %lli\n", cpu[cpuc].regi[arg2]);
			#endif
			cpu[cpuc].eoffs = 5;
			break;

		case 9:
			//printf ("INPUTI\n");
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			cpu[cpuc].input_str[0] = '\0';
			if (fgets ((char *) cpu[cpuc].input_str, MAXINPUT - 1, stdin) != NULL)
			{
				cpu[cpuc].regi[arg2] = 0;
				sscanf ((const char *) cpu[cpuc].input_str, "%lli", &cpu[cpuc].regi[arg2]);
			}
			else
			{
				printf ("input integer: can't read!\n");
				PRINT_EPOS();
			}
			cpu[cpuc].eoffs = 5;
			break;

		case 10:
			//printf ("INPUTD\n");
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			cpu[cpuc].input_str[0] = '\0';
			if (fgets ((char *) cpu[cpuc].input_str, MAXINPUT - 1, stdin) != NULL)
			{
				cpu[cpuc].regd[arg2] = 0.0;
				sscanf ((const char *) cpu[cpuc].input_str, "%lf", &cpu[cpuc].regd[arg2]);
			}
			else
			{
				printf ("input double: can't read!\n");
				PRINT_EPOS();
			}
			cpu[cpuc].eoffs = 5;
			break;

		case 11:
			//printf ("INPUTS\n");
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			arg3 = code[ep + 3];

			{
				U1 ch;
				S8 i = 0;

				while (1)
				{
					if (i < cpu[cpuc].regi[arg2] - 1)
					{
						ch = getchar ();
						if (memory_bounds (cpu[cpuc].regi[arg3], i) != 0)
						{
							// ERROR string variable overflow!
							printf ("ERROR: input: string variable overflow!\n");
							PRINT_EPOS();
							free_jumpoffs (cpuc);
							loop_stop ();
							pthread_exit ((void *) 1);
						}
						cpu[cpuc].data[cpu[cpuc].regi[arg3] + i] = ch;
						if (ch == 10)
						{
							if (i == 0)
							{
								cpu[cpuc].data[cpu[cpuc].regi[arg3]] = '\0';
								break;
							}
							else
							{
								cpu[cpuc].data[cpu[cpuc ].regi[arg3] + i] = '\0';
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
			cpu[cpuc].eoffs = 5;
			break;

		case 12:
			//printf ("SHELLARGSNUM\n");
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			cpu[cpuc].regi[arg2] = shell_args_ind + 1;
			cpu[cpuc].eoffs = 5;
			break;

		case 13:
			//printf ("GETSHELLARG\n");
			{
			ep = cpu[cpuc].ep;

			S8 shell_arg_len ALIGN = 0;
			arg2 = code[ep + 2];
			arg3 = code[ep + 3];
			if (cpu[cpuc].regi[arg2] > shell_args_ind)
			{
				printf ("ERROR: shell argument index out of range!\n");
				PRINT_EPOS();
				free_jumpoffs (cpuc);
				loop_stop ();
				pthread_exit ((void *) 1);
			}

			// check target string size
			shell_arg_len = strlen_safe ((const char *) shell_args[cpu[cpuc].regi[arg2]], MAXLINELEN) + 1;

			if (memory_bounds (cpu[cpuc].regi[arg3], shell_arg_len) != 0)
			{
				printf ("ERROR: string overflow: shell argument %lli can't be saved in variable!\n", cpu[cpuc].regi[arg2]);
				PRINT_EPOS();
				free_jumpoffs (cpuc);
				loop_stop ();
        		pthread_exit ((void *) 1);
			}

			snprintf ((char *) &cpu[cpuc].data[cpu[cpuc].regi[arg3]], shell_arg_len, "%s", (const char *) shell_args[cpu[cpuc].regi[arg2]]);
			cpu[cpuc].eoffs = 5;
			}
			break;

		case 14:
			//printf("SHOWSTACKPOINTER\n");
			ep = cpu[cpuc].ep;

			printf ("stack pointer sp: %lli\n", (S8) cpu[cpuc].sp);
			if (cpu[cpuc].sp == cpu[cpuc].sp_top)
			{
				printf ("stack is empty!\n\n");
			}
			else
			{
				printf ("stack has data!\n\n");
			}
			cpu[cpuc].eoffs = 5;
			break;

        case 15:
            // return number of CPU cores available
            ep = cpu[cpuc].ep;

            arg2 = code[ep + 2];
            cpu[cpuc].regi[arg2] = max_cpu;
            cpu[cpuc].eoffs = 5;
            break;

        case 16:
            // return endianess of host machine
            ep = cpu[cpuc].ep;

            arg2 = code[ep + 2];
#if MACHINE_BIG_ENDIAN
            cpu[cpuc].regi[arg2] = 1;
#else
            cpu[cpuc].regi[arg2] = 0;
#endif
            cpu[cpuc].eoffs = 5;
            break;

		case 17:
			// return current time
			arg2 = code[ep + 2];
			arg3 = code[ep + 3];
			arg4 = code[ep + 4];

			// EDIT TIME
			time (&cpu[cpuc].secs);
			tm = localtime (&cpu[cpuc].secs);
			cpu[cpuc].regi[arg2] = tm->tm_hour;
			cpu[cpuc].regi[arg3] = tm->tm_min;
			cpu[cpuc].regi[arg4] = tm->tm_sec;
			cpu[cpuc].eoffs = 5;
			break;

		case 18:
			// return date
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			arg3 = code[ep + 3];
			arg4 = code[ep + 4];

			time (&cpu[cpuc].secs);
			tm = localtime (&cpu[cpuc].secs);
			cpu[cpuc].regi[arg2] = tm->tm_year + 1900;
			cpu[cpuc].regi[arg3] = tm->tm_mon + 1;
			cpu[cpuc].regi[arg4] = tm->tm_mday;
			cpu[cpuc].eoffs = 5;
			break;

		case 19:
			// return weekday since Sunday
			// Sunday = 0
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];

			time (&cpu[cpuc].secs);
			tm = localtime (&cpu[cpuc].secs);
			cpu[cpuc].regi[arg2] = tm->tm_wday;
			cpu[cpuc].eoffs = 5;
			break;

		case 20:
			//printf ("PRINTI format\n");
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			arg3 = code[ep + 3];

			printf ((char *) &cpu[cpuc].data[cpu[cpuc].regi[arg3]], cpu[cpuc].regi[arg2]);
			cpu[cpuc].eoffs = 5;
			break;

		case 21:
			//printf ("PRINTD format\n");
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			arg3 = code[ep + 3];

			printf ((char *) &cpu[cpuc].data[cpu[cpuc].regi[arg3]], cpu[cpuc].regd[arg2]);
			cpu[cpuc].eoffs = 5;
			break;

		case 22:
			//printf ("PRINTI as int16\n");
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];

			printf ("%d", (S2) cpu[cpuc].regi[arg2]);
			cpu[cpuc].eoffs = 5;
			break;

		case 23:
			//printf ("PRINTI as int32\n");
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];

			printf ("%i", (S4) cpu[cpuc].regi[arg2]);
			cpu[cpuc].eoffs = 5;
			break;

#if TIMER_USE
		case 24:
			ep = cpu[cpuc].ep;

			gettimeofday (&timer_start, NULL);
			cpu[cpuc].eoffs = 5;
			break;

		case 25:
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			gettimeofday (&timer_end, NULL);

			timer_double = (double) (timer_end.tv_usec - timer_start.tv_usec) / 1000000 + (double) (timer_end.tv_sec - timer_start.tv_sec);
			timer_double = timer_double * 1000.0; 	// get ms
			printf ("TIMER ms: %.10lf\n", timer_double);
			timer_int = ceil (timer_double);
			cpu[cpuc].regi[arg2] = timer_int;
			cpu[cpuc].eoffs = 5;
			break;
#else
		case 24:
			ep = cpu[cpuc].ep;

			printf ("FATAL ERROR: no start timer!\n");
			PRINT_EPOS();
			free_jumpoffs (cpuc);
			loop_stop ();
			pthread_exit ((void *) 1);
			break;

		case 25:
			ep = cpu[cpuc].ep;

			printf ("FATAL ERROR: no end timer!\n");
			PRINT_EPOS();
			free_jumpoffs (cpuc);
			loop_stop ();
			pthread_exit ((void *) 1);
			break;
#endif

		case 26:
			// check if stack is empty, if not then give an error message and exit!!!
			ep = cpu[cpuc].ep;

			if (cpu[cpuc].sp != cpu[cpuc].sp_top)
			{
				printf ("ERROR: stack has data! Stack should be empty!\n");
				PRINT_EPOS();
				free_jumpoffs (cpuc);
				loop_stop ();
				pthread_exit ((void *) 1);
			}
			cpu[cpuc].eoffs = 5;
			break;

		case 27:
			// print out debug note
			ep = cpu[cpuc].ep;

			printf ("INTR0: 27 DEBUG marking\n")
			PRINT_EPOS();
			cpu[cpuc].eoffs = 5;
			break;

		case 28:
			// check if int variable value is in legal range
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			arg3 = code[ep + 3];	// min range
			arg4 = code[ep + 4];	// max range

			if (cpu[cpuc].regi[arg2] < cpu[cpuc].regi[arg3] || cpu[cpuc].regi[arg2] > cpu[cpuc].regi[arg4])
			{
				printf ("ERROR: int variable value in illegal range!\nvar: %lli : min: %lli, max: %lli\n\n", cpu[cpuc].regi[arg2], cpu[cpuc].regi[arg3], cpu[cpuc].regi[arg4]);
				PRINT_EPOS();
				free_jumpoffs (cpuc);
				loop_stop ();
				pthread_exit ((void *) 1);
			}
			cpu[cpuc].eoffs = 5;
			break;

		case 29:
			// check if double variable value is in legal range
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			arg3 = code[ep + 3];	// min range
			arg4 = code[ep + 4];	// max range

			if (cpu[cpuc].regd[arg2] < cpu[cpuc].regd[arg3] || cpu[cpuc].regd[arg2] > cpu[cpuc].regd[arg4])
			{
				printf ("ERROR: double variable value in illegal range!\nvar: %.10lf : min: %.10lf, max: %.10lf\n\n", cpu[cpuc].regd[arg2], cpu[cpuc].regd[arg3], cpu[cpuc].regd[arg4]);
				PRINT_EPOS();
				free_jumpoffs (cpuc);
				loop_stop ();
				pthread_exit ((void *) 1);
			}
	    	cpu[cpuc].eoffs = 5;
			break;

		case 30:
			// check if pointer variable is of check type
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];    // pointer var
			arg3 = code[ep + 3];	// pointer type var

			if (pointer_check (cpu[cpuc].regi[arg2], cpu[cpuc].regi[arg3]) != 0)
			{
				PRINT_EPOS();
				free_jumpoffs (cpuc);
				loop_stop ();
				pthread_exit ((void *) 1);
			}
			cpu[cpuc].eoffs = 5;
			break;

		case 31:
			// get pointer variable type
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];    // pointer var
			arg3 = code[ep + 3];	// pointer type var return

			cpu[cpuc].regi[arg3] = pointer_type (cpu[cpuc].regi[arg2]);
			cpu[cpuc].eoffs = 5;
			break;

        case 32:
			// get type of stack object
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];

			cpu[cpuc].sp = stack_type ((U1 *) &cpu[cpuc].regi[arg2], cpu[cpuc].sp, cpu[cpuc].sp_top);
			if (cpu[cpuc].sp == NULL)
			{
				PRINT_EPOS();
				free_jumpoffs (cpuc);
				loop_stop ();
				pthread_exit ((void *) 1);
	       }
		   cpu[cpuc].eoffs = 5;
		   break;

		case 33:
			// get variable pointer byte size
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];    // pointer var
			arg3 = code[ep + 3];	// pointer type var return

			cpu[cpuc].regi[arg3] = memory_size (cpu[cpuc].regi[arg2]);
			if (cpu[cpuc].regi[arg3] == 0)
			{
				// ERROR: variable pointer not found in vars!
				PRINT_EPOS();
				free_jumpoffs (cpuc);
				loop_stop ();
				pthread_exit ((void *) 1);
			}
			cpu[cpuc].eoffs = 5;
			break;

		case 34:
			// set byte/string variable as immutable
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2]; // pointer to string var
			if (set_immutable_string (cpu[cpuc].regi[arg2]) != 0)
			{
				// ERROR: variable not string!
				PRINT_EPOS();
				free_jumpoffs (cpuc);
				loop_stop ();
				pthread_exit ((void *) 1);
			}
			cpu[cpuc].eoffs = 5;
			break;

		case 35:
			// get host CPU type
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
            cpu[cpuc].regi[arg2] = MACHINE_CPU;
            cpu[cpuc].eoffs = 5;
            break;

		case 36:
			// get host OS type
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
            cpu[cpuc].regi[arg2] = MACHINE_OS;
            cpu[cpuc].eoffs = 5;
            break;

		case 37:
			ep = cpu[cpuc].ep;

			if (debugger ((S8 *) &cpu[cpuc].regi, (F8 *) &cpu[cpuc].regd, ep, cpu[cpuc].sp, cpu[cpuc].sp_bottom, cpu[cpuc].sp_top, cpu_core) == 0)
			{
				if (silent_run == 0)
				{
					printf ("EXIT\n");
				}
				retcode = 0;
				free_jumpoffs (cpuc);
				pthread_mutex_lock (&data_mutex);
				threaddata[cpu_core].status = STOP;
				pthread_mutex_unlock (&data_mutex);
				loop_stop ();
				pthread_exit ((void *) retcode);
			}
			cpu[cpuc].eoffs = 5;
			break;

		case 38:
			// clear stack: stack_clear
			ep = cpu[cpuc].ep;

			cpu[cpuc].sp = cpu[cpuc].sp_top;
			cpu[cpuc].eoffs = 5;
			break;

		case 39:
			// set memory bounds check for variable push/pull on
			ep = cpu[cpuc].ep;

			cpu[cpuc].do_memory_bounds_check = 1;

			cpu[cpuc].eoffs = 5;
			break;

		case 40:
			// set memory bounds check for variables push/pul off
			ep = cpu[cpuc].ep;

			cpu[cpuc].do_memory_bounds_check = 0;

			cpu[cpuc].eoffs = 5;
			break;

        case 41:
            // get time in ms after epoch
            {
			   ep = cpu[cpuc].ep;

			   arg2 = code[ep + 2];
              
               struct timeval tv;

               S8 epoch_ms = 0;
  
               gettimeofday(&tv, NULL);

               unsigned long long millisecondsSinceEpoch = (unsigned long long)(tv.tv_sec) * 1000 + (unsigned long long)(tv.tv_usec) / 1000;
               epoch_ms = (S8) millisecondsSinceEpoch;

               cpu[cpuc].regi[arg2] = (S8) epoch_ms;
            }
            
            cpu[cpuc].eoffs = 5;
			break;

		case 251:
			// set overflow on double reg
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			cpu[cpuc].overflow = 0;
			if (double_state (cpu[cpuc].regd[arg2]) == 1)
			{
				cpu[cpuc].overflow = 1;
			}
			cpu[cpuc].eoffs = 5;
			break;

		case 252:
			// get overflow flag
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			cpu[cpuc].regi[arg2] = cpu[cpuc].overflow;
			cpu[cpuc].eoffs = 5;
			break;

#if JIT_COMPILER
        case 253:
			// run JIT compiler
			ep = cpu[cpuc].ep;

            arg2 = code[ep + 2];
            arg3 = code[ep + 3];
            arg4 = code[ep + 4];
			if (jit_compiler ((U1 *) code, (U1 *) cpu[cpuc].data, (S8 *) cpu[cpuc].jumpoffs, (S8 *) &cpu[cpuc].regi, (F8 *) &cpu[cpuc].regd, (U1 *) cpu[cpuc].sp, cpu[cpuc].sp_top, cpu[cpuc].sp_bottom, cpu[cpuc].regi[arg2], cpu[cpuc].regi[arg3], JIT_code, cpu[cpuc].regi[arg4], code_size) != 0)
            {
                printf ("FATAL ERROR: JIT compiler: can't compile!\n");
				PRINT_EPOS();
                free_jumpoffs (cpuc);
				loop_stop ();
            	pthread_exit ((void *) 1);
            }

            cpu[cpuc].eoffs = 5;
            break;

        case 254:
			ep = cpu[cpuc].ep;

            arg2 = code[ep + 2];
            // printf ("intr0: 254: RUN JIT CODE: %i\n", arg2);
			if (run_jit (cpu[cpuc].regi[arg2], JIT_code) == 1)
			{
				// ERROR can't run JIT-code 
				PRINT_EPOS();
                free_jumpoffs (cpuc);
				loop_stop ();
            	pthread_exit ((void *) 1);
			}

            cpu[cpuc].eoffs = 5;
            break;
#else
		case 253:
			ep = cpu[cpuc].ep;

			printf ("FATAL ERROR: no JIT compiler: can't compile!\n");
			PRINT_EPOS();
			free_jumpoffs (cpuc);
			loop_stop ();
			pthread_exit ((void *) 1);
			break;

		case 254:
			ep = cpu[cpuc].ep;

			printf ("FATAL ERROR: no JIT compiler: can't execute!\n");
			PRINT_EPOS();
			free_jumpoffs (cpuc);
			loop_stop ();
			pthread_exit ((void *) 1);
			break;
#endif


		case 255:
			ep = cpu[cpuc].ep;

			if (silent_run == 0)
			{
				printf ("EXIT\n");
			}
			arg2 = code[ep + 2];
			retcode = cpu[cpuc].regi[arg2];
			free_jumpoffs (cpuc);
			pthread_mutex_lock (&data_mutex);
			threaddata[cpu_core].status = STOP;
			pthread_mutex_unlock (&data_mutex);
			loop_stop ();
			if (cpuc == 0)
		    {
				exit (retcode);
			}
			else
			{
				pthread_exit ((void *) retcode);
			}
			break;

		default:
			printf ("FATAL ERROR: INTR0: %lli does not exist!\n", arg1);
			PRINT_EPOS();
			free_jumpoffs (cpuc);
			loop_stop ();
			pthread_exit ((void *) 1);
	}
	SCHEXE_NEXT();

	intr1:
	#if DEBUG
	printf("%lli INTR1\n", cpu_core);
	#endif
	// special interrupt
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];

	switch (arg1)
	{
		case 0:
			// run new CPU instance
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			arg2 = cpu[cpuc].regi[arg2];

            // search for a free CPU core
            // if none free found set new_cpu to -1, to indicate all CPU cores are used!!
            cpu[cpuc].new_cpu = -1;
            pthread_mutex_lock (&data_mutex);
            for (i = 0; i < max_cpu; i++)
            {
                if (threaddata[i].status == STOP)
                {
                    cpu[cpuc].new_cpu = i;
                    break;
                }
            }
	        pthread_mutex_unlock (&data_mutex);
			if (cpu[cpuc].new_cpu == -1)
			{
				// maximum of CPU cores used, no new core possible

				printf ("ERROR: can't start new CPU core!\n");
				PRINT_EPOS();
				free_jumpoffs (cpuc);
				loop_stop ();
				pthread_exit ((void *) 1);
			}

            // run new CPU core
            // set threaddata

			if (silent_run == 0)
			{
				printf ("current CPU: %lli, starts new CPU: %lli\n", cpu_core, cpu[cpuc].new_cpu);
			}

			pthread_mutex_lock (&data_mutex);
			threaddata[cpu[cpuc].new_cpu].sp = cpu[cpuc].sp;
			threaddata[cpu[cpuc].new_cpu].sp_top = cpu[cpuc].sp_top;
			threaddata[cpu[cpuc].new_cpu].sp_bottom = cpu[cpuc].sp_bottom;

			threaddata[cpu[cpuc].new_cpu].sp_top_thread = cpu[cpuc].sp_top + (cpu[cpuc].new_cpu * stack_size);
			threaddata[cpu[cpuc].new_cpu].sp_bottom_thread = cpu[cpuc].sp_bottom + (cpu[cpuc].new_cpu * stack_size);
			threaddata[cpu[cpuc].new_cpu].sp_thread = threaddata[cpu[cpuc].new_cpu].sp_top_thread - (cpu[cpuc].sp_top - cpu[cpuc].sp);
			threaddata[cpu[cpuc].new_cpu].ep_startpos = arg2;
			threaddata[cpu[cpuc].new_cpu].exit_request = 0;
			pthread_mutex_unlock (&data_mutex);

            // create new POSIX thread

			if (pthread_create (&threaddata[cpu[cpuc].new_cpu].id, NULL, (void *) run, (void*) cpu[cpuc].new_cpu) != 0)
			{
				printf ("ERROR: can't start new thread!\n");
				PRINT_EPOS();
				free_jumpoffs (cpuc);
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
			threaddata[cpu[cpuc].new_cpu].status = RUNNING;
			pthread_mutex_unlock (&data_mutex);
			cpu[cpuc].eoffs = 5;
			break;

		case 1:
			// join threads
			ep = cpu[cpuc].ep;

			if (silent_run == 0)
			{
				printf ("JOINING THREADS...\n");
			}

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

			cpu[cpuc].eoffs = 5;
			break;

		case 2:
			// lock data_mutex
			ep = cpu[cpuc].ep;

			pthread_mutex_lock (&data_mutex);
			cpu[cpuc].eoffs = 5;
			break;

		case 3:
			// unlock data_mutex
			ep = cpu[cpuc].ep;

			pthread_mutex_unlock (&data_mutex);
			cpu[cpuc].eoffs = 5;
			break;

		case 4:
			// return number of current CPU core
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			cpu[cpuc].regi[arg2] = cpu_core;

			cpu[cpuc].eoffs = 5;
			break;

		case 5:
			// return number of free CPU cores
			// search for a free CPU core
			// if none free found set cpus_free to 0, to indicate all CPU cores are used!!
			ep = cpu[cpuc].ep;

			cpu[cpuc].cpus_free = 0;
			pthread_mutex_lock (&data_mutex);
			for (i = 0; i < max_cpu; i++)
			{
				if (threaddata[i].status == STOP)
				{
					cpu[cpuc].cpus_free++;
				}
			}
			pthread_mutex_unlock (&data_mutex);

			arg2 = code[ep + 2];
			cpu[cpuc].regi[arg2] = cpu[cpuc].cpus_free;

			cpu[cpuc].eoffs = 5;
			break;

		case 6:
			// set global mutex
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			if (cpu[cpuc].regi[arg2] >= 0 && cpu[cpuc].regi[arg2] < MAX_MUTEXES)
			{
				pthread_mutex_lock (&global_mutex[cpu[cpuc].regi[arg2]]);
			}
			else 
			{
				printf ("interrupt 1: set global mutex index overflow!\n");
				PRINT_EPOS();
				free_jumpoffs (cpuc);
				loop_stop ();
				pthread_exit ((void *) 1);
			}

			cpu[cpuc].eoffs = 5;
			break;
			
		case 7:
			// unset global mutex
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			if (cpu[cpuc].regi[arg2] >= 0 && cpu[cpuc].regi[arg2] < MAX_MUTEXES)
			{
				pthread_mutex_unlock (&global_mutex[cpu[cpuc].regi[arg2]]);
			}
			else 
			{
				printf ("interrupt 1: unset global mutex index overflow!\n");
				PRINT_EPOS();
				free_jumpoffs (cpuc);;
				loop_stop ();
				pthread_exit ((void *) 1);
			}

			cpu[cpuc].eoffs = 5;
			break;

		case 8:
		{
			ep = cpu[cpuc].ep;

			S8 data_local_size ALIGN = data_mem_size - (stack_size * max_cpu);

		    if (threaddata[cpu_core].data == NULL)
			{
				threaddata[cpu_core].data = (U1 *) calloc (data_local_size, sizeof (U1));
				if (threaddata[cpu_core].data == NULL)
				{
					printf ("interrupt 1: allocate local data: out of memory!\n");
					free_jumpoffs (cpuc);
					loop_stop ();
					pthread_exit ((void *) 1);
				}
			}

			// copy data global to data local
			if (memcpy (threaddata[cpu_core].data, data_global, data_local_size) == NULL)
			{
				printf ("interrupt 1: allocate local data: memory copy error!\n");
				free_jumpoffs (cpuc);
				loop_stop ();
				free (threaddata[cpu_core].data);
				pthread_exit ((void *) 1);
			}
		}

		cpu[cpuc].eoffs = 5;
		break;

		case 9:
			// sane check:
			ep = cpu[cpuc].ep;

		    if (threaddata[cpu_core].data == NULL)
			{
				// ERROR no data local allocated!
				printf ("interrupt 1: switch to local data: local data not allocated!\n");
				free_jumpoffs (cpuc);
				loop_stop ();
				pthread_exit ((void *) 1);
			}
			// switch data access to data local
			cpu[cpuc].data = 	threaddata[cpu_core].data;

			cpu[cpuc].eoffs =  5;
			break;

		case 10:
			// switch data access to data global
			ep = cpu[cpuc].ep;

			cpu[cpuc].data = data_global;

			cpu[cpuc].eoffs = 5;
			break;

		case 11:
			ep = cpu[cpuc].ep;

			pthread_mutex_lock (&data_mutex);
			cpu[cpuc].local_data_ind++;
			pthread_mutex_unlock (&data_mutex);

			if (cpu[cpuc].local_data_ind >= local_data_max)
			{
				printf ("interrupt 1: allocate local function data: out of memory!\n");

				free_jumpoffs (cpuc);
				loop_stop ();
				pthread_exit ((void *) 1);
			}

			S8 data_local_size ALIGN = data_mem_size - (stack_size * max_cpu);

	        if (threaddata[cpu_core].local_data[cpu[cpuc].local_data_ind] == NULL)
			{
				threaddata[cpu_core].local_data[cpu[cpuc].local_data_ind] = (U1 *) calloc (data_local_size, sizeof (U1));
				if (threaddata[cpu_core].local_data[cpu[cpuc].local_data_ind] == NULL)
				{
					printf ("interrupt 1: allocate local function data: out of memory!\n");
					free_jumpoffs (cpuc);
					loop_stop ();
					pthread_exit ((void *) 1);
				}
			}

			// copy data global to data local
			if (memcpy (threaddata[cpu_core].local_data[cpu[cpuc].local_data_ind], data_global, data_local_size) == NULL)
			{
				printf ("interrupt 1: allocate local function data: memory copy error!\n");
				free_jumpoffs (cpuc);
				loop_stop ();
				free (threaddata[cpu_core].data);
				pthread_exit ((void *) 1);
			}

			cpu[cpuc].eoffs = 5;
			break;

		case 12:
			// sane check:
			ep = cpu[cpuc].ep;

		    if (threaddata[cpu_core].local_data[cpu[cpuc].local_data_ind] == NULL)
			{
				// ERROR no data local allocated!
				printf ("interrupt 1: switch to local function data: local data not allocated!\n");
				free_jumpoffs (cpuc);
				loop_stop ();
				pthread_exit ((void *) 1);
			}
			// switch data access to data local
			cpu[cpuc].data = threaddata[cpu_core].local_data[cpu[cpuc].local_data_ind];

			cpu[cpuc].eoffs = 5;
			break;

		case 13:
			// switch data access to data global
			ep = cpu[cpuc].ep;

			cpu[cpuc].data = data_global;

			cpu[cpuc].eoffs = 5;
			break;

		case 14:
			// free local data
			ep = cpu[cpuc].ep;

            if (cpu[cpuc].local_data_ind >= 0)
			{
				if (threaddata[cpu_core].local_data[cpu[cpuc].local_data_ind])
				{
					{
						S8 j ALIGN;

						// overwrite memory with zeroes
						for (j = 0; j < data_local_size; j++)
						{
							threaddata[cpu_core].local_data[cpu[cpuc].local_data_ind][j] = 0;
						}
					}

					free (threaddata[cpu_core].local_data[cpu[cpuc].local_data_ind]);
					threaddata[cpu_core].local_data[cpu[cpuc].local_data_ind] = NULL;

					// decrease index
					pthread_mutex_lock (&data_mutex);
					cpu[cpuc].local_data_ind--;
					pthread_mutex_unlock (&data_mutex);
				}
			}

			cpu[cpuc].eoffs = 5;
			break;

		case 15:
			// do code hot reload from bytecode
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];

			// dealloc code
			free (code);

			// load new bytecode
			if (load_object (&cpu[cpuc].data[cpu[cpuc].regi[arg2]], 1) != 0)
			{
				printf ("interrupt 1: 15: load code: error loading bytecode: %s !\n", &cpu[cpuc].data[cpu[cpuc].regi[arg2]]);

				free_jumpoffs (cpuc);
				loop_stop ();
				pthread_exit ((void *) 1);
			}

		    // global flag set to on:
		    bytecode_hot_reload = 1;
			free_jumpoffs (cpuc);
			pthread_mutex_lock (&data_mutex);
			threaddata[cpu_core].status = STOP;
			// if (threaddata[cpu_core].data != NULL) free (threaddata[cpu_core].data);
			pthread_mutex_unlock (&data_mutex);
			loop_stop ();
			pthread_exit ((void *) 0);
			break;

		case 16:
			// set thread run exit request on given CPU
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];

            if (cpu[cpuc].regi[arg2] < max_cpu)
			{
				threaddata[cpu[cpuc].regi[arg2]].exit_request = 1;
			}
			else
			{
			     printf ("intr1: 16: set CPU exit request: thread number overflow!\n");
			}

			cpu[cpuc].eoffs = 5;
			break;

		case 17:
			// check thread run exit request
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];

		    cpu[cpuc].regi[arg2] = threaddata[cpu_core].exit_request;

			cpu[cpuc].eoffs = 5;
			break;

		case 18:
			// virtual CPU, get number of free cores
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
		    {
				S8 i ALIGN;
				S8 free_vcpu ALIGN = 0;

				// EDIT INTR1
			    pthread_mutex_lock (&cpu_mutex);
				for (i = 0; i < max_virtcpu; i++)
				{
					if (cpu[i].status == STOP)
					{
						free_vcpu++;
					}
				}
				pthread_mutex_unlock (&cpu_mutex);
				cpu[cpuc].regi[arg2] = free_vcpu;
		    }
            cpu[cpuc].eoffs = 5;
			break;

		case 19:
			// virtual CPU, run thread
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			arg3 = code[ep + 3];
			{
				S8 new_vcpu ALIGN = -1;

				for (i = 0; i < max_virtcpu; i++)
				{
					if (cpu[i].status == STOP)
					{
						new_vcpu = i;
						break;
					}
				}

				if (new_vcpu == -1)
				{
					// maximum of virtual CPU cores used, no new core possible

					printf ("ERROR: can't start new virtual CPU core!\n");
					PRINT_EPOS();
					free_jumpoffs (cpuc);
					loop_stop ();
					pthread_exit ((void *) 1);
				}

				if (cpu[cpuc].regi[arg2] == 0)
				{
					printf ("ERROR: can't start new virtual CPU core! ep: 0!\n");
					PRINT_EPOS();
					free_jumpoffs (cpuc);
					loop_stop ();
					pthread_exit ((void *) 1);
				}

				// run new CPU core
				// set threaddata

				if (silent_run == 0)
				{
					printf ("current CPU: %lli, starts new virtual CPU: %lli\n", cpu_core, new_vcpu);
				}

				cpu[new_vcpu].sp = cpu[cpuc].sp;
			    cpu[new_vcpu].sp_top = cpu[cpuc].sp_top;
				cpu[new_vcpu].sp_bottom = cpu[cpuc].sp_bottom;

				cpu[new_vcpu].ep = cpu[cpuc].regi[arg2];

				// printf ("INTR1: 19: start ep: %lli\n", cpu[new_vcpu].ep);

				cpu[new_vcpu].startpos = cpu[cpuc].regi[arg2];
				// set scheduler
				cpu[new_vcpu].status = RUNNING;
				cpu[cpuc].scheduler = 0;
				cpu[new_vcpu].scheduler = SCHEDULER_MAX;

				cpu[new_vcpu].overflow = 0;
				cpu[new_vcpu].data = data_global;
				cpu[new_vcpu].jumpstack_ind = -1;
				cpu[new_vcpu].local_data_ind = -1;
				cpu[new_vcpu].do_memory_bounds_check = 1;

				//cpu[new_vcpu].ins_count = INSCOUNT_MAX;

				if (cpu[cpuc].sp != cpu[cpuc].sp_top)
				{
					// something on mother thread stack, copy it
					cpu[cpuc].srcptr = cpu[cpuc].sp_top;
					cpu[cpuc].dstptr = cpu[new_vcpu].sp_top;

					while (cpu[cpuc].srcptr >= cpu[cpuc].sp)
					{
						*cpu[cpuc].dstptr-- = *cpu[cpuc].srcptr--;
					}
				}

				cpu[new_vcpu].jumpoffs = (S8 *) calloc (code_size, sizeof (S8));
				if (cpu[new_vcpu].jumpoffs == NULL)
				{
					printf ("ERROR: can't allocate %lli bytes for jumpoffsets!\n", code_size);
					loop_stop ();
					pthread_exit ((void *) 1);
				}

		        /*
				threaddata[new_vcpu].local_data = (U1 **) calloc (local_data_max, sizeof (U1*));
				if (threaddata[new_vcpu].local_data == NULL)
				{
					printf ("ERROR: can't allocate %lli elements for local function data!\n", local_data_max);
					free_jumpoffs (cpuc);
					loop_stop ();
					pthread_exit ((void *) 1);
				}
				*/

				{
					S8 i ALIGN;
					for (i = 0; i < local_data_max; i++)
					{
						threaddata[new_vcpu].local_data[i] = NULL;
					}
				}

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

						cpu[new_vcpu].jumpoffs[i] = arg1;
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

						cpu[new_vcpu].jumpoffs[i] = arg1;
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

						cpu[new_vcpu].jumpoffs[i] = arg1;
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

						cpu[new_vcpu].jumpoffs[i] = arg1;
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
						free_jumpoffs (cpuc);
						loop_stop ();
						pthread_exit ((void *) 1);
					}

					if (i >= code_size) break;
				}

				cpu[cpuc].regi[arg3] = new_vcpu;
			}
			cpu[cpuc].ins_count = 0;
			cpu[cpuc].eoffs = 5;
			break;

		case 20:
			// stop vcpu
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
			arg2 = cpu[cpuc].regi[arg2];

			if (cpu[cpuc].jumpoffs)
			{
				free (cpu[cpuc].jumpoffs);
				cpu[cpuc].jumpoffs = NULL;
			}

			pthread_mutex_lock (&cpu_mutex);
			cpu[arg2].status = STOP;
			cpu[arg2].scheduler = SCHEDULER_OFF;
			pthread_mutex_unlock (&cpu_mutex);
			cpu[cpuc].eoffs = 0;
			break;

		case 21:
			// stop vcpu self by running core
			ep = cpu[cpuc].ep;

			pthread_mutex_lock (&cpu_mutex);
			cpu[cpuc].status = STOP;
			cpu[cpuc].scheduler = SCHEDULER_OFF;
			pthread_mutex_unlock (&cpu_mutex);
			if (cpu[cpuc].jumpoffs)
			{
				free (cpu[cpuc].jumpoffs);
				cpu[cpuc].jumpoffs = NULL;
			}

			cpu[cpuc].eoffs = 0;
			break;

		case 22:
			// check if vcpus are stopped, like POSIX thread join
			ep = cpu[cpuc].ep;

			arg2 = code[ep + 2];
            {
				S8 i ALIGN;
				U1 status = STOP;

				for (i = 1; i < max_virtcpu; i++)
				{
					if (cpu[i].status == RUNNING)
					{
						status = RUNNING;
					}
				}

				cpu[cpuc].regi[arg2] = status;
			}

			cpu[cpuc].eoffs = 5;
			break;

		case 255:
			ep = cpu[cpuc].ep;

			if (silent_run == 0)
			{
				printf ("thread EXIT\n");
			}
			arg2 = code[ep + 2];
			retcode = cpu[cpuc].regi[arg2];
			pthread_mutex_lock (&data_mutex);
			threaddata[cpu_core].status = STOP;
			// if (threaddata[cpu_core].data != NULL) free (threaddata[cpu_core].data);
			pthread_mutex_unlock (&data_mutex);
			// loop_stop ();
			pthread_exit ((void *) retcode);
			break;

		default:
			printf ("FATAL ERROR: INTR1: %lli does not exist!\n", arg1);
			PRINT_EPOS();
			free_jumpoffs (cpuc);
			loop_stop ();
			pthread_exit ((void *) 1);
	}
	SCHEXE_NEXT();

	//  superopcodes for counter loops
	inclsijmpi:
	#if DEBUG
	printf ("%lli INCLSIJMPI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];

	arg3 = cpu[cpuc].jumpoffs[ep];

	//printf ("jump to: %li\n", arg3);

	cpu[cpuc].regi[arg1]++;
	if (cpu[cpuc].regi[arg1] < cpu[cpuc].regi[arg2])
	{
		cpu[cpuc].eoffs = 0;
		ep = arg3;
		SCHEXE_NEXT();
	}

	cpu[cpuc].eoffs = 11;

	SCHEXE_NEXT();

	decgrijmpi:
	#if DEBUG
	printf ("%lli DECGRIJMPI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];

	arg3 = cpu[cpuc].jumpoffs[ep];

	cpu[cpuc].regi[arg1]--;
	if (cpu[cpuc].regi[arg1] > cpu[cpuc].regi[arg2])
	{
		cpu[cpuc].eoffs = 0;
		ep = arg3;
		SCHEXE_NEXT();
	}

	cpu[cpuc].eoffs = 11;

	SCHEXE_NEXT();

	movi:
	#if DEBUG
	printf ("%lli MOVI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];

	cpu[cpuc].regi[arg2] = cpu[cpuc].regi[arg1];

	cpu[cpuc].eoffs = 3;
	SCHEXE_NEXT();

	movd:
	#if DEBUG
	printf ("%lli MOVD\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];

	cpu[cpuc].regd[arg2] = cpu[cpuc].regd[arg1];

	cpu[cpuc].eoffs = 3;
	SCHEXE_NEXT();

	loadl:
	#if DEBUG
	printf ("%lli LOADL\n", cpu_core);
	#endif
	// data
	ep = cpu[cpuc].ep;
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

	cpu[cpuc].regi[arg2] = arg1;

	cpu[cpuc].eoffs = 10;
	SCHEXE_NEXT();

	jmpa:
	#if DEBUG
	printf ("%lli JMPA\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];

	if (cpu[cpuc].regi[arg1] >= 16 && cpu[cpuc].regi[arg1] < code_size)
	{
		cpu[cpuc].eoffs = 0;
		cpu[cpuc].ep = cpu[cpuc].regi[arg1];
        #if DEBUG
	    printf ("%lli JUMP TO %lli\n", cpu_core, ep);
	    #endif
	}
	else
	{
		printf ("ERROR: jmpa illegal label!\n");
		PRINT_EPOS();
		free_jumpoffs (cpuc);
		loop_stop ();
		pthread_exit ((void *) 1);
	}
	SCHEXE_NEXT();

	jsr:
	#if DEBUG
	printf ("%lli JSR\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = cpu[cpuc].jumpoffs[ep];

	if (cpu[cpuc].jumpstack_ind == MAXSUBJUMPS - 1)
	{
		printf ("ERROR: jumpstack full, no more jsr!\n");
		PRINT_EPOS();
		free_jumpoffs (cpuc);
		loop_stop ();
		pthread_exit ((void *) 1);
	}

	cpu[cpuc].jumpstack_ind++;
	cpu[cpuc].jumpstack[cpu[cpuc].jumpstack_ind] = ep + 9;

	cpu[cpuc].eoffs = 0;
	cpu[cpuc].ep = arg1;

	SCHEXE_NEXT();

	jsra:
	#if DEBUG
	printf ("%lli JSRA\n", cpu_core);
	#endif

	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];

	#if DEBUG
	printf ("%lli JUMP TO %lli\n", cpu_core, ep);
	#endif

	if (cpu[cpuc].jumpstack_ind == MAXSUBJUMPS - 1)
	{
		printf ("ERROR: jumpstack full, no more jsra!\n");
		PRINT_EPOS();
		free_jumpoffs (cpuc);
		loop_stop ();
		pthread_exit ((void *) 1);
	}

	if (cpu[cpuc].regi[arg1] >= 16 && cpu[cpuc].regi[arg1] < code_size)
	{
		cpu[cpuc].jumpstack_ind++;
		cpu[cpuc].jumpstack[cpu[cpuc].jumpstack_ind] = ep + 2;

		cpu[cpuc].eoffs = 0;
		cpu[cpuc].ep = cpu[cpuc].regi[arg1];
	}
	else
	{
		printf ("ERROR: jsra illegal label!\n");
		PRINT_EPOS();
		free_jumpoffs (cpuc);
		loop_stop ();
		pthread_exit ((void *) 1);
	}
	SCHEXE_NEXT();

	rts:
    #if DEBUG
    printf ("%lli RTS\n", cpu_core);
    #endif

	//printf ("rts: cpu: %lli\n", cpuc);

    if (cpuc > 0)
	{
		pthread_mutex_lock (&cpu_mutex);
        cpu[cpuc].status = STOP;
		cpu[cpuc].scheduler = SCHEDULER_OFF;
	    pthread_mutex_unlock (&cpu_mutex);

		if (cpu[cpuc].jumpoffs)
		{
			free (cpu[cpuc].jumpoffs);
			cpu[cpuc].jumpoffs = NULL;
		}
		cpu[cpuc].eoffs = 1;
	    SCHEXE_NEXT();
	}

    if (cpu[cpuc].jumpstack_ind < 0)
    {
        printf ("ERROR: RTS on an empty jumpstack (underflow)!\n");
        PRINT_EPOS();
        free_jumpoffs (cpuc);
        loop_stop ();
        pthread_exit ((void *) 1);
    }

    cpu[cpuc].ep = cpu[cpuc].jumpstack[cpu[cpuc].jumpstack_ind];
    cpu[cpuc].eoffs = 0;

    cpu[cpuc].jumpstack_ind--;

    SCHEXE_NEXT();


	load:
	#if DEBUG
	printf ("%lli LOAD\n", cpu_core);
	#endif

	// data
	ep = cpu[cpuc].ep;
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

	cpu[cpuc].regi[arg3] = arg1 + arg2;

	cpu[cpuc].eoffs = 18;
	SCHEXE_NEXT();


    noti:
	#if DEBUG
	printf ("%lli NOTI\n", cpu_core);
	#endif
	ep = cpu[cpuc].ep;
	arg1 = code[ep + 1];
	arg2 = code[ep + 2];

	cpu[cpuc].regi[arg2] = ! cpu[cpuc].regi[arg1];

	cpu[cpuc].eoffs = 3;
	SCHEXE_NEXT();

	/* --- END OF OPCODES --- */

	task_scheduler:
{
    /* STEP 1: Sync local IP to the structure before checking quantum.
    if (cpu[cpuc].ep != 0)
	{
		cpu[cpuc].ep = ep;
	}

    /* STEP 2: Handle the Quantum (Time-slice).
     * If the current CPU still has time left, continue execution immediately.
     */
	if (cpu[cpuc].status == RUNNING)
    {
		cpu[cpuc].scheduler--;
		if (cpu[cpuc].scheduler > 0)
		{
			goto *jumpt[code[ep]];
		}
	}

    /* STEP 3: Quantum expired or Switch requested.
     * Reset the quantum for the current core for its next turn.
     */
    cpu[cpuc].scheduler = SCHEDULER_MAX;
    S8 start_cpuc = cpuc;

    /* STEP 4: Round-Robin Search.
     * Look for the next core that is actually in RUNNING state.
     */
    for (S8 i = 1; i < max_virtcpu; i++)
    {
        S8 next_idx = (start_cpuc + i) % max_virtcpu;

        if (cpu[next_idx].status == RUNNING)
        {
            /* Found a ready core! Perform the Context Switch. */
            cpuc = next_idx;

            /* IMPORTANT: Load the NEW core's Instruction Pointer into the local register 'ep'. */
            ep = cpu[cpuc].ep;

            /* Debug Output to verify the switch in your log. */
            // printf("task_scheduler: switched from %lli to %lli\n", start_cpuc, cpuc);
            goto *jumpt[code[ep]];
        }
    }

    /* STEP 5: Fallback.
     * If no other RUNNING cores were found, stay on the current core.
     * Refresh the local 'ep' just in case.
     */
    ep = cpu[cpuc].ep;
    goto *jumpt[code[ep]];
}


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
	S8 func ALIGN;

    for (i = 0; i < MODULES; i++)
    {
        strcpy ((char *) modules[i].name, "");
		modules[i].used = 0;

		for (func = 0; func < MODULES_MAXFUNC; func++)
		{
			modules[i].func_set[func] = 0;
		}
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
	printf ("l1vm <program> [-C cpu_cores] [-T virtual_cpu_cores] [-S stacksize] [-D local_data_entries] [-q] [-p run priority (-20 - 19)] <-args> <cmd args>\n");
	printf ("-C cores : set maximum of threads that can be run\n");
	printf ("-T cores : set maximum of virtual threads that can be run\n");
	printf ("-S stacksize : set the stack size\n");
	printf ("-D local data entries: for thread local data\n");
	printf ("-q : quiet run, don't show welcome messages\n");
	printf ("-p : set run priority, -20 = highest, 19 = lowest priority\n\n");
	printf ("program arguments for the program must be set by '-args':\n");
	printf ("l1vm programname -args foo bar\n");
    printf ("\nDonate at: https://ko-fi.com/P5P2Y3KP\nThank you!\n\n");
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

void print_host_os (void)
{
	U1 os = MACHINE_OS;

	switch (os)
	{
		case 0:
			printf ("Host OS: unknown\n");
			break;

		case 1:
			printf ("Host OS: Linux\n");
			break;

		case 2:
			printf ("Host OS: FreeBSD\n");
			break;

		case 3:
			printf ("Host OS: OpenBSD\n");
			break;

		case 4:
			printf ("Host OS: NetBSD\n");
			break;

		case 5:
			printf ("Host OS: DragonFly BSD\n");
			break;

		case 6:
			printf ("Host OS: Haiku\n");
			break;

		case 7:
			printf ("Host OS: Windows\n");
			break;

		case 8:
			printf ("Host OS: CYGWIN\n");
			break;

		case 9:
			printf ("Host OS: macOS\n");
			break;
	}
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
	printf (">>> %s <<<\n", MOTTO_STR);
	printf ("CPU cores: %lli (STATIC)\n", max_cpu);

	printf ("internal type check: S8 = %lli bytes, F8 = %lli bytes. All OK!\n", size_int64, size_double64);
	print_host_os ();

	#if defined(__clang__)
		printf ("C compiler: clang version %s\n", __clang_version__);
	#elif defined(__GNUC__) || defined(__GNUG__)
		printf ("C compiler: gcc version %s\n", __VERSION__);
	#endif

	printf ("build on: %s\n", __DATE__);

	show_file_root_path ();

	#if JIT_COMPILER
	    printf ("JIT-compiler: ready!\n");
	#endif

	#if MATH_LIMITS
		printf (">> math double overflow check << ");
	#endif

	#if MATH_LIMITS_INT
		printf (">> math int overflow check << ");
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

// L1VM shared library functions
// interface to calling program
#if L1VM_EMBEDDED
long long int l1vm_get_global_data_size (void)
{
	return (data_size);
}

unsigned char *l1vm_get_global_data (void)
{
	return (data_global);
}

void l1vm_cleanup (void)
{
	cleanup ();
}

int l1vm_run_program (char *program_name, int ac, char *av[])
{
	S8 i ALIGN;
	pthread_t id;

	S8 new_cpu ALIGN;

	S8 arglen ALIGN;
	S8 strind ALIGN;
	S8 avind ALIGN;
	U1 cmd_args = 0;		// switched to one, if arguments follow

	// process priority on Linux
    S4 run_priority = 0;        // -20 = highest priority, 19 = lowest priority

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

							if (av[i][0] == '-' && av[i][1] == 'T')
							{
								if (i < ac - 1)
				                {
									// set max cpu cores flag...
									// EDIT T
								    max_virtcpu = atoi (av[i + 1]);
									if (max_cpu == 0)
									{
										printf ("ERROR: max_virtcpu less than 1 core!\n");
										cleanup ();
										exit (1);
									}
									printf ("max_virtcpu: cores set to: %lli\n", max_cpu);
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

	#if JIT_COMPILER
		if (alloc_jit_code () != 0)
		{
			printf ("FATAL ERROR: JIT compiler: can't alloc memory!\n");
		    cleanup ();
			exit (1);
		}
	#endif

    if (load_object ((U1 *) program_name, 0))
    {
		cleanup ();
        exit (1);
    }

	threaddata = (struct threaddata *) calloc (max_cpu, sizeof (struct threaddata));
	if (threaddata == NULL)
	{
		printf ("ERROR: can't allocate threaddata!\n");
		cleanup ();
		exit (1);
	}
// EDIT 1
	cpu = (struct cpu *) calloc (max_virtcpu, sizeof (struct cpu));
	if (cpu == NULL)
	{
		printf ("ERROR: can't allocate CPU data!\n");
		cleanup ();
		exit (1);
	}

    init_modules ();
	// signal (SIGINT, (void *) break_handler);

	// set all higher threads as STOPPED = unused
	for (i = 1; i < max_cpu; i++)
	{
		threaddata[i].status = STOP;
		threaddata[i].data = NULL;
	}
	threaddata[0].status = RUNNING;		// main thread will run
	threaddata[0].data = NULL;

	new_cpu = 0;

	threaddata[new_cpu].sp = (U1 *) &data_global + (data_mem_size - ((max_cpu - 1) * stack_size) - 1);
	threaddata[new_cpu].sp_top = threaddata[new_cpu].sp;
	threaddata[new_cpu].sp_bottom = threaddata[new_cpu].sp_top - stack_size + 1;

	threaddata[new_cpu].sp_thread = threaddata[new_cpu].sp + (new_cpu * stack_size);
	threaddata[new_cpu].sp_top_thread = threaddata[new_cpu].sp_top + (new_cpu * stack_size);
	threaddata[new_cpu].sp_bottom_thread = threaddata[new_cpu].sp_bottom + (new_cpu * stack_size);
	threaddata[new_cpu].ep_startpos = 16;

	// on macOS use wait loop thread, because the SDL module must be run from main thread!!!
	#if __MACH__ || __HAIKU__
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
	/* NOTE!!! */
	/* if on NetBSD, then uncomment this code block, and make a comment on the next code block!! */
	/*
	printf ("\n\nNetBSD: NOTE: POSIX threading does not work!\nIf you know how to call 'pthread_create()' then contact me, please!\n\n");
	run(0);
	*/

	/* if not on NetBSD then uncomment this block */
	/* you should not uncomment both!! */
	/* not NetBSD start */
	if (pthread_create (&id, NULL, (void *) run, (void *) new_cpu) != 0)
	{
		printf ("ERROR: can't start main thread!\n");
		cleanup ();
		exit (1);
	}

    pthread_join (id, NULL);
	#endif
	/* not NetBSD end!*/

	return (retcode);
}
#endif

#if ! L1VM_EMBEDDED
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

							if (av[i][0] == '-' && av[i][1] == 'D')
							{
								if (i < ac - 1)
				                {
									// set max local data flag...
									local_data_max = atoi (av[i + 1]);
									if (stack_size == 0)
									{
										printf ("ERROR: local data max is 0!\n");
										cleanup ();
										exit (1);
									}
									printf ("local_data_max: local data entries set to %lli\n", local_data_max);
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

	#if __linux__
	// set run priority
	nice (run_priority);
	#endif

	if (silent_run == 0)
	{
		show_run_info ();
	}
    if (load_object ((U1 *) av[1], 0))
    {
		cleanup ();
        exit (1);
    }

    run_begin:
	#if JIT_COMPILER
		if (alloc_jit_code () != 0)
		{
			printf ("FATAL ERROR: JIT compiler: can't alloc memory!\n");
		    cleanup ();
			exit (1);
		}
	#endif

	threaddata = (struct threaddata *) calloc (max_cpu, sizeof (struct threaddata));
	if (threaddata == NULL)
	{
		printf ("ERROR: can't allocate threaddata!\n");
		cleanup ();
		exit (1);
	}

	cpu = (struct cpu *) calloc (max_virtcpu, sizeof (struct cpu));
	if (cpu == NULL)
	{
		printf ("ERROR: can't allocate CPU data!\n");
		cleanup ();
		exit (1);
	}

    init_modules ();
	signal (SIGINT, (void *) break_handler);

	// set all higher threads as STOPPED = unused
	for (i = 1; i < max_cpu; i++)
	{
		threaddata[i].status = STOP;
		threaddata[i].data = NULL;
	}
	threaddata[0].status = RUNNING;		// main thread will run
	threaddata[0].data = NULL;

	new_cpu = 0;

	threaddata[new_cpu].sp = (U1 *) &data_global + (data_mem_size - ((max_cpu - 1) * stack_size) - 1);
	threaddata[new_cpu].sp_top = threaddata[new_cpu].sp;
	threaddata[new_cpu].sp_bottom = threaddata[new_cpu].sp_top - stack_size + 1;

	threaddata[new_cpu].sp_thread = threaddata[new_cpu].sp + (new_cpu * stack_size);
	threaddata[new_cpu].sp_top_thread = threaddata[new_cpu].sp_top + (new_cpu * stack_size);
	threaddata[new_cpu].sp_bottom_thread = threaddata[new_cpu].sp_bottom + (new_cpu * stack_size);
	threaddata[new_cpu].ep_startpos = 16;
	threaddata[new_cpu].exit_request = 0;

	// on macOS use wait loop thread, because the SDL module must be run from main thread!!!
	#if __MACH__ || __HAIKU__
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
	/* NOTE!!! */
	/* if on NetBSD, then uncomment this code block, and make a comment on the next code block!! */
	/*
	printf ("\n\nNetBSD: NOTE: POSIX threading does not work!\nIf you know how to call 'pthread_create()' then contact me, please!\n\n");
	run(0);
	*/

	/* if not on NetBSD then uncomment this block */
	/* you should not uncomment both!! */
	/* not NetBSD start */
	if (pthread_create (&id, NULL, (void *) run, (void *) new_cpu) != 0)
	{
		printf ("ERROR: can't start main thread!\n");
		cleanup ();
		exit (1);
	}

    pthread_join (id, NULL);
	#endif
	/* not NetBSD end!*/

	if (bytecode_hot_reload == 1)
	{
		bytecode_hot_reload = 0;
		cleanup_hold_data ();
		goto run_begin;
	}

	cleanup ();
	exit (retcode);
}
#endif
