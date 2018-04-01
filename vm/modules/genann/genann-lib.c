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

static genann *ann;

U1 *genann_read_ann (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
   FILE *input;

   S8 i = 0 ALIGN;
   U1 name[512];
   U1 err = 0;
   
   while (*sp != 0)
	{
		if (i < 512)
		{
			name[i] = *sp;
			i++;
			sp++;
			if (sp == sp_top)
			{
				printf ("FATAL ERROR: genann_read_ann stack corrupt!\n");
				err = 1;
			}
		}
	}
	name[i] = '\0';        // end of string
	
	if (err == 1)
	{
		printf ("genann_read_ann ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	input = fopen ((const char *) name, "r");
    if (input)
    {
        ann = (genann *) genann_read (input);
        fclose (input);
        
        sp = stpushb (0, sp, sp_bottom);		// error ok code
        return (sp);
    }
    else
    {
        sp = stpushb (1, sp, sp_bottom);		// error FAIL code
        return (NULL);
    }
}

U1 *genann_write_ann (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    FILE *output;
    
    S8 i = 0 ALIGN;
    U1 name[512];
    U1 err = 0;
   
    while (*sp != 0)
	{
		if (i < 512)
		{
			name[i] = *sp;
			i++;
			sp++;
			if (sp == sp_top)
			{
				printf ("FATAL ERROR: genann_read_ann stack corrupt!\n");
				err = 1;
			}
		}
	}
	name[i] = '\0';        // end of string
	
	if (err == 1)
	{
		printf ("genann_write_ann ERROR: stack corrupt!\n");
		return (NULL);
	}
	
	output = fopen ((const char *) name, "w");
    if (output)
    {
        genann_write (ann, output);
        fclose (output);
        
        sp = stpushb (0, sp, sp_bottom);		// error ok code
        return (sp);
    }
    else
    {
        sp = stpushb (1, sp, sp_bottom);		// error FAIL code
        return (NULL);
    }
}

U1 *genann_init_ann (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 outputs ALIGN;
    S8 inputs ALIGN;
    S8 hidden_layers ALIGN;
    S8 hidden ALIGN;
    
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
    
    ann = (genann *) genann_init (inputs, hidden_layers, hidden, outputs);
    if (ann != NULL)
    {
        sp = stpushb (0, sp, sp_bottom);		// error OK code
        return (sp);
    }
    else
    {
        sp = stpushb (1, sp, sp_bottom);		// FAIL code
        return (NULL);
    }
}

U1 *genann_train_ann (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 learning_rate ALIGN;
    S8 desired_output ALIGN;
    S8 inputs ALIGN;
    
    U1 err = 0;
    U1 *ret_data = data;
    
    F8 *input;
    F8 *output;
    
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
	
	if (err == 1)
    {
        printf ("FATAL ERROR: genann_train_ann stack corrupt!\n");
        return (NULL);
    }
    
    input = (F8 *) &ret_data[inputs];
    output = (F8 *) &ret_data[desired_output];
    
    // printf ("genann_train_ann: 1: %lf, 2: %lf\n", input[0], input[1]);
    genann_train ((genann *) ann, (const double *) input, (const double *) output, learning_rate);
    return (sp);
}

U1 *genann_run_ann (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    F8 *output;
    F8 *input_d;
    U1 err = 0;
    
    S8 inputs ALIGN;
    S8 outputs ALIGN;
    S8 outputs_num ALIGN;
    
    U1 *ret_data = data;
    
    S8 i ALIGN;
    S8 j ALIGN;
    U1 *src, *dst;
    
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
	
	if (err == 1)
    {
        printf ("FATAL ERROR: genann_run_ann stack corrupt!\n");
        return (NULL);
    }
    
    input_d = (F8 *) &ret_data[inputs];
    // printf ("genann_run_ann: input1: %lf, input2: %lf\n", input_d[0], input_d[1]);
    output  = (F8 *) genann_run ((genann *) ann, input_d);
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
