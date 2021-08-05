#charset "us-ascii"
#pragma once

/* 
 *   Copyright (c) 1999, 2006 Michael J. Roberts
 *   
 *   This file is part of TADS 3
 *   
 *   This header defines the tads-io intrinsic function set.  
 *   
 *   The TADS input/output function set provides access to the user
 *   interface.  This lets you read input from the keyboard and display
 *   output on the monitor or terminal.  It also provides access to windowing
 *   features (via the "banner" functions) on systems that support multiple
 *   display windows (which doesn't necessarily mean GUI-type systems: many
 *   character-mode systems support the banner operations as well, simply by
 *   dividing up the character-mode screen into rectangular regions).  
 */


/*
 *   tads-io - the TADS Input/Output intrinsic function set 
 */
intrinsic 'tads-io/030007'
{
    /* 
     *   Display values on the console.  One or more values can be displayed.
     *   Each value can be a string, in which case the string is displayed as
     *   given (with HTML interpretation); an integer, in which case it's
     *   converted to a string, using a decimal (base 10) radix and
     *   displayed; a BigNumber, in which case it's converted to a string
     *   using the default formatting; or nil, in which case nothing is
     *   displayed.  
     */
    tadsSay(val, ...);

    /* 
     *   Set the output log file (which records the output transcript) or the
     *   command log file (which records command lines the user enters).
     *   'fname' is the name of the file to open, and 'logType' gives the
     *   type of log to open, as a LogTypeXxx value.  
     */
    setLogFile(fname, logType?);

    /* 
     *   Clear the display.  This clears the main window.  
     */
    clearScreen();

    /* 
     *   Show the "more" prompt, if supported on the platform.  This causes a
     *   "more" prompt to be displayed, according to local system
     *   conventions, as though consecutive text output had exceeded the
     *   screen/window height.  
     */
    morePrompt();

    /* 
     *   Read a line of text from the keyboard.  Pauses to let the user edit
     *   and enter a command line, then returns the entered text as a string.
     */
    inputLine();

    /* 
     *   Read a single keystroke from the keyboard.  Waits until the user
     *   presses a key, then returns the keystroke as a string.  
     */
    inputKey();

    /* 
     *   Read a single input event.  Waits until an input event is available,
     *   then returns the event as a list.  The first element of the list is
     *   an InEvtXxx value indicating the type of the event; the remainder of
     *   the list varies according to the event type.  If 'timeout' is
     *   provided, it gives the maximum waiting interval in milliseconds; if
     *   no input event occurs within this interval, the function returns an
     *   InEvtTimeout event.  
     */
    inputEvent(timeout?);

    /* 
     *   Display a simple "message box" dialog (known on some systems as an
     *   "alert" dialog).  This displays a dialog that includes a short
     *   message for the user to read, an icon indicating the general nature
     *   of the condition that gave rise to the dialog (an error, a warning,
     *   a choice for the user to make, etc.), and a set of push-buttons that
     *   dismiss the dialog and (in some cases) let the user choose among
     *   options.  'icon' is an InDlgIconXxx value giving the type of icon to
     *   show, if any; 'prompt' is the message string to display; 'buttons'
     *   gives the set of buttons to display; 'defaultButton' is the index
     *   (starting at 1) among the buttons of the default button; and
     *   'cancelButton' is the index of the cancellation button.
     *   
     *   'buttons' can be given as an InDlgXxx constant (InDlgOk,
     *   InDlgOkCancel, etc.) to select one of the standard sets of buttons.
     *   Or, it can be a list giving a custom set of buttons, in which case
     *   each element of the list is either a string giving a custom label
     *   for the button, or one of the InDlgLblXxx values to select a
     *   standard label.  The standard labels should be used when possible,
     *   as these will be automatically localized; labels given explicitly as
     *   strings will be used exactly as given.  If a list of custom button
     *   labels is given, the buttons are displayed in the dialog in the
     *   order of the list (usually left to right, but this could vary
     *   according to system conventions and localization).
     *   
     *   Each custom button label string can incorporate an ampersand ("&").
     *   The letter immediately following the ampersand, if provided, is used
     *   as the keyboard shortcut for the button.  This is particularly
     *   important on character-mode systems, where the "dialog" is typically
     *   shown merely as a text prompt, and the user responds by selecting
     *   the letter of the desired option.  Typically, you should use the
     *   first character of a button label as its keyboard shortcut, but this
     *   obviously won't work when two button labels have the same first
     *   letter; in these cases, you should choose another letter from the
     *   button label, preferably something like the first letter of the
     *   second word of the button label, or the first letter of the stressed
     *   syllable of the most important word of the label.
     *   
     *   The return value is the index among the buttons of the button that
     *   the user selects to dismiss the dialog.  The function doesn't return
     *   until the user selects one of the buttons.  
     */
    inputDialog(icon, prompt, buttons, defaultButton, cancelButton);

    /* 
     *   Display a file selector dialog.  This prompts the user to select a
     *   file.  On GUI systems, this will typically display the standard
     *   system file selection dialog; on character-mode systems, it might
     *   simply display the prompt string and let the user type the name of a
     *   file directly.
     *   
     *   'prompt' is the message string to display in the dialog to let the
     *   user know what type of file is being requested.  'dialogType' is one
     *   of the InFileXxx constants specifying whether the request is to
     *   select an existing file or to specify the name for a new file.
     *   'fileType' is a FileTypeXxx constant giving the format of the file
     *   being requested; this is used on some systems to filter the
     *   displayed list of existing files so that only files of the same
     *   format are included, to reduce clutter.  'flags' is reserved for
     *   future use and should simply be set to zero.
     *   
     *   The return value is a list.  The first element is an integer giving
     *   the status: InFileSuccess indicates that the user successfully
     *   selected a file, whose name is given as a string in the second
     *   element of the result list; InFileFailure indicates a system error
     *   of some kind showing the dialog; and InFileCancel indicates that the
     *   user explicitly canceled the dialog.
     *   
     *   On success (return list[1] == InFileSuccess), the list contains the
     *   following additional elements:
     *   
     *.     [2] = the selected filename
     *.     [3] = nil (reserved for future use)
     *.     [4] = script warning message, or nil if no warning
     *   
     *   The warning message is a string to be displayed to the user to warn
     *   about a possible error condition in the script input.  The script
     *   reader checks the file specified in the script to see if it's valid;
     *   if the dialog type is Open, the script reader verifies that the file
     *   exists, and for a Save dialog the reader warns if the file *does*
     *   already exist or is not writable.  In the conventional UI, the
     *   script reader displays these warnings directly to the user through
     *   the console UI, but this isn't possible in the Web UI since the user
     *   might be running on a remote browser.  Instead, the script reader
     *   still checks for the possible errors, but rather than displaying any
     *   warnings, it returns them here.  The caller is responsible for
     *   displaying the warning and asking the user for confirmation.
     *   
     *   For localization purposes, the warning message starts with a
     *   two-letter code indicating the specific error, followed by a space,
     *   followed by the English text of the warning.  The codes are:
     *   
     *.   OV - the script might overwrite an existing file (Save dialog)
     *.   WR - the file can't be created/written (Save dialog)
     *.   RD - the file doesn't exist/can't be read (Open dialog)
     *   
     *   Note that the warning message will always be nil if the script
     *   reader displayed the warning message itself.  This means that your
     *   program can unconditionally display this message if it's non-nil -
     *   there's no danger that the script reader will have redundantly
     *   displayed the message.  
     */
    inputFile(prompt, dialogType, fileType, flags);

    /* 
     *   Pause for the given number of milliseconds.  
     */
    timeDelay(delayMilliseconds);

    /* 
     *   Retrieve local system information.  'infoType' is a SysInfoXxx
     *   constant giving the type of information to retrieve.  Additional
     *   arguments and the return value vary according to the infoType value.
     */
    systemInfo(infoType, ...);

    /* 
     *   Set the status-line display mode.  This is meaningful only with
     *   text-only interpreters that don't support banner windows; other
     *   interpreters ignore this.  'mode' is a StatModeXxx constant giving
     *   the new mode.  
     */
    statusMode(mode);

    /* 
     *   Write text on the right half of the status line.  This is meaningful
     *   only for text-only interpreters that don't support banner windows;
     *   other interpreters ignore this.  On non-banner interpreters, this
     *   sets the right half of the status line to display the given text,
     *   right-justified.  
     */
    statusRight(txt);
    
    /* 
     *   Determine if a multimedia resource exists.  'fname' is the name of a
     *   resource (a JPEG image file, PNG image file, etc), given in
     *   URL-style path notation.  Returns true if the resource is available,
     *   nil if not.  
     */
    resExists(fname);

    /* 
     *   Set the script input file.  This opens the given file as the script
     *   input file.  'filename' is a string giving the name of the file to
     *   open, and 'flags' is a combination of ScriptFileXxx bit flags giving
     *   the mode to use to read the file.  When a script file is active, the
     *   system reads command-line input from the file rather than from the
     *   keyboard.  This lets the program replay an input script.
     *   
     *   Note that the ScriptFileEvent flag is ignored if included in the
     *   'flags' parameter.  The script reader automatically determines the
     *   script type by examining the file's contents, so you can't set the
     *   type using flags.  This flag is used only in "get status" requests
     *   (ScriptReqGetStatus) - it's included in the returned flags if
     *   applicable.  The purpose of this flag is to let you determine what
     *   the script reader decided about the script, rather than telling the
     *   script reader how to interpret the script.
     *   
     *   If 'filename' is nil, this cancels the current script.  If the
     *   script was invoked from an enclosing script, this resumes the
     *   enclosing script, otherwise it resumes reading input from the
     *   keyboard.  The 'flags' argument is ignored in this case.
     *   
     *   New in 3.0.17: if 'filename' is one of the ScriptReqXxx constants,
     *   this performs a special script request.  See the ScriptReqXxx
     *   constants for details.  Note that calling this function with a
     *   ScriptReqXxx constant on an VM prior to 3.0.17 will result in a
     *   run-time error, so you can use try-catch to detect whether the
     *   request is supported.  
     */
    setScriptFile(filename, flags?);

    /* 
     *   Get the local default character set.  'which' is a CharsetXxx value
     *   giving which local character set to retrieve.  Returns a string
     *   giving the name of the given local character set.  
     */
    getLocalCharSet(which);
    
    /* 
     *   Flush text output and update the main display window.  This ensures
     *   that any text displayed with tadsSay() is actually displayed, for
     *   the user to see (rather than being held in internal buffers).  
     */
    flushOutput();

    /* 
     *   Read a line of text from the keyboard.  Waits for the user to edit
     *   and enter a command line.  If a 'timeout' value is specified, it
     *   gives the maximum interval to wait for the user to finish entering
     *   the input, in milliseconds.  If the timeout expires before the user
     *   finishes entering the line, the function stops waiting and returns.
     *   
     *   The return value is a list.  The first element is an InEvtXxx code
     *   giving the status.  If the status is InEvtLine, the second element
     *   is a string giving the command line the user entered.  If the status
     *   is InEvtTimeout, the second element is a string giving the text of
     *   the command line so far - that is, the text that the user had typed
     *   up to the point when the timeout expired.  Other status codes have
     *   no additional list elements.
     *   
     *   When an InEvtTimeout status is returned, the caller must either
     *   cancel the interrupted input line with inputLineCancel(), or must
     *   make another call to inputLineTimeout() without any intervening call
     *   to any output function that displays anything in the main window, or
     *   any input function other than inputLineTimeout().  
     */
    inputLineTimeout(timeout?);

    /* 
     *   Cancel an input line that was interrupted by timeout.  This function
     *   must be called after an inputLineTimeout() returns with an
     *   InEvtTimeout status indication and before any subsequent output
     *   function that displays anything in the main window, or any input
     *   fucntion other than inputLineTimeout().
     *   
     *   This function updates the UI to reflect that command line editing is
     *   no longer in progress.  If 'reset' is true, it also resets the
     *   internal memory of the command editing session, so that a subsequent
     *   call to inputLineTimeout() will start from scratch with an empty
     *   command line.  If 'reset' is nil, this function merely adjusts the
     *   UI, but does not clear the internal memory; the next call to
     *   inputLineTimeout() will automatically restore the editing status,
     *   re-displaying what the user had typed so far on the interrupted
     *   command line and restoring the cursor position to its position when
     *   the timeout occurred.
     *   
     *   Note that it's not necessary (or desirable) to call this function
     *   after a timed-out input line if the next input/output function that
     *   affects the main window is simply another call to
     *   inputLineTimeout().  In this case, inputLineTimeout() simply picks
     *   up where it left off, without any indication to the user that the
     *   input editing was ever interrupted.  
     */
    inputLineCancel(reset);
    
    /* 
     *   Create a banner window.  Returns the "handle" of the new window,
     *   which is used to identify the window in subsequent bannerXxx()
     *   functions.  Not all interpreters support banner windows; if the
     *   interpreter does not support this feature, the return value is nil.
     *   
     *   'parent' is the handle of the parent window; if this is nil, the
     *   banner is split off from the main display window.  'where' is a
     *   BannerXxx value giving the list position; if this is BannerBefore or
     *   BannerAfter, 'other' is the handle of an existing banner window
     *   child of the same parent.  'windowType' is a BannerTypeXxx value
     *   giving the type of window to create.  'align' is a BannerAlignXxx
     *   value giving the alignment - that is, the edge of the parent window
     *   to which the new banner window attaches.  'size' is the size of the
     *   window, in the units given by 'sizeUnits', which is a BannerSizeXxx
     *   value.  'styleFlags' is a combination of BannerStyleXxx bit flags
     *   that specifies the desired combination of visual styles and UI
     *   behavior for the new window.  
     */
    bannerCreate(parent, where, other, windowType, align,
                 size, sizeUnits, styleFlags);

    /* 
     *   Delete a banner window.  'handle' is the handle to the window to be
     *   removed. 
     */
    bannerDelete(handle);

    /* 
     *   Clear the contents of a banner window.  'color' is the color to use
     *   for the screen color after clearing the window, given as a ColorXxx
     *   value (see below).  
     */
    bannerClear(handle);

    /* 
     *   Write text to a banner window.  The text is displayed in the given
     *   banner.  For a BannerTypeText window, HTML tags in the text are
     *   interpreted; for a BannerTypeTextGrid window, the text is written
     *   exactly as given, without any HTML interpretation.
     *   
     *   The value list is handled the same way as the arguments to tadsSay()
     *   in terms of type conversions.  
     */
    bannerSay(handle, ...);

    /* 
     *   Flush all buffers for a banner window.  This ensures that any text
     *   written with bannerSay() is actually displayed for the user to see
     *   (rather than being held in internal buffers).  
     */
    bannerFlush(handle);

    /* 
     *   Size a banner to fit its contents.  This resizes the banner such
     *   that the contents just fit.  In the case of a top- or bottom-aligned
     *   banner, the height is set just high enough to hold all of the text
     *   currently displayed.  In the case of a left- or right-aligned
     *   banner, the width is set just wide enough to hold the widest single
     *   word that can't be broken across lines.  In all cases, the size
     *   includes any fixed margin space, to ensure that all of the text in
     *   the window is actually visible without scrolling.
     *   
     *   Note that not all systems support this function.  On systems where
     *   the function is not supported, this call has no effect.  Because of
     *   this, you should always use this function in conjunction with an
     *   "advisory" call to bannerSetSize().  
     */
    bannerSizeToContents(handle);

    /* 
     *   Go to to an output position.  This is meaningful only for
     *   BannerTypeTextGrid windows.  This sets the next text output position
     *   to the given row and column in the text grid; the next call to
     *   bannerSay() will display its output starting at this position.  
     */
    bannerGoTo(handle, row, col);

    /* 
     *   Set text foreground and background colors.  This affects the color
     *   of subsequently displayed text; text displayed previously is not
     *   affected.  The colors are given as ColorXxx values (see below).  If
     *   'bg' is ColorTransparent, then text is shown with the current screen
     *   color in the window.  
     */
    bannerSetTextColor(handle, fg, bg);

    /*
     *   Set the "screen color" in the banner window.  This is the color used
     *   to fill parts of the window that aren't displaying any text, and as
     *   the background color for all text displayed when the text background
     *   color is ColorTransparent.  Setting the screen color immediately
     *   sets the color for the entire window - even text previously
     *   displayed in the window is affected by this change.  
     */
    bannerSetScreenColor(handle, color);

    /* 
     *   Get information on the banner.  This returns a list giving a
     *   detailed set of information describing the banner.  
     */
    bannerGetInfo(handle);

    /* 
     *   Set the size of a banner.  This explicitly sets the banner's height
     *   (for a top or bottom banner) or width (for a left or right) banner
     *   to the 'size', which is specified in units given by 'sizeUnits',
     *   which is a BannerSizeXxx constant.  If 'isAdvisory' is true, the
     *   caller is indicating that this call will be followed soon by a call
     *   to bannerSizeToContents().  On systems that support sizing to
     *   contents, an "advisory" call to bannerSetSize() will simply be
     *   ignored in anticipation of the upcoming call to
     *   bannerSizeToContents().  On systems that don't support sizing to
     *   contents, an advisory call will actually resize the window.  
     */
    bannerSetSize(handle, size, sizeUnits, isAdvisory);

    /* 
     *   Create a log file console.  This creates a console that has no
     *   display, but simply captures its output to the given log file.
     *   Writing to a log console is different from writing to a regular text
     *   file in that we apply all of the normal formatting (including
     *   text-only-mode HTML interpretation) to the output sent to this
     *   console.  
     */
    logConsoleCreate(filename, charset, width);

    /* 
     *   Close a log console.  This closes the file associated with the log
     *   console and deletes the console object.  The given console handle is
     *   no longer valid after this function is called.  
     */
    logConsoleClose(handle);

    /* 
     *   Write text to a log console.  This works the same as tadsSay(), but
     *   writes the output to the given log console rather than to the main
     *   output window.  
     */
    logConsoleSay(handle, ...);

    /*
     *   Log an input event that's obtained externally - i.e., from a source
     *   other than the system input APIs (inputLine, inputKey, inputEvent,
     *   etc).  This adds the event to any command or event log that the
     *   system is currently writing, as set with setLogFile().
     *   
     *   It's only necessary to call this function when obtaining user input
     *   from custom code that bypasses the system input APIs.  The system
     *   input functions all log events automatically, so you must not call
     *   this for input obtained from them (doing so would write each input
     *   twice, since it's already being written once by the input
     *   functions).  For example, this is useful for the Web UI, since it
     *   obtains input via network transactions with the javascript client.
     *   
     *   'evt' is a list describing the event, using the same format that
     *   inputEvent() returns.  Note one special extension: if the first
     *   element of the list is a string, the string is used as the tag name
     *   if we're writing an event script.  This can be used to write custom
     *   events or events with no InEvtXxx type code, such as <dialog> input
     *   events.
     */
    logInputEvent(evt);
}

/* log file types */
#define LogTypeTranscript 1     /* log all input and output to a transcript */
#define LogTypeCommand    2                  /* log only command-line input */
#define LogTypeScript     3                         /* log all input events */

/* 
 *   The special log console handle for the main console window's transcript.
 *   This can be used as the handle in logConsoleSay(), to write text
 *   directly to the main console's log file, if any.  Note that this console
 *   cannot be closed via logConsoleClose(); use setLogFile(nil) instead.  
 */
#define MainWindowLogHandle  (-1)

/* 
 *   constants for the event codes returned by the inputEvent() and
 *   inputLineTimeout() intrinsic functions 
 */
#define InEvtKey         1
#define InEvtTimeout     2
#define InEvtHref        3
#define InEvtNoTimeout   4
#define InEvtNotimeout   4         /* (note minor capitalization variation) */
#define InEvtEof         5
#define InEvtLine        6
#define InEvtSysCommand  0x100
#define InEvtEndQuietScript  10000
#define InEvtEndScript       10003

/*
 *   Command codes for InEvtSysCommand 
 */
#define SaveCommand       0x0001               /* SAVE the current position */
#define RestoreCommand    0x0002                /* RESTORE a saved position */
#define UndoCommand       0x0003                           /* UNDO one turn */
#define QuitCommand       0x0004                           /* QUIT the game */
#define CloseCommand      0x0005                   /* close the game window */
#define HelpCommand       0x0006                          /* show game HELP */


/*
 *   constants for inputDialog() 
 */
#define InDlgOk              1
#define InDlgOkCancel        2
#define InDlgYesNo           3
#define InDlgYesNoCancel     4

#define InDlgIconNone        0
#define InDlgIconWarning     1
#define InDlgIconInfo        2
#define InDlgIconQuestion    3
#define InDlgIconError       4

#define InDlgLblOk           1
#define InDlgLblCancel       2
#define InDlgLblYes          3
#define InDlgLblNo           4

/*
 *   inputFile() dialog types 
 */
#define InFileOpen    1                /* open an existing file for reading */
#define InFileSave    2                                 /* save to the file */

/*
 *   inputFile() return codes - these are returned in the first element of
 *   the list returned from inputFile() 
 */
#define InFileSuccess        0    /* success - 2nd list element is filename */
#define InFileFailure        1       /* an error occurred asking for a file */
#define InFileCancel         2         /* player canceled the file selector */

/*
 *   constants for inputFile() file type codes 
 */
#define FileTypeLog     2                        /* a transcript (log) file */
#define FileTypeData    4                            /* arbitrary data file */
#define FileTypeCmd     5                             /* command input file */
#define FileTypeText    7                                      /* text file */
#define FileTypeBin     8                               /* binary data file */
#define FileTypeUnknown 11                             /* unknown file type */
#define FileTypeT3Image 12               /* T3 executable image (game) file */
#define FileTypeT3Save  15                           /* T3 saved state file */

/*
 *   constants for systemInfo information type codes 
 */
#define SysInfoVersion       2
#define SysInfoOsName        3
#define SysInfoJpeg          5
#define SysInfoPng           6
#define SysInfoWav           7
#define SysInfoMidi          8
#define SysInfoWavMidiOvl    9
#define SysInfoWavOvl        10
#define SysInfoPrefImages    11
#define SysInfoPrefSounds    12
#define SysInfoPrefMusic     13
#define SysInfoPrefLinks     14
#define SysInfoMpeg          15
#define SysInfoMpeg1         16
#define SysInfoMpeg2         17
#define SysInfoMpeg3         18
#define SysInfoLinksHttp     20
#define SysInfoLinksFtp      21
#define SysInfoLinksNews     22
#define SysInfoLinksMailto   23
#define SysInfoLinksTelnet   24
#define SysInfoPngTrans      25
#define SysInfoPngAlpha      26
#define SysInfoOgg           27
#define SysInfoMng           28
#define SysInfoMngTrans      29
#define SysInfoMngAlpha      30
#define SysInfoTextHilite    31
#define SysInfoTextColors    32
#define SysInfoBanners       33
#define SysInfoInterpClass   34
#define SysInfoAudioFade     35
#define SysInfoAudioCrossfade 36

/* SysInfoTextColors support level codes */
#define SysInfoTxcNone       0
#define SysInfoTxcParam      1
#define SysInfoTxcAnsiFg     2
#define SysInfoTxcAnsiFgBg   3
#define SysInfoTxcRGB        4

/* SysInfoInterpClass codes */
#define SysInfoIClassText    1
#define SysInfoIClassTextGUI 2
#define SysInfoIClassHTML    3

/* SysInfoAudioFade and SysInfoAudioCrossfade result codes */
#define SysInfoFadeMPEG      0x0001
#define SysInfoFadeOGG       0x0002
#define SysInfoFadeWAV       0x0004
#define SysInfoFadeMIDI      0x0008

/*
 *   constants for statusMode 
 */
#define StatModeNormal    0                       /* displaying normal text */
#define StatModeStatus    1                     /* display status line text */

/*
 *   flags for setScriptFile 
 */
#define ScriptFileQuiet    1  /* do not display output while reading script */
#define ScriptFileNonstop  2   /* turn off MORE prompt while reading script */
#define ScriptFileEvent    4        /* this is an event script (query only) */

/*
 *   Script Request - get current script status.  In 3.0.17+, passing this
 *   constant as the 'filename' argument to getScriptFile() will perform a
 *   "get script status" request.  If there's no script file in progress, the
 *   function returns nil.  If a script file is being read, the function
 *   returns an integer value giving a combination of ScriptFileXxx flag
 *   values indicating the current script status.  Note that a return value
 *   of 0 (zero) means that a script is running but none of the ScriptFileXxx
 *   flags are applicable.  
 */
#define ScriptReqGetStatus   0x7000

/*
 *   selectors for getLocalCharSet
 */
#define CharsetDisplay  1             /* the display/keyboard character set */
#define CharsetFileName 2                  /* the file system character set */
#define CharsetFileCont 3            /* default file contents character set */

/*
 *   banner insertion point specifies (for 'where' in bannerCreate)
 */
#define BannerFirst   1
#define BannerLast    2
#define BannerBefore  3
#define BannerAfter   4

/*
 *   banner types 
 */
#define BannerTypeText     1                 /* ordinary text stream window */
#define BannerTypeTextGrid 2                            /* text grid window */

/* 
 *   banner alignment types 
 */
#define BannerAlignTop     0
#define BannerAlignBottom  1
#define BannerAlignLeft    2
#define BannerAlignRight   3

/*
 *   banner size unit types 
 */
#define BannerSizePercent   1    /* size is a percentage of available space */
#define BannerSizeAbsolute  2    /* size is in natural units of window type */

/*
 *   banner style flags 
 */
#define BannerStyleBorder       0x0001       /* banner has a visible border */
#define BannerStyleVScroll      0x0002                /* vertical scrollbar */
#define BannerStyleHScroll      0x0004              /* horizontal scrollbar */
#define BannerStyleAutoVScroll  0x0008      /* automatic vertical scrolling */
#define BannerStyleAutoHScroll  0x0010    /* automatic horizontal scrolling */
#define BannerStyleTabAlign     0x0020           /* <TAB> alignment support */
#define BannerStyleMoreMode     0x0040                     /* use MORE mode */
#define BannerStyleHStrut       0x0080    /* include in parent's auto width */
#define BannerStyleVStrut       0x0100   /* include in parent's auto height */

/*
 *   Color codes.  A color can be specified with explicit RGB
 *   (red-green-blue) component values, or can be "parameterized," which
 *   means that the color uses a pre-defined color for a particular purpose.
 *   
 *   RGB colors are specified with each component given in the range 0 to
 *   255; the color (0,0,0) is pure black, and (255,255,255) is pure white.
 *   
 *   The special value "transparent" is not a color at all, but rather
 *   specifies that the current screen color should be used.
 *   
 *   The "Text" and "TextBg" colors are the current default text and text
 *   background colors.  The actual colors displayed for these values depend
 *   on the system, and on some systems these colors might be configurable by
 *   the user through a preferences selection.  These are the same colors
 *   selected by the HTML parameterized color names 'text' and 'bgcolor'.
 *   
 *   The "StatusText" and "StatusBg" colors are the current default
 *   statusline text and background colors, which depend on the system and
 *   may be user-configurable on some systems.  These are the same colors
 *   selected by the HTML parameterized color names 'statustext' and
 *   'statusbg'.
 *   
 *   The "input" color is the current default input text color.  
 */
#define ColorRGB(r, g, b) \
    ((((r) & 0xff) << 16) + (((g) & 0xff) << 8) + ((b) & 0xff))
#define ColorTransparent     0x01000000
#define ColorText            0x02000000
#define ColorTextBg          0x03000000
#define ColorStatusText      0x04000000
#define ColorStatusBg        0x05000000
#define ColorInput           0x06000000

/* some specific colors by name, for convenience */
#define ColorBlack    ColorRGB(0x00, 0x00, 0x00)
#define ColorWhite    ColorRGB(0xff, 0xff, 0xff)
#define ColorRed      ColorRGB(0xff, 0x00, 0x00)
#define ColorBlue     ColorRGB(0x00, 0x00, 0xFF)
#define ColorGreen    ColorRGB(0x00, 0x80, 0x00)
#define ColorYellow   ColorRGB(0xFF, 0xFF, 0x00)
#define ColorCyan     ColorRGB(0x00, 0xFF, 0xFF)
#define ColorAqua     ColorRGB(0x00, 0xFF, 0xFF)
#define ColorMagenta  ColorRGB(0xFF, 0x00, 0xFF)
#define ColorSilver   ColorRGB(0xC0, 0xC0, 0xC0)
#define ColorGray     ColorRGB(0x80, 0x80, 0x80)
#define ColorMaroon   ColorRGB(0x80, 0x00, 0x00)
#define ColorPurple   ColorRGB(0x80, 0x00, 0x80)
#define ColorFuchsia  ColorRGB(0xFF, 0x00, 0xFF)
#define ColorLime     ColorRGB(0x00, 0xFF, 0x00)
#define ColorOlive    ColorRGB(0x80, 0x80, 0x00)
#define ColorNavy     ColorRGB(0x00, 0x00, 0x80)
#define ColorTeal     ColorRGB(0x00, 0x80, 0x80)

