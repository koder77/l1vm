/*
 * This file ciphersaber.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2022
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
#include "../../../include/stack.h"

#include <sodium.h>

#define ENCRYPT 0
#define DECRYPT 1

#define KEYSET_REPEAT 32

#if defined(__linux__) || defined(__FreeBSD__) || defined(__DragonFly__) || defined(__MACH__ )  /* Linux & FreeBSD & DragonFly BSD & macOS */
#define RANDOM_DEV 1
#else 
#define RANDOM_DEV 0
#endif

// protos
extern U1 get_sandbox_filename (U1 *filename, U1 *sandbox_filename, S2 max_name_len);

S2 memory_bounds (S8 start, S8 offset_access);
size_t strlen_safe (const char * str, int maxlen);

unsigned char state[256], key[256];

extern struct data_info data_info[MAXDATAINFO];
extern S8 data_info_ind;

S2 init_memory_bounds (struct data_info *data_info_orig, S8 data_info_ind_orig)
{
	memcpy (&data_info, &data_info_orig, sizeof (data_info_orig));
	data_info_ind = data_info_ind_orig;

	return (0);
}

void cleanup (void)
{
    int i;

    for (i = 0; i <= 255; i++)
    {
        key[i] = 0;
        state[i] = 0;
    }
}

int get_random (unsigned char *ptr)
{
    FILE *rand;

    #if RANDOM_DEV
    if (! (rand = fopen ("/dev/random", "rb")))
    {
        printf ("error: can't open /dev/random\n");
        return (-1);
    }

    if (fread (ptr, sizeof (unsigned char), 10, rand) != 10)
    {
        printf ("error: can't read from /dev/random!\n");
        fclose (rand);
        return (-1);
    }

    fclose (rand);

    #else

    if (sodium_init () < 0)
	{
		// error can't init lib sodium
		printf ("get_random: ERROR: can't initialize lib sodium!\n");
		return (NULL);
	}

    randombytes_buf (ptr, 10);
    
    #endif

    return (0);
}

U1 *ciphersaber (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    FILE *infile, *outfile;

    U1 *ptr;
    S8 i ALIGN;
    S8 j ALIGN;
    S8 k ALIGN;
    S8 key_len ALIGN;
    S8 s ALIGN;
    S8 n ALIGN;
    S8 in ALIGN;
    S8 out ALIGN;

    S8 infile_addr ALIGN = 0;
    S8 outfile_addr ALIGN = 0;
    S8 key_addr ALIGN;
    S8 mode ALIGN = 0;

    U1 source_fullnamestr[256];
    U1 dest_fullnamestr[256];

    sp = stpopi ((U1 *) &mode, sp, sp_top);
	if (sp == NULL)
	{
        cleanup ();
		// error
		printf ("ciphersaber: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &key_addr, sp, sp_top);
	if (sp == NULL)
	{
        cleanup ();
		// error
		printf ("ciphersaber: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &outfile_addr, sp, sp_top);
	if (sp == NULL)
	{
        cleanup ();
		// error
		printf ("ciphersaber: ERROR: stack corrupt!\n");
		return (NULL);
    }

    sp = stpopi ((U1 *) &infile_addr, sp, sp_top);
	if (sp == NULL)
	{
        cleanup ();
		// error
		printf ("ciphersaber: ERROR: stack corrupt!\n");
		return (NULL);
    }

    if (mode < 0 || mode >= 2)
    {
        cleanup ();
        // error
        printf ("ciphersaber mode must be: 0 (encrypt) or: 1 (decrypt)!\n");
        return (NULL);
    }

    // get source full name
    #if SANDBOX
	if (get_sandbox_filename (&data[infile_addr],  source_fullnamestr, 255) != 0)
	{
		printf ("ERROR: file_copy: illegal filename: %s\n", &data[infile_addr]);
		return (NULL);
	}
	#else
	if (strlen_safe ((char *) &data[infile_addr], 255) < 255)
	{
    	strcpy ((char *) source_fullnamestr, (char *) &data[infile_addr]);
	}
	else
	{
		printf ("ERROR: file_copy: filename %s too long!\n", &data[infile_addr]);
		return (NULL);
	}
    #endif

    // get dest full name
    #if SANDBOX
	if (get_sandbox_filename (&data[outfile_addr],  dest_fullnamestr, 255) != 0)
	{
		printf ("ERROR: file_copy: illegal filename: %s\n", &data[outfile_addr]);
		return (NULL);
	}
	#else
	if (strlen_safe ((char *) &data[out_fileaddr], 255) < 255)
	{
    	strcpy ((char *) dest_fullnamestr, (char *) &data[outfile_addr]);
	}
	else
	{
		printf ("ERROR: file_copy: filename %s too long!\n", &data[outfile_addr]);
		return (NULL);
	}
    #endif

    if (! (infile = fopen ((const char *) source_fullnamestr, "rb")))
    {
        printf ("ciphersaber: error: can't open input-file %s\n", source_fullnamestr);

        cleanup ();
        // error
        sp = stpushi (1, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("ciphersaber: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    if (! (outfile = fopen ((const char *) dest_fullnamestr, "wb")))
    {
        printf ("ciphersaber: error: can't open output-file %s\n", dest_fullnamestr);

        fclose (infile);
        cleanup ();
        // error
        sp = stpushi (1, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("ciphersaber: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    /* check if mode argument is valid */

    if (mode < 0 || mode >=2)
    {
        printf ("ciphersaber: error: mode not valid: must be 0 (encrypt) or 1 (decrypt)!\n");

        fclose (outfile);
        fclose (infile);
        cleanup ();
        // error
        sp = stpushi (1, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("ciphersaber: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    key_len = strlen ((const char *) &data[key_addr]) - 1;
    if (key_len > 246)
    {
        printf ("ciphersaber: error key string overflow!\n");

        fclose (outfile);
        fclose (infile);
        cleanup ();
        // error
        sp = stpushi (1, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("ciphersaber: ERROR: stack corrupt!\n");
            return (NULL);
        }
        return (sp);
    }

    if (mode == DECRYPT)
    {
        ptr = (unsigned char *) &data[key_addr + key_len];

        if (fread (ptr, sizeof (unsigned char), 10, infile) != 10)
        {
            printf ("ciphersaber: error: can't read initialization vector!\n");

            fclose (outfile);
            fclose (infile);
            cleanup ();
            // error
            sp = stpushi (1, sp, sp_bottom);
            if (sp == NULL)
            {
                // error
                printf ("ciphersaber: ERROR: stack corrupt!\n");
                return (NULL);
            }
            return (sp);
        }
    }
    else
    {
        if (get_random (&data[key_addr + key_len]) < 0)
        {
            printf ("ciphersaber: error: can't get random data!\n");

            fclose (outfile);
            fclose (infile);
            cleanup ();
            // error
            sp = stpushi (1, sp, sp_bottom);
            if (sp == NULL)
            {
                // error
                printf ("ciphersaber: ERROR: stack corrupt!\n");
                return (NULL);
            }
            return (sp);

        }

        i = key_len;
        for (j = 1; j <= 10; j++)
        {
            if (fputc ((int) data[key_addr + i], outfile) < 0)
            {
                printf ( "ciphersaber: error: can't write output-file %s \n", &data[outfile_addr]);

                fclose (outfile);
                fclose (infile);
                cleanup ();
                // error
                sp = stpushi (1, sp, sp_bottom);
                if (sp == NULL)
                {
                    // error
                    printf ("ciphersaber: ERROR: stack corrupt!\n");
                    return (NULL);
                }
                return (sp);
            }
            i++;
        }
    }

    key_len = key_len + 10;

    for (i = 0; i <= 255; i++)
    {
        state[i] = i;
    }

    j = 0;
    for (k = 1; k <= KEYSET_REPEAT; k++)
    {
        for (i = 0; i <= 255; i++)
        {
            j = (j + state[i] + key[i % key_len]) % 256;

            s = state[i];
            state[i] = state[j];
            state[j] = s;
        }
    }

    i = 0; j = 0;

    /* encrypt/decrypt data */

    while (1)
    {
        in = fgetc (infile);
        if (in == EOF)
        {
            break;
        }

        i = (i + 1) % 256;
        j = (j + state[i]) % 256;

        s = state[i];
        state[i] = state[j];
        state[j] = s;

        n = (state[i] + state[j]) % 256;

        out = in ^ state[n];

        if (fputc (out, outfile) < 0)
        {
            printf ("ciphersaber: error: can't write output-file %s \n", &data[outfile_addr]);

            fclose (outfile);
            fclose (infile);
            cleanup ();
            // error
            sp = stpushi (1, sp, sp_bottom);
            if (sp == NULL)
            {
                // error
                printf ("ciphersaber: ERROR: stack corrupt!\n");
                return (NULL);
            }
            return (sp);
        }
    }

    fclose (outfile);
    fclose (infile);
    cleanup ();

    // all ok!
    sp = stpushi (0, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("ciphersaber: ERROR: stack corrupt!\n");
		return (NULL);
	}
    return (sp);
}
