/*
 * This file jit.cpp is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2018
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
// Generates code for x86 64 bit. The double floating point number opcodes use SSE opcodes.
// The integer opcodes are "optimized" by reducing a.mov if possible.

// TODO: Add more opcodes.


#include <asmjit/asmjit.h>

using namespace asmjit;

JitRuntime rt;                          // Create a runtime specialized for JIT.

// #define DEBUG 1

#define JIT_COMPILER 1

#include "../include/global.h"
#include "../include/stack.h"

// This is type of function we will generate
typedef void (*Func)(void);

#define MAXJITCODE 64
#define MAXJUMPLEN 64

#define OFFSET(x) x * 8
#define JIT_X86_64 1

	#if JIT_X86_64
		#define RSI 	x86::rsi
		#define RDI 	x86::rdi
		#define R8 		x86::r8
		#define R9 		x86::r9
		#define R10		x86::r10
		#define R11 	x86::r11
		#define RBX 	x86::rbx
		#define RCX 	x86::rcx
		#define EAX 	x86::eax
		#define EDX		x86::edx
		#define ST0     x86::fp7
		#define ST1     x86::fp6
	#endif



extern "C" struct JIT_code JIT_code[];

struct JIT_label
{
	asmjit::Label lab;
	S4 pos;
	S4 if_;
	S4 endif;
};

struct JIT_label JIT_label[MAXJUMPLEN];

S8 JIT_label_ind ALIGN = -1;
extern "C" S8 JIT_code_ind;


extern "C" int jit_compiler (U1 *code, U1 *data, S8 *jumpoffs ALIGN, S8 *regi ALIGN, F8 *regd ALIGN, U1 *sp, U1 *sp_top, U1 *sp_bottom, S8 start ALIGN, S8 end ALIGN)
{
    S8 i ALIGN;
    S8 j ALIGN;
    S8 l ALIGN;
	S8 rj1 ALIGN;
	S8 rj2 ALIGN;
	S8 rj3 ALIGN;
	S8 rjnext ALIGN = 0;
    S8 r1 ALIGN;
    S8 r2 ALIGN;
    S8 r3 ALIGN;
    S2 offset;
    U1 label_created;
    S8 label ALIGN;
    U1 run_jit = 0;

	// for storing VM registers
	S8 jit_regs[6];		// R8 (0) to R11 (3)

	S8 jit_regsd[6]; 	// xmm0 (0) to xmm5 (5)
	S8 jit_regsd_max ALIGN = 5;

    CodeHolder jcode;                        // Create a CodeHolder.
    jcode.init(rt.getCodeInfo());            // Initialize it to be compatible with `rt`.

    X86Assembler a(&jcode);                  // Create and attach X86Assembler to `code`.

    X86Gp RSIback;

    a.mov (RSI, imm ((intptr_t)(void *) regi)); /* long registers base: rsi */

	a.mov (RDI, imm ((intptr_t)(void *) regd)); /* double registers base: rdi */

    RSIback = RSI;

    /* initialize label pos */

	for (i = 0; i < MAXJUMPLEN; i++)
	{
		JIT_label[i].pos = -1;
	}

	// init jit_regs for integer math
	// CPU register
	for (i = 0; i <= 5; i++)
	{
		jit_regs[i] = 0;  // 0 = is empty register
		jit_regsd[i] = 0;
	}

    i = start;
    do_jit:

    if (i <= end)
    {
		offset = 0;
		#if DEBUG
			printf ("opcode: %i\n", code[i]);
		#endif

        /* check if current opcode is on label */
		label_created = 0;

		for (j = 0; j < MAXJUMPLEN; j++)
		{
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
			// no offset found!!!
			printf ("FATAL error: JIT compiler: setting jump offset failed! opcode: %i\n", code[i]);
            return (1);
		}

		switch (code[i])
        {
            case ADDI:
			case SUBI:
			case MULI:
			case DIVI:
                r1 = code[i + 1];
                r2 = code[i + 2];
                r3 = code[i + 3];

				// Try to reduce a.mov opcodes by register checking.
				if (jit_regs[0] == 0 && jit_regs[1] == 0)
				{
					// reg 8 empty, reg 9 empty

					a.mov (R8, asmjit::x86::qword_ptr (RSI, OFFSET(r1))); /* r1v */
					a.mov (R9, asmjit::x86::qword_ptr (RSI, OFFSET(r2))); /* r2v */

					switch (code[i])
					{
						case ADDI:
							a.add (R8, R9);
							break;

                    	case SUBI:
							a.sub (R8, R9);
							break;

						case MULI:
							a.imul (R8, R9);
							break;

						case DIVI:
							a.mov (EAX, R8);
							a.xor_ (EDX, EDX);		/* clear the upper 64 bit */
							a.idiv (R9);
							a.mov (R8, EAX);
							break;
					}

					a.mov (asmjit::x86::qword_ptr (RSI, OFFSET(r3)), R8);
					jit_regs[0] = r3; jit_regs[1] = r2;
                	run_jit = 1;
					break;
				}
				else
				{
					if (jit_regs[2] == 0 && jit_regs[3] == 0)
					{
						// reg 10 empty, reg 11 empty

						a.mov (R10, asmjit::x86::qword_ptr (RSI, OFFSET(r1))); /* r1v */
						a.mov (R11, asmjit::x86::qword_ptr (RSI, OFFSET(r2))); /* r2v */

						switch (code[i])
						{
							case ADDI:
								a.add (R10, R11);
								break;

	                    	case SUBI:
								a.sub (R10, R11);
								break;

							case MULI:
								a.imul (R10, R11);
								break;

							case DIVI:
								a.mov (EAX, R10);
								a.xor_ (EDX, EDX);		/* clear the upper 64 bit */
								a.idiv (R11);
								a.mov (R10, EAX);
								break;
						}

						a.mov (asmjit::x86::qword_ptr (RSI, OFFSET(r3)), R10);
						jit_regs[2] = r3; jit_regs[3] = r2;
	                	run_jit = 1;
						break;
					}
					else
					{
						// check if VM register is already in CPU register
						if (jit_regs[0] == r3 && jit_regs[1] == r2)
						{
							a.mov (R10, asmjit::x86::qword_ptr (RSI, OFFSET(r1))); /* r1v */

							switch (code[i])
							{
								case ADDI:
									a.add (R10, R9);
									break;

	                    		case SUBI:
									a.sub (R10, R9);
									break;

								case MULI:
									a.imul (R10, R9);
									break;

								case DIVI:
									a.mov (EAX, R10);
									a.xor_ (EDX, EDX);		/* clear the upper 64 bit */
									a.idiv (R9);
									a.mov (R10, EAX);
									break;
							}

							a.mov (asmjit::x86::qword_ptr (RSI, OFFSET(r3)), R10);
							jit_regs[0] = r3; jit_regs[1] = r2;
	                		run_jit = 1;
							break;
						}
						if (jit_regs[2] == r3 && jit_regs[3] == r2)
						{
							a.mov (R8, asmjit::x86::qword_ptr (RSI, OFFSET(r1))); /* r1v */

							switch (code[i])
							{
								case ADDI:
									a.add (R8, R11);
									break;

	                    		case SUBI:
									a.sub (R8, R11);
									break;

								case MULI:
									a.imul (R8, R11);
									break;

								case DIVI:
									a.mov (EAX, R8);
									a.xor_ (EDX, EDX);		/* clear the upper 64 bit */
									a.idiv (R11);
									a.mov (R8, EAX);
									break;
							}

							a.mov (asmjit::x86::qword_ptr (RSI, OFFSET(r3)), R8);
							jit_regs[2] = r3; jit_regs[3] = r2;
	                		run_jit = 1;
							break;
						}
						else
						{
							// Use register 8 and 9. Reset registers 10 and 11.

							a.mov (R8, asmjit::x86::qword_ptr (RSI, OFFSET(r1))); /* r1v */
							a.mov (R9, asmjit::x86::qword_ptr (RSI, OFFSET(r2))); /* r2v */

							switch (code[i])
							{
								case ADDI:
									a.add (R8, R9);
									break;

		                    	case SUBI:
									a.sub (R8, R9);
									break;

								case MULI:
									a.imul (R8, R9);
									break;

								case DIVI:
									a.mov (EAX, R8);
									a.xor_ (EDX, EDX);		/* clear the upper 64 bit */
									a.idiv (R9);
									a.mov (R8, EAX);
									break;
							}

							a.mov (asmjit::x86::qword_ptr (RSI, OFFSET(r3)), R8);
							jit_regs[0] = r3; jit_regs[1] = r2;
							jit_regs[2] = 0; jit_regs[3] = 0;
		                	run_jit = 1;
							break;
						}
					}
				}
                break;

			case ADDD:
			case SUBD:
			case MULD:
			case DIVD:
				// double precision floating point using SSE
				r1 = code[i + 1];
				r2 = code[i + 2];
				r3 = code[i + 3];

				rj1 = 0; rj2 = 0; rj3 = 0;

				// search for r1 in CPU registers
				for (j = 0; j < jit_regsd_max; j++)
				{
					if (jit_regsd[i] == r1)
					{
						rj1 = i;
						break;
					}
				}

				// search for r2 in CPU registers
				for (j = 0; j < jit_regsd_max; j++)
				{
					if (jit_regsd[i] == r2)
					{
						rj2 = i;
						break;
					}
				}

				if (rj1 == 0 && rj2 == 0)
				{
					switch (rjnext)
					{
						case 0:
							// move to XMM registers
							a.movsd (asmjit::x86::xmm0, asmjit::x86::qword_ptr (RDI, OFFSET(r1)));
							a.movsd (asmjit::x86::xmm1, asmjit::x86::qword_ptr (RDI, OFFSET(r2)));

							switch (code[i])
							{
								case ADDD:
									a.addsd (asmjit::x86::xmm0, asmjit::x86::xmm1);
									break;

								case SUBD:
									a.subsd (asmjit::x86::xmm0, asmjit::x86::xmm1);
									break;

								case MULD:
									a.mulsd (asmjit::x86::xmm0, asmjit::x86::xmm1);
									break;

								case DIVD:
									a.divsd (asmjit::x86::xmm0, asmjit::x86::xmm1);
									break;
							}

							// move to destination register
							a.movsd (asmjit::x86::qword_ptr (RDI, OFFSET(r3)), asmjit::x86::xmm0);
							jit_regsd[0] = r3; jit_regsd[1] = r2;
							rjnext = 2;
							run_jit = 1;
							break;

						case 2:
							// move to XMM registers
							a.movsd (asmjit::x86::xmm2, asmjit::x86::qword_ptr (RDI, OFFSET(r1)));
							a.movsd (asmjit::x86::xmm3, asmjit::x86::qword_ptr (RDI, OFFSET(r2)));

							switch (code[i])
							{
								case ADDD:
									a.addsd (asmjit::x86::xmm2, asmjit::x86::xmm3);
									break;

								case SUBD:
									a.subsd (asmjit::x86::xmm2, asmjit::x86::xmm3);
									break;

								case MULD:
									a.mulsd (asmjit::x86::xmm2, asmjit::x86::xmm3);
									break;

								case DIVD:
									a.divsd (asmjit::x86::xmm2, asmjit::x86::xmm3);
									break;
							}

							// move to destination register
							a.movsd (asmjit::x86::qword_ptr (RDI, OFFSET(r3)), asmjit::x86::xmm2);
							jit_regsd[2] = r3; jit_regsd[3] = r2;
							rjnext = 4;
							run_jit = 1;
							break;

						case 4:
							// move to XMM registers
							a.movsd (asmjit::x86::xmm4, asmjit::x86::qword_ptr (RDI, OFFSET(r1)));
							a.movsd (asmjit::x86::xmm5, asmjit::x86::qword_ptr (RDI, OFFSET(r2)));

							switch (code[i])
							{
								case ADDD:
									a.addsd (asmjit::x86::xmm4, asmjit::x86::xmm5);
									break;

								case SUBD:
									a.subsd (asmjit::x86::xmm4, asmjit::x86::xmm5);
									break;

								case MULD:
									a.mulsd (asmjit::x86::xmm4, asmjit::x86::xmm5);
									break;

								case DIVD:
									a.divsd (asmjit::x86::xmm4, asmjit::x86::xmm5);
									break;
							}

							// move to destination register
							a.movsd (asmjit::x86::qword_ptr (RDI, OFFSET(r3)), asmjit::x86::xmm4);
							jit_regsd[4] = r3; jit_regsd[5] = r2;
							run_jit = 1; rjnext = 0;
							break;
					}
				}
				else
				{
					if (jit_regsd[rj1] == r3 && jit_regsd[rj2] == r2)
					{
						if (rj1 == 0)
						{
							// move to XMM registers
							a.movsd (asmjit::x86::xmm0, asmjit::x86::qword_ptr (RDI, OFFSET(r1)));

							switch (code[i])
							{
								case ADDD:
									a.addsd (asmjit::x86::xmm0, asmjit::x86::xmm1);
									break;

								case SUBD:
									a.subsd (asmjit::x86::xmm0, asmjit::x86::xmm1);
									break;

								case MULD:
									a.mulsd (asmjit::x86::xmm0, asmjit::x86::xmm1);
									break;

								case DIVD:
									a.divsd (asmjit::x86::xmm0, asmjit::x86::xmm1);
									break;
							}

							// move to destination register
							a.movsd (asmjit::x86::qword_ptr (RDI, OFFSET(r3)), asmjit::x86::xmm0);
							jit_regsd[0] = r3; jit_regsd[1] = r2;
							run_jit = 1;
						}
						if (rj1 == 2)
						{
							// move to XMM registers
							a.movsd (asmjit::x86::xmm2, asmjit::x86::qword_ptr (RDI, OFFSET(r1)));

							switch (code[i])
							{
								case ADDD:
									a.addsd (asmjit::x86::xmm2, asmjit::x86::xmm3);
									break;

								case SUBD:
									a.subsd (asmjit::x86::xmm2, asmjit::x86::xmm3);
									break;

								case MULD:
									a.mulsd (asmjit::x86::xmm2, asmjit::x86::xmm3);
									break;

								case DIVD:
									a.divsd (asmjit::x86::xmm2, asmjit::x86::xmm3);
									break;
							}

							// move to destination register
							a.movsd (asmjit::x86::qword_ptr (RDI, OFFSET(r3)), asmjit::x86::xmm2);
							jit_regsd[2] = r3; jit_regsd[3] = r2;
							run_jit = 1;
						}
						if (rj1 == 4)
						{
							// move to XMM registers
							a.movsd (asmjit::x86::xmm4, asmjit::x86::qword_ptr (RDI, OFFSET(r1)));

							switch (code[i])
							{
								case ADDD:
									a.addsd (asmjit::x86::xmm4, asmjit::x86::xmm5);
									break;

								case SUBD:
									a.subsd (asmjit::x86::xmm4, asmjit::x86::xmm5);
									break;

								case MULD:
									a.mulsd (asmjit::x86::xmm4, asmjit::x86::xmm5);
									break;

								case DIVD:
									a.divsd (asmjit::x86::xmm4, asmjit::x86::xmm5);
									break;
							}

							// move to destination register
							a.movsd (asmjit::x86::qword_ptr (RDI, OFFSET(r3)), asmjit::x86::xmm4);
							jit_regsd[4] = r3; jit_regsd[5] = r2;
							run_jit = 1;
						}
					}
				}
				break;


			case BANDI:
				r1 = code[i + 1];
				r2 = code[i + 2];
				r3 = code[i + 3];

				a.movq (asmjit::x86::mm0, asmjit::x86::qword_ptr (RSI, OFFSET(r1))); /* r1v */
				a.movq (asmjit::x86::mm1, asmjit::x86::qword_ptr (RSI, OFFSET(r2))); /* r2v */

				a.pand (asmjit::x86::mm0, asmjit::x86::mm1);

				a.movq (asmjit::x86::qword_ptr (RSI, OFFSET(r3)), asmjit::x86::mm0);

				run_jit = 1;
				break;

			case BORI:
				r1 = code[i + 1];
				r2 = code[i + 2];
				r3 = code[i + 3];

				a.movq (asmjit::x86::mm0, asmjit::x86::qword_ptr (RSI, OFFSET(r1))); /* r1v */
				a.movq (asmjit::x86::mm1, asmjit::x86::qword_ptr (RSI, OFFSET(r2))); /* r2v */

				a.por (asmjit::x86::mm0, asmjit::x86::mm1);

				a.movq (asmjit::x86::qword_ptr (RSI, OFFSET(r3)), asmjit::x86::mm0);

				run_jit = 1;
				break;

			case BXORI:
				r1 = code[i + 1];
				r2 = code[i + 2];
				r3 = code[i + 3];

				a.movq (asmjit::x86::mm0, asmjit::x86::qword_ptr (RSI, OFFSET(r1))); /* r1v */
				a.movq (asmjit::x86::mm1, asmjit::x86::qword_ptr (RSI, OFFSET(r2))); /* r2v */

				a.pxor (asmjit::x86::mm0, asmjit::x86::mm1);

				a.movq (asmjit::x86::qword_ptr (RSI, OFFSET(r3)), asmjit::x86::mm0);

				run_jit = 1;
				break;

			default:
                printf ("JIT compiler: UNKNOWN opcode: %i - exiting!\n", code[i]);
                return (1);
        }
        i = i + offset;
        goto do_jit;
    }

    if (run_jit)
    {
        a.ret ();		/* return to main program code */

		// printf ("JIT_code_ind: %lli\n", JIT_code_ind);

        if (JIT_code_ind < MAXJITCODE)
        {
            /* create JIT code function */

            JIT_code_ind++;

            Func funcptr;

            Error err = rt.add (&funcptr, &jcode);
            if (err == 1)
            {
                printf ("JIT compiler: code generation failed!\n");
                return (1);
            }

            JIT_code[JIT_code_ind].fn = (Func) funcptr;

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

extern "C" int run_jit (S8 code ALIGN)
{
	#if	 DEBUG
		printf ("run_jit: code: %li\n", code);
	#endif

	Func func = JIT_code[code].fn;

    #if DEBUG
        printf ("run_jit: code address: %lli\n", func);
    #endif

    if (func == NULL)
	{
		printf("JIT compiler: FATAL ERROR! NULL pointer code!!!\n");
		return (1);
	}

	/* call JIT code function, stored in JIT_code[] */
	//func ();
	JIT_code[code].fn();
	return (0);
}

extern "C" int free_jit_code ()
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
