//  l1vm
//  file.c
// 

#include "../../../include/global.h"

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


U1 *file_open (U1 *sp, U1 *sp_top, U1 *sp_bottom)
{
    // first argument: U1 filehandle number -> on stack top
    // second argument: U1 access
    // third argument: zero terminated filename
    
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
    
    printf ("file_open: sp: %i handle\n", *sp);
    
    handle = *sp;
    sp++;
    
	printf ("file_open: sp: %i access\n", *sp);
	
    access = *sp;
    sp++;
    
    while (*sp != 0)
    {
       if (i < 512)
       {
           name[i] = *sp;
		   printf ("file_open: stack: filenname: '%c'\n", *sp);
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
        
    printf ("file_open: filename: '%s'\n", files[handle].name);    
        
    files[handle].fptr = fopen ((char *) files[handle].name, (char *) access_str);
    if (files[handle].fptr == NULL)
    {
        printf ("ERROR: file_open: can't open file %s!\n", files[handle].name);
        return (NULL);
    }
    
    files[handle].state = FILEOPEN;
    return (sp);
}

U1 *file_close (U1 *sp, U1 *sp_top, U1 *sp_bottom)
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
	
U1 *file_putc (U1 *sp, U1 *sp_top, U1 *sp_bottom)
{
    // first argument: U1 filehandle number -> on stack top
    // second argument: U1 byte to write
    
    U1 ch, handle;
    
    if (sp == sp_top)
    {
        // nothing on stack!! can't pop!!
        
        printf ("FATAL ERROR: file_open: stack pointer can't pop empty stack!\n");
        return (NULL);
    }
    
    handle = *sp;
    sp++;
    
    ch = *sp;
    sp++;
    
	printf ("file_putc: handle %i\n", handle);
	
    if (handle < 0 || handle >= MAXFILES)
    {
        printf ("ERROR: file_putc: handle out of range!\n");
        return (NULL);
    }
    
    if (fputc (ch, files[handle].fptr) != ch)
    {
        printf ("ERROR: file_putc: can't write to file %s!\n", files[handle].name);
        return (NULL);
    }
    return (sp);
}

