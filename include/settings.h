// user settings ==============================================================
// machine
#define MAXCPUCORES	    		9		// threads that can be runned, set this to your logical CPU cores + 1
										// for example to 5 on a 4 CPU core CPU!
#define MACHINE_BIG_ENDIAN      0       // endianess of host machine, = 0 little endianess, 1 = big endianness

// define if machine is ARM
// #define M_ARM				1

// division by zero checking
#define DIVISIONCHECK           1

// integer and double floating point math functions overflow detection
// See vm/main.c interrupt0:
// 251 sets overflow flag for double floating point number
// 252 returns value of overflow flag
#define MATH_LIMITS				0

// set if only double numbers calculation results are checked (0), or
// arguments and results get checked for full check (1)
#define MATH_LIMITS_DOUBLE_FULL	0

// data bounds check exactly
#define BOUNDSCHECK             1

// switch on on Linux
#define CPU_SET_AFFINITY		1

// set to defined for DEBUGGING
#define DEBUG					0

// SANDBOX FILE ACCESS
#define SANDBOX                 1			// secure file acces: 1 = use secure access, 0 = OFF!!
#define SANDBOX_ROOT			"/l1vm/"	// in /home directory!, get by $HOME env variable

// VM: max sizes of code
#define MAX_CODE_SIZE 			4294967296L		// 4GB

// VM: max size of data
#define MAX_DATA_SIZE			4294967296L		// 4GB

// VM: set timer interrupt
#define TIMER_USE				1 				// 1 = set timer measurement interrupt

#define DO_ALIGNMENT			1 				// set 64 bit var alignment

#define LOW_RAM					0				// set to 1 on a machine with LOW RAM, like I do on the Psion 5MX Linux build! :)

// user settings end ==========================================================