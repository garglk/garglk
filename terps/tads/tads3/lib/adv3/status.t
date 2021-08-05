#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library - Status Line
 *   
 *   This module defines the framework for displaying the status line,
 *   which is the area conventionally displayed at the top of the screen
 *   showing information such as the current location, score (if scoring is
 *   used at all), and number of turns.  
 */

/* include the library header */
#include "adv3.h"


/* ------------------------------------------------------------------------ */
/* 
 *   In case the 'score' module isn't included, make sure we can refer to
 *   totalScore as a property.  Likewise for the banner API and Web UI
 *   frame API.  
 */
property totalScore;
property showBanner, setSize, sizeToContents;
property flushWin, resize;

/* ------------------------------------------------------------------------ */
/*
 *   A special OutputStream for the <BANNER> tag contents.  This is really
 *   just part of the main output stream, but we use a separate output
 *   stream object so that we have our own separate stream state variables
 *   (for paragraph breaking and so forth).  
 */
transient statusTagOutputStream: OutputStream
    /*
     *   We're really part of the main window's output stream as far as the
     *   underlying interpreter I/O system is concerned, so we have to
     *   coordinate with the main game window's input manager. 
     */
    myInputManager = inputManager

    /* we sit atop the system-level main console output stream */
    writeFromStream(txt)
    {
        /* write the text directly to the main output stream */
        tadsSay(txt);
    }
;

/*
 *   A special OutputStream for the left half of the status line (the
 *   short description area) in text mode.  We use a separate stream for
 *   this because we must write the text using the output mode switching
 *   for the status line.
 *   
 *   We only use this stream when we use the old-style text-mode status
 *   line interface, which explicitly separates the status line into a
 *   left part and a right part.  When we have the banner API available in
 *   the interpreter, we'll use banners instead, since banners give us
 *   much more flexibility.  
 */
transient statusLeftOutputStream: OutputStream
    /* we sit atop the system-level main console output stream */
    writeFromStream(txt)
    {
        /* write the text directly to the main output stream */
        tadsSay(txt);
    }
;

/*
 *   A special OutputStream for the right half of the status line (the
 *   score/turn count area) in text mode.  We use a separate stream for
 *   this because we have to write this text with the special
 *   statusRight() intrinsic in text mode.
 *   
 *   We only use this stream when we use the old-style text-mode status
 *   line interface, which explicitly separates the status line into a
 *   left part and a right part.  When we have the banner API available in
 *   the interpreter, we'll use banners instead, since banners give us
 *   much more flexibility.  
 */
transient statusRightOutputStream: OutputStream
    /*
     *   Write from the stream.  We simply buffer up text until we're
     *   asked to display the final data. 
     */
    writeFromStream(txt)
    {
        /* buffer the text */
        buf_ += txt;
    }

    /*
     *   Flush the buffer.  This writes whatever we've buffered up to the
     *   right half of the text-mode status line. 
     */
    flushStream()
    {
        /* write the text to the system console */
        statusRight(buf_);

        /* we no longer have anything buffered */
        buf_ = '';
    }

    /* our buffered text */
    buf_ = ''
;


/* ------------------------------------------------------------------------ */
/*
 *   Statusline modes.  We have three different ways to display the
 *   statusline, depending on the level of support in the interpreter.
 *   
 *   StatusModeApi - use the banner API.  This is preferred method, because
 *   it gives us uniform capabilities on text and graphical interpreters,
 *   and provides an output stream for the statusline that is fully
 *   independent on the main game window's output stream.
 *   
 *   StatusModeTag - use the <BANNER> tag.  This is the method we must use
 *   for HTML-enabled interpreters that don't support the banner API.  This
 *   gives us the full formatting capabilities of HTML, but isn't as good
 *   as StatusModeApi because we have to share our output stream with the
 *   main game window.
 *   
 *   StatusModeText - use the old-style dedicated statusline in a text-only
 *   interpreter.  This is the least desirable method, because it gives us
 *   a rigid format for the statusline (exactly one line high, no control
 *   over colors, and with the strict left/right bifurcation).  We'll only
 *   use this method if we're on a text-only interpreter that doesn't
 *   support the banner API.
 *   
 *   StatusModeBrowser - use the Web UI.  This is similar to API mode, but
 *   instead of using banner windows we use Web UI windows, which are
 *   implemented as IFRAMEs in the browser.  
 */
enum StatusModeApi, StatusModeTag, StatusModeText, StatusModeBrowser;

/* ------------------------------------------------------------------------ */
/*
 *   Status line - this is an abstract object that controls the status line
 *   display.  
 *   
 *   We provide two main methods: showStatusHtml, which shows the status
 *   line in HTML format, and showStatusText, which shows the status line
 *   in plain text mode.  To display the status line, we invoke one or the
 *   other of these methods, according to the current mode, to display the
 *   statusline.  The default implementations of these methods generate the
 *   appropriate formatting codes for a statusline with a left part and a
 *   right part, calling showStatusLeft and showStatusRight, respectively,
 *   to display the text for the parts.
 *   
 *   Games can customize the statusline at two levels.  At the simpler
 *   level, a game can modify showStatusLeft and/or showStatusRight to
 *   change the text displayed on the left and/or right of the statusline.
 *   Since these two methods are used regardless of the statusline style of
 *   the underlying interpreter, games don't have to worry about the
 *   different modes when overriding these.
 *   
 *   At the more complex level, a game can modify showStatusHtml and/or
 *   showStatusText.  Modifying these routines provides complete control
 *   over the formatting of the entir status line.  If a game wants to use
 *   something other than the traditional left/right display, it must
 *   modify these methods.
 *   
 *   This object is transient, because the statusline style is a function
 *   of the interpreter we're currently running on, and thus isn't suitable
 *   for saving persistently.
 */
transient statusLine: object
    /* 
     *   Show the status line, in HTML or text mode, as appropriate.  By
     *   default, the library sets this up as a "prompt daemon," which
     *   means that this will be called automatically just before each
     *   command line is read.  
     */
    showStatusLine()
    {
        local oldStr;

        /* if the status line isn't active, or there's no PC, skip this */
        if (statusDispMode == nil || gPlayerChar == nil)
            return;

        /* 
         *   showing the status line doesn't normally change any game
         *   state, so we can turn on the sense cache while generating the
         *   display 
         */
        libGlobal.enableSenseCache();

        /* 
         *   Enter status-line mode.  This will do whatever is required for
         *   our current status-line display style to prepare the output
         *   manager so that any text we display to the default output
         *   stream is displayed on the status line. 
         */
        oldStr = beginStatusLine();

        /* make sure we restore statusline mode before we're done */
        try
        {
            /*
             *   Generate a text or HTML status line, as appropriate.  If
             *   we're in <BANNER> tag mode or banner API mode, use HTML to
             *   format the contents of the status line; if we're using the
             *   old-style text mode, use plain text, since the formatting
             *   is rigidly defined in this mode.  
             */
            switch (statusDispMode)
            {
            case StatusModeTag:
            case StatusModeApi:
            case StatusModeBrowser:
                /* show the HTML status line */
                showStatusHtml();
                break;

            case StatusModeText:
                /* show the status line in plain text mode */
                showStatusText();
                break;
            }
        }
        finally
        {
            /* end status-line mode */
            endStatusLine(oldStr);

            /* turn off sense caching */
            libGlobal.disableSenseCache();
        }
    }

    /* prompt-daemon showing the status line */
    showStatusLineDaemon()
    {
        /* show the status line as normal */
        showStatusLine();

        /* 
         *   Explicitly flush the status line if it's in a banner window.
         *   This will ensure that we'll redraw the status line on each
         *   turn if we're reading an input script, which is nice because
         *   it provides a visual indication that something's happening. 
         */
        if (statusDispMode is in (StatusModeApi, StatusModeBrowser))
            statuslineBanner.flushBanner();
    }

    /*
     *   Show the status line in HTML format.  Our default implementation
     *   shows the traditional two-part (left/right) status line, using
     *   showStatusLeft() and showStatusRight() to display the parts.  
     */
    showStatusHtml()
    {
        /* 
         *   start the left half, and write the <A HREF> to hyperlink the
         *   location name to a "look around" command 
         */
        "<<statusHTML(0)>><<
          aHref(gLibMessages.commandLookAround, nil, nil, AHREF_Plain)>>";

        /* show the left part of the status line */
        showStatusLeft();
            
        /* 
         *   end the left portion and start the right portion, then
         *   generate the <A HREF> to link the score to a FULL SCORE
         *   command 
         */
        "<./a></a><<statusHTML(1)>><<
          aHref(gLibMessages.commandFullScore, nil, nil, AHREF_Plain)>>";
          
        /* show the right part of the status line */
        showStatusRight();
          
        /* end the score link, and end the right half wrapper */
        "<./a></a><<statusHTML(2)>>";
        
        /* add the status-line exit list, if desired */
        if (gPlayerChar.location != nil)
            gPlayerChar.location.showStatuslineExits();
    }

    /*
     *   Get the estimated HTML-style banner height, in lines of text.
     *   This is used to set the status line banner size for platforms
     *   where sizing to the exact height of the rendered contents isn't
     *   supported.
     *   
     *   If showStatusHtml() is overridden to display more or fewer lines
     *   of text than the basic implementation here, then this routine must
     *   be overridden as well to reflect the new height.  
     */
    getEstimatedHeightHtml()
    {
        local ht;
        
        /* 
         *   we need one line for the basic display (the location name and
         *   score/turn count) 
         */
        ht = 1;

        /* add in the estimated height of the exits display, if appropriate */
        if (gPlayerChar.location != nil)
            ht += gPlayerChar.location.getStatuslineExitsHeight();

        /* return the result */
        return ht;
    }

    /*
     *   Show the statusline in text mode.  Our default implementation
     *   shows the traditional two-part (left/right) status line, using
     *   showStatusLeft() and showStatusRight() to display the parts.  
     */
    showStatusText()
    {
        /* show the left part of the display */
        showStatusLeft();

        /* switch to the right-side status stream */
        outputManager.setOutputStream(statusRightOutputStream);

        /* show the right-half text */
        showStatusRight();
        
        /* flush the right-side stream */
        statusRightOutputStream.flushStream();
    }

    /*
     *   Show the left part of a standard left/right statusline.  By
     *   default, we'll show the player character's location, by calling
     *   statusName() on the PC's immediate container.  
     */
    showStatusLeft()
    {
        local actor;

        /* get the player character actor */
        actor = gPlayerChar;

        "<.statusroom>";

        /* show the actor's location's status name */
        if (actor != nil && actor.location != nil)
            actor.location.statusName(actor);

        "<./statusroom>";
    }

    /*
     *   Show the right part of a standard left/right statusline.  By
     *   default, we'll show the current score, a slash, and the number of
     *   turns. 
     */
    showStatusRight()
    {
        local s;

        /* if there's a score object, show the score */
        if ((s = libGlobal.scoreObj) != nil)
        {
            /* show the score and the number of turns so far */
            "<.statusscore><<s.totalScore>>/<<
            libGlobal.totalTurns>><./statusscore>";
        }
    }

    /*
     *   Set up the status line's color scheme.  This is called each time
     *   we redraw the status line to set the background and text colors.
     *   We call the statusline banner window to do the work, since the
     *   mechanism is different between the traditional and Web UIs.
     */
    setColorScheme()
    {
        /* call the banner window to do the work */
        statuslineBanner.setColorScheme();
    }

    /* 
     *   Begin status-line mode.  This sets up the output manager so that
     *   text written to the default output stream is displayed on the
     *   status line.  Returns the original output stream.
     */
    beginStatusLine()
    {
        local oldStr;

        /* check what kind of statusline display we're using */
        switch(statusDispMode)
        {
        case StatusModeApi:
            /* 
             *   We have a banner API window.  Start by clearing the
             *   window, so we can completely replace everything in it.  
             */
            statuslineBanner.clearWindow();

            /*
             *   If the platform doesn't support size-to-contents, then set
             *   the height to our best estimate for the size.
             *   
             *   If we do support size-to-contents, we'll set the height to
             *   the exact rendered size when we're done, so we don't need
             *   to worry about setting an estimate; indicate this to the
             *   interpreter by setting the is-advisory flag to true.  
             */
            statuslineBanner.setSize(getEstimatedHeightHtml(),
                                     BannerSizeAbsolute, true);

            /* switch to the banner's output stream */
            oldStr = statuslineBanner.setOutputStream();

            /* set up the statusline color in the window */
            setColorScheme();

            /* done */
            break;

        case StatusModeBrowser:
            /* browser UI - clear the window */
            statuslineBanner.clearWindow();

            /* switch to its output stream */
            oldStr = statuslineBanner.setOutputStream();
            break;

        case StatusModeTag:
            /* 
             *   We're using <BANNER> tags.  Switch to our statusline
             *   output stream.  
             */
            oldStr = outputManager.setOutputStream(statusTagOutputStream);

            /* set up the <BANNER> tag */
            "<banner id=StatusLine height=previous border>";

            /* set up the color scheme */
            setColorScheme();

            /* done */
            break;

        case StatusModeText:
            /* flush the main window */
            flushOutput();

            /* plain text mode - enter text status mode */
            statusMode(StatModeStatus);

            /* switch to the status-left output stream */
            oldStr = outputManager.setOutputStream(statusLeftOutputStream);

            /* done */
            break;
        }

        /* return the original output stream */
        return oldStr;
    }

    /* end statusline display */
    endStatusLine(oldStr)
    {
        /* restore the old default output stream */
        outputManager.setOutputStream(oldStr);

        /* check the type of statusline we're generating */
        switch (statusDispMode)
        {
        case StatusModeApi:
            /* banner API mode - end the last line */
            statuslineBanner.writeToBanner('\n');

            /* 
             *   Size the window to its current contents.  This doesn't
             *   work everywhere - on a few platforms, it does nothing -
             *   but this will give us the optimal size where it's
             *   supported.  On platforms that don't support this, it'll do
             *   nothing, which means we'll simply be left with the
             *   "advisory" size we established earlier.  
             */
            statuslineBanner.sizeToContents();
            break;

        case StatusModeBrowser:
            /* browser mode - flush the window and update the content size */
            statuslineBanner.flushWin();
            break;

        case StatusModeTag:
            /* HTML <BANNER> mode - end the <BANNER> tag */
            statusTagOutputStream.writeToStream('</banner>');
            break;

        case StatusModeText:
            /* plain text statusline - end status mode */
            statusMode(StatModeNormal);
        }
    }

    /*
     *   Initialize the banner window, given the BannerWindow object
     *   representing the status line banner API window.  
     */
    initBannerWindow(win)
    {
        /* 
         *   Try showing the banner API window.  If that succeeds, use the
         *   banner API window, since it's the most portable and flexible
         *   way to show the status line.  If we can't create the banner
         *   API window, it means we're on an interpreter that doesn't
         *   support the banner API, so fall back on one of the older, less
         *   flexible mechanisms; which older mechanism we choose depends
         *   on what kind of interpreter we're on.
         *   
         *   Since we create the status line banner during initialization
         *   and normally leave it as the first item in the display list at
         *   all times, we can attach to an existing status line banner
         *   window if there is one.  This will avoid unnecessary redrawing
         *   on RESTART.  
         */
        if (win.showBanner(nil, BannerFirst, nil, BannerTypeText,
                           BannerAlignTop, nil, nil,
                           BannerStyleBorder | BannerStyleTabAlign))
        {
            /* 
             *   we successfully created the banner window - use the banner
             *   API to show the status line 
             */
            statusDispMode = StatusModeApi;
        }
        else if (outputManager.htmlMode)
        {
            /*
             *   We failed to create a banner API window, and we're running
             *   on a full HTML interpreter, so use <BANNER> tags to
             *   produce the status line.  
             */
            statusDispMode = StatusModeTag;
        }
        else
        {
            /*
             *   We failed to create a banner API window, and we're running
             *   on a text-only interpreter - use the old-style
             *   fixed-format status line mechanism.  
             */
            statusDispMode = StatusModeText;
        }
    }

    /* 
     *   The status mode we're using.  If this is nil, it means we haven't
     *   chosen a mode yet. 
     */
    statusDispMode = nil
;

