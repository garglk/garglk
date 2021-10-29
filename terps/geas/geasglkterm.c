/*$Id: //depot/prj/geas/master/code/geasglkterm.c#4 $
  geasglkterm.c

  Bridge file between Geas and GlkTerm.  Only needed if using GlkTerm.

  Copyright (C) 2006 David Jones.  Distribution or modification in any
  form permitted.

  Unix specific (see the call to close()).
*/

#include <stddef.h>

#include <unistd.h>
#include <string.h>

#include "glk.h"
#include "glkstart.h"

const char *storyfilename = NULL;
int use_inputwindow = 0;

glkunix_argumentlist_t glkunix_arguments[] = {
    { "-w", glkunix_arg_NoValue,    "-w:       Use a separate input window."},
    { "", glkunix_arg_ValueFollows, "filename: The game file to load."},
    { NULL, glkunix_arg_End, NULL }
};

int
glkunix_startup_code(glkunix_startup_t *data)
{
  int i = 1;

  if (data->argc > 1 && strcmp (data->argv[i], "-w") == 0) {
      use_inputwindow = 1;
      i++;
  }

#ifdef GARGLK
  garglk_set_program_name("Geas 0.4");
  garglk_set_program_info(
        "Geas 0.4 by Mark Tilford and David Jones.\n"
        "Additional Glk support by Simon Baldwin.");
#endif

  if (i == data->argc - 1) {
      storyfilename = data->argv[i];
#ifdef GARGLK
      char *s = strrchr(storyfilename, '/');
      if (!s) s = strrchr(storyfilename, '\\');
      garglk_set_story_name(s ? s + 1 : storyfilename);
#endif
  }

  return 1;
}
