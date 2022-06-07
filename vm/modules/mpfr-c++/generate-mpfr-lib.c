/*
 * This file generate-mpfr-lib.c is part of L1vm.
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

#include "../../../include/global.h"

#define MAX_FLOAT_NUM 256
#define MAXFUNC 256

#define MATH_FUNCTIONS		"// Math Functions"
#define FUNCTION_BEGIN		"friend const mpreal"
#define NUM_TYPE_CONST		"const"
#define NUM_TYPE_MPREAL		"mpreal&"
#define NUM_TYPE_RND		"mp_rnd_t"
#define NUM_TYPE_RND2		"rnd_mode"
#define LINE_ARGS			256

// functions already build in:
#define FUNCTION_HEAD			18

// input line splitted into arguments
U1 line_args[LINE_ARGS][MAXSTRLEN];
U1 num_names[LINE_ARGS][MAXSTRLEN];

U1 func_names[MAXFUNC][MAXSTRLEN];
S8 func_ind ALIGN = FUNCTION_HEAD;

// protos
// file input =================================================================
char *fgets_uni (char *str, int len, FILE *fptr);

// string functions ===========================================================
size_t strlen_safe (const char * str, int maxlen);
S2 searchstr (U1 *str, U1 *srchstr, S2 start, S2 end, U1 case_sens);
void convtabs (U1 *str);

S2 parse_line (U1 *line, S8 startpos)
{
	S8 pos, arg, arg_pos, len, arg_len, num_arg;
	U1 ok = 0, numtype_mpreal;
	S8 i ALIGN;

	len = strlen_safe ((const char *) line, MAXLINELEN);
	pos = 24;

	// get function name
	arg_pos = 0;

	printf ("\n\n'%s'\n", line);
	printf ("startpos: %lli\n", startpos);
	printf ("pos: %lli\n", pos);

	while (! ok)
	{
		//printf ("line[pos]: '%c'\n", line[pos]);
		if (line[pos] != ' ' && line[pos] != '(')
		{
			line_args[0][arg_pos] = line[pos];
			// printf ("%c", line_args[0][arg_pos]);
			arg_pos++;
		}
		else
		{
			line_args[0][arg_pos] = '\0';		// set arg string end
			ok = 1;
		}
		pos++;
	}
	printf ("parse_line: got function: '%s'\n", line_args[0]);

	// get function arguments
	ok = 0; arg = 1; arg_pos = 0;
	while (! ok)
	{
		if (line[pos] != ' ' && line[pos] != '(' && line[pos] != ')' && line[pos] != ',')
		{
			line_args[arg][arg_pos] = line[pos];
			// printf ("%c", line_args[0][arg_pos]);
			arg_pos++;
		}
		else
		{
			line_args[arg][arg_pos] = '\0';		// set arg string end
			arg++; arg_pos = 0;
			if (arg >= LINE_ARGS)
			{
				printf ("parse_line: ERROR: too many arguments!\n");
				return (-1);
			}
		}
		if (pos < len - 1)
		{
			pos++;
		}
		else
		{
			ok = 1;
		}
	}
	printf ("parse_line args:\n");
	for (i = 0; i <= arg; i++)
	{
		printf ("%lli: '%s'\n", i, line_args[i]);
	}

	// check variable types
	numtype_mpreal = 0, num_arg = 0;
	for (i = 2; i <= arg - 1; i++)
	{
		arg_len = strlen_safe ((const char *) line_args[i], MAXLINELEN);
		if (arg_len != 0)
		{
			numtype_mpreal = 0;
			if (strcmp ((const char *) line_args[i], NUM_TYPE_CONST) == 0)
			{
				numtype_mpreal = 1;
			}

			if (strcmp ((const char *) line_args[i], NUM_TYPE_RND) == 0)
			{
				numtype_mpreal = 1;
			}

			if (strcmp ((const char *) line_args[i], NUM_TYPE_RND2) == 0)
			{
				numtype_mpreal = 1;
			}

			if (strcmp ((const char *) line_args[i], NUM_TYPE_MPREAL) == 0)
			{
				// get next argument as variable name
				strcpy ((char *) num_names[num_arg], (const char *) line_args[i + 1]);

				printf ("parse_line: number name: '%s'\n", num_names[num_arg]);
				num_arg++;
				if (num_arg >= LINE_ARGS)
				{
					printf ("parse_line: ERROR: too many arguments!\n");
					return (-1);
				}
				numtype_mpreal = 1;
				i++;
			}

			// check if legal types
			if (numtype_mpreal == 0)
			{
				// not mpreal type only
				printf ("parse_line: no mpreal only function! EXIT\n");
				return (-1);
			}
		}

	}
	return (num_arg);
}

S2 generate_init_vm (FILE *vm_file, S8 max_vars)
{
	if (fprintf (vm_file, "(set string s %sstr@gmp \"mp_%s_float\")\n", line_args[0], line_args[0]) < 0)
	{
		printf ("generate_vm: error writing function begin!\n");
		return (1);
	}

	if (fprintf (vm_file, "(set int64 1 %s@gmp %lli)\n", line_args[0], func_ind) < 0)
	{
		printf ("generate_vm: error writing function begin!\n");
		return (1);
	}
	return (0);
}

S2 generate_main_vm (FILE *vm_file)
{
	S8 i ALIGN;

	for (i = FUNCTION_HEAD; i < func_ind; i++)
	{
		if (fprintf (vm_file, "(2 mod@gmp %s@gmp %sstr@gmpaddr intr0)\n", func_names[i], func_names[i]) < 0)
		{
			printf ("generate_vm: error writing function table!\n");
			return (1);
		}
	}
	if (fprintf (vm_file, "(funcend)\n") < 0)
	{
		printf ("generate_vm: error writing function table!\n");
		return (1);
	}

	// write wrapper functions

	for (i = FUNCTION_HEAD; i < func_ind; i++)
	{
		if (fprintf (vm_file, "(mp_%s_float func)\n", func_names[i]) < 0)
		{
			printf ("generate_vm: error writing function table!\n");
			return (1);
		}

		if (fprintf (vm_file, "(3 mod@gmp %s@gmp 0 intr0)\n(funcend)\n", func_names[i]) < 0)
		{
			printf ("generate_vm: error writing function table!\n");
			return (1);
		}
	}
	return (0);
}

S2 generate_c (FILE *c_file, S8 max_vars)
{
	S8 i ALIGN;

	// already defined in handwritten part...
	if (strcmp ((const char *) line_args[0], "add") == 0)
	{
		return (2);
	}
	if (strcmp ((const char *) line_args[0], "sub") == 0)
	{
		return (2);
	}
	if (strcmp ((const char *) line_args[0], "mul") == 0)
	{
		return (2);
	}
	if (strcmp ((const char *) line_args[0], "div") == 0)
	{
		return (2);
	}

	// start writing C code...
	if (fprintf (c_file, "extern \"C\" U1 *mp_%s_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)\n{", line_args[0]) < 0)
	{
		printf ("generate_c: error writing C function begin!\n");
		return (1);
	}
	if (fprintf (c_file, "\nS8 float_index_res ALIGN;\n") < 0)
	{
		printf ("generate_c: error writing C function begin!\n");
		return (1);
	}

	if (fprintf (c_file, "\nsp = stpopi ((U1 *) &float_index_res, sp, sp_top);\nif (sp == NULL)\n{\nprintf (\"gmp_%s_float: ERROR: stack corrupt!\\n\");\nreturn (NULL);\n}\n", line_args[0]) < 0)
	{
		printf ("generate_c: error writing C function stpopi float result index!\n");
		return (1);
	}

	if (fprintf (c_file, "\nif (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)\n{\nprintf (\"gmp_%s_float: ERROR float index result out of range! Must be 0 < %%i\", MAX_FLOAT_NUM);\nreturn (NULL);\n}\n\n", line_args[0]) < 0)
	{
		printf ("generate_c: error writing C function check float index overflow!\n");
		return (1);
	}

	// write stpopi blocks
	for (i = max_vars - 1; i >= 0; i--)
	{
		if (fprintf (c_file, "\nS8 %s ALIGN;\n", num_names[i]) < 0)
		{
			printf ("generate_c: error writing C function var stpopi!\n");
			return (1);
		}

		if (fprintf (c_file, "\nsp = stpopi ((U1 *) &%s, sp, sp_top);\nif (sp == NULL)\n", num_names[i]) < 0)
		{
			printf ("generate_c: error writing C function stpopi float result index!\n");
			return (1);
		}

		if (fprintf (c_file, "{\nprintf (\"gmp_%s_float: ERROR: stack corrupt!\\n\");\nreturn (NULL);\n}\n", line_args[0]) < 0)
		{
			printf ("generate_c: error writing C function stpopi float result index!\n");
			return (1);
		}

		if (fprintf (c_file, "\nif (%s >= MAX_FLOAT_NUM || %s < 0)\n{\nprintf (\"gmp_%s_float: ERROR float index %s out of range! Must be 0 < %%i\", MAX_FLOAT_NUM);\nreturn (NULL);\n}\n\n", num_names[i], num_names[i], line_args[0], num_names[i]) < 0)
		{
			printf ("generate_c: error writing C function check float index overflow!\n");
			return (1);
		}
	}

	if (fprintf (c_file, "\nmpf_float[float_index_res] = %s (", line_args[0]) < 0)
	{
		printf ("generate_c: error writing C function calculation!\n");
		return (1);
	}

	for (i = 0; i < max_vars; i++)
	{
		if (fprintf (c_file, "mpf_float[%s]", num_names[i]) < 0)
		{
			printf ("generate_c: error writing C function calculation!\n");
			return (1);
		}
		if (max_vars > 1 && i < max_vars - 1)
		{
			if (fprintf (c_file, ",") < 0)
			{
				printf ("generate_c: error writing C function calculation!\n");
				return (1);
			}
		}
	}
	if (fprintf (c_file, ");\nreturn (sp);\n}\n\n") < 0)
	{
		printf ("generate_c: error writing C function end!\n");
		return (1);
	}

	// end of function mark:
	if (fprintf (c_file, "// ===============================================================================\n\n") < 0)
	{
		printf ("generate_c: error writing C function end mark!\n");
		return (1);
	}

	return (0);
}

int main (int ac, char *av[])
{
	FILE *mpreal_h;
	FILE *mpreal_c;
	FILE *mpreal_vm;

	U1 mpreal_h_line[MAXSTRLEN];

	U1 ok;
	S8 mpreal_h_linenum ALIGN = 0;
	S8 mpreal_h_line_len ALIGN;
	S8 pos ALIGN;
	S8 max_functions ALIGN = 0;
	S8 max_vars ALIGN;
	char *read;

	mpreal_h = fopen ("mpreal.h", "r");
	if (mpreal_h == NULL)
	{
		printf ("ERROR opening 'mpreal.h'\n");
		exit (1);
	}

	mpreal_c = fopen ("mpfr.cpp", "w");
	if (mpreal_c == NULL)
	{
		printf ("ERROR opening 'mpfr.cpp'\n");
		fclose (mpreal_h);
		exit (1);
	}

	// write #include "mpfr-head.cpp"
	if (fprintf (mpreal_c, "#include \"mpfr-head.cpp\"\n\n") < 0)
	{
		printf ("generate_c: error writing C #include\n");
		fclose (mpreal_h);
		fclose (mpreal_c);
		return (1);
	}

	mpreal_vm = fopen ("mpfr-lib.l1com", "w");
	if (mpreal_vm == NULL)
	{
		printf ("ERROR opening 'mpfr-lib.l1com'\n");
		fclose (mpreal_h);
		fclose (mpreal_c);
		exit (1);
	}
	// get mpreal.h '// Math Functions' line and start from there
	ok = 0;
	while (! ok)
	{
		read = fgets_uni ((char *) mpreal_h_line, MAXLINELEN, mpreal_h);
        if (read != NULL)
        {
			mpreal_h_linenum++;
            convtabs (mpreal_h_line);                    /* convert the funny tabs into spaces! */
            mpreal_h_line_len = strlen_safe ((const char *) mpreal_h_line, MAXLINELEN);

			pos = searchstr (mpreal_h_line, (U1 *) MATH_FUNCTIONS, 0, 0, TRUE);
            if (pos != -1)
            {
				ok = 1;
				printf ("found '%s' line: %lli, parsing file...\n", MATH_FUNCTIONS, mpreal_h_linenum);
			}
		}
	}

	ok = 0;
	while (! ok)
	{
		read = fgets_uni ((char *) mpreal_h_line, MAXLINELEN, mpreal_h);
        if (read == NULL)
		{
			ok = 1;
			break;
		}
		mpreal_h_linenum++;
		convtabs (mpreal_h_line);

		pos = searchstr (mpreal_h_line, (U1 *) FUNCTION_BEGIN, 0, 0, TRUE);
		if (pos != -1)
		{
			max_vars = parse_line (mpreal_h_line, pos);
			if (max_vars < 0)
			{
				printf ("parse_line: got skipp function.\n");
			}
			else
			{
				// generate code...

				// save function name
				if (func_ind < MAXFUNC)
				{
					strcpy ((char *) func_names[func_ind], (const char *) line_args[0]);
					func_ind++;
				}
				else
				{
					printf ("ERROR: func_names list overflow!\n");
					fclose (mpreal_vm);
					fclose (mpreal_c);
					fclose (mpreal_h);
					exit (1);
				}

				printf ("generating code...\n");
				if (generate_c (mpreal_c, max_vars) != 0)
				{
					printf ("ERROR: can't generate c code file!\n");
					fclose (mpreal_vm);
					fclose (mpreal_c);
					fclose (mpreal_h);
					exit (1);
				}
				if (generate_init_vm (mpreal_vm, max_vars) != 0)
				{
					printf ("ERROR: can't generate c code file!\n");
					fclose (mpreal_vm);
					fclose (mpreal_c);
					fclose (mpreal_h);
					exit (1);
				}

				max_functions++;
			}
			// printf ("found '%s' line: %lli, parsing file...\n", MATH_FUNCTIONS, mpreal_h_linenum);
		}
	}

	if (generate_main_vm (mpreal_vm) != 0)
	{
		printf ("ERROR: can't generate vm code file!\n");
		fclose (mpreal_vm);
		fclose (mpreal_c);
		fclose (mpreal_h);
		exit (1);
	}

	printf ("\n\nfunctions generated: %lli\n", max_functions);
	fclose (mpreal_vm);
	fclose (mpreal_c);
	fclose (mpreal_h);
	exit (0);
}
