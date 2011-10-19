/*----------------------------------------------------------------------* \

	arun.c

	Main program for interpreter for ALAN Adventure Language

\*----------------------------------------------------------------------*/
#include <locale.h>

#include "main.h"
#include "term.h"
#include "options.h"
#include "utils.h"
#include "memory.h"
#include "output.h"
#include "args.h"

#include "alan.version.h"

#ifdef HAVE_GLK
#include "glkio.h"
#ifdef HAVE_WINGLK
#include "WinGlk.h"
#else
#include "glk.h"
#endif
#endif


#ifdef DMALLOC
#include "dmalloc.h"
#endif

/*======================================================================

main()

Main program of main unit in Alan interpreter module, ARUN

*/

#ifdef HAVE_GLK
void glk_main(void)
#else
int main(int argc, char *argv[])
#endif
{
#ifdef DMALLOC
    /*
     * Get environ variable DMALLOC_OPTIONS and pass the settings string
     * on to dmalloc_debug_setup to setup the dmalloc debugging flags.
     */
    //dmalloc_debug_setup(getenv("DMALLOC_OPTIONS"));
#endif

    /* Pick up any locale settings */
    setlocale(LC_ALL, "");

    /* Set up page format in case we get a system error */
    lin = col = 1;
    header->pageLength = 24;
    header->pageWidth = 70;

    getPageSize();

#ifdef HAVE_GLK
    /* args() is called from glkstart.c */
#else
    args(argc, argv);

    if (adventureFileName == NULL || strcmp(adventureFileName, "") == 0) {
	printf("You should supply a game file to play.\n");
	usage(PROGNAME);
	terminate(0);
    }
#endif

    if ((debugOption && !regressionTestOption) || verboseOption) {
	if (debugOption) printf("<");
	printVersion(BUILD);
	if (debugOption) printf(">");
	newline();
	newline();
    }

#ifdef HAVE_GARGLK
    garglk_set_story_name(adventureName);
#endif

    run();

#ifdef HAVE_GLK
    return;
#else
    return(EXIT_SUCCESS);
#endif
}

