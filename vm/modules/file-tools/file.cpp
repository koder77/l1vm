/*
 * This file file.cpp is part of L1vm.
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

// file_copy, directory_create, remove_all, file_rename, file_size_bytes, file_exists


#include "../../../include/global.h"
#include "../../../include/stack.h"

#include <string>
#include <fstream>
#include <iostream>
#include <string>
#include <experimental/filesystem>

using namespace std::experimental::filesystem::v1;

// protos
extern "C" S2 memory_bounds (S8 start, S8 offset_access);


extern "C" U1 get_sandbox_filename (U1 *filename, U1 *sandbox_filename, S2 max_name_len);
extern "C" size_t strlen_safe (const char * str, int maxlen);

extern "C" U1 *file_copy (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 sourceaddr ALIGN;
    S8 destaddr ALIGN;
    bool copy = false;
    S8 ret ALIGN = 0;
	S8 file_name_len ALIGN;

    U1 source_fullnamestr[256];
    U1 dest_fullnamestr[256];

    std::string sourcestr;
    std::string deststr;

    sp = stpopi ((U1 *) &destaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_copy: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &sourceaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_copy: ERROR: stack corrupt!\n");
		return (NULL);
	}

	file_name_len = strlen_safe ((char *) &data[sourceaddr], 255);
	if (file_name_len > 255)
	{
		printf ("ERROR: file_copy: file name source : '%s' too long!\n", (char *) &data[sourceaddr]);
		return (NULL);
	}

	file_name_len = strlen_safe ((char *) &data[destaddr], 255);
	if (file_name_len > 255)
	{
		printf ("ERROR: file_copy: file name dest: '%s' too long!\n", (char *) &data[destaddr]);
		return (NULL);
	}

    // get source full name
    #if SANDBOX
	if (get_sandbox_filename (&data[sourceaddr],  source_fullnamestr, 255) != 0)
	{
		printf ("ERROR: file_copy: illegal filename: %s\n", &data[sourceaddr]);
		return (NULL);
	}
	#else
	if (strlen_safe (&data[sourceaddr], 255) < 255)
	{
    	strcpy ((char *) source_fullnamestr, (char *) &data[sourceaddr]);
	}
	else
	{
		printf ("ERROR: file_copy: filename too long!\n", &data[sourceaddr]);
		return (NULL);
	}
    #endif

    // get dest full name
    #if SANDBOX
	if (get_sandbox_filename (&data[destaddr],  dest_fullnamestr, 255) != 0)
	{
		printf ("ERROR: file_copy: illegal filename: %s\n", &data[destaddr]);
		return (NULL);
	}
	#else
	if (strlen_safe (&data[destaddr], 255) < 255)
	{
    	strcpy ((char *) dest_fullnamestr, (char *) &data[destaddr]);
	}
	else
	{
		printf ("ERROR: file_copy: filename too long!\n", &data[destaddr]);
		return (NULL);
	}
    #endif


    sourcestr.assign ((const char *) source_fullnamestr);
    deststr.assign ((const char *) dest_fullnamestr);

    copy = copy_file (sourcestr, deststr);
    if (copy == true)
    {
        ret = 0; // OK
    }
    else
    {
        ret = 1; // error
    }

    // push return value
    sp = stpushi (ret, sp, sp_bottom);
    if (sp == NULL)
    {
    	// error
    	printf ("file_copy: ERROR: stack corrupt!\n");
    	return (NULL);
    }
    return (sp);
}

extern "C" U1 *directory_create (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 diraddr ALIGN;
    bool mkdir = false;
    S8 ret ALIGN = 0;
	S8 dir_name_len ALIGN;

    U1 dir_namestr[256];

    std::string dirstr;

    sp = stpopi ((U1 *) &diraddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: directory_create: ERROR: stack corrupt!\n");
		return (NULL);
	}

    dir_name_len = strlen_safe ((char *) &data[diraddr], 255);
	if (dir_name_len > 255)
	{
		printf ("ERROR: directory_create: '%s' too long!\n", (char *) &data[diraddr]);
		return (NULL);
	}

   // get dir full name
    #if SANDBOX
	if (get_sandbox_filename (&data[diraddr], dir_namestr, 255) != 0)
	{
		printf ("ERROR: directory_create: illegal dirname: %s\n", &data[diraddr]);
		return (NULL);
	}
	#else
	if (strlen_safe (&data[diraddr], 255) < 255)
	{
    	strcpy ((char *) dir_namestr, (char *) &data[sourceaddr]);
	}
	else
	{
		printf ("ERROR: directory_create: dirname too long!\n", &data[diraddr]);
		return (NULL);
	}
    #endif

    dirstr.assign ((const char *) dir_namestr);

    mkdir = create_directory (dirstr);
    if (mkdir == true)
    {
        ret = 0; // OK
    }
    else
    {
        ret = 1; // error
    }

    // push return value
    sp = stpushi (ret, sp, sp_bottom);
    if (sp == NULL)
    {
    	// error
    	printf ("directory_create: ERROR: stack corrupt!\n");
    	return (NULL);
    }
    return (sp);
}

extern "C" U1 *remove_all (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 diraddr ALIGN;
    bool mkdir = false;
    S8 ret ALIGN = 0;
	S8 dir_name_len ALIGN;

    U1 dir_namestr[256];

    std::string dirstr;

    sp = stpopi ((U1 *) &diraddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: remov_all: ERROR: stack corrupt!\n");
		return (NULL);
	}

    dir_name_len = strlen_safe ((char *) &data[diraddr], 255);
	if (dir_name_len > 255)
	{
		printf ("ERROR: remove_all %s' too long!\n", (char *) &data[diraddr]);
		return (NULL);
	}

   // get dir full name
    #if SANDBOX
	if (get_sandbox_filename (&data[diraddr], dir_namestr, 255) != 0)
	{
		printf ("ERROR: remove_all: illegal name: %s\n", &data[diraddr]);
		return (NULL);
	}
	#else
	if (strlen_safe (&data[diraddr], 255) < 255)
	{
    	strcpy ((char *) dir_namestr, (char *) &data[sourceaddr]);
	}
	else
	{
		printf ("ERROR: remove_all: name too long!\n", &data[diraddr]);
		return (NULL);
	}
    #endif

    dirstr.assign ((const char *) dir_namestr);

    mkdir = remove_all (dirstr);
    if (mkdir == true)
    {
        ret = 0; // OK
    }
    else
    {
        ret = 1; // error
    }

    // push return value
    sp = stpushi (ret, sp, sp_bottom);
    if (sp == NULL)
    {
    	// error
    	printf ("remove_all: ERROR: stack corrupt!\n");
    	return (NULL);
    }
    return (sp);
}

extern "C" U1 *file_rename (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 sourceaddr ALIGN;
    S8 destaddr ALIGN;
    S8 ret ALIGN = 0;
	S8 file_name_len ALIGN;

    U1 source_fullnamestr[256];
    U1 dest_fullnamestr[256];

    std::string sourcestr;
    std::string deststr;

    sp = stpopi ((U1 *) &destaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_rename: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &sourceaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_rename: ERROR: stack corrupt!\n");
		return (NULL);
	}

	file_name_len = strlen_safe ((char *) &data[sourceaddr], 255);
	if (file_name_len > 255)
	{
		printf ("ERROR: file_rename: file name source : '%s' too long!\n", (char *) &data[sourceaddr]);
		return (NULL);
	}

	file_name_len = strlen_safe ((char *) &data[destaddr], 255);
	if (file_name_len > 255)
	{
		printf ("ERROR: file_rename: file name dest: '%s' too long!\n", (char *) &data[destaddr]);
		return (NULL);
	}

    // get source full name
    #if SANDBOX
	if (get_sandbox_filename (&data[sourceaddr],  source_fullnamestr, 255) != 0)
	{
		printf ("ERROR: file_rename: illegal filename: %s\n", &data[sourceaddr]);
		return (NULL);
	}
	#else
	if (strlen_safe (&data[sourceaddr], 255) < 255)
	{
    	strcpy ((char *) source_fullnamestr, (char *) &data[sourceaddr]);
	}
	else
	{
		printf ("ERROR: file_rename: filename too long!\n", &data[sourceaddr]);
		return (NULL);
	}
    #endif

    // get dest full name
    #if SANDBOX
	if (get_sandbox_filename (&data[destaddr],  dest_fullnamestr, 255) != 0)
	{
		printf ("ERROR: file_rename: illegal filename: %s\n", &data[destaddr]);
		return (NULL);
	}
	#else
	if (strlen_safe (&data[destaddr], 255) < 255)
	{
    	strcpy ((char *) dest_fullnamestr, (char *) &data[destaddr]);
	}
	else
	{
		printf ("ERROR: file_rename: filename too long!\n", &data[destaddr]);
		return (NULL);
	}
    #endif

    sourcestr.assign ((const char *) source_fullnamestr);
    deststr.assign ((const char *) dest_fullnamestr);

    rename (sourcestr, deststr);

    // push return value
    sp = stpushi (ret, sp, sp_bottom);
    if (sp == NULL)
    {
    	// error
    	printf ("file_rename: ERROR: stack corrupt!\n");
    	return (NULL);
    }
    return (sp);
}

extern "C" U1 *file_size_bytes (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 filenameaddr ALIGN;
	S8 file_name_len ALIGN;
    S8 file_size_bytes ALIGN;

    U1 source_fullnamestr[256];

    std::string filenamestr;

	sp = stpopi ((U1 *) &filenameaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_size_bytes: ERROR: stack corrupt!\n");
		return (NULL);
	}

	file_name_len = strlen_safe ((char *) &data[filenameaddr], 255);
	if (file_name_len > 255)
	{
		printf ("ERROR: file_size_bytes: file name source : '%s' too long!\n", (char *) &data[filenameaddr]);
		return (NULL);
	}

    // get file full name
    #if SANDBOX
	if (get_sandbox_filename (&data[filenameaddr],  source_fullnamestr, 255) != 0)
	{
		printf ("ERROR: file_size_bytes: illegal filename: %s\n", &data[filenameaddr]);
		return (NULL);
	}
	#else
	if (strlen_safe (&data[filenameeaddr], 255) < 255)
	{
    	strcpy ((char *) source_fullnamestr, (char *) &data[filenameaddr]);
	}
	else
	{
		printf ("ERROR: file_size_bytes: filename too long!\n", &data[filenameddr]);
		return (NULL);
	}
    #endif

    filenamestr.assign ((const char *) source_fullnamestr);

    file_size_bytes = file_size (filenamestr);

    // push return value
    sp = stpushi (file_size_bytes, sp, sp_bottom);
    if (sp == NULL)
    {
    	// error
    	printf ("file_size_bytes: ERROR: stack corrupt!\n");
    	return (NULL);
    }
    return (sp);
}

extern "C" U1 *file_exists (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 filenameaddr ALIGN;
    S8 ret ALIGN = 0;
	S8 file_name_len ALIGN;

    bool file_exists;

    U1 source_fullnamestr[256];

    std::string filenamestr;

	sp = stpopi ((U1 *) &filenameaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_exists: ERROR: stack corrupt!\n");
		return (NULL);
	}

	file_name_len = strlen_safe ((char *) &data[filenameaddr], 255);
	if (file_name_len > 255)
	{
		printf ("ERROR: file_exists: file name source : '%s' too long!\n", (char *) &data[filenameaddr]);
		return (NULL);
	}

    // get file full name
    #if SANDBOX
	if (get_sandbox_filename (&data[filenameaddr],  source_fullnamestr, 255) != 0)
	{
		printf ("ERROR: file_exists illegal filename: %s\n", &data[filenameaddr]);
		return (NULL);
	}
	#else
	if (strlen_safe (&data[filenameeaddr], 255) < 255)
	{
    	strcpy ((char *) source_fullnamestr, (char *) &data[filenameaddr]);
	}
	else
	{
		printf ("ERROR: file_exists: filename too long!\n", &data[filenameddr]);
		return (NULL);
	}
    #endif

    filenamestr.assign ((const char *) source_fullnamestr);

    file_exists = exists (filenamestr);

    if (file_exists == true)
    {
        ret = 1;
    }
    else
    {
        ret = 0;
    }

    // push return value
    sp = stpushi (ret, sp, sp_bottom);
    if (sp == NULL)
    {
    	// error
    	printf ("file_size_exists: ERROR: stack corrupt!\n");
    	return (NULL);
    }
    return (sp);
}

extern "C" U1 *directory_entries (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 diraddr ALIGN;
	S8 dir_name_len ALIGN;
    S8 dir_entries ALIGN = 0;     // number of files in directory

    U1 dir_namestr[256];

    std::string dirstr;

    sp = stpopi ((U1 *) &diraddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: directory_entries: ERROR: stack corrupt!\n");
		return (NULL);
	}

    dir_name_len = strlen_safe ((char *) &data[diraddr], 255);
	if (dir_name_len > 255)
	{
		printf ("ERROR: directory %s' too long!\n", (char *) &data[diraddr]);
		return (NULL);
	}

   // get dir full name
    #if SANDBOX
	if (get_sandbox_filename (&data[diraddr], dir_namestr, 255) != 0)
	{
		printf ("ERROR: directory_entries: illegal name: %s\n", &data[diraddr]);
		return (NULL);
	}
	#else
	if (strlen_safe (&data[diraddr], 255) < 255)
	{
    	strcpy ((char *) dir_namestr, (char *) &data[sourceaddr]);
	}
	else
	{
		printf ("ERROR: directory_entries: name too long!\n", &data[diraddr]);
		return (NULL);
	}
    #endif

    dirstr.assign ((const char *) dir_namestr);

    for (const directory_entry& entry: directory_iterator{dirstr})
    {
        dir_entries++;
    }

    // push return value file entries
    sp = stpushi (dir_entries, sp, sp_bottom);
    if (sp == NULL)
    {
    	// error
    	printf ("directory_entries: ERROR: stack corrupt!\n");
    	return (NULL);
    }
    return (sp);
}

extern "C" U1 *directory_files (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 diraddr ALIGN;
	S8 dir_name_len ALIGN;
    S8 dir_entries ALIGN = 0;     // number of files in directory

    U1 dir_namestr[256];

    S8 strdestaddr ALIGN;
	S8 index ALIGN = 0;
	S8 array_size ALIGN;
	S8 string_len ALIGN;
	S8 index_real ALIGN;
	S8 string_len_src ALIGN;

    std::string dirstr;
    std::string direlement;

	sp = stpopi ((U1 *) &array_size, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_string_to_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &string_len, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_string_to_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// ERROR:
		printf ("string_string_to_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &diraddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: directory_files: ERROR: stack corrupt!\n");
		return (NULL);
	}

    dir_name_len = strlen_safe ((char *) &data[diraddr], 255);
	if (dir_name_len > 255)
	{
		printf ("ERROR: directory %s' too long!\n", (char *) &data[diraddr]);
		return (NULL);
	}

    // get dir full name
    #if SANDBOX
	if (get_sandbox_filename (&data[diraddr], dir_namestr, 255) != 0)
	{
		printf ("ERROR: directory_files: illegal name: %s\n", &data[diraddr]);
		return (NULL);
	}
	#else
	if (strlen_safe (&data[diraddr], 255) < 255)
	{
    	strcpy ((char *) dir_namestr, (char *) &data[sourceaddr]);
	}
	else
	{
		printf ("ERROR: directory_files: name too long!\n", &data[diraddr]);
		return (NULL);
	}
    #endif

    dirstr.assign ((const char *) dir_namestr);

    for (const directory_entry& entry: directory_iterator{dirstr})
    {
        direlement = entry.path ();

        // convert string to C string
        U1 *dir_namestr = (U1 *) direlement.c_str ();

        string_len_src = strlen_safe ((const char *) dir_namestr, MAXLINELEN);
        if (string_len_src > string_len)
        {
            // ERROR:
            printf ("directory_files: ERROR: source string overflow!\n");
            return (NULL);
        }

        index_real = index * string_len;
        if (index_real < 0 || index_real > array_size)
        {
            // ERROR:
            printf ("directory_files: ERROR: destination array overflow!\n");
            return (NULL);
        }

        #if BOUNDSCHECK
     	if (memory_bounds (strdestaddr + index_real, string_len_src) != 0)
        {
            printf ("directiory_files: ERROR: dest string overflow!\n");
            return (NULL);
        }
        #endif

        strcpy ((char *) &data[strdestaddr + index_real], (const char *) dir_namestr);

        dir_entries++;
        // increase target string index
        index++;
    }

    // push return value file entries
    sp = stpushi (dir_entries, sp, sp_bottom);
    if (sp == NULL)
    {
    	// error
    	printf ("directory_files: ERROR: stack corrupt!\n");
    	return (NULL);
    }

    return (sp);
}
