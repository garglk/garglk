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

#ifdef ZTERP_UNIX
#define _XOPEN_SOURCE	600
#endif

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>

#include "osdep.h"
#include "screen.h"

/* OS-specific functions should all be collected in this file for
 * convenience.  A sort of poor-man’s “reverse” inheritance is used: for
 * each function that a particular operating system provides, it should
 * #define a macro of the same name.  At the end of the file, a generic
 * function is provided for each function that has no associated macro
 * definition.
 *
 * The functions required are as follows:
 *
 * long zterp_os_filesize(FILE *fp)
 *
 * Return the size of the file referred to by fp.  It is safe to assume
 * that the file is opened in binary mode.  The file position indicator
 * need not be maintained.  If the size of the file is larger than
 * LONG_MAX, -1 should be returned.
 *
 * int zterp_os_have_unicode(void)
 *
 * The main purpose behind this function is to indicate whether
 * transcripts will be written in UTF-8 or Latin-1.  This is, of course,
 * not necessarily an OS matter, but I’ve run into some issues with
 * UTF-8 and Windows (at least through Wine), so I want to be as
 * sensible as I can with the defaults.  The user is able to override
 * this value if he so desires.
 * If a GLK build is not being used, this function also serves to
 * indicate whether all I/O, not just transcripts, should be UTF-8 or
 * not.  GLK libraries are able to be queried as to their support for
 * Unicode so there is no need to make assumptions in that case.
 *
 * void zterp_os_rcfile(char *s, size_t n)
 *
 * Different operating systems have different ideas about where
 * configuration data should be stored; this function will copy a
 * suitable value for the bocfel configuration file into the buffer s
 * which is n bytes long.
 *
 * void zterp_os_reopen_binary(FILE *fp)
 *
 * Writing UTF-8 requires that no mangling be done, such as might happen
 * when a stream is opened in text mode.  This function should, if
 * necessary, set the mode on the file pointer in fp to be binary.
 *
 * The following functions are useful for non-GLK builds only.  They
 * provide for some handling of screen functions that is normally taken
 * care of by GLK.
 *
 * void zterp_os_get_winsize(unsigned *w, unsigned *h)
 *
 * The size of the terminal, if known, is written into *w (width) and *h
 * (height).  If terminal size is unavalable, nothing should be written.
 *
 * void zterp_os_init_term(void)
 *
 * If something special needs to be done to prepare the terminal for
 * output, it should be done here.  This function is called once at
 * program startup.
 *
 * int zterp_os_have_style(int style)
 *
 * This should return true if the provided style (see style.h for valid
 * STYLE_ values) is available.  It is safe to assume that styles will
 * not be combined; e.g. this will not be called as:
 * zterp_os_have_style(STYLE_BOLD | STYLE_ITALIC);
 *
 * int zterp_os_have_colors(void)
 *
 * Returns true if the terminal supports colors.
 *
 * void zterp_os_set_style(int style, int fg, int bg)
 *
 * Set both a style and foreground/background color.  Any previous
 * settings should be ignored; for example, if the last call to
 * zterp_os_set_style() turned on italics and the current call sets
 * bold, the result should be bold, not bold italic.
 * Unlike in zterp_os_have_style(), here styles may be combined.  See
 * the Unix implementation for a reference.
 * The colors are Z-machine colors (see §8.3.1), with the following
 * note: the only color values that will ever be passed in are 1–9.
 */

/******************
 * Unix functions *
 ******************/
#ifdef ZTERP_UNIX
#include <sys/stat.h>

long zterp_os_filesize(FILE *fp)
{
  struct stat buf;
  int fd = fileno(fp);

  if(fd == -1 || fstat(fd, &buf) == -1 || !S_ISREG(buf.st_mode) || buf.st_size > LONG_MAX) return -1;

  return buf.st_size;
}
#define zterp_os_filesize

int zterp_os_have_unicode(void)
{
  return 1;
}
#define zterp_os_have_unicode

void zterp_os_rcfile(char *s, size_t n)
{
  snprintf(s, n, "%s/.bocfelrc", getenv("HOME") != NULL ? getenv("HOME") : ".");
}
#define zterp_os_rcfile

#ifndef ZTERP_GLK
#include <unistd.h>
#include <sys/ioctl.h>
#include <curses.h>
#include <term.h>
#ifdef TIOCGWINSZ
void zterp_os_get_winsize(unsigned *w, unsigned *h)
{
  struct winsize winsize;

  if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &winsize) == 0)
  {
    *w = winsize.ws_col;
    *h = winsize.ws_row;
  }
}
#define zterp_os_get_winsize
#endif

static const char *ital = NULL, *rev = NULL, *bold = NULL, *none = NULL;
static char *fg_string = NULL, *bg_string = NULL;
static int have_colors = 0;
void zterp_os_init_term(void)
{
  if(setupterm(NULL, STDIN_FILENO, NULL) != OK) return;

  /* prefer italics over underline for emphasized text */
  ital = tgetstr("ZH", NULL);
  if(ital == NULL) ital = tgetstr("us", NULL);
  rev  = tgetstr("mr", NULL);
  bold = tgetstr("md", NULL);
  none = tgetstr("me", NULL);

  fg_string = tgetstr("AF", NULL);
  bg_string = tgetstr("AB", NULL);

  have_colors = none != NULL && fg_string != NULL && bg_string != NULL;
}

int zterp_os_have_style(int style)
{
  if(none == NULL) return 0;

  if     (style == STYLE_ITALIC)  return ital != NULL;
  else if(style == STYLE_REVERSE) return rev  != NULL;
  else if(style == STYLE_BOLD)    return bold != NULL;
  else if(style == STYLE_NONE)    return none != NULL;

  return 0;
}
int zterp_os_have_colors(void)
{
  return have_colors;
}
void zterp_os_set_style(int style, int fg, int bg)
{
  /* If the terminal cannot be reset, nothing can be used. */
  if(none == NULL) return;

  putp(none);

  if((style & STYLE_ITALIC)  && ital != NULL) putp(ital);
  if((style & STYLE_REVERSE) && rev  != NULL) putp(rev);
  if((style & STYLE_BOLD)    && bold != NULL) putp(bold);

  if(have_colors)
  {
    if(fg > 1) putp(tparm(fg_string, fg - 2, 0, 0, 0, 0, 0, 0, 0, 0));
    if(bg > 1) putp(tparm(bg_string, bg - 2, 0, 0, 0, 0, 0, 0, 0, 0));
  }
}
#define zterp_os_init_term
#endif

/*********************
 * Windows functions *
 *********************/
#elif defined(ZTERP_WIN32)
void zterp_os_rcfile(char *s, size_t n)
{
  snprintf(s, n, "bocfel.ini");
}
#define zterp_os_rcfile

#endif

/*********************
 * Generic functions *
 *********************/
#ifndef zterp_os_filesize
long zterp_os_filesize(FILE *fp)
{
  /* Assume fseek() can seek to the end of binary streams. */
  if(fseek(fp, 0, SEEK_END) == -1) return -1;

  return ftell(fp);
}
#endif

#ifndef zterp_os_have_unicode
int zterp_os_have_unicode(void)
{
  return 0;
}
#endif

#ifndef zterp_os_rcfile
void zterp_os_rcfile(char *s, size_t n)
{
  snprintf(s, n, "bocfelrc");
}
#endif

/* When UTF-8 output is enabled, special translation of characters (e.g.
 * newline) should not be done.  Theoretically this function exists to
 * set stdin/stdout to binary mode, if necessary.  Unix makes no
 * text/binary distinction, but Windows does.  I’m under the impression
 * that there is a setmode() function that should be able to do this,
 * but my knowledge of Windows is so small that I do not want to do much
 * more than I have, lest I completely break Windows support—assuming it
 * even works.
 * freopen() should be able to do this, but with my testing under Wine,
 * no text gets output in such a case.
 */
#ifndef zterp_os_reopen_binary
void zterp_os_reopen_binary(FILE *fp)
{
}
#endif

#ifndef ZTERP_GLK
#ifndef zterp_os_get_winsize
void zterp_os_get_winsize(unsigned *w, unsigned *h)
{
}
#endif

#ifndef zterp_os_init_term
void zterp_os_init_term(void)
{
}
int zterp_os_have_style(int style)
{
  return 0;
}
int zterp_os_have_colors(void)
{
  return 0;
}
void zterp_os_set_style(int style, int fg, int bg)
{
}
#endif
#endif
