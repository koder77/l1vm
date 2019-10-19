/*
 * This file time.c is part of L1vm.
 *
 * (c) Copyright Stefan Pietzonke (jay-t@gmx.net), 2018
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

// time functions ------------------------------------

U1 *time_date_to_string (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 strdestaddr ALIGN;
	S8 str_len ALIGN;
	U1 buf[11];
	U1 datestr[11];
	S8 bufsize ALIGN = 11;

	time_t secs;
	struct tm *tm;

	sp = stpopi ((U1 *) &str_len, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("time_date_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("time_date_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	time (&secs);
	tm = localtime (&secs);

	if (snprintf ((char *) buf, bufsize, "%i", tm->tm_mday) <= 0)
	{
		printf ("time_date_to_string: ERROR: conversion failed!\n");
		return (NULL);
	}
	strcpy ((char *) datestr, "");
	if (tm->tm_mday < 10)
	{
		strcat ((char *) datestr, "0");
	}
	strcat ((char *) datestr, (const char *) buf);
	strcat ((char *) datestr, ".");

	if (snprintf ((char *) buf, bufsize, "%i", tm->tm_mon + 1) <= 0)
	{
		printf ("time_date_to_string: ERROR: conversion failed!\n");
		return (NULL);
	}
	if (tm->tm_mon + 1 < 10)
	{
		strcat ((char *) datestr, "0");
	}
	strcat ((char *) datestr, (const char *) buf);
	strcat ((char *) datestr, ".");

	if (snprintf ((char *) buf, bufsize, "%i", tm->tm_year + 1900) <= 0)
	{
		printf ("time_date_to_string: ERROR: conversion failed!\n");
		return (NULL);
	}
	strcat ((char *) datestr, (const char *) buf);

	snprintf ((char *) &data[strdestaddr], str_len, "%s", (const char *) datestr);
	return (sp);
}

U1 *time_time_to_string (U1 *sp, U1 *sp_top, U1 *sp_bottom, U1 *data)
{
	S8 strdestaddr ALIGN;
	S8 str_len ALIGN;
	U1 buf[9];
	U1 timestr[9];
	S8 bufsize ALIGN = 9;

	time_t secs;
	struct tm *tm;

	sp = stpopi ((U1 *) &str_len, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("time_time_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	sp = stpopi ((U1 *) &strdestaddr, sp, sp_top);
	if (sp == NULL)
	{
		// error
		printf ("time_time_to_string: ERROR: stack corrupt!\n");
		return (NULL);
	}

	time (&secs);
	tm = localtime (&secs);

	strcpy ((char *) timestr, "");
	if (snprintf ((char *) buf, bufsize, "%i", tm->tm_hour) <= 0)
	{
		printf ("time_time_to_string: ERROR: conversion failed!\n");
		return (NULL);
	}

	if (tm->tm_hour < 10)
	{
		strcat ((char *) timestr, "0");
	}
	strcpy ((char *) timestr, (const char *) buf);
	strcat ((char *) timestr, ":");

	if (snprintf ((char *) buf, bufsize, "%i", tm->tm_min) <= 0)
	{
		printf ("time_time_to_string: ERROR: conversion failed!\n");
		return (NULL);
	}

	if (tm->tm_min < 10)
	{
		strcat ((char *) timestr, "0");
	}
	strcat ((char *) timestr, (const char *) buf);
	strcat ((char *) timestr, ":");

	if (snprintf ((char *) buf, bufsize, "%i", tm->tm_sec) <= 0)
	{
		printf ("time_time_to_string: ERROR: conversion failed!\n");
		return (NULL);
	}

	if (tm->tm_sec < 10)
	{
		strcat ((char *) timestr, "0");
	}
	strcat ((char *) timestr, (const char *) buf);

	snprintf ((char *) &data[strdestaddr], str_len, "%s", (const char *) timestr);

	return (sp);
}
