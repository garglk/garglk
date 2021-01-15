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
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

#include "util.h"
#include "process.h"
#include "screen.h"
#include "unicode.h"
#include "zterp.h"

#ifdef ZTERP_GLK
#include <glk.h>
#endif

#ifndef ZTERP_NO_SAFETY_CHECKS
void assert_fail(const char *fmt, ...)
{
  va_list ap;
  char str[1024];

  va_start(ap, fmt);
  vsnprintf(str, sizeof str, fmt, ap);
  va_end(ap);

  snprintf(str + strlen(str), sizeof str - strlen(str), " (pc = 0x%lx)", current_instruction);

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

  show_message("Warning: %s", str);
}

void die(const char *fmt, ...)
{
  va_list ap;
  char str[1024];

  va_start(ap, fmt);
  vsnprintf(str, sizeof str, fmt, ap);
  va_end(ap);

  show_message("Fatal error: %s", str);

#ifdef ZTERP_GLK
#ifdef GARGLK
  fprintf(stderr, "Fatal error: %s\n", str);
#endif
  glk_exit();
#endif

  exit(EXIT_FAILURE);
}

void help(void)
{
  /* Simulate a glkunix_argumentlist_t structure so help can be shared. */
  const struct
  {
    const char *flag;
    enum { glkunix_arg_NoValue, glkunix_arg_NumberValue, glkunix_arg_ValueFollows } arg;
    const char *description;
  }
  flags[] = {
#include "help.h"
  };

#ifdef ZTERP_GLK
  glk_set_style(style_Preformatted);
#endif

  screen_puts("Usage: bocfel [args] filename");
  for(size_t i = 0; i < sizeof flags / sizeof *flags; i++)
  {
    char line[1024];
    const char *arg;

    switch(flags[i].arg)
    {
      case glkunix_arg_NumberValue:  arg = "number"; break;
      case glkunix_arg_ValueFollows: arg = "string"; break;
      default:                       arg = "";       break;
    }

    snprintf(line, sizeof line, "%s %-12s%s", flags[i].flag, arg, flags[i].description);
    screen_puts(line);
  }
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
  if(r == NULL) die("unable to allocate memory");

  memcpy(r, s, n);

  return r;
}

enum arg_status arg_status = ARG_OK;
void process_arguments(int argc, char **argv)
{
  int c;

  while( (c = zgetopt(argc, argv, "a:A:cCdDeE:fFgGhHikl:mn:N:prR:sS:tT:u:vxXyYz:Z:")) != -1 )
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
        options.disable_color = true;
        break;
      case 'C':
        options.disable_config = true;
        break;
      case 'd':
        options.disable_timed = true;
        break;
      case 'D':
        options.disable_sound = true;
        break;
      case 'e':
        options.enable_escape = true;
        break;
      case 'E':
        options.escape_string = xstrdup(zoptarg);
        break;
      case 'f':
        options.disable_fixed = true;
        break;
      case 'F':
        options.assume_fixed = true;
        break;
      case 'g':
        options.disable_graphics_font = true;
        break;
      case 'G':
        options.enable_alt_graphics = true;
        break;
      case 'h':
        arg_status = ARG_HELP;
        return;
      case 'H':
        options.disable_history_plaback = true;
        break;
      case 'i':
        options.show_id = true;
        break;
      case 'k':
        options.disable_term_keys = true;
        break;
      case 'l':
        options.username = xstrdup(zoptarg);
        break;
      case 'm':
        options.disable_meta_commands = true;
        break;
      case 'n':
        options.int_number = strtol(zoptarg, NULL, 10);
        break;
      case 'N':
        options.int_version = zoptarg[0];
        break;
      case 'p':
        options.disable_patches = true;
        break;
      case 'r':
        options.replay_on = true;
        break;
      case 'R':
        options.replay_name = xstrdup(zoptarg);
        break;
      case 's':
        options.record_on = true;
        break;
      case 'S':
        options.record_name = xstrdup(zoptarg);
        break;
      case 't':
        options.transcript_on = true;
        break;
      case 'T':
        options.transcript_name = xstrdup(zoptarg);
        break;
      case 'u':
        options.max_saves = strtol(zoptarg, NULL, 10);
        break;
      case 'v':
        options.show_version = true;
        break;
      case 'x':
        options.disable_abbreviations = true;
        break;
      case 'X':
        options.enable_censorship = true;
        break;
      case 'y':
        options.overwrite_transcript = true;
        break;
      case 'Y':
        options.override_undo = true;
        break;
      case 'z':
        options.random_seed = strtol(zoptarg, NULL, 10);
        break;
      case 'Z':
        options.random_device = xstrdup(zoptarg);
        break;
      default:
        arg_status = ARG_FAIL;
        return;
    }
  }

  /* Just ignore excess stories for now. */
  if(zoptind < argc) game_file = argv[zoptind];
}
