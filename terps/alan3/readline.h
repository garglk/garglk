#ifndef _READLINE_H_
#define _READLINE_H_
/*----------------------------------------------------------------------*\

  readline.h

  Header file for user input, history and editing support

\*----------------------------------------------------------------------*/

#include "types.h"

#define HISTORYLENGTH 20

extern bool readline(char usrbuf[]);

#endif
