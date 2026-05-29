U1 *msg_set_int64 (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
    S8 id ALIGN = 0;
    S8 message ALIGN = 0;
    S8 handle ALIGN = 0;

    // For demo here, not needed, as set atomatically by l1vm-cfunc!
    // args-start

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

    // args-end

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

    // Needed here, because there is an error stpushi above!!
    // return-start

    sp = stpushi (0, sp, sp_bottom);
    if (sp == NULL)
    {                 // error
        printf ("msg_set_int64: ERROR: stack corrupt!\n");
        return (NULL);
    }

    // return-end

    return (sp);
}
