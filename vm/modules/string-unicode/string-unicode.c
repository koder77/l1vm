/*
 * This file string-unicode.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (info@midnight-coding.de), 2025
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

#include <utf8proc.h>

// Windows MSYS2 fix for false positive bounds error:
#if _WIN32
#undef BOUNDSCHECK
#define BOUNDSCHECK 0
#endif

// protos
S2 memory_bounds (S8 start, S8 offset_access);

struct data_info data_info[MAXDATAINFO];
S8 data_info_ind;

S2 init_memory_bounds (struct data_info *data_info_orig, S8 data_info_ind_orig)
{
	memcpy (&data_info, &data_info_orig, sizeof (data_info_orig));
	data_info_ind = data_info_ind_orig;

	return (0);
}

S8 my_strlen_utf8_c (char *s) {
    // return the char length of a utf8 string

    S8 i = 0, j = 0;

    while (s[i])
    {
        if ((s[i] & 0xc0) != 0x80) j++;
        i++;
    }
    return j;
}

S2 get_part_of_utf8_string (char *s, S8 pos, char *part)
{
	S8 i = 0, j = 0;
	S8 part_pos = 0;

	while (s[i])
	{
		//printf ("i: %lli\n", i);

		if ((s[i] & 0xc0) != 0x80)
		{
			j++;
		}
		//printf ("j: %lli\n", j);

		if (j  - 1 == pos)
		{
			//printf ("part_pos: %lli\n", part_pos);

			part[part_pos] = s[i];
			part_pos++;
		}
		i++;
	}

	if (part_pos > 0)
	{
		// all ok!
		part[part_pos] = '\0';
		return (0);
	}
	else
	{
		// char at pos not found
		part[part_pos] = '\0';
		return (1);
	}
}

U1 *codepoint_to_string (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 strdestaddr ALIGN;
	S8 code ALIGN;

	utf8proc_int32_t code_utf8;
	utf8proc_uint8_t str_utf8[5];

	S8 i ALIGN = 0;

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("codepoint_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &code, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("codepoint_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	#if BOUNDSCHECK
	if (memory_bounds (strdestaddr, 4) != 0)
	{
		printf ("codepoint_to_string: ERROR: dest string overflow!\n");
		return (NULL);
	}
	#endif

	// clear utf8 string
	for (i = 0; i < 5; i++)
	{
		 str_utf8[i] = '\0';
	}

	code_utf8 = code;
	utf8proc_encode_char (code_utf8, str_utf8);

	i = 0;
	while (str_utf8[i])
	{
		data[strdestaddr + i] = str_utf8[i];
		i++;
	}

	return (sp);
}

U1 *string_to_codepoint (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 straddr ALIGN;
	utf8proc_int32_t code_utf8;

	sp = stpopi ((U1 *) &straddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_to_codepoint: ERROR: stack corrupt!\n");
		return (NULL);
	}

	utf8proc_iterate (&data[straddr], -1, &code_utf8);

	sp = stpushi (code_utf8, sp, sp_bottom);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_to_codepoint: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

U1 *utf8_strlen (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// return utf8 chars length of a utf8 string
    // multi byte chars count as 1!

	S8 straddr ALIGN;
	S8 strlen ALIGN;

	sp = stpopi ((U1 *) &straddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("utf8_strlen: ERROR: stack corrupt!\n");
		return (NULL);
	}

	strlen =  my_strlen_utf8_c ((char *) &data[straddr]);

	sp = stpushi (strlen, sp, sp_bottom);
	if (sp == NULL)
	{
		// ERROR:
		printf ("utf8_strlen: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

U1 *utf8_strpart (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// return utf8 chars length of a utf8 string
    // multi byte chars count as 1!

	S8 straddr ALIGN;
	S8 partstraddr ALIGN;
	S8 strpos ALIGN;
	S8 ret ALIGN;

	sp = stpopi ((U1 *) &partstraddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("utf8_strpart: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strpos, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("utf8_strpart: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &straddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("utf8_strpart: ERROR: stack corrupt!\n");
		return (NULL);
	}

    #if BOUNDSCHECK
	if (memory_bounds (partstraddr, 4) != 0)
	{
		printf ("utf8_strpart: ERROR: dest string overflow!\n");
		return (NULL);
	}
	#endif

	ret = get_part_of_utf8_string ((char *) &data[straddr], strpos, (char *) &data[partstraddr]);

    sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// ERROR:
		printf ("utf8_strpart: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}
