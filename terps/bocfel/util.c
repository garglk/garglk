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
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "util.h"
#include "screen.h"
#include "unicode.h"
#include "zterp.h"

#ifdef ZTERP_GLK
#include <glk.h>
#endif

#ifndef ZTERP_NO_SAFETY_CHECKS
unsigned long zassert_pc;

void assert_fail(const char *func, const char *file, int line, const char *fmt, ...)
{
  va_list ap;
  char str[1024];

  snprintf(str, sizeof str, "%s():%s:%d: ", func, file, line);

  va_start(ap, fmt);
  vsnprintf(str + strlen(str), sizeof str - strlen(str), fmt, ap);
  va_end(ap);

  snprintf(str + strlen(str), sizeof str - strlen(str), " (pc = 0x%lx)", zassert_pc);
  
  die("%s", str);
}
#endif

void warning(const char *fmt, ...)
{
  va_list ap;
  char str[1024];

  va_start(ap, fmt);
  vsnprintf(str, sizeof str, fmt, ap);
  va_end(ap);

  show_message("WARNING: %s", str);
}

void die(const char *fmt, ...)
{
  va_list ap;
  char str[1024];

  va_start(ap, fmt);
  vsnprintf(str, sizeof str, fmt, ap);
  va_end(ap);

  show_message("%s", str);

#ifdef ZTERP_GLK
#ifdef GARGLK
  fprintf(stderr, "%s\n", str);
#endif
  glk_exit();
#endif

  exit(1);
}

/* This is not POSIX compliant, but it gets the job done.
 * It should not be called more than once.
 */
static int zoptind = 0;
static const char *zoptarg;
static int zgetopt(int argc, char **argv, const char *optstring)
{
  static const char *p = "";
  const char *optp;
  int c;

  if(*p == 0)
  {
    /* No more arguments. */
    if(++zoptind >= argc) return -1;

    p = argv[zoptind];

    /* No more options. */
    if(p[0] != '-' || p[1] == 0) return -1;

    /* Handle “--” */
    if(*++p == '-')
    {
      zoptind++;
      return -1;
    }
  }

  c = *p++;

  optp = strchr(optstring, c);
  if(optp == NULL) return '?';

  if(optp[1] == ':')
  {
    if(*p != 0) zoptarg = p;
    else        zoptarg = argv[++zoptind];

    p = "";
    if(zoptarg == NULL) return '?';
  }

  return c;
}

char *xstrdup(const char *s)
{
  size_t n;
  char *r;

  n = strlen(s) + 1;

  r = malloc(n);
  if(r != NULL) memcpy(r, s, n);

  return r;
}

int process_arguments(int argc, char **argv)
{
  int c;

  while( (c = zgetopt(argc, argv, "a:A:cCdeE:fgGiklLn:N:rR:sS:tT:u:UvxXyz:")) != -1 )
  {
    switch(c)
    {
      case 'a':
        options.eval_stack_size = strtol(zoptarg, NULL, 10);
        break;
      case 'A':
        options.call_stack_size = strtol(zoptarg, NULL, 10);
        break;
      case 'c':
        options.disable_color = 1;
        break;
      case 'C':
        options.disable_config = 1;
        break;
      case 'd':
        options.disable_timed = 1;
        break;
      case 'e':
        options.enable_escape = 1;
        break;
      case 'E':
        options.escape_string = xstrdup(zoptarg);
        break;
      case 'f':
        options.disable_fixed = 1;
        break;
      case 'g':
        options.disable_graphics_font = 1;
        break;
      case 'G':
        options.enable_alt_graphics = 1;
        break;
      case 'i':
        options.show_id = 1;
        break;
      case 'k':
        options.disable_term_keys = 1;
        break;
      case 'l':
        options.disable_utf8 = 1;
        break;
      case 'L':
        options.force_utf8 = 1;
        break;
      case 'n':
        options.int_number = strtol(zoptarg, NULL, 10);
        break;
      case 'N':
        options.int_version = zoptarg[0];
        break;
      case 'r':
        options.replay_on = 1;
        break;
      case 'R':
        options.replay_name = xstrdup(zoptarg);
        break;
      case 's':
        options.script_on = 1;
        break;
      case 'S':
        options.script_name = xstrdup(zoptarg);
        break;
      case 't':
        options.transcript_on = 1;
        break;
      case 'T':
        options.transcript_name = xstrdup(zoptarg);
        break;
      case 'u':
        options.max_saves = strtol(zoptarg, NULL, 10);
        break;
      case 'U':
        options.disable_undo_compression = 1;
        break;
      case 'v':
        options.show_version = 1;
        break;
      case 'x':
        options.disable_abbreviations = 1;
        break;
      case 'X':
        options.enable_censorship = 1;
        break;
      case 'y':
        options.overwrite_transcript = 1;
        break;
      case 'z':
        options.random_seed = strtol(zoptarg, NULL, 10);
        break;
      default:
        return 0;
    }
  }

  /* Just ignore excess stories for now. */
  if(zoptind < argc) game_file = argv[zoptind];

  return 1;
}
