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

#include <setjmp.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
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

struct backing
{
  uint8_t *memory;
  long allocated;
  long size;
  long offset;
};

struct zterp_io
{
  enum type
  {
    IO_STDIO,
    IO_MEMORY,
#ifdef ZTERP_GLK
    IO_GLK,
#endif
  } type;

  union
  {
    FILE *stdio;
    struct backing backing;
#ifdef ZTERP_GLK
    strid_t glk;
#endif
  } file;
  enum zterp_io_mode mode;
  enum zterp_io_purpose purpose;

  bool exception_mode;
  jmp_buf exception;
};

/* Due to C’s less-than-ideal type system, there’s no way to guarantee
 * that an enum actually contains a valid value. When checking the value
 * of the I/O object’s type, this function is used as a sort-of run-time
 * type checker.
 */
znoreturn
static void bad_type(enum type type)
{
  die("internal error: unknown IO type %d", type);
}

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
  io->exception_mode = false;

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

/* Instead of being file-backed, indicate that this I/O object is
 * memory-backed. This allows internal save states (for @save_undo as
 * well as meta-saves created by /ps) to use Quetzal as their save
 * format. This is helpful because meta-saves have to track extra
 * information, and reusing the Quetzal code (plus extensions)
 * eliminates the need for code duplication.
 *
 * If “buf” is null, this is a write-only memory object that will
 * allocate space as it is required. Otherwise, this is a read-only
 * object backed by the passed-in buffer. A copy is made so the caller
 * is free to do what it likes with the memory afterward.
 */
zterp_io *zterp_io_open_memory(const void *buf, size_t n)
{
  struct zterp_io *io;

  io = malloc(sizeof *io);
  if(io == NULL) return NULL;

  io->type = IO_MEMORY;
  io->purpose = ZTERP_IO_DATA;
  io->exception_mode = false;

  if(buf != NULL)
  {
    io->mode = ZTERP_IO_RDONLY;
    io->file.backing.memory = malloc(n);
    if(io->file.backing.memory == NULL)
    {
      free(io);
      return NULL;
    }

    memcpy(io->file.backing.memory, buf, n);

    io->file.backing.allocated = 0;
    io->file.backing.size = n;
    io->file.backing.offset = 0;
  }
  else
  {
    io->mode = ZTERP_IO_WRONLY;
    io->file.backing.allocated = 32768;
    io->file.backing.memory = malloc(io->file.backing.allocated);
    if(io->file.backing.memory == NULL)
    {
      free(io);
      return NULL;
    }
    io->file.backing.size = 0;
    io->file.backing.offset = 0;
  }

  return io;
}

zterp_io *zterp_io_stdin(void)
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

zterp_io *zterp_io_stdout(void)
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
  switch(io->type)
  {
    case IO_STDIO:
      fclose(io->file.stdio);
      break;
    case IO_MEMORY:
      free(io->file.backing.memory);
      break;
#ifdef ZTERP_GLK
    case IO_GLK:
      glk_stream_close(io->file.glk, NULL);
      break;
#endif
  }

  free(io);
}

/* Close a memory-backed I/O object (this returns false for any other
 * type of I/O object). Store the buffer and size in “buf” and “n”. The
 * caller is responsible for the memory and must free it using free().
 */
bool zterp_io_close_memory(zterp_io *io, uint8_t **buf, long *n)
{
  uint8_t *tmp;

  if(io->type != IO_MEMORY) return false;

  tmp = realloc(io->file.backing.memory, io->file.backing.size);
  if(tmp == NULL) *buf = io->file.backing.memory;
  else            *buf = tmp;

  *n = io->file.backing.size;

  free(io);

  return true;
}

/* Turn on “exception handling” for this I/O object. After calling this
 * function, all future I/O calls which return a bool will now instead
 * cause this function to return false in case of error. Simple usage:
 *
 * if(!zterp_io_try(io)) return ERROR;
 * // call I/O functions here
 *
 * This *only* affects boolean functions. Those which return non-boolean
 * values, such as zterp_io_write() and zterp_io_getc() are unaffected.
 * This may be inconsistent, at least for zterp_io_getc(), which can
 * return a failure condition, but non-boolean functions are not
 * currently used with exception handling, so it’s not relevant.
 */
bool zterp_io_try(zterp_io *io)
{
  io->exception_mode = true;

  if(setjmp(io->exception) != 0) return false;

  return true;
}

static bool wrap(zterp_io *io, bool b)
{
  if(io->exception_mode && !b) longjmp(io->exception, 1);

  return b;
}

bool zterp_io_seek(zterp_io *io, long offset, int whence)
{
  /* To smooth over differences between Glk and standard I/O, don’t
   * allow seeking in append-only streams.
   */
  if(io->mode == ZTERP_IO_APPEND) return wrap(io, false);

  switch(io->type)
  {
    case IO_STDIO:
      return wrap(io, fseek(io->file.stdio, offset, whence) == 0);
    case IO_MEMORY:
      /* Negative offsets are unsupported because they aren’t used. */
      if(offset < 0) return wrap(io, false);

      switch(whence)
      {
        case SEEK_CUR:
          /* Overflow. */
          if(LONG_MAX - offset < io->file.backing.offset) return wrap(io, false);

          offset = io->file.backing.offset + offset;
        case SEEK_SET:
          /* Don’t allow seeking beyond the end. */
          if(offset > io->file.backing.size) return wrap(io, false);

          io->file.backing.offset = offset;

          return wrap(io, true);
        default:
          /* No support for SEEK_END because it’s not used. */
          return wrap(io, false);
      }
#ifdef ZTERP_GLK
    case IO_GLK:
      glk_stream_set_position(io->file.glk, offset, whence == SEEK_SET ? seekmode_Start : whence == SEEK_CUR ? seekmode_Current : seekmode_End);
      return wrap(io, true); /* glk_stream_set_position can’t signal failure */
#endif
    default:
      bad_type(io->type);
  }
}

long zterp_io_tell(zterp_io *io)
{
  switch(io->type)
  {
    case IO_STDIO:
      return ftell(io->file.stdio);
    case IO_MEMORY:
      return io->file.backing.offset;
#ifdef ZTERP_GLK
    case IO_GLK:
      return glk_stream_get_position(io->file.glk);
#endif
    default:
      bad_type(io->type);
  }
}

/* zterp_io_read() and zterp_io_write() always operate in terms of
 * bytes, not characters.
 */
size_t zterp_io_read(zterp_io *io, void *buf, size_t n)
{
  size_t total = 0;

  while(total < n)
  {
    size_t s;
    if(io->type == IO_STDIO)
    {
      s = fread(buf, 1, n - total, io->file.stdio);
    }
    else if(io->type == IO_MEMORY)
    {
      struct backing *b = &io->file.backing;
      long remaining = b->size - b->offset;

      if(io->mode != ZTERP_IO_RDONLY) return 0;

      s = remaining < n ? remaining : n;
      memcpy(buf, &b->memory[b->offset], s);

      b->offset += s;
    }
#ifdef ZTERP_GLK
    else if(io->type == IO_GLK)
    {
      glui32 s32 = glk_get_buffer_stream(io->file.glk, buf, n - total);
      /* This should only happen if io->file.glk is invalid. */
      if(s32 == (glui32)-1) break;
      s = s32;
    }
#endif
    else
    {
      bad_type(io->type);
    }

    if(s == 0) break;
    total += s;
    buf = ((char *)buf) + s;
  }

  return total;
}

bool zterp_io_read_exact(zterp_io *io, void *buf, size_t n)
{
  return wrap(io, zterp_io_read(io, buf, n) == n);
}

size_t zterp_io_write(zterp_io *io, const void *buf, size_t n)
{
  switch(io->type)
  {
    case IO_STDIO:
      {
        size_t s, total = 0;

        while(total < n && (s = fwrite(buf, 1, n - total, io->file.stdio)) > 0)
        {
          total += s;
          buf = ((const char *)buf) + s;
        }

        return total;
      }
    case IO_MEMORY:
      {
        struct backing *b = &io->file.backing;
        long remaining = b->size - b->offset;

        if(io->mode != ZTERP_IO_WRONLY) return 0;

        if(n > remaining)
        {
          b->size += n - remaining;
          if(b->size > b->allocated)
          {
            long grow = b->size - b->allocated;
            uint8_t *tmp;

            if(grow < 32768) grow = 32768;
            b->allocated += grow;
            tmp = realloc(b->memory, b->allocated);
            if(tmp == NULL)
            {
              b->size -= n - remaining;
              b->allocated -= grow;
              return 0;
            }
            b->memory = tmp;
          }
        }

        memcpy(&b->memory[b->offset], buf, n);

        b->offset += n;

        return n;
      }
#ifdef ZTERP_GLK
    case IO_GLK:
      glk_put_buffer_stream(io->file.glk, (char *)buf, n);
      return n; /* glk_put_buffer_stream() can’t signal a short write */
#endif
    default:
      bad_type(io->type);
  }
}

bool zterp_io_write_exact(zterp_io *io, const void *buf, size_t n)
{
  return wrap(io, zterp_io_write(io, buf, n) == n);
}

bool zterp_io_read8(zterp_io *io, uint8_t *v)
{
  return zterp_io_read_exact(io, v, sizeof *v);
}

bool zterp_io_read16(zterp_io *io, uint16_t *v)
{
  uint8_t buf[2];

  if(!zterp_io_read_exact(io, buf, sizeof buf)) return wrap(io, false);

  *v = (buf[0] << 8) | buf[1];

  return wrap(io, true);
}

bool zterp_io_read32(zterp_io *io, uint32_t *v)
{
  uint8_t buf[4];

  if(!zterp_io_read_exact(io, buf, sizeof buf)) return wrap(io, false);

  *v = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];

  return wrap(io, true);
}

bool zterp_io_write8(zterp_io *io, uint8_t v)
{
  return zterp_io_write_exact(io, &v, sizeof v);
}

bool zterp_io_write16(zterp_io *io, uint16_t v)
{
  uint8_t buf[2];

  buf[0] = v >> 8;
  buf[1] = v & 0xff;

  return zterp_io_write_exact(io, buf, sizeof buf);
}

bool zterp_io_write32(zterp_io *io, uint32_t v)
{
  uint8_t buf[4];

  buf[0] = (v >> 24) & 0xff;
  buf[1] = (v >> 16) & 0xff;
  buf[2] = (v >>  8) & 0xff;
  buf[3] = (v >>  0) & 0xff;

  return zterp_io_write_exact(io, buf, sizeof buf);
}

/* Read a byte and make sure it’s part of a valid UTF-8 sequence. */
#define READ_BYTE(c)	do { \
  if(zterp_io_read(io, (c), sizeof *(c)) != sizeof *(c)) return -1; \
  if((*(c) & 0x80) != 0x80) return UNICODE_REPLACEMENT; \
} while(false)

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
long zterp_io_getc(zterp_io *io)
{
  long ret;
  uint8_t c;

  if(!zterp_io_read_exact(io, &c, sizeof c)) return -1;

  if((c & 0x80) == 0) /* One byte. */
  {
    ret = c;
  }
  else if((c & 0xe0) == 0xc0) /* Two bytes. */
  {
    ret = (c & 0x1f) << 6;

    READ_BYTE(&c);

    ret |= (c & 0x3f);
  }
  else if((c & 0xf0) == 0xe0) /* Three bytes. */
  {
    ret = (c & 0x0f) << 12;

    READ_BYTE(&c);

    ret |= ((c & 0x3f) << 6);

    READ_BYTE(&c);

    ret |= (c & 0x3f);
  }
  else if((c & 0xf8) == 0xf0) /* Four bytes. */
  {
    /* The Z-machine doesn’t support Unicode this large, but at least
     * try not to leave a partial character in the stream.
     */
    for(int i = 0; i < 3; i++)
    {
      READ_BYTE(&c);
    }

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
void zterp_io_putc(zterp_io *io, uint16_t c)
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
long zterp_io_readline(zterp_io *io, uint16_t *buf, size_t len)
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

long zterp_io_filesize(zterp_io *io)
{
  switch(io->type)
  {
    case IO_STDIO:
      if(!textmode(io)) return zterp_os_filesize(io->file.stdio);
      break;
    case IO_MEMORY:
      return io->file.backing.size;
#ifdef ZTERP_GLK
    case IO_GLK:
      break;
#endif
  }

  return -1;
}

void zterp_io_flush(zterp_io *io)
{
  if(io != NULL && io->type == IO_STDIO && (io->mode == ZTERP_IO_WRONLY || io->mode == ZTERP_IO_APPEND))
  {
    fflush(io->file.stdio);
  }
}
