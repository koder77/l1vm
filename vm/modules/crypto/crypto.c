/*
 * This file crypto.c is part of L1vm.
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

// sodium library simple encrypt/decrypt function
// used for both encrypt/decrypt! See "mode" variable!

#include "../../../include/global.h"
#include "../../../include/stack.h"
#include <sodium.h>

#define ENCRYPT 0
#define DECRYPT 1


// protos
S2 memory_bounds (S8 start, S8 offset_access);
size_t strlen_safe (const char * str, int maxlen);

struct data_info data_info[MAXDATAINFO];
S8 data_info_ind;

S2 init_memory_bounds (struct data_info *data_info_orig, S8 data_info_ind_orig)
{
	memcpy (&data_info, &data_info_orig, sizeof (data_info_orig));
	data_info_ind = data_info_ind_orig;

	return (0);
}

U1 *encrypt_sodium (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    // mode 0 = encrypt/mode 1 = decrypt

    S8 in_address ALIGN;
    S8 out_address ALIGN;
    S8 key_address ALIGN;
    S8 nonce_address ALIGN;
    S8 size ALIGN;
    S8 mode ALIGN = 0;
    S8 cipher_text_len ALIGN;

    if (sodium_init () < 0)
    {
        printf ("encrypt_sodium: init sodium library error!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &mode, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("encrypt_sodium: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &nonce_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("encrypt_sodium: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &key_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("encrypt_sodium: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &out_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("encyrpt_sodium: ERROR: stack corrupt!\n");
		return (NULL);
    }

    sp = stpopi ((U1 *) &size, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("encrypt_sodium: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &in_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("envcrypt_sodium: ERROR: stack corrupt!\n");
		return (NULL);
    }

    cipher_text_len = crypto_secretbox_MACBYTES + size;

    if (mode == ENCRYPT)
    {
        #if BOUNDSCHECK
        if (memory_bounds (key_address, crypto_secretbox_KEYBYTES - 1) != 0)
	    {
		    printf ("encrypt_sodium: ERROR: key array overflow! Need %i bytes!\n", crypto_secretbox_KEYBYTES);
		    return (NULL);
	    }
        if (memory_bounds (nonce_address, crypto_secretbox_NONCEBYTES - 1) != 0)
	    {
		    printf ("encrypt_sodium: ERROR: nonce array overflow! Need %i bytes!\n", crypto_secretbox_NONCEBYTES);
		    return (NULL);
	    }
        #endif

        crypto_secretbox_keygen (&data[key_address]);
        randombytes_buf (&data[nonce_address], sizeof (crypto_secretbox_NONCEBYTES));

        #if BOUNDSCHECK
        if (memory_bounds (out_address, cipher_text_len - 1) != 0)
	    {
		    printf ("encrypt_sodium: ERROR: output array overflow!\n");
		    return (NULL);
	    }
        if (memory_bounds (in_address, size - 1) != 0)
	    {
		    printf ("encrypt_sodium: ERROR: input array overflow!\n");
		    return (NULL);
	    }
        #endif

        crypto_secretbox_easy (&data[out_address], &data[in_address], size, &data[nonce_address], &data[key_address]);
    }
    else
    {
        #if BOUNDSCHECK
        if (memory_bounds (key_address, crypto_secretbox_KEYBYTES - 1) != 0)
	    {
		    printf ("encrypt_sodium: ERROR: key array overflow! Need %i bytes!\n", crypto_secretbox_KEYBYTES);
		    return (NULL);
	    }
        if (memory_bounds (nonce_address, crypto_secretbox_NONCEBYTES - 1) != 0)
	    {
		    printf ("encrypt_sodium: ERROR: nonce array overflow! Need %i bytes!\n", crypto_secretbox_NONCEBYTES);
		    return (NULL);
	    }
        if (memory_bounds (out_address, cipher_text_len - 1) != 0)
	    {
		    printf ("encrypt_sodium: ERROR: output array overflow!\n");
		    return (NULL);
	    }
        if (memory_bounds (in_address, size - 1) != 0)
	    {
		    printf ("encrypt_sodium: ERROR: input array overflow!\n");
		    return (NULL);
	    }
        #endif

        if (crypto_secretbox_open_easy (&data[out_address], &data[in_address], cipher_text_len, &data[nonce_address], &data[key_address]) != 0)
        {
            // error on decrypt!!!
            sp = stpushi (1, sp, sp_bottom);
	        if (sp == NULL)
	        {
		        // error
		        printf ("encrypt_sodium: ERROR: stack corrupt!\n");
		        return (NULL);
	        }
        }
    }

    sp = stpushi (0, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("encrypt_sodium: ERROR: stack corrupt!\n");
		return (NULL);
	}

    return (sp);
}
