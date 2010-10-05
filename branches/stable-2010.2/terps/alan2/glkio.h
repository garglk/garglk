#ifndef _GLKIO_H_
#define _GLKIO_H_

/*----------------------------------------------------------------------*\

  glkio.c

  Header file for Glk output for Alan interpreter

\*----------------------------------------------------------------------*/

#include "glk.h"

winid_t glkMainWin;
winid_t glkStatusWin;

/* NB: this header must be included in any file which calls printf() */

#define printf glkio_printf

void glkio_printf(char *, ...);

#endif
