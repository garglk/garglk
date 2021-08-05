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
 *   This is the "introductory" starter program; it provides an example of
 *   how to set up a T3 program.  
 */

/* include the TADS and T3 system headers */
#include <tads.h>
#include <t3.h>

/* main program entrypoint */
_main(args)
{
    try
    {
        /* set up the default output routine */
        t3SetSay(&_say_embed);
        
        /* if we haven't run preinitialization yet, run it now */
        if (!_global._preinited)
            preinit();

        /* if we're not in preinit mode, run the main program */
        if (!t3GetVMPreinitMode())
            main(args);
    }
    catch(RuntimeError exc)
    {
        "Unhandled exception: <<exc.exceptionMessage>>\n";
    }
}

/*
 *   Function to use as the default output routine - we'll just pass
 *   through the output to the TADS tadsSay() built-in 
 */
_say_embed(val) { tadsSay(val); }

/*
 *   this object is a place where we can store some global state
 *   information 
 */
_global: object
    _preinited = nil
;

/*
 *   RuntimeError class - the VM will throw a new instance of this class
 *   each time a run-time error occurs. 
 */
class RuntimeError: object
    construct(errno, ...) { errno_ = errno; }
    exceptionMessage = ''
    errno_ = 0
;

/*
 *   Preinitialization function - this routine can perform any
 *   time-consuming static initialization that can be done immediately
 *   after compilation, so that it doesn't have to be done every time the
 *   program starts up.  
 */
preinit()
{
    /* put your pre-initialization code here */
}

/*
 *   Main program body.  Note that _main() is the true entrypoint into the
 *   program, but _main() calls this routine as soon as it has completed
 *   some set-up operations.  preinit() will always be called (either
 *   during the original compilation, or at program startup, depending on
 *   the compilation mode) before this routine is called.  
 */
main(args)
{
    /* show the VM banner and a blank line */
    tadsSay(t3GetVMBanner()); "\b";

    /* show an introductory message */
    "Hello from TADS 3!  Type any command you like; SCORE will increase
    the score, QUIT will end the program, and everything else will
    be ignored. ";

    /* read commands */
command_loop:
    for (;;)
    {
        local cmd;

        /* increase the turn counter */
        ++global.turncount;

        /* update the status line */
        "<banner id=StatusLine height=previous border>
        <body bgcolor=statusbg text=statustext><b>
        Command Line Room
        </b>
        <tab align=right><i><<global.score>>/<<global.turncount>></i>
        </banner>";

        /* show a prompt and enter input font mode */
        "\b&gt;<font face='TADS-Input'>";

        /* read a command */
        cmd = inputLine();
        
        /* end the input font mode */
        "</font>";
        
        /* remove any leading spaces */
        cmd = rexReplace('^ +', cmd, '', ReplaceOnce);

        /* remove any trailing spaces */
        cmd = rexReplace(' +$', cmd, '', ReplaceOnce);

        /* see what we have */
        switch(cmd.toLower())
        {
        case 'quit':
            break command_loop;

        case 'score':
            "Your score has increased by ten points. ";
            global.score += 10;
            break;

        default:
            "That doesn't seem to work&emdash;yet!!! ";
            break;
        }
    }

    "Thanks for playing!!!\n";
}

/*
 *   'global' - an object where we can store some global state information 
 */
global: object
    score = 0
    turncount = 0
;

