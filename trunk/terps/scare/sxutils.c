/* vi: set ts=2 shiftwidth=2 expandtab:
 *
 * Copyright (C) 2003-2008  Simon Baldwin and Mark J. Tilford
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

/*
 * Module notes:
 *
 * o ...
 */

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "scare.h"
#include "sxprotos.h"


/*
 * sx_trace()
 *
 * Debugging trace function; printf wrapper that writes to stdout.  Note that
 * this differs from sc_trace(), which writes to stderr.  We use stdout so
 * that trace output is synchronized to test expectation failure messages.
 */
void
sx_trace (const sc_char *format, ...)
{
  va_list ap;
  assert (format);

  va_start (ap, format);
  vfprintf (stdout, format, ap);
  va_end (ap);
}


/*
 * sx_error()
 * sx_fatal()
 *
 * Error reporting functions.  sx_error() prints a message and continues.
 * sx_fatal() prints a message, then calls abort().
 */
void
sx_error (const sc_char *format, ...)
{
  va_list ap;
  assert (format);

  fprintf (stderr, "sx: ");
  va_start (ap, format);
  vfprintf (stderr, format, ap);
  va_end (ap);
}

void
sx_fatal (const sc_char *format, ...)
{
  va_list ap;
  assert (format);

  fprintf (stderr, "sx: internal error: ");
  va_start (ap, format);
  vfprintf (stderr, format, ap);
  va_end (ap);
  fprintf (stderr, "sx: aborting...\n");
  abort ();
}


/* Unique non-heap address for zero size malloc() and realloc() requests. */
static void *sx_zero_allocation = &sx_zero_allocation;
 
/*
 * sx_malloc()
 * sx_realloc()
 * sx_free()
 *
 * Non-failing wrappers around malloc functions.  Newly allocated memory is
 * cleared to zero.  In ANSI/ISO C, zero byte allocations are implementation-
 * defined, so we have to take special care to get predictable behavior.
 */
void *
sx_malloc (size_t size)
{
  void *allocated;

  if (size == 0)
    return sx_zero_allocation;

  allocated = malloc (size);
  if (!allocated)
    sx_fatal ("sx_malloc: requested %lu bytes\n", (sc_uint) size);
  else if (allocated == sx_zero_allocation)
    sx_fatal ("sx_malloc: zero-byte allocation address returned\n");

  memset (allocated, 0, size);
  return allocated;
}

void *
sx_realloc (void *pointer, size_t size)
{
  void *allocated;

  if (size == 0)
    {
      sx_free (pointer);
      return sx_zero_allocation;
    }

  if (pointer == sx_zero_allocation)
    pointer = NULL;

  allocated = realloc (pointer, size);
  if (!allocated)
    sx_fatal ("sx_realloc: requested %lu bytes\n", (sc_uint) size);
  else if (allocated == sx_zero_allocation)
    sx_fatal ("sx_realloc: zero-byte allocation address returned\n");

  if (!pointer)
    memset (allocated, 0, size);
  return allocated;
}

void
sx_free (void *pointer)
{
  if (sx_zero_allocation != &sx_zero_allocation)
    sx_fatal ("sx_free: write to zero-byte allocation address detected\n");

  if (pointer && pointer != sx_zero_allocation)
    free (pointer);
}


/*
 * sx_fopen()
 *
 * Open a file for a given test name with the extension and mode supplied.
 * Returns NULL if unsuccessful.
 */
FILE *
sx_fopen (const sc_char *name, const sc_char *extension, const sc_char *mode)
{
  sc_char *filename;
  FILE *stream;
  assert (name && extension && mode);

  filename = sx_malloc (strlen (name) + strlen (extension) + 2);
  sprintf (filename, "%s.%s", name, extension);

  stream = fopen (filename, mode);

  sx_free (filename);
  return stream;
}


/* Miscellaneous general ascii constants. */
static const sc_char NUL = '\0';

/*
 * sx_isspace()
 * sx_isprint()
 *
 * Built in replacements for locale-sensitive libc ctype.h functions.
 */
static sc_bool
sx_isspace (sc_char character)
{
  static const sc_char *const WHITESPACE = "\t\n\v\f\r ";

  return character != NUL && strchr (WHITESPACE, character) != NULL;
}

static sc_bool
sx_isprint (sc_char character)
{
  static const sc_int MIN_PRINTABLE = ' ', MAX_PRINTABLE = '~';

  return character >= MIN_PRINTABLE && character <= MAX_PRINTABLE;
}


/*
 * sx_trim_string()
 *
 * Trim leading and trailing whitespace from a string.  Modifies the string
 * in place, and returns the string address for convenience.
 */
sc_char *
sx_trim_string (sc_char *string)
{
  sc_int index_;
  assert (string);

  for (index_ = strlen (string) - 1;
       index_ >= 0 && sx_isspace (string[index_]); index_--)
    string[index_] = NUL;

  for (index_ = 0; sx_isspace (string[index_]);)
    index_++;
  memmove (string, string + index_, strlen (string) - index_ + 1);

  return string;
}


/*
 * sx_normalize_string()
 *
 * Trim a string, set all runs of whitespace to a single space character,
 * and convert all non-printing characters to '?'.  Modifies the string in
 * place, and returns the string address for convenience.
 */
sc_char *
sx_normalize_string (sc_char *string)
{
  sc_int index_;
  assert (string);

  string = sx_trim_string (string);

  for (index_ = 0; string[index_] != NUL; index_++)
    {
      if (sx_isspace (string[index_]))
        {
          sc_int cursor;

          string[index_] = ' ';
          for (cursor = index_ + 1; sx_isspace (string[cursor]);)
            cursor++;
          memmove (string + index_ + 1,
                   string + cursor, strlen (string + cursor) + 1);
        }
      else if (!sx_isprint (string[index_]))
        string[index_] = '?';
    }

  return string;
}
