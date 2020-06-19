/*-
 * Copyright 2010-2016 Chris Spiegel.
 *
 * This file is part of Bocfel.
 *
 * Bocfel is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version
 * 2 or 3, as published by the Free Software Foundation.
 *
 * Bocfel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Bocfel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <limits.h>

#ifdef ZTERP_GLK
#include <glk.h>
#endif

#include "io.h"
#include "osdep.h"
#include "unicode.h"

#ifndef ZTERP_GLK
#define MAX_PATH	4096
#endif

struct zterp_io
{
  enum type
  {
    IO_STDIO,
#ifdef ZTERP_GLK
    IO_GLK,
#endif
  } type;

  union
  {
    FILE *stdio;
#ifdef ZTERP_GLK
    strid_t glk;
#endif
  } file;
  enum zterp_io_mode mode;
  enum zterp_io_purpose purpose;
};

/* Certain streams are intended for use in text mode: stdin/stdout,
 * transcripts, and command scripts.  It is reasonable for users to
 * expect newline translation to be properly handled in these cases,
 * even though UNICODE_LINEFEED (10), as required by Glk (Glk API 0.7.0
 * §2.2), is used internally.
 */
static bool textmode(const struct zterp_io *io)
{
  return io == zterp_io_stdin() ||
         io == zterp_io_stdout() ||
         io->purpose == ZTERP_IO_TRANS ||
         io->purpose == ZTERP_IO_INPUT;
}

/* Glk does not like you to be able to pass a full filename to
 * glk_fileref_create_by_name(); this means that Glk cannot be used to
 * open arbitrary files.  However, Glk is still required to prompt for
 * files, such as in a save game situation.  To allow zterp_io to work
 * for opening files both with and without a prompt, it will use stdio
 * when either Glk is not available, or when Glk is available but
 * prompting is not necessary.
 *
 * This is needed because the IFF parser is required for both opening
 * games (zblorb files) and for saving/restoring.  The former needs to
 * be able to access any file on the filesystem, and the latter needs to
 * prompt.  This is a headache.
 *
 * Prompting is assumed to be necessary if “filename” is NULL.
 */
zterp_io *zterp_io_open(const char *filename, enum zterp_io_mode mode, enum zterp_io_purpose purpose)
{
  zterp_io *io;
  char smode[] = "wb";

  io = malloc(sizeof *io);
  if(io == NULL) goto err;
  io->mode = mode;
  io->purpose = purpose;

  if     (mode == ZTERP_IO_RDONLY) smode[0] = 'r';
  else if(mode == ZTERP_IO_APPEND) smode[0] = 'a';

  if(textmode(io)) smode[1] = 0;

#ifdef ZTERP_GLK
  glui32 usage = fileusage_BinaryMode, filemode;

  if     (purpose == ZTERP_IO_DATA)  usage |= fileusage_Data;
  else if(purpose == ZTERP_IO_SAVE)  usage |= fileusage_SavedGame;
  else if(purpose == ZTERP_IO_TRANS) usage |= fileusage_Transcript;
  else if(purpose == ZTERP_IO_INPUT) usage |= fileusage_InputRecord;
  else goto err;

  if     (mode == ZTERP_IO_RDONLY) filemode = filemode_Read;
  else if(mode == ZTERP_IO_WRONLY) filemode = filemode_Write;
  else if(mode == ZTERP_IO_APPEND) filemode = filemode_WriteAppend;
  else goto err;
#else
  const char *prompt;

  if     (purpose == ZTERP_IO_DATA)  prompt = "Enter filename for data: ";
  else if(purpose == ZTERP_IO_SAVE)  prompt = "Enter filename for save game: ";
  else if(purpose == ZTERP_IO_TRANS) prompt = "Enter filename for transcript: ";
  else if(purpose == ZTERP_IO_INPUT) prompt = "Enter filename for command record: ";
  else goto err;
#endif

  /* No need to prompt. */
  if(filename != NULL)
  {
    io->type = IO_STDIO;
    io->file.stdio = fopen(filename, smode);
    if(io->file.stdio == NULL) goto err;
  }
  /* Prompt. */
  else
  {
#ifdef ZTERP_GLK
    frefid_t ref;

    ref = glk_fileref_create_by_prompt(usage, filemode, 0);
    if(ref == NULL) goto err;

    io->type = IO_GLK;
    io->file.glk = glk_stream_open_file(ref, filemode, 0);
    glk_fileref_destroy(ref);
    if(io->file.glk == NULL) goto err;
#else
    char fn[MAX_PATH], *p;

    printf("\n%s", prompt);
    fflush(stdout);
    if(fgets(fn, sizeof fn, stdin) == NULL || fn[0] == '\n') goto err;
    p = strchr(fn, '\n');
    if(p != NULL) *p = 0;

    io->type = IO_STDIO;
    io->file.stdio = fopen(fn, smode);
    if(io->file.stdio == NULL) goto err;
#endif
  }

  return io;

err:
  free(io);

  return NULL;
}

const zterp_io *zterp_io_stdin(void)
{
  static zterp_io io;

  if(io.file.stdio == NULL)
  {
    io.type = IO_STDIO;
    io.mode = ZTERP_IO_RDONLY;
    io.purpose = ZTERP_IO_INPUT;
    io.file.stdio = stdin;
  }

  return &io;
}

const zterp_io *zterp_io_stdout(void)
{
  static zterp_io io;

  if(io.file.stdio == NULL)
  {
    io.type = IO_STDIO;
    io.mode = ZTERP_IO_WRONLY;
    io.purpose = ZTERP_IO_TRANS;
    io.file.stdio = stdout;
  }

  return &io;
}

void zterp_io_close(zterp_io *io)
{
#ifdef ZTERP_GLK
  if(io->type == IO_GLK)
  {
    glk_stream_close(io->file.glk, NULL);
  }
  else
#endif
  {
    fclose(io->file.stdio);
  }

  free(io);
}

bool zterp_io_seek(const zterp_io *io, long offset, int whence)
{
  /* To smooth over differences between Glk and standard I/O, don’t
   * allow seeking in append-only streams.
   */
  if(io->mode == ZTERP_IO_APPEND) return false;

#ifdef ZTERP_GLK
  if(io->type == IO_GLK)
  {
    glk_stream_set_position(io->file.glk, offset, whence == SEEK_SET ? seekmode_Start : whence == SEEK_CUR ? seekmode_Current : seekmode_End);
    return true; /* glk_stream_set_position can’t signal failure */
  }
  else
#endif
  {
    return fseek(io->file.stdio, offset, whence) == 0;
  }
}

long zterp_io_tell(const zterp_io *io)
{
#ifdef ZTERP_GLK
  if(io->type == IO_GLK)
  {
    return glk_stream_get_position(io->file.glk);
  }
  else
#endif
  {
    return ftell(io->file.stdio);
  }
}

/* zterp_io_read() and zterp_io_write() always operate in terms of
 * bytes, not characters.
 */
size_t zterp_io_read(const zterp_io *io, void *buf, size_t n)
{
  size_t total = 0;

  while(total < n)
  {
    size_t s;
#ifdef ZTERP_GLK
    if(io->type == IO_GLK)
    {
      glui32 s32 = glk_get_buffer_stream(io->file.glk, buf, n - total);
      /* This should only happen if io->file.glk is invalid. */
      if(s32 == (glui32)-1) break;
      s = s32;
    }
    else
#endif
    {
      s = fread(buf, 1, n - total, io->file.stdio);
    }

    if(s == 0) break;
    total += s;
    buf = ((char *)buf) + s;
  }

  return total;
}

size_t zterp_io_write(const zterp_io *io, const void *buf, size_t n)
{
#ifdef ZTERP_GLK
  if(io->type == IO_GLK)
  {
    glk_put_buffer_stream(io->file.glk, (char *)buf, n);
    return n; /* glk_put_buffer_stream() can’t signal a short write */
  }
  else
#endif
  {
    size_t s, total = 0;

    while(total < n && (s = fwrite(buf, 1, n - total, io->file.stdio)) > 0)
    {
      total += s;
      buf = ((const char *)buf) + s;
    }

    return total;
  }
}

bool zterp_io_read16(const zterp_io *io, uint16_t *v)
{
  uint8_t buf[2];

  if(zterp_io_read(io, buf, sizeof buf) != sizeof buf) return false;

  *v = (buf[0] << 8) | buf[1];

  return true;
}

bool zterp_io_read32(const zterp_io *io, uint32_t *v)
{
  uint8_t buf[4];

  if(zterp_io_read(io, buf, sizeof buf) != sizeof buf) return false;

  *v = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];

  return true;
}

/* Read a byte and make sure it’s part of a valid UTF-8 sequence. */
static bool read_byte(const zterp_io *io, uint8_t *c)
{
  return (zterp_io_read(io, c, sizeof *c) == sizeof *c) &&
         ((*c & 0x80) == 0x80);
}

/* zterp_io_getc() and zterp_io_putc() are meant to operate in terms of
 * characters, not bytes.  That is, unlike C, bytes and characters are
 * not equivalent as far as Zterp’s I/O system is concerned.
 */

/* Read a UTF-8 character, returning it.
 * -1 is returned on EOF.
 *
 * If there is a problem reading the UTF-8 (either from an invalid
 * sequence or from a too-large value), the Unicode replacement
 * character is returned.
 */
long zterp_io_getc(const zterp_io *io)
{
  long ret;

  uint8_t c;

  if(zterp_io_read(io, &c, sizeof c) != sizeof c)
  {
    ret = -1;
  }
  else if((c & 0x80) == 0) /* One byte. */
  {
    ret = c;
  }
  else if((c & 0xe0) == 0xc0) /* Two bytes. */
  {
    ret = (c & 0x1f) << 6;

    if(!read_byte(io, &c)) return UNICODE_REPLACEMENT;

    ret |= (c & 0x3f);
  }
  else if((c & 0xf0) == 0xe0) /* Three bytes. */
  {
    ret = (c & 0x0f) << 12;

    if(!read_byte(io, &c)) return UNICODE_REPLACEMENT;

    ret |= ((c & 0x3f) << 6);

    if(!read_byte(io, &c)) return UNICODE_REPLACEMENT;

    ret |= (c & 0x3f);
  }
  else if((c & 0xf8) == 0xf0) /* Four bytes. */
  {
    /* The Z-machine doesn’t support Unicode this large, but at least
     * try not to leave a partial character in the stream.
     */
    zterp_io_seek(io, 3, SEEK_CUR);

    ret = UNICODE_REPLACEMENT;
  }
  else /* Invalid value. */
  {
    ret = UNICODE_REPLACEMENT;
  }

  if(ret > UINT16_MAX) ret = UNICODE_REPLACEMENT;

  if(textmode(io) && ret == '\n') ret = UNICODE_LINEFEED;

  return ret;
}

/* Write a Unicode character as UTF-8. */
void zterp_io_putc(const zterp_io *io, uint16_t c)
{
  uint8_t hi = c >> 8, lo = c & 0xff;

  if(textmode(io) && c == UNICODE_LINEFEED) c = '\n';

#define WRITE(c)	zterp_io_write(io, &(uint8_t){ c }, sizeof (uint8_t))
  if(c < 128)
  {
    WRITE(c);
  }
  else if(c < 2048)
  {
    WRITE(0xc0 | (hi << 2) | (lo >> 6));
    WRITE(0x80 | (lo & 0x3f));
  }
  else
  {
    WRITE(0xe0 | (hi >> 4));
    WRITE(0x80 | ((hi << 2) & 0x3f) | (lo >> 6));
    WRITE(0x80 | (lo & 0x3f));
  }
#undef WRITE
}

/* Read up to “len” characters, storing them in “buf”, and return the
 * number of characters read in this way.  Processing stops when a
 * newline is encountered (Unicode linefeed, i.e. 0x0a); the newline is
 * consumed but is *not* stored in “buf”, nor is it included in the
 * return count.
 *
 * If no newline is encountered before “len” characters are read, any
 * remaining characters in the line will remain for the next I/O
 * operation, much in the way that fgets() operates.  If EOF is
 * encountered at any point (including after characters have been read,
 * but before a newline), -1 is returned, which means that all lines,
 * including the last one, must end in a newline.  Any characters read
 * before the EOF can be considered lost and unrecoverable.
 */
long zterp_io_readline(const zterp_io *io, uint16_t *buf, size_t len)
{
  long ret;

  if(len > LONG_MAX) return -1;

  for(ret = 0; ret < len; ret++)
  {
    long c = zterp_io_getc(io);

    /* EOF before newline means there was a problem. */
    if(c == -1) return -1;

    /* Don’t count the newline. */
    if(c == UNICODE_LINEFEED) break;

    buf[ret] = c;
  }

  return ret;
}

long zterp_io_filesize(const zterp_io *io)
{
  if(io->type == IO_STDIO && !textmode(io))
  {
    return zterp_os_filesize(io->file.stdio);
  }
  else
  {
    return -1;
  }
}

void zterp_io_flush(const zterp_io *io)
{
  if(io == NULL || io->type != IO_STDIO || (io->mode != ZTERP_IO_WRONLY && io->mode != ZTERP_IO_APPEND)) return;

  fflush(io->file.stdio);
}
