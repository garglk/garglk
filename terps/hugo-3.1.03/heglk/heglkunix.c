/* glkstart.c: Unix-specific startup code -- sample file.
    Designed by Andrew Plotkin <erkyrath@netcom.com>
    http://www.eblong.com/zarf/glk/index.html

    This is Unix startup code for the simplest possible kind of Glk
    program -- no command-line arguments; no startup files; no nothing.

    Remember, this is a sample file. You should copy it into the Glk
    program you are compiling, and modify it to your needs. This should
    *not* be compiled into the Glk library itself.
*/

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "glk.h"
#include "glkstart.h"
#include "heheader.h"

static winid_t errorwin = NULL;

static void glkunix_startup_error (char *fmt, ...)
#ifdef __GNUC__
__attribute__ ((format (printf, 1, 2)))
#endif
;

glkunix_argumentlist_t glkunix_arguments[] = {
  { "", glkunix_arg_ValueFollows, "filename: The game file to load." },
  { NULL, glkunix_arg_End, NULL }
};

/*
 * Using sprintf() to build an error string would be easier, of course, but
 * that is subject to buffer overflow at runtime. This still uses sprintf(),
 * but in a more controlled fashion. (Yes, I know it's probably overkill.)
 */

static void
glkunix_startup_error (char *fmt, ...)
{
  va_list ap;
  char int_buf[20]; /* Hopefully big enough */
  char *p;

  if (errorwin == NULL)
    {
      errorwin = glk_window_open (0, 0, 0, wintype_TextBuffer, 0);
      if (errorwin == NULL)
	return;
    }

  glk_set_window (errorwin);

  va_start (ap, fmt);
  for (p = fmt; *p; p++)
    {
      if (*p == '%')
	{
	  switch (*++p)
	    {
	    case '%':
	      glk_put_char ('%');
	      break;

	    case 's':
	      glk_put_string (va_arg (ap, char *));
	      break;

	    case 'd':
	      sprintf (int_buf, "%d", va_arg (ap, int));
	      glk_put_string (int_buf);
	      break;

	    default:
	      break;
	    }
	}
      else
	glk_put_char (*p);
    }
  va_end (ap);
}
  
int
glkunix_startup_code (glkunix_startup_t *data)
{
  char *base_name;

  if (data->argc != 2)
    {
      base_name = strrchr (data->argv[0], '/');
      base_name = (base_name != NULL) ? base_name + 1 : data->argv[0];
      glkunix_startup_error ("Usage: %s game-file\n", base_name);
      return FALSE;
    }

  game = glkunix_stream_open_pathname (data->argv[1], 0, 0);
  if (game == NULL)
    {
      glkunix_startup_error ("Error: cannot open game file `%s'\n",
			     data->argv[1]);
      return FALSE;
    }

  /* This should not happen, but handle it anyway. */
  if (errorwin != NULL)
    glk_window_close (errorwin, NULL);

  return TRUE;
}
