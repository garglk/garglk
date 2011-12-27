/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
 *   Status line scripts for TADS 3 Web UI.
 */

function statLineInit()
{
    // initialize the utility package
    utilInit();

    // ask our parent to recalculate the window layout now that we're here
    parent.calcLayout();

    // set up the default document-level key handler - this will send focus
    // back to the main command window if the user presses a key while
    // focus is in our window (and not in a key-consuming control)
    addKeyHandler(document, genericDocKeyFocus);

    // get the initial state
    getInitState();

    // load preference styles
    $win().prefsToCSS(null, window);
}

function onGameState(req, resp)
{
    // set the text from the status
    setStatusText(xmlChildText(resp, "text"));

    // recalculate the parent window layout, in case the layout is sensitive
    // to our content size
    parent.calcLayout();
}

function onGameEvent(req, resp)
{
    if (xmlHasChild(resp, "text"))
        setStatusText(xmlChildText(resp, "text"));
    if (xmlHasChild(resp, "appendText"))
        appendStatusText(xmlChildText(resp, "appendText"));
    if (xmlHasChild(resp, "resize"))
        parent.calcLayout();
    if (xmlHasChild(resp, "setsize"))
        parent.setWinLayout(this, xmlChildText(resp, "setsize"));
}

var statText = "";
function setStatusText(msg)
{
    msg = BrowserInfo.adjustText(msg);
    statText = msg;
    $("statusMain").innerHTML = msg;
}

function appendStatusText(msg)
{
    msg = BrowserInfo.adjustText(msg);
    statText += msg;
    $("statusMain").innerHTML = statText;
}

/* ------------------------------------------------------------------------ */
/*
 *   Network waiting indicator ("throbber").  We show this as part of the
 *   status line because the throbber is basically a status display, so (a)
 *   it's logical to group with the status line in the abstract, and (b) in
 *   concrete terms, the status line is usually a good visual location for
 *   this because it's prominent but out of the way of the input area of the
 *   window.  
 */
var NetThrobber =
{
    // number of open network requests
    netRequestCount: 0,

    // timeout for starting the visual indicator, if one is pending
    starter: null,

    // start a network request
    onStartRequest: function()
    {
        // Count the request; if it's the first one, set a timer to
        // start the throbber.  Note that we don't simply start it
        // immediately: if the request completes quickly, it's better
        // not to show the throbber at all, since the quick on-off
        // flash would be annoying.  We only need the throbber when
        // the request takes a noticeable amount of time to complete.
        // So, wait until a few moments before starting it; if the
        // request completes before the wait expires, we'll skip the
        // whole thing and avoid any flashing.  If the request turns
        // out to take longer than the timeout, it's a long enough wait
        // that we want visual feedback that something's going on.
        if (this.netRequestCount++ == 0 && !this.starter)
        {
            this.starter = setTimeout(function() { 
                NetThrobber.starter = null;
                NetThrobber.show(); 
            }, 500);
        }
    },

    // end a network request
    onEndRequest: function()
    {
        // Decrement the open request count.  If this was the last open
        // request, turn off the wait indicator.
        if (--this.netRequestCount == 0)
            this.hide();
    },

    // show the throbber
    show: function()
    {
        $("netThrobberDiv").style.display = "block";
        $win().status = "Waiting for the game to respond...";
    },

    // hide the throbber
    hide: function()
    {
        // if we have a timer waiting to show the throbber, simply kill
        // the timer; otherwise hide the display
        if (this.starter)
        {
            clearTimeout(this.starter);
            this.starter = null;
        }
        else
        {
            $("netThrobberDiv").style.display = "none";
            $win().status = "";
        }
    }
};    

