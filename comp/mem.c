/*
 * This file mem.c is part of L1vm.
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

#include "../include/global.h"

void dealloc_array_U1 (U1** array, size_t n_rows)
{
    size_t c;

    assert (array || n_rows == 0);

    for (c = 0; c != n_rows; ++c)
    {
        free (array[c]);
    }
    free (array);
}

U1** alloc_array_U1 (size_t n_rows, size_t n_columns)
{
    size_t c;
    U1 **array;

    assert (n_columns > 0);

    if (n_rows == 0) { return 0; }

    array = (U1**) (calloc (n_rows, sizeof (U1*)));
    if (!array)
    {
        errno = ENOMEM;
        return 0;
    }

    for (c = 0; c != n_rows; ++c)
    {
        array[c] = (U1*) (calloc (n_columns, sizeof (U1)));
        if (!array[c])
        {
            free (array[c]);
            errno = ENOMEM;
            return 0;
        }
    }

    return array;
}
