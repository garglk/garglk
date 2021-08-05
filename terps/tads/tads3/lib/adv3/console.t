#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library - console input/output manager
 *   
 *   This module defines the low-level functions for handling input and
 *   output via the traditional interpreter's user interface, using the
 *   local keyboard and console via the "tads-io" function set.
 *   
 *   The functions in this module are designed primarily for internal use
 *   within the library itself.  Games should use the higher level objects
 *   and functions defined in input.t and output.t instead of directly
 *   calling the functions defined here.  The reason for separating these
 *   functions is so that we can substitute the Web UI versions for games
 *   that wish to use the Web UI insetad of the traditional console UI.  
 */

/* include the library header */
#include "adv3.h"


/* ------------------------------------------------------------------------ */
/*
 *   Initialize the user interface.  The library calls this once at the
 *   start of the interpreter session to set up the UI.  For the console
 *   interpreter, we don't need to do anything here; the interpreter takes
 *   care of setting up the display window for us.  
 */
initUI()
{
}

/*
 *   Initialize the display.  The library calls this at the start of the
 *   game, and after each RESTART, to set up the layout of the game window.
 */
initDisplay()
{
    /* set the interpreter window title */
    gameMain.setGameTitle();

    /* set up the ABOUT box */
    gameMain.setAboutBox();
}

/*
 *   Shut down the user interface.  The library calls this once just before
 *   the game is about to terminate.  
 */
terminateUI()
{
    /* we don't need to do any work to close the display */
}

/* ------------------------------------------------------------------------ */
/*
 *   Check to see if we're in HTML mode 
 */
checkHtmlMode()
{
    /*
     *   The HTML mode depends on the interpreter's capabilities.  If this
     *   is a full multimedia interpreter - i.e., HTML-class - then we
     *   operate in HTML mode.  Otherwise we operate in plain text mode.
     *   
     *   Note that the text-mode interpreters actually do interpret HTML in
     *   the output stream, but most markups don't do anything in text
     *   mode.  Knowing the mode is thus useful in a couple of ways.  In
     *   some cases we simply avoid the overhead of generating a bunch of
     *   HTML that will just be ignored.  In other cases, we display
     *   something a little differently in text mode, to compensate for the
     *   lack of the effect we use in HTML mode.  
     */
    return systemInfo(SysInfoInterpClass) == SysInfoIClassHTML;
}

/* ------------------------------------------------------------------------ */
/*
 *   Write text to the main game window
 */
aioSay(txt)
{
    /* call the interpreter's console output writer */
    tadsSay(txt);
}

/* ------------------------------------------------------------------------ */
/*
 *   Get a line of input from the keyboard, with timeout 
 */
aioInputLineTimeout(timeout)
{
    /* call the interpreter's console input reader */
    return inputLineTimeout(timeout);
}

/*
 *   Cancel a suspended input line 
 */
aioInputLineCancel(reset)
{
    /* call the interpreter's console input line handler */
    inputLineCancel(reset);
}


/* ------------------------------------------------------------------------ */
/*
 *   Read an input event 
 */
aioInputEvent(timeout)
{
    /* call the interpreter's console event reader */
    return inputEvent(timeout);
}


/* ------------------------------------------------------------------------ */
/*
 *   Show a "More" prompt 
 */
aioMorePrompt()
{
    /* call the interpreter's console More prompt generator */
    morePrompt();
}


/* ------------------------------------------------------------------------ */
/*
 *   Show a file selector dialog 
 */
aioInputFile(prompt, dialogType, fileType, flags)
{
    /* call the interpreter's console file dialog handler */
    return inputFile(prompt, dialogType, fileType, flags);
}

/* ------------------------------------------------------------------------ */
/*
 *   Show an input dialog 
 */
aioInputDialog(icon, prompt, buttons, defaultButton, cancelButton)
{
    /* call the interpreter's native input dialog handler */
    return inputDialog(icon, prompt, buttons, defaultButton, cancelButton);
}

/* ------------------------------------------------------------------------ */
/*
 *   Set/remove the output logging file
 */
aioSetLogFile(fname, logType?)
{
    /* set the log file in the interpreter console */
    return setLogFile(fname, logType);
}

/* ------------------------------------------------------------------------ */
/*
 *   Clear the screen 
 */
aioClearScreen()
{
    /* clear the local interpreter console screen */
    clearScreen();

    /* re-initialize any <ABOUTBOX> tag */
    gameMain.setAboutBox();
}

/* ------------------------------------------------------------------------ */
/* 
 *   Generate a string to show hyperlinked text.  If we're not in HTML
 *   mode, we'll simply return the text without the hyperlink; otherwise,
 *   we'll return the text with a hyperlink to the given HREF.  
 *   
 *   If the display text is included, we'll generate the entire link,
 *   including the <A HREF> tag, the hyperlinked text contents, and the
 *   </A> end tag.  If the text is omitted, we'll simply generate the <A
 *   HREF> tag itself, leaving it to the caller to display the text and the
 *   </A>.
 *   
 *   The optional 'flags' is a combination of AHREF_xxx flags indicating
 *   any special properties of the hyperlink.  
 */
aHref(href, txt?, title?, flags = 0)
{
    /* check for HTML mode */
    if (outputManager.htmlMode)
    {
        /* figure the <a> properties based on the flags */
        local props = '';
        if ((flags & AHREF_Plain) != 0)
            props += 'plain ';

        /* 
         *   We're in HTML mode - generate a <a> tag enclosing the text.
         *   If there's text, include the text and </a> end tag, otherwise
         *   just show the <a> tag itself.  
         */
        return '<a <<props>> href="<<href>>"<<
            (title != nil ? ' title="' + title + '"' : '')
            >>><.a><<
            (txt != nil ? txt + '<./a></a>' : '')>>';
    }
    else
    {
        /* plain text mode - just return the text unchanged */
        return txt;
    }
}


/* ------------------------------------------------------------------------ */
/* 
 *   Generate a string to show hyperlinked text, with alternate text if
 *   we're not in HTML mode.  If we're in HTML mode, we'll return
 *   linkedTxt linked to the given HREF; if we're in plain text mode,
 *   we'll return the alternate text as-is.  
 */
aHrefAlt(href, linkedText, altText, title?)
{
    /* check HTML mode */
    if (outputManager.htmlMode)
    {
        /* we're in HTML mode - generate an <A> tag for the linked text */
        return '<a href="<<href>>"<<
               (title != nil ? ' title="' + title + '"' : '')
               >>><.a><<linkedText>><./a></a>';
    }
    else
    {
        /* plain text mode - just return the alternate text */
        return altText;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Generate HTML to wrap the left/right portions of the status line.  The
 *   basic status line has three stages: stage 0 precedes the left portion,
 *   stage 1 comes between the left and right portions, and stage 2 follows
 *   the right portion.  If we're listing exits, we get two more stages:
 *   stage 3 precedes the exit listing, stage 4 follows it.  
 */
statusHTML(stage)
{
    switch (stage)
    {
    case 1:
        /* return the right-alignment tab between the two sections */
        return '<tab align=right>';

    case 3:
    case 4:
        /* show a line break before and after the exit listing */
        return '<br>';

    default:
        /* other stages don't require any special text */
        return '';
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   The banner window for the status line.  
 */
statuslineBanner: BannerWindow
    /* close the window */
    removeBanner()
    {
        /* remove the banner */
        inherited();

        /* tell the statusLine object to refigure the display mode */
        statusLine.statusDispMode = nil;
    }

    /* initialize */
    initBannerWindow()
    {
        /* if we're already initialized, do nothing */
        if (inited_)
            return;

        /* inherit the default handling (to set our 'inited_' flag) */
        inherited();
        
        /* tell the status line to initialize its banner window */
        statusLine.initBannerWindow(self);
    }

    /* 
     *   Set the color scheme.  We simply show a <BODY> tag that selects
     *   the parameterized colors STATUSBG and STATUSTEXT.  (These are
     *   called "parameterized" colors because they don't select specific
     *   colors, but rather select whatever colors the interpreter wishes
     *   to use for the status line.  In many cases, the interpreter lets
     *   the user select these colors via a Preferences dialog.)  
     */
    setColorScheme()
    {
        /* set up the interpreter's standard status line colors */
        "<body bgcolor=statusbg text=statustext>";
    }
;

