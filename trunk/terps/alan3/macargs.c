/*----------------------------------------------------------------------*\

  macargs - Argument handling for arun on mac pre-OSX

  This file is included in args.c when compiling for Mac

  Handles the various startup methods on all machines.

  Main function args() will set up global variables adventureName,
  adventureFileName and the flags, the terminal will also be set up
  and connected if necessary.

  WARNING! This has not been tested, or even compiled, for a very long
  time...

\*----------------------------------------------------------------------*/

#include "macArgs.h"

/*======================================================================*/
void args(int argc, char * argv[])
{
  char *prgnam;

#include <console.h>
#ifdef __MWERKS__
#include <SIOUX.h>
#endif
  short msg, files;
  static char advbuf[256], prgbuf[256];
  /*AppFile af;*/
  OSErr oe;

#ifdef __MWERKS__
  /*SIOUXSettings.setupmenus = FALSE;*/
  SIOUXSettings.autocloseonquit = FALSE;
  SIOUXSettings.asktosaveonclose = FALSE;
  SIOUXSettings.showstatusline = FALSE;
#endif

  GetMacArgs(advbuf);
  adventureFileName = advbuf;
  adventureName = advbuf;

  /* MISSING: handling of a renamed Arun executable, then look for a
     game file with the same name */
}
