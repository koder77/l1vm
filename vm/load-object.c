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

#include <libbz3.h>

#define BYTE_BUF 4096  // byte variable read buffer

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

size_t strlen_safe (const char * str, S8  maxlen);

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

// object code stream: reads from a file or from a RAM buffer
// =========================================================
static FILE *obj_fptr = NULL;
static U1 *obj_mem_base = NULL;
static U1 *obj_memptr = NULL;
static S8 obj_mem_remaining = 0;

static void obj_stream_open_file (FILE *fptr)
{
	obj_fptr = fptr;
	obj_mem_base = NULL;
	obj_memptr = NULL;
	obj_mem_remaining = 0;
}

static void obj_stream_open_mem (U1 *buffer, S8 size)
{
	obj_fptr = NULL;
	obj_mem_base = buffer;
	obj_memptr = buffer;
	obj_mem_remaining = size;
}

// reads like fread () from the file or from the RAM buffer
static size_t obj_stream_read (void *ptr, size_t size, size_t nmemb)
{
	if (obj_memptr != NULL)
	{
		size_t bytes = size * nmemb;

		if ((S8) bytes > obj_mem_remaining)
		{
			return (0);
		}

		memcpy (ptr, obj_memptr, bytes);
		obj_memptr += bytes;
		obj_mem_remaining -= bytes;
		return (nmemb);
	}

	return (fread (ptr, size, nmemb, obj_fptr));
}

// closes the file or frees the RAM buffer
static void obj_stream_close (void)
{
	if (obj_mem_base != NULL)
	{
		free (obj_mem_base);
		obj_mem_base = NULL;
		obj_memptr = NULL;
		obj_mem_remaining = 0;
	}
	if (obj_fptr != NULL)
	{
		fclose (obj_fptr);
		obj_fptr = NULL;
	}
}

// read a little endian 32 bit integer from the buffer
static S4 bz3_read_s32 (const U1 *buffer)
{
	return ((S4) buffer[0] | ((S4) buffer[1] << 8) | ((S4) buffer[2] << 16) | ((S4) buffer[3] << 24));
}

// load the uncompressed data of a bzip3 packed file into RAM
// returns 0 on success, 1 on error
// on success *outbuff points to the malloc'd uncompressed data and *outsize holds its size
S2 bzip3_uncompress_file (const U1 *filename, U1 **outbuff, S8 *outsize)
{
	FILE *fptr;
	U1 *inbuff = NULL;	// compressed data in RAM
	U1 *unbuff = NULL;	// uncompressed data in RAM
	U1 *block_buffer = NULL;
	S8 insize = 0;
	S8 unbsize = 0;
	S8 pos = 0;
	S8 outpos = 0;
	S4 block_size = 0;
	S4 blocksize = 0;
	S4 origsize = 0;
	struct bz3_state *state = NULL;
	size_t block_buf_size = 0;
	size_t readsize = 0;

	fptr = fopen ((const char *) filename, "rb");
	if (fptr == NULL)
	{
		printf ("bzip3_uncompress_file: ERROR: can't open file: '%s'!\n", filename);
		return (1);
	}

	// get compressed file size
	if (fseek (fptr, 0, SEEK_END) != 0)
	{
		printf ("bzip3_uncompress_file: ERROR: can't seek file: '%s'!\n", filename);
		fclose (fptr);
		return (1);
	}
	insize = ftell (fptr);
	rewind (fptr);

	if (insize < 17)
	{
		printf ("bzip3_uncompress_file: ERROR: file to small: '%s'!\n", filename);
		fclose (fptr);
		return (1);
	}

	inbuff = (U1 *) malloc ((size_t) insize);
	if (inbuff == NULL)
	{
		printf ("bzip3_uncompress_file: ERROR: can't allocate %lli bytes for compressed data!\n", insize);
		fclose (fptr);
		return (1);
	}

	readsize = fread (inbuff, sizeof (U1), (size_t) insize, fptr);
	fclose (fptr);
	if (readsize != (size_t) insize)
	{
		printf ("bzip3_uncompress_file: ERROR: can't read file: '%s'!\n", filename);
		free (inbuff);
		return (1);
	}

	// check bzip3 signature
	if (memcmp (inbuff, "BZ3v1", 5) != 0)
	{
		printf ("bzip3_uncompress_file: ERROR: no bzip3 signature in file: '%s'!\n", filename);
		free (inbuff);
		return (1);
	}

	block_size = bz3_read_s32 ((const U1 *) (inbuff + 5));
	if (block_size < 65 * 1024 || block_size > 511 * 1024 * 1024)
	{
		printf ("bzip3_uncompress_file: ERROR: invalid bzip3 block size: %i in file: '%s'!\n", block_size, filename);
		free (inbuff);
		return (1);
	}

	// first pass: walk through the blocks and sum up the uncompressed size
	pos = 9;
	while (pos < insize)
	{
		if (insize - pos < 8)
		{
			printf ("bzip3_uncompress_file: ERROR: truncated block header in file: '%s'!\n", filename);
			free (inbuff);
			return (1);
		}

		blocksize = bz3_read_s32 ((const U1 *) (inbuff + pos));
		origsize = bz3_read_s32 ((const U1 *) (inbuff + pos + 4));
		pos += 8;

		if (blocksize < 0 || origsize < 0 || (size_t) blocksize > bz3_bound (block_size) || (size_t) origsize > bz3_bound (block_size))
		{
			printf ("bzip3_uncompress_file: ERROR: invalid block sizes in file: '%s'!\n", filename);
			free (inbuff);
			return (1);
		}

		if (insize - pos < blocksize)
		{
			printf ("bzip3_uncompress_file: ERROR: truncated block data in file: '%s'!\n", filename);
			free (inbuff);
			return (1);
		}
		pos += blocksize;
		unbsize += origsize;
	}

	unbuff = (U1 *) malloc ((size_t) unbsize);
	if (unbuff == NULL)
	{
		printf ("bzip3_uncompress_file: ERROR: can't allocate %lli bytes for uncompressed data!\n", unbsize);
		free (inbuff);
		return (1);
	}

	state = bz3_new (block_size);
	if (state == NULL)
	{
		printf ("bzip3_uncompress_file: ERROR: can't create bzip3 state!\n");
		free (inbuff);
		free (unbuff);
		return (1);
	}

	block_buf_size = bz3_bound (block_size);
	block_buffer = (U1 *) malloc ((size_t) block_buf_size);
	if (block_buffer == NULL)
	{
		printf ("bzip3_uncompress_file: ERROR: can't allocate block buffer!\n");
		bz3_free (state);
		free (inbuff);
		free (unbuff);
		return (1);
	}

	// second pass: decode all blocks
	pos = 9;
	outpos = 0;
	while (pos < insize)
	{
		blocksize = bz3_read_s32 ((const U1 *) (inbuff + pos));
		origsize = bz3_read_s32 ((const U1 *) (inbuff + pos + 4));
		pos += 8;

		memcpy (block_buffer, inbuff + pos, (size_t) blocksize);
		pos += blocksize;

		if (bz3_decode_block (state, block_buffer, block_buf_size, blocksize, origsize) == -1)
		{
			printf ("bzip3_uncompress_file: ERROR: can't decode block: %s!\n", bz3_strerror (state));
			bz3_free (state);
			free (block_buffer);
			free (inbuff);
			free (unbuff);
			return (1);
		}

		memcpy (unbuff + outpos, block_buffer, (size_t) origsize);
		outpos += origsize;
	}

	bz3_free (state);
	free (block_buffer);
	free (inbuff);

	*outbuff = unbuff;
	*outsize = unbsize;
	return (0);
}

S2 load_object (U1 *name, S2 load_code_only)
{
	FILE *fptr;
	U1 objname[512];
	U1 full_path[512];
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

	// RAM buffer with the decompressed object code, used for packed objects
	U1 *objbuffer = NULL;
	S8 objsize = 0;

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
		// check if .bz3 compressed object archive?
		strcpy ((char *) objname, (const char *) name);
		strcat ((char *) objname, ".l1obj.bz3");

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
				strcat ((char *) full_path, ".bz3");

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
			// close the .bz3 archive file
			fclose (fptr);

			// decompress the packed object code into RAM
			if (object_root == 0)
			{
				if (bzip3_uncompress_file ((const U1 *) objname, &objbuffer, &objsize) != 0)
				{
					printf ("load_object: ERROR: can't decompress object code: '%s'!\n", objname);
					return (1);
				}
			}
			else
			{
				// object file in sandbox_root/prog
				if (bzip3_uncompress_file ((const U1 *) full_path, &objbuffer, &objsize) != 0)
				{
					printf ("load_object: ERROR: can't decompress object code: '%s'!\n", full_path);
					return (1);
				}
			}

			if (silent_run == 0)
			{
				printf ("decompressed %lli bytes object code into RAM\n", objsize);
			}

			obj_stream_open_mem (objbuffer, objsize);
		}
		else
		{
			// object code is not packed, read it from the opened file
			obj_stream_open_file (fptr);
		}
	}

	// object code is not packed, read it from the opened file
	if (object_packed == 0)
	{
		obj_stream_open_file (fptr);
	}

	// check header
	readsize = obj_stream_read (&quadword, sizeof (S8), 1);
	if (readsize != 1)
	{
		printf ("error: can't load header!\n");
		obj_stream_close ();
		return (1);
	}

	header = conv_quadword (quadword);
	if (header != (S8) 0xC0DEBABE00002019)
	{
		printf ("ERROR: wrong header!\n");
		obj_stream_close ();
		return (1);
	}

	// codesize
	readsize = obj_stream_read (&quadword, sizeof (S8), 1);
	if (readsize != 1)
	{
		printf ("error: can't load codesize!\n");
		obj_stream_close ();
		return (1);
	}

	code_size = conv_quadword (quadword);

	// printf ("codesize: %lli\n", code_size);
	// check if codesize in legal range
	if (max_code_size > 0)
	{
		if (code_size > max_code_size)
		{
			printf ("ERROR: code_size to big: %lli, must be less than: %lli!\n", code_size, max_code_size);
			obj_stream_close ();
			return (1);
		}
	}

	code = (U1 *) calloc (code_size, sizeof (U1));
	if (code == NULL)
	{
		printf ("ERROR: can't allocate %lli bytes for code!\n", code_size);
		obj_stream_close ();
		return (1);
	}

	ok = 0; i = 16;
	while (! ok)
	{
		// opcode
		readsize = obj_stream_read (&op, sizeof (U1), 1);
		if (readsize != 1)
		{
			printf ("error: can't load opcode!\n");
			obj_stream_close ();
			return (1);
		}

		if (op >= MAXOPCODES)
		{
			printf ("error: illegal opcode!\n");
			obj_stream_close ();
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
					readsize = obj_stream_read (&byte, sizeof (U1), 1);
					if (readsize != 1)
					{
						printf ("error: can't load opcode arg!\n");
						obj_stream_close ();
						return (1);
					}
					code[i] = byte;
					i++;
					break;

				case DATA:
				case DATA_OFFS:
				case LABEL:
					//printf ("LOAD CODE QUADWORD...\n");
					readsize = obj_stream_read (&quadword, sizeof (S8), 1);
					if (readsize != 1)
					{
						printf ("error: can't load opcode arg!\n");
						obj_stream_close ();
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
					readsize = obj_stream_read (&byte, sizeof (U1), 1);
					if (readsize != 1)
					{
						printf ("error: can't load opcode arg!\n");
						obj_stream_close ();
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

	if (load_code_only == 1)
	{
		// leave data as is
		// load new code only
		obj_stream_close ();
		return (0);
	}

	/*
	for (j = 0; j < i; j++)
	{
		printf ("%i\n", code[j]);
	}
	*/
	readsize = obj_stream_read (&byte, sizeof (U1), 1);
	if (readsize != 1)
	{
		printf ("error: can't load info header!\n");
		obj_stream_close ();
		return (1);
	}

	if (byte != 'i')
	{
		printf ("ERROR: wrong info header!\n");
		printf ("%i\n", byte);
		obj_stream_close ();
		return (1);
	}

	readsize = obj_stream_read (&byte, sizeof (U1), 1);
	if (readsize != 1)
	{
		printf ("error: can't load info header!\n");
		obj_stream_close ();
		return (1);
	}
	if (byte != 'n')
	{
		printf ("ERROR: wrong info header!\n");
		obj_stream_close ();
		return (1);
	}

	readsize = obj_stream_read (&byte, sizeof (U1), 1);
	if (readsize != 1)
	{
		printf ("error: can't load info header!\n");
		obj_stream_close ();
		return (1);
	}
	if (byte != 'f')
	{
		printf ("ERROR: wrong info header!\n");
		obj_stream_close ();
		return (1);
	}

	readsize = obj_stream_read (&byte, sizeof (U1), 1);
	if (readsize != 1)
	{
		printf ("error: can't load info header!\n");
		obj_stream_close ();
		return (1);
	}
	if (byte != 'o')
	{
		printf ("ERROR: wrong info header!\n");
		obj_stream_close ();
		return (1);
	}

	// data info
	ok = 0; data_mem_size = 0;
	while (! ok)
	{
		readsize = obj_stream_read (&byte, sizeof (U1), 1);
		if (readsize != 1)
		{
			printf ("error: can't load data info!\n");
			obj_stream_close ();
			return (1);
		}

		if (byte != 'd')
		{
			data_info_ind++;
			data_info[data_info_ind].type = byte;

			readsize = obj_stream_read (&quadword, sizeof (S8), 1);
			if (readsize != 1)
			{
				printf ("error: can't load data info!\n");
				obj_stream_close ();
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

	readsize = obj_stream_read (&byte, sizeof (U1), 1);
	if (readsize != 1)
	{
		printf ("error: can't load data header!\n");
		obj_stream_close ();
		return (1);
	}
	if (byte != 'a')
	{
		printf ("ERROR: wrong data header!\n");
		obj_stream_close ();
		return (1);
	}

	readsize = obj_stream_read (&byte, sizeof (U1), 1);
	if (readsize != 1)
	{
		printf ("error: can't load data header!\n");
		obj_stream_close ();
		return (1);
	}
	if (byte != 't')
	{
		printf ("ERROR: wrong data header!\n");
		obj_stream_close ();
		return (1);
	}

	readsize = obj_stream_read (&byte, sizeof (U1), 1);
	if (readsize != 1)
	{
		printf ("error: can't data info header!\n");
		obj_stream_close ();
		return (1);
	}
	if (byte != 'a')
	{
		printf ("ERROR: wrong data header!\n");
		obj_stream_close ();
		return (1);
	}

	// data
	readsize = obj_stream_read (&quadword, sizeof (S8), 1);
	if (readsize != 1)
	{
		printf ("error: can't load data: SIZE!\n");
		obj_stream_close ();
		return (1);
	}

	data_size = conv_quadword (quadword);

	// printf ("data size: (data only) %lli\n", data_size);

	data_mem_size = data_mem_size + (stack_size * max_cpu);

	// check if datasize in legal range
	if (max_data_size > 0)
	{
		if (data_mem_size > max_data_size)
		{
			printf ("ERROR: data_mem_size to big: %lli, must be less than: %lli!\n", data_mem_size, max_data_size);
			obj_stream_close ();
			return (1);
		}
	}

	data_global = (U1 *) calloc (data_mem_size, sizeof (U1));
	if (data_global == NULL)
	{
		printf ("ERROR: can't allocate %lli bytes for data!\n", data_mem_size);
		obj_stream_close ();
		return (1);
	}

	data = data_global;

	i = 0;
	for (j = 0; j <= data_info_ind; j++)
	{
		// printf ("load_object: type: %i, start: %lli\n", data_info[j].type, i);

		switch (data_info[j].type)
		{
			case BYTE:
				//printf ("DATA BYTE\n");
				data_info[j].offset = i;

				// printf ("load_object: BYTE: size: %lli\n", data_info[j].size);
				{
					U1 read_buf[BYTE_BUF];
					S8 toread ALIGN = data_info[j].size;
                    S8 todo ALIGN = 0;
					S8 b ALIGN = 0;

					while (toread > 0)
					{
						if (toread > BYTE_BUF)
						{
							todo = BYTE_BUF;
						}
						else
						{
							todo = toread;
						}

						readsize = obj_stream_read (read_buf, sizeof (U1), todo);
						if (readsize != todo)
						{
							printf ("error: can't load data: BYTE!\n");
							obj_stream_close ();
							return (1);
						}

						for (b = 0; b < todo; b++)
						{
							data[i] = read_buf[b];
							i++;
						}
						toread = toread - todo;
					}

					data_info[j].end = i - 1;
					data_info[j].type_size = sizeof (U1);
				}
				break;

			case WORD:
				//printf ("DATA WORD\n");
				data_info[j].offset = i;
				for (k = 1; k <= (S8) (data_info[j].size / sizeof (S2)); k++)
				{
					readsize = obj_stream_read (&word, sizeof (S2), 1);
					if (readsize != 1)
					{
						printf ("error: can't load data: WORD!\n");
						obj_stream_close ();
						return (1);
					}

					word = conv_word (word);

					bptr = (U1 *) &word;

					data[i] = *bptr;
					i++; bptr++;
					data[i] = *bptr;
					i++;
				}
				data_info[j].end = i - 1;
				data_info[j].type_size = sizeof (S2);
				break;

			case DOUBLEWORD:
				//printf ("DATA DOUBLEWORD\n");
				data_info[j].offset = i;
				for (k = 1; k <= (S8) (data_info[j].size / sizeof (S4)); k++)
				{
					readsize = obj_stream_read (&doubleword, sizeof (S4), 1);
					if (readsize != 1)
					{
						printf ("error: can't load data: DOUBLEWORD!\n");
						obj_stream_close ();
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
				data_info[j].end = i - 1;
				data_info[j].type_size = sizeof (S4);
				break;

			case QUADWORD:
			case DOUBLEFLOAT:
				data_info[j].offset = i;
				for (k = 1; k <= (S8) (data_info[j].size / sizeof (S8)); k++)
				{
					readsize = obj_stream_read (&quadword, sizeof (S8), 1);
					if (readsize != 1)
					{
						printf ("error: can't load data: QUADWORD | DOUBLEFLOAT! index: %lli\n", j);
						obj_stream_close ();
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
				data_info[j].end = i - 1;
				data_info[j].type_size = sizeof (S8);
				break;
		}
	}
	obj_stream_close ();
	return (0);
}
