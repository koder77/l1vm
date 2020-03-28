/*
 * This file global.h is part of L1vm.
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

//  global.h
#if ! _GNU_SOURCE
#define _GNU_SOURCE 1
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#if __linux__
#include <dlfcn.h>
#endif
#if _WIN32
#include <Windows.h>
#endif
#include <inttypes.h>
#include <unistd.h>
#include <sys/time.h>

#include <pthread.h>
#include <sched.h>

#include <math.h>
#include <time.h>
#include <assert.h>
#include <limits.h>

// user settings ==========================================
// machine
#define MAXCPUCORES	    		5		// threads that can be runned
#define MACHINE_BIG_ENDIAN      0       // endianess of host machine

// define if machine is ARM
// #define M_ARM				1

// division by zero checking
#define DIVISIONCHECK 			0

// integer and double floating point math functions overflow detection
// See vm/main.c interrupt0:
// 251 sets overflow flag for double floating point number
// 252 returns value of overflow flag
#define MATH_LIMITS				0

// set if only double numbers calculation results are checked (0), or
// arguments and results get checked for full check (1)
#define MATH_LIMITS_DOUBLE_FULL	0

// data bounds check exactly
#define BOUNDSCHECK				1

// switch on on Linux
#define CPU_SET_AFFINITY		1

// SDL library support
#define SDL_module 				0

// set to defined for DEBUGGING
#define DEBUG				0

// SANDBOX FILE ACCESS
#define SANDBOX                 1			// secure file acces: 1 = use secure access, 0 = OFF!!
#define SANDBOX_ROOT			"/home/stefan/l1vm/"	// in /home directory!

// VM: max sizes of code
#define MAX_CODE_SIZE 			4294967296L		// 4GB

// VM: max size of data
#define MAX_DATA_SIZE			4294967296L		// 4GB

// user settings end ======================================

#define  VM_VERSION_STR		"0.9.15"

#if MAXCPUCORES == 0
	#error "global.h: ERROR MAXCPUCORES is 0!"
#endif

#if MATH_LIMITS_DOUBLE_FULL == 1
	#if MATH_LIMITS == 0
		#error "global.h: ERROR YOU HAVE TO DEFINE MATH_LIMITS TO 1 IN ORDER TO USE MATH_LIMITS_DOUBLE_FULL!"
	#endif
#endif

#if MATH_LIMITS
	#pragma message ("MATH LIMITS ON")
#endif
#if MATH_LIMITS_DOUBLE_FULL
	#pragma message ("MATH LIMITS DOUBLE FULL ON")
#endif

typedef unsigned char           U1;		/* UBYTE   */
typedef int16_t                 S2;     /* INT     */
typedef uint16_t                U2;     /* UINT    */
typedef int32_t                 S4;     /* LONGINT */

typedef long long               S8;     /* 64 bit long */
typedef double                  F8;     /* DOUBLE */


/* set alignment for Android ARM */
#if M_ARM
#define ALIGN		__attribute__ ((aligned(8)))
#else
#define ALIGN
#endif

#define TRUE					1
#define FALSE					0

// machine
#define MAXREG			256			// registers (integer and double float)

#define MAXLINELEN      512
#define MAXARGS         64
#define MAXBRACKETLEVEL	64
#define MAXEXPRESSION	32				// expressions per bracket level
#define MAXLINES		4096

#define REM_SB          "//"
#define ASM_SB 			"(ASM)"
#define ASM_END_SB		"(ASM_END)"

#define COMP_AOT_SB      "(COMP_AOT)"
#define COMP_AOT_END_SB  "(COMP_AOT_END)"

// data types UNKNOWN (0) -> x
#define UNKNOWN         7
#define BYTE            8
#define WORD            9
#define DOUBLEWORD      10
#define QUADWORD        11
#define DOUBLEFLOAT     12

#define STRING			13

// translate.h:
#define INTEGER			14
#define DOUBLE			15

#define MODULES                 32
#define MODULES_MAXFUNC         256

#define RUNNING			1
#define STOP			0

// max input string len
#define MAXINPUT 512

// stacksize in bytes
#define STACKSIZE	64000

// jump call stack, for jsr, jsra
#define MAXSUBJUMPS		256


// ERROR codes returned by VM
#define ERR_FILE_OK         0
#define ERR_FILE_OPEN       1
#define ERR_FILE_CLOSE      2
#define ERR_FILE_READ       3
#define ERR_FILE_WRITE      4
#define ERR_FILE_NUMBER     5
#define ERR_FILE_EOF        6
#define ERR_FILE_FPOS       7


// for time functions
struct tm *tm;


struct threaddata
{
	U1 *sp;			// stack pointer of mother thread
	U1 *sp_top;     // stack pointer top of mother thread
	U1 *sp_bottom;	// stack botttom of mother thread
	U1 *sp_thread;  // stack pointer of new thread
	U1 *sp_top_thread;
	U1 *sp_bottom_thread;
	S8 ep_startpos ALIGN;	// code startpos for new thread
	pthread_t id;			// thread ID
	U1 status;				// thread status
};

struct t_var
{
    U1 type;
    U1 digitstr[MAXLINELEN];
    U1 digitstr_type;
	U1 base;
};

#define MAXDATAINFO             4096 // data names

struct data_info
{
    U1 name[MAXLINELEN];
    S8 offset ALIGN;
    S8 size ALIGN;
    S8 end ALIGN;
	S8 type_size ALIGN;
	U1 type;

	// compiler
	U1 value_str[MAXLINELEN];
	U1 type_str[2];
};

#define MAXOPCODES              61


#if ! JIT_COMPILER
struct opcode
{
    U1 op[16];
    S2 args;
    U1 type[4];
};
#endif

// compiler:
struct translate
{
	U1 op[32];
	S2 args;
	U1 type[4];
	U1 assemb_op;
};

#define MAXLABELS				1024
#define LABELLEN				64

struct label
{
	U1 name[LABELLEN];
	S8 pos;
};

struct call_label
{
	U1 name[LABELLEN];
	S8 pos;
};

// shell arguments
#define MAXSHELLARGS			32
#define MAXSHELLARGLEN			256

#if ! JIT_COMPILER
struct opcode opcode[MAXOPCODES];
#endif

// This is type of function we will generate
// for JIT-compiler
typedef void (*Func)(void);

struct JIT_code
{
	Func fn;
};


// 61 opcodes
#define PUSHB   0
#define PUSHW   1
#define PUSHDW  2
#define PUSHQW  3
#define PUSHD   4

#define PULLB   5
#define PULLW   6
#define PULLDW  7
#define PULLQW  8
#define PULLD   9

#define ADDI    10
#define SUBI    11
#define MULI    12
#define DIVI    13

#define ADDD    14
#define SUBD    15
#define MULD    16
#define DIVD    17

#define SMULI   18
#define SDIVI   19

#define ANDI    20
#define ORI     21
#define BANDI   22
#define BORI    23
#define BXORI   24
#define MODI    25

#define EQI     26
#define NEQI    27
#define GRI     28
#define LSI     29
#define GREQI   30
#define LSEQI   31

#define EQD     32
#define NEQD    33
#define GRD     34
#define LSD     35
#define GREQD   36
#define LSEQD   37

#define JMP     38
#define JMPI    39

#define STPUSHB     40
#define STPOPB      41

#define STPUSHI   42
#define STPOPI   43

#define STPUSHD   44
#define STPOPD    45

#define LOADA     46
#define LOADD     47

#define INTR0      48
#define INTR1      49

// superopcodes for counter loops
#define INCLSIJMPI 50
#define DECGRIJMPI 51

#define MOVI	52
#define MOVD	53

#define LOADL   54
#define JMPA	55

#define JSR		56
#define JSRA	57
#define RTS		58

#define LOAD    59

#define NOTI	60
