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
#include <sodium/crypto_box.h>
#include <sodium/crypto_pwhash.h>

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

    if (sodium_init () < 0)
    {
        printf ("Init sodium library error!\n");
        return (1);
    }

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
    S8 generate_key ALIGN = 1;

    sp = stpopi ((U1 *) &mode, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("encrypt_sodium: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &generate_key, sp, sp_top);
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

        if (generate_key == 1)
        {
            crypto_secretbox_keygen (&data[key_address]);
            randombytes_buf (&data[nonce_address], sizeof (crypto_secretbox_NONCEBYTES));
        }

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

// public/private key pair functions
U1 *generate_keys (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 public_key_address ALIGN;
    S8 private_key_address ALIGN;

    sp = stpopi ((U1 *) &private_key_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("generate_keys: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &public_key_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("generate_keys: ERROR: stack corrupt!\n");
		return (NULL);
	}

    // check if output arrays are of right size
    #if BOUNDSCHECK
    if (memory_bounds (private_key_address, crypto_box_SECRETKEYBYTES - 1 ) != 0)
	{
		 printf ("generate_keys: ERROR: private key output array overflow! Not in size of: %i\n", crypto_box_SECRETKEYBYTES);
		 return (NULL);
	}
    if (memory_bounds (public_key_address, crypto_box_PUBLICKEYBYTES - 1) != 0)
	{
		printf ("generate_keys: ERROR: public key output array overflow! Not in size of: %i\n", crypto_box_PUBLICKEYBYTES);
        return (NULL);
	}
    #endif

    crypto_box_keypair (&data[public_key_address], &data[private_key_address]) ;

    return (sp);
}

U1 *encrypt_message (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 public_key_address ALIGN;
    S8 private_key_address ALIGN;
    S8 message_address ALIGN;
    S8 message_len ALIGN;
    S8 encrypt_address ALIGN;
    S8 nonce_address ALIGN;
    S8 ret ALIGN;
    unsigned char nonce[crypto_box_NONCEBYTES];

    sp = stpopi ((U1 *) &private_key_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("encrypt_message: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &public_key_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("encrypt_message: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &nonce_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("encrypt_message: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &encrypt_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("encrypt_message: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &message_len, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("encrypt_message: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &message_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("encrypt_message: ERROR: stack corrupt!\n");
		return (NULL);
	}

    #if BOUNDSCHECK
    if (memory_bounds (encrypt_address, crypto_box_MACBYTES + message_len - 1) != 0)
	{
		 printf ("encrypt_message: ERROR: encrypt array overflow! Not in size of: %lli\n", crypto_box_MACBYTES + message_len);
		 return (NULL);
	}
    if (memory_bounds (message_address, message_len - 1) != 0)
	{
		 printf ("encrypt_message: ERROR: message array overflow!\n");
		 return (NULL);
	}
    if (memory_bounds (nonce_address, sizeof (nonce) - 1) != 0)
	{
		 printf ("encrypt_message: ERROR: nonce array overflow! Not in size of %li\n", sizeof (nonce));
		 return (NULL);
	}
    #endif

    randombytes_buf (&data[nonce_address], sizeof (nonce));

    ret = crypto_box_easy ((unsigned char *) &data[encrypt_address], (const unsigned char *) &data[message_address], message_len, &data[nonce_address], (const unsigned char *) &data[public_key_address], (const unsigned char *) &data[private_key_address]);
    if (ret < 0)
    {
        // error
        sp = stpushi (1, sp, sp_bottom);
        if (sp == NULL)
	    {
            // error
		    printf ("encrypt_message: ERROR: stack corrupt!\n");
		    return (NULL);
	    }
    }
    else
    {
        // all OK!
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
	    {
            // error
		    printf ("encrypt_message: ERROR: stack corrupt!\n");
		    return (NULL);
	    }
    }

    return (sp);
}

U1 *decrypt_message (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 public_key_address ALIGN;
    S8 private_key_address ALIGN;
    S8 message_address ALIGN;
    S8 message_len ALIGN;
    S8 decrypt_address ALIGN;
    S8 nonce_address ALIGN;
    S8 ret ALIGN;

    sp = stpopi ((U1 *) &private_key_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("decrypt_message: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &public_key_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("decrypt_message: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &nonce_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("decrypt_message: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &decrypt_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("decrypt_message: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &message_len, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("decrypt_message: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &message_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("decrypt_message: ERROR: stack corrupt!\n");
		return (NULL);
	}

    #if BOUNDSCHECK
    if (memory_bounds (decrypt_address, message_len + crypto_box_MACBYTES - 1) != 0)
	{
		 printf ("decrypt_message: ERROR: encrypt array overflow! Not in size of: %lli\n", crypto_box_MACBYTES + message_len);
		 return (NULL);
	}
    #endif

    ret = crypto_box_open_easy ((unsigned char *) &data[decrypt_address], (const unsigned char *) &data[message_address], message_len + crypto_box_MACBYTES, (const unsigned char *) &data[nonce_address], &data[public_key_address], &data[private_key_address]);
    if (ret < 0)
    {
        // error
        sp = stpushi (1, sp, sp_bottom);
        if (sp == NULL)
	    {
            // error
		    printf ("decrypt_message: ERROR: stack corrupt!\n");
		    return (NULL);
	    }
    }
    else
    {
        // all OK!
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
	    {
            // error
		    printf ("decrypt_message: ERROR: stack corrupt!\n");
		    return (NULL);
	    }
    }

    return (sp);
}

U1 *generate_key_hash (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 hashed_password_address ALIGN;
    S8 password_address ALIGN;
    S8 password_len ALIGN;
    S8 ret ALIGN;

    sp = stpopi ((U1 *) &hashed_password_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("generate_key_hash: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &password_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("generate_key_hash: ERROR: stack corrupt!\n");
		return (NULL);
	}

    #if BOUNDSCHECK
    if (memory_bounds (hashed_password_address, crypto_pwhash_STRBYTES - 1) != 0)
	{
		 printf ("generate_key_hash: ERROR: hash array overflow! Not in size of: %i\n", crypto_pwhash_STRBYTES);
		 return (NULL);
	}
    #endif

    password_len = strlen_safe ((const char *) &data[password_address], STRINGMOD_MAXSTRLEN);
    ret = crypto_pwhash_str ((char *) &data[hashed_password_address], (const char *) &data[password_address], password_len, crypto_pwhash_OPSLIMIT_SENSITIVE, crypto_pwhash_MEMLIMIT_SENSITIVE);
    if (ret != 0)
    {
        // error
        sp = stpushi (1, sp, sp_bottom);
        if (sp == NULL)
	    {
            // error
		    printf ("generate_key_hash: ERROR: stack corrupt!\n");
		    return (NULL);
	    }
    }
    else
    {
        // all OK!
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
	    {
            // error
		    printf ("generate_key_hash: ERROR: stack corrupt!\n");
		    return (NULL);
	    }
    }

    return (sp);
}

U1 *check_key_hash (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 hashed_password_address ALIGN;
    S8 password_address ALIGN;
    S8 password_len ALIGN;
    S8 ret ALIGN;

    sp = stpopi ((U1 *) &hashed_password_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("check_key_hash: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &password_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("check_key_hash: ERROR: stack corrupt!\n");
		return (NULL);
	}

    password_len = strlen_safe ((const char *) &data[password_address], STRINGMOD_MAXSTRLEN);
    ret = crypto_pwhash_str_verify ((const char *) &data[hashed_password_address], (const char *) &data[password_address], password_len);
    if (ret != 0)
    {
        // error
        sp = stpushi (1, sp, sp_bottom);
        if (sp == NULL)
	    {
            // error
		    printf ("check_key_hash: ERROR: stack corrupt!\n");
		    return (NULL);
	    }
    }
    else
    {
        // all OK!
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
	    {
            // error
		    printf ("check_key_hash: ERROR: stack corrupt!\n");
		    return (NULL);
	    }
    }

    return (sp);
}


U1 *generate_key (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 password_address ALIGN;
    S8 password_len ALIGN;
    S8 key_address ALIGN;
    S8 key_len ALIGN;
    S8 salt_address ALIGN;
    S8 ret ALIGN;

    sp = stpopi ((U1 *) &salt_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("generate_key: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &key_len, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("generate_key: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &key_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("generate_key: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &password_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("generate_key: ERROR: stack corrupt!\n");
		return (NULL);
	}

    #if BOUNDSCHECK
    if (memory_bounds (key_address, key_len - 1) != 0)
	{
		 printf ("generate_key: ERROR: key array overflow! Not in size of: %i\n", key_len);
		 return (NULL);
	}
    if (memory_bounds (salt_address, crypto_pwhash_SALTBYTES - 1) != 0)
	{
		 printf ("generate_key: ERROR: salt array overflow! Not in size of: %i\n", crypto_pwhash_SALTBYTES);
		 return (NULL);
	}
    #endif

    password_len = strlen_safe((const char *) &data[password_address], STRINGMOD_MAXSTRLEN);

    // generate the salt, you have to store it somewhere and rememeber the password
    randombytes_buf(&data[salt_address], crypto_pwhash_SALTBYTES);

    ret = crypto_pwhash ((unsigned char *) &data[key_address], key_len, (const char *) &data[password_address], password_len, &data[salt_address], crypto_pwhash_OPSLIMIT_MODERATE, crypto_pwhash_MEMLIMIT_MODERATE, crypto_pwhash_ALG_ARGON2ID13);
    if (ret != 0)
    {
        // error
        sp = stpushi (1, sp, sp_bottom);
        if (sp == NULL)
	    {
            // error
		    printf ("generate_key: ERROR: stack corrupt!\n");
		    return (NULL);
	    }
    }
    else
    {
        // all OK!
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
	    {
            // error
		    printf ("generate_key: ERROR: stack corrupt!\n");
		    return (NULL);
	    }
    }

    return (sp);
}
