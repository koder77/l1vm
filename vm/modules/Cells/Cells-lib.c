/*
 * This file Cells-lib.c is part of L1vm.
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

// This is the L1VM wrapper library for my Cells FANN library:
// https://github.com/koder77/Cells

#include "../../../include/global.h"
#include "../../../include/stack.h"

#include <cells.h>



U1 get_sandbox_filename (U1 *filename, U1 *sandbox_filename, S2 max_name_len);

// global struct
static struct cell *cells = NULL;
static S8 ALIGN cells_number = 0;

S2 init_memory_bounds (struct data_info *data_info_orig, S8 data_info_ind_orig)
{
	return (0);
}

U1 *cells_alloc (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S2 ret;

	sp = stpopi ((U1 *) &cells_number, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_alloc: ERROR: stack corrupt!\n");
		return (NULL);
	}

	cells = (struct cell *) calloc (cells_number, sizeof (struct cell));
	if (cells == NULL)
	{
		printf ("cells_alloc: ERROR: can't allocate %lli cells!\n", cells_number);

		ret = 1;
	}
	else
	{
		ret = 0;
	}

	// push return value to stack
	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("cells_alloc: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

U1 *cells_dealloc (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	if (cells) free (cells);
	// set cells to NULL, to mark it as not allocated
	cells = NULL;
	return (sp);
}

U1 *cells_alloc_neurons_equal (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 max_cells ALIGN;
	S8 neurons ALIGN;
	S2 ret;

	sp = stpopi ((U1 *) &neurons, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_alloc_neurons_equal: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &max_cells, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_alloc_neurons_equal: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (max_cells > cells_number)
	{
		// error
		printf ("cells_alloc_neurons_equal: ERROR: max cells to high!\n");
		return (NULL);
	}

	ret = Cells_alloc_neurons_equal (cells, max_cells, neurons);

	// push return value to stack
	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("cells_alloc_neurons_equal: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

U1 *cells_alloc_neurons (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 cell ALIGN;
	S8 neurons ALIGN;
	S2 ret;

	sp = stpopi ((U1 *) &neurons, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_alloc_neurons: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &cell, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_alloc_neurons: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (cell > cells_number)
	{
		// error
		printf ("cells_alloc_neurons: ERROR: cell to high!\n");
		return (NULL);
	}

	ret = Cells_alloc_neurons (cells, cell, neurons);

	// push return value to stack
	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("cells_alloc_neurons: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

U1 *cells_dealloc_neurons (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 max_cells ALIGN;
	S2 ret;

	sp = stpopi ((U1 *) &max_cells, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_dealloc_neurons: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (max_cells > cells_number)
	{
		// error
		printf ("cells_dealloc_neurons: ERROR: max cells to high!\n");
		return (NULL);
	}

	ret = Cells_dealloc_neurons (cells, max_cells);

	// push return value to stack
	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("cells_dealloc_neurons: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

U1 *cells_fann_do_update_ann (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 cell ALIGN;
	S8 node ALIGN;
	S8 inputs_node_addr ALIGN;
	S2 ret;
	U1 *src_p;
	U1 *dst_p;
	F8 *inputf ALIGN;
	S8 i ALIGN;
	S8 j ALIGN;
	S8 offset ALIGN;
	F8 dataf ALIGN;
	S8 inputs ALIGN;

	sp = stpopi ((U1 *) &inputs_node_addr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_do_update_ann: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &inputs, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_do_update_ann: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &node, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_do_update_ann: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &cell, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_do_update_ann: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (cell > cells_number)
	{
		// error
		printf ("cells_fann_do_update_ann: ERROR: max cells to high!\n");
		return (NULL);
	}


	inputf = calloc (inputs, sizeof (F8));
	if (inputf == NULL)
	{
		// error
		printf ("cells_fann_do_update_ann: ERROR: out of memory alloacting inputf!\n");
		return (NULL);
	}

	// copy data to allocated inputf and outputf
	offset = 0;
	for (i = 0; i < inputs; i++)
	{
		src_p = &data[inputs_node_addr + offset];
		dst_p = (U1 *) &dataf;
		for (j = 0; j < sizeof (F8); j++)
		{
			*dst_p++ = *src_p++;
		}
		inputf[i] = dataf;
		offset += sizeof (F8);

		// printf ("cells_fann_read_ann: input: %lli, value: %lf\n", i, inputf[i]);
	}

	ret = Cells_fann_do_update_ann (cells, cell, node, inputf);

	free (inputf);

	// push return value to stack
	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_do_update_ann: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

U1 *cells_fann_read_ann (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 cell ALIGN;
	S8 node ALIGN;
	S8 filename_addr ALIGN;
	S8 inputs ALIGN;
	S8 outputs ALIGN;
	S8 inputs_node_addr ALIGN;
	S8 outputs_node_addr ALIGN;
	S8 layer ALIGN;
	S8 init ALIGN;
	S2 ret;
	U1 *src_p;
	U1 *dst_p;
	F8 *inputf ALIGN;
	F8 *outputf ALIGN;
	S8 i ALIGN;
	S8 j ALIGN;
	S8 offset ALIGN;
	F8 dataf ALIGN;

	// sandbox filename
	U1 sandbox_filename[256];

	sp = stpopi ((U1 *) &init, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_read_ann: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &layer, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_read_ann: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &outputs_node_addr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_read_ann: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &inputs_node_addr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_read_ann: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &outputs, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_read_ann: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &inputs, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_read_ann: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &filename_addr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_read_ann: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &node, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_read_ann: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &cell, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_read_ann: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (cell > cells_number)
	{
		// error
		printf ("cells_fann_read_ann: ERROR: cell to high!\n");
		return (NULL);
	}

	inputf = calloc (inputs, sizeof (F8));
	if (inputf == NULL)
	{
		// error
		printf ("cells_fann_read_ann: ERROR: out of memory alloacting inputf!\n");
		return (NULL);
	}

	outputf = calloc (outputs, sizeof (F8));
	if (outputf == NULL)
	{
		// error
		free (inputf);
		printf ("cells_fann_read_ann: ERROR: out of memory alloacting outputf!\n");
		return (NULL);
	}

	// copy data to allocated inputf and outputf
	offset = 0;
	for (i = 0; i < inputs; i++)
	{
		src_p = &data[inputs_node_addr + offset];
		dst_p = (U1 *) &dataf;
		for (j = 0; j < sizeof (F8); j++)
		{
			*dst_p++ = *src_p++;
		}
		inputf[i] = dataf;
		offset += sizeof (F8);

		// printf ("cells_fann_read_ann: input: %lli, value: %lf\n", i, inputf[i]);
	}

	offset = 0;
	for (i = 0; i < outputs; i++)
	{
		src_p = &data[outputs_node_addr + offset];
		dst_p = (U1 *) &dataf;
		for (j = 0; j < sizeof (F8); j++)
		{
			*dst_p++ = *src_p++;
		}
		outputf[i] = dataf;
		offset += sizeof (F8);
	}

#if SANDBOX
	if (get_sandbox_filename (&data[filename_addr], sandbox_filename, 255) != 0)
	{
		printf ("cells_fann_read_ann: ERROR filename illegal: %s\n", &data[filename_addr]);
		return (NULL);
	}
	ret = Cells_fann_read_ann (cells, cell, node, sandbox_filename, inputs, outputs, inputf, outputf, layer, init);
#else
	ret = Cells_fann_read_ann (cells, cell, node, &data[filename_addr], inputs, outputs, inputf, outputf, layer, init);
#endif

	// free allocated buffers
	free (outputf);
	free (inputf);

	// push return value to stack
	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_read_ann: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

U1 *cells_fann_run_ann (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 cell ALIGN;
	S8 node ALIGN;
	S2 ret;

	sp = stpopi ((U1 *) &node, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_run_ann: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &cell, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_run_ann: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (cell > cells_number)
	{
		// error
		printf ("cells_fann_run_ann: ERROR: cell to high!\n");
		return (NULL);
	}

	ret = Cells_fann_run_ann (cells, cell, node);

	// push return value to stack
	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_run_ann: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

U1 *cells_fann_get_output (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 cell ALIGN;
	S8 node ALIGN;
	S8 output ALIGN;
	S8 return_value_addr ALIGN;
	S2 ret;
	U1 *src_p;
	U1 *dst_p;
	F8 outputf ALIGN;
	S8 i ALIGN;

	sp = stpopi ((U1 *) &return_value_addr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_get_output: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &output, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_get_output: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &node, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_get_output: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &cell, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_get_output: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (cell > cells_number)
	{
		// error
		printf ("cells_fann_get_output: ERROR: cell to high!\n");
		return (NULL);
	}

	ret = Cells_fann_get_output (cells, cell, node, output, &outputf);

	// printf ("cells_fann_get_output: output: %lf\n", outputf);
	// printf ("cells_fann_get_output: return_value_addr: %lli\n", return_value_addr);

	// copy value to data
	dst_p = &data[return_value_addr];
	src_p = (U1 *) &outputf;
	for (i = 0; i < sizeof (F8); i++)
	{
		*dst_p++ = *src_p++;
	}

	// push return value to stack
	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_get_output: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

U1 *cells_alloc_node_links (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 cell ALIGN;
	S8 node ALIGN;
	S8 links ALIGN;
	S2 ret;

	sp = stpopi ((U1 *) &links, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_alloc_node_links: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &node, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_alloc_node_links: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &cell, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_alloc_node_links: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (cell > cells_number)
	{
		// error
		printf ("cells_alloc_node_links: ERROR: cell to high!\n");
		return (NULL);
	}

	ret = Cells_alloc_node_links (cells, cell, node, links);

	// push return value to stack
	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("cells_alloc_node_links: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

U1 *cells_dealloc_node_links (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 cell ALIGN;
	S8 node ALIGN;
	S2 ret;

	sp = stpopi ((U1 *) &node, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_dealloc_node_links: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &cell, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_dealloc_node_links: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (cell > cells_number)
	{
		// error
		printf ("cells_dealloc_node_links: ERROR: cell to high!\n");
		return (NULL);
	}

	ret = Cells_dealloc_node_links (cells, cell, node);

	// push return value to stack
	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("cells_dealloc_node_links: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

U1 *cells_set_node_link (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 cell ALIGN;
	S8 node ALIGN;
	S8 link ALIGN;
	S8 link_node ALIGN;
	S8 input ALIGN;
	S8 output ALIGN;
	S2 ret;

	sp = stpopi ((U1 *) &output, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_set_node_link: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &input, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_set_node_link: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &link_node, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_set_node_link: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &link, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_set_node_link: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &node, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_set_node_link: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &cell, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_set_node_link: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (cell > cells_number)
	{
		// error
		printf ("cells_set_node_link: ERROR: cell to high!\n");
		return (NULL);
	}

	ret = Cells_set_node_link (cells, cell, node, link, link_node, input, output);

	// push return value to stack
	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("cells_set_node_link: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

U1 *cells_fann_get_max_layer (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 start_cell ALIGN;
	S8 end_cell ALIGN;
	S8 max_layer_addr ALIGN;
	S2 ret;

	sp = stpopi ((U1 *) &max_layer_addr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_get_max_layer: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &end_cell, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_get_max_layer: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start_cell, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_get_max_layer: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (start_cell > cells_number)
	{
		// error
		printf ("cells_fann_get_max_layer: ERROR: start cell to high!\n");
		return (NULL);
	}

	if (end_cell > cells_number)
	{
		// error
		printf ("cells_fann_get_max_layer: ERROR: end cell to high!\n");
		return (NULL);
	}

	ret = Cells_fann_get_max_layer (cells, start_cell, end_cell, (S8 *) &data[max_layer_addr]);

	// push return value to stack
	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_get_max_layer: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

U1 *cells_fann_get_max_nodes (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 cell ALIGN;
	S8 neurons_max_addr ALIGN;
	S2 ret;

	sp = stpopi ((U1 *) &neurons_max_addr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_get_max_nodes: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &cell, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_get_max_nodes: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (cell > cells_number)
	{
		// error
		printf ("cells_fann_get_max_nodes: ERROR: cell to high!\n");
		return (NULL);
	}

	ret = Cells_fann_get_max_nodes (cells, cell, (S8 *) &data[neurons_max_addr]);

	// push return value to stack
	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_get_max_nodes: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

U1 *cells_fann_run_ann_go_links (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 start_cell ALIGN;
	S8 end_cell ALIGN;
	S8 start_layer ALIGN;
	S8 end_layer ALIGN;
	S2 ret;

	sp = stpopi ((U1 *) &end_layer, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_run_ann_go_links: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start_layer, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_run_ann_go_links: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &end_cell, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_run_ann_go_links: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start_cell, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_run_ann_go_links: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (start_cell >= cells_number)
	{
		// error
		printf ("cells_fann_run_ann_go_links: ERROR: start cell to high!\n");
		return (NULL);
	}

	if (end_cell > cells_number)
	{
		// error
		printf ("cells_fann_run_ann_go_links: ERROR: end cell to high!\n");
		return (NULL);
	}

	ret = Cells_fann_run_ann_go_links (cells, start_cell, end_cell, start_layer, end_layer);

	// push return value to stack
	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_run_ann_go_links: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

U1 *cells_fann_save_cells (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 start_cell ALIGN;
	S8 end_cell ALIGN;
	S8 filename_addr ALIGN;
	S2 ret;

	U1 sandbox_filename[256];

	sp = stpopi ((U1 *) &filename_addr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_save_cells: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &end_cell, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_save_cells: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start_cell, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_save_cells: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (start_cell > cells_number)
	{
		// error
		printf ("cells_fann_save_cells: ERROR: start cell to high!\n");
		return (NULL);
	}

	if (end_cell > cells_number)
	{
		// error
		printf ("cells_fann_save_cells: ERROR: end cell to high!\n");
		return (NULL);
	}

#if SANDBOX
	if (get_sandbox_filename (&data[filename_addr], sandbox_filename, 255) != 0)
	{
		printf ("cells_fann_save_cells: ERROR filename illegal: %s\n", &data[filename_addr]);
		return (NULL);
	}
	ret = Cells_fann_save_cells (cells, sandbox_filename, start_cell, end_cell);
#else
	ret = Cells_fann_save_cells (cells, &data[filename_addr], start_cell, end_cell);
#endif

	// push return value to stack
	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_save_cells: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}

U1 *cells_fann_load_cells (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 filename_addr ALIGN;
	S2 ret;

	U1 sandbox_filename[256];

	sp = stpopi ((U1 *) &filename_addr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_load_cells: ERROR: stack corrupt!\n");
		return (NULL);
	}

#if SANDBOX
	if (get_sandbox_filename (&data[filename_addr], sandbox_filename, 255) != 0)
	{
		printf ("cells_fann_load_cells: ERROR filename illegal: %s\n", &data[filename_addr]);
		return (NULL);
	}

	cells = Cells_fann_load_cells (sandbox_filename);
#else
	cells = Cells_fann_load_cells (&data[filename_addr]);
#endif

	if (cells == NULL)
	{
		// error
		ret = 1;
	}
	else
	{
		ret = 0;
	}

	// push return value to stack
	sp = stpushi (ret, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("cells_fann_load_cells: ERROR: stack corrupt!\n");
		return (NULL);
	}

	return (sp);
}
