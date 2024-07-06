/*
 * This file if.c is part of L1vm.
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
#include "if.h"

struct jumplist jumplist[MAXJUMPLIST];
struct if_comp if_comp[MAXIF];
struct while_comp while_comp[MAXWHILE];
struct for_comp for_comp[MAXFOR];
struct switch_comp switch_comp[MAXSWITCH];

S8 if_ind ALIGN;
S8 while_ind ALIGN;
S8 for_ind ALIGN;
S8 switch_ind ALIGN;
S8 jumplist_ind ALIGN;

// string functions ===============================================================================
size_t strlen_safe (const char * str, S8  maxlen);
S2 searchstr (U1 *str, U1 *srchstr, S2 start, S2 end, U1 case_sens);
void convtabs (U1 *str);

void init_if (void)
{
    S4 i;

    for (i = 0; i < MAXIF; i++)
    {
        if_comp[i].used = FALSE;
        if_comp[i].else_pos = EMPTY_IF;
        if_comp[i].endif_pos = EMPTY_IF;
		if_comp[i].ifplus = FALSE;
		if_comp[i].else_set = FALSE;
    }

	if_ind = -1;
	jumplist_ind = -1;
}

void init_switch (void)
{
	S4 i;

    for (i = 0; i < MAXSWITCH; i++)
    {
        switch_comp[i].used = FALSE;
		switch_comp[i].switch_set = FALSE;
	}
	switch_ind = -1;
}

S4 get_if_pos (void)
{
    S4 i;

    for (i = 0; i < MAXIF; i++)
    {
        if (if_comp[i].used == FALSE)
        {
            if_comp[i].used = TRUE;
			if_comp[i].ifplus = FALSE;
            return (i);
        }
    }

    /* no EMPTY_IF if found => error! */

    return (-1);
}

S4 get_ifplus_pos (void)
{
    S4 i;

    for (i = 0; i < MAXIF; i++)
    {
        if (if_comp[i].used == FALSE)
        {
            if_comp[i].used = TRUE;
			if_comp[i].ifplus = TRUE;
            return (i);
        }
    }

    /* no EMPTY_IF if found => error! */

    return (-1);
}

S4 get_act_if (void)
{
    S4 i;

    for (i = MAXIF - 1; i >= 0; i--)
    {
        if (if_comp[i].used == TRUE)
        {
            return (i);
        }
    }
    return (-1);
}

S4 get_act_ifplus (void)
{
    S4 i;

    for (i = MAXIF - 1; i >= 0; i--)
    {
        if (if_comp[i].used == TRUE)
        {
			if (if_comp[i].ifplus == TRUE)
			{
				// found if+ which was set earlier
            	return (i);
			}
        }
    }
    return (-1);
}

S4 check_ifplus (S8 ind)
{
	if (if_comp[ind].ifplus == TRUE)
	{
		return (0);
	}
	else
	{
		return (1);
	}
}

void set_else (S8 ind)
{
	if_comp[ind].else_set = TRUE;
}

S4 check_else (S8 ind)
{
	return (if_comp[ind].else_set);
}

U1 get_if_label (S8 ind, U1 *label)
{
    U1 labelnum[10];

    strcpy ((char *) label, ":if_");
    sprintf ((char *) labelnum, "%lld", ind);
    strcat ((char *) label, (const char *) labelnum);
    return (TRUE);
}

U1 get_endif_label (S8 ind, U1 *label)
{
    U1 labelnum[10];

    strcpy ((char *) label, ":endif_");
    sprintf ((char *) labelnum, "%lld", ind);
    strcat ((char *) label, (const char *) labelnum);

	#if DEBUG
		printf ("set_endif_jmp: label: %s\n", label);
	#endif

    return (TRUE);
}

U1 get_else_label (S8 ind, U1 *label)
{
    U1 labelnum[10];

    strcpy ((char *) label, ":else_");
    sprintf ((char *)labelnum, "%lld", ind);
    strcat ((char *) label, (const char *) labelnum);

	#if DEBUG
    	printf ("get_else_label: '%s'\n", label);
	#endif

    return (TRUE);
}

void set_endif_finished (S8 ind)
{
    if_comp[ind].used = IF_FINISHED;
}

S4 get_else_lab (S8 ind)
{
    return (if_comp[ind].else_pos);
}

S4 get_endif_lab (S8 ind)
{
    return (if_comp[ind].endif_pos);
}

S4 get_else_set (S8 ind)
{
    return (jumplist[if_comp[ind].else_pos].pos);
}

// if optimize helper function ==========================================================

S4 get_if_optimize_reg (U1 *code_line)
{
	S4 pos, i, j = 0, str_len;
	U1 if_found = 0;
	U1 reg_num[256];
	S4 reg;
	U1 comma = ',';
	U1 comma_count = 0;
	U1 ch;

	// printf ("DEBUG: if.c: optimize if code_line: '%s'\n", code_line);

	// eqi to lseqd compare opcodes
	str_len = strlen_safe ((const char *) code_line, MAXLINELEN);
	for (i = EQI; i <= LSEQD; i++)
	{
		pos = searchstr (code_line, opcode[i].op, 0, 0, TRUE);
		if (pos != -1)
		{
			if_found = 1;
			break;
		}
	}

	// andi and ori
	for (i = ANDI; i <= ORI; i++)
	{
		pos = searchstr (code_line, opcode[i].op, 0, 0, TRUE);
		if (pos != -1)
		{
			if_found = 1;
			break;
		}
	}

	if (if_found == 0)
	{
		// no if found, return -1
		return (-1);
	}

	// get last argument, it's the needed register number
	for (i = 0; i < str_len; i++)
	{
		if (code_line[i] == comma)
		{
			comma_count++;
		}
		if (comma_count == 2)
		{
			pos = i + 2;
			break;
		}
	}
	// get register after the second comma
	for (i = pos; i < str_len; i++)
	{
		ch = code_line[i];
		if (ch != ' ' && ch != 10 && ch != 13)
		{
			reg_num[j] = ch;
			j++;
		}
	}
	reg_num[j] = '\0';		// set end of register number string

	reg = atoi ((const char *) reg_num);

	// printf ("DEBUG: if.c: optimize if last_arg: '%s'\n", reg_num);

	return (reg);
}

// while ====================================================
void init_while (void)
{
    S4 i;

    for (i = 0; i < MAXWHILE; i++)
    {
        while_comp[i].used = FALSE;
        while_comp[i].while_pos = EMPTY_IF;
        while_comp[i].while_set = FALSE;
    }
}

S4 get_while_pos (void)
{
    S4 i;

    for (i = 0; i < MAXWHILE; i++)
    {
        if (while_comp[i].used == FALSE)
        {
            while_comp[i].used = TRUE;
            return (i);
        }
    }

    /* no EMPTY_IF if found => error! */

    return (-1);
}

S4 get_act_while (void)
{
    S4 i;

    for (i = MAXWHILE - 1; i >= 0; i--)
    {
        if (while_comp[i].used == TRUE && while_comp[i].while_set == FALSE)
        {
            return (i);
        }
    }
	return (-1);
}

U1 get_while_label (S8 ind, U1 *label)
{
    U1 labelnum[10];

    strcpy ((char *) label, ":while_");
    sprintf ((char *) labelnum, "%lld", ind);
    strcat ((char *) label, (const char *) labelnum);

	#if DEBUG
		printf ("DEBUG: set_while_label: '%s'\n", label);
	#endif

    while_comp[ind].while_pos = jumplist_ind;
    return (TRUE);
}

S4 get_while_lab (S8 ind)
{
    return (while_comp[ind].while_pos);
}

void set_wend (S8 ind)
{
    while_comp[ind].while_set = TRUE;
}

// for ====================================================
void init_for (void)
{
    S4 i;

    for (i = 0; i < MAXFOR; i++)
    {
        for_comp[i].used = FALSE;
        for_comp[i].for_pos = EMPTY_IF;
        for_comp[i].for_set = FALSE;
		for_comp[i].for_def_set = FALSE;
    }
}

S4 get_for_pos (void)
{
    S4 i;

    for (i = 0; i < MAXFOR; i++)
    {
        if (for_comp[i].used == FALSE)
        {
            for_comp[i].used = TRUE;
            return (i);
        }
    }

    /* no EMPTY_IF if found => error! */

    return (-1);
}

S4 get_act_for (void)
{
    S4 i;

    for (i = MAXFOR - 1; i >= 0; i--)
    {
        if (for_comp[i].used == TRUE && for_comp[i].for_set == FALSE)
        {
            return (i);
        }
    }
	return (-1);
}

U1 get_for_label (S8 ind, U1 *label)
{
    U1 labelnum[10];

    strcpy ((char *) label, ":for_");
    sprintf ((char *) labelnum, "%lld", ind);
    strcat ((char *) label, (const char *) labelnum);

	#if DEBUG
		printf ("DEBUG: set_for_label: '%s'\n", label);
	#endif

    for_comp[ind].for_pos = jumplist_ind;
    return (TRUE);
}

U1 get_for_label_2 (S8 ind, U1 *label)
{
    U1 labelnum[10];

    strcpy ((char *) label, ":for2_");
    sprintf ((char *) labelnum, "%lld", ind);
    strcat ((char *) label, (const char *) labelnum);

	#if DEBUG
		printf ("DEBUG: set_for_label_2: '%s'\n", label);
	#endif

    // for_comp[ind].for_pos = jumplist_ind;
    return (TRUE);
}

U1 get_for_label_end (S8 ind, U1 *label)
{
    U1 labelnum[10];

    strcpy ((char *) label, ":forend_");
    sprintf ((char *) labelnum, "%lld", ind);
    strcat ((char *) label, (const char *) labelnum);

	#if DEBUG
		printf ("DEBUG: set_for_label_end: '%s'\n", label);
	#endif

    // for_comp[ind].for_pos = jumplist_ind;
    return (TRUE);
}

S4 get_for_lab (S8 ind)
{
    return (for_comp[ind].for_pos);
}

void set_for_end (S8 ind)
{
    for_comp[ind].for_set = TRUE;
}

void set_for (S8 ind)
{
	for_comp[ind].for_def_set = TRUE;
}

S4 check_for (S8 ind)
{
	return (for_comp[ind].for_def_set);
}

// switch =====================================================================
S4 get_switch_pos (void)
{
    S4 i;

    for (i = 0; i < MAXIF; i++)
    {
        if (switch_comp[i].used == FALSE)
        {
            switch_comp[i].used = TRUE;
            return (i);
        }
    }

    /* no EMPTY_IF if found => error! */

    return (-1);
}

S4 get_act_switch (void)
{
    S4 i;

    for (i = MAXSWITCH - 1; i >= 0; i--)
    {
        if (switch_comp[i].used == TRUE)
        {
            return (i);
        }
    }
    return (-1);
}

void set_switch_finished (S8 ind)
{
    switch_comp[ind].used = SWITCH_FINISHED;
}
