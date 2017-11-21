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
