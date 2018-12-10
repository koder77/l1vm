// stack operations ---------------------------------------
// byte

U1 *stpushb (U1 data, U1 *sp, U1 *sp_bottom)
{
	if (sp >= sp_bottom)
	{
		sp--;

		*sp = data;
		return (sp);		// success
	}
	else
	{
		// fatal ERROR: stack pointer can't go below address ZERO!
		return (NULL);		// FAIL
	}
}

U1 *stpopb (U1 *data, U1 *sp, U1 *sp_top)
{
	if (sp == sp_top)
	{
		// nothing on stack!! can't pop!!
		return (NULL);		// FAIL
	}

	*data = *sp;

	sp++;
	return (sp);			// success
}


// string, NULL terminated

U1 *stpushstring (U1 *data, S8 data_len, U1 *sp, U1 *sp_bottom)
{
	S8 i ALIGN;

	if (sp >= sp_bottom + data_len)
	{
		for (i = data_len; i >= 0; i--)
		{
			sp--;
			*sp = data[i];
		}
		return (sp);
	}
	else
	{
		// fatal ERROR: stack pointer can't go below address ZERO!
		return (NULL);			// FAIL
	}
}

U1 *stpopstring (U1 *data, S8 data_len, U1 *sp, U1 *sp_top)
{
	S8 i ALIGN = 0;

	if (sp == sp_top)
	{
		// nothing on stack!! can't pop!!
		return (NULL);		// FAIL
	}

	while (*sp != 0)
	{
   		if (i < data_len)
   		{
	   		data[i] = *sp;
	   		i++;
	   		sp++;
	   		if (sp == sp_top)
	   		{
		   		return (NULL);
	   		}
   		}
	}
	data[i] = '\0';        // end of string
	return (sp);
}


// quadword

U1 *stpushi (S8 data ALIGN, U1 *sp, U1 *sp_bottom)
{
	U1 *bptr;

	if (sp >= sp_bottom + 8)
	{
		// set stack pointer to lower address

		bptr = (U1 *) &data;

		sp--;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp = *bptr;

		return (sp);			// success
	}
	else
	{
		// fatal ERROR: stack pointer can't go below address ZERO!
		return (NULL);			// FAIL
	}
}

U1 *stpopi (U1 *data, U1 *sp, U1 *sp_top)
{
	if (sp == sp_top)
	{
		// nothing on stack!! can't pop!!
		return (NULL);			// FAIL
	}

	data[7] = *sp++;
	data[6] = *sp++;
	data[5] = *sp++;
	data[4] = *sp++;
	data[3] = *sp++;
	data[2] = *sp++;
	data[1] = *sp++;
	data[0] = *sp++;

	return (sp);			// success
}

U1 *stpushd (F8 data, U1 *sp, U1 *sp_bottom)
{
	U1 *bptr;

	if (sp >= sp_bottom + 8)
	{
		// set stack pointer to lower address

		bptr = (U1 *) &data;

		sp--;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp-- = *bptr;
		bptr++;
		*sp = *bptr;

		return (sp);			// success
	}
	else
	{
		// fatal ERROR: stack pointer can't go below address ZERO!
		return (NULL);			// FAIL
	}
}

U1 *stpopd (U1 *data, U1 *sp, U1 *sp_top)
{
	if (sp == sp_top)
	{
		// nothing on stack!! can't pop!!
		return (NULL);			// FAIL
	}

	data[7] = *sp++;
	data[6] = *sp++;
	data[5] = *sp++;
	data[4] = *sp++;
	data[3] = *sp++;
	data[2] = *sp++;
	data[1] = *sp++;
	data[0] = *sp++;

	return (sp);			// success
}
