/*----------------------------------------------------------------------*\

  args - Argument handling for arun

  Handles the various startup methods on all machines.

  Main function args() will set up global variable adventureName and the flags,
  the terminal will also be set up and connected.

\*----------------------------------------------------------------------*/

#include "sysdep.h"
#include "args.h"

#include "main.h"

#ifdef HAVE_GLK
#include "glk.h"
#include "glkio.h"
#endif

#ifdef __windows__
#include <windows.h>
#endif


/*======================================================================*/
char *gameName(char *fullPathName) {
  char *foundGameName = "";

  if (fullPathName != NULL) {
    foundGameName = strdup(baseNameStart(fullPathName));
    foundGameName[strlen(foundGameName)-4] = '\0'; /* Strip off .A3C */
  }
  return foundGameName;
}


/*----------------------------------------------------------------------*/
static void switches(int argc, char *argv[])
{
  int i;
  
  for (i = 1; i < argc; i++) {

    if (argv[i][0] == '-') {
#ifdef HAVE_GLK
      switch (glk_char_to_lower(argv[i][1]))
#else
      switch (tolower(argv[i][1]))
#endif
      {
      case 'i':
	ignoreErrorOption = TRUE;
	break;
      case 't':
	sectionTraceOption = TRUE;
	switch (argv[i][2]) {
	case '5' : traceStackOption = TRUE;
	case '4' : tracePushOption = TRUE;
	case '3' : singleStepOption = TRUE;
	case '2' : traceSourceOption = TRUE;
	case '\0':
	case '1': sectionTraceOption = TRUE;
	}
	break;
      case 'd':
	debugOption = TRUE;
	break;
      case 'l':
	transcriptOption = TRUE;
	logOption = FALSE;
	break;
      case 'v':
	verbose = TRUE;
	break;
      case 'n':
	statusLineOption = FALSE;
	break;
      case 'c':
	logOption = TRUE;
	transcriptOption = FALSE;
	break;
      case 'r':
	regressionTestOption = TRUE;
	break;
      default:
	printf("Unrecognized switch, -%c\n", argv[i][1]);
	usage();
	terminate(0);
      }
    } else {

      if (argv[i][0] == '"' && strlen(argv[i]) > 2) {
	/* Probably quoting names including spaces... */
	char *str = strdup(&argv[i][1]);
	adventureFileName = str;
	adventureFileName[strlen(adventureFileName)-1] = '\0';
      } else
	adventureFileName = strdup(argv[i]);

      if (!compareStrings(&adventureFileName[strlen(adventureFileName)-4],
			  ACODEEXTENSION) == 0) {
	adventureFileName = realloc(adventureFileName, strlen(adventureFileName)+5);
	strcat(adventureFileName, ACODEEXTENSION);
      }

      adventureName = gameName(adventureFileName);

    }
  }
}


/*----------------------------------------------------------------------*/
static Bool differentInterpreterName(char *string) {
  return strcasecmp(string, PROGNAME) != 0;
}


/*======================================================================*/
#if defined(__dos__) || defined(__windows__) || defined(__cygwin__)
#include "winargs.c"
#else
#if defined(__unix__) || defined(__macosx__) || defined(__APPLE__)
#include "unixargs.c"
#else
#ifdef __mac__
#include "macargs.c"
#else
/***********************************************************************\

   UNIMPLEMENTED OS

\***********************************************************************/
#endif
#endif
#endif
