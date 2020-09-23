/*
 * This file if.h is part of L1vm.
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

// definitions for if and for loops

#define MAXJUMPLIST 1024
#define MAXJUMPNAME 64
#define MAXIF 1024
#define MAXWHILE 1024
#define MAXFOR 1024
#define MAXSWITCH 1024
#define EMPTY -1
#define NOTDEF -1
#define IF_FINISHED 2
#define SWITCH_FINISHED 3


extern struct opcode opcode[MAXOPCODES];


struct if_comp
{
    U1 used;
    S8 else_pos ALIGN;
    S8 endif_pos ALIGN;
    U1 else_name[MAXJUMPNAME + 1];
    U1 endif_name[MAXJUMPNAME + 1];
};

struct while_comp
{
    U1 used;
    S8 while_pos ALIGN;
    U1 while_set;
    U1 while_name[MAXJUMPNAME + 1];
};

struct for_comp
{
    U1 used;
    S8 for_pos ALIGN;
    U1 for_set;
    U1 for_name[MAXJUMPNAME + 1];
};

struct switch_comp
{
    U1 used;
	U1 switch_set;
};

struct jumplist
{
    U1 islabel;
    U1 lab[MAXJUMPNAME + 1];
    S8 pos ALIGN;                                 /* position in exelist */
    S8 source_pos ALIGN;                          /* position in sourcecode */
};
