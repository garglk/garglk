#ifndef TERM
#define TERM
/*----------------------------------------------------------------------*\

  term.h

  Header file for terminal functions in ARUN, the Alan interpreter

\*----------------------------------------------------------------------*/


#ifdef _PROTOTYPES_

extern void getPageSize(void);

#else
extern void getPageSize();
#endif /* _PROTOTYPES_ */

#ifdef HAVE_TERMIO

#ifdef __linux__
#include <sys/ioctl.h>
#include <asm/ioctls.h>
#endif

#ifdef __FreeBSD__
#include <sys/ioctl.h>
#endif

#endif /* HAVE_TERMIO */

#endif
