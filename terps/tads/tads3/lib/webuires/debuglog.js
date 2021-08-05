/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
 *   Debug log window scripts for TADS 3 Web UI. 
 */

function debugLogInit()
{
    // initialize the utilities package
    utilInit();

    // tell our parent window that we're done loading
    if (window.opener)
        window.opener.debugLogLoaded.fire();
}

function debugLogPrint(msg)
{
    // add the message to the debug log division
    var d = $("debugLogDiv");
    var s = document.createElement("SPAN");
    s.innerHTML = msg;
    d.appendChild(s);

    // scroll the window to the bottom
    var ht = d.offsetHeight;
    var wrc = getWindowRect();
    var y = ht - wrc.height;
    if (y > 0)
        window.scrollTo(0, y);
}
