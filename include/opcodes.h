/*
 * This file opcodes.h is part of L1vm.
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

#include "opcodes-types.h"

struct opcode opcode[] =
{
    { "pushb", 3, { I_REG, I_REG, I_REG, EMPTY } },
    { "pushw", 3, { I_REG, I_REG, I_REG, EMPTY } },
    { "pushdw", 3, { I_REG, I_REG, I_REG, EMPTY } },
    { "pushqw", 3, { I_REG, I_REG, I_REG, EMPTY } },
    { "pushd", 3, { I_REG, I_REG, D_REG, EMPTY } },

    { "pullb", 3, { I_REG, I_REG, I_REG, EMPTY } },
    { "pullw", 3, { I_REG, I_REG, I_REG, EMPTY } },
    { "pulldw", 3, { I_REG, I_REG, I_REG, EMPTY } },
    { "pullqw", 3, { I_REG, I_REG, I_REG, EMPTY } },
    { "pulld", 3, { D_REG, I_REG, I_REG, EMPTY} },	// 9

    { "addi", 3, { I_REG, I_REG, I_REG, EMPTY }, },
    { "subi", 3, { I_REG, I_REG, I_REG, EMPTY }, },
    { "muli", 3, { I_REG, I_REG, I_REG, EMPTY }, },
    { "divi", 3, { I_REG, I_REG, I_REG, EMPTY }, },

    { "addd", 3, { D_REG, D_REG, D_REG, EMPTY }, }, // 14
    { "subd", 3, { D_REG, D_REG, D_REG, EMPTY }, },
    { "muld", 3, { D_REG, D_REG, D_REG, EMPTY }, },
    { "divd", 3, { D_REG, D_REG, D_REG, EMPTY }, },

    { "smuli", 3, { I_REG, I_REG, I_REG, EMPTY }, }, // 18
    { "sdivi", 3, { I_REG, I_REG, I_REG, EMPTY }, },

    { "andi", 3, { I_REG, I_REG, I_REG, EMPTY }, },
    { "ori", 3, { I_REG, I_REG, I_REG, EMPTY }, },
    { "bandi", 3, { I_REG, I_REG, I_REG, EMPTY }, },
    { "bori", 3, { I_REG, I_REG, I_REG, EMPTY }, },
    { "bxori", 3, { I_REG, I_REG, I_REG, EMPTY }, },
    { "modi", 3, { I_REG, I_REG, I_REG, EMPTY }, }, // 25

    { "eqi", 3, { I_REG, I_REG, I_REG, EMPTY }, },
    { "neqi", 3, { I_REG, I_REG, I_REG, EMPTY }, },
    { "gri", 3, { I_REG, I_REG, I_REG, EMPTY }, },
    { "lsi", 3, { I_REG, I_REG, I_REG, EMPTY }, },
    { "greqi", 3, { I_REG, I_REG, I_REG, EMPTY }, },
    { "lseqi", 3, { I_REG, I_REG, I_REG, EMPTY }, }, // 31

    { "eqd", 3, { D_REG, D_REG, I_REG, EMPTY }, },
    { "neqd", 3, { D_REG, D_REG, I_REG, EMPTY }, },
    { "grd", 3, { D_REG, D_REG, I_REG, EMPTY }, },
    { "lsd", 3, { D_REG, D_REG, I_REG, EMPTY }, },
    { "greqd", 3, { D_REG, D_REG, I_REG, EMPTY }, },
    { "lseqd", 3, { D_REG, D_REG, I_REG, EMPTY }, },

    { "jmp", 1, { LABEL, EMPTY, EMPTY, EMPTY }, },  // 38
    { "jmpi", 2, { I_REG, LABEL, EMPTY, EMPTY }, },

    { "stpushb", 1, { I_REG, EMPTY, EMPTY, EMPTY }, },
    { "stpopb", 1, { I_REG, EMPTY, EMPTY, EMPTY }, },

    { "stpushi", 1, { I_REG, EMPTY, EMPTY, EMPTY }, },
    { "stpopi", 1, { I_REG, EMPTY, EMPTY, EMPTY }, },

    { "stpushd", 1, { D_REG, EMPTY, EMPTY, EMPTY }, },
    { "stpopd", 1, { D_REG, EMPTY, EMPTY, EMPTY }, },

    { "loada", 3, { DATA, DATA_OFFS, I_REG, EMPTY }, },
	{ "loadd", 3, { DATA, DATA_OFFS, D_REG, EMPTY }, },
    { "intr0", 4, { ALL, ALL, ALL, ALL }, },
    { "intr1", 4, { ALL, ALL, ALL, ALL }, },

    { "inclsijmpi", 3, { I_REG, I_REG, LABEL, EMPTY }, },
    { "decgrijmpi", 3, { I_REG, I_REG, LABEL, EMPTY }, },

    { "movi", 2, { I_REG, I_REG, EMPTY, EMPTY }, },
    { "movd", 2, { D_REG, D_REG, EMPTY, EMPTY }, },
    { "loadl", 2, { LABEL, I_REG, EMPTY, EMPTY }, },
    { "jmpa", 1, { I_REG, EMPTY, EMPTY, EMPTY }, },
    { "jsr", 1, { LABEL, EMPTY, EMPTY, EMPTY }, },
	{ "jsra", 1, { I_REG, EMPTY, EMPTY, EMPTY }, },
	{ "rts", 0, { EMPTY, EMPTY, EMPTY, EMPTY }, },

	{ "load", 3, { DATA, DATA_OFFS, I_REG, EMPTY }, },

    { "noti", 2, { I_REG, I_REG, EMPTY, EMPTY }, }
};
