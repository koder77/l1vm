/*
 * This file mem.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (info@midnight-coding.de), 2021
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

#include "../../include/global.h"
#include "l1vm-embedded.h"

// functions read from data
S2 read_data_byte (S8 addr, U1 *ret)
{
    U1 *data_ptr = l1vm_get_global_data ();

    if (memory_bounds (addr, 0) == 1)
    {
        return (1); // error bounds check!
    }

    *ret =  data_ptr[addr];

    return (0); // all ok!
}

S2 read_data_int16 (S8 addr, S2 *ret)
{
    U1 *data_ptr = l1vm_get_global_data ();
    S8 i;
    U1 *byte_ptr = (U1 *) ret;

    if (memory_bounds (addr, 1) == 1)
    {
        return (1); // error bounds check!
    }

    for (i = 0; i < 2; i++)
    {
        byte_ptr[addr + i] = data_ptr[addr + i];
    }

    return (0); // all ok!
}

S2 read_data_int32 (S8 addr, S4 *ret)
{
    U1 *data_ptr = l1vm_get_global_data ();
    S8 i;
    U1 *byte_ptr = (U1 *) ret;

    if (memory_bounds (addr, 3) == 1)
    {
        return (1); // error bounds check!
    }

    for (i = 0; i < 2; i++)
    {
        byte_ptr[addr + i] = data_ptr[addr + i];
    }

    return (0); // all ok!
}

S8 read_data_int64 (S8 addr, S4 *ret)
{
    U1 *data_ptr = l1vm_get_global_data ();
    S8 i;
    U1 *byte_ptr = (U1 *) ret;

    if (memory_bounds (addr, 7) == 1)
    {
        return (1); // error bounds check!
    }

    for (i = 0; i < 2; i++)
    {
        byte_ptr[addr + i] = data_ptr[addr + i];
    }

    return (0); // all ok!
}

S2 read_data_double (S8 addr, F8 *ret)
{
    U1 *data_ptr = l1vm_get_global_data ();
    S8 i;
    U1 *byte_ptr = (U1 *) ret;

    if (memory_bounds (addr, 7) == 1)
    {
        return (1); // error bounds check!
    }

    for (i = 0; i < 2; i++)
    {
        byte_ptr[addr + i] = data_ptr[addr + i];
    }

    return (0); // all ok!
}

// functions write to data
S2 write_data_byte (S8 addr, U1 data)
{
    U1 *data_ptr = l1vm_get_global_data ();

    if (memory_bounds (addr, 0) == 1)
    {
        return (1); // error bounds check!
    }

    data_ptr[addr] = data;

    return (0); // all ok!
}

S2 write_data_int16 (S8 addr, S2 *data)
{
    U1 *data_ptr = l1vm_get_global_data ();
    S8 i;
    U1 *byte_ptr = (U1 *) data;

    if (memory_bounds (addr, 2) == 1)
    {
        return (1); // error bounds check!
    }

    for (i = 0; i < 2; i++)
    {
        data_ptr[addr + i] = byte_ptr[addr + i];
    }

    return (0); // all ok!
}

S2 write_data_int32 (S8 addr, S4 *data)
{
    U1 *data_ptr = l1vm_get_global_data ();
    S8 i;
    U1 *byte_ptr = (U1 *) data;

    if (memory_bounds (addr, 3) == 1)
    {
        return (1); // error bounds check!
    }

    for (i = 0; i < 2; i++)
    {
        data_ptr[addr + i] = byte_ptr[addr + i];
    }

    return (0); // all ok!
}

S2 write_data_int64 (S8 addr, S8 *data)
{
    U1 *data_ptr = l1vm_get_global_data ();
    S8 i;
    U1 *byte_ptr = (U1 *) data;

    if (memory_bounds (addr, 7) == 1)
    {
        return (1); // error bounds check!
    }

    for (i = 0; i < 2; i++)
    {
        data_ptr[addr + i] = byte_ptr[addr + i];
    }

    return (0); // all ok!
}

S2 write_data_double (S8 addr, F8 *data)
{
    U1 *data_ptr = l1vm_get_global_data ();
    S8 i;
    U1 *byte_ptr = (U1 *) data;

    if (memory_bounds (addr, 7) == 1)
    {
        return (1); // error bounds check!
    }

    for (i = 0; i < 2; i++)
    {
        data_ptr[addr + i] = byte_ptr[addr + i];
    }

    return (0); // all ok!
}
