/*
 * This file file.c is part of L1vm.
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
#include "../../../include/stack.h"

// protos

extern S2 memory_bounds (S8 start, S8 offset_access);


#define FILEOPEN 1              // state flags
#define FILECLOSED 0

#define FILEREAD        1
#define FILEWRITE       2
#define FILEREADWRITE   3
#define FILEWRITEREAD   4
#define FILEAPPEND		5

struct data_info data_info[MAXDATAINFO];
S8 data_info_ind;

S2 init_memory_bounds (struct data_info *data_info_orig, S8 data_info_ind_orig)
{
	memcpy (&data_info, &data_info_orig, sizeof (data_info_orig));
	data_info_ind = data_info_ind_orig;

	return (0);
}

U1 get_sandbox_filename (U1 *filename, U1 *sandbox_filename, S2 max_name_len);

size_t strlen_safe (const char * str, int maxlen);

struct file
{
    FILE *fptr;
    U1 name[256];
    U1 state;
};

static struct file *files = NULL;
static S8 filemax ALIGN = 0;


U1 *file_init_state (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 i ALIGN;
	S8 maxind ALIGN;

	sp = stpopi ((U1 *) &maxind, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("file_init_state: ERROR: stack corrupt!\n");
		return (NULL);
	}

	// allocate gobal mem structure
	files = (struct file*) calloc (maxind, sizeof (struct file));
	if (files == NULL)
	{
		printf ("file_init_state: ERROR can't allocate %lli memory indexes!\n", maxind);

		sp = stpushi (1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("file_init_state: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}

	filemax = maxind;	// save to global var

	for (i = 0; i < filemax; i++)
	{
		files[i].state = FILECLOSED;
	}

	// error code ok
	sp = stpushi (0, sp, sp_bottom);
	if (sp == NULL)
	{
		// error
		printf ("file_init_state: ERROR: stack corrupt!\n");
		return (NULL);
	}
	return (sp);
}

U1 *free_mem (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	if (files) free (files);
	return (sp);
}

S8 get_free_file_handle (void)
{
	S8 i ALIGN;

	for (i = 0; i < filemax; i++)
	{
		if (files[i].state == FILECLOSED)
		{
			return (i);
		}
	}

	// no free file handle found
	return (-1);
}

U1 *file_open (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    // first argument: int64 filehandle number
    // second argument: U1 access
    // third argument: int64 name string address
    // return value: ERROR code

    S8 handle ALIGN;
    U1 access;
    U1 access_str[3];
    S8 nameaddr ALIGN;

	S8 file_name_len ALIGN;
	S8 file_access_name_len ALIGN;

    sp = stpopi ((U1 *) &nameaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_open: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopb ((U1 *) &access, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_open: ERROR: stack corrupt!\n");
		return (NULL);
	}

	handle = get_free_file_handle ();
	if (handle == -1)
	{
		// no free file handle available, EXIT
		sp = stpushi (ERR_FILE_OPEN, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("file_fopen: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}

    if (files[handle].state == FILEOPEN)
    {
        printf ("ERROR: file_open: handle %lli already open!\n", handle);
        return (NULL);
    }

	file_name_len = strlen_safe ((char *) &data[nameaddr], 255);
	if (file_name_len > 255)
	{
		printf ("ERROR: file_open: file name: '%s' too long!\n", (char *) &data[nameaddr]);
		return (NULL);
	}

	file_access_name_len = file_name_len;

	#if SANDBOX
	if (get_sandbox_filename (&data[nameaddr], files[handle].name, 255) != 0)
	{
		printf ("ERROR: file_open: illegal filename: %s\n", &data[nameaddr]);
		return (NULL);
	}
	#else
	if (strlen_safe ((char *) &data[nameaddr], 255) < 255)
	{
    	strcpy ((char *) files[handle].name, (char *) &data[nameaddr]);
	}
	else
	{
		printf ("ERROR: file_open: filename %s too long!\n", &data[nameaddr]);
		return (NULL);
	}
	#endif

    switch (access)
    {
        case FILEREAD:
            strcpy ((char *) access_str, "r");
            break;

        case FILEWRITE:
            strcpy ((char *) access_str, "w");
            break;

        case FILEREADWRITE:
            strcpy ((char *) access_str, "r+");
            break;

        case FILEWRITEREAD:
            strcpy ((char *) access_str, "w+");
            break;

		case FILEAPPEND:
			strcpy ((char *) access_str, "a");
			break;

        default:
            printf ("ERROR: file_open: unknown file mode! %i\n", access);
            return (NULL);
    }

	// DEBUG
    // printf ("file_open: filename: '%s'\n", files[handle].name);

    files[handle].fptr = fopen ((char *) files[handle].name, (char *) access_str);
    if (files[handle].fptr == NULL)
    {
        files[handle].state = FILECLOSED;

        // push ERROR code ERROR
        sp = stpushi (ERR_FILE_OPEN, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_fopen: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    else
    {
        files[handle].state = FILEOPEN;

        // push file handle
        sp = stpushi (handle, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_fopen: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    return (sp);
}

U1 *file_close (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// first argument: U1 filehandle number -> on stack top

	S8 handle ALIGN;

	if (sp == sp_top)
	{
		// nothing on stack!! can't pop!!

		printf ("FATAL ERROR: file_close: stack pointer can't pop empty stack!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_close: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (handle < 0 || handle >= filemax)
	{
		printf ("ERROR: file_close: handle out of range!\n");
		return (NULL);
	}

    // avoid double close bug
    if (files[handle].state == FILEOPEN)
    {
        fclose (files[handle].fptr);
        files[handle].state = FILECLOSED;
    }
	return (sp);
}

U1 *file_seek (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    // first argument: U1 filehandle number
    // second argument: offset
    // third argument: origin
    // return value: ERROR code

    // origin:
    // SEEK_SET: 1, beginning of file
    // SEEK_CURR: 2, current position
    // SEEK_END: 3, end of file

    S8 handle ALIGN;
    S8 offset ALIGN;
    S8 origin ALIGN;

    if (sp == sp_top)
	{
		// nothing on stack!! can't pop!!

		printf ("FATAL ERROR: file_seek: stack pointer can't pop empty stack!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &origin, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("file_seek: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &offset, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("file_seek: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_seek: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (handle < 0 || handle >= filemax)
	{
		printf ("ERROR: file_seek: handle out of range!\n");
		return (NULL);
	}

    if (fseek (files[handle].fptr, offset, origin) == 0)
    {
        // push ERROR code OK
        sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_fseek: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    else
    {
        // push ERROR code WRONG POS
        sp = stpushi (ERR_FILE_FPOS, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_fseek: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    return (sp);
}

U1 *file_flush (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    // flush open files buffers to disk

    S2 ret;

    ret = fflush (NULL);     // NULL pointer means flush all write file streams buffers to disk.
    if (ret == 0)
    {
        // push ERROR code WRONG POS
        sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_flush: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    else
    {
        // push ERROR code WRONG POS
        sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_flush: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    return (sp);
}


// int16 put/get

U1 *file_put_int16 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    // first argument: U1 filehandle number
    // second argument: S8 int to write
    // return value: ERROR CODE

    S8 handle ALIGN;
    S2 num;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: file_put_int16: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &num, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("file_put_int16: ERROR: stack corrupt!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_put_int16: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (handle < 0 || handle >= filemax)
    {
        printf ("ERROR: file_put_int16: handle out of range!\n");
        return (NULL);
    }

    if (fwrite (&num, sizeof (S2), 1, files[handle].fptr) != 1)
    {
        // push ERROR code WRITE
        sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_put_int16 ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    else
    {
        // push OK code WRITE
        sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_put_int16 ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    return (sp);
}

U1 *file_get_int16 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    // first argument: U1 filehandle number -> on stack top
    // return value: ERROR CODE, number

    S8 handle ALIGN;
    S2 num;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: file_get_int16: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_get_int16: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (handle < 0 || handle >= filemax)
    {
        printf ("ERROR: file_get_int16: handle out of range!\n");
        return (NULL);
    }

    if (fread (&num, sizeof (S2), 1, files[handle].fptr) != 1)
    {
        // dummy read push
        num = 0;
        sp = stpushi (num, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_get_int16: ERROR: stack corrupt!\n");
    		return (NULL);
    	}

        // push ERROR code READ
        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_get_int16 ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
	else
	{
    	sp = stpushi (num, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("file_get_int16: ERROR: stack corrupt!\n");
			return (NULL);
		}

    	sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
	    if (sp == NULL)
    	{
        	// error
        	printf ("file_get_int16 ERROR: stack corrupt!\n");
        	return (NULL);
		}
	}

	return (sp);
}

// int32 put/get

U1 *file_put_int32 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    // first argument: U1 filehandle number
    // second argument: S8 int to write
    // return value: ERROR CODE

    S8 handle ALIGN;
    S4 num;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: file_put_int32: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &num, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("file_put_int32: ERROR: stack corrupt!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_put_int32: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (handle < 0 || handle >= filemax)
    {
        printf ("ERROR: file_put_int32: handle out of range!\n");
        return (NULL);
    }

    if (fwrite (&num, sizeof (S4), 1, files[handle].fptr) != 1)
    {
        // push ERROR code WRITE
        sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_put_int32 ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    else
    {
        // push OK code WRITE
        sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_put_int32 ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    return (sp);
}

U1 *file_get_int32 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    // first argument: U1 filehandle number -> on stack top
    // return value: ERROR CODE, number

    S8 handle ALIGN;
    S4 num;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: file_get_int32: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_get_int32: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (handle < 0 || handle >= filemax)
    {
        printf ("ERROR: file_get_int32: handle out of range!\n");
        return (NULL);
    }

    if (fread (&num, sizeof (S4), 1, files[handle].fptr) != 1)
    {
        // dummy read push
        num = 0;
        sp = stpushi (num, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_get_int32: ERROR: stack corrupt!\n");
    		return (NULL);
    	}

        // push ERROR code READ
        sp = stpushi(ERR_FILE_READ, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_get_int32 ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
	else
	{
    	sp = stpushi (num, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("file_get_int32: ERROR: stack corrupt!\n");
			return (NULL);
		}

	    sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
	    if (sp == NULL)
	    {
	        // error
	        printf ("file_get_int32 ERROR: stack corrupt!\n");
        	return (NULL);
		}
    }

	return (sp);
}

// int64 put/get

U1 *file_put_int64 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    // first argument: U1 filehandle number
    // second argument: S8 int to write
    // return value: ERROR CODE

    S8 handle ALIGN;
    S8 num ALIGN;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: file_put_int64: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &num, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("file_put_int64: ERROR: stack corrupt!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_put_int64: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (handle < 0 || handle >= filemax)
    {
        printf ("ERROR: file_put_int64: handle out of range!\n");
        return (NULL);
    }

    if (fwrite (&num, sizeof (S8), 1, files[handle].fptr) != 1)
    {
        // push ERROR code WRITE
        sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_put_int64 ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    else
    {
        // push OK code WRITE
        sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_put_int64 ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    return (sp);
}

U1 *file_get_int64 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    // first argument: U1 filehandle number -> on stack top
    // return value: ERROR CODE, number

    S8 handle ALIGN;
    S8 num ALIGN;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: file_get_int64: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_get_int64: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (handle < 0 || handle >= filemax)
    {
        printf ("ERROR: file_get_int64: handle out of range!\n");
        return (NULL);
    }

    if (fread (&num, sizeof (S8), 1, files[handle].fptr) != 1)
    {
        // dummy read push
        num = 0;
        sp = stpushi (num, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_get_int64: ERROR: stack corrupt!\n");
    		return (NULL);
    	}

        // push ERROR code READ
        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_get_int64 ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
	else
	{
    	sp = stpushi (num, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("file_get_int64: ERROR: stack corrupt!\n");
			return (NULL);
		}

	    sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    	if (sp == NULL)
	    {
        	// error
        	printf ("file_get_int64 ERROR: stack corrupt!\n");
        	return (NULL);
		}
    }

	return (sp);
}

// double

U1 *file_put_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    // first argument: U1 filehandle number
    // second argument: S8 int to write
    // return value: ERROR CODE

    S8 handle ALIGN;
    F8 num ALIGN;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: file_put_double: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopd ((U1 *) &num, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("file_put_double: ERROR: stack corrupt!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_put_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (handle < 0 || handle >= filemax)
    {
        printf ("ERROR: file_put_double: handle out of range!\n");
        return (NULL);
    }

    if (fwrite (&num, sizeof (F8), 1, files[handle].fptr) != 1)
    {
        // push ERROR code WRITE
        sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_put_double ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    else
    {
        // push OK code WRITE
        sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_put_double ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    return (sp);
}

U1 *file_get_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    // first argument: U1 filehandle number -> on stack top
    // return value: ERROR CODE, number

    S8 handle ALIGN;
    F8 num ALIGN;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: file_get_double: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_get_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

	if (handle < 0 || handle >= filemax)
    {
        printf ("ERROR: file_get_double: handle out of range!\n");
        return (NULL);
    }

    if (fread (&num, sizeof (F8), 1, files[handle].fptr) != 1)
    {
        // dummy read push
        num = 0;
        sp = stpushd (num, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_get_double: ERROR: stack corrupt!\n");
    		return (NULL);
    	}

        // push ERROR code READ
        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_get_double ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
	else
	{
    	sp = stpushd (num, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("file_get_double: ERROR: stack corrupt!\n");
			return (NULL);
		}

    	sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    	if (sp == NULL)
    	{
        	// error
        	printf ("file_get_double ERROR: stack corrupt!\n");
	        return (NULL);
		}
    }

	return (sp);
}

// byte put/get

U1 *file_putc (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    // first argument: U1 filehandle number -> on stack top
    // second argument: U1 byte to write
    // return value: ERROR CODE

	S8 handle ALIGN;
    U1 ch;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: file_putc: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

	sp = stpopb ((U1 *) &ch, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_putc: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_putc: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (handle < 0 || handle >= filemax)
    {
        printf ("ERROR: file_putc: handle out of range!\n");
        return (NULL);
    }

    if (fputc (ch, files[handle].fptr) != ch)
    {
        // push ERROR code WRITE
        sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_putc: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    else
    {
        // push ERROR code OK
        sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_putc: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    return (sp);
}

U1 *file_getc (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    // first argument: U1 filehandle number -> on stack top
    // return value: char of file
    // return value: ERROR CODE

	S8 handle ALIGN;
    S2 ch;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: file_getc: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_getc: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (handle < 0 || handle >= filemax)
    {
        printf ("ERROR: file_getc: handle out of range!\n");
        return (NULL);
    }
    if ((ch = getc (files[handle].fptr)) != EOF)
    {
        sp = stpushi (ch, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_getc: ERROR: stack corrupt!\n");
    		return (NULL);
    	}

        // push ERROR code OK
        sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_getc: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    else
    {
        sp = stpushi (0, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_getc: ERROR: stack corrupt!\n");
    		return (NULL);
    	}

        // push ERROR code EOF
        sp = stpushi (ERR_FILE_EOF, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_getc: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    return (sp);
}


// string put/get

char *fgets_uni (char *str, int len, FILE *fptr)
{
    int ch, nextch;
    int i = 0, eol = FALSE;
    char *ret;

    ch = fgetc (fptr);
    if (feof (fptr))
    {
        return (NULL);
    }
    while (! feof (fptr) || i == len - 2)
    {
        switch (ch)
        {
            case '\r':
                /* check for '\r\n\' */

                nextch = fgetc (fptr);
                if (! feof (fptr))
                {
                    if (nextch != '\n')
                    {
                        ungetc (nextch, fptr);
                    }
                }
                str[i] = '\n';
                i++; eol = TRUE;
                break;

            case '\n':
                /* check for '\n\r\' */

                nextch = fgetc (fptr);
                if (! feof (fptr))
                {
                    if (nextch != '\r')
                    {
                        ungetc (nextch, fptr);
                    }
                }
                str[i] = '\n';
                i++; eol = TRUE;
                break;

            default:
				str[i] = ch;
				i++;

                break;
        }

        if (eol)
        {
            break;
        }

        ch = fgetc (fptr);
    }

    if (feof (fptr))
    {
//        str[i] = '\n';
//        i++;
        str[i] = '\0';
    }
    else
    {
        str[i] = '\0';
    }

    ret = str;
    return (ret);
}

U1 *file_put_string (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    // first argument: U1 filehandle number -> on stack top
    // second argument: string address to write
    // return value: ERROR CODE

    S8 handle ALIGN;
    S8 string_address ALIGN;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: file_put_string: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &string_address, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("file_put_string: ERROR: stack corrupt!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_put_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (handle < 0 || handle >= filemax)
    {
        printf ("ERROR: file_put_string: handle out of range!\n");
        return (NULL);
    }

    if (fputs ((const char *) &data[string_address], files[handle].fptr) == EOF)
    {
    	// push ERROR code WRITE
        sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_put_string: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    else
    {
        // push ERROR code OK
        sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_put_string: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    return (sp);
}

U1 *file_get_string (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    // first argument: U1 filehandle number -> on stack top
    // second argument: string address to read
    // third argument: string max len
    // return value: ERROR CODE

    S8 handle ALIGN;
    S8 string_address ALIGN;
    S8 slen ALIGN;

	S8 real_slen ALIGN;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: file_get_string: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &slen, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("file_get_string: ERROR: stack corrupt!\n");
        return (NULL);
    }

    sp = stpopi ((U1 *) &string_address, sp, sp_top);
    if (sp == NULL)
    {
        // error
        printf ("file_get_string: ERROR: stack corrupt!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_get_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (handle < 0 || handle >= filemax)
    {
        printf ("ERROR: file_get_string: handle out of range!\n");
        return (NULL);
    }

    // char *fgets_uni (char *str, int len, FILE *fptr)
    if (fgets_uni ((char *) &data[string_address], slen, files[handle].fptr) == NULL)
    {
        // error!!!

        // push ERROR code READ
        sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_get_string: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    else
    {
		// check for "\n" and remove it if found

		real_slen = strlen_safe ((const char *) &data[string_address], MAXLINELEN);
		if (data[string_address + real_slen - 1] == '\n')
		{
			data[string_address + real_slen - 1] = '\0';
		}

        // push ERROR code OK
        sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("file_get_string: ERROR: stack corrupt!\n");
            return (NULL);
        }
    }
    return (sp);
}

// array read
U1 *file_read_array (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
    S8 array_data_ptr ALIGN;
	S8 start ALIGN;
	S8 size ALIGN;
	S8 type ALIGN;
	S8 readsize ALIGN;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: file_read_array: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &type, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_read_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &size, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_read_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_read_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_read_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_read_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (handle < 0 || handle >= filemax)
    {
        printf ("ERROR: file_read_array: handle out of range!\n");
        return (NULL);
    }

	switch (type)
	{
		case BYTE:
			#if BOUNDSCHECK
			if (memory_bounds (array_data_ptr, start) != 0)
			{
				printf ("file_read_array: ERROR: array overflow!\n");
				return (NULL);
			}
			if (memory_bounds (array_data_ptr, start + (size - 1) != 0))
			{
				printf ("file_read_array: ERROR: array overflow!\n");
				return (NULL);
			}
			#endif
			
			readsize = fread (&data[array_data_ptr + start], sizeof (U1), size, files[handle].fptr);

            // printf ("DEBUG: file_read_array: readsize: %lli, size: %lli, handle: %lli\n", readsize, size, handle);

			if (readsize != size)
			{
				// push ERROR code READ
        		sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    			if (sp == NULL)
    			{
    				// error
    				printf ("file_read_array: ERROR: stack corrupt!\n");
    				return (NULL);
    			}
				return (sp);
			}
			break;

		case WORD:
			#if BOUNDSCHECK
			if (memory_bounds (array_data_ptr, start * 2) != 0)
			{
				printf ("file_read_array: ERROR: array overflow!\n");
				return (NULL);
			}
			if (memory_bounds (array_data_ptr, (start * 2) + ((size - 1) * 2)) != 0)
			{
				printf ("file_read_array: ERROR: array overflow!\n");
				return (NULL);
			}
			#endif
			
			readsize = fread (&data[array_data_ptr + (start * 2)], sizeof (S2), size, files[handle].fptr);
			if (readsize != size)
			{
				// push ERROR code READ
        		sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    			if (sp == NULL)
    			{
    				// error
    				printf ("file_read_array: ERROR: stack corrupt!\n");
    				return (NULL);
    			}
				return (sp);
			}
			break;

		case DOUBLEWORD:
			#if BOUNDSCHECK
			if (memory_bounds (array_data_ptr, start * 4) != 0)
			{
				printf ("file_read_array: ERROR: array overflow!\n");
				return (NULL);
			}
			if (memory_bounds (array_data_ptr, (start * 4) + ((size - 1) * 4)) != 0)
			{
				printf ("file_read_array: ERROR: array overflow!\n");
				return (NULL);
			}
			#endif
			
			readsize = fread (&data[array_data_ptr + (start * 4)], sizeof (S4), size, files[handle].fptr);
			if (readsize != size)
			{
				// push ERROR code READ
        		sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    			if (sp == NULL)
    			{
    				// error
    				printf ("file_read_array: ERROR: stack corrupt!\n");
    				return (NULL);
    			}
				return (sp);
			}
			break;

		case QUADWORD:
			#if BOUNDSCHECK
			if (memory_bounds (array_data_ptr, start * 8) != 0)
			{
				printf ("file_read_array: ERROR: array overflow!\n");
				return (NULL);
			}
			if (memory_bounds (array_data_ptr, (start * 8) + ((size - 1) * 8)) != 0)
			{
				printf ("file_read_array: ERROR: array overflow!\n");
				return (NULL);
			}
			#endif
			
			readsize = fread (&data[array_data_ptr + (start * 8)], sizeof (S8), size, files[handle].fptr);
			if (readsize != size)
			{
				// push ERROR code READ
        		sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    			if (sp == NULL)
    			{
    				// error
    				printf ("file_read_array: ERROR: stack corrupt!\n");
    				return (NULL);
    			}
				return (sp);
			}
			break;

		case DOUBLEFLOAT:
			#if BOUNDSCHECK
			if (memory_bounds (array_data_ptr, start * 8) != 0)
			{
				printf ("file_read_array: ERROR: array overflow!\n");
				return (NULL);
			}
			if (memory_bounds (array_data_ptr, (start * 8) + ((size - 1) * 8)) != 0)
			{
				printf ("file_read_array: ERROR: array overflow!\n");
				return (NULL);
			}
			#endif
			
			readsize = fread (&data[array_data_ptr + (start * 8)], sizeof (F8), size, files[handle].fptr);
			if (readsize != size)
			{
				// push ERROR code READ
        		sp = stpushi (ERR_FILE_READ, sp, sp_bottom);
    			if (sp == NULL)
    			{
    				// error
    				printf ("file_read_array: ERROR: stack corrupt!\n");
    				return (NULL);
    			}
				return (sp);
			}
			break;
	}

	// push ERROR code OK
    sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    if (sp == NULL)
    {
    	// error
    	printf ("file_read_array: ERROR: stack corrupt!\n");
    	return (NULL);
    }
    return (sp);
}

// array write
U1 *file_write_array (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 handle ALIGN;
    S8 array_data_ptr ALIGN;
	S8 start ALIGN;
	S8 size ALIGN;
	S8 type ALIGN;
	S8 writesize ALIGN;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: file_write_array: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

	sp = stpopi ((U1 *) &type, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_write_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &size, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_write_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &start, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_write_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &array_data_ptr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_write_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("ERROR: file_write_array: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (handle < 0 || handle >= filemax)
    {
        printf ("ERROR: file_write_array: handle out of range!\n");
        return (NULL);
    }

	switch (type)
	{
		case BYTE:
			#if BOUNDSCHECK
			if (memory_bounds (array_data_ptr, start) != 0)
			{
				printf ("file_write_array: ERROR: array overflow!\n");
				return (NULL);
			}
			if (memory_bounds (array_data_ptr, start + (size - 1) != 0))
			{
				printf ("file_write_array: ERROR: array overflow!\n");
				return (NULL);
			}
			#endif
			
			writesize = fwrite (&data[array_data_ptr + start], sizeof (U1), size, files[handle].fptr);
			if (writesize != size)
			{
				// push ERROR code WRITE
        		sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
    			if (sp == NULL)
    			{
    				// error
    				printf ("file_write_array: ERROR: stack corrupt!\n");
    				return (NULL);
    			}
				return (sp);
			}
			break;

		case WORD:
			#if BOUNDSCHECK
			if (memory_bounds (array_data_ptr, start * 2) != 0)
			{
				printf ("file_write_array: ERROR: array overflow!\n");
				return (NULL);
			}
			if (memory_bounds (array_data_ptr, (start * 2) + ((size - 1) * 2)) != 0)
			{
				printf ("file_write_array: ERROR: array overflow!\n");
				return (NULL);
			}
			#endif
			
			writesize = fwrite (&data[array_data_ptr + (start * 2)], sizeof (S2), size, files[handle].fptr);
			if (writesize != size)
			{
				// push ERROR code WRITE
        		sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
    			if (sp == NULL)
    			{
    				// error
    				printf ("file_write_array: ERROR: stack corrupt!\n");
    				return (NULL);
    			}
				return (sp);
			}
			break;

		case DOUBLEWORD:
			#if BOUNDSCHECK
			if (memory_bounds (array_data_ptr, start * 4) != 0)
			{
				printf ("file_write_array: ERROR: array overflow!\n");
				return (NULL);
			}
			if (memory_bounds (array_data_ptr, (start * 4) + ((size - 1) * 4)) != 0)
			{
				printf ("file_write_array: ERROR: array overflow!\n");
				return (NULL);
			}
			#endif
			
			writesize = fwrite (&data[array_data_ptr + (start * 4)], sizeof (S4), size, files[handle].fptr);
			if (writesize != size)
			{
				// push ERROR code WRITE
        		sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
    			if (sp == NULL)
    			{
    				// error
    				printf ("file_write_array: ERROR: stack corrupt!\n");
    				return (NULL);
    			}
				return (sp);
			}
			break;

		case QUADWORD:
			#if BOUNDSCHECK
			if (memory_bounds (array_data_ptr, start * 8) != 0)
			{
				printf ("file_write_array: ERROR: array overflow!\n");
				return (NULL);
			}
			if (memory_bounds (array_data_ptr, (start * 8) + ((size - 1) * 8)) != 0)
			{
				printf ("file_write_array: ERROR: array overflow!\n");
				return (NULL);
			}
			#endif
			
			writesize = fwrite (&data[array_data_ptr + (start * 8)], sizeof (S8), size, files[handle].fptr);
			if (writesize != size)
			{
				// push ERROR code WRITE
        		sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
    			if (sp == NULL)
    			{
    				// error
    				printf ("file_write_array: ERROR: stack corrupt!\n");
    				return (NULL);
    			}
				return (sp);
			}
			break;

		case DOUBLEFLOAT:
			#if BOUNDSCHECK
			if (memory_bounds (array_data_ptr, start * 8) != 0)
			{
				printf ("file_write_array: ERROR: array overflow!\n");
				return (NULL);
			}
			if (memory_bounds (array_data_ptr, (start * 8) + ((size - 1) * 8)) != 0)
			{
				printf ("file_write_array: ERROR: array overflow!\n");
				return (NULL);
			}
			#endif
			
			writesize = fwrite (&data[array_data_ptr + (start * 8)], sizeof (F8), size, files[handle].fptr);
			if (writesize != size)
			{
				// push ERROR code WRITE
        		sp = stpushi (ERR_FILE_WRITE, sp, sp_bottom);
    			if (sp == NULL)
    			{
    				// error
    				printf ("file_write_array: ERROR: stack corrupt!\n");
    				return (NULL);
    			}
				return (sp);
			}
			break;
	}
	
	// push ERROR code OK
    sp = stpushi (ERR_FILE_OK, sp, sp_bottom);
    if (sp == NULL)
    {
    	// error
    	printf ("file_write_array: ERROR: stack corrupt!\n");
    	return (NULL);
    }
    return (sp);
}
