/*
	-------------------------------------------
	HUGO ENGINE by Kent Tessman
	Copyright (c) 1995-2006

	The General Coffee Company Film Productions
	-------------------------------------------

	This source code is provided to allow porting of the Hugo Engine
	to different operating systems.  It may be distributed as is,
	providing that there has been no modification or alteration of
	the material contained therein; distribution of modified source code
	is prohibited by the copyright holder.
*/

#include "heheader.h"

/* Library/engine globals */
const int object = 0;
const int xobject = 1;
const int self = 2;
const int wordcount = 3;
const int player = 4;
const int actor = 5;
const int location = 6;
const int verbroutine = 7;
const int endflag = 8;
const int prompt = 9;
const int objectcount = 10;
const int system_status = 11;

/* Library/engine properties */
const int before = 1;
const int after = 2;
const int noun = 3;
const int adjective = 4;
const int article = 5;

/* "display" object properties */
const int screenwidth = 1;
const int screenheight = 2;
const int linelength = 3;
const int windowlines = 4;
const int cursor_column = 5;
const int cursor_row = 6;
const int hasgraphics = 7;
const int title_caption = 8;
const int hasvideo = 9;
const int needs_repaint = 10;
const int pointer_x = 11;
const int pointer_y = 12;

/* address_scale refers to the factor by which addresses are multiplied to
   get the "real" address.  In this way, a 16-bit integer can reference
   64K * 16 = 1024K of memory.
*/
int address_scale = 16;

static char **my_argv = NULL;
char program_path[MAXPATH] = "";

void MakeProgramPath(char *path);


/* MAIN

	If FRONT_END is defined, the external main() function can pass the
   	calling parameters argc and argv to he_main.
*/

#if !defined (FRONT_END)
int main(int argc, char *argv[])
#else
int he_main(int argc, char *argv[])
#endif
{
	time_t seed;

	my_argv = argv;

	if (!strcmp(program_path, "") && argv) MakeProgramPath(argv[0]);

	/* Seed the random number generator */
#if !defined (RANDOM)
	srand((unsigned int)time((time_t *)&seed));
#else
	SRANDOM((unsigned int)time((time_t *)&seed));
#endif

#if !defined (GLK)	/* no command line under Glk */
	ParseCommandLine(argc, argv);
#endif

	hugo_init_screen();

#if defined (DEBUGGER)
	debug_getinvocationpath(argv[0]);
	SwitchtoGame();
#endif
	SetupDisplay();

	strcpy(pbuffer, "");

	gameseg = 0;

	LoadGame();

#if defined (DEBUGGER)
	LoadDebuggableFile();
	StartDebugger();
#endif

	RunGame();

	hugo_cleanup_screen();

	hugo_blockfree(mem);
	mem = NULL;
	hugo_closefiles();

	return 0;
}


void Banner(void)
{
	printf("HUGO %s v%d.%d%s by Kent Tessman (c) 1995-2006\n",
#if defined (DEBUGGER)
	"DEBUGGER",
#else
	"ENGINE",
#endif
	HEVERSION, HEREVISION, HEINTERIM);

	printf("The General Coffee Company Film Productions\n");
#if defined (PORT_NAME)
	printf("%s port by %s\n", PORT_NAME, PORTER_NAME);
#endif
	printf("SYNTAX:  %s filename[%s]\n", my_argv?my_argv[0]:PROGRAM_NAME,
#if defined (DEBUGGER)
	".HDX");
#else
	".HEX");
#endif
}

void MakeProgramPath(char *path)
{
#ifndef GLK
	char drive[MAXDRIVE], dir[MAXDIR], fname[MAXFILENAME], ext[MAXEXT];

	hugo_splitpath(path, drive, dir, fname, ext);
	hugo_makepath(program_path, drive, dir, "", "");
#endif
}
