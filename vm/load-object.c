/*
 * This file load-object.c is part of L1vm.
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

#include "../include/global.h"
#include "../include/opcodes.h"

extern U1 *code;
extern U1 *data;
struct data_info data_info[MAXDATAINFO];
extern S8 data_info_ind ALIGN;

extern S8 data_size ALIGN;
extern S8 code_size ALIGN;
extern S8 data_mem_size ALIGN;
extern S8 stack_size ALIGN;

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

S8 conv_quadword (S8 val ALIGN)
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

S2 load_object (U1 *name)
{
	FILE *fptr;
	U1 objname[512];

	S4 slen;

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

	U1 *bptr;

	slen = strlen ((const char *) name);
	if (slen > 506)
	{
		printf ("ERROR: filename too long!\n");
		return (1);
	}

	strcpy ((char *) objname, (const char *) name);
	strcat ((char *) objname, ".l1obj");

	fptr = fopen ((const char *) objname, "r");
	if (fptr == NULL)
	{
		printf ("ERROR: can't open object file '%s'!\n", objname);
		return (1);
	}

	// check header
	readsize = fread (&quadword, sizeof (S8), 1, fptr);
	if (readsize != 1)
	{
		printf ("error: can't load header!\n");
		fclose (fptr);
		return (1);
	}

	header = conv_quadword (quadword);
	if (header != 0xBADBABEAABBCCDDE)
	{
		printf ("ERROR: wrong header!\n");
		fclose (fptr);
		return (1);
	}

	// codesize
	readsize = fread (&quadword, sizeof (S8), 1, fptr);
	if (readsize != 1)
	{
		printf ("error: can't load codesize!\n");
		fclose (fptr);
		return (1);
	}

	code_size = conv_quadword (quadword);

	// printf ("codesize: %lli\n", code_size);

	code = (U1 *) calloc (code_size, sizeof (U1));
	if (code == NULL)
	{
		printf ("ERROR: can't allocate %lli bytes for code!\n", code_size);
		fclose (fptr);
		return (1);
	}

	ok = 0; i = 16;
	while (! ok)
	{
		// opcode
		readsize = fread (&op, sizeof (U1), 1, fptr);
		if (readsize != 1)
		{
			printf ("error: can't load opcode!\n");
			fclose (fptr);
			return (1);
		}

		code[i] = op;
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
						return (1);
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
					break;

				case ALL:
					readsize = fread (&byte, sizeof (U1), 1, fptr);
					if (readsize != 1)
					{
						printf ("error: can't load opcode arg!\n");
						return (1);
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
		fclose (fptr);
		return (1);
	}

	if (byte != 'i')
	{
		printf ("ERROR: wrong info header!\n");
		printf ("%i\n", byte);
		fclose (fptr);
		return (1);
	}

	readsize = fread (&byte, sizeof (U1), 1, fptr);
	if (readsize != 1)
	{
		printf ("error: can't load info header!\n");
		fclose (fptr);
		return (1);
	}
	if (byte != 'n')
	{
		printf ("ERROR: wrong info header!\n");
		fclose (fptr);
		return (1);
	}

	readsize = fread (&byte, sizeof (U1), 1, fptr);
	if (readsize != 1)
	{
		printf ("error: can't load info header!\n");
		fclose (fptr);
		return (1);
	}
	if (byte != 'f')
	{
		printf ("ERROR: wrong info header!\n");
		fclose (fptr);
		return (1);
	}

	readsize = fread (&byte, sizeof (U1), 1, fptr);
	if (readsize != 1)
	{
		printf ("error: can't load info header!\n");
		fclose (fptr);
		return (1);
	}
	if (byte != 'o')
	{
		printf ("ERROR: wrong info header!\n");
		fclose (fptr);
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
		return (1);
	}
	if (byte != 'a')
	{
		printf ("ERROR: wrong data header!\n");
		fclose (fptr);
		return (1);
	}

	readsize = fread (&byte, sizeof (U1), 1, fptr);
	if (readsize != 1)
	{
		printf ("error: can't load data header!\n");
		fclose (fptr);
		return (1);
	}
	if (byte != 't')
	{
		printf ("ERROR: wrong data header!\n");
		fclose (fptr);
		return (1);
	}

	readsize = fread (&byte, sizeof (U1), 1, fptr);
	if (readsize != 1)
	{
		printf ("error: can't data info header!\n");
		fclose (fptr);
		return (1);
	}
	if (byte != 'a')
	{
		printf ("ERROR: wrong data header!\n");
		fclose (fptr);
		return (1);
	}

	// data
	readsize = fread (&quadword, sizeof (S8), 1, fptr);
	if (readsize != 1)
	{
		printf ("error: can't load data: SIZE!\n");
		fclose (fptr);
		return (1);
	}

	data_size = conv_quadword (quadword);

	printf ("data size: (data only) %lli\n", data_size);

	data_mem_size = data_mem_size + (stack_size * MAXCPUCORES);
	data = (U1 *) calloc (data_mem_size, sizeof (U1));
	if (data == NULL)
	{
		printf ("ERROR: can't allocate %lli bytes for data!\n", data_mem_size);
		fclose (fptr);
		return (1);
	}

	i = 0;
	for (j = 0; j <= data_info_ind; j++)
	{
		switch (data_info[j].type)
		{
			case BYTE:
				//printf ("DATA BYTE\n");
				for (k = 1; k <= data_info[j].size; k++)
				{
					readsize = fread (&byte, sizeof (U1), 1, fptr);
					if (readsize != 1)
					{
						printf ("error: can't load data: BYTE!\n");
						fclose (fptr);
						return (1);
					}

					data[i] = byte;
					i++;
				}
				break;

			case WORD:
				//printf ("DATA WORD\n");
				for (k = 1; k <= (data_info[j].size / sizeof (S2)); k++)
				{
					readsize = fread (&word, sizeof (S2), 1, fptr);
					if (readsize != 1)
					{
						printf ("error: can't load data: WORD!\n");
						fclose (fptr);
						return (1);
					}

					word = conv_word (word);

					bptr = (U1 *) &word;

					data[i] = *bptr;
					i++; bptr++;
					data[i] = *bptr;
					i++;
				}
				break;

			case DOUBLEWORD:
				//printf ("DATA DOUBLEWORD\n");
				for (k = 1; k <= (data_info[j].size / sizeof (S4)); k++)
				{
					readsize = fread (&doubleword, sizeof (S4), 1, fptr);
					if (readsize != 1)
					{
						printf ("error: can't load data: DOUBLEWORD!\n");
						fclose (fptr);
						return (1);
					}

					doubleword = conv_doubleword (doubleword);

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
				break;

			case QUADWORD:
			case DOUBLEFLOAT:
				for (k = 1; k <= (data_info[j].size / sizeof (S8)); k++)
				{
					readsize = fread (&quadword, sizeof (S8), 1, fptr);
					if (readsize != 1)
					{
						printf ("error: can't load data: QUADWORD | DOUBLEFLOAT! index: %lli\n", j);
						fclose (fptr);
						return (1);
					}

					quadword = conv_quadword (quadword);

					//printf ("load_object: QUADWORD: %lli\n", quadword);

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
				break;
		}
	}
	fclose (fptr);
	return (0);
}
