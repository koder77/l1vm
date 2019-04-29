/*
 * This file fann-lib.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2019
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

// This is the FANN neural networks library for L1VM
// You can load a trained FANN network and run it.

#include "../../../include/global.h"
#include "../../../include/stack.h"

#include <floatfann.h>

static struct fann *ann;

U1 *fann_read_ann (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 nameaddr ALIGN;

    sp = stpopi ((U1 *) &nameaddr, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: fann_read_ann: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

	ann = (struct fann *) fann_create_from_file ((const char *) &data[nameaddr]);

	return (sp);
}

U1 *fann_run_ann (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 inputs ALIGN;
	S8 outputs ALIGN;
	S8 inputs_num ALIGN;
	S8 outputs_num ALIGN;

	fann_type *output_d;

	U1 *ret_data = data;

	F8 *dst ALIGN;
	S8 i ALIGN;
	S8 j ALIGN;
	S8 offset ALIGN;

	U1 *src_p;
	U1 *dst_p;
	F8 data_f;

	fann_type *input_f;

	sp = stpopi ((U1 *) &outputs_num, sp, sp_top);
	if (sp == NULL)
	{
	   // error
	   printf ("ERROR: fann_run_ann: ERROR: stack corrupt!\n");
	   return (NULL);
	}

	sp = stpopi ((U1 *) &inputs_num, sp, sp_top);
	if (sp == NULL)
	{
	   // error
	   printf ("ERROR: fann_run_ann: ERROR: stack corrupt!\n");
	   return (NULL);
	}

	sp = stpopi ((U1 *) &outputs, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: fann_run_ann: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

	sp = stpopi ((U1 *) &inputs, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: fann_run_ann: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

	input_f = calloc (inputs_num, sizeof (F8));
	if (input_f == NULL)
	{
		printf ("ERROR: fann_run_ann: out of memory!\n");
		return (NULL);
	}

	// It's so ugly, but works!!!

	offset = 0;
	for (j = 0; j < inputs_num; j++)
	{
		src_p = &data[inputs + offset];
		dst_p = (U1 *) &data_f;
		for (i = 0; i < sizeof (F8); i++)
		{
			*dst_p++ = *src_p++;
		}
		input_f[j] = data_f;
		offset += sizeof (F8);
	}

	output_d = fann_run (ann, input_f);

    dst = (F8 *) &ret_data[outputs];
    for (j = 0; j < outputs_num; j++)
    {
		*dst = output_d[j];
		dst++;
    }

	free (input_f);
    return (sp);
}

U1 *fann_free_ann (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	fann_destroy (ann);

	return (sp);
}
