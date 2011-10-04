/******************************************************************************
 *                                                                            *
 * Copyright (C) 2006-2009 by Tor Andersson, Jesse McGrew.                    *
 * Copyright (C) 2010 by Ben Cressey, Andrew Plotkin, Jörg Walter.            *
 *                                                                            *
 * This file is part of Gargoyle.                                             *
 *                                                                            *
 * Gargoyle is free software; you can redistribute it and/or modify           *
 * it under the terms of the GNU General Public License as published by       *
 * the Free Software Foundation; either version 2 of the License, or          *
 * (at your option) any later version.                                        *
 *                                                                            *
 * Gargoyle is distributed in the hope that it will be useful,                *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of             *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the              *
 * GNU General Public License for more details.                               *
 *                                                                            *
 * You should have received a copy of the GNU General Public License          *
 * along with Gargoyle; if not, write to the Free Software                    *
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA *
 *                                                                            *
 *****************************************************************************/

/* cgstream.c: Stream functions for Glk API, version 0.7.0.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html

    Portions of this file are copyright 1998-2007 by Andrew Plotkin.
    You may copy, distribute, and incorporate it into your own programs,
    by any means and under any conditions, as long as you do not modify it.
    You may also modify this file, incorporate it into your own programs,
    and distribute the modified version, as long as you retain a notice
    in your program or documentation which mentions my name and the URL
    shown above.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "glk.h"
#include "garglk.h"

/* This implements pretty much what any Glk implementation needs for 
    stream stuff. Memory streams, file streams (using stdio functions), 
    and window streams (which print through window functions in other
    files.) A different implementation would change the window stream
    stuff, but not file or memory streams. (Unless you're on a 
    wacky platform like the Mac and want to change stdio to native file 
    functions.) 
*/

static stream_t *gli_streamlist = NULL;
static stream_t *gli_currentstr = NULL;

extern void gli_stream_close(stream_t *str);

stream_t *gli_new_stream(glui32 type, int readable, int writable, glui32 rock, int unicode)
{
  stream_t *str = (stream_t *)malloc(sizeof(stream_t));

  if (!str)
    return NULL;

  str->type = type;
  str->unicode = unicode;

  str->rock = rock;
  str->readcount = 0;
  str->writecount = 0;
  str->readable = readable;
  str->writable = writable;

  /* start everything up empty, for now */
  str->buf = NULL;
  str->bufptr = NULL;
  str->bufend = NULL;
  str->bufeof = NULL;
  str->buflen = 0;
  str->win = NULL;
  str->lastop = 0;
  str->file = NULL;
  str->textfile = FALSE;

  str->prev = NULL;
  str->next = gli_streamlist;
  gli_streamlist = str;
  if (str->next)
    str->next->prev = str;

  if (gli_register_obj)
    str->disprock = (*gli_register_obj)(str, gidisp_Class_Stream);
  else
    str->disprock.ptr = NULL;

  return str;
}

void gli_delete_stream(stream_t *str)
{
  stream_t *prev, *next;

  if (gli_unregister_obj)
    (*gli_unregister_obj)(str, gidisp_Class_Stream, str->disprock);

  str->type = -1;
  str->readcount = 0;
  str->writecount = 0;
  str->win = NULL;
  str->buf = NULL;
  str->bufptr = NULL;
  str->bufend = NULL;
  str->bufeof = NULL;
  str->buflen = 0;

  prev = str->prev;
  next = str->next;
  str->prev = NULL;
  str->next = NULL;

  if (prev)
    prev->next = next;
  else
    gli_streamlist = next;
  if (next)
    next->prev = prev;

  free(str);
}

stream_t *glk_stream_iterate(stream_t *str, glui32 *rock)
{
  if (!str)
    str = gli_streamlist;
  else
    str = str->next;

  if (str)
  {
    if (rock)
      *rock = str->rock;
    return str;
  }

  if (rock)
    *rock = 0;
  return NULL;
}

glui32 glk_stream_get_rock(stream_t *str)
{
  if (!str)
  {
    gli_strict_warning("stream_get_rock: invalid ref");
    return 0;
  }
  return str->rock;
}

static stream_t *gli_stream_open_file(frefid_t fref, glui32 fmode,
    glui32 rock, int unicode)
{
  char modestr[16];
  char msg[256];
  stream_t *str;
  FILE *fl;

  if (!fref)
  {
    gli_strict_warning("stream_open_file: invalid fileref id");
    return 0;
  }

  /* The spec says that Write, ReadWrite, and WriteAppend create the
  file if necessary. However, fopen(filename, "r+") doesn't create
  a file. So we have to pre-create it in the ReadWrite and
  WriteAppend cases. (We use "a" so as not to truncate, and "b" 
  because we're going to close it immediately, so it doesn't matter.) */

  /* Another Unix quirk: in r+ mode, you're not supposed to flip from
  reading to writing or vice versa without doing an fseek. We will
  track the most recent operation (as lastop) -- Write, Read, or
  0 if either is legal next. */

  if (fmode == filemode_ReadWrite || fmode == filemode_WriteAppend)
  {
    fl = fopen(fref->filename, "ab");
    if (!fl)
    {
      sprintf(msg, "stream_open_file: unable to open file (ab): %s", fref->filename);
      gli_strict_warning(msg);
      return 0;
    }
    fclose(fl);
  }

  switch (fmode)
  {
      case filemode_Write:
          strcpy(modestr, "w");
          break;
      case filemode_Read:
          strcpy(modestr, "r");
          break;
      case filemode_ReadWrite:
          strcpy(modestr, "r+");
          break;
      case filemode_WriteAppend:
          /* Can't use "a" here, because then fseek wouldn't work.
             Instead we use "r+" and then fseek to the end. */
          strcpy(modestr, "r+");
          break;
  }

  if (!fref->textmode)
    strcat(modestr, "b");

  fl = fopen(fref->filename, modestr);
  if (!fl)
  {
    sprintf(msg, "stream_open_file: unable to open file (%s): %s", modestr, fref->filename);
    gli_strict_warning(msg);
    return 0;
  }

  if (fmode == filemode_WriteAppend)
    fseek(fl, 0, 2); /* ...to the end. */

  str = gli_new_stream(strtype_File, 
    (fmode == filemode_Read || fmode == filemode_ReadWrite), 
    !(fmode == filemode_Read), 
    rock,
    unicode);
  if (!str)
  {
    gli_strict_warning("stream_open_file: unable to create stream.");
    fclose(fl);
    return 0;
  }

  str->file = fl;
  str->lastop = 0;
  str->textfile = fref->textmode;

  return str;
}

stream_t *glk_stream_open_file(frefid_t fref, glui32 fmode, glui32 rock)
{
    return gli_stream_open_file(fref, fmode, rock, FALSE);
}

stream_t *glk_stream_open_file_uni(frefid_t fref, glui32 fmode, glui32 rock)
{
    return gli_stream_open_file(fref, fmode, rock, TRUE);
}

stream_t *gli_stream_open_pathname(char *pathname, int textmode, glui32 rock)
{
  char modestr[16];
  stream_t *str;
  FILE *fl;

  strcpy(modestr, "r");
  if (!textmode)
    strcat(modestr, "b");

  fl = fopen(pathname, modestr);
  if (!fl)
    return 0;

  str = gli_new_stream(strtype_File, 
    TRUE, FALSE, rock, FALSE);
  if (!str)
  {
    fclose(fl);
    return 0;
  }

  str->file = fl;
  str->lastop = 0;
  str->textfile = textmode;

  return str;
}

stream_t *glk_stream_open_memory(char *buf, glui32 buflen, glui32 fmode, 
    glui32 rock)
{
  stream_t *str;

  if (fmode != filemode_Read && fmode != filemode_Write 
   && fmode != filemode_ReadWrite)
  {
    gli_strict_warning("stream_open_memory: illegal filemode");
    return 0;
  }

  str = gli_new_stream(strtype_Memory, 
    (fmode != filemode_Write), 
    (fmode != filemode_Read), 
    rock,
    FALSE);
  if (!str)
    return 0;

  if (buf && buflen)
  {
    str->buf = buf;
    str->bufptr = buf;
    str->buflen = buflen;
    str->bufend = (unsigned char *)str->buf + str->buflen;
    if (fmode == filemode_Write)
      str->bufeof = buf;
    else
      str->bufeof = str->bufend;
    if (gli_register_arr)
      str->arrayrock = (*gli_register_arr)(buf, buflen, "&+#!Cn");
  }

  return str;
}

stream_t *glk_stream_open_memory_uni(glui32 *buf, glui32 buflen, glui32 fmode, 
    glui32 rock)
{
  stream_t *str;

  if (fmode != filemode_Read && fmode != filemode_Write 
   && fmode != filemode_ReadWrite)
  {
    gli_strict_warning("stream_open_memory: illegal filemode");
    return 0;
  }

  str = gli_new_stream(strtype_Memory, 
    (fmode != filemode_Write), 
    (fmode != filemode_Read), 
    rock,
    TRUE);
  if (!str)
    return 0;

  if (buf && buflen)
  {
    str->buf = buf;
    str->bufptr = buf;
    str->buflen = buflen;
    str->bufend = (glui32 *)str->buf + str->buflen;
    if (fmode == filemode_Write)
      str->bufeof = buf;
    else
      str->bufeof = str->bufend;
    if (gli_register_arr)
      str->arrayrock = (*gli_register_arr)(buf, buflen, "&+#!Iu");
  }

  return str;
}

stream_t *gli_stream_open_window(window_t *win)
{
  stream_t *str;

  str = gli_new_stream(strtype_Window, FALSE, TRUE, 0, TRUE);
  if (!str)
    return NULL;

  str->win = win;

  return str;
}

void gli_stream_fill_result(stream_t *str, stream_result_t *result)
{
  if (!result)
    return;

  result->readcount = str->readcount;
  result->writecount = str->writecount;
}

void glk_stream_close(stream_t *str, stream_result_t *result)
{
  if (!str)
  {
    gli_strict_warning("stream_close: invalid ref");
    return;
  }

  if (str->type == strtype_Window)
  {
    gli_strict_warning("stream_close: cannot close window stream");
    return;
  }

  gli_stream_fill_result(str, result);

  gli_stream_close(str);
}

void gli_stream_close(stream_t *str)
{
  window_t *win;

  if (str == gli_currentstr)
    gli_currentstr = NULL;

  for (win = gli_window_iterate_treeorder(NULL); 
       win != NULL; 
       win = gli_window_iterate_treeorder(win))
  {
    if (win->echostr == str)
      win->echostr = NULL;
  }

  switch (str->type)
  {
      case strtype_Window:
          /* nothing necessary; the window is already being closed */
          break;
      case strtype_Memory: 
          if (gli_unregister_arr)
          {
              /* This could be a char array or a glui32 array. */
              char *typedesc = (str->unicode ? "&+#!Iu" : "&+#!Cn");
              (*gli_unregister_arr)(str->buf, str->buflen, typedesc, str->arrayrock);
          }
          break;
      case strtype_File:
          fclose(str->file);
          str->file = NULL;
          str->lastop = 0;
          break;
  }

  gli_delete_stream(str);
}

void gli_streams_close_all()
{
  /* This is used only at shutdown time; it closes file streams (the
     only ones that need finalization.) */
  stream_t *str, *strnext;

  str = gli_streamlist;

  while (str)
  {
    strnext = str->next;
    if (str->type == strtype_File)
      gli_stream_close(str);
    str = strnext;
  }
}

void glk_stream_set_position(stream_t *str, glsi32 pos, glui32 seekmode)
{
  if (!str)
  {
    gli_strict_warning("stream_set_position: invalid ref");
    return;
  }

  switch (str->type)
  {
      case strtype_Memory: 
          if (!str->unicode)
          {
              if (seekmode == seekmode_Current)
                  pos = ((unsigned char *)str->bufptr - (unsigned char *)str->buf) + pos;
              else if (seekmode == seekmode_End)
                  pos = ((unsigned char *)str->bufeof - (unsigned char *)str->buf) + pos;
              else
                  /* pos = pos */ ;
              if (pos < 0)
                  pos = 0;
              if (pos > ((unsigned char *)str->bufeof - (unsigned char *)str->buf))
                  pos = ((unsigned char *)str->bufeof - (unsigned char *)str->buf);
              str->bufptr = (unsigned char *)str->buf + pos;
          }
          else
          {
              if (seekmode == seekmode_Current)
                  pos = ((glui32 *)str->bufptr - (glui32 *)str->buf) + pos;
              else if (seekmode == seekmode_End)
                  pos = ((glui32 *)str->bufeof - (glui32 *)str->buf) + pos;
              else
                  /* pos = pos */ ;
              if (pos < 0)
                  pos = 0;
              if (pos > ((glui32 *)str->bufeof - (glui32 *)str->buf))
                  pos = ((glui32 *)str->bufeof - (glui32 *)str->buf);
              str->bufptr = (glui32 *)str->buf + pos;
          }
          break;
      case strtype_Window:
          /* don't pass to echo stream */
          break;
      case strtype_File:
          /* Either reading or writing is legal after an fseek. */
          str->lastop = 0;
          if (str->unicode)
              pos *= 4;
          fseek(str->file, pos, 
              ((seekmode == seekmode_Current) ? 1 :
              ((seekmode == seekmode_End) ? 2 : 0)));
          break;
  }
}

glui32 glk_stream_get_position(stream_t *str)
{
  if (!str)
  {
    gli_strict_warning("stream_get_position: invalid ref");
    return 0;
  }

  switch (str->type)
  {
      case strtype_Memory: 
          if (str->unicode)
              return ((glui32 *)str->bufptr - (glui32 *)str->buf);
          else
              return ((unsigned char *)str->bufptr - (unsigned char *)str->buf);
      case strtype_File:
          if (str->unicode)
              return ftell(str->file) / 4;
          else
              return ftell(str->file);
      case strtype_Window:
      default:
          return 0;
  }
}

void gli_stream_set_current(stream_t *str)
{
    gli_currentstr = str;
}

void glk_stream_set_current(stream_t *str)
{
  if (!str)
  {
    gli_currentstr = NULL;
    return;
  }

  gli_currentstr = str;
}

stream_t *glk_stream_get_current()
{
  if (!gli_currentstr)
    return 0;
  return gli_currentstr;
}

static void gli_stream_ensure_op(stream_t *str, glui32 op)
{
  /* We have to do an fseek() between reading and writing. This will
     only come up for ReadWrite or WriteAppend files. */
  if (str->lastop != 0 && str->lastop != op)
  {
    long pos = ftell(str->file);
    fseek(str->file, pos, SEEK_SET);
  }
  str->lastop = op;
}

static void gli_put_char(stream_t *str, unsigned char ch)
{
  if (!str || !str->writable)
    return;

  str->writecount++;

  switch (str->type)
  {
      case strtype_Memory:
          if (str->bufptr < str->bufend)
          {
              if (str->unicode)
              {
                  *((glui32 *)str->bufptr) = ch;
                  str->bufptr = ((glui32 *)str->bufptr) + 1;
              }
              else
              {
                  *((unsigned char *)str->bufptr) = ch;
                  str->bufptr = ((unsigned char *)str->bufptr) + 1;
              }
              if (str->bufptr > str->bufeof)
                  str->bufeof = str->bufptr;
          }
          break;
      case strtype_Window:
          if (str->win->line_request || str->win->line_request_uni)
          {
              if (gli_conf_safeclicks && gli_forceclick)
              {
                  glk_cancel_line_event(str->win, NULL);
                  gli_forceclick = 0;
              }
              else
              {
                  gli_strict_warning("put_char: window has pending line request");
                  break;
              }
          }
          gli_window_put_char_uni(str->win, ch);
          if (str->win->echostr)
              gli_put_char(str->win->echostr, ch);
          break;
      case strtype_File:
          gli_stream_ensure_op(str, filemode_Write);
          if (!str->unicode)
          {
              putc(ch, str->file);
          }
          else if (str->textfile)
          {
              gli_putchar_utf8((glui32)ch, str->file);
          }
          else
          {
              putc(0, str->file);
              putc(0, str->file);
              putc(0, str->file);
              putc(ch, str->file);
          }
          fflush(str->file);
          break;
  }
}

static void gli_put_char_uni(stream_t *str, glui32 ch)
{
  if (!str || !str->writable)
    return;

  str->writecount++;

  switch (str->type)
  {
      case strtype_Memory:
          if (str->bufptr < str->bufend)
          {
              if (str->unicode)
              {
                  *((glui32 *)str->bufptr) = ch;
                  str->bufptr = ((glui32 *)str->bufptr) + 1;
              }
              else
              {
                  *((unsigned char *)str->bufptr) = (unsigned char)ch;
                  str->bufptr = ((unsigned char *)str->bufptr) + 1;
              }
              if (str->bufptr > str->bufeof)
                  str->bufeof = str->bufptr;
          }
          break;
      case strtype_Window:
          if (str->win->line_request || str->win->line_request_uni)
          {
              if (gli_conf_safeclicks && gli_forceclick)
              {
                  glk_cancel_line_event(str->win, NULL);
                  gli_forceclick = 0;
              }
              else
              {
                  gli_strict_warning("put_char: window has pending line request");
                  break;
              }
          }
          gli_window_put_char_uni(str->win, ch);
          if (str->win->echostr)
              gli_put_char_uni(str->win->echostr, ch);
          break;
      case strtype_File:
          gli_stream_ensure_op(str, filemode_Write);
          if (!str->unicode)
          {
              if (ch >= 0x100)
                  ch = '?';
              putc(ch, str->file);
          }
          else if (str->textfile)
          {
              gli_putchar_utf8(ch, str->file);
          }
          else
          {
              putc(((ch >> 24) & 0xFF), str->file);
              putc(((ch >> 16) & 0xFF), str->file);
              putc(((ch >>  8) & 0xFF), str->file);
              putc( (ch        & 0xFF), str->file);
          }
          fflush(str->file);
          break;
  }
}

static void gli_put_buffer(stream_t *str, char *buf, glui32 len)
{
    glui32 lx;
    unsigned char *cx;

    if (!str || !str->writable)
        return;

    str->writecount += len;

    switch (str->type)
    {
        case strtype_Memory:
            if (str->bufptr >= str->bufend)
            {
                len = 0;
            }
            else
            {
                if (!str->unicode)
                {
                    unsigned char *bp = str->bufptr;
                    if (bp + len > (unsigned char *)str->bufend)
                    {
                        lx = (bp + len) - (unsigned char *)str->bufend;
                        if (lx < len)
                            len -= lx;
                        else
                            len = 0;
                    }
                    if (len)
                    {
                        memmove(bp, buf, len);
                        bp += len;
                        if (bp > (unsigned char *)str->bufeof)
                            str->bufeof = bp;
                    }
                    str->bufptr = bp;
                }
                else
                {
                    glui32 *bp = str->bufptr;
                    if (bp + len > (glui32 *)str->bufend)
                    {
                        lx = (bp + len) - (glui32 *)str->bufend;
                        if (lx < len)
                            len -= lx;
                        else
                            len = 0;
                    }
                    if (len)
                    {
                        glui32 i;
                        for (i = 0; i < len; i++)
                            bp[i] = buf[i];
                        bp += len;
                        if (bp > (glui32 *)str->bufeof)
                            str->bufeof = bp;
                    }
                    str->bufptr = bp;
                }
            }
            break;
        case strtype_Window:
            if (str->win->line_request || str->win->line_request_uni)
            {
                if (gli_conf_safeclicks && gli_forceclick)
                {
                    glk_cancel_line_event(str->win, NULL);
                    gli_forceclick = 0;
                }
                else
                {
                    gli_strict_warning("put_buffer: window has pending line request");
                    break;
                }
            }
            for (lx=0, cx=buf; lx<len; lx++, cx++)
                gli_window_put_char_uni(str->win, *cx);
            if (str->win->echostr)
                gli_put_buffer(str->win->echostr, buf, len);
            break;
        case strtype_File:
            gli_stream_ensure_op(str, filemode_Write);
            for (lx=0; lx<len; lx++)
            {
                unsigned char ch = ((unsigned char *)buf)[lx];
                if (!str->unicode)
                {
                    putc(ch, str->file);
                }
                else if (str->textfile)
                {
                    gli_putchar_utf8((glui32)ch, str->file);
                }
                else
                {
                    putc(((ch >> 24) & 0xFF), str->file);
                    putc(((ch >> 16) & 0xFF), str->file);
                    putc(((ch >>  8) & 0xFF), str->file);
                    putc( (ch        & 0xFF), str->file);
                }
            }
            fflush(str->file);
            break;
    }
}

static void gli_put_buffer_uni(stream_t *str, glui32 *buf, glui32 len)
{
    glui32 lx;
    glui32 *cx;

    if (!str || !str->writable)
        return;

    str->writecount += len;

    switch (str->type)
    {
        case strtype_Memory:
            if (str->bufptr >= str->bufend)
            {
                len = 0;
            }
            else
            {
                if (!str->unicode)
                {
                    unsigned char *bp = str->bufptr;
                    if (bp + len > (unsigned char *)str->bufend)
                    {
                        lx = (bp + len) - (unsigned char *)str->bufend;
                        if (lx < len)
                            len -= lx;
                        else
                            len = 0;
                    }
                    if (len)
                    {
                        glui32 i;
                        for (i = 0; i < len; i++)
                        {
                            glui32 ch = buf[i];
                            if (ch > 0xff)
                                ch = '?';
                            bp[i] = (unsigned char)ch;
                        }
                        bp += len;
                        if (bp > (unsigned char *)str->bufeof)
                            str->bufeof = bp;
                    }
                    str->bufptr = bp;
                }
                else
                {
                    glui32 *bp = str->bufptr;
                    if (bp + len > (glui32 *)str->bufend)
                    {
                        lx = (bp + len) - (glui32 *)str->bufend;
                        if (lx < len)
                            len -= lx;
                        else
                            len = 0;
                    }
                    if (len)
                    {
                        memmove(bp, buf, len * 4);
                        bp += len;
                        if (bp > (glui32 *)str->bufeof)
                            str->bufeof = bp;
                    }
                    str->bufptr = bp;
                }
            }
            break;
        case strtype_Window:
            if (str->win->line_request || str->win->line_request_uni)
            {
                if (gli_conf_safeclicks && gli_forceclick)
                {
                    glk_cancel_line_event(str->win, NULL);
                    gli_forceclick = 0;
                }
                else
                {
                    gli_strict_warning("put_buffer: window has pending line request");
                    break;
                }
            }
            for (lx=0, cx=buf; lx<len; lx++, cx++)
                gli_window_put_char_uni(str->win, *cx);
            if (str->win->echostr)
                gli_put_buffer_uni(str->win->echostr, buf, len);
            break;
        case strtype_File:
            gli_stream_ensure_op(str, filemode_Write);
            for (lx=0; lx<len; lx++)
            {
                glui32 ch = buf[lx];
                if (!str->unicode)
                {
                    if (ch >= 0x100)
                        ch = '?';
                    putc(ch, str->file);
                }
                else if (str->textfile)
                {
                    gli_putchar_utf8(ch, str->file);
                }
                else
                {
                    putc(((ch >> 24) & 0xFF), str->file);
                    putc(((ch >> 16) & 0xFF), str->file);
                    putc(((ch >>  8) & 0xFF), str->file);
                    putc( (ch        & 0xFF), str->file);
                }
            }
            fflush(str->file);
            break;
    }
}

static void gli_unput_buffer(stream_t *str, char *buf, glui32 len)
{
    glui32 lx;
    unsigned char *cx;

    if (!str || !str->writable)
        return;

    if (str->type == strtype_Window)
    {
        if (str->win->line_request || str->win->line_request_uni)
        {
            if (gli_conf_safeclicks && gli_forceclick)
            {
                glk_cancel_line_event(str->win, NULL);
                gli_forceclick = 0;
            }
            else
            {
                gli_strict_warning("unput_buffer: window has pending line request");
                return;
            }
        }
        for (lx=0, cx=buf+len-1; lx<len; lx++, cx--)
        {
            if (!gli_window_unput_char_uni(str->win, *cx))
                break;
            str->writecount--;
        }
        if (str->win->echostr)
            gli_unput_buffer(str->win->echostr, buf, len);
    }
}

static void gli_unput_buffer_uni(stream_t *str, glui32 *buf, glui32 len)
{
    glui32 lx;
    glui32 *cx;

    if (!str || !str->writable)
        return;

    if (str->type == strtype_Window)
    {
        if (str->win->line_request || str->win->line_request_uni)
        {
            if (gli_conf_safeclicks && gli_forceclick)
            {
                glk_cancel_line_event(str->win, NULL);
                gli_forceclick = 0;
            }
            else
            {
                gli_strict_warning("unput_buffer: window has pending line request");
                return;
            }
        }
        for (lx=0, cx=buf+len-1; lx<len; lx++, cx--)
        {
            if (!gli_window_unput_char_uni(str->win, *cx))
                break;
            str->writecount--;
        }
        if (str->win->echostr)
            gli_unput_buffer_uni(str->win->echostr, buf, len);
    }
}

static void gli_set_style(stream_t *str, glui32 val)
{
    if (!str || !str->writable)
        return;

    if (val >= style_NUMSTYLES)
        val = 0;
    if (val < 0)
        val = 0;

    switch (str->type)
    {
        case strtype_Window:
            str->win->attr.style = val;
            if (str->win->echostr)
                gli_set_style(str->win->echostr, val);
            break;
    }
}

static void gli_set_zcolors(stream_t *str, glui32 fg, glui32 bg)
{
    if (!str || !str->writable)
        return;

    if (!gli_conf_stylehint)
        return;

    unsigned char fore[3];
    fore[0] = (fg >> 16) & 0xff;
    fore[1] = (fg >> 8) & 0xff;
    fore[2] = (fg) & 0xff;

    unsigned char back[3];
    back[0] = (bg >> 16) & 0xff;
    back[1] = (bg >> 8) & 0xff;
    back[2] = (bg) & 0xff;

    switch (str->type)
    {
        case strtype_Window:
            if (fg != zcolor_Transparent && fg != zcolor_Cursor)
            {
                if (fg == zcolor_Default)
                {
                    str->win->attr.fgset = 0;
                    str->win->attr.fgcolor = 0;
                    gli_override_fg_set = 0;
                    gli_override_fg_val = 0;
                    memcpy(gli_more_color, gli_more_save, 3);
                    memcpy(gli_caret_color, gli_caret_save, 3);
                    memcpy(gli_link_color, gli_link_save, 3);
                }
                else if (fg != zcolor_Current)
                {
                    str->win->attr.fgset = 1;
                    str->win->attr.fgcolor = fg;
                    gli_override_fg_set = 1;
                    gli_override_fg_val = fg;
                    memcpy(gli_more_color, fore, 3);
                    memcpy(gli_caret_color, fore, 3);
                    memcpy(gli_link_color, fore, 3);
                }
            }

            if (bg != zcolor_Transparent && bg != zcolor_Cursor)
            {
                if (bg == zcolor_Default)
                {
                    str->win->attr.bgset = 0;
                    str->win->attr.bgcolor = 0;
                    gli_override_bg_set = 0;
                    gli_override_bg_val = 0;
                    memcpy(gli_window_color, gli_window_save, 3);
                    memcpy(gli_border_color, gli_border_save, 3);
                }
                else if (bg != zcolor_Current)
                {
                    str->win->attr.bgset = 1;
                    str->win->attr.bgcolor = bg;
                    gli_override_bg_set = 1;
                    gli_override_bg_val = bg;
                    memcpy(gli_window_color, back, 3);
                    memcpy(gli_border_color, back, 3);
                }
            }

            if (fg == zcolor_Default && bg == zcolor_Default)
                gli_override_reverse = 0;
            else
                gli_override_reverse = 1;

            if (str->win->echostr)
                gli_set_zcolors(str->win->echostr, fg, bg);
            break;
    }

    gli_force_redraw = 1;
}

static void gli_set_reversevideo(stream_t *str, glui32 reverse)
{
    if (!str || !str->writable)
        return;

    if (!gli_conf_stylehint)
        return;

    switch (str->type)
    {
        case strtype_Window:
            str->win->attr.reverse = (reverse != 0);
            if (str->win->echostr)
                gli_set_reversevideo(str->win->echostr, reverse);
            break;
    }
    gli_force_redraw = 1;
}

static void gli_set_hyperlink(stream_t *str, glui32 linkval)
{
    if (!str || !str->writable)
        return;

    switch (str->type)
    {
        case strtype_Window:
            str->win->attr.hyper = linkval;
            break;
    }

}

void gli_stream_echo_line(stream_t *str, char *buf, glui32 len)
{
    gli_put_buffer(str, buf, len);
    gli_put_char(str, '\n');
}

void gli_stream_echo_line_uni(stream_t *str, glui32 *buf, glui32 len)
{
    gli_put_buffer_uni(str, buf, len);
    gli_put_char_uni(str, '\n');
}

static glsi32 gli_get_char(stream_t *str)
{
    if (!str || !str->readable)
        return -1;

    switch (str->type)
    {
        case strtype_Memory:
            if (str->bufptr < str->bufend)
            {
                if (!str->unicode)
                {
                    unsigned char ch;
                    ch = *((unsigned char *)str->bufptr);
                    str->bufptr = ((unsigned char *)str->bufptr) + 1;
                    str->readcount++;
                    return ch;
                }
                else
                {
                    glui32 ch;
                    ch = *((glui32 *)str->bufptr);
                    str->bufptr = ((glui32 *)str->bufptr) + 1;
                    str->readcount++;
                    if (ch > 0xff)
                        ch = '?';
                    return ch;
                }
            }
            else
            {
                return -1;
            }
        case strtype_File:
            gli_stream_ensure_op(str, filemode_Read);
            int res;
            if (!str->unicode)
            {
                res = getc(str->file);
            }
            else if (str->textfile)
            {
                res = gli_getchar_utf8(str->file);
            }
            else
            {
                glui32 ch;
                res = getc(str->file);
                if (res == -1)
                    return -1;
                ch = (res & 0xFF);
                res = getc(str->file);
                if (res == -1)
                    return -1;
                ch = (ch << 8) | (res & 0xFF);
                res = getc(str->file);
                if (res == -1)
                    return -1;
                ch = (ch << 8) | (res & 0xFF);
                res = getc(str->file);
                if (res == -1)
                    return -1;
                ch = (ch << 8) | (res & 0xFF);
                res = ch;
            }
            if (res != -1)
            {
                str->readcount++;
                if (res >= 0x100)
                    return '?';
                return (glsi32)res;
            }
            else
            {
                return -1;
            }
        case strtype_Window:
        default:
            return -1;
    }
}

static glsi32 gli_get_char_uni(stream_t *str)
{
    if (!str || !str->readable)
        return -1;

    switch (str->type)
    {
        case strtype_Memory:
            if (str->bufptr < str->bufend)
            {
                if (!str->unicode)
                {
                    unsigned char ch;
                    ch = *((unsigned char *)str->bufptr);
                    str->bufptr = ((unsigned char *)str->bufptr) + 1;
                    str->readcount++;
                    return ch;
                }
                else
                {
                    glui32 ch;
                    ch = *((glui32 *)str->bufptr);
                    str->bufptr = ((glui32 *)str->bufptr) + 1;
                    str->readcount++;
                    return ch;
                }
            }
            else
            {
                return -1;
            }
        case strtype_File:
            gli_stream_ensure_op(str, filemode_Read);
            int res;
            if (!str->unicode)
            {
                res = getc(str->file);
            }
            else if (str->textfile)
            {
                res = gli_getchar_utf8(str->file);
            }
            else
            {
                glui32 ch;
                res = getc(str->file);
                if (res == -1)
                    return -1;
                ch = (res & 0xFF);
                res = getc(str->file);
                if (res == -1)
                    return -1;
                ch = (ch << 8) | (res & 0xFF);
                res = getc(str->file);
                if (res == -1)
                    return -1;
                ch = (ch << 8) | (res & 0xFF);
                res = getc(str->file);
                if (res == -1)
                    return -1;
                ch = (ch << 8) | (res & 0xFF);
                res = ch;
            }
            if (res != -1)
            {
                str->readcount++;
                return (glsi32)res;
            }
            else
            {
                return -1;
            }
        case strtype_Window:
        default:
            return -1;
    }
}

static glui32 gli_get_buffer(stream_t *str, char *buf, glui32 len)
{
    if (!str || !str->readable)
        return 0;

    switch (str->type)
    {
        case strtype_Memory:
            if (str->bufptr >= str->bufend)
            {
                len = 0;
            }
            else
            {
                if (!str->unicode)
                {
                    unsigned char *bp = str->bufptr;
                    if (bp + len > (unsigned char *)str->bufend)
                    {
                        glui32 lx;
                        lx = (bp + len) - (unsigned char *)str->bufend;
                        if (lx < len)
                            len -= lx;
                        else
                            len = 0;
                    }
                    if (len)
                    {
                        memcpy(buf, bp, len);
                        bp += len;
                        if (bp > (unsigned char *)str->bufeof)
                            str->bufeof = bp;
                    }
                    str->readcount += len;
                    str->bufptr = bp;
                }
                else
                {
                    glui32 *bp = str->bufptr;
                    if (bp + len > (glui32 *)str->bufend)
                    {
                        glui32 lx;
                        lx = (bp + len) - (glui32 *)str->bufend;
                        if (lx < len)
                            len -= lx;
                        else
                            len = 0;
                    }
                    if (len)
                    {
                        glui32 i;
                        for (i = 0; i < len; i++)
                        {
                            glui32 ch = *bp++;
                            if (ch > 0xff)
                                ch = '?';
                            *buf++ = (char)ch;
                        }
                        if (bp > (glui32 *)str->bufeof)
                            str->bufeof = bp;
                    }
                    str->readcount += len;
                    str->bufptr = bp;
                }
            }
            return len;
        case strtype_File:
            gli_stream_ensure_op(str, filemode_Read);
            if (!str->unicode)
            {
                glui32 res;
                res = fread(buf, 1, len, str->file);
                str->readcount += res;
                return res;
            }
            else if (str->textfile)
            {
                glui32 lx;
                for (lx=0; lx<len; lx++)
                {
                    glui32 ch;
                    ch = gli_getchar_utf8(str->file);
                    if (ch == -1)
                        break;
                    str->readcount++;
                    if (ch >= 0x100)
                        ch = '?';
                    buf[lx] = (char)ch;
                }
                return lx;
            }
            else
            {
                glui32 lx;
                for (lx=0; lx<len; lx++)
                {
                    int res;
                    glui32 ch;
                    res = getc(str->file);
                    if (res == -1)
                        break;
                    ch = (res & 0xFF);
                    res = getc(str->file);
                    if (res == -1)
                        break;
                    ch = (ch << 8) | (res & 0xFF);
                    res = getc(str->file);
                    if (res == -1)
                        break;
                    ch = (ch << 8) | (res & 0xFF);
                    res = getc(str->file);
                    if (res == -1)
                        break;
                    ch = (ch << 8) | (res & 0xFF);
                    str->readcount++;
                    if (ch >= 0x100)
                        ch = '?';
                    buf[lx] = (char)ch;
                }
                return lx;
            }
        case strtype_Window:
        default:
            return 0;
    }
}

static glui32 gli_get_buffer_uni(stream_t *str, glui32 *buf, glui32 len)
{
    if (!str || !str->readable)
        return 0;

    switch (str->type)
    {
        case strtype_Memory:
            if (str->bufptr >= str->bufend)
            {
                len = 0;
            }
            else
            {
                if (!str->unicode)
                {
                    unsigned char *bp = str->bufptr;
                    if (bp + len > (unsigned char *)str->bufend)
                    {
                        glui32 lx;
                        lx = (bp + len) - (unsigned char *)str->bufend;
                        if (lx < len)
                            len -= lx;
                        else
                            len = 0;
                    }
                    if (len)
                    {
                        glui32 i;
                        for (i = 0; i < len; i++)
                            buf[i] = bp[i];
                        bp += len;
                        if (bp > (unsigned char *)str->bufeof)
                            str->bufeof = bp;
                    }
                    str->readcount += len;
                    str->bufptr = bp;
                }
                else
                {
                    glui32 *bp = str->bufptr;
                    if (bp + len > (glui32 *)str->bufend)
                    {
                        glui32 lx;
                        lx = (bp + len) - (glui32 *)str->bufend;
                        if (lx < len)
                            len -= lx;
                        else
                            len = 0;
                    }
                    if (len)
                    {
                        memcpy(buf, bp, len * 4);
                        bp += len;
                        if (bp > (glui32 *)str->bufeof)
                            str->bufeof = bp;
                    }
                    str->readcount += len;
                    str->bufptr = bp;
                }
            }
            return len;
        case strtype_File:
            gli_stream_ensure_op(str, filemode_Read);
            if (!str->unicode)
            {
                glui32 lx;
                for (lx=0; lx<len; lx++)
                {
                    int res;
                    glui32 ch;
                    res = getc(str->file);
                    if (res == -1)
                        break;
                    ch = (res & 0xFF);
                    str->readcount++;
                    buf[lx] = ch;
                }
                return lx;
            } 
            else if (str->textfile)
            {
                glui32 lx;
                for (lx=0; lx<len; lx++)
                {
                    glui32 ch;
                    ch = gli_getchar_utf8(str->file);
                    if (ch == -1)
                        break;
                    str->readcount++;
                    buf[lx] = ch;
                }
                return lx;
            } 
            else
            {
                glui32 lx;
                for (lx=0; lx<len; lx++)
                {
                    int res;
                    glui32 ch;
                    res = getc(str->file);
                    if (res == -1)
                        break;
                    ch = (res & 0xFF);
                    res = getc(str->file);
                    if (res == -1)
                        break;
                    ch = (ch << 8) | (res & 0xFF);
                    res = getc(str->file);
                    if (res == -1)
                        break;
                    ch = (ch << 8) | (res & 0xFF);
                    res = getc(str->file);
                    if (res == -1)
                        break;
                    ch = (ch << 8) | (res & 0xFF);
                    str->readcount++;
                    buf[lx] = ch;
                }
                return lx;
            }
        case strtype_Window:
        default:
            return 0;
    }
}

static glui32 gli_get_line(stream_t *str, char *cbuf, glui32 len)
{
    glui32 lx;
    int gotnewline;

    if (!str || !str->readable)
        return 0;

    switch (str->type)
    {
        case strtype_Memory:
            if (len == 0)
                return 0;
            len -= 1; /* for the terminal null */
            if (!str->unicode)
            {
                if (str->bufptr >= str->bufend)
                {
                    len = 0;
                }
                else
                {
                    if ((char *)str->bufptr + len > (char *)str->bufend)
                    {
                        lx = ((char *)str->bufptr + len) - (char *)str->bufend;
                        if (lx < len)
                            len -= lx;
                        else
                            len = 0;
                    }
                }
                gotnewline = FALSE;
                for (lx=0; lx<len && !gotnewline; lx++)
                {
                    cbuf[lx] = ((char *)str->bufptr)[lx];
                    gotnewline = (cbuf[lx] == '\n');
                }
                cbuf[lx] = '\0';
                str->bufptr = ((char *)str->bufptr) + lx;
            }
            else
            {
                if (str->bufptr >= str->bufend)
                {
                    len = 0;
                }
                else
                {
                    if ((char *)str->bufptr + len > (char *)str->bufend)
                    {
                        lx = ((char *)str->bufptr + len) - (char *)str->bufend;
                        if (lx < len)
                            len -= lx;
                        else
                            len = 0;
                    }
                }
                gotnewline = FALSE;
                for (lx=0; lx<len && !gotnewline; lx++)
                {
                    glui32 ch;
                    ch = ((glui32 *)str->bufptr)[lx];
                    if (ch >= 0x100)
                        ch = '?';
                    cbuf[lx] = (char)ch;
                    gotnewline = (ch == '\n');
                }
                cbuf[lx] = '\0';
                str->bufptr = ((glui32 *)str->bufptr) + lx;
            }
            str->readcount += lx;
            return lx;
        case strtype_File: 
            if (len == 0)
                return 0;
            gli_stream_ensure_op(str, filemode_Read);
            if (!str->unicode)
            {
                char *res;
                res = fgets(cbuf, len, str->file);
                if (!res)
                {
                    return 0;
                }
                else
                {
                    lx = strlen(cbuf);
                    str->readcount += lx;
                    return lx;
                }
            }
            else if (str->textfile)
            {
                len -= 1; /* for the terminal null */
                gotnewline = FALSE;
                for (lx=0; lx<len && !gotnewline; lx++)
                {
                    glui32 ch;
                    ch = gli_getchar_utf8(str->file);
                    if (ch == -1)
                        break;
                    str->readcount++;
                    if (ch >= 0x100)
                        ch = '?';
                    cbuf[lx] = (char)ch;
                    gotnewline = (ch == '\n');
                }
                cbuf[lx] = '\0';
                return lx;
            }
            else
            {
                len -= 1; /* for the terminal null */
                gotnewline = FALSE;
                for (lx=0; lx<len && !gotnewline; lx++)
                {
                    int res;
                    glui32 ch;
                    res = getc(str->file);
                    if (res == -1)
                        break;
                    ch = (res & 0xFF);
                    res = getc(str->file);
                    if (res == -1)
                        break;
                    ch = (ch << 8) | (res & 0xFF);
                    res = getc(str->file);
                    if (res == -1)
                        break;
                    ch = (ch << 8) | (res & 0xFF);
                    res = getc(str->file);
                    if (res == -1)
                        break;
                    ch = (ch << 8) | (res & 0xFF);
                    str->readcount++;
                    if (ch >= 0x100)
                        ch = '?';
                    cbuf[lx] = (char)ch;
                    gotnewline = (ch == '\n');
                }
                cbuf[lx] = '\0';
                return lx;
            }
        case strtype_Window:
        default:
            return 0;
    }
}

static glui32 gli_get_line_uni(stream_t *str, glui32 *ubuf, glui32 len)
{
    glui32 lx;
    int gotnewline;

    if (!str || !str->readable)
        return 0;

    switch (str->type)
    {
        case strtype_Memory:
            if (len == 0)
                return 0;
            len -= 1; /* for the terminal null */
            if (!str->unicode)
            {
                if (str->bufptr >= str->bufend)
                {
                    len = 0;
                }
                else
                {
                    if ((char *)str->bufptr + len > (char *)str->bufend)
                    {
                        lx = ((char *)str->bufptr + len) - (char *)str->bufend;
                        if (lx < len)
                            len -= lx;
                        else
                            len = 0;
                    }
                }
                gotnewline = FALSE;
                for (lx=0; lx<len && !gotnewline; lx++)
                {
                    ubuf[lx] = ((unsigned char *)str->bufptr)[lx];
                    gotnewline = (ubuf[lx] == '\n');
                }
                ubuf[lx] = '\0';
                str->bufptr = ((unsigned char *)str->bufptr) + lx;
            }
            else
            {
                if (str->bufptr >= str->bufend)
                {
                    len = 0;
                }
                else
                {
                    if ((glui32 *)str->bufptr + len > (glui32 *)str->bufend)
                    {
                        lx = ((glui32 *)str->bufptr + len) - (glui32 *)str->bufend;
                        if (lx < len)
                            len -= lx;
                        else
                            len = 0;
                    }
                }
                gotnewline = FALSE;
                for (lx=0; lx<len && !gotnewline; lx++)
                {
                    glui32 ch;
                    ch = ((glui32 *)str->bufptr)[lx];
                    ubuf[lx] = ch;
                    gotnewline = (ch == '\n');
                }
                ubuf[lx] = '\0';
                str->bufptr = ((glui32 *)str->bufptr) + lx;
            }
            str->readcount += lx;
            return lx;
        case strtype_File: 
            if (len == 0)
                return 0;
            gli_stream_ensure_op(str, filemode_Read);
            if (!str->unicode)
            {
                len -= 1; /* for the terminal null */
                gotnewline = FALSE;
                for (lx=0; lx<len && !gotnewline; lx++)
                {
                    int res;
                    glui32 ch;
                    res = getc(str->file);
                    if (res == -1)
                        break;
                    ch = (res & 0xFF);
                    str->readcount++;
                    ubuf[lx] = ch;
                    gotnewline = (ch == '\n');
                }
                ubuf[lx] = '\0';
                return lx;
            }
            else if (str->textfile)
            {
                len -= 1; /* for the terminal null */
                gotnewline = FALSE;
                for (lx=0; lx<len && !gotnewline; lx++)
                {
                    glui32 ch;
                    ch = gli_getchar_utf8(str->file);
                    if (ch == -1)
                        break;
                    str->readcount++;
                    ubuf[lx] = ch;
                    gotnewline = (ch == '\n');
                }
                ubuf[lx] = '\0';
                return lx;
            }
            else
            {
                len -= 1; /* for the terminal null */
                gotnewline = FALSE;
                for (lx=0; lx<len && !gotnewline; lx++)
                {
                    int res;
                    glui32 ch;
                    res = getc(str->file);
                    if (res == -1)
                        break;
                    ch = (res & 0xFF);
                    res = getc(str->file);
                    if (res == -1)
                        break;
                    ch = (ch << 8) | (res & 0xFF);
                    res = getc(str->file);
                    if (res == -1)
                        break;
                    ch = (ch << 8) | (res & 0xFF);
                    res = getc(str->file);
                    if (res == -1)
                        break;
                    ch = (ch << 8) | (res & 0xFF);
                    str->readcount++;
                    ubuf[lx] = ch;
                    gotnewline = (ch == '\n');
                }
                ubuf[lx] = '\0';
                return lx;
            }
        case strtype_Window:
        default:
            return 0;
    }
}

void glk_put_char(unsigned char ch)
{
    gli_put_char(gli_currentstr, ch);
}

void glk_put_char_uni(glui32 ch)
{
    gli_put_char_uni(gli_currentstr, ch);
}

void glk_put_char_stream(stream_t *str, unsigned char ch)
{
  if (!str)
  {
    gli_strict_warning("put_char_stream: invalid ref");
    return;
  }
  gli_put_char(str, ch);
}

void glk_put_char_stream_uni(stream_t *str, glui32 ch)
{
    if (!str)
    {
        gli_strict_warning("put_char_stream_uni: invalid ref");
        return;
    }
    gli_put_char_uni(str, ch);
}

void glk_put_string(char *s)
{
    gli_put_buffer(gli_currentstr, s, strlen(s));
}

glui32 strlen_uni(glui32 *s)
{
    glui32 length = 0;
    while (*s++) length++;
    return length;
}

void glk_put_string_uni(glui32 *s)
{
    gli_put_buffer_uni(gli_currentstr, s, strlen_uni(s));
}

void glk_put_string_stream(stream_t *str, char *s)
{
    if (!str)
    {
        gli_strict_warning("put_string_stream: invalid ref");
        return;
    }
    gli_put_buffer(str, s, strlen(s));
}

void glk_put_string_stream_uni(stream_t *str, glui32 *s)
{
    if (!str)
    {
        gli_strict_warning("put_string_stream_uni: invalid ref");
        return;
    }
    gli_put_buffer_uni(str, s, strlen_uni(s));
}

void glk_put_buffer(char *buf, glui32 len)
{
    gli_put_buffer(gli_currentstr, buf, len);
}

void glk_put_buffer_uni(glui32 *buf, glui32 len)
{
    gli_put_buffer_uni(gli_currentstr, buf, len);
}

void glk_put_buffer_stream(stream_t *str, char *buf, glui32 len)
{
    if (!str)
    {
        gli_strict_warning("put_string_stream: invalid ref");
        return;
    }
    gli_put_buffer(str, buf, len);
}

void glk_put_buffer_stream_uni(stream_t *str, glui32 *buf, glui32 len)
{
  if (!str)
  {
    gli_strict_warning("put_string_stream_uni: invalid ref");
    return;
  }
  gli_put_buffer_uni(str, buf, len);
}

void garglk_unput_string(char *s)
{
    gli_unput_buffer(gli_currentstr, s, strlen(s));
}

void garglk_unput_string_uni(glui32 *s)
{
    gli_unput_buffer_uni(gli_currentstr, s, strlen_uni(s));
}

void glk_set_style(glui32 val)
{
    gli_set_style(gli_currentstr, val);
}

void glk_set_style_stream(stream_t *str, glui32 val)
{
    if (!str)
    {
        gli_strict_warning("set_style_stream: invalid ref");
        return;
    }
    gli_set_style(str, val);
}

void garglk_set_zcolors(glui32 fg, glui32 bg)
{
    gli_set_zcolors(gli_currentstr, fg, bg);
}

void garglk_set_zcolors_stream(stream_t *str, glui32 fg, glui32 bg)
{
    if (!str)
    {
        gli_strict_warning("set_style_stream: invalid ref");
        return;
    }
    gli_set_zcolors(str, fg, bg);
}

void garglk_set_reversevideo(glui32 reverse)
{
    gli_set_reversevideo(gli_currentstr, reverse);
}

void garglk_set_reversevideo_stream(stream_t *str, glui32 reverse)
{
    if (!str)
    {
        gli_strict_warning("set_style_stream: invalid ref");
        return;
    }
    gli_set_reversevideo(str, reverse);
}

void glk_set_hyperlink(glui32 linkval)
{
    gli_set_hyperlink(gli_currentstr, linkval);
}

void glk_set_hyperlink_stream(stream_t *str, glui32 linkval)
{
    if (!str)
    {
        gli_strict_warning("set_hyperlink_stream: invalid ref");
        return;
    }
    gli_set_hyperlink(str, linkval);
}

glsi32 glk_get_char_stream(stream_t *str)
{
    if (!str)
    {
        gli_strict_warning("get_char_stream: invalid ref");
        return -1;
    }
    return gli_get_char(str);
}

glsi32 glk_get_char_stream_uni(stream_t *str)
{
    if (!str)
    {
        gli_strict_warning("get_char_stream_uni: invalid ref");
        return -1;
    }
    return gli_get_char_uni(str);
}

glui32 glk_get_line_stream(stream_t *str, char *buf, glui32 len)
{
    if (!str)
    {
        gli_strict_warning("get_line_stream: invalid ref");
        return -1;
    }
    return gli_get_line(str, buf, len);
}

glui32 glk_get_line_stream_uni(stream_t *str, glui32 *buf, glui32 len)
{
    if (!str)
    {
        gli_strict_warning("get_line_stream_uni: invalid ref");
        return -1;
    }
    return gli_get_line_uni(str, buf, len);
}

glui32 glk_get_buffer_stream(stream_t *str, char *buf, glui32 len)
{
    if (!str)
    {
        gli_strict_warning("get_buffer_stream: invalid ref");
        return -1;
    }
    return gli_get_buffer(str, buf, len);
}

glui32 glk_get_buffer_stream_uni(stream_t *str, glui32 *buf, glui32 len)
{
    if (!str)
    {
        gli_strict_warning("get_buffer_stream_uni: invalid ref");
        return -1;
    }
    return gli_get_buffer_uni(str, buf, len);
}
