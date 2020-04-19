/*
 * This file genann-lib.c is part of L1vm.
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
#include "../../../include/stack.h"

#include "genann.h"

#define ANNOPEN 1              // state flags
#define ANNCLOSED 0

struct genanns
{
	struct genann *ann;
	U1 state;
};

static struct genanns *genanns = NULL;
static S8 genannmax ALIGN = 0;


U1 get_sandbox_filename (U1 *filename, U1 *sandbox_filename, S2 max_name_len);

U1 *genann_init_state (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 i ALIGN;
	S8 maxind ALIGN;
	
	sp = stpopi ((U1 *) &maxind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("genann_init_state: ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	// allocate gobal mem structure
	genanns = (struct genanns *) calloc (maxind, sizeof (struct genanns));
	if (genanns == NULL)
	{
		printf ("genann_init_state: ERROR can't allocate %lli memory indexes!\n", maxind);
		
		sp = stpushi (1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("genann_init_state: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}
	
	genannmax = maxind;	// save to global var
	
	for (i = 0; i < genannmax; i++)
	{
		genanns[i].state = ANNCLOSED;
	}
	
	// error code ok
	sp = stpushi (0, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("genann_init_state: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *free_mem (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	if (genanns) free (genanns);
	return (sp);
}

S8 get_free_genann (void)
{
	S8 i ALIGN;
	
	for (i = 0; i < genannmax; i++)
	{
		if (genanns[i].state == ANNCLOSED)
		{
			return (i);
		}
	}
	
	// no free memory found
	return (-1);
}

U1 *genann_read_ann (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
   FILE *input;
   S8 nameaddr ALIGN;
	S8 handle ALIGN;
   
   U1 sandbox_filename[256];
   
   sp = stpopi ((U1 *) &nameaddr, sp, sp_top);
   if (sp == NULL)
   {
	   // error
	   printf ("ERROR: genann_read_ann: ERROR: stack corrupt!\n");
	   return (NULL);
   }

   handle = get_free_genann ();
   if (handle == -1)
   {
	   // ERROR no more free handle!
	   
	   printf ("genann_read_ann: no more fann slot free!\n");
	   return (NULL);
   }
   
   if (handle < 0 || handle >= genannmax)
   {
	   printf ("ERROR: genann_read_ann: handle out of range!\n");
	   return (NULL);
   }
   
#if SANDBOX 
   if (get_sandbox_filename (&data[nameaddr], sandbox_filename, 255) != 0)
   {
	   printf ("ERROR: genann_read_ann: ERROR filename illegal: %s\n", &data[nameaddr]);
	   return (NULL);
   }
	input = fopen ((const char *) sandbox_filename, "r");
#else
	input = fopen ((const char *) &data[nameaddr], "r");
#endif
	
    if (input)
    {
        genanns[handle].ann = (genann *) genann_read (input);
        fclose (input);

        sp = stpushb (handle, sp, sp_bottom);		// error ok code
        return (sp);
    }
    else
    {
        sp = stpushb (-1, sp, sp_bottom);		// error FAIL code
        return (sp);
    }
}

U1 *genann_write_ann (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    FILE *output;
    S8 nameaddr ALIGN;
	S8 handle ALIGN;
	
	U1 sandbox_filename[256];
	
	sp = stpopi ((U1 *) &nameaddr, sp, sp_top);
    if (sp == NULL)
    {
 	   // error
 	   printf ("ERROR: genann_write_ann: ERROR: stack corrupt!\n");
 	   return (NULL);
    }

    sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: genann_write_ann: ERROR: stack corrupt!\n");
		return (NULL);
	}
    
    if (handle < 0 || handle >= genannmax)
	{
		printf ("ERROR: genann_write_ann: handle out of range!\n");
		return (NULL);
	}
    
#if SANDBOX 
    if (get_sandbox_filename (&data[nameaddr], sandbox_filename, 255) != 0)
	{
		printf ("ERROR: genann_write_ann: ERROR filename illegal: %s\n", &data[nameaddr]);
		return (NULL);
	}
	output = fopen ((const char *) sandbox_filename, "w");
#else
	output = fopen ((const char *) &data[nameaddr], "w");
#endif
	
    if (output)
    {
        genann_write (genanns[handle].ann, output);
        fclose (output);

        sp = stpushb (0, sp, sp_bottom);		// error ok code
		if (sp == NULL)
		{
			// error
			printf ("ERROR: genann_write_ann: ERROR: stack corrupt!\n");
			return (NULL);
		}
        return (sp);
    }
    else
    {
        sp = stpushb (1, sp, sp_bottom);		// error FAIL code
		if (sp == NULL)
		{
			// error
			printf ("ERROR: genann_write_ann: ERROR: stack corrupt!\n");
			return (NULL);
		}
        return (sp);
    }
}

U1 *genann_init_ann (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 outputs ALIGN;
    S8 inputs ALIGN;
    S8 hidden_layers ALIGN;
    S8 hidden ALIGN;
	S8 handle ALIGN;
	
    U1 err = 0;

    sp = stpopi ((U1 *) &outputs, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &hidden, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &hidden_layers, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &inputs, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
    }
    
    if (err == 1)
    {
        printf ("FATAL ERROR: genann_init_ann stack corrupt!\n");
        return (NULL);
    }

    handle = get_free_genann ();
	if (handle == -1)
	{
		// ERROR no more free handle!
		
		printf ("genann_init_ann: no more genann slot free!\n");
		return (NULL);
	}
	
	if (handle < 0 || handle >= genannmax)
	{
		printf ("ERROR: genann_init_ann: handle out of range!\n");
		return (NULL);
	}
    
    genanns[handle].ann = (genann *) genann_init (inputs, hidden_layers, hidden, outputs);
    if (genanns[handle].ann != NULL)
    {
        sp = stpushb (handle, sp, sp_bottom);		// error OK code
		if (sp == NULL)
		{
			// error
			printf ("ERROR: genann_init_ann: ERROR: stack corrupt!\n");
			return (NULL);
		}
        return (sp);
    }
    else
    {
        sp = stpushb (-1, sp, sp_bottom);		// FAIL code
		if (sp == NULL)
		{
			// error
			printf ("ERROR: genann_init_ann: ERROR: stack corrupt!\n");
			return (NULL);
		}
        return (sp);
    }
}

U1 *genann_free_ann (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
	
	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: genann_free_ann: ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	if (handle < 0 || handle >= genannmax)
	{
		printf ("ERROR: genann_free_ann: handle out of range!\n");
		return (NULL);
	}
	
	if (genanns[handle].ann) 
	{
		// free ANN
		genann_free (genanns[handle].ann);
		genanns[handle].ann = NULL;
		genanns[handle].state = ANNOPEN;
	}
    return (sp);
}

U1 *genann_train_ann (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 learning_rate ALIGN;
    S8 desired_output ALIGN;
    S8 inputs ALIGN;
	S8 handle ALIGN;
	
    U1 err = 0;
    U1 *ret_data = data;

    F8 *input ALIGN;
    F8 *output ALIGN;

    sp = stpopi ((U1 *) &learning_rate, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

    sp = stpopi ((U1 *) &desired_output, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &inputs, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}
	
	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	if (handle < 0 || handle >= genannmax)
	{
		printf ("ERROR: genann_train_ann: handle out of range!\n");
		return (NULL);
	}
	
	if (err == 1)
    {
        printf ("FATAL ERROR: genann_train_ann stack corrupt!\n");
        return (NULL);
    }

    input = (F8 *) &ret_data[inputs];
    output = (F8 *) &ret_data[desired_output];

    // printf ("genann_train_ann: 1: %lf, 2: %lf\n", input[0], input[1]);
    genann_train ((genann *) genanns[handle].ann, (const double *) input, (const double *) output, learning_rate);
    return (sp);
}

U1 *genann_run_ann (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    F8 *output ALIGN;
    F8 *input_d ALIGN;
    U1 err = 0;

    S8 inputs ALIGN;
    S8 outputs ALIGN;
    S8 outputs_num ALIGN;

    U1 *ret_data = data;

    S8 i ALIGN;
    S8 j ALIGN;
    U1 *src, *dst;

	S8 handle ALIGN;
	
    // get number of output values
    sp = stpopi ((U1 *) &outputs_num, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

    sp = stpopi ((U1 *) &outputs, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

	sp = stpopi ((U1 *) &inputs, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}

    sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		err = 1;
	}
	
	if (err == 1)
	{
		printf ("FATAL ERROR: genann_run_ann stack corrupt!\n");
		return (NULL);
	}
	
	if (handle < 0 || handle >= genannmax)
	{
		printf ("ERROR: genann_run_ann: handle out of range!\n");
		return (NULL);
	}
    
    input_d = (F8 *) &ret_data[inputs];
    // printf ("genann_run_ann: input1: %lf, input2: %lf\n", input_d[0], input_d[1]);
    output  = (F8 *) genann_run ((genann *) genanns[handle].ann, input_d);
    // printf ("genann_run_ann output %lf\n", *output);
    // ret_data = ret_data + outputs;
    //  output_d = (F8 *) ret_data;
    // *output_d = *output;

    src = (U1 *) output;
    dst = (U1 *) ret_data + outputs;

    for (j = outputs_num; j > 0; j--)
    {
        for (i = 0; i < sizeof (F8); i++)
        {
            *dst = *src;
            dst++; src++;
        }
    }

    // printf ("genann_run_ann: output: %lf\n", *output);

    return (sp);
}
