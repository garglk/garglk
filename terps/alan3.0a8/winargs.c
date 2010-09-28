/*----------------------------------------------------------------------*\

  winargs - Argument handling for arun on DOS, WINDOWS and CYGWIN

  This file is included in args.c when compiling for DOS, WINDOWS or CYGWIN

  Handles the various startup methods on all machines.

  Main function args() will set up global variables adventureName,
  adventureFileName and the flags, the terminal will also be set up
  and connected if necessary.

\*----------------------------------------------------------------------*/

#ifdef __windows__
#include <windows.h>
#endif


/*======================================================================*/
void args(int argc, char * argv[])
{
  char *programName;
  char *exePoint;

#ifdef ARGSDISPLAY
  int i;

  MessageBox(NULL, "Hello!", "Windows Arun interpreter", MB_OK);
  MessageBox(NULL, GetCommandLine(), "", MB_OK);
  for (i = 0; i < argc; i++) {
    char buf[199];
    sprintf(buf, "arg %d :\"%s\"", i, argv[i]);
    MessageBox(NULL, buf, "Alan V3 compiler", MB_OK);
  }
#endif

#ifdef HAVE_WINGLK
  argv[0] = GetCommandLine();
#endif
  if ((programName = strrchr(argv[0], '\\')) == NULL
      && (programName = strrchr(argv[0], '/')) == NULL
      && (programName = strrchr(argv[0], ':')) == NULL)
    programName = strdup(argv[0]);
  else
    programName = strdup(&programName[1]);

  if (strlen(programName) > 4 && (((exePoint = strstr(programName, ".EXE")) != NULL) || (exePoint = strstr(programName, ".exe")) != NULL))
    *exePoint = '\0';

  /* Now look at the switches and arguments */
  switches(argc, argv);

#ifdef ARGSDISPLAY
  {
    char buf[100];
    sprintf(buf, "programName = '%s'\nadventureFileName = '%s'", programName, adventureFileName);
    MessageBox(NULL, buf, "Alan V3 compiler", MB_OK);
  }
#endif

  // TODO This is the same as in unixargs.c
  if (adventureFileName == NULL) {
    /* No game given, try program name */
    if (differentInterpreterName(programName)) {
      // TODO break out as a function
      FILE *adventureFile;
      adventureFileName = duplicate(programName,
				    strlen(programName)
				    +strlen(ACODEEXTENSION)+1);
      strcat(adventureFileName, ACODEEXTENSION);
      // TODO break out as utils::fileExists()
      if ((adventureFile = fopen(adventureFileName, "r")) == NULL) {
	free(adventureFileName);
	adventureFileName = NULL;
      } else
	fclose(adventureFile);
    }
  }
  adventureName = gameName(adventureFileName);
  free(programName);
}
