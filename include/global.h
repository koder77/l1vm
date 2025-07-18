/*
 * This file global.h is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (info@midnight-coding.de), 2022
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

// OS type messsage
// 0 means unknown
#if __linux__
#pragma message ("OS type: Linux")
#define MACHINE_OS 1
#elif __FreeBSD__
#pragma message ("OS type: FreeBSD")
#define MACHINE_OS 2
#elif __OpenBSD__
#pragma message ("OS type: OpenBSD")
#define MACHINE_OS 3
#elif __NetBSD__
#pragma message ("OS type: NetBSD")
#define MACHINE_OS 4
#elif __DragonFly__
#pragma message ("OS type: Dragonfly BSD")
#define MACHINE_OS 5
#elif __HAIKU__
#pragma message ("OS type: Haiku")
#define MACHINE_OS 6
#elif _WIN32
#pragma message ("OS type: Windows")
#define MACHINE_OS 7
#elif __CYGWIN__
#pragma message ("OS type: CYGWIN")
#define MACHINE_OS 8
#elif __MACH__
#pragma message ("OS type: macOS")
#define MACHINE_OS 9
#else
#pragma message ("OS type: unknown")
#define MACHINE_OS 0
#endif


// set __linux__
#if __CYGWIN__ || __MACH__ || __OpenBSD__ || __FreeBSD__ || __NetBSD__ || __DragonFly__ || ___HAIKU__
	#define __linux__	1
#endif

// get CPU type on compile time:
// 0 means unknown
#if defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
#define MACHINE_CPU 1
#elif defined(__x86_64__) || defined(_M_X64)
#define MACHINE_CPU 2
#elif defined(__aarch64__) || defined(_M_ARM64)
#define MACHINE_CPU 3
#elif defined(__powerpc) || defined(__powerpc__) || defined(__powerpc64__) || defined(__POWERPC__) || defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC)
#define MACHINE_CPU 4
#elif defined(__PPC64__) || defined(__ppc64__) || defined(_ARCH_PPC64)
#define MACHINE_CPU 5
#elif defined(__sparc__) || defined(__sparc)
#define MACHINE_CPU 6
#elif defined(__m68k__)
#define MACHINE_CPU 7
#elif defined(mips) || defined(__mips__) || defined(__mips)
#define MACHINE_CPU 8
#elif defined(__sh__)
#define MACHINE_CPU 9
#else
#pragma message ("CPU type: unknown")
#define MACHINE_CPU 0
#endif


// new for parse-infix.c compiler
#include <sys/types.h>
#include <regex.h>
// ------------------------------
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
#include <winsock2.h>
#include <ws2tcpip.h>
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

#if __NetBSD__
#include <sys/wait.h>
#include <sys/resource.h>

#define SDL_BYTEORDER LITTLE_ENDIAN
#endif

// include user settings file
#include "settings.h"


// internal settings ==========================================================
// handle with care!
// The values you can try on an machine with low RAM, are shown in the comments below!
// This should it make possible to run the L1VM in just a few MB of RAM!

// max input string len
#define MAXINPUT 				512

// stacksize in bytes
//#define STACKSIZE				64000
#define STACKSIZE               4096

// code labels name len
#define LABELLEN				64

#if LOW_RAM
	#define MODULES				32
	#define MODULES_MAXFUNC		256

	// Jump call stack, for jsr, jsra: how many jsr and jsra MAXSUBJUMPS can be done without a rts (return) call.
	// If you want to save RAM you can set this value to 256!
	#define MAXSUBJUMPS			256   // 256 on low RAM

	// max number of code labels, and their name length:
	#define MAXLABELS			512	// 1024 on low RAM
										//
    // max number of Brackets objects
    #define MAXOBJECTS          512

    #define MAXLINELEN      100 	// MAX LENGTH for strlen_safe string length
    #define MAXSTRLEN		256		// MAX LENGTH for maximum string size for static allocation
#else
    #if MEDIUM_RAM
    #define MODULES				128
	#define MODULES_MAXFUNC	    2048

	// Jump call stack, for jsr, jsra: how many jsr and jsra MAXSUBJUMPS can be done without a rts (return) call.
	// If you want to save RAM you can set this value to 256!
	#define MAXSUBJUMPS			1000

	// max number of code labels:
	#define MAXLABELS			4096

	// max number of Brackets objects
    #define MAXOBJECTS      512

    #define MAXLINELEN      512    	// MAX LENGTH for strlen_safe string length, was 4096
    #define MAXSTRLEN		4096	// MAX LENGTH for maximum string size for static allocation

    #else

	#define MODULES				1024
	#define MODULES_MAXFUNC		4096

	// Jump call stack, for jsr, jsra: how many jsr and jsra MAXSUBJUMPS can be done without a rts (return) call.
	// If you want to save RAM you can set this value to 256!
	#define MAXSUBJUMPS			40960

	// max number of code labels:
	#define MAXLABELS			40960

	// max number of Brackets objects
    #define MAXOBJECTS          40960

    #define MAXLINELEN      512    	// MAX LENGTH for strlen_safe string length, was 4096
    #define MAXSTRLEN		4096	// MAX LENGTH for maximum string size for static allocation
    #endif
#endif

// internal settings end ======================================================

// info strings:
#define COPYRIGHT_STR " 2025 (C) 2017-2025 Stefan Pietzonke - software research"
#define VM_VERSION_STR "3.5.1"
#define MOTTO_STR "Level 1 VM - supernova ^ 2"

// no user defined definitions below this section! ============================

#if MAXCPUCORES == 0
	#error "global.h: ERROR MAXCPUCORES is 0!"
#endif

#if MATH_LIMITS_DOUBLE_FULL == 1
	#if MATH_LIMITS == 0
		#error "global.h: ERROR YOU HAVE TO DEFINE MATH_LIMITS TO 1 IN ORDER TO USE MATH_LIMITS_DOUBLE_FULL!"
	#endif
#endif

// BOUNDSCHECK now always must be set!
#if BOUNDSCHECK == 0
    #error "BOUNDSCHECK always must be set to 1!"
#endif


typedef unsigned char           U1;		/* UBYTE   */
typedef int16_t                 S2;     /* INT     */
typedef uint16_t                U2;     /* UINT    */
typedef int32_t                 S4;     /* LONGINT */

typedef long long               S8;     /* 64 bit long */
typedef double                  F8;     /* DOUBLE */


/* set alignment for Android ARM */
#if DO_ALIGNMENT
#undef ALIGN
#define ALIGN		__attribute__ ((aligned(8)))
// #pragma message ("ALIGNMENT FOR 64 bit ON")
#else
#define ALIGN
#endif

#define TRUE					1
#define FALSE					0

// machine
#define MAXREG			256			// registers (integer and double float)

#define MAXARGS         64
#define MAXBRACKETLEVEL	64
#define MAXEXPRESSION	32				// expressions per bracket level
#define MAXLINES		4096

#define REM_SB          "//"
#define ASM_SB 			"(ASM)"
#define ASM_END_SB		"(ASM_END)"

// data types UNKNOWN (0) -> x
#define UNKNOWN         7
#define BYTE            8
#define WORD            9
#define DOUBLEWORD      10
#define QUADWORD        11
#define DOUBLEFLOAT     12

#define STACK_BYTE			0
#define STACK_QUADWORD		1
#define STACK_DOUBLEFLOAT	2
#define STACK_UNSET			3

#define STRING			13
#define STRING_CONST    14

// translate.h:
#define INTEGER			15
#define DOUBLE			16

#define RUNNING			1
#define STOP			0


// ERROR codes returned by VM
#define ERR_FILE_OK         0
#define ERR_FILE_OPEN      -1
#define ERR_FILE_CLOSE     -2
#define ERR_FILE_READ      -3
#define ERR_FILE_WRITE     -4
#define ERR_FILE_NUMBER    -5
#define ERR_FILE_EOF       -6
#define ERR_FILE_FPOS      -7


// local_data_max
#define LOCAL_DATA_MAX 1024

// for time functions
// struct tm *tm;

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
    U1 *data;               // own thread data
    U1 **local_data;       // local data for functions
    U1 exit_request;       // set by the caller thread, can be checked by the new called thread, 1 = exit requested, 0 = off
};

struct t_var
{
    U1 type;
    U1 digitstr[MAXSTRLEN];
    U1 digitstr_type;
	U1 base;
};

struct data_info
{
    U1 name[MAXSTRLEN];
	S8 offset ALIGN;
	S8 size ALIGN;
	S8 end ALIGN;
	S8 type_size ALIGN;
	U1 type;

	// compiler
	U1 value_str[MAXSTRLEN];
	U1 type_str[2];
	U1 constant;				// set to one if variable is constant
};


// compiler, set variables as unused/used
// to get defined but unused variables
struct data_info_var
{
	U1 used;
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
#define MAXSHELLARGLEN			1048576


// This is type of function we will generate
// for JIT-compiler
typedef void (*Func)(void);

struct JIT_code
{
	Func fn;
	U1 used;
};

// ranges for variable assign checks
// store the variable name and the min/max variable names
struct range
{
    U1 varname[MAXSTRLEN];
    U1 min[MAXSTRLEN];
    U1 max[MAXSTRLEN];
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

#define ADDI    10	// in JIT-Compiler
#define SUBI    11	//
#define MULI    12	//
#define DIVI    13	//

#define ADDD    14	// in JIT-compiler
#define SUBD    15	//
#define MULD    16	//
#define DIVD    17	//

#define SMULI   18
#define SDIVI   19

#define ANDI    20  // in JIT-compiler
#define ORI     21	// in JIT-compiler
#define BANDI   22	// in JIT-compiler
#define BORI    23	// in JIT-compiler
#define BXORI   24	// in JIT-compiler
#define MODI    25

#define EQI     26	// in JIT-compiler
#define NEQI    27	//
#define GRI     28	//
#define LSI     29	//
#define GREQI   30	//
#define LSEQI   31	//

#define EQD     32	// in JIT-Compiler
#define NEQD    33	//
#define GRD     34	//
#define LSD     35	//
#define GREQD   36	//
#define LSEQD   37	//

#define JMP     38	// in JIT-compiler
#define JMPI    39	//

#define STPUSHB 40
#define STPOPB  41

#define STPUSHI  42
#define STPOPI   43

#define STPUSHD  44
#define STPOPD   45

#define LOADA    46
#define LOADD    47

#define INTR0    48
#define INTR1    49

// superopcodes for counter loops
#define INCLSIJMPI 50
#define DECGRIJMPI 51

#define MOVI	52	// in JIT_compiler
#define MOVD	53	//

#define LOADL   54
#define JMPA	55

#define JSR		56
#define JSRA	57
#define RTS		58

#define LOAD    59

#define NOTI	60
