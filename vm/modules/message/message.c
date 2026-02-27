*
 * This file message.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (info@midnight-coding.de), 2026
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

//#define MSG_MAX 512
#define MSG_STRING_LEN 4096

#define MSG_EMPTY 0
#define MSG_STRING 1
#define MSG_INT64 2
#define MSG_DOUBLE 3
#define MSG_USED 4

static pthread_mutex_t msg_lock = PTHREAD_MUTEX_INITIALIZER;


// protos
extern S2 memory_bounds (S8 start, S8 offset_access);
size_t strlen_safe (const char *str, S8 maxlen);

struct data_info data_info[MAXDATAINFO];
S8 data_info_ind;

struct message
{
    S8 id ALIGN;
    U1 type;
    U1 *string;
    S8 int64 ALIGN;
    F8 doublef ALIGN;
};

struct message *msg;
S8 msg_max;

void msg_init_mem (void)
{
    S8 i ALIGN = 0;

    pthread_mutex_lock (&msg_lock);
    for (i = 0; i < msg_max; i++)
    {
        msg[i].type = MSG_EMPTY;
        msg[i].string = NULL;
    }
    pthread_mutex_unlock (&msg_lock);
}

S2 init_memory_bounds (struct data_info *data_info_orig, S8 data_info_ind_orig)
{
    memcpy (&data_info, &data_info_orig, sizeof (data_info_orig));
    data_info_ind = data_info_ind_orig;

    return (0);
}

S8 msg_get_free_handle (void)
{
    S8 i ALIGN = 0;

    pthread_mutex_lock (&msg_lock);
    for (i = 0; i < msg_max; i++)
    {
        if (msg[i].type == MSG_EMPTY)
        {
            msg[i].type = MSG_USED;
            pthread_mutex_unlock (&msg_lock);
            return (i);
        }
    }
    pthread_mutex_unlock (&msg_lock);
    return (-1); // no free msg handle
}

S8 msg_remove (S8 handle)
{
    pthread_mutex_lock (&msg_lock);
    if (handle >= 0 && handle < msg_max)
    {
        msg[handle].type = MSG_EMPTY;
        if (msg[handle].string) free (msg[handle].string);
        pthread_mutex_unlock (&msg_lock);
        return (0);
    }
    else
    {
        pthread_mutex_unlock (&msg_lock);
        return (-1); // error
    }
}

U1 *msg_init (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 max_handles ALIGN = 0;

    sp = stpopi ((U1 *) &max_handles, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("msg_init: ERROR: stack corrupt!\n");
		return (NULL);
	}

    msg = (struct message *) calloc (max_handles, sizeof (struct message));
    if (msg == NULL)
	{
		printf ("msg_init: ERROR can't allocate %lli message indexes!\n", max_handles);

		sp = stpushi (1, sp, sp_bottom);
		if (sp == NULL)
		{
			// error
			printf ("msg_init: ERROR: stack corrupt!\n");
			return (NULL);
		}
		return (sp);
	}

    msg_max = max_handles; // save to global var

    msg_init_mem ();

    return (sp);
}



U1 *msg_set_int64 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 id ALIGN = 0;
    S8 message ALIGN = 0;
    S8 handle ALIGN = 0;

    sp = stpopi ((U1 *) &message, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("msg_set_int64: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &id, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("msg_set_int64: ERROR: stack corrupt!\n");
		return (NULL);
	}

    handle = msg_get_free_handle ();
    if (handle < 0)
    {
        sp = stpushi (-1, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("msg_set_int64: ERROR: stack corrupt!\n");
            return (NULL);
        }

        return (sp);
    }

    pthread_mutex_lock (&msg_lock);
    msg[handle].type = MSG_INT64;
    msg[handle].id = id;
    msg[handle].int64 = message;
    pthread_mutex_unlock (&msg_lock);

    sp = stpushi (0, sp, sp_bottom);
    if (sp == NULL)
    {                 // error
        printf ("msg_set_int64: ERROR: stack corrupt!\n");
        return (NULL);
    }

    return (sp);
}

U1 *msg_set_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 id ALIGN = 0;
    F8 message ALIGN = 0;
    S8 handle ALIGN = 0;

    sp = stpopi ((U1 *) &message, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("msg_set_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &id, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("msg_set_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

    handle = msg_get_free_handle ();
    if (handle < 0)
    {
        sp = stpushi (-1, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("msg_set_double: ERROR: stack corrupt!\n");
            return (NULL);
        }

        return (sp);
    }

    pthread_mutex_lock (&msg_lock);
    msg[handle].type = MSG_DOUBLE;
    msg[handle].id = id;
    msg[handle].doublef = message;
    pthread_mutex_unlock (&msg_lock);

    sp = stpushi (0, sp, sp_bottom);
    if (sp == NULL)
    {                 // error
        printf ("msg_set_double: ERROR: stack corrupt!\n");
        return (NULL);
    }

    return (sp);
}

U1 *msg_set_string (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 id ALIGN = 0;
    S8 messageaddr ALIGN = 0;
    S8 message_len ALIGN = 0;
    S8 handle ALIGN = 0;

    sp = stpopi ((U1 *) &messageaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("msg_set_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &id, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("msg_set_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

    handle = msg_get_free_handle ();
    if (handle < 0)
    {
        sp = stpushi (-1, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("msg_set_string: ERROR: stack corrupt!\n");
            return (NULL);
        }

        return (sp);
    }

    message_len = strlen_safe ((const char *) data[messageaddr], STRINGMOD_MAXSTRLEN);
    if (message_len == 0)
    {
        sp = stpushi (-1, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("msg_set_string: ERROR: stack corrupt!\n");
            return (NULL);
        }

        return (sp);
    }

    pthread_mutex_lock (&msg_lock);
    msg[handle].type = MSG_STRING;
    msg[handle].id = id;

    // check if already a string was allocated at this handle:
    if (msg[handle].string != NULL)
    {
        free (msg[handle].string);
    }

    msg[handle].string = (U1 *) calloc (message_len + 1, sizeof (U1));
    if (msg[handle].string == NULL)
    {
        printf ("msg_set_string: ERROR can't allocate %lli length for string!\n", message_len + 1);
        pthread_mutex_unlock (&msg_lock);

        sp = stpushi (-1, sp, sp_bottom);
        if (sp == NULL)
        {
            // error
            printf ("msg_set_string: ERROR: stack corrupt!\n");
            return (NULL);
        }

        return (sp);
    }

    strcpy ((char *) msg[handle].string, (const char *) data[messageaddr]);
    pthread_mutex_unlock (&msg_lock);

    sp = stpushi (0, sp, sp_bottom);
    if (sp == NULL)
    {                 // error
        printf ("msg_set_string: ERROR: stack corrupt!\n");
        return (NULL);
    }

    return (sp);
}

U1 *msg_search_id (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 i ALIGN = 0;
    S8 id ALIGN = 0;
    S8 handle ALIGN = 0;

    sp = stpopi ((U1 *) &id, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("msg_search_id: ERROR: stack corrupt!\n");
		return (NULL);
	}

    pthread_mutex_lock (&msg_lock);
    for (i = 0; i < msg_max; i++)
    {
        if (msg[i].id == id)
        {
           pthread_mutex_unlock (&msg_lock);
           sp = stpushi (i, sp, sp_bottom);
           if (sp == NULL)
           {                 // error
               printf ("msg_search_id: ERROR: stack corrupt!\n");
               return (NULL);
           }

           return (sp);
        }
    }
    pthread_mutex_unlock (&msg_lock);

    // id not found
    sp = stpushi (-1, sp, sp_bottom);
    if (sp == NULL)
    {  // error
        printf ("msg_search_id: ERROR: stack corrupt!\n");
        return (NULL);
    }

    return (sp);
}

U1 *msg_get_int64 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 handle ALIGN = 0;

    sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("msg_get_int64: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (handle < 0 || handle >= msg_max)
    {
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {  // error
            printf ("msg_get_int64: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (-1, sp, sp_bottom);
        if (sp == NULL)
        {  // error
            printf ("msg_get_int64: ERROR: stack corrupt!\n");
            return (NULL);
        }

        return (sp);
    }

    if (msg[handle].type != MSG_INT64)
    {
        sp = stpushi (0, sp, sp_bottom);
        if (sp == NULL)
        {  // error
            printf ("msg_get_int64: ERROR: stack corrupt!\n");
            return (NULL);
        }

        // error code
        sp = stpushi (-1, sp, sp_bottom);
        if (sp == NULL)
        {  // error
            printf ("msg_get_int64: ERROR: stack corrupt!\n");
            return (NULL);
        }

        return (sp);
    }

    // return msg
    pthread_mutex_lock (&msg_lock);
    sp = stpushi (msg[handle].int64, sp, sp_bottom);
    if (sp == NULL)
    {  // error
        pthread_mutex_unlock (&msg_lock);
        printf ("msg_get_int64: ERROR: stack corrupt!\n");
        return (NULL);
    }
    pthread_mutex_unlock (&msg_lock);

    sp = stpushi (0, sp, sp_bottom);
    if (sp == NULL)
    {// error
        printf ("msg_get_int64: ERROR: stack corrupt!\n");
        return (NULL);
    }

    return (sp);
}

U1 *msg_get_double (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 handle ALIGN = 0;

    sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("msg_get_double: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (handle < 0 || handle >= msg_max)
    {
        sp = stpushd (0.0, sp, sp_bottom);
        if (sp == NULL)
        {  // error
            printf ("msg_get_int64: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (-1, sp, sp_bottom);
        if (sp == NULL)
        {  // error
            printf ("msg_get_double: ERROR: stack corrupt!\n");
            return (NULL);
        }

        return (sp);
    }

    if (msg[handle].type != MSG_DOUBLE)
    {
        sp = stpushd (0.0, sp, sp_bottom);
        if (sp == NULL)
        {  // error
            printf ("msg_get_double: ERROR: stack corrupt!\n");
            return (NULL);
        }

        sp = stpushi (-1, sp, sp_bottom);
        if (sp == NULL)
        {  // error
            printf ("msg_get_double: ERROR: stack corrupt!\n");
            return (NULL);
        }

        return (sp);
    }

    // return msg
    pthread_mutex_lock (&msg_lock);
    sp = stpushd (msg[handle].doublef, sp, sp_bottom);
    if (sp == NULL)
    {  // error
        pthread_mutex_unlock (&msg_lock);
        printf ("msg_get_double: ERROR: stack corrupt!\n");
        return (NULL);
    }
    pthread_mutex_unlock (&msg_lock);

    sp = stpushi (0, sp, sp_bottom);
    if (sp == NULL)
    {  // error
         printf ("msg_get_double: ERROR: stack corrupt!\n");
        return (NULL);
    }

    return (sp);
}

U1 *msg_get_string (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 handle ALIGN = 0;
    S8 stringaddr ALIGN = 0;
    S8 string_len ALIGN = 0;

    sp = stpopi ((U1 *) &stringaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("msg_get_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("msg_get_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (handle < 0 || handle >= msg_max)
    {
        sp = stpushi (-1, sp, sp_bottom);
        if (sp == NULL)
        {  // error
            printf ("msg_get_string: ERROR: stack corrupt!\n");
            return (NULL);
        }

        return (sp);
    }

    if (msg[handle].type != MSG_STRING)
    {
        // error code
        sp = stpushi (-1, sp, sp_bottom);
        if (sp == NULL)
        {  // error
            printf ("msg_get_string: ERROR: stack corrupt!\n");
            return (NULL);
        }

        return (sp);
    }

    // return msg
    pthread_mutex_lock (&msg_lock);
    string_len = strlen_safe ((const char *) msg[handle].string, STRINGMOD_MAXSTRLEN);

    #if BOUNDSCHECK
    if (memory_bounds (stringaddr, string_len) != 0)
	{
        pthread_mutex_unlock (&msg_lock);
		printf ("msg_get_string: ERROR: dest string overflow!\n");
		return (NULL);
	}
	#endif

    strcpy ((char *) &data[stringaddr], (const char *) msg[handle].string);
    pthread_mutex_unlock (&msg_lock);

    sp = stpushi (0, sp, sp_bottom);
    if (sp == NULL)
    {// error
        printf ("msg_get_string: ERROR: stack corrupt!\n");
        return (NULL);
    }

    return (sp);
}

U1 *msg_delete (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 handle ALIGN = 0;

    sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("msg_delete: ERROR: stack corrupt!\n");
		return (NULL);
	}

    sp = stpushi (msg_remove (handle), sp, sp_bottom);
    if (sp == NULL)
    {// error
        printf ("msg_delete: ERROR: stack corrupt!\n");
        return (NULL);
    }

    return (sp);
}

U1 *msg_get_type (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    // return -1 on error or the message variable type
    S8 handle ALIGN = 0;
    S8 msg_type ALIGN = 0;

    sp = stpopi ((U1 *) &handle, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("msg_get_type: ERROR: stack corrupt!\n");
		return (NULL);
	}

    if (handle < 0 || handle >= msg_max)
    {
        sp = stpushi (-1, sp, sp_bottom);
        if (sp == NULL)
        {  // error
            printf ("msg_get_type: ERROR: stack corrupt!\n");
            return (NULL);
        }

        return (sp);
    }

    pthread_mutex_lock (&msg_lock);
    msg_type = msg[handle].type;
    pthread_mutex_unlock (&msg_lock);

    sp = stpushi (msg_type, sp, sp_bottom);
    if (sp == NULL)
    {// error
        printf ("msg_get_type: ERROR: stack corrupt!\n");
        return (NULL);
    }

    return (sp);
}

U1 *msg_free_mem (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 i ALIGN = 0;

    pthread_mutex_lock (&msg_lock);
    for (i = 0; i < msg_max; i++)
    {
        if (msg[i].string) free (msg[i].string);
    }

    free (msg);

    pthread_mutex_unlock (&msg_lock);
    return (sp);
}
