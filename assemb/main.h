/*
 * This file main.h is part of L1vm.
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

 #ifndef _ASSEMB_MAIN_H_
 #define _ASSEMB_MAIN_H_

// forward declarations
U1 checkdigit (U1 *str);
S8 get_temp_int (void);
F8 get_temp_double (void);
char *fgets_uni (char *str, int len, FILE *fptr);
size_t strlen_safe (const char * str, S8  maxlen);

void convtabs (U1 *str);
S2 strip_end_commas (U1 *str);
S2 searchstr (U1 *str, U1 *srchstr, S2 start, S2 end, U1 case_sens);

// code_datasize.c
void show_code_data_size (S8 codesize, S8 datasize);
void show_filesize (S8 filesize);

// file module
U1 get_sandbox_filename (U1 *filename, U1 *sandbox_filename, S2 max_name_len);

#endif
