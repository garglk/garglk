/*
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  Permission is
 *   granted to anyone to copy and use this file for any purpose.  
 *   
 *   This is a starter TADS 3 source file.  This is a complete TADS 3
 *   program that you can compile and run.
 *   
 *   To compile this game in TADS Workbench, open the "Build" menu and
 *   select "Compile for Debugging."  To run the game, after compiling it,
 *   open the "Debug" menu and select "Go."
 *   
 *   This is the "advanced" starter program - it has only the minimum set
 *   of definitions needed for a working program.  If you would like some
 *   more examples, create a new game, and choose the "introductory"
 *   version when asked for the type of starter game to create.  
 */

/* include the TADS and T3 system headers */
#include <tads.h>
#include <t3.h>

/*
 *   Main program body.  Note that _main() is the true entrypoint into the
 *   program, but _main() calls this routine as soon as it has completed
 *   some set-up operations.  preinit() will always be called (either
 *   during the original compilation, or at program startup, depending on
 *   the compilation mode) before this routine is called.  
 */
main(args)
{
    /* put your main program here */
    "Hello from TADS 3!\n";
}

