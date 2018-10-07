#include "../include/global.h"
#include "if.h"

struct jumplist jumplist[MAXJUMPLIST];
struct if_comp if_comp[MAXIF];

S4 if_ind;
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
