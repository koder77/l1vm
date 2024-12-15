/*
 * This file mpfr-head.c is part of L1vm.
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
#include <iostream>
#include <fstream>
#include <filesystem>

#include <string>
#include <iostream>
#include <sstream>

#if __MACH__
#undef CPP_FILE_EXPERIMENTAL
#define CPP_FILE_EXPERIMENTAL 0
#endif

#if CPP_FILE_EXPERIMENTAL
#include <experimental/filesystem>
#else
#include <filesystem>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <mpfr.h>
#include "mpreal.h"


#include "../../../include/stack.h"
#include "../nanoid/nanoid/nanoid.h"

using mpfr::mpreal;
using std::cout;

struct data_info data_info[MAXDATAINFO];
S8 data_info_ind;

// protos
extern "C" S2 memory_bounds (S8 start, S8 offset_access);

#define MAX_FLOAT_NUM 4096

static mpreal mpf_float[MAX_FLOAT_NUM];

extern "C" U1 get_sandbox_filename (U1 *filename, U1 *sandbox_filename, S2 max_name_len);
char* simple ();

char *fgets_uni (char *str, int len, FILE *fptr)
{
    int ch, nextch;
    int i = 0, eol = FALSE;
    char *ret;

    ch = fgetc (fptr);
    if (feof (fptr))
    {
        return (NULL);
    }
    while (! feof (fptr) || i == len - 2)
    {
        switch (ch)
        {
            case '\r':
                /* check for '\r\n\' */

                nextch = fgetc (fptr);
                if (! feof (fptr))
                {
                    if (nextch != '\n')
                    {
                        ungetc (nextch, fptr);
                    }
                }
                str[i] = '\n';
                i++; eol = TRUE;
                break;

            case '\n':
                /* check for '\n\r\' */

                nextch = fgetc (fptr);
                if (! feof (fptr))
                {
                    if (nextch != '\r')
                    {
                        ungetc (nextch, fptr);
                    }
                }
                str[i] = '\n';
                i++; eol = TRUE;
                break;

            default:
				str[i] = ch;
				i++;

                break;
        }

        if (eol)
        {
            break;
        }

        ch = fgetc (fptr);
    }

    if (feof (fptr))
    {
//        str[i] = '\n';
//        i++;
        str[i] = '\0';
    }
    else
    {
        str[i] = '\0';
    }

    ret = str;
    return (ret);
}


size_t strlen_safe (const char * str, S8 maxlen)
{
	 long long int i = 0;

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
		if (maxlen > 0)
		{
			if (i > maxlen)
			{
				return (0);
			}
		}
	}
}

S2 check_is_digit (const char *numberstr)
{
	S8 number_len ALIGN;
	S8 i ALIGN = 0;
	U1 check = 1;

	number_len = strlen_safe (numberstr, STRINGMOD_MAXSTRLEN);

	// printf ("DEBUG: check_is_digit pi len: %lli\n", number_len);

	if (number_len == 0)
	{
		// error: empty string!
		return (1);
	}

	while (check == 1)
	{
		if (numberstr[i] == ' ')
		{
			printf ("check_is_digit: number has spaces in string: error!\n");
			return (1);
		}

		if (isdigit (numberstr[i]) == 0)
		{
			// check if it is decimal point:
			if (numberstr[i] != '.')
			{
				printf ("check_is_digit: number is not digit: %i\n", numberstr[i]);

				// error no valid number char
				return (1);
			}
		}
	    if (i < number_len - 1)
		{
			i++;
		}
		else
		{
			check = 0;
		}
	}
	// all digits are numbers, is a number!
	return (0);
}

extern "C" U1 *mp_set_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index ALIGN;
	S8 numstring_address ALIGN;
	S8 num_base ALIGN;

	sp = stpopi ((U1 *) &float_index, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_set_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &num_base, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_set_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &numstring_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_set_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index >= MAX_FLOAT_NUM || float_index < 0)
	{
		printf ("mp_set_float: ERROR: float index out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (check_is_digit ((const char *) &data[numstring_address]) != 0)
    {
		printf ("mp_set_float: ERROR: float is not a number!\n%s\n", &data[numstring_address]);
		return (NULL);
	}

	mpf_float[float_index] = mpfr::mpreal ((const char *) &data[numstring_address]);

	return (sp);
}

extern "C" U1 *mp_set_float_prec (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index ALIGN;
	S8 numstring_address ALIGN;
	S8 num_base ALIGN;
	S8 precision ALIGN;

	sp = stpopi ((U1 *) &float_index, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_set_float_prec: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &precision, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_set_float_prec: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &num_base, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_set_float_prec: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &numstring_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_set_float_prec: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index >= MAX_FLOAT_NUM || float_index < 0)
	{
		printf ("mp_set_float_prec: ERROR float index out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (precision >= MPFR_PREC_MAX)
	{
		printf ("mp_set_float_prec: warning: precision %lli is too high! Limit is: %l\n", precision, MPFR_PREC_MAX);
	}

	// printf ("DEBUG: mp_set_float: '%s'\n", (const char *) &data[numstring_address]);
	//
	if (check_is_digit ((const char *) &data[numstring_address]) != 0)
    {
		printf ("mp_set_float_prec: ERROR: float is not a number!\n%s\n", &data[numstring_address]);
		return (NULL);
	}

	mpf_float[float_index] = mpfr::mpreal ((const char *) &data[numstring_address], precision, num_base, MPFR_RNDN);

	return (sp);
}

extern "C" U1 *mp_set_float_prec_from_file (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index ALIGN;
	S8 filename_address ALIGN;
	S8 num_base ALIGN;
	S8 precision ALIGN;
	S8 file_size_bytes ALIGN;
	U1 *numstr;
	FILE *numfile;
    U1 filename[256];
	S8 ret ALIGN;

	std::string filenamestr;

	sp = stpopi ((U1 *) &float_index, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_set_float_prec_from_file: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &precision, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_set_float_prec_from_file: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &num_base, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_set_float_prec_from_file: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &filename_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_set_float_prec_from_file: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index >= MAX_FLOAT_NUM || float_index < 0)
	{
		printf ("mp_set_float_prec_from_file: ERROR float index out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (precision >= MPFR_PREC_MAX)
	{
		printf ("mp_set_float_prec_from_file: warning: precision %lli is too high! Limit is: %l\n", precision, MPFR_PREC_MAX);
		return (NULL);
	}

	if (get_sandbox_filename (&data[filename_address], filename, 255) != 0)
	{
		printf ("mp_set_float_prec_from_file: illegal filename: %s\n", &data[filename_address]);
		return (NULL);
	}

	// get file size
	filenamestr.assign ((const char *) filename);

	#if CPP_FILE_EXPERIMENTAL
	file_size_bytes = std::experimental::filesystem::v1::file_size (filenamestr);
	#else
	file_size_bytes = std::filesystem::file_size (filenamestr);
    #endif

	// alloc array for number in file
    numstr = (U1 *) calloc (file_size_bytes + 1, sizeof (U1));
	if (numstr == NULL)
	{
		printf ("mp_set_float_prec_from_file: ERROR can't allocate %lli bytes for number string!\n", file_size_bytes + 1);
		return (NULL);
	}

	if (get_sandbox_filename (&data[filename_address], filename, 255) != 0)
	{
		printf ("mp_set_float_prec_from_file: illegal filename: %s\n", &data[filename_address]);
		free (numstr);
		return (NULL);
	}

	numfile = fopen ((char *) filename, "r");
	if (numfile == NULL)
	{
		printf ("mp_set_float_prec_from_file: ERROR can't open file: '%s' !\n", &data[filename_address]);
		free (numstr);
		return (NULL);
	}

	// size_t fread(void * buffer, size_t size, size_t count, FILE * stream);
	ret = fread (numstr, sizeof (U1), file_size_bytes, numfile);
	if (ret != file_size_bytes)
	{
		printf ("mp_set_float_prec_from_file: ERROR can't read file: '%s' !\n", &data[filename_address]);
		free (numstr);
		fclose (numfile);
		return (NULL);
	}

    fclose (numfile);

	mpf_float[float_index] = mpfr::mpreal ((const char *) numstr, precision, num_base, MPFR_RNDN);
	free (numstr);

	return (sp);
}

extern "C" U1 *mp_get_precision_bits (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// gets the precision in digits
	// returns the precision in bits for the other mpfr number set functions above!

	S8 precision_bits ALIGN = 0;
	S8 precision ALIGN;

	sp = stpopi ((U1 *) &precision, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_get_precision_bits: ERROR: stack corrupt!\n");
		return (NULL);
	}

	precision_bits = mpfr::digits2bits (precision);

	sp = stpushi (precision_bits, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("mp_get_precision_bits: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

// get const ==================================================================

extern "C" U1 *mp_get_pi_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_res ALIGN;
	S8 float_prec ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_get_pi_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	// floating point precision
	sp = stpopi ((U1 *) &float_prec, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_get_pi_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	mpf_float[float_index_res] = mpfr::const_pi (float_prec, MPFR_RNDN);
	return (sp);
}

extern "C" U1 *mp_get_log2_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_res ALIGN;
	S8 float_prec ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_get_log2_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	// floating point precision
	sp = stpopi ((U1 *) &float_prec, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_get_log2_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	mpf_float[float_index_res] = mpfr::const_log2 (float_prec, MPFR_RNDN);
	return (sp);
}

extern "C" U1 *mp_get_euler_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_res ALIGN;
	S8 float_prec ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_get_euler_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	// floating point precision
	sp = stpopi ((U1 *) &float_prec, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_get_euler_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	mpf_float[float_index_res] = mpfr::const_euler (float_prec, MPFR_RNDN);
	return (sp);
}

extern "C" U1 *mp_get_catalan_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_res ALIGN;
	S8 float_prec ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_get_catalan_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	// floating point precision
	sp = stpopi ((U1 *) &float_prec, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_get_catalan_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	mpf_float[float_index_res] = mpfr::const_catalan (float_prec, MPFR_RNDN);
	return (sp);
}


// print float number =========================================================

extern "C" U1 *mp_print_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_format_address ALIGN;
	S8 precision_out ALIGN;

	sp = stpopi ((U1 *) &precision_out, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_print_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_format_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_print_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_print_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index_x >= MAX_FLOAT_NUM || float_index_x < 0)
	{
		printf ("mp_print_float: ERROR float index x out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	cout.precision (precision_out);
	cout << std::fixed;
	cout << mpf_float[float_index_x];

	return (sp);
}

extern "C" U1 *mp_prints_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index ALIGN;
	S8 numstring_address_dest ALIGN;
	S8 numstring_len ALIGN;
	S8 float_format_address ALIGN;
	S8 precision ALIGN;

	char file_name_id[21];
	strcpy (file_name_id, simple ());

	std::stringstream number_outstr;
	std::string number_outsavestr = "";

	sp = stpopi ((U1 *) &precision, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_prints_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_format_address, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_prints_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &numstring_len, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_prints_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &numstring_address_dest, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_prints_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_prints_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index >= MAX_FLOAT_NUM || float_index < 0)
	{
		printf ("mp_prints_float: ERROR float index out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	// check if destination string is big enough
	#if BOUNDSCHECK
	if (memory_bounds (numstring_address_dest, numstring_len) != 0)
	{
		printf ("mp_prints_float: ERROR: dest string overflow!\n");
		return (NULL);
	}
	#endif

	number_outstr.precision (precision);
	number_outstr << std::fixed;
    number_outstr << mpf_float[float_index];

	number_outstr >> number_outsavestr;
	if (number_outsavestr.size () >= numstring_len)
	{
		printf ("mp_prints_float: ERROR: dest string overflow!\n");
		return (NULL);
	}

	strcpy ((char *) &data[numstring_address_dest], number_outsavestr.c_str());

	return (sp);
}

// cleanup ====================================================================
extern "C" U1 *mp_cleanup (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	mpfr_free_cache ();
	return (sp);
}

// float math =================================================================

// calculate float ============================================================

extern "C" U1 *mp_add_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_y ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_add_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_add_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_add_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index_x >= MAX_FLOAT_NUM || float_index_x < 0)
	{
		printf ("mp_add_float: ERROR float index x out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_y >= MAX_FLOAT_NUM || float_index_y < 0)
	{
		printf ("mp_add_float: ERROR float index y out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
	{
		printf ("mp_add_float: ERROR float index res out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	//mpf_float[float_index_res] = mpf_float[float_index_x] + mpf_float[float_index_y];
	mpfr_add (mpf_float[float_index_res].mpfr_ptr(), mpf_float[float_index_x].mpfr_srcptr(), mpf_float[float_index_y].mpfr_srcptr(), MPFR_RNDN);
	return (sp);
}

extern "C" U1 *mp_sub_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_y ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_sub_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_sub_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_sub_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index_x >= MAX_FLOAT_NUM || float_index_x < 0)
	{
		printf ("mp_sub_float: ERROR float index x out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_y >= MAX_FLOAT_NUM || float_index_y < 0)
	{
		printf ("mp_sub_float: ERROR float index y out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
	{
		printf ("mp_sub_float: ERROR float index res out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	//mpf_float[float_index_res] = mpf_float[float_index_x] - mpf_float[float_index_y];
	mpfr_sub (mpf_float[float_index_res].mpfr_ptr(), mpf_float[float_index_x].mpfr_srcptr(), mpf_float[float_index_y].mpfr_srcptr(), MPFR_RNDN);
	return (sp);
}

extern "C" U1 *mp_mul_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_y ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_mul_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_mul_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_mul_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index_x >= MAX_FLOAT_NUM || float_index_x < 0)
	{
		printf ("mp_mul_float: ERROR float index x out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_y >= MAX_FLOAT_NUM || float_index_y < 0)
	{
		printf ("mp_mul_float: ERROR float index y out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
	{
		printf ("mp_mul_float: ERROR float index res out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	// mpf_float[float_index_res] = mpf_float[float_index_x] * mpf_float[float_index_y];
	mpfr_mul (mpf_float[float_index_res].mpfr_ptr(), mpf_float[float_index_x].mpfr_srcptr(), mpf_float[float_index_y].mpfr_srcptr(), MPFR_RNDN);
	return (sp);
}

extern "C" U1 *mp_div_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_y ALIGN;
	S8 float_index_res ALIGN;

	sp = stpopi ((U1 *) &float_index_res, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_div_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_div_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_div_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index_x >= MAX_FLOAT_NUM || float_index_x < 0)
	{
		printf ("mp_div_float: ERROR float index x out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_y >= MAX_FLOAT_NUM || float_index_y < 0)
	{
		printf ("mp_div_float: ERROR float index y out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_res >= MAX_FLOAT_NUM || float_index_res < 0)
	{
		printf ("mp_div_float: ERROR float index res out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	// mpf_float[float_index_res] = mpf_float[float_index_x] / mpf_float[float_index_y];
	mpfr_div (mpf_float[float_index_res].mpfr_ptr(), mpf_float[float_index_x].mpfr_srcptr(), mpf_float[float_index_y].mpfr_srcptr(), MPFR_RNDN);
	return (sp);
}


// compare float ==============================================================

extern "C" U1 *mp_less_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_y ALIGN;
	S8 float_res ALIGN;

	sp = stpopi ((U1 *) &float_index_y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_less_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_less_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index_x >= MAX_FLOAT_NUM || float_index_x < 0)
	{
		printf ("mp_less_float: ERROR float index x out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_y >= MAX_FLOAT_NUM || float_index_y < 0)
	{
		printf ("mp_less_float: ERROR float index y out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	float_res = mpf_float[float_index_x] < mpf_float[float_index_y];

	// push result on stack
	sp = stpushi (float_res, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("mp_less_float: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

extern "C" U1 *mp_less_equal_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_y ALIGN;
	S8 float_res ALIGN;

	sp = stpopi ((U1 *) &float_index_y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_less_equal_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_less_equal_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index_x >= MAX_FLOAT_NUM || float_index_x < 0)
	{
		printf ("mp_less_equal_float: ERROR float index x out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_y >= MAX_FLOAT_NUM || float_index_y < 0)
	{
		printf ("mp_less_equal_float: ERROR float index y out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	float_res = mpf_float[float_index_x] <= mpf_float[float_index_y];

	// push result on stack
	sp = stpushi (float_res, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("mp_less_equal_float: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

extern "C" U1 *mp_greater_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_y ALIGN;
	S8 float_res ALIGN;

	sp = stpopi ((U1 *) &float_index_y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_greater_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_greater_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index_x >= MAX_FLOAT_NUM || float_index_x < 0)
	{
		printf ("mp_greater_float: ERROR float index x out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_y >= MAX_FLOAT_NUM || float_index_y < 0)
	{
		printf ("mp_greater_float: ERROR float index y out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	float_res = mpf_float[float_index_x] > mpf_float[float_index_y];

	// push result on stack
	sp = stpushi (float_res, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("mp_greater_float: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

extern "C" U1 *mp_greater_equal_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_y ALIGN;
	S8 float_res ALIGN;

	sp = stpopi ((U1 *) &float_index_y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_greater_equal_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_greater_equal_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index_x >= MAX_FLOAT_NUM || float_index_x < 0)
	{
		printf ("mp_greater_equal_float: ERROR float index x out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_y >= MAX_FLOAT_NUM || float_index_y < 0)
	{
		printf ("mp_greater_equal_float: ERROR float index y out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	float_res = mpf_float[float_index_x] >= mpf_float[float_index_y];

	// push result on stack
	sp = stpushi (float_res, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("mp_greater_equal_float: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

extern "C" U1 *mp_equal_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_y ALIGN;
	S8 float_res ALIGN;

	sp = stpopi ((U1 *) &float_index_y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_equal_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_equal_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index_x >= MAX_FLOAT_NUM || float_index_x < 0)
	{
		printf ("mp_equal_float: ERROR float index x out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_y >= MAX_FLOAT_NUM || float_index_y < 0)
	{
		printf ("mp_equal_float: ERROR float index y out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	float_res = mpf_float[float_index_x] == mpf_float[float_index_y];

	// push result on stack
	sp = stpushi (float_res, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("mp_equal_float: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

extern "C" U1 *mp_not_equal_float (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 float_index_x ALIGN;
	S8 float_index_y ALIGN;
	S8 float_res ALIGN;

	sp = stpopi ((U1 *) &float_index_y, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_not_equal_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &float_index_x, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("mp_not_equal_float: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (float_index_x >= MAX_FLOAT_NUM || float_index_x < 0)
	{
		printf ("mp_not_equal_float: ERROR float index x out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	if (float_index_y >= MAX_FLOAT_NUM || float_index_y < 0)
	{
		printf ("mp_not_equal_float: ERROR float index y out of range! Must be 0 < %i\n", MAX_FLOAT_NUM);
		return (NULL);
	}

	float_res = mpf_float[float_index_x] != mpf_float[float_index_y];

	// push result on stack
	sp = stpushi (float_res, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("mp_not_equal_float: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}
