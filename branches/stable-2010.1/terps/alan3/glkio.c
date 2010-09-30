/*----------------------------------------------------------------------*\

  glkio.c

  Glk output for Alan interpreter

\*----------------------------------------------------------------------*/

#include <stdarg.h>
#include <stdio.h>

#include "glk.h"
#include "glkio.h"

void glkio_printf(char *fmt, ...)
{
  va_list argp;
  va_start(argp, fmt);
  if (glkMainWin == NULL)
    /* assume stdio is available in this case only */
    vprintf(fmt, argp);
  else
  {
    char buf[1024];	/* FIXME: buf size should be foolproof */
    vsprintf(buf, fmt, argp);
    glk_put_string(buf);
  }
  va_end(argp);
}

