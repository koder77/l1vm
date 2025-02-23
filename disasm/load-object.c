/*
 * This file load-object.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (info@midnight-coding.de), 2017
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

#include "../include/global.h"
#include "../include/opcodes.h"
#include "../include/home.h"

extern U1 *code;
extern U1 *data_global;
extern struct data_info data_info[MAXDATAINFO];
extern S8 data_info_ind ALIGN;

extern S8 data_size ALIGN;
extern S8 code_size ALIGN;
extern S8 data_mem_size ALIGN;
extern S8 stack_size ALIGN;

// see global.h user settings on top
extern S8 max_code_size ALIGN;
extern S8 max_data_size ALIGN;

// defined in main.c
// sets max number of CPU cores
extern S8 max_cpu;

extern U1 silent_run;

size_t strlen_safe (const char * str, S8 maxlen);

S2 conv_word (S2 val)
{
	S2 ret;

	U1 *valptr = (U1 *) &val;
	U1 *retptr = (U1 *) &ret;

#if MACHINE_BIG_ENDIAN
	*retptr = *valptr;
	retptr++; valptr++;
	*retptr = *valptr;
#else
	*retptr = valptr[1];
	retptr++;
	*retptr = valptr[0];
#endif

	return (ret);
}

S4 conv_doubleword (S4 val)
{
	S4 ret;

	U1 *valptr = (U1 *) &val;
	U1 *retptr = (U1 *) &ret;

	#if MACHINE_BIG_ENDIAN
	*retptr = *valptr;
	retptr++; valptr++;
	*retptr = *valptr;
	retptr++; valptr++;
	*retptr = *valptr;
	retptr++; valptr++;
	*retptr = *valptr;
	#else
	*retptr = valptr[3];
	retptr++;
	*retptr = valptr[2];
	retptr++;
	*retptr = valptr[1];
	retptr++;
	*retptr = valptr[0];
	#endif

	return (ret);
}

S8 conv_quadword (S8 val)
{
	S8 ret ALIGN;

	U1 *valptr = (U1 *) &val;
	U1 *retptr = (U1 *) &ret;

	#if MACHINE_BIG_ENDIAN
	*retptr = *valptr;
	retptr++; valptr++;
	*retptr = *valptr;
	retptr++; valptr++;
	*retptr = *valptr;
	retptr++; valptr++;
	*retptr = *valptr;
	retptr++; valptr++;
	*retptr = *valptr;
	retptr++; valptr++;
	*retptr = *valptr;
	retptr++; valptr++;
	*retptr = *valptr;
	retptr++; valptr++;
	*retptr = *valptr;
	#else
	*retptr = valptr[7];
	retptr++;
	*retptr = valptr[6];
	retptr++;
	*retptr = valptr[5];
	retptr++;
	*retptr = valptr[4];
	retptr++;
	*retptr = valptr[3];
	retptr++;
	*retptr = valptr[2];
	retptr++;
	*retptr = valptr[1];
	retptr++;
	*retptr = valptr[0];
	#endif

	return (ret);
}

F8 conv_double (S8 val)
{
	F8 ret ALIGN;

	U1 *valptr = (U1 *) &val;
	U1 *retptr = (U1 *) &ret;

	#if MACHINE_BIG_ENDIAN
	*retptr = *valptr;
	retptr++; valptr++;
	*retptr = *valptr;
	retptr++; valptr++;
	*retptr = *valptr;
	retptr++; valptr++;
	*retptr = *valptr;
	retptr++; valptr++;
	*retptr = *valptr;
	retptr++; valptr++;
	*retptr = *valptr;
	retptr++; valptr++;
	*retptr = *valptr;
	retptr++; valptr++;
	*retptr = *valptr;
	#else
	*retptr = valptr[7];
	retptr++;
	*retptr = valptr[6];
	retptr++;
	*retptr = valptr[5];
	retptr++;
	*retptr = valptr[4];
	retptr++;
	*retptr = valptr[3];
	retptr++;
	*retptr = valptr[2];
	retptr++;
	*retptr = valptr[1];
	retptr++;
	*retptr = valptr[0];
	#endif

	return (ret);
}

S2 load_object (U1 *name, U1 byte_char)
{
	FILE *fptr;
	U1 objname[512];
	U1 full_path[512];
	U1 run_shell[512];
	char *home;

	S4 slen;
	S4 sandbox_root_len;
	U1 object_packed = 0;
	U1 object_root = 0;

	S8 header ALIGN;

	S8 readsize ALIGN;

	U1 op;
	U1 byte;
	S2 word;
	S4 doubleword;
	S8 quadword ALIGN;

	// S8 offset ALIGN;

	U1 ok;
	S8 i ALIGN;
	S8 j ALIGN;
	S8 k ALIGN;

	U1 *data;   // local data pointer, points to data_global later

	U1 *bptr;

	// bzip compressed file flag
	U1 bzip2 = 0;

	// for byte array, string decode
	U1 decodestr[MAXSTRLEN];
	S8 decodestr_ind ALIGN = 0;

	slen = strlen_safe ((const char *) name, MAXLINELEN);
	if (slen > 506)
	{
		printf ("ERROR: filename too long!\n");
		return (1);
	}

	sandbox_root_len = strlen_safe ((const char *) name, MAXLINELEN);

	strcpy ((char *) objname, (const char *) name);
	strcat ((char *) objname, ".l1obj");

	strcpy ((char *) full_path, "");	// init full_path

	fptr = fopen ((const char *) objname, "rb");
	if (fptr == NULL)
	{
		// check if .bz2 compressed object archive?
		strcpy ((char *) objname, (const char *) name);
		strcat ((char *) objname, ".l1obj.bz2");

		fptr = fopen ((const char *) objname, "rb");
		if (fptr == NULL)
		{
			// check if program is in SANDBOX_ROOT directory
			if (sandbox_root_len + slen + 4 + 10 > 511)
			{
				printf ("ERROR: filename or sandbox root too long!\n");
				return (1);
			}

			home = get_home ();
			strcpy ((char *) full_path, (const char *) home);
			strcat ((char *) full_path, SANDBOX_ROOT);
			strcat ((char *) full_path, (const char *) "prog/");
			strcat ((char *) full_path, (const char *) name);
			strcat ((char *) full_path, ".l1obj");

			fptr = fopen ((const char *) full_path, "rb");
			if (fptr == NULL)
			{
				// check if .bz2 compressed object archive?
				strcat ((char *) full_path, ".bz2");

				fptr = fopen ((const char *) full_path, "rb");
				if (fptr == NULL)
				{
					strcpy ((char *) objname, (const char *) name);
					strcat ((char *) objname, ".l1obj");

					printf ("ERROR: can't open object file '%s'!\n", objname);
					printf ("Can't open an packed object file or object file in: '%s' !\n", full_path);
					return (1);
				}
				else
				{
					object_packed = 1;
					object_root = 1;
				}
			}
		}
		else
		{
			object_packed = 1;
		}

		if (object_packed == 1)
		{
			if (silent_run == 0)
			{
				printf ("loading packed object file...\n");
			}
			// close archive
			fclose (fptr);
			// unpack archive by running bzip2 unpack

			// bzip2 -d -k -f primes.l1obj.bz2  -c >/tmp/primes.l1obj

			strcpy ((char *) run_shell, "bzip2 -d -k -f ");
			if (object_root == 0)
			{
				strcat ((char *) run_shell, (const char *) objname);
			}
			else
			{
				// object file in sandbox_root/prog
				strcat ((char *) run_shell, (const char *) full_path);
			}

			// printf ("shell unpack: '%s'\n", run_shell

			if (system ((char *) run_shell) != 0)
			{
				printf ("load_object: ERROR: can't run bzip2 to unpack object code: '%s'!\n", objname);
				return (1);
			}

			if (object_root == 0)
			{
				strcpy ((char *) objname, (const char *) name);
				strcat ((char *) objname, ".l1obj");
			}
			else
			{
				home = get_home ();
				strcpy ((char *) full_path, (const char *) home);
				strcpy ((char *) objname, SANDBOX_ROOT);
				strcat ((char *) objname, (const char *) "prog/");
				strcat ((char *) objname, (const char *) name);
				strcat ((char *) objname, ".l1obj");
			}

			fptr = fopen ((const char *) objname, "rb");
			if (fptr == NULL)
			{
				printf ("ERROR: can't open object file '%s'!\n", objname);
				return (1);
			}

			bzip2 = 1;
		}
	}

	// check header
	readsize = fread (&quadword, sizeof (S8), 1, fptr);
	if (readsize != 1)
	{
		printf ("error: can't load header!\n");
		fclose (fptr);
		if (bzip2)
		{
			remove ((const char *) objname);
		}
		return (1);
	}

	header = conv_quadword (quadword);
	if (header != (S8) 0xC0DEBABE00002019)
	{
		printf ("ERROR: wrong header!\n");
		fclose (fptr);
		if (bzip2)
		{
			remove ((const char *) objname);
		}
		return (1);
	}

	// codesize
	readsize = fread (&quadword, sizeof (S8), 1, fptr);
	if (readsize != 1)
	{
		printf ("error: can't load codesize!\n");
		fclose (fptr);
		if (bzip2)
		{
			remove ((const char *) objname);
		}
		return (1);
	}

	code_size = conv_quadword (quadword);

	// printf ("codesize: %lli\n", code_size);
	// check if codesize in legal range
	if (code_size > max_code_size)
	{
		printf ("ERROR: code_size to big: %lli, must be less than: %lli!\n", code_size, max_code_size);
		fclose (fptr);
		if (bzip2)
		{
			remove ((const char *) objname);
		}
		return (1);
	}

	code = (U1 *) calloc (code_size, sizeof (U1));
	if (code == NULL)
	{
		printf ("ERROR: can't allocate %lli bytes for code!\n", code_size);
		fclose (fptr);
		if (bzip2)
		{
			remove ((const char *) objname);
		}
		return (1);
	}

	ok = 0; i = 16;

	printf (".code\n:main\n");

	while (! ok)
	{
		// opcode
		readsize = fread (&op, sizeof (U1), 1, fptr);
		if (readsize != 1)
		{
			printf ("error: can't load opcode!\n");
			fclose (fptr);
			if (bzip2)
			{
				remove ((const char *) objname);
			}
			return (1);
		}

		code[i] = op;

		printf ("%lli: %s ", i, opcode[op].op);
		if (op == RTS)
		{
			printf ("\n");
		}
		//printf ("offset: %lli OPCODE: %i\n", i, op);
		i++;

		for (j = 0; j < opcode[op].args; j++)
		{
			// printf ("argument type: %i\n", opcode[op].type[j]);
			switch (opcode[op].type[j])
			{
				case EMPTY:
					printf ("ERROR: EMPTY code argument type!\n");
					break;

				case I_REG:
				case D_REG:
					//printf ("LOAD CODE REG...\n");
					readsize = fread (&byte, sizeof (U1), 1, fptr);
					if (readsize != 1)
					{
						printf ("error: can't load opcode arg!\n");
						fclose (fptr);
						if (bzip2)
						{
							remove ((const char *) objname);
						}
						return (1);
					}

					printf ("%i", byte);

					if (j < opcode[op].args - 1)
					{
						printf (", ");
					}

					if (j == opcode[op].args - 1)
					{
						printf ("\n");
					}

					code[i] = byte;
					i++;
					break;

				case DATA:
				case DATA_OFFS:
				case LABEL:
					//printf ("LOAD CODE QUADWORD...\n");
					readsize = fread (&quadword, sizeof (S8), 1, fptr);
					if (readsize != 1)
					{
						printf ("error: can't load opcode arg!\n");
						fclose (fptr);
						if (bzip2)
						{
							remove ((const char *) objname);
						}
						return (1);
					}
					quadword = conv_quadword (quadword);

					bptr = (U1 *) &quadword;

					code[i] = *bptr;
					bptr++;
					code[i + 1] = *bptr;
					bptr++;
					code[i + 2] = *bptr;
					bptr++;
					code[i + 3] = *bptr;
					bptr++;
					code[i + 4] = *bptr;
					bptr++;
					code[i + 5] = *bptr;
					bptr++;
					code[i + 6] = *bptr;
					bptr++;
					code[i + 7] = *bptr;
					i = i + 8;

					printf ("%lli", quadword);

					if (j < opcode[op].args - 1)
					{
						printf (", ");
					}

					if (j == opcode[op].args - 1)
					{
						printf ("\n");
					}
					break;

				case ALL:
					readsize = fread (&byte, sizeof (U1), 1, fptr);
					if (readsize != 1)
					{
						printf ("error: can't load opcode arg!\n");
						fclose (fptr);
						if (bzip2)
						{
							remove ((const char *) objname);
						}
						return (1);
					}

					printf ("%i", byte);

					if (j < opcode[op].args - 1)
					{
						printf (", ");
					}

					if (j == opcode[op].args - 1)
					{
						printf ("\n");
					}

					code[i] = byte;
					i++;
					break;
			}
		}
		if (i > code_size - 1)
		{
			ok = 1;
		}
	}
	/*
	for (j = 0; j < i; j++)
	{
		printf ("%i\n", code[j]);
	}
	*/
	readsize = fread (&byte, sizeof (U1), 1, fptr);
	if (readsize != 1)
	{
		printf ("error: can't load info header!\n");
		if (bzip2)
		{
			remove ((const char *) objname);
		}
		fclose (fptr);
		return (1);
	}

	if (byte != 'i')
	{
		printf ("ERROR: wrong info header!\n");
		printf ("%i\n", byte);
		fclose (fptr);
		if (bzip2)
		{
			remove ((const char *) objname);
		}
		return (1);
	}

	readsize = fread (&byte, sizeof (U1), 1, fptr);
	if (readsize != 1)
	{
		printf ("error: can't load info header!\n");
		fclose (fptr);
		if (bzip2)
		{
			remove ((const char *) objname);
		}
		return (1);
	}
	if (byte != 'n')
	{
		printf ("ERROR: wrong info header!\n");
		fclose (fptr);
		if (bzip2)
		{
			remove ((const char *) objname);
		}
		return (1);
	}

	readsize = fread (&byte, sizeof (U1), 1, fptr);
	if (readsize != 1)
	{
		printf ("error: can't load info header!\n");
		fclose (fptr);
		if (bzip2)
		{
			remove ((const char *) objname);
		}
		return (1);
	}
	if (byte != 'f')
	{
		printf ("ERROR: wrong info header!\n");
		fclose (fptr);
		if (bzip2)
		{
			remove ((const char *) objname);
		}
		return (1);
	}

	readsize = fread (&byte, sizeof (U1), 1, fptr);
	if (readsize != 1)
	{
		printf ("error: can't load info header!\n");
		fclose (fptr);
		if (bzip2)
		{
			remove ((const char *) objname);
		}
		return (1);
	}
	if (byte != 'o')
	{
		printf ("ERROR: wrong info header!\n");
		fclose (fptr);
		if (bzip2)
		{
			remove ((const char *) objname);
		}
		return (1);
	}

	// data info
	ok = 0; data_mem_size = 0;
	while (! ok)
	{
		readsize = fread (&byte, sizeof (U1), 1, fptr);
		if (readsize != 1)
		{
			printf ("error: can't load data info!\n");
			fclose (fptr);
			if (bzip2)
			{
				remove ((const char *) objname);
			}
			return (1);
		}

		if (byte != 'd')
		{
			data_info_ind++;
			data_info[data_info_ind].type = byte;

			readsize = fread (&quadword, sizeof (S8), 1, fptr);
			if (readsize != 1)
			{
				printf ("error: can't load data info!\n");
				fclose (fptr);
				if (bzip2)
				{
					remove ((const char *) objname);
				}
				return (1);
			}
			quadword = conv_quadword (quadword);

			data_info[data_info_ind].size = quadword;
			data_mem_size += quadword;
		}
		else
		{
			ok = 1;
		}
	}

	// "ata"

	readsize = fread (&byte, sizeof (U1), 1, fptr);
	if (readsize != 1)
	{
		printf ("error: can't load data header!\n");
		fclose (fptr);
		if (bzip2)
		{
			remove ((const char *) objname);
		}
		return (1);
	}
	if (byte != 'a')
	{
		printf ("ERROR: wrong data header!\n");
		fclose (fptr);
		if (bzip2)
		{
			remove ((const char *) objname);
		}
		return (1);
	}

	readsize = fread (&byte, sizeof (U1), 1, fptr);
	if (readsize != 1)
	{
		printf ("error: can't load data header!\n");
		fclose (fptr);
		if (bzip2)
		{
			remove ((const char *) objname);
		}
		return (1);
	}
	if (byte != 't')
	{
		printf ("ERROR: wrong data header!\n");
		fclose (fptr);
		if (bzip2)
		{
			remove ((const char *) objname);
		}
		return (1);
	}

	readsize = fread (&byte, sizeof (U1), 1, fptr);
	if (readsize != 1)
	{
		printf ("error: can't data info header!\n");
		fclose (fptr);
		if (bzip2)
		{
			remove ((const char *) objname);
		}
		return (1);
	}
	if (byte != 'a')
	{
		printf ("ERROR: wrong data header!\n");
		fclose (fptr);
		if (bzip2)
		{
			remove ((const char *) objname);
		}
		return (1);
	}

	// data
	readsize = fread (&quadword, sizeof (S8), 1, fptr);
	if (readsize != 1)
	{
		printf ("error: can't load data: SIZE!\n");
		fclose (fptr);
		if (bzip2)
		{
			remove ((const char *) objname);
		}
		return (1);
	}

	data_size = conv_quadword (quadword);

	// printf ("data size: (data only) %lli\n", data_size);

	data_mem_size = data_mem_size + (stack_size * max_cpu);

	// check if datasize in legal range
	if (data_mem_size > max_data_size)
	{
		printf ("ERROR: data_mem_size to big: %lli, must be less than: %lli!\n", data_mem_size, max_data_size);
		fclose (fptr);
		if (bzip2)
		{
			remove ((const char *) objname);
		}
		return (1);
	}

	data_global = (U1 *) calloc (data_mem_size, sizeof (U1));
	if (data_global == NULL)
	{
		printf ("ERROR: can't allocate %lli bytes for data!\n", data_mem_size);
		fclose (fptr);
		if (bzip2)
		{
			remove ((const char *) objname);
		}
		return (1);
	}

	data = data_global;

    printf ("\n\n.data\n");

	i = 0;
	for (j = 0; j <= data_info_ind; j++)
	{
		// printf ("load_object: type: %i, start: %lli\n", data_info[j].type, i);

		switch (data_info[j].type)
		{
			case BYTE:
				//printf ("DATA BYTE\n");
				data_info[j].offset = i;
				decodestr_ind = 0;

				// printf ("load_object: BYTE: size: %lli\n", data_info[j].size);

				printf ("\nbyte offset: %lli, size: %lli\n", i, data_info[j].size);

				for (k = 1; k <= data_info[j].size; k++)
				{
					readsize = fread (&byte, sizeof (U1), 1, fptr);
					if (readsize != 1)
					{
						printf ("error: can't load data: BYTE!\n");
						fclose (fptr);
						if (bzip2)
						{
							remove ((const char *) objname);
						}
						return (1);
					}

					if (byte_char == 1)
					{
						// print byte value as char also
						printf ("%i '%c', ", byte, byte);
					}
					else
					{
						printf ("%i, ", byte);
					}

					if (decodestr_ind < MAXSTRLEN - 1)
					{
						decodestr[decodestr_ind] =  byte;
						decodestr_ind++;
					}

					data[i] = byte;
					i++;
				}
				data_info[j].end = i - 1;
				data_info[j].type_size = sizeof (U1);

				decodestr[decodestr_ind] = '\0'; // add binary zero as string end

				printf ("\n");

				if (byte_char == 1)
				{
					printf ("string: '%s'\n", decodestr); // print full string
				}

				break;

			case WORD:
				//printf ("DATA WORD\n");
				data_info[j].offset = i;

				printf ("\nint16 offset: %lli, size: %lli\n", i, data_info[j].size);

				for (k = 1; k <= (S8) (data_info[j].size / sizeof (S2)); k++)
				{
					readsize = fread (&word, sizeof (S2), 1, fptr);
					if (readsize != 1)
					{
						printf ("error: can't load data: WORD!\n");
						fclose (fptr);
						if (bzip2)
						{
							remove ((const char *) objname);
						}
						return (1);
					}

					word = conv_word (word);

					printf ("%i, ", word);

					bptr = (U1 *) &word;

					data[i] = *bptr;
					i++; bptr++;
					data[i] = *bptr;
					i++;
				}
				data_info[j].end = i - 1;
				data_info[j].type_size = sizeof (S2);

				printf ("\n");
				break;

			case DOUBLEWORD:
				//printf ("DATA DOUBLEWORD\n");
				data_info[j].offset = i;

				printf ("\nint32 offset: %lli, size: %lli\n", i, data_info[j].size);

				for (k = 1; k <= (S8) (data_info[j].size / sizeof (S4)); k++)
				{
					readsize = fread (&doubleword, sizeof (S4), 1, fptr);
					if (readsize != 1)
					{
						printf ("error: can't load data: DOUBLEWORD!\n");
						fclose (fptr);
						if (bzip2)
						{
							remove ((const char *) objname);
						}
						return (1);
					}

					doubleword = conv_doubleword (doubleword);

					printf ("%i ", doubleword);

					bptr = (U1* ) &doubleword;

					data[i] = *bptr;
					i++; bptr++;
					data[i] = *bptr;
					i++; bptr++;
					data[i] = *bptr;
					i++; bptr++;
					data[i] = *bptr;
					i++;
				}
				data_info[j].end = i - 1;
				data_info[j].type_size = sizeof (S4);

				printf ("\n");
				break;

			case QUADWORD:
			case DOUBLEFLOAT:

				if (data_info[j].type == QUADWORD)
				{
					printf ("\nint64 offset: %lli, size: %lli\n", i, data_info[j].size);
				}
				else
				{
					printf ("\ndouble offset: %lli, size: %lli\n", i, data_info[j].size);
				}

				{
					F8 quadword_d = 0.0;

				data_info[j].offset = i;
				for (k = 1; k <= (S8) (data_info[j].size / sizeof (S8)); k++)
				{
					readsize = fread (&quadword, sizeof (S8), 1, fptr);
					if (readsize != 1)
					{
						printf ("error: can't load data: QUADWORD | DOUBLEFLOAT! index: %lli\n", j);
						fclose (fptr);
						if (bzip2)
						{
							remove ((const char *) objname);
						}
						return (1);
					}

					if (data_info[j].type == QUADWORD)
					{
						quadword = conv_quadword (quadword);
						printf ("%lli, ", quadword);
					}
					else
					{
						quadword_d = conv_double (quadword);
						printf ("%lf, ", quadword_d);
					}

					bptr = (U1* ) &quadword;

					data[i] = *bptr;
					i++; bptr++;
					data[i] = *bptr;
					i++; bptr++;
					data[i] = *bptr;
					i++; bptr++;
					data[i] = *bptr;
					i++; bptr++;
					data[i] = *bptr;
					i++; bptr++;
					data[i] = *bptr;
					i++; bptr++;
					data[i] = *bptr;
					i++; bptr++;
					data[i] = *bptr;
					i++;
				}
				data_info[j].end = i - 1;
				data_info[j].type_size = sizeof (S8);

				printf ("\n");
				}
				break;
		}
	}
	fclose (fptr);
	if (bzip2)
	{
		remove ((const char *) objname);
	}
	return (0);
}
