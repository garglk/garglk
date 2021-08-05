/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
 *   Main window scripts for TADS 3 Web UI.  These scripts are for the
 *   top-level layout window, which usually contains one or more "widget"
 *   windows, such as the main game transcript window and the status line.  
 */

/* ------------------------------------------------------------------------ */
/*
 *   main window initialization 
 */
function mainInit()
{
    // the top window is always called "main"
    pageName = "main";

    // check for the Flash plug-in
    if (getFlashPlayerVersion())
    {
        // Flash is available, so load our SWF object.  We don't embed the
        // object statically, because doing so provokes some browsers to
        // display a UI prompt about installing Flash every single time the
        // page is loaded, which is annoying to users who intentionally run
        // sans Flash.  But now that we know Flash is present, we can safely
        // load our SWF, so dynamically insert the <object> tag for it.
        $("__TADS_swf_div.<DIV>").innerHTML =
            "<object id='__TADS_swf' "
            + "classid='clsid:d27cdb6e-ae6d-11cf-96b8-444553540000' "
            + "codebase='http://fpdownload.macromedia.com/pub/shockwave/cabs/flash/swflash.cab#version=9,0,0,0' "
            + "width='0' height='0' align='middle' tabindex='-1'>"
            + "<param name='movie' value='/webuires/TADS.swf'/>"
            + "<param name='quality' value='high'/>"
            + "<param name='bgcolor' value='#ffffff'/>"
            + "<param name='allowScriptAccess' value='always'/>"
            + "<param name='FlashVars' value='onload=TADS_swf.loaded.fire'/>"
            + "<embed name='__TADS_swf' src='/webuires/TADS.swf' "
            + "width='0' height='0' quality='high' bgcolor='#ffffff' "
            + "allowScriptAccess='always' align='middle' "
            + "type='application/x-shockwave-flash' "
            + "pluginspage='http://www.macromedia.com/go/getflashplayer' "
            + "FlashVars='onload=TADS_swf.loaded.fire'/>"
            + "</object>";
    }
    else
    {
        // The Flash plug-in isn't available, so we won't be able to run
        // our SWF object.  Fire our load-completion event to signal that
        // there's nothing to wait for.
        TADS_swf.loaded.fire();
    }

    // initialize the utilities and layout window packages
    utilInit();
    layoutInit();

    // initialize the XML request frame
    initXmlFrame();

    // override some handlers
    window.onresize = mainResize;
    window.onGameEvent = mainGameEvent;
    window.onGameState = mainGameState;
    window.onUload = mainUnload;

    // Cancel any pending events from past incarnations, in case we just
    // refreshed the page.  Lingering events from past loads refer to old
    // objects from the previous DOM tree, so they won't work with the new
    // incarnation of the page.
    serverRequest("/webui/flushEvents");

    // get the initial state
    getInitState();

    // cache some images that we might need if we lose our connection
    cacheImage("/webuires/dlg-corners.gif");
    cacheImage("/webuires/dlg-title-corners.gif");
    cacheImage("/webuires/modal-cover.png");
    cacheImage("/webuires/errorPopupBkg.gif");
    cacheImage("/webuires/warningPopupBkg.gif");
    cacheImage("/webuires/modal-cover.png");
}

/* ------------------------------------------------------------------------ */
/*
 *   Main game window events and state 
 */

// are we in an inputEvent() wait?
var wantInputEvent = false;

// event handler
function mainGameEvent(req, resp)
{
    // check for title changes
    if (xmlHasChild(resp, "setTitle"))
        document.title = xmlChildText(resp, "setTitle");

    // check for preference settings changes
    if (xmlHasChild(resp, "setPrefs"))
        xmlToPrefs(xmlChild(resp, "setPrefs")[0]);

    // check for preference dialog invocations
    if (xmlHasChild(resp, "showPrefsDialog"))
        showPrefsDialog();

    // check for file dialog invocations
    if (xmlHasChild(resp, "fileDialog"))
        showFileDialog(xmlChild(resp, "fileDialog")[0]);
    else if (xmlHasChild(resp, "uploadFileDialog"))
        showUploadDialog(xmlChild(resp, "uploadFileDialog")[0]);

    // check for downloadable files
    if (xmlHasChild(resp, "offerDownload"))
    {
        var file = xmlChildText(resp, "offerDownload");
        addDownloadFile(file);
        addDownloadFrame(file);
    }
    if (xmlHasChild(resp, "cancelDownload"))
    {
        var file = xmlChildText(resp, "cancelDownload");
        removeDownloadFile(file);
        removeDownloadFrame(file);
    }

    // check for input dialog invocations
    if (xmlHasChild(resp, "inputDialog"))
        showInputDialog(xmlChild(resp, "inputDialog")[0]);

    // check for event input waits and cancellations
    if (xmlHasChild(resp, "getInputEvent"))
        wantInputEvent = true;
    else if (xmlHasChild(resp, "cancelInputEvent"))
        wantInputEvent = false;

    // check for menu system events
    if (xmlHasChild(resp, "menusys"))
        showMenuSys(xmlChild(resp, "menusys")[0]);

    // "inherit" the layout window handling
    layoutGameEvent(req, resp);
}

// state handler
function mainGameState(req, resp)
{
    // check for title settings
    if (xmlHasChild(resp, "title"))
        document.title = xmlChildText(resp, "title");

    // check for preference settings
    if (xmlHasChild(resp, "prefs"))
        xmlToPrefs(xmlChild(resp, "prefs")[0]);

    // "inherit" the layout window handling
    layoutGameState(req, resp);

    // check for file dialog invocations
    if (xmlHasChild(resp, "fileDialog"))
        showFileDialog(xmlChild(resp, "fileDialog")[0]);
    else if (xmlHasChild(resp, "uploadFileDialog"))
        showUploadDialog(xmlChild(resp, "uploadFileDialog")[0]);

    // check for input dialog invocations
    if (xmlHasChild(resp, "inputDialog"))
        showInputDialog(xmlChild(resp, "inputDialog")[0]);

    // check for input event waits
    if (xmlHasChild(resp, "getInputEvent"))
        wantInputEvent = true;

    // check for downloadable files
    if (xmlHasChild(resp, "offerDownload"))
    {
        var lst = xmlChild(resp, "offerDownload");
        for (var i = 0 ; i < lst.length ; ++i)
            addDownloadFile(xmlNodeText(lst[i]));
    }

    // check for menu system dialogs
    if (xmlHasChild(resp, "menusys"))
        showMenuSys(xmlChild(resp, "menusys")[0]);

    // This is the main, top-level window, so once we get the initial
    // state set up, start the "event thread".  This is only conceptually
    // a thread, since javascript is single-threaded, but it acts roughly
    // like a thread in that the browser sends the request asynchronously
    // and "resumes" our processing when the request completes, by
    // invoking our event callback.

    // Ask for our first event.  Do this in a timeout rather than directly,
    // to allow the browser to process any outstanding events in its own
    // queue; this avoids a problem that shows up in Mobile Safari and IE9
    // that seems to be related to the timing of receiving the IFRAME
    // contents on the initial page setup.  A zero timeout is fine; we
    // don't need a delay per se, we just need to order things so that we
    // don't generate this request until the browser has finished processing
    // UI events already in its internal queue.
    setTimeout(function() {
        serverRequest(
            "/webui/getEvent", ServerRequest.Subscription, cbGetEvent);
    }, 0);
}

// getEvent request callback.  This handles a response from the server to
// a "get event" request we sent earlier.  This allows the server to send
// us events: we ask the server for an event via a request, and as soon as
// the server has an event available, it replies to the request.  
function cbGetEvent(req, resp)
{
    // get the target window
    var w = getEventWin(req, resp);

    // If we got found a window, forward the event.  If we didn't find
    // the window, or it's not loaded yet, drop the event.  This should
    // be okay, since the window will ask for a full status refresh when
    // it first loads - this will compensate for any missed events that
    // came in before it loaded.
    if (w && w.onGameEvent)
        w.onGameEvent(req, resp);

    // if we're shutting down, skip asking for another event
    if (xmlHasChild(resp, "shutdown"))
    {
        // display the disconnecting dialog
        showDialog({
            title: "TADS",
            id: "shutdown",
            contents: "The game server has terminated the connection."
        });

        // done
        return;
    }

    // go back for the next request
    serverRequest("/webui/getEvent",
                  ServerRequest.Subscription, cbGetEvent);
}

// Get the window for a state/event reply 
function getEventWin(req, resp)
{
    // if there's a window spec, get it; assume it's directed to the
    // main frame window otherwise
    if (xmlHasChild(resp, "window"))
    {
        // get the window from the path
        return windowFromAbsPath(xmlChildText(resp, "window"));
    }
    else
    {
        // no window spec, so use the main window
        return window;
    }
}

// get a window from the absolute path
function windowFromAbsPath(path)
{
    // turn the path string into a list
    if (typeof(path) == "string")
        path = path.split(".");

    // make sure it starts with our page name
    if (path.shift() != pageName)
        return null;

    // if it's just the one element, it's the main window
    if (path.length == 0)
        return window;

    // resolve the rest as a relative path
    return windowFromPath(path);
}

/* ------------------------------------------------------------------------ */
/*
 *   Key event handler for inputEvent() requests from the server.  When the
 *   server program wants to solicit an event from the client, it sends us a
 *   <getInputEvent> status message.  We set the global variable
 *   wantInputEvent to true when we receive this message; this tells us that
 *   when we get a keystroke or hyperlink click event that isn't handled in
 *   the UI itself, we should send it back to the server as an input event
 *   message.
 *   
 *   All subwindows should forward their unhandled key events here so that we
 *   can transmit the appropriate events to the server.  Only unhandled
 *   events should be sent here; if the event is handled within the client
 *   UI, it doesn't have to be sent to the server.  
 */

function keyToServer(desc)
{
    // if the server wants an input event, send it
    if (desc && wantInputEvent)
    {
        // translate the key from DOM 3 naming to TADS naming
        var key = DOM3toTADSKey[desc.keyName] || desc.ch;

        // if we don't have a TADS translation for the key, don't send an
        // event for this key after all
        if (!key)
            return;

        // translate control keys to [ctrl-x] notation
        var ch = key.charCodeAt(0);
        if (ch == 10 || ch == 13)
            ch = '\n';
        else if (ch < 32)
            key = "[ctrl-" + String.fromCharCode(ch + 64) + "]";

        // send the event
        serverRequest("/webui/inputEvent?type=key&param="
                      + encodeURIComponent(key));

        // we're no longer waiting for an input event
        wantInputEvent = false;

        // consider this key handled - don't do the default browser action
        preventDefault(desc.event);
    }
}

/* event map from DOM 3 key names to TADS key names */
var DOM3toTADSKey = {
    "U+001B":     "[esc]",
    "Up":         "[up]",
    "Down":       "[down]",
    "Left":       "[left]",
    "Right":      "[right]",
    "Home":       "[home]",
    "End":        "[end]",
    "U+007F":     "[del]",
    "Insert":     "[insert]",
    "ScrollLock": "[scroll]",
    "PageUp":     "[page up]",
    "PageDown":   "[page down]",
    "F1":         "[f1]",
    "F2":         "[f2]",
    "F3":         "[f3]",
    "F4":         "[f4]",
    "F5":         "[f5]",
    "F6":         "[f6]",
    "F7":         "[f7]",
    "F8":         "[f8]",
    "F9":         "[f9]",
    "F10":        "[f10]",
    "F11":        "[f11]",
    "F12":        "[f12]",
    "U+0008":     "[bksp]"
};

/*
 *   Document-level click handler for inputEvent() requests from the server.
 *   Unhandled hyperlink clicks should be sent here, so we can send them on
 *   to the server as inputEvent() results.  
 */
function clickToServer(ev, href,ele)
{
    // if the server wants an input event, send it
    if (wantInputEvent)
    {
        // send the event to the server
        serverRequest("/webui/inputEvent?type=href&param="
                      + encodeURIComponent(href));

        // we're no longer waiting for an input event
        wantInputEvent = false;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   resize the main window 
 */
function mainResize()
{
    // move the errorPopupDiv to the bottom right, if it's visible
    if (errorPopupVis)
    {
        var rc = getWindowRect();
        var ediv = $("errorPopupDiv");
        ediv.style.left = (rc.width - ediv.offsetWidth) + "px";
        ediv.style.top = (rc.height - ediv.offsetHeight) + "px";
    }
    if (downloadFiles.length)
    {
        var rc = getWindowRect();
        var ddiv = $("downloadPopupDiv");
        ddiv.style.left = (rc.width - ddiv.offsetWidth) + "px";
        var top = rc.height - ddiv.offsetHeight;
        if (errorPopupVis)
            top -= ediv.offsetHeight;
        ddiv.style.top = top + "px";
    }

    // reposition any dialogs
    for (var i = 0 ; i < dialogStack.length ; ++i)
    {
        if (!dialogStack[i].manuallyPositioned)
            positionDialog(dialogStack[i].ele, false);
    }

    // inherit the layout window handling
    calcLayout();
}

/* ------------------------------------------------------------------------ */
/*
 *   Close the window 
 */
function mainUnload()
{
    if (debugLogWin)
    {
        debugLogWin.close();
        debugLogWin = null;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Error logging.  This is used mainly for network connection errors.
 *   Network errors are often due to temporary conditions - network
 *   congestion, router latency, server hiccups.  Many times, an XML request
 *   will fail due to a network hiccup, but retrying the same request will
 *   succeed.  We therefore don't want to be too strident about network
 *   errors, but at the same time we don't want to completely ignore them.
 *   Our compromise is a small message panel that appears at the bottom
 *   corner of the main window when an error occurs.  This alerts the user to
 *   the error, and offers an option to see more details.  This doesn't
 *   intrude too much into the UI visually, and doesn't intrude at all
 *   modally, since there's no need to acknowledge or dismiss the popup.  
 */

// error log
var errorLog = [];

// Is the error popup visible?  This is the little button that we show
// in the bottom right corner when a new error is available to be viewed.
var errorPopupVis = false;

// Add an error to the error log.  'msg' is the explanatory error message
// text, and 'detail' is addition technical detail that might be helpful
// for a programmer debugging the error (javascript stack trace, bad XML
// text, HTTP result code, etc, as appropriate to the specific error
// condition).
//
// 'id' is an optional object that tells us the status of an error that
// might be possible to resolve automatically with retries.  First, this
// serves to tie together a series of errors that results from a series
// of retries for the same operation.  For example, for XML requests,
// the 'id' is the request descriptor object.  Tying the retry errors
// to the original error is useful because (a) the error log can show
// the relationship, so that the user doesn't think five separate things
// went wrong when they were really five repeated failed attempts at
// the same thing, and (b) if a retry eventually succeeds, the error log
// can show that the earlier attempts were eventually resolved and thus
// can be ignored.  Second, the 'id' object has two fields that tell us
// whether the operation is in fact being retried:
//
//   id.failed - true if the operation has permanently failed and will not
//       be retried, false if the operation is being retried
//   id.succeeded - true if the operation has succeeded on a retry
//
// If 'id' is null, we'll supply a default.
function logError(msg, detail, id)
{
    // for testing purposes, it's sometimes useful to pop this up
    // in the separate debug log window, but we don't want to do
    // this in release builds
    // debugLog("Error: " + msg + "<br>" + detail + "<br><br>");

    // if there's no 'id' object, supply a default
    if (!id)
        id = new LogErrorID();
             
    // add the message at the start of the error list
    errorLog.unshift({msg: msg, detail: detail, id: id, viewed: false,
                      timestamp: new Date()});

    // if the error frame isn't visible, show it
    if (!errorPopupVis)
    {
        // mark it as visible, but make it completely transparent
        errorPopupVis = true;
        var d = $("errorPopupDiv");
        setAlpha(d, 0);

        // do an explicit size recalc to position the popup
        mainResize();

        // start fading it in
        startFade(d, 1, 700);
    }

    // If all unviewed errors are being retried automatically, set the
    // error popup background to orange.  Otherwise set it to red.
    var allRetry = true;
    for (var i = 0 ; i < errorLog.length ; ++i)
    {
        // get this error
        var e = errorLog[i];

        // if it's unviewed, and it's marked as failed, we have an error
        // that won't be retried
        if (!e.viewed && e.id.failed)
        {
            allRetry = false;
            break;
        }
    }

    // set the appropriate background color in the popup
    $("errorPopupDiv.[content]").style.background =
        (allRetry ? "#ff8000" : "#ff0000");
    $("errorPopupDiv.[icon]").style.backgroundImage =
        (allRetry ? "url(/webuires/warningPopupBkg.gif)"
                  : "url(/webuires/errorPopupBkg.gif)");
}

function LogErrorID()
{
    this.succeeded = false;
    this.failed = true;
}

function openErrorDetail(i)
{
    $("errorDetailDiv-" + i).style.display = "block";
    $("openErrorDetailLink-" + i).style.display = "none";
}

// Un-log a network error.  Call this when a request that failed with an
// error subsequently succeeds on a retry.
function unlogError(id)
{
    // find the error in the error log
    var openCnt = 0;
    for (var i = 0 ; i < errorLog.length ; ++i)
    {
        // get this error
        var e = errorLog[i];
        
        // if this error isn't resolved and hasn't been viewed,
        // count it as open
        if (!e.id.succeeded && !e.viewed)
            ++openCnt;
    }

    // if there are now no open errors, the error popup is no longer needed
    if (openCnt == 0)
        hideErrorPopup();
}

// display the error log
var errorLogViewCount = 0;
function showErrorLog()
{
    // build the error list
    var lst = [];
    for (var i = 0 ; i < errorLog.length ; ++i)
    {
        var msg = errorLog[i].msg;
        var id = errorLog[i].id;
        var detail = errorLog[i].detail;
        var ts = errorLog[i].timestamp;
        if (detail)
        {
            msg +=
                "<div class=\"errorFooter\">"
                + ts.toLocaleTimeString()
                + (errorLogViewCount == 0 ? "" :
                   errorLog[i].viewed ? " | Previously viewed" : " | New")
                + (id.succeeded
                   ? " | Error was resolved automatically" :
                   !id.failed
                   ? " | Request is being retried automatically" :
                   "")
                + "<span id=\"openErrorDetailLink-" + i + "\"> | "
                + "<a href=\"#\" onclick=\"javascript:openErrorDetail("
                + i + ");return false;\">View Details</a></span>"
                + "</div>"
                + "<div class=\"errorDetail\" id=\"errorDetailDiv-"
                + i + "\">" + detail
                + "</div>";
        }
        lst.push(msg);
    }

    // count this view of the error log
    errorLogViewCount++;

    // mark all messages as viewed
    for (var i = 0 ; i < errorLog.length ; ++i)
        errorLog[i].viewed = true;

    // If there's a single error, show it as the whole dialog contents.
    // Otherwise show a scrolling list of the errors.
    var cont;
    if (errorLog.length == 0)
    {
        cont = "This window shows any errors encountered in the browser "
               + "since you loaded the page, including network problems "
               + "and javascript errors. So far, no errors have occurred "
               + "since you loaded or refreshed the page.";
    }
    else if (errorLog.length == 1)
    {
        // one error - just show the error
        cont = lst[0];
    }
    else
    {
        // Multiple errors - show a scrolling list
        cont = "The game encountered the following errors (the most "
               + "recent is listed first):"
               + "<div class='errorListDiv'>"
               + lst.join("<hr>")
               + "</div>";
    }        

    // hide the error popup
    hideErrorPopup();

    // show the dialog
    showDialog({ title: "Error Log", id: 'errorLog', contents: cont });
}

// hide the error log popup
function hideErrorPopup()
{
    // move the popup division off-screen
    errorPopupVis = false;
    var d = $("errorPopupDiv");
    startFade(d, -1, 250, function() {
        d.style.top = "-1000px";
        mainResize();
    });
}

/* ------------------------------------------------------------------------ */
/*
 *   Download popup.  This appears at the lower right corner of the window,
 *   just above the error popup if there is one.  This shows the list of
 *   outstanding downloadable files sent by the game server.  
 */

// list of downloadable files
var downloadFiles = [];

// add a downloadable file
function addDownloadFile(fname)
{
    // add the file to the list
    downloadFiles.push(fname);

    // adjust the height
    var d = $("downloadPopupDiv");

    // rebuild the download list
    rebuildDownloadDiv();

    // if this is the first file, show the popup
    if (downloadFiles.length == 1)
    {
        // make it completely transparent, and start fading it in
        setAlpha(d, 0);
        startFade(d, 1, 700);
    }

    // do an explicit size recalc to position and resize the popup
    mainResize();
}

// remove a downloadable file
function removeDownloadFile(fname)
{
    // find the file
    var idx = downloadFiles.indexOf(fname);
    if (idx >= 0)
    {
        // remove it from the list
        downloadFiles.splice(idx, 1);

        // rebuild the download list
        rebuildDownloadDiv();

        // if the list is empty, hide the popup, otherwise resize it
        var cnt = downloadFiles.length;
        var d = $("downloadPopupDiv");
        if (cnt == 0)
        {
            // empty - fade it out
            startFade(d, -1, 250, function() {
                d.style.top = "-1000px";
            });
        }
        else
        {
            // do an explicit resize to position and size the popup
            mainResize();
        }
    }
}

// rebuild the download file division contents
function rebuildDownloadDiv()
{
    var s = ["<b>Download Files</b>"];
    downloadFiles.forEach(function(fname) {
        var shortName = fname.replace(/^.*\//, "").htmlify();
        var href = encodeURI(fname);
        s.push("<div class='dlfile'>&bull; "
               + "<a href=\"#\" onclick=\"javascript:addDownloadFrame('"
               + href + "');return false;\">" + shortName + "</a>"
               + " | <a href=\"#\" onclick=\"javascript:cancelDownloadFile('"
               + href + "');return false;\">Cancel</a>"
               + "</div>");
    });

    $("downloadPopupDiv.[content]").innerHTML = s.join("");
}

function cancelDownloadFile(fname)
{
    serverRequest(fname + "?cancel");
}

/* ------------------------------------------------------------------------ */
/*
 *   Debugging messages.  During development, it's often useful to instrument
 *   the code with message displays; this provides a place to put them that's
 *   less intrusive than 'alert()'.  Messages written here are displayed in a
 *   separate popup window that we open the first time this is called.  
 */
var debugLogWin = null, debugLogLoaded = new AbsEvent();
function debugLog(msg)
{
    // if the log window isn't open yet, or it's been closed, open it
    if (!debugLogWin || debugLogWin.closed)
    {
        debugLogLoaded.isDone = false;
        debugLogWin = window.open(
            "/webuires/debuglog.htm", "tadsDebugLog",
            "location=no, resizable=yes, scrollbars=yes, status=yes, "
            + "width=550, height=400");
    }

    // when the log window is loaded, add the message
    debugLogLoaded.whenDone(function() {
        debugLogWin.debugLogPrint(msg);
    });
}

/* ------------------------------------------------------------------------ */
/*
 *   Show an HTML dialog.  This shows the dialog as a foreground DIV within
 *   the main window, not as an 'alert()' or new window popup.
 *   
 *   'id' is an optional identification string for identifying the dialog
 *   within the program.  This is useful if you want to be sure that the
 *   active dialog is a particular dialog, rather than a nested dialog it
 *   opens.  We don't make any of this here; it's purely for the dialog's own
 *   use in other scripts.
 *   
 *   'params' is an object with the properties listed below.  All properties
 *   are optional except the ones marked required.
 *   
 *   title: a string to display in the dialog's title bar.  Required.
 *   
 *   contents: a string giving the HTML contents of the dialog, OR a DOM
 *   element whose contents will be duplicated to populate the dialog.
 *   Required.
 *   
 *   buttons: an array of button objects.  Each object has these members:
 *.  {
 *.    name: string with the display name of the button
 *.    isDefault: true if pressing the Return key activates this button
 *.    isCancel: true if pressing the Escape key activates this button
 *.    onclick: function to call when clicking the button; if not set,
 *.      clicking the button simply dismisses the dialog
 *.    key: the key that can be used to select this button; applies only
 *.      when focus isn't in an input control; you can specify either an
 *.      ordinary character (using upper-case for letters), or a key
 *.      name (e.g., 'U+0008' for backspace)
 *.  }
 *   
 *   If you omit the name of the button, the button won't be displayed, but
 *   will still be available as a keystroke handler.
 *   
 *   init: a function to call to initialize the dialog.  This is called after
 *   the new dialog has been set up, but before it's been displayed.  The
 *   dialog's root element is passed as the argument - init(dialogElement).
 *   
 *   initFocus: the control that should initially receive focus.  If this
 *   isn't set, we'll search for a focusable control (INPUT, SELECT, <A>) and
 *   set focus there.
 *   
 *   initAfterVisible: a function to call to initialize after the dialog has
 *   been displayed and focus has been set.  As with init(), this is called
 *   with the dialog's root element as the argument.
 *   
 *   dismiss: a function to call when dismissing the dialog.  This will be
 *   called as dismiss(dialogElement, button), where 'button' is the button
 *   object from the 'buttons' list corresponding to the button the user
 *   clicked to close the dialog.  Return true to allow the dialog to close,
 *   false to prevent it from closing.  You can use a false return to perform
 *   validation.
 *   
 *   dimensions: an object { width:, height: } with style strings to use for
 *   the dimensions of the dialog.  If not provided, we'll let the browser
 *   set dimensions based on the content size.
 *   
 *   fillWidth: true if the dialog contents should be stretched to fill the
 *   whole width of the containing dialog.  By default, we'll show the
 *   contents at their natural width, centered in the dialog container.  In
 *   some cases we instead want the contents to scale according to the
 *   container, though, such as when the dialog contains an iframe.  Set this
 *   to true to stretch the contents to the container width.  
 */
var dialogStack = [];
var fontSelectorOptions = null;
function showDialog(params)
{
    // get the contents, retrieving HTML source if it's a DOM object
    var contents = params.contents;
    if (typeof(contents) == "object")
        contents = contents.innerHTML;

    // assume all we'll need to do is show the dialog
    var go = function() { showDialogMain(params); };

    // check for $[fontpicker] macros
    if (contents.match(/\$\[fontpicker([^\]]*)\]/))
    {
        // we have one or more font pickers, so we need to build the 
        // list of installed fonts before we can show the dialog
        var go1 = go;
        go = function() { TADS_swf.waitForFontList(go1); };
    }

    // execute the dialog display procedure we've built up
    go();
}

// main dialog builder
function showDialogMain(params)
{
    // figure the z-index for the new dialog
    var z = 10500 + dialogStack.length*10;
    var dlgID = "dialogDiv" + (dialogStack.length + 1);

    // create the global descriptor for the dialog
    var dlgDesc = {
        title: params.title,
        dd: dlgID, 
        manuallyPositioned: false,
        dismiss: params.dismiss,
        initFocus: params.initFocus,
        id: params.id
    };
    
    // push it onto the active dialog stack
    dialogStack.push(dlgDesc);

    // create the background cover for the new dialog
    var bkg = dlgDesc.bkg = document.createElement("div");
    bkg.className = "dialogCoverDiv";
    bkg.style.zIndex = z;
    document.body.appendChild(bkg);

    // create the canvas
    var canvas = dlgDesc.canvas = document.createElement("div");
    canvas.className = "dialogCanvas";
    canvas.style.zIndex = "" + (z + 1);
    document.body.appendChild(canvas);

    // create the new dialog division
    var dlg = dlgDesc.ele = document.createElement("div");
    dlg.id = dlgID;
    dlg.innerHTML = $("dialogTemplate").innerHTML;
    dlg.className = "dialogDiv";
    dlg.style.zIndex = "" + (z + 1);
    dlg.onkeydown = function(ev) { return $kd(ev, dlgKey, dlgDesc); };
    dlg.onkeypress = function(ev) { return $kp(ev, dlgKey, dlgDesc); };
    canvas.appendChild(dlg);

    // if dimensions were given, set them in the dialog element
    if (params.dimensions && params.dimensions.height)
        dlg.style.height = params.dimensions.height;
    if (params.dimensions && params.dimensions.width)
        dlg.style.width = params.dimensions.width;

    // get a couple of elements for further manipulation
    var ttl = $(dlg, "[dialogTitle]");
    var cont = $(dlg, "[dialogContent]");

    // if desired, stretch the content division to fill the dialog
    if (params.fillWidth)
        cont.style.width = "100%";

    // fill in the title
    ttl.innerHTML = params.title;

    // set default buttons if none were given
    var buttons = params.buttons;
    if (!buttons)
        buttons = [{name: "OK", isDefault: true, isCancel: true}];

    // save the buttons in the dialog descriptor
    dlgDesc.buttons = buttons || [];

    // if it's a pre-defined division, fetch its contents
    var contents = params.contents;
    if (typeof(contents) == "object")
        contents = contents.innerHTML;

    // expand $[fontpicker] macros
    var fontSizeOptions =
        [8, 9, 10, 11, 12, 14, 16, 18, 20,
         22, 24, 26, 28, 30, 32, 36, 48, 72]
        .map(function(x) {
        return "<option value=\"" + x + "pt\">" + x + "pt</option>";
    }).join("");
    fontSizeOptions = "<option value=\"*\">(Default)</option>"
                      + fontSizeOptions;
    var fontPickers = [];
    var preFontCont = contents;
    contents = contents.replace(
        /\$\[fontpicker([^\]]*)\]/gm, function(match, attrs) {

        // parse the attributes
        var pattrs = parseTagAttrs(attrs);

        // add this to the font select pending list
        fontPickers.push({ attrs: attrs, pattrs: pattrs });

        // Build the font picker: a combo for the font name, another
        // for the font size, and a color picker for the text color.
        // Set the font picker select list to initially empty, since
        // we might have to wait for the font lister thread to finish.
        return createCombo(pattrs.id, 40, "")
            + createCombo(pattrs.id + "Size", 5, fontSizeOptions);
    });

    // note if we actually needed any font pickers - we do if we found
    // any $[fontpicker] macros to expand
    var needFonts = (contents != preFontCont);

    // expand $[colorpicker] macros
    var colorPickers = [];
    contents = contents.replace(
        /\$\[colorpicker([^\]]*)\]/gm, function(match, attrs) {
        var pattrs = parseTagAttrs(attrs);
        var btnID = dlgID + "." + pattrs.id;
        colorPickers[pattrs.id] = pattrs;
        return "<a href=\"#\" " + attrs + " class=\"colorbutton\" "
            + "onclick=\"javascript:openColorPicker(event, '" + btnID
            + "', true);return false;\" "
            + "onkeydown=\"javascript:return $kd(event, colorbuttonKey, ['"
            + btnID + "',true]);\" "
            + "onkeypress=\"javascript:return $kp(event, colorbuttonKey, ['"
            + btnID + "',true]);\">"
            + "<span></span></a>";
    });

    // expand $[checkbox]
    contents = contents.replace(
        /\$\[checkbox([^\]]*)\]/gm, function(match, attrs) {
        var pattrs = parseTagAttrs(attrs);
        return "<label><nobr><input type=\"checkbox\" " + attrs
            + "><label for=\"" + pattrs.id + "\"> " + pattrs.label
            + "</label></nobr></label>";
    });

    // add the buttons to the contents
    if (buttons.length)
    {
        contents += "<div class=\"dialogButtons\">";
        for (var i = 0 ; i < buttons.length ; ++i)
        {
            var b = buttons[i];
            if (b.name)
            {
                contents += "<span style='padding: 0 1em 0 1em;'>"
                            + "<a href='#' "
                            + "onclick='javscript:clickDialogButton("
                            + i + ");return false;'>" + b.name
                            + "</a></span>";
            }
        }
        contents += "</div>";
    }

    // fill in the contents
    cont.innerHTML =
        "<div class=\""
        + (params.fillWidth ? "dialogZeroMarginDiv" : "dialogMarginDiv")
        + "\">" + contents + "</div>";

    // call the initializer function
    if (params.init)
        params.init(dlg, params);

    // initialize the contents and adjust the size immediately
    positionDialog(dlg, true);

    // just in case the browser needs a moment to finish processing the
    // innerHTML changes, resize it again in a few moments
    setTimeout(function() {
        // set the size again
        positionDialog(dlg, true);

        // set it opaque
        setAlpha(canvas, 100);

        // set the initial focus
        initDialogFocus();

        // run the after-visible function
        if (params.initAfterVisible)
            setTimeout(
                function() { params.initAfterVisible(dlg, params); },
                10);
    }, 1);

    // If we needed any font pickers, load the system's font list, if
    // we haven't already.  When that's done, populate the font pickers.
    if (needFonts)
    {
        // get the cached font list
        var fonts = TADS_swf.fontList;

        // build the font select list
        fontSelectorOptions = fonts.map(function(ele) {
            ele = ele.htmlify();
            return "<option value=\"" + ele + "\">" + ele + "</option>"
        }).join("");

        // add "Default" as the first item
        fontSelectorOptions = "<option value=\"*\">(Default)</option>"
                              + fontSelectorOptions;
        
        // populate each font picker
        for (var i = 0 ; i < fontPickers.length ; ++i)
        {
            // get this font picker's field, and from that get the <select>
            var f = fontPickers[i];
            var fld = $(dlg, f.pattrs.id);
            var sel = comboGetElement(fld, "SELECT");
            
            // If we have outerHTML, replace the entire SELECT with a new
            // one.  This is for IE's sake; IE doesn't rebuild the option
            // list if we only replace the SELECT's innerHTML.  If we don't
            // have outerHTML, replace the innerHTML and hope for the best;
            // this seems to work properly on other browsers tested.
            //
            // (Why bother with the inner/outerHTML at all?  Why not just
            // rebuild the option list via the Select/Option object API?
            // That's arguably the "cleaner" way to do this, but in my
            // testing it's *vastly* faster to let the browser rebuild
            // the control from HTML.  I think the best argument in favor
            // of the API approach is that it should perform better than
            // going through all that string building and parsing, but in
            // this case, at least, it turns out to be just the opposite.
            if (sel.outerHTML)
            {
                // outerHTML available - replace the whole <SELECT>
                sel.outerHTML = sel.outerHTML.replace(
                    /<\/select>/im, fontSelectorOptions + "</SELECT>")
                    .replace(/size=("[^"]+"|'[^']'|[^'"][^\s>]*)/im,
                             "size=\"20\"");
            }
            else
            {
                // no outerHTML - replace just the option list
                sel.innerHTML = fontSelectorOptions;
                sel.size = "20";
            }
        }
    }
}

/*
 *   Initialize focus in the currently active dialog 
 */
function initDialogFocus()
{
    // ignore this if there's no dialog
    if (dialogStack.length == 0)
        return;
    
    // get the dialog description
    var desc = dialogStack.top();
    var dlg = desc.ele;

    // get the initial focus control specified in the parameters
    var f;
    if (desc.initFocus)
        f = $(dlg, desc.initFocus);
    
    // if that wasn't specified, search for the first focusable object
    // within the dialog
    if (!f)
    {
        f = breadthSearch(dlg, function(ele) {
            return (ele.focus
                    && (ele.nodeName == "INPUT"
                        || ele.nodeName == "TEXTAREA"
                        || ele.nodeName == "SELECT"
                        || ele.nodeName == "A"));
        });
    }
    
    // Move focus to the control, if we found one, or to the dialog
    // itself if not.  If the dialog doesn't accept focus, just
    // remove focus from wherever it is now.
    if (f)
    {
        // move focus to the control
        f.focus();
        
        // select its initial text if it's an INPUT or TEXTAREA
        if (f.nodeName == "INPUT" || f.nodeName == "TEXTAREA")
            setSelRangeInEle(f);
    }
    else if (dlg.focus)
        dlg.focus();
    else if (document.activeElement)
        document.activeElement.blur();
}

/*
 *   Reposition a dialog for the current window size 
 */
function positionDialog(dlg, init)
{
    // get the dialog and window areas
    var drc = getObjectRect(dlg);
    var wrc = getWindowRect();

    // center the dialog in the window
    var x = (wrc.width - drc.width)/2, y = (wrc.height - drc.height)/2;

    // if this is a sub-dialog, show it relative to the parent dialog
    // instead of centered
    if (init && dialogStack.length > 1)
    {
        // get the preceding dialog position
        var par = dialogStack[dialogStack.length - 2];
        var prc = getObjectRect(par.ele);

        // move the new dialog below and right of the parent
        x = prc.x + 25;
        y = prc.y + 25;

        // mark this dialog and the parent as manually positioned
        par.manuallyPositioned = true;
        dialogStack.top().manuallyPositioned = true;
    }

    // set the position
    dlg.style.left = (x > 0 ? x : 0) + "px";
    dlg.style.top = (y > 0 ? y : 0) + "px";
}

/*
 *   handle mouse tracking in a dialog title bar 
 */
var dlgTrackInfo = null;
function dlgTitleDown(ev)
{
    // if there's no dialog, ignore it
    if (dialogStack.length == 0)
        return;

    // get the dialog and its bounding box
    var dlg = dialogStack.top().ele;
    var rc = getObjectRect(dlg);

    // get the event
    ev = getEvent(ev);

    // note the start offset of the tracking
    dlgTrackInfo = { x: ev.clientX - rc.x, y: ev.clientY - rc.y, dlg: dlg };

    // track the mouse throughout the document
    addEventHandler(document, "mousemove", dlgTitleMove);
    addEventHandler(document, "mouseup", dlgTitleUp);
    addEventHandler(document, "selectstart", dlgTitleSelect);

    // Show the dialog's "move cover" division.  This is a transparent
    // division that covers the whole dialog at a higher z-index than
    // any of its contents.  The point is to block out any IFRAME within
    // the dialog for the duration of the move, so that events are directed
    // to our document rather than to the IFRAME's document.  If we didn't
    // have this cover, mouse events would disappear into the IFRAME any
    // time the mouse moves quickly enough to stray into the IFRAME before
    // it generates a mousemove event within our title bar.  Cross-domain
    // scripting limits prevent those events from bubbling up to our
    // document handlers if the IFRAME is showing an outside page (such
    // as from the storage server).  The dialog cover blocks the IFRAME
    // and keeps all the events within our document.
    var cover = $(dlg, "[dialogMoveCover]");
    cover.style.display = "block";
    cover.style.width = rc.width + "px";
    cover.style.height = rc.height + "px";

    // skip the default handling
    return false;
}

// prevent text selection while tracking the mouse in IE
function dlgTitleSelect(ev)
{
    return false;
}

function dlgTitleMove(ev)
{
    // if we're not tracking a dialog, skip it
    if (!dlgTrackInfo)
        return;

    // get the event
    ev = getEvent(ev);

    // move the dialog
    var dlg = dlgTrackInfo.dlg;
    dlg.style.left = (ev.clientX - dlgTrackInfo.x) + "px";
    dlg.style.top = (ev.clientY - dlgTrackInfo.y) + "px";

    // mark the dialog as manually moved
    dialogStack.top().manuallyPositioned = true;

    // suppress the default handling
    preventDefault(ev);
    cancelBubble(ev);
    return false;
}

function dlgTitleUp(ev)
{
    // if we're not tracking a dialog, skip it
    if (!dlgTrackInfo)
        return;

    // remove the event handlers that we installed just for the move
    removeEventHandler(document, "mousemove", dlgTitleMove);
    removeEventHandler(document, "mouseup", dlgTitleUp);
    removeEventHandler(document, "selectstart", dlgTitleSelect);

    // remove the dialog cover division
    $(dlgTrackInfo.dlg, "[dialogMoveCover]").style.display = "none";

    // done tracking
    dlgTrackInfo = null;

    // suppress the default handling
    preventDefault(ev);
    cancelBubble(ev);
    return false;
}

/*
 *   Handle a keystroke in a color button in the main dialog.  We'll let
 *   Enter keystrokes get handled in the <A>, but we'll block these from
 *   bubbling up to the dialog.  This lets us use the keyboard interface to
 *   select a color button without dismissing the dialog.  
 */
function colorbuttonKey(desc, ctx)
{
    var ele = ctx[0];
    var hasDefault = ctx[1];
    if (desc.keyName == "Enter" || desc.keyName == "U+0020")
    {
        openColorPicker(desc.event, ele, hasDefault);
        cancelBubble(desc.event);
        preventDefault(desc.event);
        return false;
    }
    return true;
}

/*
 *   Open a color picker 
 */
var colorPickerBuilt = false;
var colorPickerTarget = null;
function openColorPicker(ev, ele, hasDefault)
{
    // get the button and the color picker popup
    ele = $(ele);
    var cp = $("colorPicker");

    // if it's already open, close it
    if (cp.style.display == "block")
    {
        closePopup(cp);
        return;
    }

    // build out the buttons if we haven't already done so
    if (!colorPickerBuilt)
    {
        var colors = ["ff8080", "ffff80", "80ff80", "ff80ff",
                      "80ffff", "0080ff", "ff80c0", "ff80ff",
                      "ff0000", "ffff00", "80ff00", "00ff40",
                      "00ffff", "0080c0", "8080c0", "ff00ff",
                      "804040", "ff8040", "00ff00", "008080",
                      "004080", "8080ff", "800040", "ff0080",
                      "800000", "ff8000", "008000", "008040",
                      "0000ff", "0000A0", "800080", "8000ff",
                      "400000", "804000", "004000", "004040",
                      "000080", "000040", "400040", "400080",
                      "000000", "808000", "808040", "808080",
                      "408080", "C0C0C0", "400000", "ffffff"];

        // set up the template for each button
        var keyTpl = "\"javascript:return @FUNC@"
                     + "(event,colorBtnKey,['#@COLOR@',this]);\"";
        var btnTpl = "<a class=\"colorPickerButton\" href=\"#\" "
                     + "id=\"colorPickerButton#@I@\" "
                     + "style=\"background-color:#@COLOR@;\" "
                     + "onclick=\"javascript:return colorBtnClick("
                     + "event,'#@COLOR@',this);\" "
                     + " onkeydown=" + keyTpl.replace(/@FUNC@/m, "$kd")
                     + " onkeypress=" + keyTpl.replace(/@FUNC@/m, "$kp")
                     + "></a>";
        
        // build the button array - eight buttons per line
        for (var i = 0, s = [] ; i < colors.length ; )
        {
            // build this line
            for (var j = 0, sl = [] ; j < 8 ; ++j, ++i)
            {
                // build this button and add it to the line list
                sl.push(btnTpl
                        .replace(/@COLOR@/gm, colors[i])
                        .replace(/@I@/gm, i));
            }

            // combine the line into a <nobr> and add it to the master list
            s.push("<nobr>" + sl.join("") + "</nobr><br>");
        }

        // add the custom color entry field
        s.push("<div><nobr>Custom (rrggbb): <input type=\"text\" size=\"6\" "
               + "id=\"colorPickerCustom\" "
               + "onkeydown=\"javascript:return $kd(event,colorPickFldKey);\" "
               + "onkeypress=\"javascript:return $kp(event,colorPickFldKey);\">"
               + "</nobr></div>");

        // add the Use Default button
        s.push("<div id=\"colorPickerDefault\" class=\"colorPickerDefault\">"
               + "<div class=\"off\" "
               + "onmouseover=\"javascript:"
               +    "this.style.borderColor='#ffffff';\" "
               + "onmouseout=\"javascript:"
               +    "this.style.borderColor='transparent';\" "
               + "style=\"border: 1px solid transparent;\">"
               + "<a href=\"#\" onclick=\"javascript:"
               + "return colorDefaultClick(event,this);\">"
               + "<img src=\"/webuires/ckbox8pt-off.gif\">Use default color</a>"
               + "</div></div>");

        // combine the element list into a string and plug it into the
        // popup division
        cp.innerHTML = s.join("");

        // we've now built the color picker
        colorPickerBuilt = true;
    }

    // remember the opener button as the value destination on close
    colorPickerTarget = ele;

    // move the popup under the opener buton
    var rc = getObjectRect(ele);
    cp.style.left = rc.x + "px";
    cp.style.top = (rc.y + rc.height) + "px";
    cp.style.display = "block";
    openPopup(ev, cp, null, ele, ele);

    // Force the browser to recalculate the layout after digesting its
    // internal event queue.  IE for one has problems figuring containing
    // box sizes on the first pass, but gets it right if we explicitly
    // resize the box (even setting it to its current size) later.
    setTimeout(function() {
        cp.style.width = getObjectRect(cp).width + "px";
    }, 1);

    // get the current color
    var curColor = canonicalizeColor(getStyle(ele, "background-color"));

    // set the custom field to the current color
    var customFld = $("colorPickerCustom");
    customFld.value = curColor.substr(1);

    // if there's a button showing the current selected color, focus it
    var sel = breadthSearch(cp, function(ele) {
        return (ele.nodeName == "A"
                && ele.className == "colorPickerButton"
                && ele.style.backgroundColor == curColor);
    });

    // set up the Default button
    var dflt = $("colorPicker.colorPickerDefault");
    if (hasDefault)
    {
        dflt.style.display = "block";
        checkColorPickerDefault(ele.useDefaultColor);
    }
    else
        dflt.style.display = "none";

    // if we have a selection, set focus there; otherwise set focus on
    // the custom field
    if (sel)
        sel.focus();
    else
    {
        customFld.focus();
        setSelRangeInEle(customFld);
    }
}

/*
 *   handle a key in the color picker popup background 
 */
function colorPopupKey(desc)
{
    switch (desc.keyName)
    {
    case "Up":
    case "Down":
    case "Left":
    case "Right":
        // if I have focus directly (i.e., it's not in a child),
        // move focus to the first button on any arrow key
        if (document.activeElement == $("colorPicker"))
            $("colorPicker.<A>").focus();
        break;
    }

    // skip default actions on all keys
    return false;
}

/*
 *   Click on a color picker opener button
 */
function colorBtnClick(ev, color)
{
    // select the color, and de-select "Use Default"
    colorPickerApply(color, false);

    // close the popup
    closePopup();

    // don't process the event further
    return false;
}

/*
 *   apply a change to a color picker button 
 */
function colorPickerApply(color, useDefault)
{
    // set the color of the color element
    setColorButton(colorPickerTarget, color, useDefault);

    // generate an onchange event
    sendEvent(colorPickerTarget, createEvent("Event", "change"));
}

/*
 *   click on the Use Default button in a color picker
 */
function colorDefaultClick(ev, ele)
{
    // set Use Default and close the popop
    colorPickerApply(null, true);
    closePopup();
    return false;
}

function checkColorPickerDefault(checked)
{
    var span = $("colorPicker.colorPickerDefault.<DIV>");
    var img = $("colorPicker.colorPickerDefault.<IMG>");
    if (checked)
    {
        span.className = "on";
        img.src = "/webuires/ckbox8pt-on.gif";
    }
    else
    {
        span.className = "off";
        img.src = "/webuires/ckbox8pt-off.gif";
    }
}

/*
 *   handle a key in a color picker button 
 */
function colorBtnKey(desc, ctx)
{
    var color = ctx[0], ele = ctx[1];
    var b = null;
    var nextButton = function(b, dir)
    {
        var idx = parseInt(b.id.substr(18));
        return $("colorPicker.colorPickerButton#" + (idx + dir));
    };
    switch (desc.keyName)
    {
    case "Up":
        for (var i = 0, b = ele ; i < 8 && b ; ++i)
            b = nextButton(b, -1);
        break;
        
    case "Down":
        for (var i = 0, b = ele ; i < 8 && b ; ++i)
            b = nextButton(b, 1);
        break;

    case "Left":
        b = nextButton(ele, -1);
        break;
        
    case "Right":
        b = nextButton(ele, 1);
        break;

    case "U+0020":
    case "Enter":
        // select the color, and de-select "Use Default"
        colorPickerApply(color, false);

        // close the popup and swallow the event
        closePopup();
        cancelBubble(desc.event);
        preventDefault(desc.event);
        break;

    case "U+0009":
        // we're on a button, so tab into the field
        var fld = $("colorPickerCustom");
        fld.focus();
        setSelRangeInEle(fld);
        
        // bypass all the normal handling
        cancelBubble(desc.event);
        preventDefault(desc.event);
        return false;
    }

    // move the focus to the new target, if we found one
    if (b)
    {
        // move focus to the new button, and select its color
        b.focus();
        $("colorPickerCustom").value = b.style.backgroundColor.substr(1);

        // uncheck "Use default"
        checkColorPickerDefault(false);
    }

    // don't bother bubbling the event
    return false;
}

/*
 *   Keystroke in color picker custom color field
 */
function colorPickFldKey(desc)
{
    switch (desc.keyName)
    {
    case "Enter":
        var v = $("colorPickerCustom").value;
        if (v.charAt(0) != '#')
            v = '#' + v;
        if (v.match(/^#[0-9a-f]{6}$/i))
        {
            // valid - select the color and de-select "Use Default"
            colorPickerApply(v, false);

            // close the popup
            closePopup();
        }
        else
        {
            cancelBubble(desc.event);
            alert("Please use the HTML-style hexadecimal RRGGBB format. "
                  + "For example, for bright red, enter FF0000.");
        }

        // bypass all the normal handling
        cancelBubble(desc.event);
        preventDefault(desc.event);
        return false;

    case "U+0009":
        // if the custom color corresponds to a button, tab to the button
        var fld = $("colorPickerCustom");
        var val = parseColor(fld.value);
        var b = breadthSearch($("colorPicker"), function(ele) {
            return (ele.nodeName == "A"
                    && ele.className == "colorPickerButton"
                    && parseColor(ele.style.backgroundColor) == val);
        });
        if (b)
        {
            b.focus();
            fld.value = b.style.backgroundColor.substr(1);
        }

        // bypass all the normal handling
        cancelBubble(desc.event);
        preventDefault(desc.event);
        return false;

    default:
        // do the default processing for other keys
        return true;
    }
}

/*
 *   Click a dialog button 
 */
function clickDialogButton(n)
{
    // get the button info
    var b = dialogStack.top().buttons[n];

    // call the button function, or just dismiss the dialog if there isn't one
    if (b.onclick)
        b.onclick();
    else
        dismissDialog(b);
}

/*
 *   Dismiss the dialog 
 */
function dismissDialog(btn)
{
    // get the dialog info
    var dinfo = dialogStack.top();
    var dlg = dinfo.ele;
    var bkg = dinfo.bkg;

    // if there's not button, it's the close box in the title bar - create
    // a pseudo button structure for it
    if (!btn)
        btn = { name: "(Close Box)", isCancel: true };

    // call the dismiss callback, if present
    if (dinfo.dismiss && !dinfo.dismiss(dlg, btn))
        return;

    // remove the dialog from the stack
    dialogStack.pop();

    // remove focus from any active dialog element
    if (document.activeElement)
        document.activeElement.blur();
    
    // remove them from the document
    document.body.removeChild(dinfo.canvas);
    document.body.removeChild(bkg);

    // move focus to the default input field in the document
    setDefaultFocus();
}

/*
 *   Dismiss a dialog by ID.  If the given dialog is open, we'll cancel it
 *   and any active child dialogs.  
 */
function dismissDialogById(id, btn)
{
    // scan to see if this dialog is open in the first place
    for (var i = 0 ; i < dialogStack.length ; ++i)
    {
        if (dialogStack[i].id == id)
        {
            // this is the one - dismiss all nested child dialogs
            while (dialogStack.length > i)
                dismissDialog(btn);

            // no need to look any further
            break;
        }
    }
}

/*
 *   Set the default focus.  If a dialog is active, set focus to the dialog.
 *   Otherwise set the focus to the default input field in the default child
 *   window. 
 */
function setDefaultFocus(desc, fromWin)
{
    // if the event is directed to a focus-keeping control, leave focus
    // where it is
    if (evtKeepsFocus(desc))
        return;

    // if a dialog is active, set focus there; otherwise refer this to
    // the default focusable child window
    if (dialogStack.length)
        initDialogFocus();
    else
        setDefaultFocusChild(desc, fromWin);

    // If the original event target isn't a control that reads keystrokes,
    // buffer the keystroke for the next time we want to read input.  This
    // ensures that we won't drop keystrokes between command input field
    // activations.
    bufferKey(desc);
}

// Type-ahead buffer.  When we receive keystrokes at the document level,
// and focus isn't in a control that reads keystrokes, we'll buffer the
// keys for future use when a focus-taking control is activated.
//
// The idea is that the IF user interface is "conversational": the UI
// interaction consists primarily of alternating prompts and user
// keyboard inputs.  Users become accustomed to this and will anticipate
// that a new command line (or other input reader) will be forthcoming
// shortly after each input is completed, so they'll sometimes start
// typing even before the new command line is visually activated.  In
// traditional terminal interfaces, these in-between keystrokes are
// nearly always buffered.  Our key buffering scheme here is designed to
// simulate that behavior.
var keybuf = [];
function bufferKey(desc)
{
    // don't buffer keystrokes if the server is waiting for an input
    // event - we'll instead send the key to the server (later)
    if (wantInputEvent)
        return;

    // if there's no descriptor, there's nothing to save
    if (!desc)
        return;

    // if the key has already been buffered, don't buffer it again
    if (desc.keyBufferedInMain)
        return;

    // discard CRs - IE9 sends CR LF for a newline, but we only want the LF
    if (desc.keyCode == 13)
        return;

    // note that the key has been buffered
    desc.keyBufferedInMain = true;

    // If the buffer's full, drop the oldest key.  The limit is arbitrary;
    // the point isn't to conserve memory or anything like that, but simply
    // to limit the UI weirdness that can happen if we buffer too many
    // keystrokes.  The problem is that if there's an extended period where
    // the UI is buffering keystrokes rather than reading them, then when we
    // do finally get around to activating an input control, the UI can seem
    // to be possessed by demons as the huge buffer plays back.  Limiting
    // the buffer size avoids this - if the buffer's small enough, playback
    // will be virtually instantaneous, so no demons.  There's really not
    // much of a tradeoff here, either, because most users will naturally
    // stop typing ahead anyway if the UI is unresponsive for any extended
    // period.
    if (keybuf.length >= 16)
        keybuf.shift();

    // make a copy of the key event for buffering, but remove the event
    // object, as it might not be valid after the handler returns
    var d = desc.copy(false);
    d.event = { };

    // buffer the event copy
    keybuf.push(d);
}


/*
 *   If the main body should get focus at any point, and gets a keystroke,
 *   send focus back to the default focus element.  This is mostly for the
 *   sake of dialog focus: we try to keep focus within the dialog by handling
 *   keyboard events in the dialog itself, but there are odd edge cases where
 *   key events bypass the dialog handlers and go to the browser's default
 *   handler, which can set focus into the main body.  This catch-all ensures
 *   that if focus is outside the dialog while the dialog is active, any
 *   keystrokes will trigger refocusing.  
 */
function mainBodyKey(desc)
{
    // if focus is in the main document body, move it back to the default
    // location (usually the command line, but sometimes a dialog object)
    if (isFieldKey(desc) && document.activeElement == document.body)
        setDefaultFocus(desc, window);

    // handle a generic document key
    genericDocKey(desc);

    // bubble the event
    return true;
}

/*
 *   Handle a key within one of our popup DIVs
 */
function popupDivKey(desc)
{
    // move focus back to the default location
    if (isFieldKey(desc))
        setDefaultFocus(desc, window);

    // bubble the event
    return true;
}

/*
 *   Handle a key in a dialog 
 */
function dlgKey(desc, dlg)
{
    // Check for explicit shortcut keys defined in the button list - these
    // override the default handling.  Shortcut keys apply only if the
    // current focus element doesn't use the keystroke directly (e.g., if
    // focus is in a text field, regular characters go to the field as
    // text editing entries.)
    var idx, ch = desc.ch ? desc.ch.toUpperCase() : "";
    if (!eleKeepsFocus(document.activeElement, desc)
        && (idx = dlg.buttons.indexWhich(function(b) {
               return b.key && (b.key == ch || b.key == desc.keyName);
            })) >= 0)
    {
        // click this button
        clickDialogButton(idx);
        
        // don't process the event any further
        cancelBubble(desc.event);
        preventDefault(desc.event);
        return false;
    }

    // check the key
    switch (desc.keyName)
    {
    case "Enter":
        // ignore this if the focus is in a button or link
        var e = document.activeElement;
        if ((e.nodeName == "INPUT" && e.type == "submit")
            || e.nodeName == "A")
        {
            // cancel the bubble, but let the default action occur
            cancelBubble(desc.event);
            return true;
        }

        // look for a default button
        for (var i = 0 ; i < dlg.buttons.length ; ++i)
        {
            // if this is the default button, click it
            if (dlg.buttons[i].isDefault)
            {
                // click the button
                clickDialogButton(i);
                break;
            }
        }

        // do no further processing on this key
        cancelBubble(desc.event);
        preventDefault(desc.event);
        return false;

    case "U+001B":
        // look for a cancel button
        for (var i = 0 ; i < dlg.buttons.length ; ++i)
        {
            // if this is the cancel button, click it
            if (dlg.buttons[i].isCancel)
            {
                // click the cancel button
                clickDialogButton(i);
                break;
            }
        }

        // do no further processing on this key
        cancelBubble(desc.event);
        preventDefault(desc.event);
        return false;

    case "U+0009":
        // check immediately after the browser processes the key to see
        // if focus has left the dialog, and move it back if so
        setTimeout(function() {

            // get the current focus and the active dialog
            var f = document.activeElement;

            // walk up the parent list to see if f is within the dialog
            for ( ; f && f != dlg.ele ; f = f.parentNode) ;

            // if not, move focus back to the dialog
            if (dlg.ele && !f)
                dlg.ele.focus();
            
        }, 0);

        // allow the default processing to proceed
        return true;

    default:
        // do the normal thing for other keys
        return true;
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Support functions for the TADS.swf Flash object.  This is an invisible
 *   Flash object that we use for certain functionality that we can't get
 *   directly from the standard browser/javascript APIs.  We use this for
 *   programmatic access only; it doesn't have any visual or UI presence.
 *   
 *   Flash is ubiquitous, but it's not universal: we can't count on it being
 *   available everywhere.  Our Flash object is invisible, so there's nothing
 *   visually missing when Flash isn't available, but we obviously won't have
 *   access to the API extensions we use it for.  We thus have to be careful
 *   to recover gracefully when the Flash object isn't present: we can use
 *   defaults, omit the desired functionality, or find some other approach to
 *   get similar functionality (e.g., proprietary browser features).
 *   
 *   We use the Flash object for the following features:
 *   
 *   - Sound playback.  Flash provides a good set of audio tools, including
 *   concurrent playback of multiple sounds, support for common audio
 *   formats, event notifications, volume control, and fades.
 *   
 *   - Font enumeration.  Flash can give us a list of the fonts installed on
 *   the system.  
 */
var TADS_swf = {
    
    // Event coordinator for TADS.swf load completion.  The swf object calls
    // loaded.fire() from its constructor via the Flash external interface
    // (the Flash mechanism that allows Flash scripts to call Javascript on
    // the embedding page).  This is specified via the "onload" parameter to
    // the Flash object loader in the <object> and <embed> tags that embed the
    // TADS.swf object, so be sure to specify the "onload" in those tags.
    loaded: new AbsEvent(),
    
    // cached font list
    fontList: null,

    // Wait for the font list to load, then proceed with the callback.
    // If the font list is already loaded, this simply invokes the callback
    // immediately.  If the font list isn't loaded, we'll populate it.
    //
    // The reason that we have to structure this as a wait-for routine
    // with a callback is that we rely on a Flash object to enumerate
    // installed fonts, and Flash objects can load asynchronously.  We
    // can't just assume that our Flash object is ready; we have to await
    // a go-ahead signal that it generates when it finished loading.
    // The wait-for structure lets us ensure that the Flash object is
    // ready before we try to invoke it.
    waitForFontList: function(doneFunc)
    {
        // First, we have to make sure we've loaded the Flash object.
        // Flash objects can be loaded asynchronously, so we have to
        // await the ready event from Flash before we can proceed.
        TADS_swf.loaded.whenDone(function() {

            // if we haven't already done so, build the font list
            if (!TADS_swf.fontList)
            {
                // Build the candidate font list.  First try the TADS.swf
                // embedded Flash object.  Flash isn't everywhere (especially
                // on mobile devices), so if we don't find it, fall back on
                // our canned list of candidate fonts.
                var fonts;
                try
                {
                    // get the flash object
                    var ie = navigator.userAgent.indexOf("Microsoft") != -1;
                    var mobj = (ie ? window["__TADS_swf"] :
                                document["__TADS_swf"]);

                    // ask the Flash object for the list of installed fonts
                    var sfonts = mobj.getFonts();

                    // Filter the list to keep only the first font of
                    // each family.  Style variations (bold, italic, etc)
                    // are represented internally on most systems as whole
                    // separate font objects, but for UI purposes we only
                    // want to show one list entry per font family.
                    // Keep only the font names in the final list.
                    fonts = [];
                    for (var i = 0 ; i < sfonts.length ; ++i)
                    {
                        // if this is a new name, add it to the list
                        if (sfonts[i].fontStyle == "regular")
                            fonts.push(sfonts[i].fontName);
                    }
                }
                catch (e)
                {
                    // An error occurred; we'll assume that the problem is
                    // that Flash isn't available.  Default to a canned list
                    // of common fonts.  We'll check each entry in a moment
                    // to see if it's actually installed, so the final list
                    // will only include fonts that are locally available.
                    // Of course, it won't necessarily include *all* of the
                    // available fonts, since we obviously can't include all
                    // possible fonts in our prefab list.  This list is only
                    // meant to capture the basic set of fonts that most
                    // operating systems include in their base installs.
                    // Note that this list is actually a union of common
                    // fonts from various operating systems, so it's unlikely
                    // that all of them will be installed on any one machine.
                    fonts = [
                        "Arial",
                        "Arial Black",
                        "Book Antiqua",
                        "Charcoal",
                        "Comic Sans MS",
                        "Courier",
                        "Courier New",
                        "Gadget",
                        "Geneva",
                        "Georgia",
                        "Helvetica",
                        "Impact",
                        "Lucida Console",
                        "Lucida Grande",
                        "Lucida Sans Unicode",
                        "Monaco",
                        "MS Sans Serif",
                        "MS Serif",
                        "New York",
                        "Palatino",
                        "Palatino Linotype",
                        "Symbol",
                        "Tahoma",
                        "Times",
                        "Times New Roman",
                        "Trebuchet MS",
                        "Verdana",
                        "Webdings",
                        "Wingdings",
                        "Zapf Chancery",
                        "ZapfChancery",
                        "Zapf Dingbats"
                    ];
                }

                // sort the font list by name (ignoring case)
                fonts.sort(function(a, b) {
                    return a.toLowerCase().localeCompare(b.toLowerCase());
                });

                // save the list
                TADS_swf.fontList = fonts;
            }

            // the font list is now ready - fire the callback
            doneFunc();
        });
    }
};

/* ------------------------------------------------------------------------ */
/*
 *   Preferences dialog 
 */
function showPrefsDialog()
{
    showDialog({
        id: 'prefs',
        title: "Customize Display",
        contents: $("prefsDialog"),
        buttons: [
            { name: "OK", isDefault: true, save: true },
            { name: "Cancel", isCancel: true }
        ],
        init: initPrefsDialog,
        dismiss: onClosePrefsDialog
    });
}

// List of preference profiles.  This is a table of preference objects,
// keyed by profile ID.
var prefProfiles = { };

// Currently selected profile, by ID.  Start with a default profile
// that doesn't exist, which will trigger automatic profile creation
// when needed.
var curPrefProfile = 0;

// Next profile ID.  This is a high water mark (plus one) of all of the
// profiles we've seen since loading; when we create a new profile, we'll
// assign it this ID and increment this value.
var nextPrefProfile = 1;


// Mapping between dialog elements and preference settings.  Each key is
// the ID of a dialog element; the corresponding value explains how to
// map between the dialog element and the style setting.
//
// type: specifies the control type:
//
//    "color" specifies a color picker control.  The value is in the
//    picker's style.backgroundColor property.
//
//    "font" specifies a font <select> list.
//
//    The default is an input control.  For text inputs and selects,
//    the value is in the element's "value" property; for checkboxes,
//    it's in the "checked" property.
//
// checkbox: a list ["off", "on"] giving the style values that correspond
// to the checkbox states.  Applies only to checkbox controls.
//
// selector: the style selector where the setting is stored.
// We ignore case, but otherwise the selector must match the literal
// text in the CSS source.  We don't attempt to parse the selectors
// or do any partial matching - we simply look for the identical
// selector text in the style sheet rule list.
//
// Multiple selectors can be specified with commas.  We'll look for
// each listed selector separately and set each one to the same list
// of values.
//
// style: the CSS property name for this setting.  The style names
// are given in the Javascript naming convention - use 'fontFamily'
// instead of 'font-family', for example.

// parent: ID of the parent item for defaulting.  A null value in the
// preference set inherits the given parent value in the set.  If
// there's no parent, a null value inherits the corresponding value
// in the game's list of default preference values.
//
var prefsMapper = {
    mainFont: {
        selector: "body",
        style: "fontFamily",
        type: "font"
    },
    mainFontSize: {
        selector: "body",
        style: "fontSize",
        type: "fontsize"
    },
    mainColor: {
        selector: "body",
        style: "color",
        type: "color"
    },
    mainBkg: {
        selector: "body",
        style: "backgroundColor",
        type: "color"
    },

    mainLinkColor: {
        selector: "a:link",
        style: "color",
        type: "color"
    },
    mainLinkHover: {
        selector: "a:hover, a.plain:hover",
        style: "color",
        type: "color"
    },
    mainLinkActive: {
        selector: "a:active, a:focus, a.plain:active, a.plain:focus",
        style: "color",
        type: "color"
    },

    cmdFont: {
        selector: ".cmdline, .cmdlineOld",
        style: "fontFamily",
        type: "font",
        parent: "mainFont"
    },
    cmdFontSize: {
        selector: ".cmdline, .cmdlineOld",
        style: "fontSize",
        type: "fontsize",
        parent: "mainFontSize"
    },
    cmdColor: {
        selector: ".cmdline, .cmdlineOld",
        style: "color",
        type: "color",
        parent: "mainColor"
    },
    cmdBold: {
        selector: ".cmdline, .cmdlineOld",
        style: "fontWeight",
        checkbox: ["normal", "bold"]
    },
    cmdItalic: {
        selector: ".cmdline, .cmdlineOld",
        style: "fontStyle",
        checkbox: ["normal", "italic"]
    },

    statusFont: {
        selector: "body.statusline",
        style: "fontFamily",
        type: "font",
        parent: "mainFont"
    },
    statusFontSize: {
        selector: "body.statusline",
        style: "fontSize",
        type: "fontsize",
        parent: "mainFontSize"
    },
    statusColor: {
        selector: "body.statusline",
        style: "color",
        type: "color"
    },
    statusBkg: {
        selector: "body.statusline",
        style: "backgroundColor",
        type: "color"
    },

    statusLinkColor: {
        selector: "body.statusline a:link",
        style: "color",
        type: "color"
    },
    statusLinkHover: {
        selector: "body.statusline a:hover, body.statusline a.plain:hover",
        style: "color",
        type: "color"
    },
    statusLinkActive: {
        selector: "body.statusline a:active,"
            + "body.statusline a:focus,"
            + "body.statusline a.plain:active,"
            + "body.statusline a.plain:focus",
        style: "color",
        type: "color"
    },

    serifFont: {
        selector: ".serifFont",
        style: "fontFamily",
        type: "font"
    },
    serifFontSize: {
        selector: ".serifFont",
        style: "fontSize",
        type: "fontsize",
        parent: "mainFontSize"
    },

    sansFont: {
        selector: ".sansFont",
        style: "fontFamily",
        type: "font"
    },
    sansFontSize: {
        selector: ".sansFont",
        style: "fontSize",
        type: "fontsize",
        parent: "mainFontSize"
    },

    scriptFont: {
        selector: ".scriptFont",
        style: "fontFamily",
        type: "font"
    },
    scriptFontSize: {
        selector: ".scriptFont",
        style: "fontSize",
        type: "fontsize",
        parent: "mainFontSize"
    },

    ttFont: {
        selector: ".ttFont,tt",
        style: "fontFamily",
        type: "font"
    },
    ttFontSize: {
        selector: ".ttFont",
        style: "fontSize",
        type: "fontsize",
        parent: "mainFontSize"
    }
};

// create a default profile, if required
function initDefProfile()
{
    // if there's no current profile, create one
    if (!(curPrefProfile in prefProfiles))
    {
        /* set up an empty profile */
        var pro = { profileName: "Default" };
        prefsMapper.forEach(function(m, id) { pro[id] = null; });

        // add it to the table
        curPrefProfile = nextPrefProfile++;
        prefProfiles[curPrefProfile] = pro;
    }
}

// initialize the preferences dialog on open
function initPrefsDialog(dlg)
{
    // initialize the default profile, if necessary
    initDefProfile();

    // Make a copy of the current preference set.  The dialog operates
    // on its own copy, so that we don't modify the official copy until
    // we save the dialog settings.
    dlgProfiles = { };
    prefProfiles.forEach(function(pro, id) {
        dlgProfiles[id] = pro.copy(false);
    });

    // set the current profile
    dlgCurProfile = curPrefProfile;

    // get a list of the profile names, to sort for the <select> list
    var proNames = [];
    dlgProfiles.forEach(function(pro, id) {
        var name = pro.profileName || "Untitled";
        proNames.push({ id: id, name: name, sortName: name.toLowerCase() });
    });

    // sort it by name
    proNames.sort(function(a, b) {
        return a.sortName.localeCompare(b.sortName);
    });

    // populate the profile list with the profile names
    var sel = $(dlg, "curProfile");
    var idx = 0;
    proNames.forEach(function(pro) {
        var o = new Option(pro.name, pro.id);
        try { sel.add(o); } catch (e) { sel.add(o, null); }
    });

    // select the current item
    setSelectByValue(sel, dlgCurProfile);

    // set the preferences settings in the dialog
    prefsToDlg(dlg, dlgProfiles[dlgCurProfile], true);

    // set up onchange event handlers for all controls
    breadthSearch(dlg, function(ele) {

        // if this is a control with a value, set an onchange handler
        if (prefsMapper[ele.id])
            ele.onchange = function(ev) { onPrefItemChange(ev, dlg, ele); };

        // continue the traversal
        return false;
    });
}

// dialog copy of the profiles
var dlgProfiles = null;
var dlgCurProfile = null;

// translate profile descriptions from XML sent by the server to our
// in-memory settings list
function xmlToPrefs(prefs)
{
    // replace the profile table with the new one
    var newPrefs = { };

    // run through the list of <profile> children
    if (xmlHasChild(prefs, "profile"))
    {
        var pros = prefs.getElementsByTagName("profile");
        for (var i = 0 ; i < pros.length ; ++i)
        {
            // create a new empty profile, and add it to the master list
            // under the given ID
            var pro = pros[i];
            var tab = { };
            var pid = xmlChildText(pro, "id");
            newPrefs[pid] = tab;

            // if this is a new high-water mark, note it
            var pidn = parseInt(pid);
            if (pidn && pidn >= nextPrefProfile)
                nextPrefProfile = pidn + 1;
            
            // read the <item> elements
            var items = pro.getElementsByTagName("item");
            for (var j = 0 ; items && j < items.length ; ++j)
            {
                // retrieve the item ID and value
                var item = items[j];
                var id = xmlChildText(item, "id");
                var val = xmlChildText(item, "value");

                // if it's an empty string value, use null
                if (val == "")
                    val = null;

                // set the item
                tab[id] = val;
            }

            // if this entry doesn't have a name, give it a default
            if (!("profileName" in tab))
                tab.profileName = "Untitled";
        }
    }

    // set the current profile ID
    var newCur = null;
    if (xmlHasChild(prefs, "currentProfile"))
        newCur = xmlChildText(prefs, "currentProfile");

    // if we have any profiles, and the current profile is defined,
    // commit the new settings; otherwise ignore them
    if (newPrefs.propCount() > 0 && newCur && newCur in newPrefs)
    {
        // replace the memory profile set with the newly loaded set
        prefProfiles = newPrefs;
        curPrefProfile = newCur;

        // update the display
        prefsToCSS();
    }
}

// load a preference set into the live CSS
function prefsToCSS(prefs, win)
{
    // create a default profile, if necessary
    initDefProfile();

    // if the preference set wasn't specified, use the current setting
    if (!prefs)
        prefs = prefProfiles[curPrefProfile];

    // if no window was specified, apply to myself and all children
    if (!win)
        win = window;

    // Build a table of styles to set.  Each entry is keyed by the
    // selector, converted to all upper-case for matching; each value
    // is a list of [property, value] pairs for the selector.
    var tab = { };
    prefsMapper.forEach(function(m, id)
    {
        // get the selector
        var sels = m.selector.toUpperCase();

        // break up the list of selectors in case there are multiples
        sels.split(",").forEach(function(sel)
        {
            // get the table entry for the selector, or add a new one if
            // it's not already there
            sel = sel.trim();
            var e;
            if (sel in tab)
                e = tab[sel];
            else
                tab[sel] = e = [];

            // add this style and its computed value to the list
            e.push([m.style, getPrefVal(prefs, id)]);
        });
    });

    // apply the styles to this window and its children
    win.applyCSS(tab);

    // recalc the window layout
    calcLayout();
}

// load a preference set into the preferences dialog
function prefsToDlg(dlg, prefs, init)
{
    // visit all controls in the dialog
    breadthSearch(dlg, function(ele)
    {
        // if this control is in the mapper, set its value
        if (ele.id && prefsMapper[ele.id])
            prefToControl(ele, prefs, init);

        // continue the tree traversal 
        return false;
    });
}

// Set the display value of a control in the preferences dialog based
// on the current value
function prefToControl(ele, prefs, init)
{
    // get the element ID and preferences mapper for the item
    var id = ele.id, m = prefsMapper[id];

    // get the value from the current preference set or defaults
    var val = prefs[id];
    var defVal = getPrefVal(prefs, id);
    if (val == undefined)
        val = null;

    // note if it's a default, and if so, apply the default
    var isDef = (val == null);
    if (isDef)
        val = defVal;

    // set the element's value from 'prefs'
    switch (m.type)
    {
    case "color":
        // set the control display if it's changes or we're initialing
        if (init
            || ele.useDefaultColor != isDef
            || ele.style.backgroundColor != val)
            setColorButton(ele, val, isDef);
        break;

    case "font":
    case "fontsize":
        // set the value in the field
        if (isDef)
            val = "*" + val;

        // set it if it's changed or we're initializing
        if (init || ele.value != val)
            ele.value = val;
        break;

    default:
        if (m.checkbox)
        {
            // checkbox - check it if the 'on' value is active
            ele.checked = (val == m.checkbox[1]);
        }
        else if (ele.type == "select-one")
        {
            // select list - find the value in the option list
            var opts = ele.options;
            for (var i = 0 ; i < opts.length ; ++i)
            {
                // if this value matches, select it
                if (opts[i].value == val)
                {
                    ele.selectedIndex = i;
                    break;
                }
            }
        }
        else
        {
            // for anything else, just set the value directly
            if (isDef)
                val = "*" + val;

            // set it if we're initializing or the value changed
            if (init || ele.value != val)
                ele.value = val;
        }
        break;
    }
}

function setColorButton(ele, color, isDef)
{
    ele.useDefaultColor = isDef;
    if (color)
    {
        ele.style.backgroundColor = color;
        ele.style.color = contrastColor(color);
    }
    ele.innerHTML = (isDef ? "*" : "");
}

// Calculate a constrasting color.  For most colors we'll just calculate
// the RGB complement, but for middle grays that gives us roughly the
// same color, so we'll simply use black.
function contrastColor(color)
{
    // parse the color to an int
    color = parseColor(color);

    // calculate the inverse of each component
    var r = 255 - ((color >> 16) & 0xff);
    var g = 255 - ((color >> 8) & 0xff);
    var b = 255 - (color & 0xff);

    // if the color is near a middle gray, the RGB complement is just
    // another middle gray, which isn't very contrasting; black is a
    // safe choice in these cases
    if (r >= 0x70 && r <= 0x90
        && g >= 0x70 && g <= 0x90
        && b >= 0x70 && b <= 0x90)
        r = g = b = 0;

    // generate the result color in #rrggbb format
    return unparseCColor(r, g, b);
}

// save the preferences dialog's current settings into the in-memory
// preferences table
function dlgToPrefs(dlg, prefs)
{
    // Traverse the dialog DOM tree and save each control value.  Note
    // that we use the "search" function for the traversal, but we're not
    // really doing a search - we just want to traverse the tree for the
    // side effects.  We can coopt the search function for this purpose
    // simply by always returning false from the test callback: the
    // search will never find what it's looking for, so it'll traverse
    // the entire tree before giving up and returning null.
    breadthSearch(dlg, function(ele)
    {
        // if this item has a mapping, save its value
        if (prefsMapper[ele.id])
            controlToPref(ele, prefs);

        // return false so that we continue the "search"
        return false;
    });
}

// set the preferences array value for a given control in the dialog
function controlToPref(ele, prefs)
{
    // get the mapper
    var id = ele.id;
    var m = prefsMapper[ele.id];

    // figure the value based on the type
    var val;
    switch (m.type)
    {
    case "color":
        val = ele.useDefaultColor ? null : ele.style.backgroundColor;
        break;

    case "font":
    case "fontsize":
        // if it starts with "*", it's a default
        val = ele.value;
        if (val.charAt(0) == '*')
            val = null;
        break;
        
    default:
        if (m.checkbox)
            val = m.checkbox[ele.checked ? 1 : 0];
        else
            val = ele.value;
        break;
    }
    
    // save the value
    prefs[id] = val;
}

function getPrefVal(prefs, id)
{
    // first look at the given preference set, then at the defaults
    for (var i = 0 ; i < 2 ; ++i)
    {
        // get the current set
        var s = [prefs, defaultPrefs][i];

        // scan up the inheritance tree for this ID
        for (var cid = id ; cid ; )
        {
            // get the mapper entry
            var m = prefsMapper[cid];

            // if this set has a non-null value for this item, it's the result
            var val = s[cid];
            if (val != null && val != undefined)
                return val;

            // get the parent ID
            cid = m ? m.parent : null;
        }
    }

    // didn't find it
    return null;
}

function onPrefItemChange(ev, dlg, ele)
{
    // get the active profile
    var profile = dlgProfiles[dlgCurProfile];
        
    // save this control's value in the current preference set
    controlToPref(ele, profile);

    // refresh all controls to show new inherited defaults as needed
    prefsToDlg(dlg, profile, false);
}

function newPrefProfile()
{
    // Create a temporary name for the profile of the form "Profile N+1",
    // where N is either the maximum of any existing profiles of the form
    // "Profile n", otherwise simply the number of existing profiles.
    var n = null;
    dlgProfiles.forEach(function (pro) {
        if (pro.profileName.match(/^\s*profile\s+(\d+)\s*$/i))
        {
            var m = parseInt(RegExp.$1);
            if (n == null || m > n)
                n = m;
        }
    });

    // if we didn't find any matching profiles, use the number of profiles
    if (n == null)
        n = dlgProfiles.propCount();

    // generate the name
    var initName = "Profile " + (n+1);

    // note the preferences dialog object
    var prefsDlg = dialogStack.top().ele;

    // show the create/rename dialog
    createOrRenamePrefProfile("Create Profile", initName, function(newName)
    {
        // copy the current profile 
        var newPro = dlgProfiles[dlgCurProfile].copy(false);

        // set the name
        newPro.profileName = newName;

        // make sure the next profile ID is above any existing IDs
        dlgProfiles.forEach(function(id) {
            id = parseInt(id);
            if (nextPrefProfile <= id)
                nextPrefProfile = id + 1;
        });

        // generate a new profile ID and store it in the dialog table
        var newID = (nextPrefProfile++).toString();
        dlgProfiles[newID] = newPro;

        // add it to the profile selector control
        var sel = $(prefsDlg, "curProfile");
        addSelectOptionSort(sel, newName, newID);

        // switch to the new profile
        dlgCurProfile = newID;
        setSelectByValue(sel, newID);
    });
}

function renamePrefProfile()
{
    // note the preferences dialog object
    var prefsDlg = dialogStack.top().ele;

    // run the rename dialog
    createOrRenamePrefProfile(
        "Rename Profile", dlgProfiles[dlgCurProfile].profileName,
        function(newName) {
        if (newName != dlgCurProfile)
        {
            // update the name in the profile table
            var pro = dlgProfiles[dlgCurProfile];
            pro.profileName = newName;
            
            // update the <select> option in the dialog display
            var sel = $(prefsDlg, "curProfile");
            var opt = sel.options[sel.selectedIndex];
            opt.text = newName;
        }
    });
}

function createOrRenamePrefProfile(title, initName, setName)
{
    // show the dialog
    showDialog({
        id: 'profileName',
        title: title,
        contents: "Profile name:<br>"
            + "<input type=\"text\" size=\"50\" value=\""
            + initName.htmlify() + "\">",
        buttons: [{ name: "OK", isDefault: true, save: true },
                  { name: "Cancel", isCancel: true }],
        dimensions: { width: "70ex" },

        // on close, add the new profile if they clicked OK
        dismiss: function(dlg, btn) {
            if (btn && btn.save)
            {
                // get the new name and trim leading and trailing spaces
                var newName = $(dlg, "<INPUT>").value.trim();

                // make sure the name is non-empty and unique
                var err = null;
                if (newName.match(/^\s+$/))
                    err = "Please enter a name for the new profile.";
                else if (newName != initName
                         && dlgProfiles.propWhich(function(ele, prop) {
                    return prop.trim().toLowerCase() == newName.toLowerCase();
                }))
                    err = "There's already a profile with this name. "
                          + "Please choose a different name.";

                // if there's an error, show it and return failure
                if (err)
                {
                    showDialog({ title: "Profile Name", contents: err });
                    return false;
                }

                // the name is valid - tell the caller to save it
                setName(newName);
            }

            // success - close the dialog
            return true;
        }
    });
}

function delPrefProfile()
{
    // note the preferences dialog object
    var prefsDlg = dialogStack.top().ele;

    // don't allow deleting the last profile
    if (dlgProfiles.propCount() == 1)
    {
        $alert("This is the only profile. If you wish to delete this "
               + "profile, please create a replacement first.",
               "Error");
        return;
    }

    // run the delete confirmation dialog
    $confirm("Do you really want to delete the profile \""
             + dlgProfiles[dlgCurProfile].profileName + "\"?",
             function(del) {
        // proceed with the deletion if desired
        if (del)
        {
            // remove this profile from the <select> list
            var sel = $(prefsDlg, "curProfile");
            var idx = sel.selectedIndex;
            sel.remove(idx);
            
            // delete the profile from the dialog globals
            delete dlgProfiles[dlgCurProfile];
            
            // arbitrarily select the first remaining item
            dlgCurProfile = dlgProfiles.propWhich(
                function() { return true; });
            
            // select this in the selector
            setSelectByValue(sel, dlgCurProfile);
            
            // display its settings in the dialog
            prefsToDlg(prefsDlg, dlgProfiles[dlgCurProfile], true);
        }
    }, "Delete Profile");
}

function resetPrefProfile()
{
    // note the preferences dialog object
    var prefsDlg = dialogStack.top().ele;

    // confirm
    $confirm("This will discard all of your custom settings for this "
             + "profile and restore its to the story's default "
             + "settings.  Are you sure you want to do proceed?",
             function(reset) {
        if (reset)
        {
            // get the profile
            var pro = dlgProfiles[dlgCurProfile];

            // set all of the known preferences to null
            prefsMapper.forEach(function(m, id) { pro[id] = null; });
            
            // display the new settings in the dialog
            prefsToDlg(prefsDlg, dlgProfiles[dlgCurProfile], true);
        }
    }, "Reset Profile to Defaults");
}

function onClosePrefsDialog(dlg, btn)
{
    // if they clicked OK, save the current settings
    if (btn && btn.save)
    {
        // save the current dialog control settings
        dlgToPrefs(dlg, dlgProfiles[dlgCurProfile]);

        // save these as the active settings
        prefProfiles = dlgProfiles;
        curPrefProfile = dlgCurProfile;

        // update the browser's live style sheet rules
        prefsToCSS();
        
        // build the preference update body to send to the server
        var post = [];
        post.push("current-profile=" + curPrefProfile);
        prefProfiles.forEach(function(pro, proID)
        {
            pro.forEach(function(val, id) {
                post.push(proID + "." + id + "=" + (val ? val : ""));
            });
        });

        // combine it into a CRLF-delimited string
        post = post.join("\r\n");

        // send the updates to the server
        var f = function(req, resp) {
            if (xmlHasChild(resp, "error"))
                logError("An error occurred saving your preference changes.",
                         xmlChildText("error"), null);
        };
        serverRequest("/webui/setPrefs", ServerRequest.Command,
                      f, post, "text/plain");
    }

    // allow the dialog to close
    return true;
}

function onChangeProfile(ev, sel)
{
    // get the active profile
    dlgCurProfile = sel.value;

    // display its settings in the dialog
    prefsToDlg(dialogStack.top().ele, dlgProfiles[dlgCurProfile], true);
}

/* ------------------------------------------------------------------------ */
/*
 *   File dialog 
 */

function showFileDialog(desc)
{
    // get the file dialog description from the xml
    var prompt = xmlChildText(desc, "prompt");
    var dlgType = xmlChildText(desc, "dialogType");
    var fileType = xmlChildText(desc, "fileType");
    var url = xmlChildText(desc, "url");

    // show the dialog
    showDialog({
        id: "inputFile",
        title: prompt,
        contents: $("fileDialog"),
        init: initFileDialog,
        fillWidth: true,
        url: url,
        buttons: [],
        dismiss: function(dlg, btn) {
            // notify the server that the dialog has been closed
            serverRequest("/webui/inputFileDismissed");
            return true;
        }
    });
}

function initFileDialog(dlg, params)
{
    var fr = $(dlg, "<IFRAME>");
    var frWin = fr.contentWindow || fr.contentDocument.window;
    frWin.location = params.url;
}

/* ------------------------------------------------------------------------ */
/*
 *   Input dialog.  This displays a generic message-and-buttons dialog per
 *   the TADS inputDialog() API.  
 */
function showInputDialog(desc)
{
    // get the dialog parameters
    var iconID = parseInt(xmlChildText(desc, "icon"));
    var prompt = xmlChildText(desc, "prompt").htmlify().replace(/\n/g, '<br>');
    var title = xmlChildText(desc, "title").htmlify();
    var defbtn = parseInt(xmlChildText(desc, "defaultButton"));
    var cxlbtn = parseInt(xmlChildText(desc, "cancelButton"));
    var buttons = xpath(desc, "buttons/button[*]#text").map(function(name, i) {
        var key = (name.match(/&(.)/) ? RegExp.$1 : null);
        name = name.htmlify().replace(/&amp;(.)/, "<B>$1</B>");
        var idx = i + 1;
        return {
            name: name,
            key: key,
            isDefault: idx == defbtn,
            isCancel: idx == cxlbtn,
            buttonIndex: idx
        };
    });

    // select the icon image for the given icon ID
    var icons = [null, "dlgIconWarning", "dlgIconInfo",
                 "dlgIconQuestion", "dlgIconError"];
    var icon = "";
    if (iconID >= 0 && iconID < icons.length)
        icon = icons[iconID];

    // if we found an image, wrap it in an <IMG> tag
    var img = (icon
               ? "<img src=\"/webuires/" + icon
               + ".gif\" class=\"inputDialogIcon\">"
               : "");

    // build the layout table
    var cont = "<table class=\"inputDialogTab\">"
               + "<tr>"
               +   "<td>" + img + "</td>"
               +   "<td class=\"inputDialogPrompt\">" + prompt + "</td>"
               + "</tr>"
               + "</table>";

    // show the dialog
    showDialog({
        id: "inputDialog",
        title: title,
        contents: "<div class=\"inputDialog\">" + cont + "</div>",
        buttons: buttons,
        dismiss: function(dlg, btn) {
            var idx = btn.buttonIndex || cxlbtn;
            serverRequest("/webui/inputDialog?button=" + idx);
            return true;
        },
        fillWidth: true
    });
}

/* ------------------------------------------------------------------------ */
/*
 *   Upload file dialog.  This allows the game server to solicit a file
 *   upload from the client; we pass along the request to the user through a
 *   dialog containing an <INPUT TYPE=FILE> field, which allows the user to
 *   select a local file to upload to the server.
 *   
 *   TADS uses file uploads when running in client/server mode without a
 *   network storage server.  In this configuration, the game server doesn't
 *   have anywhere else to put saved games and other files, so it uses
 *   uploads and downloads to the client PC.  This dialog handles the upload
 *   side.  
 */
function showUploadDialog(desc)
{
    // get the dialog description
    var prompt = xmlChildText(desc, "prompt");

    // initialization - fill in the iframe
    var init = function(dlg, params)
    {
        var doc = getIFrameDoc($(dlg, "<IFRAME>"));
        doc.open("text/html");
        doc.write(
            "<html><head><title>File Upload</title>"
            + "<link rel=\"stylesheet\" href=\"/webuires/tads.css\">"
            + "<link rel=\"stylesheet\" href=\"/webuires/main.css\">"
            + "</head>"
            + "<body>"
            + "<div class=\"dialogWrapper\"><div class=\"dialogContent\">"
            + "<form method=\"post\" name=\"uploadDialog\" "
            +   "enctype=\"multipart/form-data\" "
            +   "action=\"/webui/uploadFileDialog\" "
            +   "style=\"padding-top: 2em;\">"
            + "Please select a file:"
            + "<div style='margin: 1em;'>"
            + "<input type=\"file\" size=\"50\" name=\"file\">"
            + "</div>"
            + "</form>"
            + "</div></div>"
            + "</body>"
            + "</html>");
        doc.close();
    };

    // submit button callback
    var submit = function()
    {
        var doc = getIFrameDoc($(dialogStack.top().ele, "<IFRAME>"));
        var form = $(doc.body, "<FORM>");
        form.submit();
    };

    // show the dialog
    showDialog({
        id: "uploadFile",
        init: init,
        fillWidth: true,
        title: prompt,
        contents: $("uploadDialog"),
        buttons: [
            { name: "OK", isDefault: true, onclick: submit },
            { name: "Cancel", isCancel: true }
        ],
        dismiss: function(dlg, btn) {
            serverRequest("/webui/inputFileDismissed");
            return true;
        }
    });
}

/* ------------------------------------------------------------------------ */
/*
 *   Offer a file for download.  When the server wants to offer a file for
 *   download to the client PC, it sends a request with the server file path
 *   information, and we in turn offer the file to the user by creating an
 *   IFRAME with its source set to the downloadable file.  This will trigger
 *   the browser to offer a file download dialog, giving the user a chance to
 *   decide whether to accept the download and specify where to save it
 *   locally, or to reject the download by canceling the download dilaog.
 *   
 *   This function handles the download portion of the upload/download scheme
 *   described in the notes about showUploadDialog() above.  
 */
function addDownloadFrame(fname)
{
    // remove any old frame with the same name
    removeDownloadFrame(fname);

    // create the IFRAME
    var fr = document.createElement("IFRAME");
    fr.className = "downloadFile";

    // set its source to point to the file
    fr.src = fname;

    // add it to the document
    document.body.appendChild(fr);
}

/*
 *   Remove a downloadable file previously offered.  The server removes files
 *   after we successfully download them, or after a timeout elapses.  
 */
function removeDownloadFrame(fname)
{
    // find the iframe for the download
    for (var ele = document.body.firstChild ; ele ; ele = nxt)
    {
        // remember the next sibling, in case we unlink this one
        var nxt = ele.nextSibling;
        
        // if this is an iframe with the given file as the source, remove it
        if (ele.nodeName == "IFRAME" && ele.src == fname)
            document.body.removeChild(ele);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   MenuSys menu display - this is the client side implementation of the
 *   menusys.t module on the server.  This displays a dialog box with the
 *   given list of menu items.  Each menu item is a link that sends a command
 *   to the server to display its submenu or topic.  
 */
var menuSysLinkCount = 0, menuSysTopicCount = 0, menuSysLastTopic = 0;
function showMenuSys(r)
{
    // get the menu title
    var title = xmlChildText(r, "title");

    // no buttons yet
    var buttons = [];

    // start out with the menu wrapper div
    var s = "<div class=\"menuSys\">";

    // presume no specific initial focus
    var initFocus = null;

    // set up functions for common commands
    var prev = function() { serverRequest("/webui/menusys?prev"); };
    var up = function() { menuSysArrow(-1); };
    var down = function() { menuSysArrow(1); };
    var sel = null;

    // set up the dialog contents
    if (xmlHasChild(r, "close"))
    {
        // close the menu UI
        dismissDialogById("menuSys");
        return;
    }
    else if (xmlHasChild(r, "menuItem"))
    {
        // this is a list of submenu items - display the menu list
        var contents = xmlChild(r, "menuItem");
        for (var i = 0 ; i < contents.length ; ++i)
        {
            var item = xmlNodeText(contents[i]);
            var idx = i + 1;
            s += "<div class=\"menuSysItem\">"
                 + "<a href=\"#\" id=\"menuSysLink" + idx
                 + "\" onclick=\"javascript:menuSysSel(" + idx
                 + ");return false;\">"
                 + item + "</a></div>";
        }

        // 'select' selects the item with focus, if any
        sel = function() {
            var ae = document.activeElement;
            if (ae && ae.id.search(/^menuSysLink(\d+)$/) == 0)
                menuSysSel(parseInt(RegExp.$1));
        };

        // if we navigated from a child item, set the initial focus to
        // the child item
        if (xmlHasChild(r, "fromChild"))
            initFocus = "menuSysLink" + parseInt(xmlChildText(r, "fromChild"));

        // note the number of links
        menuSysLinkCount = contents.length;

        // add up/down arrow handlers
        buttons.push({ onclick: up, key: "Up" });
        buttons.push({ onclick: up, key: "U+0050" }); // "P"
        buttons.push({ onclick: down, key: "Down" });
        buttons.push({ onclick: down, key: "U+004E" }); // "N"
    }
    else if (xmlHasChild(r, "topicItem"))
    {
        // This is a topic menu - this type of menu is usually used for
        // hint lists.  We don't immediately display the whole list, but
        // instead just display up to the current "last displayed" item.
        // We then display more items on demands as the user requests
        // them.
        s += "<div id=\"menuSysTopicList\">";
        var contents = xmlChild(r, "topicItem");
        menuSysTopicCount = parseInt(xmlChildText(r, "numItems"));
        menuSysLastTopic = contents.length;
        for (var i = 0 ; i < contents.length ; ++i)
        {
            var idx = i + 1;
            var item = xmlNodeText(contents[i]);
            s += "<div class=\"menuSysItem\" id=\"menuSysTopic" + idx + "\">"
                 + item
                 + " [" + idx + "/" + menuSysTopicCount + "]"
                 + "</div>";
        }

        // end the topic list div
        s += "</div>";

        // start the footer with the Next button and "The End" indicator
        s += "<div class=\"menuSysTopicFooter\">";

        // if there's more to come, add the "next" item
        if (menuSysLastTopic < menuSysTopicCount)
            s += "<span id=\"menuSysNextButton\">(Press Space or "
                 + "<a href=\"#\" onclick=\"javascript:menuSysNextTopic();"
                 + "return false;\">click here</a> to see the next item)"
                 + "</span>";

        // add the "The End" indicator
        s += "<span id=\"menuSysTheEnd\""
             + (menuSysLastTopic < menuSysTopicCount
                ? " style=\"display:none;\"" : "")
             + ">[The End]</span>";

        // 'select' goes to the next topic
        sel = menuSysNextTopic;

        // add handlers to show the next topic on space, enter, or arrow down
        buttons.push({ onclick: menuSysNextTopic, key: "U+0020" });
        buttons.push({ onclick: menuSysNextTopic, key: "Enter" });
        buttons.push({ onclick: menuSysNextTopic, key: "Down" });
    }
    else if (xmlHasChild(r, "longTopic"))
    {
        // long topic item - limit the height
        var winrc = getWindowRect();
        var maxht = winrc.height * 0.75;

        // add the long topic contents
        s += "<div class=\"menuSysLongTopic\" style=\"max-height:"
             + maxht + "px;\">"
             + xmlChildText(r, "longTopic")
             + "</div>";

        // add the next and previous chapter buttons, if present
        if (xmlHasChild(r, "nextChapter") || xmlHasChild(r, "prevChapter"))
        {
            s += "<div class=\"menuSysChapterButtons\">";

            if (xmlHasChild(r, "prevChapter"))
            {
                s += "<a href=\"#\" onclick=\"javascript:menuSysGoChapter("
                     + "'prev');return false;\">&lt; Previous</a>";

                up = function() { menuSysGoChapter('prev'); };
                buttons.push({ onclick: up, key: "Left" });
                buttons.push({ onclick: up, key: "U+0008" });
            }

            if (xmlHasChild(r, "nextChapter"))
            {
                s += "<a href=\"#\" onclick=\"javascript:menuSysGoChapter("
                     + "'next');return false;\">Next: "
                     + xmlChildText(r, "nextChapter")
                     + " &gt;</a>";
                
                sel = down = function() { menuSysGoChapter('next'); };
                buttons.push({ onclick: down, key: "Right" });
                buttons.push({ onclick: down, key: "U+0020" });
            }

            s += "</div>";
        }
    }

    // close the division wrapper
    s += "</div>";

    // add a Previous button if this isn't the top-level menu
    if (!xmlHasChild(r, "isTop"))
    {
        // add the Return button
        buttons.push({ name: "Return", onclick: prev });

        // add backspace and left arrow keys as well; add these at the
        // end of the list, so that any key definitions we've already
        // added will override these
        buttons.push({ onclick: prev, key: "U+0008" });
        buttons.push({ onclick: prev, key: "Left" });
    }

    // add the Close button for all dialog types
    buttons.push({ name: "Close", isCancel: true });

    // add buttons for key list items
    var keylists = xmlChild(r, "keylist");
    if (keylists)
    {
        // the key list entries represent these commands, in order:
        // quit, previous, up, down, select
        var keyfuncs = [
            { isCancel: true, onclick: null },
            { isCancel: false, onclick: prev },
            { isCancel: false, onclick: up },
            { isCancel: false, onclick: down },
            { isCancel: false, onclick: sel }
        ];

        // run through the key list
        for (var i = 0 ; i < keylists.length ; ++i)
        {
            var kf = keyfuncs[i];
            var kl = xmlChild(keylists[i], "key");
            for (var j = 0 ; j < kl.length ; ++j)
            {
                // get the key name
                var k = xmlNodeText(kl[j]);

                // if it's in square brackets, it's a special key name;
                // otherwise it's a regular character
                if (k.search(/^\[(.*)\]$/) == 0)
                {
                    // special - look up the DOM equivalent
                    k = DOM3toTADSKey.propWhich(
                        function(val) { return val == k; });
                }
                else
                {
                    // ordinary character
                    k = k.toUpperCase();
                }

                // if we got the key, set up a button for it; insert it at
                // the start of the list so that it overrides any existing
                // entry with the same key
                if (k && (kf.onclick || kf.isCancel))
                {
                    buttons.unshift(
                        { onclick: kf.onclick,
                          isCancel: kf.isCancel, 
                          key: k });
                }
            }
        }
    }

    // If there's already a menu system dialog showing, remove it.  (Use a
    // pseudo-button with the 'isNewDlg' property set, so that our dismiss
    // handler knows that we're replacing the dialog rather than closing it.
    // This tells us not to send a 'close' command to the server to dismiss
    // the entire menu interaction.)
    dismissDialogById("menuSys", { isCancel: true, isNewDlg: true });

    // run the new dialog
    showDialog({
        id: "menuSys",
        fullWidth: true,
        title: title,
        initFocus: initFocus,
        contents: s,
        buttons: buttons,
        dimensions: { width: "90%" },
        dismiss: function(dlg, btn) {
            if (!btn.isNewDlg)
                serverRequest("/webui/menusys?close");
            return true;
        }
    });
}

function menuSysGoChapter(dir)
{
    serverRequest("/webui/menusys?chapter=" + dir);
}

/*
 *   Handle an arrow in the menu system.  This moves the focus up or down in
 *   the menu list.  
 */
function menuSysArrow(dir)
{
    // find the item with focus
    var ele = document.activeElement;

    // if it's not one of our items, move to the first or last item
    if (ele == null || !ele.id || ele.id.search(/^menuSysLink(\d+)$/) < 0)
    {
        // select the first element on Down, last element on Up
        ele = (dir > 0 ? 1 : menuSysLinkCount);
    }
    else
    {
        // we have a focus item - move to the next item
        ele = parseInt(RegExp.$1) + dir;
    }

    // make sure it's in range
    if (ele < 1)
        ele = menuSysLinkCount;
    else if (ele > menuSysLinkCount)
        ele = 1;

    // select the item
    ele = $("menuSysLink" + ele);
    if (ele)
        ele.focus();
}

/*
 *   Display the next topic in a topic/hint list 
 */
function menuSysNextTopic()
{
    // if we've displayed all items, we're done
    if (menuSysLastTopic >= menuSysTopicCount)
        return;

    // ask for the next item
    serverRequest("/webui/menusys?nextTopic", ServerRequest.Command,
                  function(req, resp)
    {
        // get the topic text from the reply
        var txt = xmlChildText(resp, "topicItem");
        if (txt)
        {
            // count the additional display item
            menuSysLastTopic++;

            // set up the topic item
            var ele = document.createElement("DIV");
            ele.className = "menuSysItem";
            ele.id = "menuSysTopic" + menuSysLastTopic;
            ele.innerHTML = txt
                            + " [" + menuSysLastTopic
                            + "/" + menuSysTopicCount + "]";

            // add it to the topic list
            $("menuSysTopicList").appendChild(ele);

            // if this is the last item, hide the "next" button and show
            // the "the end" indicator
            if (menuSysLastTopic >= menuSysTopicCount)
            {
                $("menuSysNextButton").style.display = "none";
                $("menuSysTheEnd").style.display = "inline";
            }
        }
    });
}

/* 
 *   Navigate into a given menu item.  This sends a command to the server
 *   telling it to select the given menu.  The server will update its menu
 *   state and send us display instructions for the new menu.  
 */
function menuSysSel(idx)
{
    serverRequest("/webui/menusys?select=" + idx);
}

/* ------------------------------------------------------------------------ */
/*
 *   Show the page menu.  This is typically activated by a button in the
 *   status line.  
 */
function showPageMenu(ev, arrow)
{
    // get the real event
    ev = getEvent(ev);

    // get the menu object
    var menu = $("pageMenuDiv");

    // make sure any past incarnation is closed
    closePopup(menu);

    // create the shadow div
    var sh = document.createElement("DIV");
    sh.className = "menuShadow";
    document.body.appendChild(sh);
    var closer = function() {
        document.body.removeChild(sh);
    };

    // open the menu
    menu.style.display = "block";
    openPopup(ev, menu, closer, null, arrow);

    // move it just under the arrow that opened it
    var arc = getObjectRect(arrow);
    var mrc = getObjectRect(menu);
    mrc.x = arc.x + arc.width - mrc.width;
    mrc.y = arc.y + arc.height;
    moveObject(menu, mrc.x, mrc.y);

    // set up the shadow
    sh.style.width = mrc.width + "px";
    sh.style.height = mrc.height + "px";
    moveObject(sh, mrc.x + 3, mrc.y + 3);
}

function pageMenuClick(item)
{
    closePopup($("pageMenuDiv"));
    if (item == 'prefs')
    {
        showPrefsDialog();
    }
    else if (item == 'errlog')
    {
        showErrorLog();
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Screen name setting 
 */
var screenName = "Host";
function setScreenName(name)
{
    serverRequest("/webui/setScreenName?name=" + encodeURIComponent(name));
    screenName = name;
}

