/* Copyright 2010 Michael Roberts */
/*
 *   Layout window scripts.
 */

// initialize the layout window
function layoutInit()
{
    // set up our event handlers
    window.onresize = calcLayout;
    window.onGameEvent = layoutGameEvent;
    window.onGameState = layoutGameState;

    // set up some window methods
    window.windowFromPath = windowFromPath;
}

// server-to-client game event handler for a layout window
function layoutGameEvent(req, resp)
{
    if (xmlHasChild(resp, "subwin"))
    {
        // subwindow creation - go create the window
        createLayoutFramesXML(xmlChild(resp, "subwin"));
    }

    if (xmlHasChild(resp, "shutdown"))
    {
        // propagate shutdown notifications to subwindows
        layoutWindows.forEach(function(w) {
            if (w.onGameEvent)
                w.onGameEvent(req, resp);
        });
    }
}

// server-to-client game state event handler for a layout window
function layoutGameState(req, resp)
{
    // create subwindows
    if (xmlHasChild(resp, "subwin"))
    {
        // create each subwindow in the list
        createLayoutFramesXML(xmlChild(resp, "subwin"));
    }
}

// set a new size for an existing layout window
function setWinLayout(win, rc)
{
    // find the window
    var l = layoutWinFromWindow(win);

    // if we found it, set the new size information
    if (l)
    {
        // convert 'rc' from a comma list, if applicable
        if (typeof(rc) == "string")
            rc = rc.replace(/^ +| +$/g, "").replace(/, +| +,/g, ",")
                 .split(",");

        // replace the old rectangle with non-null new elements
        for (var i = 0 ; i < rc.length && i < l.rc.length ; ++i)
        {
            // copy non-empty replacements
            if (rc[i] != "")
                l.rc[i] = rc[i];
        }
    }
}

// Window tracker list.  We keep an object here representing each IFRAME
// window we're managing.
var layoutWindows = { };

// create layout frames from a list of XML <subwin> tags
function createLayoutFramesXML(subs)
{
    // run through each <subwin> tag
    for (var i = 0 ; i < subs.length ; ++i)
    {
        // get this child and decode its XML
        var sub = subs[i];
        var name = xmlChildText(sub, "name");
        var rc = xmlChildText(sub, "pos");
        var src = xmlChildText(sub, "src");

        // turn the size list into an array
        rc = rc.replace(/^ +| +$/g, "").replace(/, +| +,/g, ",").split(",");
        
        // create the IFRAME for this window
        createLayoutFrame(name, rc, "", src);
    }
    
    // recalculate the layout for the new windows
    calcLayout();
}

// create a new iframe layout window
function createLayoutFrame(name, rc, style, src)
{
    var obj;

    // check to see if the window already exists
    if (name in layoutWindows)
    {
        // we already have a window by this name - reuse it
        obj = layoutWindows[name];

        // update the window layout rectangle
        obj.rc = rc;
    }
    else
    {
        // create the IFRAME element
        var ele = document.createElement("iframe");
        ele.setAttribute("id", name);
        ele.setAttribute("name", name);
        ele.setAttribute("style", style);

        // this is for IE, to turn off the border (the capitalization counts)
        ele.setAttribute("frameBorder", "0");

        // add the new element to the document at the root level
        var body = document.body || document.documentElement;
        body.appendChild(ele);

        // set up functions to retrieve its document and window objects
        var getDoc = function() {
            var f = this.ele;
            return f.contentWindow ? f.contentWindow.document
                                   : f.contentDocument;
        };
        var getWin = function() {
            var f = this.ele;
            return f.contentWindow || f.contentDocument.window;
        };

        // set up the tracker object
        obj = {
            name: name, rc: rc, ele: ele, getDoc: getDoc, getWin: getWin
        };

        // save it in the window list
        layoutWindows[name] = obj;

        // navigate to the location
        if (src)
            obj.getWin().location = src;

    }

    // recalculate the window layout for the new addition
    calcLayout();

    // return the new tracker
    return obj;
}

// get a window tracker object from the IFRAME element
function layoutWinFromEle(ele)
{
    return layoutWindows.valWhich(function(w) { return w.ele == ele; });
}

// get the layout object for a given window
function layoutWinFromWindow(win)
{
    // scan our list of windows for a match to the target window
    return layoutWindows.valWhich(
        function(w) { return w.getWin() == win; });
}

// Get the window for a relative path.  The first path element is
// interpreted as a child of the current window.
function windowFromPath(path)
{
    // if the argument is a string, split it up on '.' separators
    if (typeof(path) == "string")
        path = path.split(".");

    // look up the first path element in our child list
    var w;
    if (!(w = layoutWindows[path.shift()]))
        return null;

    // if that's the only path element, return the child
    if (path.length == 0)
        return w.getWin();

    // look up the rest of the path via the child
    return w.getWin().windowFromPath(path);
}

// get the name for a child window
function pathFromWindow(win)
{
    // find the layout window object for the window
    var l = layoutWinFromWindow(win);

    // if we found it, build the path from our page name plus the window name
    if (l)
        return pageName + "." + l.name;
    else
        return null;
}

// recalculate the layout
function calcLayout()
{
    // clear the cache of content rectangles
    layoutWindows.forEach(function(w) { w.rcContent = null; });

    // run through our list of tracker objects
    layoutWindows.forEach(function(w)
    {
        // calculate each coordinate for this window
        var rc = [];
        for (var j = 0 ; j < 4 ; ++j)
            rc[j] = calcLayoutCoord(w, j);

        // limit the coordinates to the window size
        var wrc = getWindowRect();
        rc[0] = (rc[0] < 0 ? 0 : rc[0] > wrc.width ? wrc.width : rc[0]);
        rc[1] = (rc[1] < 0 ? 0 : rc[1] > wrc.height ? wrc.height : rc[1]);
        rc[2] = (rc[2] < 0 ? 0 :
                 rc[0] + rc[2] > wrc.width ? wrc.width - rc[0] : rc[2]);
        rc[3] = (rc[3] < 0 ? 0 :
                 rc[1] + rc[3] > wrc.height ? wrc.height - rc[1] : rc[3]);

        // set the new layout coordinates in the IFRAME's style properties
        var ele = w.ele;
        try
        {
            ele.style.left = rc[0] + "px";
            ele.style.top = rc[1] + "px";
            ele.style.width = rc[2] + "px";
            ele.style.height = rc[3] + "px";
        }
        catch (exc)
        {
        }
    });
}

// Calculate one coordinate for an iframe window.  This resolves the
// current values of the constraint-based properties.
function calcLayoutCoord(win, i)
{
    // get the target element from the (left, top, width, height) list
    var val = win.rc[i];

    // first, turn typographical units into pixel sizes: em, en, ex
    val = val.replace(/([0-9.]+)(em|en|ex)/gi, function(m, val, unit) {

        // Create a probe item on the page to measure the em size.  This is
        // simply a DIV element with its height explicitly set to 1 unit of
        // whichever unit we're measuring.
        var d = win.getDoc().createElement("DIV");
        var b = win.getDoc().body;
        d.style.height = "1" + unit;
        b.appendChild(d);

        // measure the height of the division to get the pixels per unit,
        // then we're done with the probe element
        var pixels = d.offsetHeight;
        b.removeChild(d);

        // return the number times the pixes per per
        return parseFloat(val) * pixels;
    });

    // Next, apply percentages: these are relative to the window interior
    // dimensions.  Left/width percentages are relative to the window width,
    // and top/height percentages are relative to the height.
    val = val.replace(/([0-9]+)%/gi, function(m, val) {
        return Math.floor(parseInt(val)/100
                          * getWindowRect()[(i % 2) ? "height" : "width"]);
    });

    // Apply object.prop items
    val = val.replace(/([a-z_0-9]+)\.([a-z]+)/gi, function(m, obj, prop) {

        // check for special names
        switch(m)
        {
        case "window.width":
            return getWindowRect().width;

        case "window.height":
            return getWindowRect().height;

        case "content.width":
            // if we don't have a cached value, cache it now
            if (!win.rcContent)
                win.rcContent = getContentRect(win.getWin());

            // return the width from the cached content rectangle
            return win.rcContent.width;

        case "content.height":
            // if we don't have a cached value, cache it now
            if (!win.rcContent)
                win.rcContent = getContentRect(win.getWin());
            
            // return the height from the cached content rectangle
            return win.rcContent.height;
        }

        // if 'obj' is the name of one of our layout windows, get a property
        // of that window
        if (obj in layoutWindows)
        {
            // get the window object
            var w = layoutWindows[obj];

            // look up the property
            switch (prop)
            {
            case "left":
                return calcLayoutCoord(w, 0);

            case "top":
                return calcLayoutCoord(w, 1);

            case "width":
                return calcLayoutCoord(w, 2);

            case "height":
                return calcLayoutCoord(w, 3);

            case "right":
                return parseInt(calcLayoutCoord(w, 0))
                    + parseInt(calcLayouttCoord(w, 2));

            case "bottom":
                return parseInt(calcLayoutCoord(w, 1))
                    + parseInt(calcLayoutCoord(w, 3));
            }
        }

        // if we didn't find a translation, just return the original text
        return m;
    });

    // The whole thing should be expressed as an integer expression now,
    // with each integer being implicitly pixels (but with no explicit
    // units in the string).  Apply any arithmetic operators to the pixel
    // expression.
    if (typeof(val) == "string" && val.match(/^[- +*/0-9]+$/))
        val = eval(val);
    
    // turn it into an integer
    if (typeof(val) == "string")
        val = parseInt(val);
    
    // return the final value
    return val;
}

/*
 *   Set the default focus for a keyboard event to the appropriate child
 *   window.  If we have a default command-line window - which is identified
 *   as a window named "command" - we'll send the focus there.  
 */
function setDefaultFocusChild(desc, fromWin)
{
    // if we have a child named "command", set focus there
    var w = layoutWindows["command"];
    if (w && w.getWin().setDefaultFocusChild)
        w.getWin().setDefaultFocusChild(desc, fromWin);
}

/*
 *   Apply a set of CSS styles to this window and its children 
 */
var origApplyCSS = applyCSS;
applyCSS = function(tab)
{
    // apply the styles to this window
    origApplyCSS(tab);

    // apply the changes to each child
    layoutWindows.forEach(function(w) { w.getWin().applyCSS(tab); });
}
