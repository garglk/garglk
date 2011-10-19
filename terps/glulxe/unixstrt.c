/* unixstrt.c: Unix-specific code for Glulxe.
    Designed by Andrew Plotkin <erkyrath@eblong.com>
    http://eblong.com/zarf/glulx/index.html
*/

#include "glk.h"
#include "glulxe.h"
#include "glkstart.h" /* This comes with the Glk library. */
#include <string.h>

/* The only command-line argument is the filename. */
glkunix_argumentlist_t glkunix_arguments[] = {
  { "", glkunix_arg_ValueFollows, "filename: The game file to load." },
  { NULL, glkunix_arg_End, NULL }
};

int glkunix_startup_code(glkunix_startup_t *data)
{
  /* It turns out to be more convenient if we return TRUE from here, even 
     when an error occurs, and display an error in glk_main(). */
  char *cx;
  unsigned char buf[12];
  int res;

#ifdef GARGLK
  garglk_set_program_name("Glulxe 0.4.7");
  garglk_set_program_info("Glulxe 0.4.7 by Andrew Plotkin");
#endif

  if (data->argc <= 1) {
    init_err = "You must supply the name of a game file.";
#ifdef GARGLK
    return TRUE; /* Hack! but I want error message in glk window */
#endif
	return FALSE;
  }
  cx = data->argv[1];
    
  gamefile = glkunix_stream_open_pathname(cx, FALSE, 1);
  if (!gamefile) {
    init_err = "The game file could not be opened.";
    init_err2 = cx;
    return TRUE;
  }

#ifdef GARGLK
  cx = strrchr(data->argv[1], '/');
  if (!cx) cx = strrchr(data->argv[1], '\\');
  garglk_set_story_name(cx ? cx + 1 : data->argv[1]);
#endif

  /* Now we have to check to see if it's a Blorb file. */

  glk_stream_set_position(gamefile, 0, seekmode_Start);
  res = glk_get_buffer_stream(gamefile, (char *)buf, 12);
  if (!res) {
    init_err = "The data in this stand-alone game is too short to read.";
    return TRUE;
  }
    
  if (buf[0] == 'G' && buf[1] == 'l' && buf[2] == 'u' && buf[3] == 'l') {
    locate_gamefile(FALSE);
    return TRUE;
  }
  else if (buf[0] == 'F' && buf[1] == 'O' && buf[2] == 'R' && buf[3] == 'M'
    && buf[8] == 'I' && buf[9] == 'F' && buf[10] == 'R' && buf[11] == 'S') {
    locate_gamefile(TRUE);
    return TRUE;
  }
  else {
    init_err = "This is neither a Glulx game file nor a Blorb file "
      "which contains one.";
    return TRUE;
  }
}

