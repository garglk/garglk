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

#include "scarier.h"
#include "sxprotos.h"


/*
 * sxr_trace()
 *
 * Debugging trace function; printf wrapper that writes to stdout.  Note that
 * this differs from scr_trace(), which writes to stderr.  We use stdout so
 * that trace output is synchronized to test expectation failure messages.
 */
void
sxr_trace (const scr_char *format, ...)
{
  va_list ap;
  assert (format);

  va_start (ap, format);
  vfprintf (stdout, format, ap);
  va_end (ap);
}


/*
 * sxr_error()
 * sxr_fatal()
 *
 * Error reporting functions.  sxr_error() prints a message and continues.
 * sxr_fatal() prints a message, then calls abort().
 */
void
sxr_error (const scr_char *format, ...)
{
  va_list ap;
  assert (format);

  fprintf (stderr, "sx: ");
  va_start (ap, format);
  vfprintf (stderr, format, ap);
  va_end (ap);
}

void
sxr_fatal (const scr_char *format, ...)
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
static void *sxr_zero_allocation = &sxr_zero_allocation;
 
/*
 * sxr_malloc()
 * sxr_realloc()
 * sxr_free()
 *
 * Non-failing wrappers around malloc functions.  Newly allocated memory is
 * cleared to zero.  In ANSI/ISO C, zero byte allocations are implementation-
 * defined, so we have to take special care to get predictable behavior.
 */
void *
sxr_malloc (size_t size)
{
  void *allocated;

  if (size == 0)
    return sxr_zero_allocation;

  allocated = (decltype(allocated)) malloc (size);
  if (!allocated)
    sxr_fatal ("sxr_malloc: requested %lu bytes\n", (scr_uint) size);
  else if (allocated == sxr_zero_allocation)
    sxr_fatal ("sxr_malloc: zero-byte allocation address returned\n");

  memset (allocated, 0, size);
  return allocated;
}

void *
sxr_realloc (void *pointer, size_t size)
{
  void *allocated;

  if (size == 0)
    {
      sxr_free (pointer);
      return sxr_zero_allocation;
    }

  if (pointer == sxr_zero_allocation)
    pointer = NULL;

  allocated = (decltype(allocated)) realloc (pointer, size);
  if (!allocated)
    sxr_fatal ("sxr_realloc: requested %lu bytes\n", (scr_uint) size);
  else if (allocated == sxr_zero_allocation)
    sxr_fatal ("sxr_realloc: zero-byte allocation address returned\n");

  if (!pointer)
    memset (allocated, 0, size);
  return allocated;
}

void
sxr_free (void *pointer)
{
  if (sxr_zero_allocation != &sxr_zero_allocation)
    sxr_fatal ("sxr_free: write to zero-byte allocation address detected\n");

  if (pointer && pointer != sxr_zero_allocation)
    free (pointer);
}


/*
 * sxr_fopen()
 *
 * Open a file for a given test name with the extension and mode supplied.
 * Returns NULL if unsuccessful.
 */
FILE *
sxr_fopen (const scr_char *name, const scr_char *extension, const scr_char *mode)
{
  scr_char *filename;
  FILE *stream;
  assert (name && extension && mode);

  const size_t size = strlen (name) + strlen (extension) + 2;
  filename = (decltype(filename)) sxr_malloc (size);
  snprintf (filename, size, "%s.%s", name, extension);

  stream = fopen (filename, mode);

  sxr_free (filename);
  return stream;
}


/* Miscellaneous general ascii constants. */
static const scr_char NUL = '\0';

/*
 * sxr_isspace()
 * sxr_isprint()
 *
 * Built in replacements for locale-sensitive libc ctype.h functions.
 */
static scr_bool
sxr_isspace (scr_char character)
{
  static const scr_char *const WHITESPACE = "\t\n\v\f\r ";

  return character != NUL && strchr (WHITESPACE, character) != NULL;
}

static scr_bool
sxr_isprint (scr_char character)
{
  static const scr_int MIN_PRINTABLE = ' ', MAX_PRINTABLE = '~';

  return character >= MIN_PRINTABLE && character <= MAX_PRINTABLE;
}


/*
 * sxr_trim_string()
 *
 * Trim leading and trailing whitespace from a string.  Modifies the string
 * in place, and returns the string address for convenience.
 */
scr_char *
sxr_trim_string (scr_char *string)
{
  scr_int index_;
  assert (string);

  for (index_ = strlen (string) - 1;
       index_ >= 0 && sxr_isspace (string[index_]); index_--)
    string[index_] = NUL;

  for (index_ = 0; sxr_isspace (string[index_]);)
    index_++;
  memmove (string, string + index_, strlen (string) - index_ + 1);

  return string;
}


/*
 * sxr_normalize_string()
 *
 * Trim a string, set all runs of whitespace to a single space character,
 * and convert all non-printing characters to '?'.  Modifies the string in
 * place, and returns the string address for convenience.
 */
scr_char *
sxr_normalize_string (scr_char *string)
{
  scr_int index_;
  assert (string);

  string = sxr_trim_string (string);

  for (index_ = 0; string[index_] != NUL; index_++)
    {
      if (sxr_isspace (string[index_]))
        {
          scr_int cursor;

          string[index_] = ' ';
          for (cursor = index_ + 1; sxr_isspace (string[cursor]);)
            cursor++;
          memmove (string + index_ + 1,
                   string + cursor, strlen (string + cursor) + 1);
        }
      else if (!sxr_isprint (string[index_]))
        string[index_] = '?';
    }

  return string;
}
