#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library - browser (Web UI) input/output manager
 *   
 *   This module defines the low-level functions for handling input and
 *   output via the Web UI.
 *   
 *   The functions in this module are designed primarily for internal use
 *   within the library itself.  Games should use the higher level objects
 *   and functions defined in input.t and output.t instead of directly
 *   calling the functions defined here.  The reason for separating these
 *   functions is to make the UI selection pluggable, so that the same game
 *   can be compiled for either the traditional UI or the Web UI simply by
 *   plugging in the correct i/o module.  
 */

/* include the library header */
#include "adv3.h"


/* ------------------------------------------------------------------------ */
/*
 *   Browser globals 
 */
transient browserGlobals: object
    /* the HTTPServer object for the browser UI session */
    httpServer = nil

    /* 
     *   Log file handle.  For a LogTypeTranscript file, this is a
     *   LogConsole object; for other types, it's a regular file handle.  
     */
    logFile = nil
    
    /* logging type (LogTypeXxx from tadsio.h, or nil if not logging) */
    logFileType = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   Initialize the user interface.  The library calls this once at the
 *   start of the interpreter session to set up the UI.  For the Web UI, we
 *   create the HTTP server and send connection instructions to the client.
 */
initUI()
{
    /* 
     *   Set up the HTTP server.  Listen on the launch address, which is
     *   the address that the client used to reach the external Web server
     *   that launched the interpreter.  For local stand-alone launches,
     *   the launch address is nil, so the HTTP server will listen on
     *   localhost, which is just what we need in order to connect to the
     *   local UI.  
     */
    local srv = browserGlobals.httpServer = new HTTPServer(
        getLaunchHostAddr(), nil, 1024*1024);
    
    /* send connection instructions to the client */
    webSession.connectUI(srv);
}

/*
 *   Initialize the display.  We call this when we first enter the
 *   interpreter, and again at each RESTART, to set up the main game
 *   window's initial layout.  We set up the conventional IF screen layout,
 *   with the status line across the top and the transcript/command window
 *   filling the rest of the display.  
 */
initDisplay()
{
    /* set up the command window and status line */
    webMainWin.createFrame(commandWin, 'command',
                           '0, statusline.bottom, 100%, 100%');
    webMainWin.createFrame(statuslineBanner, 'statusline',
                           '0, 0, 100%, content.height');

    /* capture the title string */
    local title = mainOutputStream.captureOutput(
        {: gameMain.setGameTitle() });

    /* parse out the contents of the <title> tag */
    if (rexSearch('<nocase>[<]title[>](.*)[<]/title[>]', title))
        title = rexGroup(1)[3];

    /* initialize the statusline window object */
    statuslineBanner.init();
    statusLine.statusDispMode = StatusModeBrowser;

    /* set the title */
    webMainWin.setTitle(title);

    /* get the session parameters from the arguments */
    local arg = libGlobal.getCommandSwitch('-gameid=');
    if (arg != nil && arg != '')
        webSession.launcherGameID = arg;

    arg = libGlobal.getCommandSwitch('-storagesid=');
    if (arg != nil && arg != '')
        webSession.storageSID = arg;

    arg = libGlobal.getCommandSwitch('-username=');
    if (arg != nil && arg != '')
        webSession.launcherUsername = arg;
}

/*
 *   Shut down the user interface.  The library calls this when the game is
 *   about to terminate. 
 */
terminateUI()
{
    /* if we have an HTTP server, shut it down */
    if (browserGlobals.httpServer != nil)
    {
        /* flush our windows */
        webMainWin.flushWin();
        commandWin.sendWinEvent('<scrollToBottom/>');

        /* end any scripting */
        aioSetLogFile(nil, LogTypeTranscript);
        aioSetLogFile(nil, LogTypeCommand);

        /* 
         *   keep running for a few more minutes, to give clients a chance
         *   to perform final tasks like downloading log files 
         */
        ClientSession.shutdownWait(5*60*1000);

        /* send the shutdown message */
        eventPage.sendEvent('<shutdown/>');

        /* wait a short time for clients to process the shutdown event */
        ClientSession.shutdownWait(5*1000);

        /* shut down the http server */
        browserGlobals.httpServer.shutdown(true);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Check to see if we're in HTML mode 
 */
checkHtmlMode()
{
    /* 
     *   The web UI is always in HTML mode.  This is regardless of the
     *   interpreter class, because that only tells us about the
     *   interpreter's own native UI.  The actual user interface in Web UI
     *   mode runs in a separate Web browser app, which is inherently HTML
     *   capable. 
     */
    return true;
}

/* ------------------------------------------------------------------------ */
/*
 *   Write text to the main game window
 */
aioSay(txt)
{
    /* write the text to the main command window */
    commandWin.write(txt);

    /* if we're logging a full transcript, write the text */
    if (browserGlobals.logFileType == LogTypeTranscript)
        browserGlobals.logFile.writeToStream(txt);
}

/* ------------------------------------------------------------------------ */
/*
 *   Is a script file active? 
 */
readingScript()
{
    return setScriptFile(ScriptReqGetStatus) != nil;
}

/*
 *   Is an event script active? 
 */
readingEventScript()
{
    local s = setScriptFile(ScriptReqGetStatus);
    return (s != nil && (s & ScriptFileEvent) != 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   Get a line of input from the keyboard, with timeout 
 */
aioInputLineTimeout(timeout)
{
    /* check for script input */
    local scriptMode = setScriptFile(ScriptReqGetStatus);
    if (scriptMode != nil)
    {
        /* we're in a script, so use the regular input line reader */
        local e = inputLineTimeout(timeout);

        /* 
         *   If it's not an end-of-file indication, return the event.  An
         *   EOF means that there are no more events in the script, so
         *   return to reading from the live client UI.  
         */
        if (e[1] != InEvtEof)
        {
            /* echo the input if we're not in quiet mode */
            if (e[1] == InEvtLine && !(scriptMode & ScriptFileQuiet))
                aioSay(e[2].htmlify() + '\n');

            /* log and return the event */
            return aioLogInputEvent(e);
        }
    }

    /* 
     *   read an input line event from the main command window, log it, and
     *   return it 
     */
    return aioLogInputEvent(commandWin.getInputLine(timeout));
}

/*
 *   Cancel a suspended input line 
 */
aioInputLineCancel(reset)
{
    /* cancel the input line in the command window */
    commandWin.cancelInputLine(reset);
}


/* ------------------------------------------------------------------------ */
/*
 *   Read an input event 
 */
aioInputEvent(timeout)
{
    /* check for script input */
    if (readingEventScript())
    {
        /* we're in a script, so use the regular input line reader */
        local e = inputEvent(timeout);

        /* 
         *   If it's not an end-of-file indication, return the event.  An
         *   EOF means that there are no more events in the script, so
         *   return to reading from the live client UI.  
         */
        if (e[1] != InEvtEof)
            return aioLogInputEvent(e);
    }

    /* read an event from the main command window, log it, and return it */
    return aioLogInputEvent(webMainWin.getInputEvent(timeout));
}


/* ------------------------------------------------------------------------ */
/*
 *   Show a "More" prompt 
 */
aioMorePrompt()
{
    /* show a More prompt in the main command window */
    commandWin.showMorePrompt();
}

/* ------------------------------------------------------------------------ */
/*
 *   Clear the screen 
 */
aioClearScreen()
{
    /* clear the main transcript window */
    commandWin.clearWindow();
}


/* ------------------------------------------------------------------------ */
/*
 *   Show a file selector dialog 
 */
aioInputFile(prompt, dialogType, fileType, flags)
{
    /* 
     *   First, try reading from the local console.  Even though we're
     *   using the Web UI, there are two special cases where the input will
     *   come from the local (server-side) console instead of from the
     *   browser UI:
     *   
     *   1. We're reading from an event script.  In this case, regardless
     *   of the UI mode, the interpreter reads from a server-side file and
     *   parses the results into an inputFile() result, bypassing any UI
     *   interaction.
     *   
     *   2. We're running in the Web UI's local stand-alone configuration,
     *   where the browser is actually an integrated window within the
     *   interpreter.  This configuration simulates the traditional UI by
     *   running everything locally - the client and server are running on
     *   the same machine, so there's really no distinction between
     *   client-side and server-side.  Because everything's local, files
     *   are local, so we want to display traditional local file selector
     *   dialogs.  The stand-alone interpreter does this for us via the
     *   standard inputFile() function when it detects this configuration.
     *   
     *   If neither of these special cases apply, inputFile() will return
     *   an error to let us know that it can't show a file dialog in the
     *   current configuration, so we'll continue on to showing the dialog
     *   on the client side via the Web UI.  
     */
    local f = inputFile(prompt, dialogType, fileType, flags);

    /* if that failed, forget the result */
    if (f[1] == InFileFailure)
        f = nil;

    /* if we got a file, check for warnings */
    if (f != nil && f.length() >= 4 && f[4] != nil)
    {
        /* keep going until we get a definitive answer */
        for (local done = nil ; !done ; )
        {
            /* show the warning dialog */
            local d = webMainWin.getInputDialog(
                InDlgIconWarning,
                libMessages.inputFileScriptWarning(f[4], f[2]),
                libMessages.inputFileScriptWarningButtons, 1, 3);
            
            /* check the result */
            switch (d)
            {
            case 0:
            case 3:
                /* dialog error or Cancel Script - stop the script */
                setScriptFile(nil);
                
                /* return a Cancel result */
                return [InFileCancel];
                
            case 1:
                /* "Yes" - proceed */
                done = true;
                break;
                
            case 2:
                /* Choose New File button - show a file dialog */
                local fNew = webMainWin.getInputFile(
                    prompt, dialogType, fileType, flags);
                
                switch (fNew[1])
                {
                case InFileSuccess:
                    /* success - use the new file, and we're done */
                    f = fNew;
                    done = true;
                    break;
                    
                case InFileCancel:
                    /* cancel - repeat the prompt */
                    break;
                    
                case InFileFailure:
                    /* dialog error - cancel the script */
                    setScriptFile(nil);
                    return [InFileCancel];
                }
            }
        }
    }

    /* 
     *   if we didn't get a result from a script or from the local console,
     *   tell the client UI to display its file dialog 
     */
    if (f == nil)
        f = webMainWin.getInputFile(prompt, dialogType, fileType, flags);

    /* log a synthetic <file> event, if applicable */
    aioLogInputEvent(
        ['<file>',
         f[1] != InFileSuccess ? '' :
         dataType(f[2]) == TypeObject && !f[2].ofKind(FileName) ? 't' :
         f[2]]);
    
    /* return the file information */
    return f;
}

/* ------------------------------------------------------------------------ */
/*
 *   Show an input dialog 
 */
aioInputDialog(icon, prompt, buttons, defaultButton, cancelButton)
{
    /* check for script input */
    local d = nil;
    if (readingEventScript())
    {
        /* we're in a script, so use the regular dialog event reader */
        d = inputDialog(icon, prompt, buttons, defaultButton, cancelButton);

        /* if it failed, forget the result */
        if (d == 0)
            d = nil;
    }

    /* if we didn't get script input, show the dialog via the client UI */
    if (d == nil)
        d = webMainWin.getInputDialog(icon, prompt, buttons,
                                      defaultButton, cancelButton);

    /* log a synthetic <dialog> event, if applicable */
    aioLogInputEvent(['<dialog>', d]);

    /* return the result */
    return d;
}


/* ------------------------------------------------------------------------ */
/*
 *   Set/remove the output logging file
 */
aioSetLogFile(fname, typ = LogTypeTranscript)
{
    /* if there's currently a log file open, close it */
    local log = browserGlobals.logFile;
    if (log != nil)
    {
        switch (browserGlobals.logFileType)
        {
        case LogTypeTranscript:
            /* for a transcript, we have a log console as the handle */
            log.closeConsole();
            break;

        default:
            /* for other types, we have a regular file handle */
            try
            {
                log.closeFile();
            }
            catch (Exception exc)
            {
                /* ignore errors, as we have no way to return them */
            }
            break;
        }

        /* we've closed the handle, so forget it */
        log = nil;
    }

    /* presume success */
    local ok = true;

    /* if there's a filename, create a new console for this file */
    if (fname != nil)
    {
        /* create the output handle according to the type */
        switch (typ)
        {
        case LogTypeTranscript:
            /* 
             *   full transcript - create a log console, which will do the
             *   standard output formatting for us 
             */
            log = new LogConsole(fname, nil, 80);
            break;

        case LogTypeCommand:
        case LogTypeScript:
            /* for other types, create an ordinary text file */
            try
            {
                /* open the log file */
                log = File.openTextFile(fname, FileAccessWrite, nil);

                /* for an event script, write the <eventscript> opener */
                if (typ == LogTypeScript)
                    log.writeFile('<eventscript>\n');
            }
            catch (Exception exc)
            {
                /* if anything went wrong, we have no log file */
                log = nil;
            }
            break;

        default:
            throw RuntimeError.newRuntimeError(2306, 'bad log file type');
        }

        /* we failed if the log handle is nil */
        if (log == nil)
        {
            ok = nil;
            typ = nil;
        }
    }
    else
    {
        /* no longer logging */
        typ = nil;
    }

    /* remember the new handle and log type */
    browserGlobals.logFile = log;
    browserGlobals.logFileType = typ;

    /* return the success/failure indicator */
    return ok;
}

/*
 *   Log an input event.  We call this internally from each of the event
 *   input routines to add the event to any event or command log we're
 *   creating. 
 */
aioLogInputEvent(evt)
{
    /* if the system is maintaining its own input log, write it there */
    logInputEvent(evt);

    /* get the script globals */
    local ltyp = browserGlobals.logFileType;
    local log = browserGlobals.logFile;

    /* get the basic event parameters */
    local evtType = evt[1];
    local param = (evt.length() > 1 ? evt[2] : nil);

    /* format the event based on the event type */
    switch (ltyp)
    {
    case LogTypeTranscript:
        /* transcript - echo command line input */
        if (evt[1] == InEvtLine)
            log.writeToStream(evt[2].htmlify() + '\n');
        break;

    case LogTypeCommand:
        /* command script - write command inputs only */
        if (evt[1] == InEvtLine)
            log.writeFile('>' + evt[2] + '\n');
        break;

    case LogTypeScript:
        /* event script - write all event types */
        switch (evtType)
        {
        case InEvtKey:
            log.writeFile('<key>' + evtCharForScript(param));
            break;
            
        case InEvtTimeout:
            log.writeFile('<timeout>' + param);
            break;
            
        case InEvtHref:
            log.writeFile('<href>' + param);
            break;
            
        case InEvtNoTimeout:
            log.writeFile('<notimeout>');
            break;
            
        case InEvtEof:
            log.writeFile('<eof>');
            break;
            
        case InEvtLine:
            log.writeFile('<line>' + param);
            break;
            
        case InEvtSysCommand:
            log.writeFile('<command>' + param);
            break;
            
        case InEvtEndQuietScript:
            log.writeFile('<endqs>');
            break;

        default:
            /* if it's a string value, it's the literal event tag */
            if (dataType(evtType) == TypeSString)
                log.writeFile(evtType + param);
            break;
        }

        /* add a newline at the end of the event line */
        log.writeFile('\n');
        break;
    }

    /* 
     *   return the event, so that the caller can conveniently return it
     *   after logging it 
     */
    return evt;
}

/* 
 *   Get an InEvtKey event parameter in suitable format for script file
 *   output.  This returns the key as it appears in the event, except that
 *   ASCII control characters are translated to '[ctrl-X]'.  
 */
evtCharForScript(c)
{
    if (c.toUnicode(1) < 32)
    {
        /* it's a control character - return the [ctrl-X] sequence */
        return '[ctrl-<<makeString(c.toUnicode(1) + 64)>>]';
    }
    else
    {
        /* return everything else as it appears in the event descriptor */
        return c;
    }
}


/* ------------------------------------------------------------------------ */
/* 
 *   Generate a string to show hyperlinked text.  The browser UI is always
 *   in HTML mode, so we unconditionally generate the hyperlink.
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
    /* figure extra properties, based on the flags */
    local props = '';
    if (flags & AHREF_Plain)
        props += 'class="plain" ';

    /* generate the <A HREF>, text, and </A>, as applicable */
    return '<a <<props>> href="<<href.findReplace('"', '%22')>>"<<
           (title != nil
            ? ' title="' + title.findReplace('"', '&#34;') + '"'
            : '')
           >> onclick="javascript:return gamehref(event,\'<<
           href.findReplace(['\'', '"'],
                            ['\\\'', '\'+String.fromCharCode(34)+\'']) 
           >>\', \'main.command\', this);"><.a><<
           (txt != nil ? txt + '<./a></a>' : '')>>';
}

/* ------------------------------------------------------------------------ */
/* 
 *   Generate a string to show hyperlinked text, with alternate text if
 *   we're not in HTML mode.  The browser UI is always in HTML mode, so we
 *   unconditionally generate the hyperlink.  
 */
aHrefAlt(href, linkedText, altText, title?)
{
    return aHref(href, linkedText, title);
}

/* ------------------------------------------------------------------------ */
/*
 *   The standard main command window. 
 */
transient commandWin: WebCommandWin
;
    

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
    switch(stage)
    {
    case 0:
        /* start the left-aligned portion */
        return '<div class="statusleft">';

    case 1:
        /* close the left portion, and start the right-aligned portion */
        return '</div><div class="statusright">';

    case 2:
        /* 
         *   Close the right portion, and break clear of the floating
         *   sections.  The break is necessary to make sure that the
         *   contents of the two sections count in the window height; some
         *   browsers don't include floating boxes in the content height,
         *   so we need to manually extend the main vertical box's height
         *   past the floating sections. 
         */
        return '</div><div class="statusStrut"></div>';

    case 3:
    case 4:
        /* before/after exit listing - we have nothing to add here */
        return '';

    default:
        return '';
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Web Banner Window.  This is designed as a *partial* drop-in
 *   replacement for the BannerWindow class, using Web UI windows as
 *   implemented in the core TADS javascript client.
 *   
 *   This class is designed to be mixed with a WebWindow subclass.
 *   
 *   This isn't a complete replacement for BannerWindow, because the layout
 *   model for the Web UI is different from the banner window model (the
 *   Web UI model is better and more flexible).  This class implements the
 *   parts of the BannerWindow API related to the stream-oriented output to
 *   the window, so you shouldn't have to change anything that writes HTML
 *   text to the window.  However, you will have to rework code that sets
 *   up the window's layout to use the Web UI model.  
 */
class WebBannerWin: OutputStreamWindow
    /* 
     *   Initialize.  Call this when first displaying the window in the UI.
     */
    init()
    {
        /* set up our output stream */
        createOutputStream();
    }

    /* create our output stream subclass */
    createOutputStreamObj()
    {
        return new transient WebWinOutputStream(self);
    }

    /* flush output */
    flushBanner()
    {
        flushWin();
    }

    /* write text */
    writeToBanner(txt)
    {
        outputStream_.writeToStream(txt);
    }

    /*
     *   Banner window size settings.  We simply ignore these; callers must
     *   rework their layout logic for the Web UI, since the javascript
     *   layout system is so different.  
     */
    setSize(siz, units, advisory) { }
    sizeToContents() { }
;

/*
 *   Output stream for web banner windows
 */
class WebWinOutputStream: OutputStream
    /* construct */
    construct(win)
    {
        /* do the base class construction */
        inherited();
        
        /* save our window */
        win_ = win;
    }

    /* ignore preinit - we're always created dynamically */
    execute() { }

    /* write to the underlying window */
    writeFromStream(txt)
    {
        /* add the text to the window */
        win_.write(txt);
    }

    /* our status line window */
    win_ = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   The basic status line window.  The "banner" in the name is historical,
 *   because the traditional console UI implements the status line as a
 *   banner window.  We don't actually have banner windows in the Web UI;
 *   we use iframes instead.  But we keep the name to make it easier to
 *   port games written for the traditional UI to the Web UI.  
 */
transient statuslineBanner: WebStatusWin, WebBannerWin
;

