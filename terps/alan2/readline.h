#ifndef _READLINE_H_
#define _READLINE_H_
/*----------------------------------------------------------------------*\

  readline.h

  Header file for user input, history and editing support

\*----------------------------------------------------------------------*/

#include "types.h"

#define LINELENGTH 80
#define HISTORYLENGTH 20

#ifdef _PROTOTYPES_
extern Boolean readline(char usrbuf[]);

#else
extern Boolean readline();
#endif

#endif
