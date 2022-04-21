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

#define ENCRYPT 0
#define DECRYPT 1

#define KEYSET_REPEAT 32

#if defined(__linux__) || defined(__FreeBSD__) || defined(__DragonFly__) || defined(__MACH__ )  /* Linux & FreeBSD & DragonFly BSD & macOS */
#define RANDOM_DEV 1
#else 
#define RANDOM_DEV 0
#endif

// protos
extern S2 memory_bounds (S8 start, S8 offset_access);
size_t strlen_safe (const char * str, int maxlen);

int get_random (unsigned char *ptr)
{
    FILE *rand;

    #if RANDOM_DEV
    if (! (rand = fopen ("/dev/random", "rb")))
    {
        fprintf (stderr, "error: can't open /dev/random\n");
        return (-1);
    }

    if (fread (ptr, sizeof (unsigned char), 10, rand) != 10)
    {
        fprintf (stderr, "error: can't read from /dev/random!\n");
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
    S8 mode ALIGN = 0;
    S8 in_address ALIGN;
    S8 out_address ALIGN;
    S8 key_address ALIGN;
    S8 size ALIGN;
    S8 index ALIGN;
    S8 out_ind ALIGN;

    U1 *ptr;
    S8 i ALIGN;
    S8 j ALIGN;
    S8 k ALIGN;
    S8 key_len ALIGN;
    S8 s ALIGN;
    S8 n ALIGN;
    S8 in ALIGN = 0;
    S8 out ALIGN = 0;
    U1 state[256], key[256];

    sp = stpopi ((U1 *) &mode, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ciphersaber: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &size, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ciphersaber: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &key_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ciphersaber: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &out_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ciphersaber: ERROR: stack corrupt!\n");
		return (NULL);
    }

    sp = stpopi ((U1 *) &in_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ciphersaber: ERROR: stack corrupt!\n");
		return (NULL);
    }

    if (mode < 0 || mode >= 2)
    {
        printf ("ciphersaber mode must be: 0 (encrypt) or: 1 (decrypt)!\n");
        return (NULL);
    }

    if (size < 0)
    {
        printf ("ciphersaber size must be positive!\n");
        return (NULL);
    }

    key_len = strlen_safe ((const char *) &data[key_address], MAXLINELEN) - 1;
    if (key_len > 246)
    {
        printf ("ciphersaber: ERROR: key len longer as 246 chars!\n");
        return (NULL);
    }

    if (mode == DECRYPT)
    {
        #if BOUNDSCHECK
        if (memory_bounds (key_address, key_len) != 0)
	    {
		    printf ("ciphersaber: ERROR: key array overflow!\n");
		    return (NULL);
	    }
        #endif
        ptr = (U1 *) &data[key_address + key_len];
        for (i = 0; i < 10; i++)
        {
            #if BOUNDSCHECK
            if (memory_bounds (in_address, in) != 0)
	        {
		        printf ("ciphersaber: ERROR: key array overflow!\n");
		        return (NULL);
	        }
            #endif
            *ptr = data[in_address + in];
            in++;
        }
    }
    else
    {
        if (get_random (&data[key_address + key_len]) < 0)
        {
            printf ("ciphersaber: ERROR: can't get random data!\n");
            return (NULL);
        }

        i = key_len;
        for (j = 1; j <= 10; j++)
        {
            #if BOUNDSCHECK
            if (memory_bounds (out_address, out) != 0)
	        {
		        printf ("ciphersaber: ERROR: out array overflow!\n");
		        return (NULL);
	        }
            #endif
            data[out_address + out] = key[i];
            out++;
            i++;
        }
        
    }
    out_ind = out--;
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

    for (index = 0; index < size; index++)
    {
        #if BOUNDSCHECK
        if (memory_bounds (in_address, index) != 0)
	    {
		    printf ("ciphersaber: ERROR: in array overflow!\n");
		    return (NULL);
	    }
        #endif
        in = (S8) data[in_address + index];

        i = (i + 1) % 256;
        j = (j + state[i]) % 256;

        s = state[i];
        state[i] = state[j];
        state[j] = s;

        n = (state[i] + state[j]) % 256;

        out = in ^ state[n];

        #if BOUNDSCHECK
        if (memory_bounds (out_address, out_ind + index) != 0)
	    {
		    printf ("ciphersaber: ERROR: out array overflow!\n");
		    return (NULL);
	    }
        #endif
        data[out_address + out_ind + index] = (U1) out;
    }
    return (sp);
}
