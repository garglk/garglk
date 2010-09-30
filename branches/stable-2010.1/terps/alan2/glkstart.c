/* glkstart.c: Unix-specific startup code
    Adapted for Alan by Joe Mason <jcmason@uwaterloo.ca> 
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

#include "glk.h"
#include "glkstart.h"
#include "glkio.h"
#include "args.h"

#include "alan.version.h"

glkunix_argumentlist_t glkunix_arguments[] = {
  { "-v", glkunix_arg_NoValue, "-v: verbose mode" },
  { "-l", glkunix_arg_NoValue, "-l: log player command and game output" },
  { "-i", glkunix_arg_NoValue, "-i: ignore version and checksum errors" },
  { "-d", glkunix_arg_NoValue, "-d: enter debug mode" },
  { "-t", glkunix_arg_NoValue, "-t: trace game execution" },
  { "-s", glkunix_arg_NoValue, "-s: single instruction trace" },
  { "", glkunix_arg_ValueFollows, "filename: The game file to load." },
  { NULL, glkunix_arg_End, NULL }
};

int glkunix_startup_code(glkunix_startup_t *data)
{
  /* first, open a window for error output */
  glkMainWin = glk_window_open(0, 0, 0, wintype_TextBuffer, 0); 
  if (NULL == glkMainWin)
  {
    printf("FATAL ERROR: Cannot open initial window");
    glk_exit();
  }
  glk_stylehint_set (wintype_TextGrid, style_User1, stylehint_ReverseColor, 1);
  glkStatusWin = glk_window_open(glkMainWin, winmethod_Above |
    winmethod_Fixed, 1, wintype_TextGrid, 0);
  glk_set_window(glkMainWin);
  
  /* now process the command line arguments */
  args(data->argc, data->argv);

  garglk_set_program_name(alan.shortHeader);
  garglk_set_program_info("Alan Interpreter 2.8.6 by Thomas Nilsson\n");

  return TRUE;
}

