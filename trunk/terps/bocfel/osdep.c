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
 */

/******************
 * Unix functions *
 ******************/
#ifdef ZTERP_UNIX
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <curses.h>
#include <term.h>

long long zterp_os_filesize(FILE *fp)
{
  struct stat buf;
  int fd = fileno(fp);

  if(fd == -1 || fstat(fd, &buf) == -1 || !S_ISREG(buf.st_mode) || buf.st_size > LLONG_MAX) return -1;

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
long long zterp_os_filesize(FILE *fp)
{
  long saved = ftell(fp), size;

  if(saved == -1) return -1;
  /* Assume fseek() can seek to the end of binary streams. */
  if(fseek(fp, 0, SEEK_END) == -1) return -1;
  size = ftell(fp);
  if(size == -1) return -1;
  if(fseek(fp, saved, SEEK_SET) == -1) return -1;

  return size;
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
