/*
 * This file translate.h is part of L1vm.
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

// l1vm - translate table

// #include "../include/global.h"

#define EMPTY 0
#define LABEL 5

#define MAXTRANSLATE 41

struct translate translate[] =
{
	{ "+", 2, { INTEGER, INTEGER, EMPTY, EMPTY }, ADDI},
	{ "-", 2, { INTEGER, INTEGER, EMPTY, EMPTY }, SUBI},
	{ "*", 2, { INTEGER, INTEGER, EMPTY, EMPTY }, MULI},
	{ "/", 2, { INTEGER, INTEGER, EMPTY, EMPTY }, DIVI},
	{ "+d", 2, { DOUBLE, DOUBLE, EMPTY, EMPTY }, ADDD},
	{ "-d", 2, { DOUBLE, DOUBLE, EMPTY, EMPTY }, SUBD},
	{ "*d", 2, { DOUBLE, DOUBLE, EMPTY, EMPTY }, MULD},
	{ "/d", 2, { DOUBLE, DOUBLE, EMPTY, EMPTY }, DIVD},
	{ "<<", 2, { INTEGER, INTEGER, EMPTY, EMPTY }, SMULI},
	{ ">>", 2, { INTEGER, INTEGER, EMPTY, EMPTY }, SDIVI},
	{ "&&", 2, { INTEGER, INTEGER, EMPTY, EMPTY }, ANDI},
	{ "||", 2, { INTEGER, INTEGER, EMPTY, EMPTY }, ORI},
	{ "&", 2, { INTEGER, INTEGER, EMPTY, EMPTY }, BANDI},
	{ "|", 2, { INTEGER, INTEGER, EMPTY, EMPTY }, BORI},
	{ "^", 2, { INTEGER, INTEGER, EMPTY, EMPTY }, BXORI},
	{ "%", 2, { INTEGER, INTEGER, EMPTY, EMPTY }, MODI},
	{ "==", 2, { INTEGER, INTEGER, EMPTY, EMPTY }, EQI},
	{ "!=", 2, { INTEGER, INTEGER, EMPTY, EMPTY }, NEQI},
	{ ">", 2, { INTEGER, INTEGER, EMPTY, EMPTY }, GRI},
	{ "<", 2, { INTEGER, INTEGER, EMPTY, EMPTY }, LSI},
	{ ">=", 2, { INTEGER, INTEGER, EMPTY, EMPTY }, GREQI},
	{ "<=", 2, { INTEGER, INTEGER, EMPTY, EMPTY }, LSEQI},
	{ "==d", 2, { DOUBLE, DOUBLE, EMPTY, EMPTY }, EQD},
	{ "!=d", 2, { DOUBLE, DOUBLE, EMPTY, EMPTY }, NEQD},
	{ ">d", 2, { DOUBLE, DOUBLE, EMPTY, EMPTY }, GRD},
	{ "<d", 2, { DOUBLE, DOUBLE, EMPTY, EMPTY }, LSD},
	{ ">=d", 2, { DOUBLE, DOUBLE, EMPTY, EMPTY }, GREQD},
	{ "<=d", 2, { DOUBLE, DOUBLE, EMPTY, EMPTY }, LSEQD},
	{ "jmp", 1, { LABEL, EMPTY, EMPTY, EMPTY }, JMP},
	{ "jsr", 1, { LABEL, EMPTY, EMPTY, EMPTY }, JSR},
	{ "jsra", 1, { INTEGER, EMPTY, EMPTY, EMPTY }, JSRA},
	{ "jmpi", 2, { INTEGER, LABEL, EMPTY, EMPTY }, JMPI},
	{ "stpushb", 1, { INTEGER, EMPTY, EMPTY, EMPTY }, STPUSHB},
	{ "stpopb", 1, { INTEGER, EMPTY, EMPTY, EMPTY }, STPOPB},
	{ "stpushi", 1, { INTEGER, EMPTY, EMPTY, EMPTY }, STPUSHI},
	{ "stpopi", 1, { INTEGER, EMPTY, EMPTY, EMPTY }, STPOPI},
	{ "stpushd", 1, { DOUBLE, EMPTY, EMPTY, EMPTY }, STPUSHD},
	{ "stpopd", 1, { DOUBLE, EMPTY, EMPTY, EMPTY }, STPOPD},
	{ "movi", 2, { INTEGER, INTEGER, EMPTY, EMPTY }, MOVI},
	{ "movd", 2, { DOUBLE, DOUBLE, EMPTY, EMPTY }, MOVD},
	{ "loadl", 2, { LABEL, INTEGER, EMPTY, EMPTY }, LOADL}
};
