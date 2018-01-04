/*
 * This file file.c is part of L1vm.
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

#include "../../../include/global.h"
#include "../../../include/stack.h"

#define MAXFILES 32             // max number of open files

#define FILEOPEN 1              // state flags
#define FILECLOSED 0

#define FILEREAD        1
#define FILEWRITE       2
#define FILEREADWRITE   3
#define FILEWRITEREAD   4

struct file
{
    FILE *fptr;
    U1 name[256];
    U1 state;
};

struct file files[MAXFILES];


U1 *file_open (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    // first argument: U1 filehandle number -> on stack top
    // second argument: U1 access
    // third argument: zero terminated filename
    // return value: ERROR code

    U1 handle;
    U1 access;
    U1 access_str[3];
    U1 name[512];
    S8 i = 0 ALIGN;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: file_open: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    handle = *sp;
    sp++;

    access = *sp;
    sp++;

    while (*sp != 0)
    {
       if (i < 512)
       {
           name[i] = *sp;
           i++;
           sp++;
           if (sp == sp_top)
           {
               printf ("FATAL ERROR: file_open: stack corrupt!\n");
               return (NULL);
           }
       }
    }
    name[i] = '\0';        // end of string

    if (handle < 0 || handle >= MAXFILES)
    {
        printf ("ERROR: file_open: handle out of range!\n");
        return (NULL);
    }

    if (files[handle].state == FILEOPEN)
    {
        printf ("ERROR: file_open: handle %i already open!\n", handle);
        return (NULL);
    }

    strcpy ((char *) files[handle].name, (char *) name);

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

        default:
            printf ("ERROR: file_open: unknown file mode! %i\n", access);
            return (NULL);
    }

    // printf ("file_open: filename: '%s'\n", files[handle].name);

    files[handle].fptr = fopen ((char *) files[handle].name, (char *) access_str);
    if (files[handle].fptr == NULL)
    {
        // push ERROR code ERROR
        sp = stpushb (ERR_FILE_OPEN, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_fopen: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    else
    {
        // push ERROR code OK
        sp = stpushb (ERR_OK, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_fopen: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }

    files[handle].state = FILEOPEN;
    return (sp);
}

U1 *file_close (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	// first argument: U1 filehandle number -> on stack top

	U1 handle;

	if (sp == sp_top)
	{
		// nothing on stack!! can't pop!!

		printf ("FATAL ERROR: file_close: stack pointer can't pop empty stack!\n");
		return (NULL);
	}

	handle = *sp;
	sp++;

	if (handle < 0 || handle >= MAXFILES)
	{
		printf ("ERROR: file_close: handle out of range!\n");
		return (NULL);
	}

	fclose (files[handle].fptr);
	files[handle].state = FILECLOSED;
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

    U1 handle;
    S8 offset ALIGN, origin ALIGN;

    if (sp == sp_top)
	{
		// nothing on stack!! can't pop!!

		printf ("FATAL ERROR: file_seek: stack pointer can't pop empty stack!\n");
		return (NULL);
	}

	handle = *sp;
	sp++;

	if (handle < 0 || handle >= MAXFILES)
	{
		printf ("ERROR: file_seek: handle out of range!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &offset, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("file_seek: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &origin, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("file_seek: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (fseek (files[handle].fptr, offset, origin) == 0)
    {
        // push ERROR code OK
        sp = stpushb (ERR_OK, sp, sp_bottom);
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
        sp = stpushb (ERR_WRONG_POS, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_fseek: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    return (sp);
}

U1 *file_putc (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    // first argument: U1 filehandle number -> on stack top
    // second argument: U1 byte to write
    // return value: ERROR CODE

    U1 ch, handle;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: file_putc: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    handle = *sp;
    sp++;

    ch = *sp;
    sp++;

    if (handle < 0 || handle >= MAXFILES)
    {
        printf ("ERROR: file_putc: handle out of range!\n");
        return (NULL);
    }

    if (fputc (ch, files[handle].fptr) != ch)
    {
        // push ERROR code WRITE
        sp = stpushb (ERR_FILE_WRITE, sp, sp_bottom);
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
        sp = stpushb (ERR_OK, sp, sp_bottom);
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

    U1 ch, handle;

    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!

        printf ("FATAL ERROR: file_getc: stack pointer can't pop empty stack!\n");
        return (NULL);
    }

    handle = *sp;
    sp++;

    if (handle < 0 || handle >= MAXFILES)
    {
        printf ("ERROR: file_getc: handle out of range!\n");
        return (NULL);
    }

    if ((ch = getc (files[handle].fptr) != EOF))
    {
        sp = stpushb (ch, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_getc: ERROR: stack corrupt!\n");
    		return (NULL);
    	}

        // push ERROR code OK
        sp = stpushb (ERR_OK, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_getc: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    else
    {
        sp = stpushb (0, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_getc: ERROR: stack corrupt!\n");
    		return (NULL);
    	}

        // push ERROR code EOF
        sp = stpushb (ERR_FILE_EOF, sp, sp_bottom);
    	if (sp == NULL)
    	{
    		// error
    		printf ("file_getc: ERROR: stack corrupt!\n");
    		return (NULL);
    	}
    }
    return (sp);
}
