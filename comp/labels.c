/*
* This file labels.c is part of L1vm.
*
* (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2020
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

#include "../include/global.h"

extern S8 linenum;
extern S8 label_ind;
extern S8 call_label_ind;

extern struct label label[MAXLABELS];
extern struct call_label call_label[MAXLABELS];


// initialize labels list =================================
void init_labels (void)
{
	S8 i ALIGN;
	
	for (i = 0; i < MAXLABELS; i++)
	{
		label[i].pos = -1;
		strcpy ((char *) label[i].name, "");
	}
}

void init_call_labels (void)
{
	S8 i ALIGN;
	
	for (i = 0; i < MAXLABELS; i++)
	{
		call_label[i].pos = -1;
		strcpy ((char *) call_label[i].name, "");
	}
}

// set call labels ========================================

S2 set_call_label (U1 *label)
{
	S8 i ALIGN;
	
	// check if already set:
	for (i = 0; i <= call_label_ind; i++)
	{
		if (strcmp ((const char *) call_label[i].name, (const char *) label) == 0)
		{
			// just return no error!
			return (0);
		}
	}
	
	// label not found in call label list, set it!!
	
	// set call label
	// add name to labels list
	if (call_label_ind < MAXLABELS - 1)
	{
		call_label_ind++;
	}
	else
	{
		// error list full!!
		return (1);
	}
	
	call_label[call_label_ind].pos = linenum;
	strcpy ((char *) call_label[call_label_ind].name, (const char *) label);
	return (0);
}

// check if all call labels are defined!!! ================

S2 check_labels (void)
{
	S8 i ALIGN;
	S8 j ALIGN;
	U1 found = 0;
	S2 ret = 0;
	
	for (i = 0; i <= call_label_ind; i++)
	{
		found = 0;
		for (j = 0; j <= label_ind; j++)
		{
			if (strcmp ((const char *) call_label[i].name, (const char *) label[j].name) == 0)
			{
				// set found, all ok!!
				found = 1;
				break;
			}
		}
		if (found == 0)
		{
			// set ERROR value
			ret = 1;
			printf ("ERROR: line: %lli, label: '%s' not defined!\n", call_label[i].pos, call_label[i].name);
		}
	}
	return (ret);
}

S2 search_label (U1 *search_label)
{
	// returns 1 if label already defined
	S8 i ALIGN;
	U1 search_str[MAXSTRLEN];

	if (search_label[0] != ':')
	{
		strcpy ((char *) search_str, ":");
		strcat ((char *) search_str, (char *) search_label);
	}
	else
	{
		strcpy ((char *) search_str, (char *) search_label);
	}

	// check if already set:
	for (i = 0; i <= label_ind; i++)
	{
		if (strcmp ((const char *) label[i].name, (const char *) search_str) == 0)
		{
			return (1);
		}
	}
	return (0);
}
