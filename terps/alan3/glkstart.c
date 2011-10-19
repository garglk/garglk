/* glkstart.c: Unix-specific startup code
   Adapted for Alan by Joe Mason <jcmason@uwaterloo.ca>
   And tweaked by Thomas Nilsson <thomas@alanif.se>

   Based on the sample file designed by
   Andrew Plotkin <erkyrath@netcom.com>
   http://www.eblong.com/zarf/glk/index.html

   Original release note follows:

   This is Unix startup code for the simplest possible kind of Glk
   program -- no command-line arguments; no startup files; no nothing.

   Remember, this is a sample file. You should copy it into the Glk
   program you are compiling, and modify it to your needs. This should
   *not* be compiled into the Glk library itself.
*/

#include "args.h"
#include "main.h"
#include "glk.h"
#include "glkstart.h"
#include "glkio.h"
#include "resources.h"
#include "gi_blorb.h"
#include "utils.h"

#ifdef HAVE_WINGLK
#include "WinGlk.h"
#include <windows.h>
#endif


#ifdef HAVE_GARGLK
#include "alan.version.h"
#endif

glkunix_argumentlist_t glkunix_arguments[] = {
    { "-l", glkunix_arg_NoValue, "-l: log player command and game output" },
    { "-c", glkunix_arg_NoValue, "-c: log player commands to a file" },
    { "-n", glkunix_arg_NoValue, "-n: no status line" },
    { "-i", glkunix_arg_NoValue, "-i: ignore version and checksum errors" },
    { "-d", glkunix_arg_NoValue, "-d: enter debug mode" },
    { "-t", glkunix_arg_NoValue, "-t [<n>]: trace game execution, higher <n> gives more trace" },
    { "-r", glkunix_arg_NoValue, "-r: refrain from printing timestamps and paging (making regression testing easier)" },
    { "", glkunix_arg_ValueFollows, "filename: The game file to load." },
    { NULL, glkunix_arg_End, NULL }
};

/* Resources */
static strid_t resourceFile;

/*----------------------------------------------------------------------*/
static void openGlkWindows() {
    glkMainWin = glk_window_open(0, 0, 0, wintype_TextBuffer, 0);
    if (glkMainWin == NULL) {
            printf("FATAL ERROR: Cannot open initial window");
            glk_exit();
    }
#ifdef HAVE_GARGLK
   glk_stylehint_set (wintype_TextGrid, style_User1, stylehint_ReverseColor, 1);
#endif
    glkStatusWin = glk_window_open(glkMainWin, winmethod_Above |
                                   winmethod_Fixed, 1, wintype_TextGrid, 0);
    glk_set_window(glkStatusWin);
    glk_set_style(style_Preformatted);
    glk_set_window(glkMainWin);
}

/*----------------------------------------------------------------------*/
static void openResourceFile() {
    char *originalFileName = strdup(adventureFileName);
	char *resourceFileName = originalFileName;
    char *extension = strrchr(resourceFileName, '.');
    frefid_t resourceFileRef;
    giblorb_err_t ecode;

#ifdef HAVE_GARGLK
	if (strrchr(resourceFileName, '/'))
		resourceFileName = strrchr(resourceFileName, '/') + 1;
	else if (strrchr(resourceFileName, '\\'))
		resourceFileName = strrchr(resourceFileName, '\\') + 1;
	if (!resourceFileName)
		resourceFileName = originalFileName;

	if (extension)
		strcpy(extension, ".a3r");
	else
		strcat(resourceFileName, ".a3r");
#else
    strcpy(extension, ".a3r");
#endif

#ifdef HAVE_WINGLK
    resourceFileRef = winglk_fileref_create_by_name(fileusage_BinaryMode,
                                                    resourceFileName, 0, FALSE);
#else
    resourceFileRef = glk_fileref_create_by_name(fileusage_BinaryMode,
                                                 resourceFileName, 0);
#endif
    if (glk_fileref_does_file_exist(resourceFileRef)) {
        resourceFile = glk_stream_open_file(resourceFileRef, filemode_Read, 0);
        ecode = giblorb_set_resource_map(resourceFile);
    }
    free(originalFileName);
}


/*======================================================================*/
int glkunix_startup_code(glkunix_startup_t *data)
{
    glk_stylehint_set(wintype_AllTypes, style_Emphasized, stylehint_Weight, 0);
    glk_stylehint_set(wintype_AllTypes, style_Emphasized, stylehint_Oblique, 1);
    glk_stylehint_set(wintype_AllTypes, style_BlockQuote, stylehint_Indentation, 10);

    /* first, open a window for error output */
    openGlkWindows();

#ifdef HAVE_GARGLK
	garglk_set_program_name(alan.shortHeader);
	garglk_set_program_info("Alan Interpreter 3.0 beta 2 by Thomas Nilsson\n");
#endif

    /* now process the command line arguments */
    args(data->argc, data->argv);

    if (adventureFileName == NULL || strcmp(adventureFileName, "") == 0) {
        printf("You should supply a game file to play.\n");
        usage("arun"); // TODO Find real programname from arguments
        terminate(0);
    }

    /* Open any possible blorb resource file */
    openResourceFile();

    return TRUE;
}



static int argCount;
static char *argumentVector[10];

/*----------------------------------------------------------------------*/
static void splitArgs(char *commandLine) {
    unsigned char *cp = commandLine;

    while (*cp) {
        while (*cp && isspace(*cp)) cp++;
        if (*cp) {
            argumentVector[argCount++] = cp;
            if (*cp == '"') {
                do {
                    cp++;
                } while (*cp != '"');
                cp++;
            } else
                while (*cp && !isspace(*cp))
                    cp++;
            if (*cp) {
                *cp = '\0';
                cp++;
            }
        }
    }
}


#ifdef HAVE_WINGLK
/*======================================================================*/
int winglk_startup_code(const char* cmdline)
{
    char windowTitle[200];

    /* Process the command line arguments */
    argumentVector[0] = "";
    argCount = 1;

    splitArgs(strdup(cmdline));

    args(argCount, argumentVector);


    if (adventureFileName == NULL || strcmp(adventureFileName, "") == 0) {
        adventureFileName = (char*)winglk_get_initial_filename(NULL, "Arun : Select an Alan game file",
                                                               "Alan Game Files (*.a3c)|*.a3c||");
        if (adventureFileName == NULL) {
            terminate(0);
        }
        adventureName = gameName(adventureFileName);
    }

    winglk_app_set_name("WinArun");
    winglk_set_gui(IDR_ARUN);

    sprintf(windowTitle, "WinArun : %s", adventureName);
    winglk_window_set_title(windowTitle);
    openGlkWindows();

    /* Open any possible blorb resource file */
    openResourceFile();

    return TRUE;
}
#endif
