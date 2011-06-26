/*-
 * Copyright 2010 Chris Spiegel.
 *
 * This file is part of Bocfel.
 *
 * Bocfel is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version
 * 2, as published by the Free Software Foundation.
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
  { "-a",	glkunix_arg_NumberValue,	"-a N		set the size of the evaluation stack" },
  { "-A",	glkunix_arg_NumberValue,	"-A N		set the size of the call stack" },
  { "-c",	glkunix_arg_NoValue,		"-c		disable color" },
  { "-C",	glkunix_arg_NoValue,		"-C		disable the use of a config file" },
  { "-d",	glkunix_arg_NoValue,		"-d		disable timed input" },
  { "-D",	glkunix_arg_NoValue,		"-D		disable sound effects" },
  { "-e",	glkunix_arg_NoValue,		"-e		enable ANSI escapes in the transcript" },
  { "-E",	glkunix_arg_ValueFollows,	"-E string	set the escape string for -e" },
  { "-f",	glkunix_arg_NoValue,		"-f		disable fixed-width fonts" },
  { "-F",	glkunix_arg_NoValue,		"-F		assume font is fixed-width" },
  { "-g",	glkunix_arg_NoValue,		"-g		disable the character graphics font" },
  { "-G",	glkunix_arg_NoValue,		"-G		enable alternative box-drawing character graphics" },
  { "-i",	glkunix_arg_NoValue,		"-i		display the id of the story file and exit" },
  { "-k",	glkunix_arg_NoValue,		"-k		disable the use of terminating keys (notably used in Beyond Zork)" },
  { "-l",	glkunix_arg_NoValue,		"-l		disable utf-8 transcripts" },
  { "-L",	glkunix_arg_NoValue,		"-L		force utf-8 transcrips" },
  { "-m",	glkunix_arg_NoValue,		"-m		disable meta commands" },
  { "-n",	glkunix_arg_NumberValue,	"-n N		set the interpreter number (see 11.1.3 in The Z-machine Standards Document 1.0)" },
  { "-N",	glkunix_arg_NumberValue,	"-N N		set the interpreter version (see 11.1.3.1 in The Z-machine Standards Document 1.0)" },
  { "-r",	glkunix_arg_NoValue,		"-r		start the story by replaying a command record" },
  { "-R",	glkunix_arg_NoValue,		"-R filename	set the filename to be used if replaying a command record" },
  { "-s",	glkunix_arg_NoValue,		"-s		start the story with command recording on" },
  { "-S",	glkunix_arg_NoValue,		"-S filename	set the filename to be used if command recording is turned on" },
  { "-t",	glkunix_arg_NoValue,		"-t		start the story with transcripting on" },
  { "-T",	glkunix_arg_ValueFollows,	"-T filename	set the filename to be used if transcription is turned on" },
  { "-u",	glkunix_arg_NumberValue,	"-u N		set the maximum number of undo slots" },
  { "-U",	glkunix_arg_NoValue,		"-U		disable compression in undo slots" },
  { "-v",	glkunix_arg_NoValue,		"-v		display version information" },
  { "-x",	glkunix_arg_NoValue,		"-x		disable expansion of abbreviations" },
  { "-X",	glkunix_arg_NoValue,		"-X		enable tandy censorship" },
  { "-y",	glkunix_arg_NoValue,		"-y		when opening a transcript, overwrite rather than append to an existing file" },
  { "-z",	glkunix_arg_NumberValue,	"-z N		set initial random seed" },
  { "-Z",	glkunix_arg_ValueFollows,	"-Z device	read initial random seed from device" },
  { "",		glkunix_arg_ValueFollows,	"filename	file to load" },

  { NULL, glkunix_arg_End, NULL }
};

int glkunix_startup_code(glkunix_startup_t *data)
{
  if(!process_arguments(data->argc, data->argv)) return 0;

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

  if(!process_arguments(__argc, __argv)) exit(EXIT_FAILURE);

  winglk_app_set_name("Bocfel");

  glk_main();
  glk_exit();

  return 0;
}
#else
#error Glk on this platform is not supported.
#endif
