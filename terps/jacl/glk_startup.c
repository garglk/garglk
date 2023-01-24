/* glkstart.c: Unix-specific startup code -- sample file.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://www.eblong.com/zarf/glk/index.html

    This is Unix startup code for the simplest possible kind of Glk
    program -- no command-line arguments; no startup files; no nothing.

    Remember, this is a sample file. You should copy it into the Glk
    program you are compiling, and modify it to your needs. This should
    *not* be compiled into the Glk library itself.
*/

#include "jacl.h"
#include "glk.h"
#include "glkstart.h"
#include "language.h"
#include "prototypes.h"
#include <string.h>

int					jpp_error = FALSE;

extern strid_t 		game_stream;
extern char			game_file[];
extern char			temp_buffer[];
extern char			error_buffer[];
extern char			processed_file[];

short int			encrypt;
extern short int	release;

glkunix_startup_t *arguments;

/* THE STREAM FOR OPENING UP THE ARCHIVE CONTAINING GRAPHICS AND SOUND */
strid_t				blorb_stream;

/* PROTOTYPE FOR NEEDED UTILITY FUNCTION */
void create_paths();

glkunix_argumentlist_t glkunix_arguments[] = {
    {"", glkunix_arg_ValueFollows, "filename: The game file to load." },

	{"-noencrypt", glkunix_arg_NoValue, "-noencrypt: Don't encrypt the processed game file."},
	{"-release", glkunix_arg_NoValue, "-release: Don't include the debug libraries in the .j2 file."},
    { NULL, glkunix_arg_End, NULL }
};

int glkunix_startup_code(glkunix_startup_t *data)
{
	int index = 0;

	arguments = data;

#ifdef GARGLK
	sprintf(temp_buffer, "JACL %d.%d.%d", J_VERSION, J_RELEASE, J_BUILD);
    garglk_set_program_name(temp_buffer);
	sprintf(temp_buffer, "JACL %d.%d.%d by Stuart Allen.\n", J_VERSION, J_RELEASE, J_BUILD);
    garglk_set_program_info(temp_buffer);
#endif

	/* YOU CAN PUT OTHER STARTUP CODE IN glkunix_startup_code(). THIS SHOULD
	 * GENERALLY BE LIMITED TO FINDING AND OPENING DATA FILES.  */

	if (arguments->argc == 1) {
		sprintf (error_buffer, "%s^", NO_GAME);
		jpp_error = TRUE;

		/* WE NEED TO RETURN TRUE HERE SO THE INTERPRETER WILL OPEN A
		 * GLK WINDOWS TO DISPLAY THE ERROR MESSAGE IN */
		return (TRUE);
	} else {
		strcpy(temp_buffer, arguments->argv[1]);

		/* THERE IS AT LEAST ONE ARGUMENT, POSSIBLY JUST THE GAME FILE, BUT 
		 * LOOK THROUGH THE LIST FOR ANYTHING THAT NEEDS ACTING ON */
		for (index = 0; index < data->argc; index++) {
			if (!strcmp(*data->argv, "-noencrypt")) {
				encrypt = FALSE;
			} else if (!strcmp(*data->argv, "-release")) {
				release = TRUE;
			}

			/* INCREMENT THE POINTER TO THE NEXT ARGUMENT */
			data->argv++;
		}
	}

	/* SETUP ALL THE EXPECTED PATHS */
	create_paths(temp_buffer);

	/* PREPROCESS THE FILE AND WRITE IT OUT TO THE NEW FILE */
	/* WARNING: THIS FUNCTION USES stdio FUNCTIONS TO CREATE FILES
     * IN SUBDIRECTORIES. IT IS PORTABLE ACROSS MODERN DESKTOPS, IE
	 * WINDOWS, MAC, UNIX ETC, BUT IT'S NOT GLK CODE... */
	if (jpp() == FALSE) {
		jpp_error = TRUE;

		/* WE NEED TO RETURN TRUE HERE SO THE INTERPRETER WILL OPEN A
		 * GLK WINDOWS TO DISPLAY THE ERROR MESSAGE IN */
		return (TRUE);
	}

	/* THIS OPENS AN ARBITRARY FILE, IN READ-ONLY MODE. NOTE THAT THIS FUNCTION
	 * IS *ONLY* AVAILABLE DURING glkunix_startup_code(). IT IS INHERENT
	 * NON-PORTABLE; IT SHOULD NOT AND CANNOT BE CALLED FROM INSIDE 
	 * glk_main() NOTE: The middle argument FALSE indicates a binary file. */
	game_stream = glkunix_stream_open_pathname(processed_file, FALSE, 0);

	if (!game_stream) {
		strcpy (error_buffer, NOT_FOUND);
		jpp_error = TRUE;

		/* WE NEED TO RETURN TRUE HERE SO THE INTERPRETER WILL OPEN A
		 * GLK WINDOWS TO DISPLAY THE ERROR MESSAGE IN */
		return (TRUE);
	}

	/* SET THE LIBRARY'S IDEA OF THE "CURRENT DIRECTORY" FOR THE EXECUTING
	 * PROGRAM. THE ARGUMENT SHOULD BE THE NAME OF A FILE (NOT A DIRECTORY).
	 * WHEN THIS IS SET, fileref_create_by_name() WILL CREATE FILES IN THE SAME
	 * DIRECTORY AS THAT FILE, AND create_by_prompt() WILL BASE DEFAULT 
	 * FILENAMES OFF OF THE FILE. IF THIS IS NOT CALLED, THE LIBRARY WORKS IN 
	 * THE UNIX CURRENT WORKING DIRECTORY, AND PICKS REASONABLE DEFAULT 
	 * DEFAULTS. */
	glkunix_set_base_file(game_file);

    /* Set title of game */
#ifdef GARGLK
    char *s;
    s = strrchr(game_file, '\\');
    if (!s) s = strrchr(game_file, '/');
    garglk_set_story_name(s ? s + 1 : game_file);
#endif

	/* RETURN TRUE ERRORS OR NOT SO THE MAIN WINDOWS CAN BE OPENED AND
	 * ANY ERROR MESSAGE DISPLAYED */
    return (TRUE);
}

