/*-
 * Copyright 2010-2014 Chris Spiegel.
 *
 * This file is part of Bocfel.
 *
 * Bocfel is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version
 * 2 or 3, as published by the Free Software Foundation.
 *
 * Bocfel is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Bocfel.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glk.h>

/* Even on Win32, Gargoyle provides a glkunix startup. */
#if defined(ZTERP_UNIX) || defined(GARGLK)
#include <string.h>

#include <glkstart.h>

#include "util.h"
#include "zterp.h"

glkunix_argumentlist_t glkunix_arguments[] =
{
#include "help.h"
  { "",		glkunix_arg_ValueFollows,	"file to load" },
  { NULL, glkunix_arg_End, NULL }
};

int glkunix_startup_code(glkunix_startup_t *data)
{
  process_arguments(data->argc, data->argv);

#ifdef GARGLK
  garglk_set_program_name("Bocfel");
  if(game_file != NULL)
  {
    char *p = strrchr(game_file, '/');
    garglk_set_story_name(p == NULL ? game_file : p + 1);
  }
#endif

  return 1;
}
#elif defined(ZTERP_WIN32)
#include <windows.h>

#include <WinGlk.h>

#include "util.h"

int InitGlk(unsigned int);

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previnstance, LPSTR cmdline, int cmdshow)
{
  /* This works (with a linker message) under MinGW, but I don’t
   * know if it’s supposed to; I am unfamiliar with how Windows
   * handles command-line arguments.
   */
  extern int __argc;
  extern char **__argv;

  if(!InitGlk(0x00000700)) exit(EXIT_FAILURE);

  process_arguments(__argc, __argv);

  winglk_app_set_name("Bocfel");

  glk_main();
  glk_exit();

  return 0;
}
#else
#error Glk on this platform is not supported.
#endif
