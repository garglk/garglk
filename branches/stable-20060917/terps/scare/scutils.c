/* vi: set ts=8:
 *
 * Copyright (C) 2003-2005  Simon Baldwin and Mark J. Tilford
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 * USA
 */

/*
 * Module notes:
 *
 * o Implement smarter selective module tracing.
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>

#include "scare.h"
#include "scprotos.h"


/* Assorted definitions and constants. */
static const sc_char	NUL			= '\0';

/*
 * sc_trace()
 *
 * Debugging trace function; printf wrapper that writes to stderr.
 */
void
sc_trace (const sc_char *format, ...)
{
	va_list		ap;
	va_start (ap, format);
	vfprintf (stderr, format, ap);
	va_end (ap);
}


/*
 * sc_error()
 * sc_fatal()
 *
 * Error reporting functions.  sc_error() prints a message and continues.
 * sc_fatal() prints a message, then calls abort().
 */
void
sc_error (const sc_char *format, ...)
{
	va_list		ap;
	va_start (ap, format);
	vfprintf (stderr, format, ap);
	va_end (ap);
}

void
sc_fatal (const sc_char *format, ...)
{
	va_list		ap;
	va_start (ap, format);
	vfprintf (stderr, format, ap);
	va_end (ap);
	abort ();
}


/*
 * sc_malloc()
 * sc_realloc()
 * sc_free()
 *
 * Non-failing wrappers around malloc functions.
 */
void *
sc_malloc (size_t size)
{
	void	*ptr;
	ptr = malloc (size);
	if (ptr == NULL)
		sc_fatal ("malloc error,"
				" requested %lu bytes\n", (sc_uint32)size);
	return ptr;
}

void *
sc_realloc (void *pointer, size_t size)
{
	void	*ptr;
	ptr = realloc (pointer, size);
	if (ptr == NULL)
		sc_fatal ("realloc error,"
				" requested %lu bytes\n", (sc_uint32)size);
	return ptr;
}

void
sc_free (void *ptr)
{
	free (ptr);
}


/*
 * sc_strncasecmp()
 * sc_strcasecmp()
 *
 * Strncasecmp and strcasecmp are not ANSI functions, so here are local
 * definitions to do the same jobs.
 */
sc_int32
sc_strncasecmp (const sc_char *s1, const sc_char *s2, sc_int32 n)
{
	sc_int32	index;			/* Strings iterator. */

	/* Compare each character, return +/- 1 if not equal. */
	for (index = 0; index < n; index++)
	    {
		if (tolower (s1[index])
				< tolower (s2[index]))
			return -1;
		else if (tolower (s1[index])
				> tolower (s2[index]))
			return  1;
	    }

	/* Strings are identical to n characters. */
	return 0;
}

sc_int32
sc_strcasecmp (const sc_char *s1, const sc_char *s2)
{
	sc_int32	s1len, s2len;		/* String lengths. */
	sc_int32	result;			/* Result of strncasecmp. */

	/* Note the string lengths. */
	s1len = strlen (s1);
	s2len = strlen (s2);

	/* Compare first to shortest length, return any definitive result. */
	result = sc_strncasecmp (s1, s2, (s1len < s2len) ? s1len : s2len);
	if (result != 0)
		return result;

	/* Return the longer string as being the greater. */
	if (s1len < s2len)
		return -1;
	else if (s1len > s2len)
		return  1;

	/* Same length, and same characters -- identical strings. */
	return 0;
}


/*
 * sc_rand()
 * sc_randomint()
 *
 * Self-seeding rand(), and commonly used function to return a random
 * integer between two limits.
 */
sc_int32
sc_rand (void)
{
	static sc_bool		initialized = FALSE;

	/* On first call, seed random number generator. */
	if (!initialized)
	    {
		srand ((sc_uint32) time (NULL));
		initialized = TRUE;
	    }

	/* Return a normal number from rand(). */
	return rand ();
}

sc_int32
sc_randomint (sc_int32 low, sc_int32 high)
{
	return low + sc_rand () % (high - low + 1);
}


/*
 * sc_strempty()
 *
 * Small utility to return TRUE if a string is either zero-length, or
 * composed purely of whitespace.
 */
sc_bool
sc_strempty (const sc_char *string)
{
	sc_uint32	index;
	assert (string != NULL);

	/* Scan for any non-space character. */
	for (index = 0; string[index] != NUL; index++)
	    {
		if (!isspace (string[index]))
			return FALSE;
	    }

	/* None found, so string is empty. */
	return TRUE;
}


/*
 * sc_hash()
 *
 * Hash a string, hashpjw algorithm, from 'Compilers, principles, techniques,
 * and tools', page 436, unmodulo'ed.
 */
sc_uint32
sc_hash (const sc_char *name)
{
	const sc_char		*sp;
	sc_uint32		h = 0, g;

	for (sp = name; *sp != NUL; sp++)
	    {
		h = (h << 4) + (*sp);
		if ((g = (h & 0xF0000000)) != 0)
		    {
			h = h ^ (g >> 24);
			h = h ^ g;
		    }
	    }
	return h;
}
