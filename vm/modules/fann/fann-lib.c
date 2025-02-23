/*
 * This file fann-lib.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (info@midnight-coding.de), 2019
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

#define ANNOPEN 1              // state flags
#define ANNCLOSED 0

struct fanns
{
    struct fann *ann;
    U1 state;
};

static struct fanns *fanns = NULL;
static S8 fannmax ALIGN = 0;

S2 init_memory_bounds (struct data_info *data_info_orig, S8 data_info_ind_orig)
{
	return (0);
}

U1 get_sandbox_filename (U1 *filename, U1 *sandbox_filename, S2 max_name_len);

U1 *fann_init_state (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 i ALIGN;
	S8 maxind ALIGN;
	
	sp = stpopi ((U1 *) &maxind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("fann_init_state: ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	// allocate gobal mem structure
	fanns = (struct fanns *) calloc (maxind, sizeof (struct fanns));
	if (fanns == NULL)
	{
		printf ("fann_init_state: ERROR can't allocate %lli memory indexes!\n", maxind);
		
		sp = stpushi (1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("fann_init_state: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}
	
	fannmax = maxind;	// save to global var
	
	for (i = 0; i < fannmax; i++)
	{
		fanns[i].state = ANNCLOSED;
	}

	// error code ok
	sp = stpushi (0, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("fann_init_state: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *free_mem (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	if (fanns) free (fanns);
	return (sp);
}

S8 get_free_fann (void)
{
	S8 i ALIGN;
	
	for (i = 0; i < fannmax; i++)
	{
		if (fanns[i].state == ANNCLOSED)
		{
			return (i);
		}
	}
	
	// no free memory found
	return (-1);
}

U1 *fann_create_ann (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 handle ALIGN;
	S8 layers ALIGN;
    S8 inputs ALIGN;
    S8 outputs ALIGN;
    S8 neurons_hidden ALIGN;

    sp = stpopi ((U1 *) &outputs, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: fann_create_ann: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

    sp = stpopi ((U1 *) &neurons_hidden, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: fann_create_ann: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

    sp = stpopi ((U1 *) &inputs, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: fann_create_ann: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

    sp = stpopi ((U1 *) &layers, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: fann_create_ann: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

	handle = get_free_fann ();
	if (handle == -1)
	{
		// ERROR no more free handle!

		printf ("fann_create_ann: no more fann slot free!\n");
		return (NULL);
	}

	if (handle < 0 || handle >= fannmax)
    {
        printf ("ERROR: fann_create_ann: handle out of range!\n");
        return (NULL);
    }

    fanns[handle].ann = (struct fann *) fann_create_standard (layers, inputs, neurons_hidden, outputs);
    if (fanns[handle].ann == NULL)
    {
        printf ("fann_create_ann: can't allocate fann!\n");
        return (NULL);
    }

    fann_set_activation_function_hidden(fanns[handle].ann, FANN_SIGMOID_SYMMETRIC);
    fann_set_activation_function_output(fanns[handle].ann, FANN_SIGMOID_SYMMETRIC);

    fann_randomize_weights (fanns[handle].ann, -1.0, 1.0);

    fanns[handle].state = ANNOPEN;

    // return handle
	sp = stpushi (handle, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("fann_create_ann: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *fann_read_ann (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 nameaddr ALIGN;
	S8 handle ALIGN;
	
	U1 sandbox_filename[256];
	
    sp = stpopi ((U1 *) &nameaddr, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: fann_read_ann: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

	handle = get_free_fann ();
	if (handle == -1)
	{
		// ERROR no more free handle!
		
		printf ("fann_read_ann: no more fann slot free!\n");
		return (NULL);
	}

	if (handle < 0 || handle >= fannmax)
    {
        printf ("ERROR: fann_read_ann: handle out of range!\n");
        return (NULL);
    }

	if (fanns[handle].state == ANNOPEN)
	{
		printf ("ERROR: fann_read_ann: handle already used!\n");
        return (NULL);
	}

#if SANDBOX 
	if (get_sandbox_filename (&data[nameaddr], sandbox_filename, 255) != 0)
	{
		printf ("ERROR: fann_read_ann: ERROR filename illegal: %s\n", &data[nameaddr]);
		return (NULL);
	}
	fanns[handle].ann = (struct fann *) fann_create_from_file ((const char *) sandbox_filename);
#else
	fanns[handle].ann = (struct fann *) fann_create_from_file ((const char *) &data[nameaddr]);
#endif
	fanns[handle].state = ANNOPEN;

	// return handle
	sp = stpushi (handle, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("fann_read_ann: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *fann_save_ann (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 nameaddr ALIGN;
	S8 handle ALIGN;

	U1 sandbox_filename[256];
    S8 ret ALIGN = 1;

    sp = stpopi ((U1 *) &nameaddr, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: fann_save_ann: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

    sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: fann_save_ann: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

	if (handle < 0 || handle >= fannmax)
    {
        printf ("ERROR: fann_save_ann: handle out of range!\n");
        return (NULL);
    }

#if SANDBOX
	if (get_sandbox_filename (&data[nameaddr], sandbox_filename, 255) != 0)
	{
		printf ("ERROR: fann_save_ann: ERROR filename illegal: %s\n", &data[nameaddr]);
		return (NULL);
	}

    ret = fann_save (fanns[handle].ann, (const char *) sandbox_filename);
#else
    ret = fann_save (fanns[handle].ann, (const char *) &data[nameaddr]);
#endif

	// return error code
	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("fann_save_ann: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *fann_run_ann (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 inputs ALIGN;
	S8 outputs ALIGN;
	S8 inputs_num ALIGN;
	S8 outputs_num ALIGN;
	S8 handle ALIGN;

	fann_type *output_d;

	U1 *ret_data = data;

	F8 *dst ALIGN;
	S8 i ALIGN;
	S8 j ALIGN;
	S8 offset ALIGN;

	U1 *src_p;
	U1 *dst_p;
	F8 data_f ALIGN = 0.0;

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

	sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: fann_run_ann: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

	if (handle < 0 || handle >= fannmax)
    {
        printf ("ERROR: fann_run_ann: handle out of range!\n");
        return (NULL);
    }

	if (fanns[handle].state != ANNOPEN)
	{
		printf ("ERROR: fann_run_ann: handle not loaded!\n");
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

	output_d = fann_run (fanns[handle].ann, input_f);

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
	S8 handle ALIGN;

	sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: fann_free_ann: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

	if (handle < 0 || handle >= fannmax)
    {
        printf ("ERROR: fann_free_ann: handle out of range!\n");
        return (NULL);
    }

	if (fanns[handle].state != ANNOPEN)
	{
		printf ("ERROR: fann_free_ann: handle not loaded!\n");
        return (NULL);
	}

	fann_destroy (fanns[handle].ann);
	fanns[handle].state = ANNCLOSED;
	return (sp);
}

U1 *fann_train_ann (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 inputs ALIGN;
	S8 outputs ALIGN;
	S8 inputs_num ALIGN;
	S8 outputs_num ALIGN;
	S8 handle ALIGN;

	S8 i ALIGN;
	S8 j ALIGN;
	S8 offset ALIGN;

	U1 *src_p;
	U1 *dst_p;
	F8 data_f ALIGN = 0.0;

	fann_type *input_f = NULL;
    fann_type *output_f = NULL;

    sp = stpopi ((U1 *) &outputs_num, sp, sp_top);
	if (sp == NULL)
	{
	   // error
	   printf ("ERROR: fann_train_ann: ERROR: stack corrupt!\n");
	   return (NULL);
	}

	sp = stpopi ((U1 *) &inputs_num, sp, sp_top);
	if (sp == NULL)
	{
	   // error
	   printf ("ERROR: fann_train_ann: ERROR: stack corrupt!\n");
	   return (NULL);
	}

	sp = stpopi ((U1 *) &outputs, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: fann_train_ann: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

	sp = stpopi ((U1 *) &inputs, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: fann_train_ann: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

	sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: fann_train_ann: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

	if (handle < 0 || handle >= fannmax)
    {
        printf ("ERROR: fann_train_ann: handle out of range!\n");
        return (NULL);
    }

/*
    if (fanns[handle].state != ANNOPEN)
	{
		printf ("ERROR: fann_train_ann: handle not loaded!\n");
        return (NULL);
    }
*/

	input_f = calloc (inputs_num, sizeof (F8));
	if (input_f == NULL)
	{
		printf ("ERROR: fann_train_ann: out of memory!\n");
		return (NULL);
	}

    output_f = calloc (outputs_num, sizeof (F8));
	if (output_f == NULL)
	{
        free (input_f);

		printf ("ERROR: fann_train_ann: out of memory!\n");
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

    offset = 0;
	for (j = 0; j < outputs_num; j++)
	{
		src_p = &data[outputs + offset];
		dst_p = (U1 *) &data_f;
		for (i = 0; i < sizeof (F8); i++)
		{
			*dst_p++ = *src_p++;
		}
		output_f[j] = data_f;
		offset += sizeof (F8);
	}

    fann_train (fanns[handle].ann, input_f, output_f);

    free (output_f);
	free (input_f);
    return (sp);
}

U1 *fann_train_ann_file (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 nameaddr ALIGN;
    S8 max_epochs ALIGN;
    S8 epochs_between_reports ALIGN;
    F8 desired_error ALIGN;
	S8 handle ALIGN;

	U1 sandbox_filename[256];

    sp = stpopd ((U1 *) &desired_error, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: fann_train_ann_file: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

    sp = stpopi ((U1 *) &epochs_between_reports, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: fann_train_ann_file: ERROR: stack corrupt!\n");
 	   return (NULL);
    }


    sp = stpopi ((U1 *) &max_epochs, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: fann_train_ann_file: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

    sp = stpopi ((U1 *) &nameaddr, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: fann_train_ann_file: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

    sp = stpopi ((U1 *) &handle, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: fann_train_ann_file: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

	if (handle < 0 || handle >= fannmax)
    {
        printf ("ERROR: fann_train_ann_file: handle out of range!\n");
        return (NULL);
    }

#if SANDBOX
	if (get_sandbox_filename (&data[nameaddr], sandbox_filename, 255) != 0)
	{
		printf ("ERROR: fann_train_ann_file: ERROR filename illegal: %s\n", &data[nameaddr]);
		return (NULL);
	}

    fann_train_on_file (fanns[handle].ann, (const char *) sandbox_filename, max_epochs, epochs_between_reports, desired_error);
#else
    fann_train_on_file (fanns[handle].ann, (const char *) &data[nameaddr], max_epochs, epochs_between_reports, desired_error);
#endif

	return (sp);
}
