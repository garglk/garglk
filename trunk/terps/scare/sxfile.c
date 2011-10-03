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
#include <stdlib.h>
#include <string.h>

#include "scare.h"
#include "sxprotos.h"


/*
 * Structure for representing a fake game save/restore file.  Used to catch
 * a serialized gamestate, and return it later on restore.  For now we allow
 * only one of these to exist.
 */
typedef struct
{
  sc_byte *data;
  sc_int length;
  sc_bool is_open;
  sc_bool is_writable;
} sx_scr_stream_t;
static sx_scr_stream_t scr_serialization_stream = {NULL, 0, FALSE, FALSE};


/*
 * file_open_file_callback()
 * file_read_file_callback()
 * file_write_file_callback()
 * file_close_file_callback()
 *
 * Fake a single gamestate save/restore file.  Used to satisfy requests from
 * the script to serialize and restore a gamestate.  Only one "file" can
 * exist, meaning that a script must restore a saved game before trying to
 * save another.
 */
void *
file_open_file_callback (sc_bool is_save)
{
  sx_scr_stream_t *const stream = &scr_serialization_stream;

  /* Detect any problems due to scripting limitations. */
  if (stream->is_open)
    {
      scr_test_failed ("File open error: %s",
                       "stream is in use (script limitation)");
      return NULL;
    }
  else if (is_save && stream->data)
    {
      scr_test_failed ("File open error: %s",
                       "stream has not been read (script limitation)");
      return NULL;
    }

  /*
   * Set up the stream for the requested mode.  Act as if no such file if
   * no data available for a read-only open.
   */
  if (is_save)
    {
      stream->data = NULL;
      stream->length = 0;
    }
  else if (!stream->data)
    return NULL;

  stream->is_open = TRUE;
  stream->is_writable = is_save;
  return stream;
}

sc_int
file_read_file_callback (void *opaque, sc_byte *buffer, sc_int length)
{
  sx_scr_stream_t *const stream = opaque;
  sc_int bytes;
  assert (opaque && buffer && length > 0);

  /* Detect any problems with the callback parameters. */
  if (stream != &scr_serialization_stream)
    {
      scr_test_failed ("File read error: %s", "stream is invalid");
      return 0;
    }
  else if (!stream->is_open)
    {
      scr_test_failed ("File read error: %s", "stream is not open");
      return 0;
    }
  else if (stream->is_writable)
    {
      scr_test_failed ("File read error: %s", "stream is not open for read");
      return 0;
    }

  /* Read and remove the first block of data (or all if less than length). */
  bytes = (stream->length < length) ? stream->length : length;
  memcpy (buffer, stream->data, bytes);
  memmove (stream->data, stream->data + bytes, stream->length - bytes);
  stream->length -= bytes;
  return bytes;
}

void
file_write_file_callback (void *opaque, const sc_byte *buffer, sc_int length)
{
  sx_scr_stream_t *const stream = opaque;
  assert (opaque && buffer && length > 0);

  /* Detect any problems with the callback parameters. */
  if (stream != &scr_serialization_stream)
    {
      scr_test_failed ("File write error: %s", "stream is invalid");
      return;
    }
  else if (!stream->is_open)
    {
      scr_test_failed ("File write error: %s", "stream is not open");
      return;
    }
  else if (!stream->is_writable)
    {
      scr_test_failed ("File write error: %s", "stream is not open for write");
      return;
    }

  /* Reallocate, then add this block of data to the buffer. */
  stream->data = sx_realloc (stream->data, stream->length + length);
  memcpy (stream->data + stream->length, buffer, length);
  stream->length += length;
}

void
file_close_file_callback (void *opaque)
{
  sx_scr_stream_t *const stream = opaque;
  assert (opaque);

  /* Detect any problems with the callback parameters. */
  if (stream != &scr_serialization_stream)
    {
      scr_test_failed ("File close error: %s", "stream is invalid");
      return;
    }
  else if (!stream->is_open)
    {
      scr_test_failed ("File close error: %s", "stream is not open");
      return;
    }

  /*
   * If closing after a read, free allocations, and return the stream to
   * its empty state; if after write, leave the data for the later read.
   */
  if (!stream->is_writable)
    {
      sx_free (stream->data);
      stream->data = NULL;
      stream->length = 0;
    }
  stream->is_writable = FALSE;
  stream->is_open = FALSE;
}


/*
 * file_cleanup()
 *
 * Free any pending allocations and clean up on completion of a script.
 */
void
file_cleanup (void)
{
  sx_scr_stream_t *const stream = &scr_serialization_stream;

  sx_free (stream->data);
  stream->data = NULL;
  stream->length = 0;
  stream->is_writable = FALSE;
  stream->is_open = FALSE;
}
