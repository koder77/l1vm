/*
 * This file stack.h is part of L1vm.
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


// stack operations ---------------------------------------
// protos

U1 *stpushb (U1 data, U1 *sp, U1 *sp_bottom);
U1 *stpopb (U1 *data, U1 *sp, U1 *sp_top);

U1 *stpushi (S8 data, U1 *sp, U1 *sp_bottom);
U1 *stpopi (U1 *data, U1 *sp, U1 *sp_top);

U1 *stpushd (F8 data, U1 *sp, U1 *sp_bottom);
U1 *stpopd (U1 *data, U1 *sp, U1 *sp_top);
