/*-
 * Copyright 2010-2011 Chris Spiegel.
 *
 * This file is part of Bocfel.
 *
 * Bocfel is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version
 * 2, as published by the Free Software Foundation.
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
#include <string.h>
#include <errno.h>
#include <limits.h>

#ifdef ZTERP_GLK
#include <glk.h>
#endif

#include "io.h"
#include "osdep.h"
#include "unicode.h"

#ifndef PATH_MAX
#define PATH_MAX	4096
#endif

int use_utf8_io;

/* Generally speaking, UNICODE_LINEFEED (10) is used as a newline.  GLK
 * requires this (GLK API 0.7.0 §2.2), and when Unicode is available, we
 * write characters out by hand even with stdio, so no translation can
 * be done.  However, when stdio is being used, Unicode is not
 * available, and the file usage will be for a transcript or
 * command-script, use '\n' as a newline so translation can be done;
 * this is the only case where streams are opened in text mode.
 *
 * zterp_io_stdio() and zterp_io_stdout() are considered text-mode if
 * Unicode is not available, binary otherwise.
 */
#define textmode(io)	(!use_utf8_io && ((io->mode) & (ZTERP_IO_TRANS | ZTERP_IO_INPUT)))

struct zterp_io
{
  enum type { IO_STDIO, IO_GLK } type;

  FILE *fp;
  int mode;
#ifdef ZTERP_GLK
  strid_t file;
#endif
};

/* GLK does not like you to be able to pass a full filename to
 * glk_fileref_create_by_name(); this means that GLK cannot be used to
 * open arbitrary files.  However, GLK is still required to prompt for
 * files, such as in a save game situation.  To allow zterp_io to work
 * for opening files both with and without a prompt, it will use stdio
 * when either GLK is not available, or when GLK is available but
 * prompting is not necessary.
 *
 * This is needed because the IFF parser is required for both opening
 * games (zblorb files) and for saving/restoring.  The former needs to
 * be able to access any file on the filesystem, and the latter needs to
 * prompt.  This is a headache.
 *
 * Prompting is assumed to be necessary if “filename” is NULL.
 */
zterp_io *zterp_io_open(const char *filename, int mode)
{
  zterp_io *io;
  char smode[] = "wb";

  io = malloc(sizeof *io);
  if(io == NULL) goto err;
  io->mode = mode;

  if     (mode & ZTERP_IO_RDONLY) smode[0] = 'r';
  else if(mode & ZTERP_IO_APPEND) smode[0] = 'a';

  if(textmode(io)) smode[1] = 0;

#ifdef ZTERP_GLK
  int usage = fileusage_BinaryMode, filemode;

  if     (mode & ZTERP_IO_SAVE)  usage |= fileusage_SavedGame;
  else if(mode & ZTERP_IO_TRANS) usage |= fileusage_Transcript;
  else if(mode & ZTERP_IO_INPUT) usage |= fileusage_InputRecord;
  else                           usage |= fileusage_Data;

  if     (mode & ZTERP_IO_RDONLY) filemode = filemode_Read;
  else if(mode & ZTERP_IO_WRONLY) filemode = filemode_Write;
  else if(mode & ZTERP_IO_APPEND) filemode = filemode_WriteAppend;

  else goto err;
#else
  const char *prompt;

  if     (mode & ZTERP_IO_SAVE)  prompt = "Enter filename for save game: ";
  else if(mode & ZTERP_IO_TRANS) prompt = "Enter filename for transcript: ";
  else if(mode & ZTERP_IO_INPUT) prompt = "Enter filename for command record: ";
  else                           prompt = "Enter filename for data: ";
#endif

  /* No need to prompt. */
  if(filename != NULL)
  {
    io->type = IO_STDIO;
    io->fp = fopen(filename, smode);
    if(io->fp == NULL) goto err;
  }
  /* Prompt. */
  else
  {
#ifdef ZTERP_GLK
    frefid_t ref;

    ref = glk_fileref_create_by_prompt(usage, filemode, 0);
    if(ref == NULL) goto err;

    io->type = IO_GLK;
    io->file = glk_stream_open_file(ref, filemode, 0);
    glk_fileref_destroy(ref);
    if(io->file == NULL) goto err;
#else
    char fn[PATH_MAX], *p;

    printf("\n%s", prompt);
    fflush(stdout);
    if(fgets(fn, sizeof fn, stdin) == NULL || fn[0] == '\n') goto err;
    p = strchr(fn, '\n');
    if(p != NULL) *p = 0;

    io->type = IO_STDIO;
    io->fp = fopen(fn, smode);
    if(io->fp == NULL) goto err;
#endif
  }

  return io;

err:
  free(io);

  return NULL;
}

/* The zterp_os_reopen_binary() calls attempt to reopen stdin/ stdout as
 * binary streams so that reading/writing UTF-8 doesn’t cause unwanted
 * translations.  The mode of ZTERP_IO_TRANS is set when Unicode is
 * unavailable as a way to signal that these are text streams.
 */
const zterp_io *zterp_io_stdin(void)
{
  static zterp_io io;

  if(io.fp == NULL)
  {
    io.type = IO_STDIO;
    io.mode = ZTERP_IO_RDONLY;
    if(use_utf8_io) zterp_os_reopen_binary(stdin);
    else            io.mode |= ZTERP_IO_TRANS;
    io.fp = stdin;
  }

  return &io;
}

const zterp_io *zterp_io_stdout(void)
{
  static zterp_io io;

  if(io.fp == NULL)
  {
    io.type = IO_STDIO;
    io.mode = ZTERP_IO_WRONLY;
    if(use_utf8_io) zterp_os_reopen_binary(stdout);
    else            io.mode |= ZTERP_IO_TRANS;
    io.fp = stdout;
  }

  return &io;
}

void zterp_io_close(zterp_io *io)
{
#ifdef ZTERP_GLK
  if(io->type == IO_GLK)
  {
    glk_stream_close(io->file, NULL);
  }
  else
#endif
  {
    fclose(io->fp);
  }

  free(io);
}

int zterp_io_seek(const zterp_io *io, long offset, int whence)
{
  /* To smooth over differences between GLK and standard I/O, don’t
   * allow seeking in append-only streams.
   */
  if(io->mode & ZTERP_IO_APPEND) return -1;

#ifdef ZTERP_GLK
  if(io->type == IO_GLK)
  {
    glk_stream_set_position(io->file, offset, whence == SEEK_SET ? seekmode_Start : whence == SEEK_CUR ? seekmode_Current : seekmode_End);
    return 0; /* dammit */
  }
  else
#endif
  {
    return fseek(io->fp, offset, whence);
  }
}

long zterp_io_tell(const zterp_io *io)
{
#ifdef ZTERP_GLK
  if(io->type == IO_GLK)
  {
    return glk_stream_get_position(io->file);
  }
  else
#endif
  {
    return ftell(io->fp);
  }
}

/* zterp_io_read() and zterp_io_write() always operate in terms of
 * bytes, whether or not Unicode is available.
 */
size_t zterp_io_read(const zterp_io *io, void *buf, size_t n)
{
#ifdef ZTERP_GLK
  if(io->type == IO_GLK)
  {
    glui32 s = glk_get_buffer_stream(io->file, buf, n);
    /* This should only happen if io->file is invalid. */
    if(s == (glui32)-1) s = 0;
    return s;
  }
  else
#endif
  {
    return fread(buf, 1, n, io->fp);
  }
}

size_t zterp_io_write(const zterp_io *io, const void *buf, size_t n)
{
#ifdef ZTERP_GLK
  if(io->type == IO_GLK)
  {
    glk_put_buffer_stream(io->file, (char *)buf, n);
    return n; /* dammit */
  }
  else
#endif
  {
    return fwrite(buf, 1, n, io->fp);
  }
}

int zterp_io_read16(const zterp_io *io, uint16_t *v)
{
  uint8_t buf[2];

  if(zterp_io_read(io, buf, sizeof buf) != sizeof buf) return 0;

  *v = (buf[0] << 8) | buf[1];

  return 1;
}

int zterp_io_read32(const zterp_io *io, uint32_t *v)
{
  uint8_t buf[4];

  if(zterp_io_read(io, buf, sizeof buf) != sizeof buf) return 0;

  *v = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];

  return 1;
}

/* Read a byte and make sure it’s part of a valid UTF-8 sequence. */
static int read_byte(const zterp_io *io, uint8_t *c)
{
  if(zterp_io_read(io, c, sizeof *c) != sizeof *c) return 0;
  if((*c & 0x80) != 0x80) return 0;

  return 1;
}

/* zterp_io_getc() and zterp_io_putc() are meant to operate in terms of
 * characters, not bytes.  That is, unlike C, bytes and characters are
 * not equivalent as far as Zterp’s I/O system is concerned.
 */

/* Read a UTF-8 character, returning it.
 * -1 is returned on EOF.
 *
 * If there is a problem reading the UTF-8 (either from an invalid
 * sequence or from a too-large value), a question mark is returned.
 *
 * If Unicode is not available, read a single byte (assumed to be
 * Latin-1).
 * If Unicode is not available, IO_STDIO is in use, and text mode is
 * set, do newline translation.  Text mode is likely to always be
 * set—this function really shouldn’t be used in binary mode.
 */
long zterp_io_getc(const zterp_io *io)
{
  long ret;

  if(!use_utf8_io)
  {
#ifdef ZTERP_GLK
    if(io->type == IO_GLK)
    {
      ret = glk_get_char_stream(io->file);
    }
    else
#endif
    {
      int c;

      c = getc(io->fp);
      if(c == EOF) ret = -1;
      else         ret = c;

      if(textmode(io) && c == '\n') ret = UNICODE_LINEFEED;
    }
  }
  else
  {
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

      if(!read_byte(io, &c)) return UNICODE_QUESTIONMARK;

      ret |= (c & 0x3f);
    }
    else if((c & 0xf0) == 0xe0) /* Three bytes. */
    {
      ret = (c & 0x0f) << 12;

      if(!read_byte(io, &c)) return UNICODE_QUESTIONMARK;

      ret |= ((c & 0x3f) << 6);

      if(!read_byte(io, &c)) return UNICODE_QUESTIONMARK;

      ret |= (c & 0x3f);
    }
    else if((c & 0xf8) == 0xf0) /* Four bytes. */
    {
      /* The Z-machine doesn’t support Unicode this large, but at
       * least try not to leave a partial character in the stream.
       */
      zterp_io_seek(io, 3, SEEK_CUR);

      ret = UNICODE_QUESTIONMARK;
    }
    else /* Invalid value. */
    {
      ret = UNICODE_QUESTIONMARK;
    }
  }

  if(ret > UINT16_MAX) ret = UNICODE_QUESTIONMARK;

  return ret;
}

/* Write a Unicode character as UTF-8.
 *
 * If Unicode is not available, write the value out as a single Latin-1
 * byte.  If it is too large for a byte, write out a question mark.
 *
 * If Unicode is not available, IO_STDIO is in use, and text mode is
 * set, do newline translation.
 *
 * Text mode is likely to always be set—this function really shouldn’t
 * be used in binary mode.
 */
void zterp_io_putc(const zterp_io *io, uint16_t c)
{
  if(!use_utf8_io)
  {
    if(c > UINT8_MAX) c = UNICODE_QUESTIONMARK;
#ifdef ZTERP_GLK
    if(io->type == IO_GLK)
    {
      glk_put_char_stream(io->file, c);
    }
    else
#endif
    {
      if(textmode(io) && c == UNICODE_LINEFEED) c = '\n';
      putc(c, io->fp);
    }
  }
  else
  {
    uint8_t hi = c >> 8, lo = c & 0xff;

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
}

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

long long zterp_io_filesize(const zterp_io *io)
{
  if(io->type == IO_STDIO && !textmode(io))
  {
    return zterp_os_filesize(io->fp);
  }
  else
  {
    return -1;
  }
}

void zterp_io_flush(const zterp_io *io)
{
  if(io == NULL || io->type != IO_STDIO || !(io->mode & (ZTERP_IO_WRONLY | ZTERP_IO_APPEND))) return;

  fflush(io->fp);
}
