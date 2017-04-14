#charset "us-ascii"

/* 
 *   Copyright (c) 2000, 2006 Michael J. Roberts.  All Rights Reserved. 
 *   
 *   TADS 3 Library - output formatting
 *   
 *   This module defines the framework for displaying output text.  
 */

/* include the library header */
#include "adv3.h"


/* ------------------------------------------------------------------------ */
/*
 *   The standard library output function.  We set this up as the default
 *   display function (for double-quoted strings and for "<< >>"
 *   embeddings).  Code can also call this directly to display items.  
 */
say(val)
{
    /* send output to the active output stream */
    outputManager.curOutputStream.writeToStream(val);
}

/* ------------------------------------------------------------------------ */
/*
 *   Generate a string for showing quoted text.  We simply enclose the
 *   text in a <Q>...</Q> tag sequence and return the result.  
 */
withQuotes(txt)
{
    return '<q><<txt>></q>';
}

/* ------------------------------------------------------------------------ */
/*
 *   Output Manager.  This object contains global code for displaying text
 *   on the console.
 *   
 *   The output manager is transient because we don't want its state to be
 *   saved and restored; the output manager state is essentially part of
 *   the intepreter user interface, which is not affected by save and
 *   restore.  
 */
transient outputManager: object
    /*
     *   Switch to a new active output stream.  Returns the previously
     *   active output stream, so that the caller can easily restore the
     *   old output stream if the new output stream is to be established
     *   only for a specific duration.  
     */
    setOutputStream(ostr)
    {
        local oldStr;

        /* remember the old stream for a moment */
        oldStr = curOutputStream;

        /* set the new output stream */
        curOutputStream = ostr;

        /* 
         *   return the old stream, so the caller can restore it later if
         *   desired 
         */
        return oldStr;
    }

    /* 
     *   run the given function, using the given output stream as the
     *   active default output stream 
     */
    withOutputStream(ostr, func)
    {
        /* establish the new stream */
        local oldStr = setOutputStream(ostr);

        /* make sure we restore the old active stream on the way out */
        try
        {
            /* invoke the callback */
            (func)();
        }
        finally
        {
            /* restore the old output stream */
            setOutputStream(oldStr);
        }
    }

    /* the current output stream - start with the main text stream */
    curOutputStream = mainOutputStream

    /* 
     *   Is the UI running in HTML mode?  This tells us if we have a full
     *   HTML UI or a text-only UI.  Full HTML mode applies if we're
     *   running on a Multimedia TADS interpreter, or we're using the Web
     *   UI, which runs in a separate browser and is thus inherently
     *   HTML-capable.
     *   
     *   (The result can't change during a session, since it's a function
     *   of the game and interpreter capabilities, so we store the result
     *   on the first evaluation to avoid having to recompute it on each
     *   query.  Since 'self' is a static object, we'll recompute this each
     *   time we run the program, which is important because we could save
     *   the game on one interpreter and resume the session on a different
     *   interpreter with different capabilities.)  
     */
    htmlMode = (self.htmlMode = checkHtmlMode())
;

/* ------------------------------------------------------------------------ */
/*
 *   Output Stream.  This class provides a stream-oriented interface to
 *   displaying text on the console.  "Stream-oriented" means that we write
 *   text as a sequential string of characters.
 *   
 *   Output streams are always transient, since they track the system user
 *   interface in the interpreter.  The interpreter does not save its UI
 *   state with a saved position, so objects such as output streams that
 *   track the UI state should not be saved either.  
 */
class OutputStream: PreinitObject
    /*
     *   Write a value to the stream.  If the value is a string, we'll
     *   display the text of the string; if it's nil, we'll ignore it; if
     *   it's anything else, we'll try to convert it to a string (with the
     *   toString() function) and display the resulting text.  
     */
    writeToStream(val)
    {
        /* convert the value to a string */
        switch(dataType(val))
        {
        case TypeSString:
            /* 
             *   it's a string - no conversion is needed, but if it's
             *   empty, it doesn't count as real output (so don't notify
             *   anyone, and don't set any output flags) 
             */
            if (val == '')
                return;
            break;
            
        case TypeNil:
            /* nil - don't display anything for this */
            return;
            
        case TypeInt:
        case TypeObject:
            /* convert integers and objects to strings */
            val = toString(val);
            break;
        }

        /* run it through our output filters */
        val = applyFilters(val);

        /* 
         *   if, after filtering, we're not writing anything at all,
         *   there's nothing left to do 
         */
        if (val == nil || val == '')
            return;

        /* write the text to our underlying system stream */
        writeFromStream(val);
    }

    /*
     *   Watch the stream for output.  It's sometimes useful to be able to
     *   call out to some code and determine whether or not the code
     *   generated any text output.  This routine invokes the given
     *   callback function, monitoring the stream for output; if any
     *   occurs, we'll return true, otherwise we'll return nil.  
     */
    watchForOutput(func)
    {
        local mon;
        
        /* set up a monitor filter on the stream */
        addOutputFilter(mon = new MonitorFilter());

        /* catch any exceptions so we can remove our filter before leaving */
        try
        {
            /* invoke the callback */
            (func)();

            /* return the monitor's status, indicating if output occurred */
            return mon.outputFlag;
        }
        finally
        {
            /* remove our monitor filter */
            removeOutputFilter(mon);
        }
    }

    /*
     *   Call the given function, capturing all text output to this stream
     *   in the course of the function call.  Return a string containing
     *   the captured text.  
     */
    captureOutput(func, [args])
    {
        /* install a string capture filter */
        local filter = new StringCaptureFilter();
        addOutputFilter(filter);

        /* make sure we don't leave without removing our capturer */
        try
        {
            /* invoke the function */
            (func)(args...);

            /* return the text that we captured */
            return filter.txt_;
        }
        finally
        {
            /* we're done with our filter, so remove it */
            removeOutputFilter(filter);
        }
    }

    /* my associated input manager, if I have one */
    myInputManager = nil

    /* dynamic construction */
    construct()
    {
        /* 
         *   Set up filter list.  Output streams are always transient, so
         *   make our filter list transient as well.  
         */
        filterList_ = new transient Vector(10);
    }

    /* execute pre-initialization */
    execute()
    {
        /* do the same set-up we would do for dynamic construction */
        construct();
    }

    /*
     *   Write text out from this stream; this writes to the lower-level
     *   stream underlying this stream.  This routine is intended to be
     *   called only from within this class.
     *   
     *   Each output stream is conceptually "stacked" on top of another,
     *   lower-level stream.  At the bottom of the stack is usually some
     *   kind of physical device, such as the display, or a file on disk.
     *   
     *   This method must be defined in each subclass to write to the
     *   appropriate underlying stream.  Most subclasses are specifically
     *   designed to sit atop a system-level stream, such as the display
     *   output stream, so most implementations of this method will call
     *   directly to a system-level output function.
     */
    writeFromStream(txt) { }

    /* 
     *   The list of active filters on this stream, in the order in which
     *   they are to be called.  This should normally be initialized to a
     *   Vector in each instance.  
     */
    filterList_ = []

    /*
     *   Add an output filter.  The argument is an object of class
     *   OutputFilter, or any object implementing the filterText() method.
     *   
     *   Filters are always arranged in a "stack": the last output filter
     *   added is the first one called during output.  This method thus
     *   adds the new filter at the "top" of the stack.  
     */
    addOutputFilter(filter)
    {
        /* add the filter to the end of our list */
        filterList_.append(filter);
    }

    /*
     *   Add an output filter at a given point in the filter stack: add
     *   the filter so that it is "below" the given existing filter in the
     *   stack.  This means that the new filter will be called just after
     *   the existing filter during output.
     *   
     *   If 'existingFilter' isn't in the stack of existing filters, we'll
     *   add the new filter at the "top" of the stack.
     */
    addOutputFilterBelow(newFilter, existingFilter)
    {
        /* find the existing filter in our list */
        local idx = filterList_.indexOf(existingFilter);

        /* 
         *   If we found the old filter, add the new filter below the
         *   existing filter in the stack, which is to say just before the
         *   old filter in our vector of filters (since we call the
         *   filters in reverse order of the list).
         *   
         *   If we didn't find the existing filter, simply add the new
         *   filter at the top of the stack, by appending the new filter
         *   at the end of the list.  
         */
        if (idx != nil)
            filterList_.insertAt(idx, newFilter);
        else
            filterList_.append(newFilter);
    }

    /*
     *   Remove an output filter.  Since filters are arranged in a stack,
     *   only the LAST output filter added may be removed.  It's an error
     *   to remove a filter other than the last one.  
     */
    removeOutputFilter(filter)
    {
        /* get the filter count */
        local len = filterList_.length();

        /* make sure it's the last filter */
        if (len == 0 || filterList_[len] != filter)
            t3DebugTrace(T3DebugBreak);

        /* remove the filter from my list */
        filterList_.removeElementAt(len);
    }

    /* call the filters */
    applyFilters(val)
    {
        /* 
         *   Run through the list, applying each filter in turn.  We work
         *   backwards through the list from the last element, because the
         *   filter list is a stack: the last element added is the topmost
         *   element of the stack, so it must be called first.  
         */
        for (local i in filterList_.length()..1 step -1 ; val != nil ; )
            val = filterList_[i].filterText(self, val);

        /* return the result of all of the filters */
        return val;
    }

    /* 
     *   Apply the current set of text transformation filters to a string.
     *   This applies only the non-capturing filters; we skip any capture
     *   filters.  
     */
    applyTextFilters(val)
    {
        /* run through the filter stack from top to bottom */
        for (local i in filterList_.length()..1 step -1 ; val != nil ; )
        {
            /* skip capturing filters */
            local f = filterList_[i];
            if (f.ofKind(CaptureFilter))
                continue;

            /* apply the filter */
            val = f.filterText(self, val);
        }

        /* return the result */
        return val;
    }
        

    /*
     *   Receive notification from the input manager that we have just
     *   ended reading a line of input from the keyboard.
     */
    inputLineEnd()
    {
        /* an input line ending doesn't look like a paragraph */
        justDidPara = nil;
    }

    /* 
     *   Internal state: we just wrote a paragraph break, and there has
     *   not yet been any intervening text.  By default, we set this to
     *   true initially, so that we suppress any paragraph breaks at the
     *   very start of the text.  
     */
    justDidPara = true

    /*
     *   Internal state: we just wrote a character that suppresses
     *   paragraph breaks that immediately follow.  In this state, we'll
     *   suppress any paragraph marker that immediately follows, but we
     *   won't suppress any other characters.  
     */
    justDidParaSuppressor = nil
;

/*
 *   The OutputStream for the main text area.
 *   
 *   This object is transient because the output stream state is
 *   effectively part of the interpreter user interface, which is not
 *   affected by save and restore.  
 */
transient mainOutputStream: OutputStream
    /* 
     *   The main text area is the same place where we normally read
     *   command lines from the keyboard, so associate this output stream
     *   with the primary input manager. 
     */
    myInputManager = inputManager

    /* the current command transcript */
    curTranscript = nil

    /* we sit atop the system-level main console output stream */
    writeFromStream(txt)
    {
        /* if an input event was interrupted, cancel the event */
        inputManager.inputEventEnd();

        /* write the text to the console */
        aioSay(txt);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Paragraph manager.  We filter strings as they're about to be sent to
 *   the console to convert paragraph markers (represented in the source
 *   text using the "style tag" format, <.P>) into a configurable display
 *   rendering.
 *   
 *   We also process the zero-spacing paragraph, <.P0>.  This doesn't
 *   generate any output, but otherwise acts like a paragraph break in that
 *   it suppresses any paragraph breaks that immediately follow.
 *   
 *   The special marker <./P0> cancels the effect of a <.P0>.  This can be
 *   used if you want to ensure that a newline or paragraph break is
 *   displayed, even if a <.P0> was just displayed.
 *   
 *   Our special processing ensures that paragraph tags interact with one
 *   another and with other display elements specially:
 *   
 *   - A run of multiple consecutive paragraph tags is treated as a single
 *   paragraph tag.  This property is particularly important because it
 *   allows code to write out a paragraph marker without having to worry
 *   about whether preceding code or following code add paragraph markers
 *   of their own; if redundant markers are found, we'll filter them out
 *   automatically.
 *   
 *   - We can suppress paragraph markers following other specific
 *   sequences.  For example, if the paragraph break is rendered as a blank
 *   line, we might want to suppress an extra blank line for a paragraph
 *   break after an explicit blank line.
 *   
 *   - We can suppress other specific sequences following a paragraph
 *   marker.  For example, if the paragraph break is rendered as a newline
 *   plus a tab, we could suppress whitespace following the paragraph
 *   break.
 *   
 *   The paragraph manager should always be instantiated with transient
 *   instances, because this object's state is effectively part of the
 *   interpreter user interface, which doesn't participate in save and
 *   restore.  
 */
class ParagraphManager: OutputFilter
    /* 
     *   Rendering - this is what we display on the console to represent a
     *   paragraph break.  By default, we'll display a blank line.  
     */
    renderText = '\b'

    /*
     *   Flag: show or hide paragraph breaks immediately after input.  By
     *   default, we do not show paragraph breaks after an input line.  
     */
    renderAfterInput = nil

    /*
     *   Preceding suppression.  This is a regular expression that we
     *   match to individual characters.  If the character immediately
     *   preceding a paragraph marker matches this expression, we'll
     *   suppress the paragraph marker in the output.  By default, we'll
     *   suppress a paragraph break following a blank line, because the
     *   default rendering would add a redundant blank line.  
     */
    suppressBefore = static new RexPattern('\b')

    /*
     *   Following suppression.  This is a regular expression that we
     *   match to individual characters.  If the character immediately
     *   following a paragraph marker matches this expression, we'll
     *   suppress the character.  We'll apply this to each character
     *   following a paragraph marker in turn until we find one that does
     *   not match; we'll suppress all of the characters that do match.
     *   By default, we suppress additional blank lines after a paragraph
     *   break.  
     */
    suppressAfter = static new RexPattern('[\b\n]')

    /* pre-compile some regular expression patterns we use a lot */
    leadingMultiPat = static new RexPattern('(<langle><dot>[pP]0?<rangle>)+')
    leadingSinglePat = static new RexPattern(
        '<langle><dot>([pP]0?|/[pP]0)<rangle>')

    /* process a string that's about to be written to the console */
    filterText(ostr, txt)
    {
        local ret;
        
        /* we don't have anything in our translated string yet */
        ret = '';

        /* keep going until we run out of string to process */
        while (txt != '')
        {
            local len;
            local match;
            local p0;
            local unp0;
            
            /* 
             *   if we just wrote a paragraph break, suppress any
             *   character that matches 'suppressAfter', and suppress any
             *   paragraph markers that immediately follow 
             */
            if (ostr.justDidPara)
            {
                /* check for any consecutive paragraph markers */
                if ((len = rexMatch(leadingMultiPat, txt)) != nil)
                {
                    /* discard the consecutive <.P>'s, and keep going */
                    txt = txt.substr(len + 1);
                    continue;
                }

                /* check for a match to the suppressAfter pattern */
                if (rexMatch(suppressAfter, txt) != nil)
                {
                    /* discard the suppressed character and keep going */
                    txt = txt.substr(2);
                    continue;
                }
            }

            /* 
             *   we have a character other than a paragraph marker, so we
             *   didn't just scan a paragraph marker 
             */
            ostr.justDidPara = nil;

            /*
             *   if we just wrote a suppressBefore character, discard any
             *   leading paragraph markers 
             */
            if (ostr.justDidParaSuppressor
                && (len = rexMatch(leadingMultiPat, txt)) != nil)
            {
                /* remove the paragraph markers */
                txt = txt.substr(len + 1);

                /* 
                 *   even though we're not rendering the paragraph, note
                 *   that a logical paragraph just started 
                 */
                ostr.justDidPara = true;

                /* keep going */
                continue;
            }

            /* presume we won't find a <.p0> or <./p0> */
            p0 = unp0 = nil;

            /* find the next paragraph marker */
            match = rexSearch(leadingSinglePat, txt);
            if (match == nil)
            {
                /* 
                 *   there are no more paragraph markers - copy the
                 *   remainder of the input string to the output
                 */
                ret += txt;
                txt = '';

                /* we just did something other than a paragraph */
                ostr.justDidPara = nil;
            }
            else
            {
                /* add everything up to the paragraph break to the output */
                ret += txt.substr(1, match[1] - 1);

                /* get the rest of the string following the paragraph mark */
                txt = txt.substr(match[1] + match[2]);

                /* note if we found a <.p0> or <./p0> */
                p0 = (match[3] is in ('<.p0>', '<.P0>'));
                unp0 = (match[3] is in ('<./p0>', '<./P0>'));

                /* 
                 *   note that we just found a paragraph marker, unless
                 *   this is a <./p0> 
                 */
                ostr.justDidPara = !unp0;
            }

            /* 
             *   If the last character we copied out is a suppressBefore
             *   character, note for next time that we have a suppressor
             *   pending.  Likewise, if we found a <.p0> rather than a
             *   <.p>, this counts as a suppressor.  
             */
            ostr.justDidParaSuppressor =
                (p0 || rexMatch(suppressBefore,
                                ret.substr(ret.length(), 1)) != nil);

            /* 
             *   if we found a paragraph marker, and we didn't find a
             *   leading suppressor character just before it, add the
             *   paragraph rendering 
             */
            if (ostr.justDidPara && !ostr.justDidParaSuppressor)
                ret += renderText;
        }

        /* return the translated string */
        return ret;
    }
;

/* the paragraph manager for the main output stream */
transient mainParagraphManager: ParagraphManager
;

/* ------------------------------------------------------------------------ */
/*
 *   Output Filter
 */
class OutputFilter: object
    /* 
     *   Apply the filter - this should be overridden in each filter.  The
     *   return value is the result of filtering the string.
     *   
     *   'ostr' is the OutputStream to which the text is being written,
     *   and 'txt' is the original text to be displayed.  
     */
    filterText(ostr, txt) { return txt; }
;


/* ------------------------------------------------------------------------ */
/*
 *   Output monitor filter.  This is a filter that leaves the filtered
 *   text unchanged, but keeps track of whether any text was seen at all.
 *   Our 'outputFlag' is true if we've seen any output, nil if not.
 */
class MonitorFilter: OutputFilter
    /* filter text */
    filterText(ostr, val)
    {
        /* if the value is non-empty, note the output */
        if (val != nil && val != '')
            outputFlag = true;

        /* return the input value unchanged */
        return val;
    }

    /* flag: has any output occurred for this monitor yet? */
    outputFlag = nil
;


/* ------------------------------------------------------------------------ */
/*
 *   Capture Filter.  This is an output filter that simply captures all of
 *   the text sent through the filter, sending nothing out to the
 *   underlying stream.
 *   
 *   The default implementation simply discards the incoming text.
 *   Subclasses can keep track of the text in memory, in a file, or
 *   wherever desired.  
 */
class CaptureFilter: OutputFilter
    /*
     *   Filter the text.  We simply discard the text, passing nothing
     *   through to the underlying stream. 
     */
    filterText(ostr, txt)
    {
        /* leave nothing for the underlying stream */
        return nil;
    }
;

/*
 *   "Switchable" capture filter.  This filter can have its blocking
 *   enabled or disabled.  When blocking is enabled, we capture
 *   everything, leaving nothing to the underlying stream; when disabled,
 *   we pass everything through to the underyling stream unchanged.  
 */
class SwitchableCaptureFilter: CaptureFilter
    /* filter the text */
    filterText(ostr, txt)
    {
        /* 
         *   if we're blocking output, return nothing to the underlying
         *   stream; if we're disabled, return the input unchanged 
         */
        return (isBlocking ? nil : txt);
    }

    /*
     *   Blocking enabled: if this is true, we'll capture all text passed
     *   through us, leaving nothing to the underyling stream.  Blocking
     *   is enabled by default.  
     */
    isBlocking = true
;

/*
 *   String capturer.  This is an implementation of CaptureFilter that
 *   saves the captured text to a string.  
 */
class StringCaptureFilter: CaptureFilter
    /* filter text */
    filterText(ostr, txt)
    {
        /* add the text to my captured text so far */
        addText(txt);
    }

    /* add to my captured text */
    addText(txt)
    {
        /* append the text to my string of captured text */
        txt_ += txt;
    }

    /* my captured text so far */
    txt_ = ''
;

/* ------------------------------------------------------------------------ */
/*
 *   Style tag.  This defines an HTML-like tag that can be used in output
 *   text to display an author-customizable substitution string.
 *   
 *   Each StyleTag object defines the name of the tag, which can be
 *   invoked in output text using the syntax "<.name>" - we require the
 *   period after the opening angle-bracket to plainly distinguish the
 *   sequence as a style tag, not a regular HTML tag.
 *   
 *   Each StyleTag also defines the text string that should be substituted
 *   for each occurrence of the "<.name>" sequence in output text, and,
 *   optionally, another string that is substituted for occurrences of the
 *   "closing" version of the tag, invoked with the syntax "<./name>".  
 */
class StyleTag: object
    /* name of the tag - the tag appears in source text in <.xxx> notation */
    tagName = ''

    /* 
     *   opening text - this is substituted for each instance of the tag
     *   without a '/' prefix 
     */
    openText = ''

    /* 
     *   Closing text - this is substituted for each instance of the tag
     *   with a '/' prefix (<./xxx>).  Note that non-container tags don't
     *   have closing text at all.  
     */
    closeText = ''
;

/*
 *   HtmlStyleTag - this is a subclass of StyleTag that provides different
 *   rendering depending on whether the interpreter is in HTML mode or not.
 *   In HTML mode, we display our htmlOpenText and htmlCloseText; when not
 *   in HTML mode, we display our plainOpenText and plainCloseText.
 */
class HtmlStyleTag: StyleTag
    openText = (outputManager.htmlMode ? htmlOpenText : plainOpenText)

    closeText = (outputManager.htmlMode ? htmlCloseText : plainCloseText)

    /* our HTML-mode opening and closing text */
    htmlOpenText = ''
    htmlCloseText = ''

    /* our plain (non-HTML) opening and closing text */
    plainOpenText = ''
    plainCloseText = ''
;

/*
 *   Define our default style tags.  We name all of these StyleTag objects
 *   so that authors can easily change the expansion text strings at
 *   compile-time with the 'modify' syntax, or dynamically at run-time by
 *   assigning new strings to the appropriate properties of these objects.
 */

/* 
 *   <.roomname> - we use this to display the room's name in the
 *   description of a room (such as in a LOOK AROUND command, or when
 *   entering a new location).  By default, we display the room name in
 *   boldface on a line by itself.  
 */
roomnameStyleTag: StyleTag 'roomname' '\n<b>' '</b><br>\n';

/* <.roomdesc> - we use this to display a room's long description */
roomdescStyleTag: StyleTag 'roomdesc' '' '';

/* 
 *   <.roompara> - we use this to separate paragraphs within a room's long
 *   description 
 */
roomparaStyleTag: StyleTag 'roompara' '<.p>\n';

/* 
 *   <.inputline> - we use this to display the text actually entered by the
 *   user on a command line.  Note that this isn't used for the prompt text
 *   - it's used only for the command-line text itself.  
 */
inputlineStyleTag: HtmlStyleTag 'inputline'
    /* in HTML mode, switch in and out of TADS-Input font */
    htmlOpenText = '<font face="tads-input">'
    htmlCloseText = '</font>'

    /* in plain mode, do nothing */
    plainOpenText = ''
    plainCloseText = ''
;

/*
 *   <.a> (named in analogy to the HTML <a> tag) - we use this to display
 *   hyperlinked text.  Note that this goes *inside* an HTML <a> tag - this
 *   doesn't do the actual linking (the true <a> tag does that), but rather
 *   allows customized text formatting for hyperlinked text.  
 */
hyperlinkStyleTag: HtmlStyleTag 'a'
;

/* <.statusroom> - style for the room name in a status line */
statusroomStyleTag: HtmlStyleTag 'statusroom'
    htmlOpenText = '<b>'
    htmlCloseText = '</b>'
;

/* <.statusscore> - style for the score in a status line */
statusscoreStyleTag: HtmlStyleTag 'statusscore'
    htmlOpenText = '<i>'
    htmlCloseText = '</i>'
;

/* 
 *   <.parser> - style for messages explicitly from the parser.
 *   
 *   By default, we do nothing special with these messages.  Many games
 *   like to use a distinctive notation for parser messages, to make it
 *   clear that the messages are "meta" text that's not part of the story
 *   but rather specific to the game mechanics; one common convention is
 *   to put parser messages in [square brackets].
 *   
 *   If the game defines a special appearance for parser messages, for
 *   consistency it might want to use the same appearance for notification
 *   messages displayed with the <.notification> tag (see
 *   notificationStyleTag).  
 */
parserStyleTag: StyleTag 'parser'
    openText = ''
    closeText = ''
;

/* 
 *   <.notification> - style for "notification" messages, such as score
 *   changes and messages explaining how facilities (footnotes, exit
 *   lists) work the first time they come up.
 *   
 *   By default, we'll put notifications in parentheses.  Games that use
 *   [square brackets] for parser messages (i.e., for the <.parser> tag)
 *   might want to use the same notation here for consistency.  
 */
notificationStyleTag: StyleTag 'notification'
    openText = '('
    closeText = ')'
;

/*
 *   <.assume> - style for "assumption" messages, showing an assumption
 *   the parser is making.  This style is used for showing objects used by
 *   default when not specified in a command, objects that the parser
 *   chose despite some ambiguity, and implied commands.  
 */
assumeStyleTag: StyleTag 'assume'
    openText = '('
    closeText = ')'
;

/*
 *   <.announceObj> - style for object announcement messages.  The parser
 *   shows an object announcement for each object when a command is applied
 *   to multiple objects (TAKE ALL, DROP KEYS AND WALLET).  The
 *   announcement simply shows the object's name and a colon, to let the
 *   player know that the response text that follows applies to the
 *   announced object.  
 */
announceObjStyleTag: StyleTag 'announceObj'
    openText = '<b>'
    closeText = '</b>'
;

/* ------------------------------------------------------------------------ */
/*
 *   "Style tag" filter.  This is an output filter that expands our
 *   special style tags in output text.  
 */
styleTagFilter: OutputFilter, PreinitObject
    /* pre-compile our frequently-used tag search pattern */
    tagPattern = static new RexPattern(
        '<nocase><langle>%.(/?[a-z][a-z0-9]*)<rangle>')

    /* filter for a style tag */
    filterText(ostr, val)
    {
        local idx;
        
        /* search for our special '<.xxx>' tags, and expand any we find */
        idx = rexSearch(tagPattern, val);
        while (idx != nil)
        {
            local xlat;
            local afterOfs;
            local afterStr;
            
            /* ask the formatter to translate it */
            xlat = translateTag(rexGroup(1)[3]);

            /* get the part of the string that follows the tag */
            afterOfs = idx[1] + idx[2];
            afterStr = val.substr(idx[1] + idx[2]);

            /* 
             *   if we got a translation, replace it; otherwise, leave the
             *   original text intact 
             */
            if (xlat != nil)
            {
                /* replace the tag with its translation */
                val = val.substr(1, idx[1] - 1) + xlat + afterStr;

                /* 
                 *   figure the offset of the remainder of the string in
                 *   the replaced version of the string - this is the
                 *   length of the original part up to the replacement
                 *   text plus the length of the replacement text 
                 */
                afterOfs = idx[1] + xlat.length();
            }

            /* 
             *   search for the next tag, considering only the part of
             *   the string following the replacement text - we do not
             *   want to re-scan the replacement text for tags 
             */
            idx = rexSearch(tagPattern, afterStr);
                
            /* 
             *   If we found it, adjust the starting index of the match to
             *   its position in the actual string.  Note that we do this
             *   by adding the OFFSET of the remainder of the string,
             *   which is 1 less than its INDEX, because idx[1] is already
             *   a string index.  (An offset is one less than an index
             *   because the index of the first character is 1.)  
             */
            if (idx != nil)
                idx[1] += afterOfs - 1;
        }

        /* return the filtered value */
        return val;
    }

    /*
     *   Translate a tag 
     */
    translateTag(tag)
    {
        local isClose;
        local styleTag;
        
        /* if it's a close tag, so note and remove the leading slash */
        isClose = tag.startsWith('/');
        if (isClose)
            tag = tag.substr(2);

        /* look up the tag object in our table */
        styleTag = tagTable[tag];

        /* 
         *   if we found it, return the open or close text, as
         *   appropriate; otherwise return nil 
         */
        return (styleTag != nil
                ? (isClose ? styleTag.closeText : styleTag.openText)
                : nil);
    }

    /* preinitialization */
    execute()
    {
        /* create a lookup table for our style table */
        tagTable = new LookupTable();
        
        /* 
         *   Populate the table with all of the StyleTag instances.  Key
         *   by tag name, storing the tag object as the value for each
         *   key.  This will let us efficiently look up the StyleTag
         *   object given a tag name string.
         */
        forEachInstance(StyleTag, { tag: tagTable[tag.tagName] = tag });
    }

    /*
     *   Our tag translation table.  We'll initialize this during preinit
     *   to a lookup table with all of the defined StyleTag objects.  
     */
    tagTable = nil
;


/* ------------------------------------------------------------------------ */
/*
 *   MessageBuilder - this object provides a general text substitution
 *   mechanism.  Text to be substituted is enclosed in {curly braces}.
 *   Within the braces, we have the substitution parameter name, which can
 *   be in the following formats:
 *   
 *   id
 *.  id obj
 *.  id1/id2 obj
 *.  id1 obj/id2
 *   
 *   The ID string gives the type of substitution to perform.  The ID's
 *   all come from a table, which is specified by the language-specific
 *   subclass, so the ID's can vary by language (to allow for natural
 *   template-style parameter names for each language).  If the ID is in
 *   two pieces (id1 and id2), we concatenate the two pieces together with
 *   a slash between to form the name we seek in the table - so {the/he
 *   dobj} and {the dobj/he} are equivalent, and both look up the
 *   identifier 'the/he'.  If a two-part identifier is given, and the
 *   identifier isn't found in the table, we'll try looking it up with the
 *   parts reversed: if we see {he/the dobj}, we'll first try finding
 *   'he/the', and if that fails we'll look for 'the/he'.
 *   
 *   If 'obj' is present, it specificies the target object providing the
 *   text to be substitutued; this is a string passed to the current
 *   Action, and is usually something like 'actor', 'dobj', or 'iobj'.
 *   
 *   One instance of this class, called langMessageBuilder, should be
 *   created by the language-specific library.  
 */
class MessageBuilder: OutputFilter, PreinitObject
    /* pre-compile some regular expressions we use a lot */
    patUpper = static new RexPattern('<upper>')
    patAllCaps = static new RexPattern('<upper><upper>')
    patIdObjSlashId = static new RexPattern(
        '(<^space|/>+)<space>+(<^space|/>+)(/<^space|/>+)')
    patIdObj = static new RexPattern('(<^space>+)<space>+(<^space>+)')
    patIdSlash = static new RexPattern('([^/]+)/([^/]+)')

    /*
     *   Given a source string with substitution parameters, generate the
     *   expanded message with the appropriate text in place of the
     *   parameters. 
     */
    generateMessage(orig)
    {
        local result;

        /* we have nothing in the result string so far */
        result = '';

        /* keep going until we run out of substitution parameters */
        for (;;)
        {
            local idx;
            local paramStr;
            local paramName;
            local paramObj;
            local info;
            local initCap, allCaps;
            local targetObj;
            local newText;
            local prop;

            /* get the position of the next brace */
            idx = orig.find('{');

            /* 
             *   if there are no braces, the rest of the string is simply
             *   literal text; add the entire remainder of the string to
             *   the result, and we're done 
             */
            if (idx == nil)
            {
                result += processResult(genLiteral(orig));
                break;
            }

            /* add everything up to the brace to the result string */
            result += processResult(genLiteral(orig.substr(1, idx - 1)));

            /* 
             *   lop off everything up to and including the brace from the
             *   source string, since we're done with the part up to the
             *   brace now 
             */
            orig = orig.substr(idx + 1);
            
            /* 
             *   if the brace was the last thing in the source string, or
             *   it's a stuttered brace, add it literally to the result 
             */
            if (orig.length() == 0)
            {
                /* 
                 *   nothing follows - add a literal brace to the result,
                 *   and we're done 
                 */
                result += processResult('{');
                break;
            }
            else if (orig.substr(1, 1) == '{')
            {
                /* 
                 *   it's a stuttered brace - add a literal brace to the
                 *   result 
                 */
                result += processResult('{');

                /* remove the second brace from the source */
                orig = orig.substr(2);

                /* we're finished processing this brace - go back for more */
                continue;
            }

            /* find the closing brace */
            idx = orig.find('}');

            /* 
             *   if there is no closing brace, include the brace and
             *   whatever follows as literal text 
             */
            if (idx == nil)
            {
                /* add the literal brace to the result */
                result += processResult('{');

                /* we're done with the brace - go back for more */
                continue;
            }

            /* 
             *   Pull out everything up to the brace as the parameter
             *   text.
             */
            paramStr = orig.substr(1, idx - 1);

            /* assume for now that we will have no parameter object */
            paramObj = nil;

            /* 
             *   drop everything up to and including the closing brace,
             *   since we've pulled it out for processing now 
             */
            orig = orig.substr(idx + 1);

            /* 
             *   Note the capitalization of the first two letters.  If
             *   they're both lower-case, we won't adjust the case of the
             *   substitution text at all.  If the first is a capital and
             *   the second isn't, we'll capitalize the first letter of
             *   the replacement text.  If they're both capitals, we'll
             *   capitalize the entire replacement text. 
             */
            initCap = (rexMatch(patUpper, paramStr) != nil);
            allCaps = (rexMatch(patAllCaps, paramStr) != nil);

            /* lower-case the entire parameter string for matching */
            local origParamStr = paramStr;
            paramStr = paramStr.toLower();

            /* perform any language-specific rewriting on the string */
            paramStr = langRewriteParam(paramStr);

            /*
             *   Figure out which format we have.  The allowable formats
             *   are:
             *   
             *   id
             *   obj
             *.  id obj
             *.  id1/id2
             *.  id1/id2 obj
             *.  id1 obj/id2
             */
            if (rexMatch(patIdObjSlashId, paramStr) != nil)
            {
                /* we have the id1 obj/id2 format */
                paramName = rexGroup(1)[3] + rexGroup(3)[3];
                paramObj = rexGroup(2)[3];
            }
            else if (rexMatch(patIdObj, paramStr) != nil)
            {
                /* we have 'id obj' or 'id1/id2 obj' */
                paramName = rexGroup(1)[3];
                paramObj = rexGroup(2)[3];
            }
            else
            {
                /* we have no spaces, so we have no target object */
                paramName = paramStr;
                paramObj = nil;
            }

            /* look up our parameter name */
            info = paramTable_[paramName];

            /*
             *   If we didn't find it, and the parameter name contains a
             *   slash ('/'), try reversing the order of the parts before
             *   and after the slash. 
             */
            if (info == nil && rexMatch(patIdSlash, paramName) != nil)
            {
                /* 
                 *   rebuild the name with the order of the parts
                 *   reversed, and look up the result 
                 */
                info = paramTable_[rexGroup(2)[3] + '/' + rexGroup(1)[3]];
            }

            /*
             *   If we didn't find a match, simply put the entire thing in
             *   the result stream literally, including the braces. 
             */
            if (info == nil)
            {
                /* 
                 *   We didn't find it, so try treating it as a string
                 *   parameter object.  Try getting the string from the
                 *   action.  
                 */
                newText = (gAction != nil
                           ? gAction.getMessageParam(paramName) : nil);

                /* check what we found */
                if (dataType(newText) == TypeSString)
                {
                    /* 
                     *   It's a valid string parameter.  Simply add the
                     *   literal text to the result.  If we're in html
                     *   mode, translate the string to ensure that any
                     *   markup-significant characters are properly quoted
                     *   so that they aren't taken as html themselves.  
                     */
                    result += processResult(newText.htmlify());
                }
                else
                {
                    /* 
                     *   the parameter is completely undefined; simply add
                     *   the original text, including the braces 
                     */
                    result += processResult('{<<origParamStr>>}');
                }
                    
                /* 
                 *   we're done with this substitution string - go back
                 *   for more 
                 */
                continue;
            }

            /*
             *   If we have no target object specified in the substitution
             *   string, and the parameter name has an associated implicit
             *   target object, use the implied object. 
             */
            if (paramObj == nil && info[3] != nil)
                paramObj = info[3];

            /* 
             *   If we have a target object name, ask the current action
             *   for the target object value.  Otherwise, use the same
             *   target object as the previous expansion.  
             */
            if (paramObj != nil)
            {
                /* check for a current action */
                if (gAction != nil)
                {
                    /* get the target object by name through the action */
                    targetObj = gAction.getMessageParam(paramObj);
                }
                else
                {
                    /* there's no action, so we don't have a value yet */
                    targetObj = nil;
                }

                /* 
                 *   if we didn't find a value, look up the name in our
                 *   global name table 
                 */
                if (targetObj == nil)
                {
                    /* look up the name */
                    targetObj = nameTable_[paramObj];

                    /* 
                     *   if we found it, and the result is a function
                     *   pointer or an anonymous function, invoke the
                     *   function to get the result 
                     */
                    if (dataTypeXlat(targetObj) == TypeFuncPtr)
                    {
                        /* evaluate the function */
                        targetObj = (targetObj)();
                    }
                }

                /* 
                 *   remember this for next time, in case the next
                 *   substitution string doesn't include a target object 
                 */
                lastTargetObj_ = targetObj;
                lastParamObj_ = paramObj;
            }
            else
            {
                /* 
                 *   there's no implied or explicit target - use the same
                 *   one as last time 
                 */
                targetObj = lastTargetObj_;
                paramObj = lastParamObj_;
            }

            /* 
             *   if the target object wasn't found, treat the whole thing
             *   as a failure - put the entire parameter string back in
             *   the result stream literally 
             */
            if (targetObj == nil)
            {
                /* add it to the output literally, and go back for more */
                result += processResult('{' + paramStr + '}');
                continue;
            }

            /* get the property to call on the target */
            prop = getTargetProp(targetObj, paramObj, info);

            /* evaluate the parameter's associated property on the target */
            newText = targetObj.(prop);

            /* apply the appropriate capitalization to the result */
            if (allCaps)
                newText = newText.toUpper();
            else if (initCap)
                newText = newText.substr(1, 1).toUpper() + newText.substr(2);

            /* 
             *   append the new text to the output result so far, and
             *   we're finished with this round 
             */
            result += processResult(newText);
        }

        /* return the result string */
        return result;
    }

    /*
     *   Get the property to invoke on the target object for the given
     *   parameter information entry.  By default, we simply return
     *   info[2], which is the standard property to call on the target.
     *   This can be overridden by the language-specific subclass to
     *   provide a different property if appropriate.
     *   
     *   'targetObj' is the target object, and 'paramObj' is the parameter
     *   name of the target object.  For example, 'paramObj' might be the
     *   string 'dobj' to represent the direct object, in which case
     *   'targetObj' will be the gDobj object.
     *   
     *   The English version, for example, uses this routine to supply a
     *   reflexive instead of the default entry when the target object
     *   matches the subject of the sentence.  
     */
    getTargetProp(targetObj, paramObj, info)
    {
        /* return the standard property mapping from the parameter info */
        return info[2];
    }

    /*
     *   Process result text.  This takes some result text that we're
     *   about to add and returns a processed version.  This is called for
     *   all text as we add it to the result string.
     *   
     *   The text we pass to this method has already had all parameter
     *   text fully expanded, so this routine does not need to worry about
     *   { } sequences - all { } sequences will have been removed and
     *   replaced with the corresponding expansion text before this is
     *   called.
     *   
     *   This routine is called piecewise: the routine will be called once
     *   for each parameter replacement text and once for each run of text
     *   between parameters, and is called in the order in which the text
     *   appears in the original string.
     *   
     *   By default we do nothing with the result text; we simply return
     *   the original text unchanged.  The language-specific subclass can
     *   override this as desired to further modify the text for special
     *   language-specific parameterization outside of the { } mechanism.
     *   The subclass can also use this routine to maintain internal state
     *   that depends on sentence structure.  For example, the English
     *   version looks for sentence-ending punctuation so that it can
     *   reset its internal notion of the subject of the sentence when a
     *   sentence appears to be ending.  
     */
    processResult(txt) { return txt; }

    /*
     *   "Quote" a message - double each open or close brace, so that braces in
     *   the message will be taken literally when run through the substitution
     *   replacer.  This can be used when a message is expanded prior to being
     *   displayed to ensure that braces in the result won't be mistaken as
     *   substitution parameters requiring further expansion.
     *
     *   Note that only open braces need to be quoted, since lone close braces
     *   are ignored in the substitution process.
     */
    quoteMessage(str)
    {
        return str.findReplace(['{', '}'], ['{{', '}}'], ReplaceAll);
    }

    /*
     *   Internal routine - generate the literal text for the given source
     *   string.  We'll remove any stuttered close braces. 
     */
    genLiteral(str)
    {
        /* replace all '}}' sequences with '}' sequences */
        return str.findReplace('}}', '}', ReplaceAll);
    }

    /*
     *   execute pre-initialization 
     */
    execute()
    {
        /* create a lookup table for our parameter names */
        paramTable_ = new LookupTable();

        /* add each element of our list to the table */
        foreach (local cur in paramList_)
            paramTable_[cur[1]] = cur;

        /* create a lookup table for our global names */
        nameTable_ = new LookupTable();

        /* 
         *   Add an entry for 'actor', which resolves to gActor if there is
         *   a gActor when evaluated, or the current player character if
         *   not.  Note that using a function ensures that we evaluate the
         *   current gActor or gPlayerChar each time we need the 'actor'
         *   value.  
         */
        nameTable_['actor'] = {: gActor != nil ? gActor : gPlayerChar };
    }

    /*
     *   Our output filter method.  We'll run each string written to the
     *   display through our parameter substitution method.  
     */
    filterText(ostr, txt)
    {
        /* substitute any parameters in the string and return the result */
        return generateMessage(txt);
    }

    /*
     *   The most recent target object.  Each time we parse a substitution
     *   string, we'll remember the target object here; when a
     *   substitution string doesn't imply or specify a target object,
     *   we'll use the previous one by default. 
     */
    lastTargetObj_ = nil

    /* the parameter name of the last target object ('dobj', 'actor', etc) */
    lastParamObj_ = nil

    /* our parameter table - a LookupTable that we set up during preinit */
    paramTable_ = nil

    /* our global name table - a LookupTable we set up during preinit */
    nameTable_ = nil

    /*
     *   Rewrite the parameter string for any language-specific rules.  By
     *   default, we'll return the original parameter string unchanged;
     *   the language-specific instance can override this to provide any
     *   special syntax extensions to the parameter string syntax desired
     *   by the language-specific library.  The returned string must be in
     *   one of the formats recognized by the generic handler.  
     */
    langRewriteParam(paramStr)
    {
        /* by default, return the original unchanged */
        return paramStr;
    }

    /* 
     *   our parameter list - this should be initialized in the
     *   language-specific subclass to a list like this:
     *   
     *   [entry1, entry2, entry3, ...]
     *   
     *   Each entry is a list like this:
     *   
     *   [paramName, &prop, impliedTargetName, <extra>]
     *   
     *   paramName is a string giving the substitution parameter name;
     *   this can be one word or two ('the' or 'the obj', for example).
     *   
     *   prop is a property identifier.  This is the property invoked on
     *   the target object to obtain the substitution text.
     *   
     *   impliedTargetName is a string giving the target object name to
     *   use.  When this is supplied, the paramName is normally used in
     *   message text with no object name.  This should be nil for
     *   parameters that do not imply a particular target.
     *   
     *   <extra> is any number of additional parameters for the
     *   language-specific subclass.  The generic code ignores these extra
     *   parameters, but the langague-specific subclass can use them if it
     *   requires additional information.
     *   
     *   Here's an example:
     *   
     *   paramList_ = [
     *.                ['you', &theDesc, nil, 'actor'],
     *.                ['the obj' &theObjDesc, &itReflexive, nil]
     *.  ]
     *   
     *   The first item specifies a substitution name of 'you', which is
     *   expanded by evaluating the property theDesc on the target object,
     *   and specifies an implied target object of 'actor'.  When this is
     *   expanded, we'll call the current action to get the meaning of
     *   'actor', then evaulate property theDesc on the result.
     *   
     *   The second item specifies a substitution name of 'the obj',
     *   expanded by evaluating property theObjDesc on the target object.
     *   This one doesn't have an implied object, so the target object is
     *   the one explicitly given in the message source text or is the
     *   previous target object if one isn't specified in the message
     *   text.  
     */
    paramList_ = []
;


/* ------------------------------------------------------------------------ */
/*
 *   Command Sequencer Filter.  This is an output filter that handles the
 *   special <.commandsep> tag for visual command separation.  This tag has
 *   the form of a style tag, but must be processed specially.
 *   
 *   <.commandsep> shows an appropriate separator between commands.  Before
 *   the first command output or after the last command output, this has no
 *   effect.  A run of multiple consecutive <.commandsep> tags is treated
 *   as a single tag.
 *   
 *   Between commands, we show gLibMessages.commandResultsSeparator.  After
 *   an input line and before the first command result text, we show
 *   gLibMessages.commandResultsPrefix.  After the last command result text
 *   before a new input line, we show gLibMessages.commandResultsSuffix.
 *   If we read two input lines, and there is no intervening text output at
 *   all, we show gLibMessages.commandResultsEmpty.
 *   
 *   The input manager should write a <.commandbefore> tag whenever it
 *   starts reading a command line, and a <.commandafter> tag whenever it
 *   finishes reading a command line.  
 */
enum stateReadingCommand, stateBeforeCommand, stateBeforeInterruption,
    stateInCommand, stateBetweenCommands, stateWriteThrough,
    stateNoCommand;

transient commandSequencer: OutputFilter
    /*
     *   Force the sequencer into mid-command mode.  This can be used to
     *   defeat the resequencing into before-results mode that occurs if
     *   any interactive command-line input must be read in the course of
     *   a command's execution.  
     */
    setCommandMode() { state_ = stateInCommand; }

    /*
     *   Internal routine: write the given text directly through us,
     *   skipping any filtering we'd otherwise apply. 
     */
    writeThrough(txt)
    {
        local oldState;

        /* remember our old state */
        oldState = state_;

        /* set our state to write-through */
        state_ = stateWriteThrough;

        /* make sure we reset things on the way out */
        try
        {
            /* write the text */
            say(txt);
        }
        finally
        {
            /* restore our old state */
            state_ = oldState;
        }
    }

    /* pre-compile our tag sequence pattern */
    patNextTag = static new RexPattern(
        '<nocase><langle><dot>'
        + 'command(sep|int|before|after|none|mid)'
        + '<rangle>')

    /*
     *   Apply our filter 
     */
    filterText(ostr, txt)
    {
        local ret;
        
        /* 
         *   if we're in write-through mode, simply pass the text through
         *   unchanged 
         */
        if (state_ == stateWriteThrough)
            return txt;

        /* scan for tags */
        for (ret = '' ; txt != '' ; )
        {
            local match;
            local cur;
            local tag;
            
            /* search for our next special tag sequence */
            match = rexSearch(patNextTag, txt);

            /* check to see if we found a tag */
            if (match == nil)
            {
                /* no more tags - the rest of the text is plain text */
                cur = txt;
                txt = '';
                tag = nil;
            }
            else
            {
                /* found a tag - get the plain text up to the tag */
                cur = txt.substr(1, match[1] - 1);
                txt = txt.substr(match[1] + match[2]);

                /* get the tag name */
                tag = rexGroup(1)[3];
            }

            /* process the plain text up to the tag, if any */
            if (cur != '')
            {
                /* check our state */
                switch(state_)
                {
                case stateReadingCommand:
                case stateWriteThrough:
                case stateInCommand:
                case stateNoCommand:
                    /* we don't need to add anything in these states */
                    break;

                case stateBeforeCommand:
                    /* 
                     *   We're waiting for the first command output, and
                     *   we've now found it.  Write the command results
                     *   prefix separator. 
                     */
                    ret += gLibMessages.commandResultsPrefix;

                    /* we're now inside some command result text */
                    state_ = stateInCommand;
                    break;

                case stateBeforeInterruption:
                    /*
                     *   An editing session has been interrupted, and we're
                     *   showing new output.  First, switch to normal
                     *   in-command mode - do this before doing anything
                     *   else, since we might recursively show some more
                     *   text in the course of canceling the input line.  
                     */
                    state_ = stateInCommand;

                    /*
                     *   Now tell the input manager that we're canceling
                     *   the input line that was under construction.  Don't
                     *   reset the input editor state, though, since we
                     *   might be able to resume editing the same line
                     *   later.  
                     */
                    inputManager.cancelInputInProgress(nil);

                    /* insert the command interruption prefix */
                    ret += gLibMessages.commandInterruptionPrefix;
                    break;

                case stateBetweenCommands:
                    /* 
                     *   We've been waiting for a new command to start
                     *   after seeing a <.commandsep> tag.  We now have
                     *   some text for the new command, so show a command
                     *   separator. 
                     */
                    ret += gLibMessages.commandResultsSeparator;

                    /* we're now inside some command result text */
                    state_ = stateInCommand;
                    break;
                }

                /* add the plain text */
                ret += cur;
            }

            /* if we found the tag, process it */
            switch(tag)
            {
            case 'none':
                /* switching to no-command mode */
                state_ = stateNoCommand;
                break;

            case 'mid':
                /* switching back to mid-command mode */
                state_ = stateInCommand;
                break;
                
            case 'sep':
                /* command separation - check our state */
                switch(state_)
                {
                case stateReadingCommand:
                case stateBeforeCommand:
                case stateBetweenCommands:
                case stateWriteThrough:
                    /* in these states, <.commandsep> has no effect */
                    break;

                case stateInCommand:
                    /* 
                     *   We're inside some command text.  <.commandsep>
                     *   tells us that we've reached the end of one
                     *   command's output, so any subsequent output text
                     *   belongs to a new command and thus must be visually
                     *   separated from the preceding text.  Don't add any
                     *   separation text yet, because we don't know for
                     *   sure that there will ever be any more output text;
                     *   instead, switch our state to between-commands, so
                     *   that any subsequent text will trigger addition of
                     *   a separator.  
                     */
                    state_ = stateBetweenCommands;
                    break;
                }
                break;

            case 'int':
                /* 
                 *   we've just interrupted reading a command line, due to
                 *   an expired timeout event - switch to the
                 *   before-interruption state 
                 */
                state_ = stateBeforeInterruption;
                break;

            case 'before':
                /* we're about to start reading a command */
                switch (state_)
                {
                case stateBeforeCommand:
                    /* 
                     *   we've shown nothing since the last command; show
                     *   the empty command separator 
                     */
                    writeThrough(gLibMessages.commandResultsEmpty());
                    break;

                case stateBetweenCommands:
                case stateInCommand:
                    /* 
                     *   we've written at least one command result, so
                     *   show the after-command separator 
                     */
                    writeThrough(gLibMessages.commandResultsSuffix());
                    break;

                default:
                    /* do nothing in other modes */
                    break;
                }

                /* switch to reading-command mode */
                state_ = stateReadingCommand;
                break;

            case 'after':
                /* 
                 *   We've just finished reading a command.  If we're
                 *   still in reading-command mode, switch to
                 *   before-command-results mode.  Don't switch if we're
                 *   in another state, since we must have switched to
                 *   another state already by a different route, in which
                 *   case we can ignore this notification.  
                 */
                if (state_ == stateReadingCommand)
                    state_ = stateBeforeCommand;
                break;
            }
        }

        /* return the results */
        return ret;
    }

    /* our current state - start out in before-command mode */
    state_ = stateBeforeCommand
;

/* ------------------------------------------------------------------------ */
/*
 *   Log Console output stream.  This is a simple wrapper for the system
 *   log console, which allows console-style output to be captured to a
 *   file, with full processing (HTML expansion, word wrapping, etc) but
 *   without displaying anything to the game window.
 *   
 *   This class should always be instantiated with transient instances,
 *   since the underlying system object doesn't participate in save/restore
 *   operations.  
 */
class LogConsole: OutputStream
    /*
     *   Utility method: create a log file, set up to capture all console
     *   output to the log file, run the given callback function, and then
     *   close the log file and restore the console output.  This can be
     *   used as a simple means of creating a file that captures the output
     *   of a command.  
     */
    captureToFile(filename, charset, width, func)
    {
        local con;
            
        /* set up a log console to do the capturing */
        con = new LogConsole(filename, charset, width);

        /* capture to the console and run our command */
        outputManager.withOutputStream(con, func);

        /* done with the console */
        con.closeConsole();
    }

    /* create a log console */
    construct(filename, charset, width)
    {
        /* inherit base class handling */
        inherited();
        
        /* create the system log console object */
        handle_ = logConsoleCreate(filename, charset, width);

        /* install the standard output filters */
        addOutputFilter(typographicalOutputFilter);
        addOutputFilter(new transient ParagraphManager());
        addOutputFilter(styleTagFilter);
        addOutputFilter(langMessageBuilder);
    }

    /* 
     *   Close the console.  This closes the underlying system log console,
     *   which closes the operating system file.  No further text can be
     *   written to the console after it's closed.  
     */
    closeConsole()
    {
        /* close our underlying system console */
        logConsoleClose(handle_);

        /* 
         *   forget our handle, since it's no longer valid; setting the
         *   handle to nil will make it more obvious what's going on if
         *   someone tries to write more text after we've been closed 
         */
        handle_ = nil;
    }

    /* low-level stream writer - write to our system log console */
    writeFromStream(txt) { logConsoleSay(handle_, txt); }

    /* our system log console handle */
    handle_ = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   Output stream window.
 *   
 *   This is an abstract base class for UI widgets that have output
 *   streams, such as Banner Windows and Web UI windows.  This base class
 *   essentially handles the interior of the window, and leaves the details
 *   of the window's layout in the broader UI to subclasses.  
 */
class OutputStreamWindow: object
    /* 
     *   Invoke the given callback function, setting the default output
     *   stream to the window's output stream for the duration of the call.
     *   This allows invoking any code that writes to the current default
     *   output stream and displaying the result in the window.  
     */
    captureOutput(func)
    {
        /* make my output stream the global default */
        local oldStr = outputManager.setOutputStream(outputStream_);

        /* make sure we restore the default output stream on the way out */
        try
        {
            /* invoke the callback function */
            (func)();
        }
        finally
        {
            /* restore the original default output stream */
            outputManager.setOutputStream(oldStr);
        }
    }

    /* 
     *   Make my output stream the default in the output manager.  Returns
     *   the previous default output stream; the caller can note the return
     *   value and use it later to restore the original output stream via a
     *   call to outputManager.setOutputStream(), if desired.  
     */
    setOutputStream()
    {
        /* set my stream as the default */
        return outputManager.setOutputStream(outputStream_);
    }

    /*
     *   Create our output stream.  We'll create the appropriate output
     *   stream subclass and set it up with our default output filters.
     *   Subclasses can override this as needed to customize the filters.  
     */
    createOutputStream()
    {
        /* create a banner output stream */
        outputStream_ = createOutputStreamObj();

        /* set up the default filters */
        outputStream_.addOutputFilter(typographicalOutputFilter);
        outputStream_.addOutputFilter(new transient ParagraphManager());
        outputStream_.addOutputFilter(styleTagFilter);
        outputStream_.addOutputFilter(langMessageBuilder);
    }

    /*
     *   Create the output stream object.  Subclasses can override this to
     *   create the appropriate stream subclass.  Note that the stream
     *   should always be created as a transient object.  
     */
    createOutputStreamObj() { return new transient OutputStream(); }

    /*
     *   My output stream - this is a transient OutputStream instance.
     *   Subclasses must create this explicitly by calling
     *   createOutputStream() when the underlying UI window is first
     *   created.  
     */
    outputStream_ = nil
;

