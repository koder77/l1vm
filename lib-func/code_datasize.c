/*
 * This file code_datasize.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (info@midnight-coding.de), 2017
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

 void show_code_data_size (S8 codesize, S8 datasize)
 {
 	S8 factor_KB ALIGN = 1024;
 	S8 factor_MB ALIGN = 1024 * 1024;
 	S8 factor_GB ALIGN = 1024 * 1024 * 1024;
 	S8 factor_TB ALIGN = (S8) 1024 * 1024 * 1024 * 1024;

 	U1 codesize_found = 0;
 	U1 datasize_found = 0;

 	F8 codesize_float ALIGN;
 	F8 datasize_float ALIGN;

 	// codesize
 	printf ("codesize: %lli ", codesize);

 	if (codesize > factor_TB && codesize_found == 0)
 	{
 		codesize_float = (F8) codesize / factor_TB;
 		printf (", %.3lf TB\n", codesize_float);
 		codesize_found = 1;
 	}

 	if (codesize > factor_GB && codesize_found == 0)
 	{
 		codesize_float = (F8) codesize / factor_GB;
 		printf (", %.3lf GB\n", codesize_float);
 		codesize_found = 1;
 	}

 	if (codesize > factor_MB && codesize_found == 0)
 	{
 		codesize_float = (F8) codesize / factor_MB;
 		printf (", %.3lf MB\n", codesize_float);
 		codesize_found = 1;
 	}

 	if (codesize > factor_KB && codesize_found == 0)
 	{
 		codesize_float = (F8) codesize / factor_KB;
 		printf (", %.3lf KB\n", codesize_float);
 		codesize_found = 1;
 	}

 	if (codesize_found == 0)
 	{
 		printf ("bytes\n");
 	}

 	// datasize
 	printf ("datasize: %lli ", datasize);

 	if (datasize > factor_TB && datasize_found == 0)
 	{
 		datasize_float = (F8) datasize / factor_TB;
 		printf (", %.3lf TB\n", datasize_float);
 		datasize_found = 1;
 	}

 	if (datasize > factor_GB && datasize_found == 0)
 	{
 		datasize_float = (F8) datasize / factor_GB;
 		printf (", %.3lf GB\n", datasize_float);
 		datasize_found = 1;
 	}

 	if (datasize > factor_MB && datasize_found == 0)
 	{
 		datasize_float = (F8) datasize / factor_MB;
 		printf (", %.3lf MB\n", datasize_float);
 		datasize_found = 1;
 	}

 	if (datasize > factor_KB && datasize_found == 0)
 	{
 		datasize_float = (F8) datasize / factor_KB;
 		printf (", %.3lf KB\n", datasize_float);
 		datasize_found = 1;
 	}

 	if (datasize_found == 0)
 	{
 		printf ("bytes\n");
 	}
 }

 void show_filesize (S8 filesize)
 {
     S8 factor_KB ALIGN = 1024;
     S8 factor_MB ALIGN = 1024 * 1024;
     S8 factor_GB ALIGN = 1024 * 1024 * 1024;
     S8 factor_TB ALIGN = (S8) 1024 * 1024 * 1024 * 1024;

     U1 filesize_found = 0;

     F8 filesize_float ALIGN;

     // filesize
     printf ("filesize: %lli ", filesize);

     if (filesize > factor_TB && filesize_found == 0)
     {
         filesize_float = (F8) filesize / factor_TB;
         printf (", %.3lf TB\n", filesize_float);
         filesize_found = 1;
     }

     if (filesize > factor_GB && filesize_found == 0)
     {
         filesize_float = (F8) filesize / factor_GB;
         printf (", %.3lf GB\n", filesize_float);
         filesize_found = 1;
     }

     if (filesize > factor_MB && filesize_found == 0)
     {
         filesize_float = (F8) filesize / factor_MB;
         printf (", %.3lf MB\n", filesize_float);
         filesize_found = 1;
     }

     if (filesize > factor_KB && filesize_found == 0)
     {
         filesize_float = (F8) filesize / factor_KB;
         printf (", %.3lf KB\n", filesize_float);
         filesize_found = 1;
     }

     if (filesize_found == 0)
     {
         printf ("bytes\n");
     }
 }
