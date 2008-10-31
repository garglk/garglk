/*----------------------------------------------------------------------*\

  amigaargs - Argument handling for arun on Amiga

  Handles the various startup methods on all machines.

  Main function args() will set up global variables adventureName,
  adventureFileName and the flags, the terminal will also be set up
  and connected if necessary.

  WARNING! This has not been tested, or even compiled, for a very long
  time...

\*----------------------------------------------------------------------*/

#include "sysdep.h"
#include "args.h"
#include "main.h"

#include <libraries/dosextens.h>

#ifdef AZTEC_C
struct FileHandle *con = NULL;
#else
/* Geek Gadgets GCC */
BPTR window;
BPTR cd;
extern struct WBStartup *_WBenchMsg; /* From libnix */
#endif

#ifdef HAVE_GLK
#include "glk.h"
#include "glkio.h"
#endif

/*----------------------------------------------------------------------*/
static Bool differentInterpreterName(char *string) {
  return stricmp(prgnam, PROGNAME) != 0;
}


#include <intuition/intuition.h>
#include <workbench/workbench.h>

#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/icon_protos.h>

#include <fcntl.h>

extern struct Library *IconBase;

#ifndef AZTEC_C
/* Actually Geek Gadgets GCC with libnix */

/* Aztec C has its own pre-main wbparse which was used in Arun 2.7, with GCC we
   need to do it ourselves. */

#include <clib/intuition_protos.h>

extern unsigned long *__stdfiledes; /* The libnix standard I/O file descriptors */

void
wb_parse(void)
{
  char *cp;
  struct DiskObject *dop;
  struct FileHandle *fhp;

  if (_WBenchMsg->sm_NumArgs == 1) /* If no argument use program icon/info */
    dop = GetDiskObject((UBYTE *)_WBenchMsg->sm_ArgList[0].wa_Name);
  else {
    BPTR olddir = CurrentDir(_WBenchMsg->sm_ArgList[1].wa_Lock);
    dop = GetDiskObject((UBYTE *)_WBenchMsg->sm_ArgList[1].wa_Name);
    CurrentDir(olddir);
  }
  if (dop != 0 && (cp = (char *)FindToolType((UBYTE **)dop->do_ToolTypes, 
					     (UBYTE *)"WINDOW")) != NULL)
    ;
  else /* Could not find a WINDOW tool type */
    cp = "CON:10/10/480/160/Arun:Default Window/CLOSE";
  if ((window = Open((UBYTE *)cp, (long)MODE_OLDFILE))) {
    fhp = (struct FileHandle *) ((long)window << 2);
    SetConsoleTask(fhp->fh_Type);
    SelectInput(window);
    SelectOutput(window);
    __stdfiledes[0] = Input();
    __stdfiledes[1] = Output();
  } else
    exit(-1L);
  FreeDiskObject(dop);
}


/*======================================================================*/
void amigaargs(int argc, char * argv[])
{
  char *prgnam;

  if (argc == 0) { /* If started from Workbench get WbArgs : Aztec C & GG GCC */
    struct WBStartup *WBstart;

    if ((IconBase = OpenLibrary("icon.library", 0)) == NULL)
      syserr("Could not open 'icon.library'");
    /* If started from WB normal main is called with argc == 0 and argv = WBstartup message */
    WBstart = (struct WBStartup *)argv;
#ifndef AZTEC_C
    /* Geek Gadgets GCC */
    wb_parse();
#endif
    adventureFileName = prgnam = WBstart->sm_ArgList[0].wa_Name;
    if (WBstart->sm_NumArgs > 0) {
      cd = CurrentDir(DupLock(WBstart->sm_ArgList[1].wa_Lock));
      adventureFileName = WBstart->sm_ArgList[1].wa_Name;
    }
    /* Possibly other tooltypes ... */
  } else {
    /* Started from a CLI */
    if ((prgnam = strrchr(argv[0], '/')) == NULL
	&& (prgnam = strrchr(argv[0], ':')) == NULL)
      prgnam = argv[0];
    else
      prgnam++;
    /* Now look at the switches and arguments */
    switches(argc, argv);
    if (adventureFileName[0] == '\0')
      /* No game given, try program name */
      if (differentInterpreterName(prgnam)) {
	adventureFileName = duplicate(argv[0], strlen(argv[0]) + strlen(ACODEEXTENSION) + 1);
	strcat(adventureFileName, ACODEEXTENSION);
      }
  }
}
