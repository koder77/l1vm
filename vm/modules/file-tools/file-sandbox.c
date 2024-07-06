/*
 * This file file-sandbox.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2020
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

#include "../../../include/global.h"
#include "../../../include/home.h"

size_t strlen_safe (const char * str, S8  maxlen);

// for SANDBOX file access
U1 check_file_access (U1 *path)
{
	/* check for forbidden ../ in pathname */

	S2 slen, i;

	slen = strlen_safe ((const char *) path, 255);

	for (i = 0; i < slen - 1; i++)
	{
		if (path[i] == '.')
		{
			if (path[i + 1] == '.')
			{
				return (1);	// forbidden access
			}
		}
	}

	return (0);  /* path legal, all ok! */
}

U1 get_sandbox_filename (U1 *filename, U1 *sandbox_filename, S2 max_name_len)
{
	S2 filename_len;
	S2 filename_sandbox_len;
	S2 filename_home_len;
	char *home;

	home = get_home ();
	filename_home_len = strlen_safe (home, max_name_len);
	filename_sandbox_len = strlen_safe (SANDBOX_ROOT, max_name_len);
	if (filename_home_len + filename_sandbox_len > max_name_len)
	{
		printf ("ERROR: get_sandbox_filename: file root: '%s' too long!\n", SANDBOX_ROOT);
		return (1);
	}

	filename_len = strlen_safe ((const char *) filename, max_name_len);
	if (filename_len > max_name_len)
	{
		printf ("ERROR: get_sandbox_filename: file name: '%s' too long!\n", SANDBOX_ROOT);
		return (1);
	}

	filename_sandbox_len += filename_home_len;
	filename_sandbox_len += filename_len;
	if (filename_sandbox_len > max_name_len)
	{
		printf ("ERROR: get_sandbox_filename: file name: '%s' too long!\n", (char *) filename);
		return (1);
	}

	strcpy ((char *) sandbox_filename, home);
	strcat ((char *) sandbox_filename, SANDBOX_ROOT);
	strcat ((char *) sandbox_filename, (char *) filename);

	if (check_file_access (sandbox_filename) != 0)
	{
		// illegal parts in filename path, ERROR!!!
		printf ("ERROR: get_sandbox_filename: file name: '%s' illegal!\n", (char *) sandbox_filename);
		return (1);
	}

	// all OK, return 0
	return (0);
}
