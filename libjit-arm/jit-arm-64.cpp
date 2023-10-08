/*
 * This file jit-arm-64.cpp is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2023
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

// JIT-compiler uses asmjit library.
// Generates code for ARM 64 bit.

// work in progress!!!
// For now just:
// addi, subi, muli, divi
// addd, subd, muld, divd
//

// THIS CODE DOESN'T WORK! IT CRASHES!!!
// If you know how to fix this then contact me please.
// So we can work together!


#include <asmjit/a64.h>
#include <asmjit/core.h>

using namespace asmjit;
using namespace asmjit::a64;

JitRuntime rt;                          // Create a runtime specialized for JIT.
CodeHolder jcode;                   	// Holds code and relocation information.

#include "../vm/jit.h"

#include "../include/global.h"
#include "../include/stack.h"

// This is type of function we will generate
typedef void (*Func)(void);

#define MAXREGJIT_INT 4
#define MAXREGJIT_DOUBLE 16


#define OFFSET(x) x * 8

		#define RSI 	x0
		#define RDI 	x1
		#define R8 		x2
		#define R9 		x3
		#define R10		x4
		#define R11 	x5
		#define RBX 	x6
		#define RCX 	x7
		#define EAX 	x8
		#define EDX		x9
		// #define ST0     x86::fp7
		// #define ST1     x86::fp6

struct JIT_label
{
	asmjit::Label lab;
	S8 pos ALIGN;	// was S4
	S8 if_ ALIGN;
	S8 endif ALIGN;
};

struct JIT_label JIT_label[MAXJUMPLEN];

S8 JIT_label_ind ALIGN = -1;


// for storing VM registers
// S8 jit_regs[MAXREGJIT_INT];			// R8 (0) to R11 (3)
S8 jit_regsd[MAXREGJIT_DOUBLE]; 	// xmm0 (0) to xmm5 (5)

// double registers code ======================================================
S8 get_double_reg (S8 reg)
{
	S8 i ALIGN;

	for (i = 0; i < MAXREGJIT_DOUBLE; i++)
	{
		if (jit_regsd[i] == reg)
		{
			return (i);
		}
	}
	return (-1);	// jit register not found
}

S8 get_free_double_reg (S8 reg)
{
	S8 i ALIGN;

	for (i = 2; i < MAXREGJIT_DOUBLE; i++)
	{
		if (jit_regsd[i] == 0)
		{
			return (i);
		}
	}
	return (-1);	// no free jit register found
}

void free_double_reg (S8 cpu_reg)
{
	// free CPU register
	jit_regsd[cpu_reg] = 0;
}

S8 set_double_reg (S8 cpu_reg, S8 reg)
{
	jit_regsd[cpu_reg] = reg;
	return (1);
}

extern "C" int jit_compiler (U1 *code, U1 *data, S8 *jumpoffs ALIGN, S8 *regi ALIGN, F8 *regd ALIGN, U1 *sp, U1 *sp_top, U1 *sp_bottom, S8 start ALIGN, S8 end ALIGN, struct JIT_code *JIT_code, S8 JIT_code_ind ALIGN, S8 code_size ALIGN)
{
    S8 i ALIGN;
    S8 j ALIGN;
    S8 l ALIGN;
    S8 r1 ALIGN;
    S8 r2 ALIGN;
    S8 r3 ALIGN;
    S2 offset;
    U1 label_created;
    S8 label ALIGN;
    U1 run_jit = 0;

	S8 r1_d ALIGN;
	S8 r2_d ALIGN;
	S8 r3_d ALIGN;

	// JMP opcode:
	U1 jump_ok = 0;

	S8 jump_l ALIGN = 0;
	S8 jump_label ALIGN = 0;
	S8 jump_target ALIGN = 0;
 
  	jcode.init(rt.environment());      // Initialize code to match the JIT environment.
 
  	Assembler a(&jcode);           // Create and attach arm::Assembler to code.

    // X86Gp RSIback;
    a.mov (RSI, imm ((intptr_t)(void *) regi)); /* long registers base: rsi */
	a.mov (RDI, imm ((intptr_t)(void *) regd)); /* double registers base: rdi */

    /* initialize label pos */
	for (i = 0; i < MAXJUMPLEN; i++)
	{
		JIT_label[i].pos = -1;
	}

	// init jit_regs
	// CPU register
	for (i = 0; i <= 5; i++)
	{
		jit_regsd[i] = 0;
	}

    i = start;
    while (i <= end)
    {
		offset = 0;
		#if DEBUG
			printf ("opcode: %i\n", code[i]);
		#endif

        /* check if current opcode is on label */
		label_created = 0;

		// printf ("DEBUG: jit_compiler: code_size: %lli\n", code_size);

		for (j = start; j <= end; j++)
		{
			// printf ("DEBUG: jit_compiler: j: %lli\n", j);

			if (i == jumpoffs[j])
			{
				/* create label */

                for (l = 0; l < MAXJUMPLEN; l++)
				{
					if (JIT_label[l].pos == i)
					{
						label_created = 1;
						label = l;
						break;
					}
				}

				if (label_created == 0 && JIT_label_ind < MAXJUMPLEN)
				{
					JIT_label_ind++;
				}
				else
				{
					if (JIT_label_ind == MAXJUMPLEN)
					{
						printf ("JIT compiler: error label list full!\n");
						return (1);
					}
				}

				if (label_created == 0)
				{
					JIT_label[JIT_label_ind].lab = a.newLabel ();
					JIT_label[JIT_label_ind].pos = jumpoffs[j];
					JIT_label[JIT_label_ind].if_ = -1;
					JIT_label[JIT_label_ind].endif = -1;
					a.bind (JIT_label[JIT_label_ind].lab);
				}
				else
				{
					a.bind (JIT_label[label].lab);

                    #if DEBUG
                        printf ("LABEL binded!\n");
                    #endif
				}
            }
        }

		// set opcode offset for current opcode

        if (code[i] <= LSEQD)
		{
			offset = 4;
		}

		if (code[i] == JMP)
		{
            offset = 9;
        }
        if (code[i] == JMPI)
        {
            offset = 10;
        }

		if (code[i] == INCLSIJMPI || code[i] == DECGRIJMPI)
		{
			offset = 11;
		}

		if (code[i] == JSR)
		{
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
			// INTERNAL ERROR: no offset found!!!
			printf ("FATAL error: JIT compiler: setting jump offset failed! opcode: %i\n", code[i]);
            return (1);
		}

		// check if current opcode is in JIT-compiler:

		switch (code[i])
        {
			// ADDI, SUBI, MULI, DIVI ==================================================
            case ADDI:
			case SUBI:
			case MULI:
			case DIVI:
                r1 = code[i + 1];
                r2 = code[i + 2];
                r3 = code[i + 3];

				#if DEBUG
					printf ("JIT-compiler: int opcode: %i: R1 = %lli, R2 = %lli, R3 = %lli\n", code[i], r1, r2, r3);

					switch (code[i])
					{
						case ADDI:
							printf ("ADDI\n\n");
							break;

						case SUBI:
							printf ("SUBI\n\n");
							break;

						case MULI:
							printf ("MULI\n\n");
							break;

						case DIVI:
							printf ("DIVI\n\n");
							break;
					}
				#endif

	            a.ldrsb (R8, ptr (RSI, OFFSET(r1)));
	            a.ldrsb (R9, ptr (RSI, OFFSET(r2)));

				/// x86:
				// a.mov (R8, ptr (RSI, OFFSET(r1))); /* r1v */
				// a.mov (R9, asmjit::arm::qword_ptr (RSI, OFFSET(r2)));

				switch (code[i])
				{
					case ADDI:
						a.add (R10, R8, R9);
						break;

					case SUBI:
						a.sub (R10, R8, R9);
						break;

					case MULI:
						a.mul (R10, R8, R9);
						break;

					case DIVI:
						a.sdiv (R10, R8, R9);
						break;
				}

				a.str (R10, ptr (RSI, OFFSET(r3)));
				
				// x86:
				// a.mov (asmjit::x86::qword_ptr (RSI, OFFSET(r3)), R8);
				run_jit = 1;
				break;

			// ADDD, SUBD, MULD, DIVD ==================================================
			case ADDD:
			case SUBD:
			case MULD:
			case DIVD:
				// double precision floating point using SSE
				r1 = code[i + 1];
				r2 = code[i + 2];
				r3 = code[i + 3];

				#if DEBUG
					printf ("JIT-compiler: double opcode: %i: R1 = %lli, R2 = %lli, R3 = %lli\n", code[i], r1, r2, r3);

					switch (code[i])
					{
						case ADDD:
							printf ("ADDD\n\n");
							break;

						case SUBD:
							printf ("SUBD\n\n");
							break;

						case MULD:
							printf ("MULD\n\n");
							break;

						case DIVD:
							printf ("DIVD\n\n");
							break;
					}
				#endif

				// move to XMM registers
				r1_d = get_double_reg (r1);
				if (r1_d == -1)
				{
					a.ldr (d0, ptr (RDI, OFFSET(r1)));

					// x86:
					//a.movsd (asmjit::x86::xmm0, asmjit::x86::qword_ptr (RDI, OFFSET(r1)));
					// set register r1 to CPU reg 0
					set_double_reg (0, r1);
				}

				r2_d = get_double_reg (r2);
				if (r2_d == -1)
				{
					a.ldr (d1, ptr (RDI, OFFSET(r2)));

					// x86:
					// a.movsd (asmjit::x86::xmm1, asmjit::x86::qword_ptr (RDI, OFFSET(r2)));
					// set register r2 to CPU reg 1
					// set_double_reg (1, r2);

					r3_d = get_free_double_reg (r2);
					if (r3_d != -1)
					{
						switch (r3_d)
						{
							case 2:
								a.fmov (d2, d1);
								break;

							case 3:
								a.fmov (d3, d1);
								break;

							case 4:
								a.fmov (d4, d1);
								break;

							case 5:
								a.fmov (d5, d1);
								break;

							case 6:
								a.fmov (d6, d1);
								break;

							case 7:
								a.fmov (d7, d1);
								break;

							case 8:
								a.fmov (d8, d1);
								break;

							case 9:
								a.fmov (d9, d1);
								break;

							case 10:
								a.fmov (d10, d1);
								break;

							case 11:
								a.fmov (d11, d1);
								break;

							case 12:
								a.fmov (d12, d1);
								break;

							case 13:
								a.fmov (d13, d1);
								break;

							case 14:
							    a.fmov (d14, d1);
								break;

							case 15:
								a.fmov (d15, d1);
								break;
						}

						set_double_reg (r3_d, r2);
					}
					else
					{
						// store r2 into CPU reg 15
						free_double_reg (15);
						a.fmov (d15, d1);
						set_double_reg (15, r2);
					}
				}
				else
				{
					switch (r2_d)
					{
						case 2:
							a.fmov (d1, d2);
							break;

						case 3:
							a.fmov (d1, d3);
							break;

						case 4:
							a.fmov (d1, d4);
							break;

						case 5:
							a.fmov (d1, d5);
							break;

						case 6:
							a.fmov (d1, d6);
							break;

						case 7:
							a.fmov (d1, d7);
							break;

						case 8:
							a.fmov (d1, d8);
							break;

						case 9:
							a.fmov (d1, d9);
							break;

						case 10:
							a.fmov (d1, d10);
							break;

						case 11:
							a.fmov (d1, d11);
							break;

						case 12:
							a.fmov (d1, d12);
							break;

						case 13:
							a.fmov (d1, d13);
							break;

						case 14:
							a.fmov (d1, d14);
							break;

						case 15:
							a.fmov (d1, d15);
							break;
					}
				}

				switch (code[i])
				{
					case ADDD:
						a.fadd (d0, d0, d1);
						break;

					case SUBD:
						a.fsub (d0, d0, d1);
						break;

					case MULD:
						a.fmul (d0, d0, d1);
						break;

					case DIVD:
						a.fdiv (d0, d0, d1);
						break;
				}

				a.str (d0, ptr (RDI, OFFSET(r3)));

				// x86:
				// move to destination register
				// a.movsd (asmjit::x86::qword_ptr (RDI, OFFSET(r3)), asmjit::x86::xmm0);
				run_jit = 1;
				break;

			// DEFAULT: output ERROR message if oopcode not found! =====================
			default:
                printf ("JIT compiler: UNKNOWN opcode: %i - exiting!\n", code[i]);
                return (1);
        }
        i = i + offset;
    }

    if (run_jit)
    {
        a.ret (x0);		// return to main program code

		// printf ("JIT_code_ind: %lli\n", JIT_code_ind);

        if (JIT_code_ind < MAXJITCODE - 1) // JIT_code_ind overflow fix!!
        {
            // create JIT code function

            JIT_code_ind++;

            Func funcptr;

			// store JIT code:
            Error err = rt.add (&funcptr, &jcode);
            if (err == 1)
            {
                printf ("JIT compiler: code generation failed!\n");
                return (1);
            }

            JIT_code[JIT_code_ind].fn = (Func) funcptr;
            JIT_code[JIT_code_ind].used = 1;
            #if DEBUG
                printf ("JIT compiler: function saved.\n");
            #endif

            return (0);
        }
        else
        {
            printf ("JIT compiler: error jit code list full!\n");
            return (1);
        }
	}
	return (0);
}

extern "C" int run_jit (S8 code ALIGN, struct JIT_code *JIT_code)
{
	#if	 DEBUG
		printf ("run_jit: code: %lli\n", code);
	#endif

	if (code < 0 || code >= MAXJITCODE)
	{
		printf ("JIT compiler: FATAL ERROR! code index %lli out of range!!!\n", code);
		return (1);
	}

	if (JIT_code[code].used == 0)
	{
		printf ("JIT compiler: FATAL ERROR! code index %lli not compiled!\n", code);
		return (1);
	}

	Func func = JIT_code[code].fn;

    #if DEBUG
        printf ("run_jit: code address: %lli\n", (S8) func);
    #endif

    if (func == NULL)
	{
		printf ("JIT compiler: FATAL ERROR! NULL pointer code!!!\n");
		return (1);
	}

	// call JIT code function, stored in JIT_code[]
	JIT_code[code].fn();
	return (0);
}

extern "C" int free_jit_code (struct JIT_code *JIT_code, S8 JIT_code_ind)
{
	/* free all JIT code functions from memory */

	S4 i;

	if (JIT_code_ind > -1)
	{
		for (i = 0; i <= JIT_code_ind; i++)
		{
			rt.release((Func *) JIT_code[i].fn);
		}
	}

	return (0);
}
