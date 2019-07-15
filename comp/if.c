/*
 * This file if.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2018
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

S4 if_ind;
S4 while_ind;
S4 for_ind;
S4 jumplist_ind;

void init_if (void)
{
    S4 i;

    for (i = 0; i < MAXIF; i++)
    {
        if_comp[i].used = FALSE;
        if_comp[i].else_pos = EMPTY;
        if_comp[i].endif_pos = EMPTY;
    }

	if_ind = -1;
	jumplist_ind = -1;
}

S4 get_if_pos (void)
{
    S4 i;

    for (i = 0; i < MAXIF; i++)
    {
        if (if_comp[i].used == FALSE)
        {
            if_comp[i].used = TRUE;
            return (i);
        }
    }

    /* no empty if found => error! */

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

U1 get_if_label (S4 ind, U1 *label)
{
    U1 labelnum[10];

    strcpy ((char *) label, ":if_");
    sprintf ((char *) labelnum, "%d", ind);
    strcat ((char *) label, (const char *) labelnum);
    return (TRUE);
}

U1 get_endif_label (S4 ind, U1 *label)
{
    U1 labelnum[10];

    strcpy ((char *) label, ":endif_");
    sprintf ((char *) labelnum, "%d", ind);
    strcat ((char *) label, (const char *) labelnum);

	#if DEBUG
		printf ("set_endif_jmp: label: %s\n", label);
	#endif

    return (TRUE);
}

U1 get_else_label (S4 ind, U1 *label)
{
    U1 labelnum[10];

    strcpy ((char *) label, ":else_");
    sprintf ((char *)labelnum, "%d", ind);
    strcat ((char *) label, (const char *) labelnum);

	#if DEBUG
    	printf ("get_else_label: '%s'\n", label);
	#endif

    return (TRUE);
}

void set_endif_finished (S4 ind)
{
    if_comp[ind].used = IF_FINISHED;
}

S4 get_else_lab (S4 ind)
{
    return (if_comp[ind].else_pos);
}

S4 get_endif_lab (S4 ind)
{
    return (if_comp[ind].endif_pos);
}

S4 get_else_set (S4 ind)
{
    return (jumplist[if_comp[ind].else_pos].pos);
}


// while ====================================================
void init_while (void)
{
    S4 i;

    for (i = 0; i < MAXWHILE; i++)
    {
        while_comp[i].used = FALSE;
        while_comp[i].while_pos = EMPTY;
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

    /* no empty if found => error! */

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

U1 get_while_label (S4 ind, U1 *label)
{
    U1 labelnum[10];

    strcpy ((char *) label, ":while_");
    sprintf ((char *) labelnum, "%d", ind);
    strcat ((char *) label, (const char *) labelnum);

	#if DEBUG
		printf ("DEBUG: set_while_label: '%s'\n", label);
	#endif

    while_comp[ind].while_pos = jumplist_ind;
    return (TRUE);
}

S4 get_while_lab (S4 ind)
{
    return (while_comp[ind].while_pos);
}

void set_wend (S4 ind)
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
        for_comp[i].for_pos = EMPTY;
        for_comp[i].for_set = FALSE;
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

    /* no empty if found => error! */

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

U1 get_for_label (S4 ind, U1 *label)
{
    U1 labelnum[10];

    strcpy ((char *) label, ":for_");
    sprintf ((char *) labelnum, "%d", ind);
    strcat ((char *) label, (const char *) labelnum);

	#if DEBUG
		printf ("DEBUG: set_for_label: '%s'\n", label);
	#endif

    for_comp[ind].for_pos = jumplist_ind;
    return (TRUE);
}

U1 get_for_label_2 (S4 ind, U1 *label)
{
    U1 labelnum[10];

    strcpy ((char *) label, ":for2_");
    sprintf ((char *) labelnum, "%d", ind);
    strcat ((char *) label, (const char *) labelnum);

	#if DEBUG
		printf ("DEBUG: set_for_label_2: '%s'\n", label);
	#endif

    // for_comp[ind].for_pos = jumplist_ind;
    return (TRUE);
}

U1 get_for_label_end (S4 ind, U1 *label)
{
    U1 labelnum[10];

    strcpy ((char *) label, ":forend_");
    sprintf ((char *) labelnum, "%d", ind);
    strcat ((char *) label, (const char *) labelnum);

	#if DEBUG
		printf ("DEBUG: set_for_label_end: '%s'\n", label);
	#endif

    // for_comp[ind].for_pos = jumplist_ind;
    return (TRUE);
}

S4 get_for_lab (S4 ind)
{
    return (for_comp[ind].for_pos);
}

void set_for_end (S4 ind)
{
    for_comp[ind].for_set = TRUE;
}
