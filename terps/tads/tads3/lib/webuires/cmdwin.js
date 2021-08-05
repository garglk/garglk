/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
 *   Command input window scripts for TADS 3 Web UI.  These scripts are for
 *   the command-line window.  This window type simulates the traditional
 *   console model, with a mix of text output and command-line input.  This
 *   is a "widget" that can be plugged into an iframe in a layout window, so
 *   a single browser window can contain multiple command windows.  For
 *   example, you could use one command window for a game transcript, and
 *   another for player-to-player chat.  
 */

// command window initialization
function cmdWinInit()
{
    // set up our window and document event handlers
    addEventHandler(window, "scroll", onWinScroll);
    addEventHandler(window, "resize", onWinResize);
    addEventHandler(document, "mouseup", onDocMouseUp);
    addEventHandler(document, "mousedown", onDocMouseDown);
    addKeyHandler(document, onDocKey);

    // initialize the utility package
    utilInit();

    // get the initial state
    getInitState();

    // load preference styles
    $win().prefsToCSS(null, window);

    // Set up a workaround for an annoying IE bug.  Here's the situation:
    // when an <A> is dragged and dropped, it gets into this weird half-focus
    // state where IE thinks it has focus, but Windows *doesn't*.  Windows
    // leaves focus wherever it was before the drag.  When a keyboard event
    // comes in, IE does the JS event bubble according to its internal notion
    // that the <A> has focus, but Windows actually sends the keystroke to
    // the old focus control.  So what you get is a keystroke going to a
    // control without any JS events being generated.  This is a problem
    // when the keystroke is Enter and the control is our command line field:
    // IE simply inserts the newline into the field without telling us about
    // it.  I can't find any good workaround other than a timer, to check
    // after the fact for this weird situation.  Checking a few times a
    // second shouldn't bog things down too much but should correct the
    // problem before it's noticeable in the UI.
    setInterval(function() {
        var fld = $("cmdline");
        if (fld && fld.nodeName == "SPAN")
        {
            var txt = fld.innerHTML;
            if (txt && txt.search(/<BR>|[\r\n]/) >= 0)
            {
                setCommandText(fld, getCommandText(fld).replace(/[\r\n]/g, ""));
                handleEnterKey(fld);
            }
        }
    }, 250);
}

/* ------------------------------------------------------------------------ */
/*
 *   Process a game event from the server
 */
function onGameEvent(req, resp)
{
    // check what we have
    if (xmlHasChild(resp, "say"))
    {
        window.closeCommandLine(null, null, false);
        window.writeText(xmlChildText(resp, "say"));
    }
    if (xmlHasChild(resp, "closeInputLine"))
    {
        var c = xmlChild(resp, "closeInputLine")[0];
        var txt = xmlChildText(c, "text");
        var user = xmlChildText(c, "user");
        window.closeCommandLine(txt, user, false, null);
    }
    if (xmlHasChild(resp, "cancelInputLine"))
    {
        window.cancelCommandLine(
            xpath(resp, "cancelInputLine.reset") == "yes");
    }
    if (xmlHasChild(resp, "inputLine"))
    {
        window.openCommandLine();
    }
    if (xmlHasChild(resp, "scrollToBottom"))
    {
        scrollToAnchor();
    }
    if (xmlHasChild(resp, "clearWindow"))
    {
        window.clearWindow();
    }
    if (xmlHasChild(resp, "morePrompt"))
    {
        window.showMorePrompt();
    }
    if (xmlHasChild(resp, "setsize"))
    {
        // update our window layout
        parent.setWinLayout(this, xmlChildText(resp, "setsize"));
    }
    if (xmlHasChild(resp, "resize"))
    {
        // recalculate our window layout
        parent.calcLayout();
    }
}

function onGameState(req, resp)
{
    // read the scrollback items and write them as text
    var hist = [];
    xpath(resp, "scrollback/sbitem[*]").forEach(function(s)
    {
        // write this output text passage
        window.writeText(xpath(s, "text#text"));

        // if there's an input line for this block, add it
        var i = xpath(s, "input");
        if (i)
        {
            // get the appropriate style for the old command line text
            var style = xmlHasChild(i, "interrupted")
                        ? "cmdlineInterrupted" : "cmdlineOld";

            // add the command line text
            var t = xmlNodeText(i);
            window.writeText("<span class=\"" + style + "\">"
                             + t.htmlify() + "</span><br>");

            // save it in our new history list
            hist.push(t);
        }
    });

    // if there's no global history list (probably because we're refreshing
    // the page), use the new history list that we built from the scrollback
    // information as the active history list
    if (cmdLineHistory.length == 0)
    {
        // trim it to the maximum retention count
        while (hist.length > cmdLineHistoryLimit)
            hist.shift();

        // install it as the active command history
        cmdLineHistory = hist;

        // select the last item
        cmdLineHistoryIdx = hist.length;
    }

    switch (xmlChildText(resp, "mode"))
    {
    case "working":
        break;

    case "inputLine":
        window.openCommandLine();
        break;

    case "moreMode":
        window.showMorePrompt();
        break;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Document event handlers 
 */

function onDocKey(desc)
{
    // if there's an explicit More prompt showing, release it
    if (morePromptDiv && desc && !desc.modifier)
    {
        // set the new scroll anchor
        scrollAnchor = getObjectRect(morePromptDiv).y - 5;

        // remove the division
        $("maindiv").removeChild(morePromptDiv);
        morePromptDiv = null;

        // let the server know that the More prompt is done
        serverRequest("/webui/morePromptDone?window=" + pageName,
                      ServerRequest.Command);

        // we're done with the key - don't process it further
        return false;
    }

    // do the generic focus processing
    genericDocKeyFocus(desc);
}

function onDocMouseDown(ev)
{
    // apply the default processing
    return true;
}

function onDocMouseUp(ev)
{
    // apply the default processing
    return true;
}

var docFocusThread = null;
function onFieldFocus()
{
    if (docFocusThread)
    {
        clearTimeout(docFocusThread);
        docFocusThread = null;
    }
}

/*
 *   Process a selection change in the field.  We call this from the
 *   'onselect' event in the field, which fires whenever the selection
 *   changes to a new non-empty selection; and also on mouseup and mousedown
 *   messages (since these can move the cursor, but don't generate 'onselect'
 *   events if the selection range is empty); and also on key events (since
 *   these can also change the selection, and again don't generate 'onselect'
 *   events for empty selections).  
 */
var lastFldSel = null;
function onFieldSelect()
{
    // remember the new selection range in the field, if we have one
    var sel = getSelRangeInEle("cmdline");
    if (sel)
        lastFldSel = sel;
}

/*
 *   Process a mouse event (mousedown or mouseup) in the field.  Mouse clicks
 *   can cause selection changes that don't generate 'onselect' events, so
 *   note the selection after processing each click.  
 */
function onFieldMouse()
{
    // note the new selection after the click has been fully processed
    setTimeout(onFieldSelect, 0);
}

/* 
 *   Set the default focus for a keyboard event.  This moves focus to the
 *   command line if there's an active command line, and focus isn't already
 *   in the command line or some other focus-taking object.  
 */
function setDefaultFocusChild(desc, fromWin)
{
    // if the command line doesn't exist, do nothing
    var fld = $("cmdline");
    if (!fld)
        return;

    // if the command line already has focus, do nothing
    var ae = document.activeElement;
    if (ae == fld && document.hasFocus && document.hasFocus())
        return;

    // if some other input control has focus, do nothing
    if (ae && ae != fld && eleKeepsFocus(ae, desc))
        return;

    // remember current selection, since focusing will set a new default
    // selection in the field
    var sel = lastFldSel;

    // note the current text in the field
    var oldTxt = getCommandText(fld);

    // set focus after finishing with this key event
    setTimeout(function()
    {
        // move focus to the field
        try
        {
            fld.focus();
        }
        catch (e)
        {
            // couldn't set focus - the field must have been deleted
            return;
        }
        
        // Note if the field's contents have changed since we read them.
        // IE has some buggy behavior in certain situations where the
        // field *kind of* loses focus, but not quite: for JS purposes,
        // IE reports that the field doesn't have focus, but internally
        // the underlying control actually does have focus.  The result
        // is that the field turns out to process the key even though
        // IE assures us that it won't.  We can detect many of these
        // cases by checking the before and after field contents; if
        // the field changed between the key event and the timeout,
        // the event must have made some change to the field after
        // all, so we don't want to insert buffered characters
        // after all.
        var newTxt = getCommandText(fld);
        var changed = newTxt != oldTxt;

        // re-select the original range as of when we last lost focus
        if (sel && !changed)
            setSelRangeInEle(fld, sel);
        
        // make sure the field is fully in view
        var rcFld = getObjectRect(fld);
        var rcDiv = getObjectRect(fld.parentNode);
        var s = rcDiv.y + rcDiv.height - getWindowRect().height
                + rcFld.height/2;
        var scur = getScrollPos();
        if (s > scur.y)
            window.scrollTo(scur.x, s);

        // process buffered keys
        readKeyBuf(fld, changed);

    }, 0);

    // If it's an Up or Down key, cancel the default behavior, so that
    // the browser doesn't scroll the window.  These keys step through
    // the command history when a command line is active, so we don't
    // want to also scroll the window.
    if (desc && (desc.keyName == "Up" || desc.keyName == "Down"))
        preventDefault(desc.event);
}

// Process keys in the top window's keyboard buffer.  The buffer captures
// keystrokes when there's no focus, so that we can deliver them to the
// field when it initially gains focus or regains focus.
function readKeyBuf(fld, changed)
{
    // get the key buffer
    var buf = $win().keybuf;

    // read each key
    for (var done = false ; buf.length && !done ; )
    {
        // get the next key - take the oldest key first so that we process
        // keys in the same order as they were typed
        var desc = buf.shift();

        // if the field changed since we buffered the key, ignore everything
        // except certain command keys
        if (changed && desc.keyName != "Enter")
            continue;

        // note the current selection in the field
        var r = getSelRangeInEle(fld) || { start: 0, end: 0 };

        // if it's a character key, replace the selection with the
        // character; otherwise process the special key
        if (desc.ch && desc.ch.charCodeAt(0) > 27)
        {
            // pretend we typed the character
            replaceSelRange(fld, desc.ch, false);
        }
        else switch (desc.keyName)
        {
        case "U+0008":
            // Backspace - delete the selection, or the character to the left.
            // If nothing's selected, select the character to the left.
            if (r.start == r.end && r.start > 0)
                setSelRangeInEle(fld, {start: r.start - 1, end: r.start});

            // delete the selection
            replaceSelRange(fld, "", false);
            break;
            
        case "U+007F":
            // Delete - delete the selection, or the character to the right.
            // If nothing's selected, select the character to the right.
            if (r.start == r.end)
                setSelRangeInEle(fld, {start: r.start, end: r.start + 1});

            // delete the selection
            replaceSelRange(fld, "", false);
            break;

        case "Enter":
            // process the enter key
            handleEnterKey(fld);

            // stop processing buffered keys, since we're no longer active
            done = true;
            break;

        case "Left":
            // move to the start of the selection, or left one character
            if (r.start == 0)
                r = {start: 0, end: 0};
            else if (r.start == r.end)
                r = {start: r.start - 1, end: r.start - 1};
            else
                r = {start: r.start, end: r.start};
            setSelRangeInEle(fld, r);
            break;

        case "Right":
            // move to the end of the selection, or right one character
            if (r.start == r.end)
                r = {start: r.start + 1, end: r.start + 1};
            else
                r = {start: r.end, end: r.end};
            setSelRangeInEle(fld, r);
            break;

        default:
            // try running others through the normal key handler
            cmdlineKey(desc, fld);
            break;
        }
    }
}

/*
 *   IE-specific Paste filtering for the command line.  We make sure that the
 *   pasted data doesn't contain any newlines or formatted text.  
 */
function onFieldPaste()
{
    // get the clipboard contents
    var txt = window.clipboardData.getData("Text");
    if (txt != null)
    {
        // remove any newlines (replace each run of newlines with one space)
        txt = txt.replace(/[\r\n]+/g, " ");

        // Manually do the past into the field's current selection.  Do
        // this rather than allowing the native paste to proceed, because
        // the native paste will insert formatted HTML text.  There's not
        // a way to tell IE to paste the plain text version that it shows
        // us; it always just pastes the HTML.  So we have to simulate the
        // effect by doing the replacement manually.
        replaceSelRange("cmdline", txt, true);

        // stop the rest of the paste
        return false;
    }
    else
    {
        // no text available - do the native paste operation instead
        return true;
    }
}

function onWinScroll(ev)
{
    // the user has presumably scrolled the window manually, so set
    // the new anchor position to the current scroll position (assuming
    // it's below the old scroll anchor)
    var y = getScrollPos().y;
    if (y > scrollAnchor)
        scrollAnchor = y;

    // if there's a pending command line, try opening it
    if (cmdLinePending)
        maybeOpenCommandLine();
        
    // continue to the default browser processing
    return true;
}

function onWinResize(event)
{
    // if there's a pending command line, try opening it - the resize might
    // have brought the new command line real estate into view
    maybeOpenCommandLine();

    // adjust the command line width
    adjustCmdlineWidth();
    
    // apply the default processing
    return true;
}

/* ------------------------------------------------------------------------ */
/*
 *   Clear the window 
 */
function clearWindow()
{
    // end the current output division
    outputDiv = null;

    // If the active element is the body, explicitly move focus there if
    // possible.  This avoids a weird glitch on IE.  I think what's going
    // on is that IE has some residual memory of the focus being in the
    // division that contained the most recent command line, and removing
    // that division below causes IE to send the focus off into some
    // off-screen limbo to follow the removed element.  Once focus goes
    // to limbo, we can't seem to get it back programmatically; the user
    // has to manually click on the window, which is irritating for the
    // user.  Explicitly establishing focus in the body element before we
    // remove anything seems to keep focus in the window after the clear.
    var ae = document.activeElement;
    if (ae == document.body && ae.focus)
        ae.focus();

    // remove all of the "outputdiv" children of the "maindiv" division
    var main = $("maindiv");
    for (var chi = main.firstChild, nxt = null ; chi ; chi = nxt)
    {
        // note the next child, in case we delete this one
        nxt = chi.nextSibling;

        // if this is an "outputdiv" division, delete it
        if (chi.className == "outputdiv")
            main.removeChild(chi);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Window text writer
 */

// current output division, and current source text for the division
var outputDiv = null;
var outputDivTxt = null;

/*
 *   Write text to the window.  This adds the text at the end of the current
 *   output division in the document.  
 */
function writeText(txt)
{
    // open an output division if necessary
    openOutputDiv();

    // add the text to the running source for the division
    outputDivTxt += BrowserInfo.adjustText(txt);

    // set the text in the division
    outputDiv.innerHTML = outputDivTxt;
}

/*
 *   Open an output division
 */
function openOutputDiv()
{
    // if there's no output division open, create one
    if (!outputDiv)
    {
        // create a new division
        outputDiv = document.createElement("div");
        outputDiv.className = "outputdiv";

        // add it at the end of the main division
        var md = $("maindiv");
        md.appendChild(outputDiv);

        // start a new running division text section
        outputDivTxt = '';
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   "More" prompt 
 */
function showMorePrompt()
{
    // if there's already a More prompt division, do nothing
    if (morePromptDiv)
        return;
    
    // create the "More" prompt division
    var mp = morePromptDiv = document.createElement("div");
    mp.className = "morePrompt";

    // add it at the end of the main division
    var main = $("maindiv");
    main.appendChild(mp);

    // fill it in
    mp.innerHTML = "Press a key to continue...";

    // scroll it into view, if possible
    scrollToAnchor();
}


/* ------------------------------------------------------------------------ */
/*
 *   Command line processing 
 */

/* command-line globals */
var scrollAnchor = 0;
var cmdLinePending = null;
var cmdLineHistory = [];
var cmdLineHistoryIdx = 0;
var cmdLineHistoryLimit = 20;
var cmdLineNum = 1;

/*
 *   Get the contents of an open text field
 */
function getCommandText(fld)
{
    // we use a <SPAN contentEditable=true> on some browsers, and an
    // <INPUT TYPE=TEXT> field on others
    var txt = fld.nodeName == "SPAN" ? fld.innerText : fld.value;

    // if it's null, make it an empty string
    if (txt == null)
        txt = "";

    // return the text
    return txt;
}

/*
 *   Set the contents of an open text field
 */
function setCommandText(fld, txt)
{
    // before setting the text, make sure the field is big enough for it
    var probe = $("cmdline_probe.<SPAN>");
    probe.innerHTML = (txt.htmlify() + 'MMMM').replace(/ /g, '&nbsp;');
    if (probe.offsetWidth > fld.offsetWidth)
        fld.style.width = probe.offsetWidth + "px";

    // set the contents, according to the input element type
    if (fld.nodeName == "SPAN")
        fld.innerText = txt;
    else
        fld.value = txt;

    // move the cursor to the end of the new text
    setSelRangeInEle(fld, {start: txt.length, end: txt.length});
}

/*
 *   Open a command line.  This closes the current output division and adds a
 *   new input editor after it.  
 */
function openCommandLine()
{
    // add an empty span as a wrapper for the command line
    var id = "cmdline_" + cmdLineNum;
    writeText("&#65279;<span id=\"" + id + "\" class=\"cmdline\"></span>");
    cmdLinePending = $(id);

    // Try opening the new command line immediately, if possible.  (Whether
    // we can depends on the scrolling situation.)
    maybeOpenCommandLine();

    // scroll down as far as possible
    scrollToAnchor();
}

/*
 *   Scroll down as far as possible to bring new text into view.  This
 *   scrolls down to the bottom of the document, or to the point where the
 *   "scroll anchor" just goes out of view, whichever distance is shorter.
 *   The scroll anchor is the last vertical position that we can safely
 *   assume the user has seen: it's the bottom of the document at the time of
 *   the last pause for user interaction, such as a command line input or key
 *   input wait.  
 */
var inMoreMode = false, morePromptDiv = null;
function scrollToAnchor()
{
    // get the main division, and figure its bottom position in doc coords
    var main = $("maindiv");
    var rcMain = getObjectRect(main);

    // get the window size and current scroll position
    var rcWin = getWindowRect();
    var curScroll = getScrollPos();

    // the most we'd want to scroll would be far enough to bring the bottom
    // of the main division into view, so start with that
    var s = rcMain.y + rcMain.height - rcWin.height;

    // if that's beyond the anchor position, only scroll as far as the anchor
    if (s > scrollAnchor)
        s = scrollAnchor;

    // if we're not already scrolled past the target, scroll there now
    if (s > curScroll.y)
        window.scrollTo(curScroll.x, s);
}

/*
 *   Try opening the command line for editing.  This creates the actual
 *   <input> text field that we use to present the input editor in the UI.  
 */
function maybeOpenCommandLine()
{
    // IE seems to trigger some events recursively at surprising times.
    // In particular, IE seems to do queued processing on virtually any
    // system call, even seemingly non-blocking things like object size
    // retrievals, and this can trigger event calls.  Since this routine
    // is called from a number of events, that means that we can be
    // invoked recursively seemingly at random.  To avoid reentrancy
    // problems, privatize our globals while we're working.
    var cl = cmdLinePending;
    cmdLinePending = null;

    // if there's no command line pending, there's nothing to do
    if (!cl)
        return;

    // Check to see if the new command line is in view.  We've set up
    // the wrapper division for the new command line at the bottom of
    // the stream division, so we simply have to see if the bottom of
    // division is in view.
    var rcMain = getObjectRect($("maindiv"));

    // figure the distance from the scroll anchor to the bottom of the div
    var dist = rcMain.y + rcMain.height - scrollAnchor;

    // if that doesn't fit in the window, don't open the command line yet
    var rcWin = getWindowRect();
    if (dist > rcWin.height)
    {
        // set a More prompt in the status line (this doesn't work on most
        // newer browsers, and there's no workaround, since most browsers
        // now prohibit javascript from tampering with the status line for
        // security reasons; but just in case)
        window.status = "*** More ***";

        // note that More mode was activated
        inMoreMode = true;

        // restore the pending command line and return
        cmdLinePending = cl;
        return;
    }

    // if we activated More mode in the past, turn it off
    if (inMoreMode)
    {
        // remove any More prompt
        window.status = "";

        // Consider any keys that were typed up to this point to have been
        // consumed by the More mode, so clear the keystroke buffer.
        $win().keybuf = [];

        // no longer in More mode
        inMoreMode = false;
    }

    // hide the bottom spacer while the command line is open, so that
    // the command line is at the very bottom of the window
    // $("bottomSpacerDiv").style.display = "none";

    // figure the font - use the computed font of the enclosing span
    var font = "font: " + getStyle(cl, "font").replace(/'/g, '') + ";";

    // set up the event handlers for the input field
    var events =
        "onkeypress=\"javascript:return $kp(event, cmdlineKey, this);\" "
        + "onkeydown=\"javascript:return $kd(event, cmdlineKey, this);\" "
        + "onmousedown=\"javascript:onFieldMouse();return true;\" "
        + "onmouseup=\"javascript:adjustCmdlineWidth();onFieldMouse();"
        +     "return true;\" "
        + "ondrop=\"javascript:adjustCmdlineWidth();return true;\" "
        + "onpaste=\"javascript:adjustCmdlineWidth();return true;\" "
        + "onfocus=\"javascript:onFieldFocus();\" ";

    // fill in the command line wrapper with the input field
    if (BrowserInfo.ie)
    {
        // IE seems impossibly fiddly with text field alignment; as far as
        // I can tell it's simply not possible to get a field to line up
        // seamlessly with the surrounding text for arbitrary font size.
        // We could always make manual adjustments, but I can't even figure
        // out a way to predict what the adjustment needs to be - it seems
        // to vary across different fonts and sizes.  Fortunately, there's
        // the contentEditable property, which lets us make an ordinary SPAN
        // into an editable field.  This actually works on most of the other
        // browsers as well, but it has some other quirks of its own, so
        // we'll only use it where we absolutely have to (so far just IE).
        
        // Add the IE-specific "before deactivate" event, to capture the
        // selection range before we lose focus.  Also intercept Paste
        // events, so that we can limit pasting to single-line unformatted
        // text.
        events += "onbeforedeactivate=\""
                  + "javascript:onFieldSelect();return true;\" "
                  + "onpaste=\"javascript:return onFieldPaste();\" ";

        // create the content-editable span
        cl.innerHTML =
            "<span contentEditable=true class='cmdline' id='cmdline' "
            + events
            + "style='" + font + "' "
            + "></span>";
    }
    else
    {
        // some browsers need some slight tweaks to the positioning
        // to get everything aligned properly
        var hacks = "";
        if (BrowserInfo.safari || BrowserInfo.opera)
            hacks = "margin: -1px 0 0 -1px;";
        else if (BrowserInfo.firefox && BrowserInfo.firefox >= 3.05)
            hacks = "position: relative; left: -1px;";

        // We're using an <input type=text>, so monitor selection changes
        // directly on the input field.  Also, adjust the field width as
        // we edit it to keep it large enough for the text.
        events += "onselect=\"javascript:onFieldSelect();\" "
                  + "oninput=\"javascript:adjustCmdlineWidth();"
                  + "return true;\" ";

        // all of the other browsers seem to get the alignment fine with
        // a regular text field, so use a regular text field
        cl.innerHTML =
            "<input type='text' class='cmdline' id='cmdline' length=1024 "
            + "style='" + hacks + font + "' "
            + events
            + ">";
    }

    // set focus on the new input field - do this in a timeout to give
    // the browser a chance to process events related to creating the
    // new objects, to make sure the object exists in the UI before we
    // try to give it the focus
    docFocusThread = setTimeout(function()
    {
        // if we have an interrupted command line (such as from a timeout
        // interruption), restore the saved state
        if (interruptedCommandLine)
        {
            // get the field
            var fld = $("cmdline");
            
            // restore the editing state
            setCommandText(fld, interruptedCommandLine.text);
            setSelRangeInEle(fld, interruptedCommandLine.sel);
            
            // done with the interrupted editing state
            interruptedCommandLine = null;
        }

        // set the default focus
        $win().setDefaultFocus(null, null);

        // We only open the command line when it's in view, so it must
        // be in view.  Reset the scroll anchor to the new command line
        // on the assumption that the user has had a chance to view
        // everything up to this point.
        scrollAnchor = getObjectRect("cmdline").y - 5;
    }, 0);

    // set the initial field width
    adjustCmdlineWidth();

    // add a history item for the active command line
    cmdLineHistory.push("");
}

/*
 *   Explicitly cancel the command line.  This is used to interrupt a command
 *   line in progress, usually due to a timeout.  If 'reset' is true, clear
 *   the input line state, so that the next new command line starts out
 *   empty.  Otherwise, save the state from the current command line editing
 *   session, so that we can restore it on the next open.  
 */
var interruptedCommandLine = null;
function cancelCommandLine(reset)
{
    // get the active command field; if there isn't an open command line,
    // there's nothing we need to do
    var fld = $("cmdline");
    if (!fld)
        return;

    // close the command line, using the interrupted style
    closeCommandLine(null, null, false, "cmdlineInterrupted");

    // remove the placeholder top history item
    cmdLineHistory.pop();
}

/*
 *   Close the command line.  If 'style' is specified, it's the CSS class
 *   name to use to show the static text of the now-closed command line.  
 */
function closeCommandLine(txt, user, reset, style)
{
    // if the style is null, use the default
    if (!style)
        style = "cmdlineOld";

    // get the text input field
    var fld = $("cmdline");
    var div = null;
    var height = "";
    if (fld)
    {
        // if we're not resetting, save away the current editing info
        var s;
        if (reset)
            s = null;
        else {
            s = {
                text: getCommandText(fld),
                sel: getSelRangeInEle(fld),
                lastSel: lastFldSel
            };
        }

        // store the saved command line info
        interruptedCommandLine = s;

        // get its contents, if the caller didn't specify overriding text
        if (txt == null)
            txt = getCommandText(fld);

        // Note the actual height of the input field.  This allows us to
        // set a matching height for the static text span that will replace
        // the field now that we're closing the editor.  An input field
        // doesn't usually have the exactly same native height as a regular
        // text span with the same contents, so this explicit setting is
        // needed to keep the line spacing from visibly changing when we
        // change the command line from an input field to a <SPAN>.
        height = "height:" + getObjectRect(fld).height + "px";

        // remove focus from the field
        fld.blur();

        // forget the last selection
        lastFldSel = null;

        // the command line division is the field's parent
        div = fld.parentNode;
    }
    else if (cmdLinePending)
    {
        // We have a pending command line that we never activated.  This
        // happens when there's unviewed text past the bottom of the
        // window; we wait to activate the command line until the user
        // scrolls its location into view.  We do have a placeholder
        // division in this case, though, so we can just plug in the
        // entered command there - that's where it would ultimately have
        // gone anyway, it's just that we skip the entire creation and
        // deletion of the field within the division.
        div = cmdLinePending;

        // there's no longer a pending command line
        cmdLinePending = null;
    }
    else
    {
        // no command line is open or pending - there's nothing to close
        return;
    }

    // IE doesn't respect the "white-space: pre" style on inline elements,
    // so to work around this we change spaces into &nbsp's.
    var spanTxt = txt.htmlify();
    if (BrowserInfo.ie)
        spanTxt = spanTxt.replace(/ /g, '&nbsp;');

    // if a user name was provided add it
    var userPrefix = "";
    if (user)
        userPrefix = "<span class=\"cmdFromUser\">[" + user + "]</span> ";

    // replace the field with the text that was entered
    var lastCmdLineID = "closedCmdLine" + (cmdLineNum++);
    div.innerHTML =
        "<span class=\"" + style + "\" id=\"" + lastCmdLineID
        + "\" style=\"" + height + "\">"
        + userPrefix
        + spanTxt + "</span>";

    // remember the last command line object
    var lastCmdLine = $(lastCmdLineID);

    // start a new output division
    outputDiv = null;

    // return the command line text
    return txt;
}

/*
 *   Set the command line font
 */
function setCommandLineFont()
{
    // get the active input field
    var fld = $("cmdline");

    // if we have a field, set its font
    if (fld)
        fld.style.font = getStyle(fld.parentNode, "font");
}

/*
 *   Adjust the command-line width.  We call this automatically as the user
 *   types, to keep the field as least as wide as the text it contains.  This
 *   ensures that we don't scroll the contents of the input field, but rather
 *   scroll the whole window.  Scrolling the input field's contents is
 *   confusing because we try to make the field fade into the background as
 *   much as possible: we don't show a border or separate background color.
 *   We want it to look like an input line in a regular terminal window,
 *   which doesn't look any different from the surrounding output text.  
 */
function adjustCmdlineWidth()
{
    // give the browser a few moments to digest events, then adjust the width
    // of the field so that it's large enough for the field's contents
    setTimeout(function() {

        // get the field
        var fld = $("cmdline");
        if (!fld)
            return;

        // get the "probe" element, for measuring the font size
        var probe = $("cmdline_probe.<SPAN>");

        // set the probe's font and contents to match the real input's,
        // with a little extra space to spare for the cursor at the end
        var txt = getCommandText(fld);
        probe.innerHTML = (txt.htmlify() + 'MMMM').replace(/ /g, '&nbsp;');

        // set the new width
        fld.style.width = probe.offsetWidth + "px";
        
    }, 10);
}

// Common keypress/keydown handler.
function cmdlineKey(desc, fld)
{
    // Set the scroll anchor to the command line field.  Since the user
    // is typing into the field, it's safe to assume they've had a chance
    // to read everything above this point in the document.  (Take off
    // a few pixels for a little margin of visual context.)
    scrollAnchor = getObjectRect(fld).y - 5;

    // Check for a selection change after processing the key through
    // the default handling.
    setTimeout(onFieldSelect, 0);

    // check for a change in the field width
    adjustCmdlineWidth();

    // see what we have
    switch (desc.keyName)
    {
    case "Up": // up arrow
        // recall the previous history item
        if (cmdLineHistoryIdx > 0)
        {
            // if we're at the top of the list, save the current text
            if (cmdLineHistoryIdx + 1 == cmdLineHistory.length)
                cmdLineHistory[cmdLineHistoryIdx] = getCommandText(fld);
    
            // recall the previous item
            setCommandText(fld, cmdLineHistory[--cmdLineHistoryIdx]);
        }

        // we've fully handled the event
        return false;

    case "Down": // down arrow
        // recall the next history item
        if (cmdLineHistoryIdx + 1 < cmdLineHistory.length)
            setCommandText(fld, cmdLineHistory[++cmdLineHistoryIdx]);

        // we've fully handled the event
        return false;

    case "PageUp": // Page Up
    case "PageDown": // Page Down
        // Input fields sometimes capture the page up/down keys for
        // cursor navigation.  For our single-line inputs, that's
        // not desirable; instead, treat it as a document scroll event.

        // get the direction
        var dir = (desc.keyName == "PageUp" ? -1 : 1);

        // get the window height, for the scroll distance
        var wrc = getWindowRect();
        
        // scroll by a little less than a page
        var dist = wrc.height;
        if (dist > 20)
            dist -= 20;
        
        // do the scrolling
        window.scrollBy(0, dist * dir);

        // handled
        return false;

    case "U+001B": // Escape
        // Clear the command line.  Do this after finishing with the
        // keystroke: in Firefox, at least, Escape seems to be sort of
        // a super-Undo that restores the last thing that was in the
        // field, so the browser undoes our work here if we do it in-line.
        // This seems to happen even with the preventDefault() below, so
        // I'm not sure where the value reset is coming from, but it's
        // someplace we can't seem to override.  Clearing the field in a
        // timeout lets us take care of it after the browser has finished
        // the un-overridable super-Undo.
        setTimeout(function() { setCommandText(fld, ""); }, 0);

        // prevent the default handling for Escape - this is a command key
        // in many browsers meaning "cancel page load", which can also
        // cancel in-progress ajax requests
        preventDefault(desc.event);

        // handled
        return false;

    case "U+0055": // 'U'
        // check for Ctrl+U
        if (desc.ctrlKey)
        {
            // clear the line
            setCommandText(fld, "");

            // on IE, block the Ctrl+U underline handling
            if (BrowserInfo.ie)
            {
                preventDefault(desc.event);
                return false;
            }
        }

        // it's not Ctrl+U, so we don't handle it here
        return true;

    case "U+0042": // 'B'
    case "U+0049": // 'I'
    case "U+004B": // 'K'
        // When editing a 'contentEditable' span in IE, IE enables the
        // traditional rich-text formatting keys: Ctrl+B for Bold,
        // Ctrl+I for italic, Ctrl+U for underline, Ctrl+K for Hyperlink.
        // We don't want to allow formatted text, so disable these for IE.
        if (BrowserInfo.ie
            && desc.ctrlKey
            && document.activeElement
            && document.activeElement.nodeName == "SPAN"
            && document.activeElement.isContentEditable)
        {
            // in a content-editable span - suppress formatting command keys
            preventDefault(desc.event);
            return false;
        }

        // otherwise, apply the default handling
        return true;

    case "Enter":
        handleEnterKey(fld);

        // no need for further handling
        return false;

    default:
        // we don't handle anything else, so use the default behavior
        return true;
    }
}

// handle (or simulate) an enter key
function handleEnterKey()
{
    // immediately scroll down by the height of the field, and to the left
    // edge
    var rc = getObjectRect("cmdline");
    var curScroll = getScrollPos();
    window.scrollBy(-curScroll.x, rc.height);

    // close the current command line
    var txt = closeCommandLine(null, null, true);
    
    // If they selected and entered a past history item without changing
    // anything, stay at the current position in the history.  Otherwise,
    // add the new item to the end of the history and reset the index
    // to the latest item.
    if (cmdLineHistoryIdx + 1 < cmdLineHistory.length
        && cmdLineHistory[cmdLineHistoryIdx] == txt)
    {
        // We've selected and entered a past item without making any
        // changes.  The temporary last history item we added for the
        // new command line was never committed, so delete it.
        cmdLineHistory.pop();
    }
    else if (txt == "")
    {
        // don't add empty lines to the history
        cmdLineHistory.pop();
    }
    else
    {
        // We've entered a new item.  Save it as the last history item.
        // There's already an array item for it, since we added a temporary
        // item for the new command line when we opened it - so we just
        // need to set the value in the last item.
        cmdLineHistory[cmdLineHistory.length-1] = txt;
        
        // trim the history if it's getting too long
        if (cmdLineHistory.length > cmdLineHistoryLimit)
            cmdLineHistory.shift();
    }
    
    // put the history cursor at the end of the list
    cmdLineHistoryIdx = cmdLineHistory.length;
    
    // process the command through the registered callback
    if (window.onCloseCommandLine)
        window.onCloseCommandLine(txt);
    
    // send the text to the server
    serverRequest("/webui/inputLine?txt=" + encodeURIComponent(txt)
                  + "&window=" + pageName,
                  ServerRequest.Command);
}

// handle a command hyperlink
function gamehref(ev, href, win, ele)
{
    // if we have a window path, ask our parent to find the window
    if (win)
    {
        // send it to the given window, if we can find it
        if (win = $win(win))
            return win.gamehref(ev, href, null, ele);
        else
            return false;
    }

    // there's no path, so it's directed to us - put it in our command buffer
    var fld = $("cmdline");
    if (fld)
    {
        // set the text in the field
        setCommandText(fld, href);

        // enter the command
        handleEnterKey();

        // we've handled the event, so don't bubble it up
        return false;
    }
    else
    {
        // there's no active command line, so send the click to the main
        // window, in case the server wants it as an href event
        $win().clickToServer(ev, href, ele);
    }

    // bubble the event, but prevent the default browser handling
    preventDefault(getEvent(ev));
    return true;
}

