/*
 * This file nanoid.c is part of L1vm.
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

#include "nanoid/nanoid.h"

// protos
S2 memory_bounds (S8 start, S8 offset_access);
size_t strlen_safe (const char * str, S8 maxlen);

struct data_info data_info[MAXDATAINFO];
S8 data_info_ind;

S2 init_memory_bounds (struct data_info *data_info_orig, S8 data_info_ind_orig)
{
	memcpy (&data_info, &data_info_orig, sizeof (data_info_orig));
	data_info_ind = data_info_ind_orig;

	return (0);
}

size_t strlen_safe (const char *str, S8 maxlen)
{
	 S8 ALIGN i = 0;

	 while (1)
	 {
		if (str[i] != '\0')
		{
			i++;
		}
		else
		{
			return (i);
		}

		if (i >= maxlen)
		{
			return (0);
		}
	}
}

U1 *nanoid_create (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 id_address ALIGN;
    S8 id_len ALIGN = 21;
    S8 i ALIGN = 0;
    char *id;

    sp = stpopi ((U1 *) &id_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("nanoid_create: ERROR: stack corrupt!\n");
		return (NULL);
	}

    #if BOUNDSCHECK
        if (memory_bounds (id_address, id_len) != 0)
	    {
		    printf ("nanoid_create: ERROR: ID array overflow! Need %lli bytes!\n", id_len);
		    return (NULL);
	    }
    #endif

    #if _WIN32
        // no /dev/urandom in Windows, use C standard random number generator
        id = simple ();
    #else
    id = safe_simple ();
    #endif

    if (id == NULL)
    {
        printf ("nanoid_create: error: out of memory!\n");
        return (NULL);
    }

    for (i = 0; i < id_len; i++)
    {
        data[id_address + i] = id[i];
    }
    data[id_address + i] = '\0';

    free (id);
    return (sp);
}

U1 *nanoid_create_custom (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 id_address ALIGN;
    S8 id_set_len ALIGN;
    S8 alphabet_address ALIGN;
    S8 alphabet_len ALIGN;
    S8 i ALIGN = 0;

    char *id;

    sp = stpopi ((U1 *) &id_set_len, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("nanoid_create_custom: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &alphabet_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("nanoid_create_custom: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &id_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("nanoid_create_custom: ERROR: stack corrupt!\n");
		return (NULL);
	}

    // sane check of alphabet length
    alphabet_len = strlen_safe ((const char *) &data[alphabet_address], MAXSTRLEN);
    if (alphabet_len > 256)
    {
        printf ("nanoid_create_custom: error: alphabet size greater as 256 chars!\n");
        return (NULL);
    }

    #if BOUNDSCHECK
        if (memory_bounds (id_address, id_set_len) != 0)
	    {
		    printf ("nanoid_create_custom: ERROR: ID array overflow! Need %lli bytes!\n", id_set_len);
		    return (NULL);
	    }
    #endif

    #if _WIN32
        // no /dev/urandom in Windows, use C standard random number generator
        id = custom ((char *) &data[alphabet_address], id_set_len);
    #else
        id = safe_custom ((char *) &data[alphabet_address], id_set_len);
    #endif

    if (id == NULL)
    {
        printf ("nanoid_create_custom: error: out of memory!\n");
        return (NULL);
    }

    for (i = 0; i < id_set_len; i++)
    {
        data[id_address + i] = id[i];
    }
    data[id_address + i] = '\0';

    free (id);
    return (sp);
}
