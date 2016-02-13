/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
 *   Utility package for TADS 3 Web UI.  This is a javascript file that's
 *   designed to be included from the main UI page of a Web-UI game.  This
 *   provides a set of useful functions for implementing an IF-style user
 *   interface in HTML/Javascript.
 */


/* ------------------------------------------------------------------------ */
/* 
 *   Miscellaneous globals
 */

// my page name, as I'm known to the server
var pageName = "";

// the generic noun we use to refer to this program in error messages
// (game, story, program, ...)
var selfNoun = "story";

// default action suggestion
var errorRecoveryAction =
    " If the " + selfNoun + " otherwise seems to be running "
    + "properly, you can ignore this error.  If the " + selfNoun
    + " isn't responding or is acting strangely, it might help to "
    + "<a href=\"#\" onclick=\"javascript:$win().location.reload();"
    + "return false;\">reload this page</a>.";

/* ------------------------------------------------------------------------ */
/*
 *   Define a property on an object.  If the browser supports the
 *   defineProperty method, we'll use that; otherwise we'll just set the
 *   property directly.
 *   
 *   This differs from direct property setting in that we mark the property
 *   as non-enumerable.  This is particularly useful for our Object.prototype
 *   extensions when jQuery is also being used.  jQuery uses Object property
 *   enumerations internally and incorrectly assumes that all results are
 *   browser-defined properties, so our custom extensions can create problems
 *   if jQuery is also used.  Explicitly hiding those from enumerations lets
 *   us define our own custom Object.prototype extensions without confusing
 *   jQuery.  (The webui library itself doesn't use jQuery, so this isn't an
 *   issue out of the box, but it is an issue for individual game authors who
 *   want to use jQuery with the webui code.)
 */
function defineProperty(target, name, method)
{
    if (Object.defineProperty && Object.defineProperties)
        Object.defineProperty(target, name,
        {
            'value': method,
            'configurable': true,
            'enumerable': false,
            'writable': true
        });
    else
        target[name] = method;
}

/* ------------------------------------------------------------------------ */
/*
 *   Initialize.  Call this once at page load to set up this package.
 */
function utilInit()
{
    // if we have a parent, ask the parent for my name
    if (window.parent && window.parent != window)
        pageName = window.parent.pathFromWindow(window);

    // detect the browser version
    BrowserInfo.init();

    // copy an object
    defineProperty(Object.prototype, "copy", function(deep) {

        // create a blank object with the same prototype as me
        var newObj = CopyProto.create(this);
        
        // copy each property
        for (var prop in this)
        {
            // include only directly defined properties
            if (this.hasOwnProperty(prop))
            {
                // get this value
                var val = this[prop];
                
                // recursively copy objects if this is a "deep" copy
                if (deep && typeof(val) == "object")
                    val = val.copy(true);
                
                // save this value
                newObj[prop] = val;
            }
        }

        // return the new object
        return newObj;
    });

    // get the number of enumerable keys
    defineProperty(Object.prototype, "propCount", function() {
        var cnt = 0;
        for (var prop in this)
        {
            if (this.hasOwnProperty(prop))
                ++cnt;
        }
        return cnt;
    });

    // enumerate keys set in an object, excluding prototype keys
    defineProperty(Object.prototype, "forEach", function(func) {
        for (var prop in this)
        {
            // call the callback only for props directly defined in 'this'
            if (this.hasOwnProperty(prop))
                func(this[prop], prop);
        }
    });

    // find the property for which a callback returns true
    defineProperty(Object.prototype, "propWhich", function(func) {
        for (var prop in this)
        {
            if (this.hasOwnProperty(prop) && func(this[prop], prop))
                return prop;
        }
        return null;
    });

    // find the property value for which a callback returns true
    defineProperty(Object.prototype, "valWhich", function(func) {
        for (var prop in this)
        {
            var val;
            if (this.hasOwnProperty(prop) && func(val = this[prop], prop))
                return val;
        }
        return null;
    });

    // htmlify a string - convert markup-significant characters to & entities
    String.prototype.htmlify = function() {
        return this.replace(/[<>&"']/g, function(m) {
            return { '<': "&lt;", '>': "&gt;", '&': "&amp;",
                     '"': "&#34;", '\'': "&#39;" }[m];
        });
    };

    // remove html quoting from a string
    String.prototype.unhtmlify = function() {
        return this.replace(/&(gt|lt|amp|quot|nbsp|#[0-9]+);/ig,
                            function(m, i) {
            var r = { 'gt': '>', 'lt': '<', 'amp': '&', 'quot': '"',
                      'nbsp': ' ' }[i.toLowerCase()];
            if (r)
                return r;
            else
                return String.fromCharCode(parseInt(i));
        });
    }

    // trim leading and trailing spaces from a string
    String.prototype.trim = function() {
        return this.replace(/^\s+|\s+$/g, "");
    };

    // convenience method: find an item in an array
    if (!Array.prototype.indexOf) {
        Array.prototype.indexOf = function(val) {
            for (var i = 0 ; i < this.length ; ++i)
            {
                if (val == this[i])
                    return i;
            }
            return -1;
        };
    }

    // convenience method: find the index for a value matching a callback
    if (!Array.prototype.indexWhich) {
        Array.prototype.indexWhich = function(test) {
            for (var i = 0 ; i < this.length ; ++i)
            {
                if (test(this[i], i))
                    return i;
            }
            return -1;
        };
    }

    // convenience method: find a value matching a callback
    if (!Array.prototype.valWhich) {
        Array.prototype.valWhich = function(test) {
            for (var i = 0 ; i < this.length ; ++i)
            {
                var v = this[i];
                if (test(v, i))
                    return v;
            }
            return -1;
        };
    }

    // convenience method: map items in an array
    if (!Array.prototype.map) {
        Array.prototype.map = function(mapping) {
            var ret = [];
            for (var i = 0 ; i < this.length ; ++i)
                ret.push(mapping(this[i], i));
            return ret;
        };
    }

    // convenience method: filter an array
    if (!Array.prototype.filter) {
        Array.prototype.filter = function(filterFunc) {
            var ret = [];
            for (var i = 0 ; i < this.length ; ++i)
            {
                var ele = this[i];
                if (filterFunc(ele, i))
                    ret.push(ele);
            }
            return ret;
        };
    }

    // convenience method: get the top element of a stack-like array
    Array.prototype.top = function() {
        if (this.length)
            return this[this.length - 1];
        else
            return null;
    };

    // override our copy method for arrays
    Array.prototype.copy = function(deep) {

        // create the new array
        var newArray = [];

        // copy each element
        for (var i = 0 ; i < this.length ; ++i)
        {
            // get this value
            var val = this[i];

            // make a deep copy of object values, if desired
            if (deep && typeof(val) == "object")
                val = val.copy(true);

            // set the new value
            newArray[i] = val;
        }

        // return the new array
        return newArray;
    };

    // override the for-each enumerator for arrays - just step through
    // the index values from 0 to length
    Array.prototype.forEach = function(func) {
        for (var i = 0, len = this.length ; i < len ; ++i)
            func(this[i], i);
    };

    // add a listener for mouse down events on the main document,
    // for closing spring-loaded pop-up objects
    addEventHandler(document, "mousedown", popupCloser);

    // set our custom javascript error handler
    window.onerror = function(msg, url, lineNum) {

        // generate a stack trace
        var s = getStackTrace();

        // the top element is the onerror handler we're currently in,
        // which isn't part of the error, so clip it out of the list
        s.shift();

        // create a human-readable trace listing
        s = s.map(function(ele) {
            return ele.desc.htmlify(); }).join("<br>");

        // log the error
        logError("An error occurred in the " + selfNoun
                 + "'s user interface programming. This is probably a "
                 + "bug that should be reported to the "
                 + selfNoun + "'s author."
                 + errorRecoveryAction,
                 "Javascript error: " + msg.htmlify()
                 + "<br>Window: " + pageName
                 + " (" + url.htmlify() + ")"
                 + "<br>Line: " + lineNum
                 + (s ? "<br>Call stack:<div class=\"errorSubDetail\">"
                    + s + "</div>": ""),
                 { failed: true, succeeded: false });
        
        return true;
    };

    // set up an unload handler
    addEventHandler(window, "beforeunload", unloadCleanup);
}

// window unload handler
function unloadCleanup()
{
    // cancel outstanding ajax requests
    cancelServerRequests();
}

/*
 *   Create an object with the same prototype as an existing object.  To
 *   invoke, call SameProto.create(obj). 
 */
function CopyProto() { }
CopyProto.create = function(obj)
{
    /* 
     *   This is a bit tricky, but here's how this works.  The ECMA spec says
     *   that a new object's prototype is set to its constructor's prototype
     *   *at the moment of creation*.  So step 1 is to prepare the
     *   constructor's prototype so that it matches the source object's; then
     *   step 2 is to instantiate the object with 'new'.  
     */
    CopyProto.prototype = obj.constructor.prototype;
    return new CopyProto();
}


/* 
 *   Get a stack trace.  This returns a list of function call descriptors.
 */
function getStackTrace()
{
    // start with an empty list
    var s = [];

    // walk up the call stack starting with our caller
    for (var c = arguments.callee.caller ; c ; c = c.caller)
    {
        // Make sure we haven't encountered this caller before.  
        // Javascript uses the function object to represent a call
        // level, which makes it impossible to walk a recursive
        // stack because each recursive call overwrites the caller
        // information in the function object.  JS of course has
        // the actual caller information internally, but it's
        // not exposed through the standard APIs.
        for (var i = 0 ; i < s.length ; ++i)
        {
            if (s[i].fn == c)
            {
                s.push({ desc: "(recursive)" });
                return s;
            }
        }

        // set up the level descriptor
        var ele = { fn: c, args: c.arguments };

        // get a formatted version of the argument list
        var a = [];
        if (c.arguments)
        {
            for (var i = 0 ; i < c.arguments.length ; ++i)
                a.push(valToDesc(c.arguments[i]));
        }

        // build the human-readable function call description
        ele.desc = funcToDesc(c) + "(" + a.join(", ") + ")";

        // add it to the list
        s.push(ele);
    }

    // return the trace
    return s;
}

function valToDesc(v)
{
    // convert to string
    switch (typeof(v))
    {
    case "function":
        return funcToDesc(v);
        break;
        
    case "string":
        // shorten it if it's long
        if (v.length > 60)
            v = v.substr(0, 30) + "..." + v.substr(v.length - 20);

        // escape special characters
        return "\""
               + v.replace(/["\\\n\r\v\f\t]/g, function(m) {
                     return { '"': '\\"', '\n': '\\n',
                              '\r': '\\r', '\\': '\\\\',
                              '\v': '\\v', '\f': '\\f', '\t': '\\t'
                 }[m]; })
               + "\"";

    case null:
        return "null";

    case "undefined":
        return "undefined";

    default:
        if (v == null)
        {
            return "null";
        }
        else if (v instanceof Array)
        {
            var a = [], amax = (v.length > 8 ? 5 : 8);
            for (var i = 0 ; i < v.length && i < amax ; ++i)
                a.push(valToDesc(v[i]));

            if (v.length > amax)
                a.push("... (" + (v.length - amax) + " more)");

            return "[" + a.join(", ") + "]";
        }
        else
        {
            try { v = v.toString(); } catch (e) { v = typeof(v); }
            if (v.match(/\[object\s+([a-z0-9_$]+)\]/i))
                v = "object<" + RegExp.$1 + ">";
            return v;
        }
        return typeof(v);
    }
}

// format a function reference to a human-readable form (primarily for
// stack trace generation)
function funcToDesc(f)
{
    // get the function in string form, and convert all whitespace
    // (including newlines) to ordinary spaces
    fn = f.toString().replace(/[ \n\r\v\f\t]+/g, " ");

    // For a named function, the usual toString form is like this:
    //   function name(args) { body }
    // Some browsers return this same form for anonymous functions,
    // supplying 'anonymous' as the name.
    if (fn.match(
        /^function\s+([a-z0-9_$]+)\s*\([a-z0-9_$,\s]*\)\s*(\{.*)$/i))
    {
        // we have the basic syntax - if the name is 'anonymous', show
        // using our anonymous function formatter; otherwise just use
        // the function name
        if (RegExp.$1 == "anonymous")
        {
            // The browser is using literally "anonymous" as the name.
            // Pull out the body to show for context.  Use a clipped
            // version to keep the size reasonable.
            var body = anonFuncToDesc(RegExp.$2);

            // add the event handler name, if applicable
            return funcToEventDesc(f) + body;
        }
        else
        {
            // Named function.
            var body = RegExp.$2;
            fn = RegExp.$1;

            // Some browsers return a synthesized name based on the event
            // attribute for an in-line event handler.  If we have an
            // event name, and the function has the same name, assume
            // this is the situation, so include the function body for
            // context as though it were an anonymous function.
            var ev = funcToEventDesc(f);
            if (ev && ev.substr(ev.indexOf(".") + 1) == fn)
                return ev + anonFuncToDesc(body);

            // it's just a regular named function
            return fn;
        }
    }
    else
    {
        // it's not in the standard format, so it's probably some other
        // anonymous function formatting
        return funcToEventDesc(f) + anonFuncToDesc(fn);
    }
}

// Get the event object for a function call, if applicable
function eventFromFunc(f)
{
    // Check for an event object in the function arguments.  If we have
    // at least one argument, and it's an object, and it has a "type"
    // property, and the type property looks like "onxxx", assume it's
    // an event object.
    if (f.toString().match(/^function\s+[a-z]+\s*\(event\)/)
        && f.arguments
        && f.arguments.length >= 1
        && f.arguments[0] != null
        && typeof(f.arguments[0]) == "object"
        && typeof(f.arguments[0].type) == "string"
        && f.arguments[0].type.match(/^[a-z]+/))
        return f.arguments[0];

    // if this is IE, use the window.event object
    if (window.event)
        return window.event;

    // no event object
    return null;
}

// Check for an event handler.  If this is a top-level function,
// and the target of the event has an attribute for the event
// method, and the attribute points to this function, we must be
// the handler for the event.  In this case we can supply the
// name of the event and the ID or node type of the target object
// as additional context.
function funcToEventDesc(f)
{
    // only top-level functions can be event handlers
    if (f.caller != null)
        return "";

    // we obviously need to have an event for this to be an event handler
    var ev = eventFromFunc(f);
    if (!ev)
        return "";

    // figure the event attribute, based on the event type name
    var attr = "on" + ev.type;

    // The event handler could be on the target, or on a parent object
    // by event bubbling.  Scan up the parent tree for an object with
    // a handler pointing to our function.
    for (var targ = getEventTarget(ev) ; targ ; targ = targ.parentNode)
    {
        // check for a handler in this object pointing to our function
        if (targ[attr] == f)
        {
            // Found it - this must be the handler we're running.  Return
            // the ID of the element (or just the tag name if there's no ID)
            // and the event attribute name as the event description.
            return (targ.id ? targ.id : targ.nodeName)
                + "." + attr;
        }
    }

    // didn't find a handler
    return "";
}

// Format an anonymous function body to human-readable form.  We'll
// show a fragment of the source code body of the function, if available.
function anonFuncToDesc(f)
{
    // some browsers give us the format "{ javascript:source }" for
    // in-line event handlers (onclick, etc) - remove the "javascript:".
    if (f.match(/^\{\s*javascript:\s*(.*)$/i))
        f = "{ " + RegExp.$1;

    // If the body is short, return it unchanged.  If it's long, clip
    // out the middle to keep the display reasonably concise - keep
    // the head and tail for context, and keep a little more of the
    // head than the tail, since that's usually the most easily
    // recognizable portion (for the human reader).
    if (f.length > 60)
        f = f.substr(0, 30) + "..." + f.substr(f.length - 20);

    // return what we found
    return f;
}

// request our initial state
function getInitState()
{
    // send the initial status request for this window
    serverRequest("/webui/getState?window=" + pageName,
                  ServerRequest.Command,
                  function(req, resp) { window.onGameState(req, resp); });
}


/* ------------------------------------------------------------------------ */
/*
 *   Generic document-level key handler, with focus setting.  This does all
 *   the work of genericDocKey(), plus we set the default focus control if
 *   focus isn't already in a key-handling control.
 *   
 *   Any document objects that don't have their own special handling for
 *   keystrokes at the document level can install this default handler at
 *   load time.  This handler looks for a default focus destination, and
 *   establishes focus there.  The default location is normally the main
 *   command line in the main command transcript window.  This handler makes
 *   the standard terminal-style UI a little smoother by always sending focus
 *   back to the command line when the user types something with the focus
 *   set somewhere that doesn't accept input.
 *   
 *   Some windows (such as the top-level window) have their own focus
 *   management rules that differ a little from ours, so they shouldn't use
 *   this.  Instead, they should do their focus management, then call
 *   genericDocKey(), which does the rest of the generic key handling besides
 *   the focus setting.  
 */
function genericDocKeyFocus(desc)
{
    // if it's a text editing key, try setting the default focus
    if (isFieldKey(desc))
        setDefaultFocus(desc, window);

    // handle a generic document key
    genericDocKey(desc);

    // bubble the event
    return true;
}

/*
 *   Handle a generic document-level keystroke.  This should be called from
 *   each window's document key handler.
 *   
 *   First, we check to see if the key is going to an input control that has
 *   focus.  If so, we let the key pass through without further attention, so
 *   that the control gets the key.
 *   
 *   If there's no input control with focus:
 *   
 *   - We try sending the key to the main window for server event handling.
 *   If the server has asked for an input event, we send the key as the
 *   requested event.
 *   
 *   - For certain keys, we suppress the default browser handling.  Since our
 *   UI is primarily oriented around the command line, we want certain keys
 *   to keep their command-line meanings even between inputs.  This is in
 *   case the the user is typing faster than we can keep up with; we don't
 *   want the the key to suddenly change meaning when the user thinks it's an
 *   editing key.  In particular, the Backspace and Escape keys are common
 *   editing keys that would be disruptive to the UI if the browser applied
 *   their usual accelerator meanings of "go to the previous page" and "stop
 *   downloading stuff on this page".  
 */
function genericDocKey(desc)
{
    // if the current focus processes the key, allow the default handling
    var ae = document.activeElement;
    if (eleKeepsFocus(ae, desc))
        return;

    // check to see if the server wants keystroke events
    $win().keyToServer(desc);

    // this key will be processed at the document level, so filter it
    switch (desc.keyName)
    {
    case "U+0008":
    case "U+001B":
        // block the default handling for these keys
        preventDefault(desc.event);
        break;
    }
}

/*
 *   Determine if the given keyboard event is a "field" key.  Returns true if
 *   this is a key that's meaningful to an input field: a regular printable
 *   character, or a cursor movement key (arrow, Home, End, etc). 
 */
function isFieldKey(desc)
{
    // get the key
    var k = desc.keyName;

    switch (k)
    {
    case "Shift":
    case "Control":
    case "Alt":
    case "CapsLock":
    case "PageUp":
    case "PageDown":
    case "NumLock":
    case "ScrollLock":
    case "Win": // Windows/Start shift keys
    case "Pause":
    case "F1":
    case "F2":
    case "F3":
    case "F4":
    case "F5":
    case "F6":
    case "F7":
    case "F8":
    case "F9":
    case "F10":
    case "F11":
    case "F12":
    case "U+0009": // tab
        // none of these keys directly affect the field, so ignore them
        return false;

    case "U+0008":
    case "U+001B":
        // these are explicitly field keys
        return true;

    default:
        // if the Ctrl or Alt keys are down, don't consider this a field
        // key, since it's probably an accelerator or shortcut instead
        if (desc.ctrlKey || desc.altKey || desc.metaKey)
            return false;

        // looks like a regular field key
        return true;
    }
}

/*
 *   Set the default focus.  If the focus isn't currently in a control that
 *   accepts keyboard input, we'll move the focus to the default command
 *   input line, if there is one.  
 */
function setDefaultFocus(desc, fromWin)
{
    // check for retained focus
    if (evtKeepsFocus(desc))
        return;
    
    // if we have a parent window, ask it to look for a default
    var par = window.parent;
    if (par && par != window)
    {
        // there's a parent window - pass the request up to it
        par.setDefaultFocus(desc, fromWin);
    }
    else
    {
        // there's no parent; try setting the default focus to a child window
        setDefaultFocusChild(desc, fromWin);
    }
}

/*
 *   Determine if focus should say put for the current keyboard event.
 */
function evtKeepsFocus(desc)
{
    // if there's no keyboard event, focus can move
    if (!desc || !desc.event)
        return false;

    // There's a weird bug in IE where a control has focus for event
    // purposes but doesn't actually have keyboard focus.  When this
    // happens, the actual Windows event goes to the actual focus control,
    // which isn't part of the JS event bubble.
    var ae = getEventTarget(desc.event);

    // if the event is directed to an input control, leave it where it is
    for ( ; ae ; ae = ae.parentNode)
    {
        // if focus is already in an input object of some kind, leave it there
        if (eleKeepsFocus(ae, desc))
            return true;
    }

    // didn't find a focus keeper
    return false;
}

/*
 *   Determine if the given element should retain focus for the given
 *   keyboard event.  We'll return true if the element is a focusable input
 *   control (an INPUT, TEXTAREA, or SELECT), or it's an <A HREF> and the
 *   event is the Return key (a return key in a hyperlink selects the
 *   hyperlink).
 */
function eleKeepsFocus(ele, desc)
{
    // if there's no element, it doesn't keep focus
    if (!ele)
        return false;

    // Keep focus for all events if this is an input control of some
    // kind (INPUT, TEXTAREA, or SELECT).
    if (ele.nodeName == "INPUT"
        || ele.nodeName == "TEXTAREA"
        || ele.nodeName == "SELECT")
        return true;

    // For IE, also treat any element with contentEditable=true as an
    // input control.
    if (BrowserInfo.ie && ele.isContentEditable)
        return true;

    // If this is an <A HREF> element, and the event is the Return key,
    // keep focus in the hyperlink.  Pressing Return while a hyperlink
    // has focus is usually equivalent to a click in the hyperlink, so
    // this event is meaningful with focus left as it is.
    if (ele.nodeName == "A" && ele.href && desc && desc.keyName == "Enter")
        return true;

    // the event isn't a meaningful input keystroke for this control
    return false;
}

/*
 *   Move focus to a suitable child window.  When we reach the top-level
 *   container window, it will call this to start working back down the
 *   window tree to find a suitable child to set focus in.  
 */
function setDefaultFocusChild(desc, fromWin)
{
    // by default, do nothing; other window types can override it (notably
    // the layout window)
}

/* ------------------------------------------------------------------------ */
/*
 *   Default document mouse-click handler: close any spring-loaded popups
 *   that are currently open.  
 */
function popupCloser(ev)
{
    // get the event
    var ev = getEvent(ev);

    // if there's a spring-loaded popup active, and the event isn't within
    // the top popup, close the top popup
    if (springPopups.length > 0
        && !isEventInObject(ev, springPopups.top().ele)
        && !isEventInObject(ev, springPopups.top().openerEle))
        closePopup();

    // if we have a parent window, propagate the click
    if (window.parent && window.parent != window)
        window.parent.popupCloser(ev);
}

/*
 *   Close the topmost active popup 
 */
function closePopup(ele)
{
    // if there's nothing to close, don't go on
    if (springPopups.length == 0)
        return;

    // if they want to close a specific popup, and it's not the top
    // one, ignore the request - this could be a queued event related
    // to the event that already closed the indicated popup
    if (ele && springPopups.top().ele != ele)
        return;

    // remove the top popup from the list
    var s = springPopups.pop();

    // hide the popup
    s.ele.style.display = "none";

    // call its 'close' method, if it has one
    if (s.close)
        s.close();

    // if there's a default focus element, restore focus there
    if (s.focusEle)
    {
        var fe = $(s.focusEle);
        fe.focus();
        if (fe.nodeName == "INPUT" && fe.type == "text")
            setSelRangeInEle(fe);
    }
}

/*
 *   Open a popup.  'ele' is the outermost html element of the popup, and
 *   'close' is an optional function to call to close the popup.  If
 *   'closeFocusEle' is specified, it's the element where we'll set focus
 *   when the popup closes.  
 */
function openPopup(ev, ele, close, closeFocusEle, openerEle)
{
    // first make sure we don't have another popup to close
    popupCloser(ev);

    // add it to the popup list
    springPopups.push({
        ele: ele,
        close: close,
        focusEle: closeFocusEle,
        openerEle: openerEle
    });

    // move the focus to the popup object
    if (ele.focus)
        ele.focus();

    // if we haven't added our keyboard handlers, do so now
    if (!ele.popupKeyHandlers)
    {
        // add a key handler to catch return and escape while we have focus
        var k = function(desc) {
            if (desc.keyName == "Enter" || desc.keyName == "U+001B")
            {
                closePopup(ele);
                cancelBubble(desc.event);
                return false;
            }
            return true;
        };

        // Also set up a focus-loss handler: if focus leaves the popup
        // (and its children), and focus isn't going to the opener element,
        // close the popup.  Not all elements can acquire focus in the
        // first place on all browsers, so it's basically a no-op on some
        // browsers, but it addresses some edge cases on IE in particular.
        var f = function(ev) {
            var ae = document.activeElement;
            if (!(ae && (isChildElement(ele, ae) || ae == openerEle)))
                closePopup(ele);
        };

        addEventHandler(ele, "keydown", function(ev) { return $kd(ev, k); });
        addEventHandler(ele, "keypress", function(ev) { return $kp(ev, k); });
        addEventHandler(ele, "blur", f);
        ele.popupKeyHandlers = true;
    }

    // don't propagate this event any further
    cancelBubble(ev);
}

// active popup list
var springPopups = [];


/* ------------------------------------------------------------------------ */
/* 
 *   Shorthand to retrieve an object by name.  Call like this:
 *   
 *.     $(obj, [sub [, sub2 [...]]])
 *   
 *   If 'obj' is a string, we treat it as a path of elements separated by
 *   periods.  Each element can be:
 *   
 *     - an object ID
 *.    - a class name, given in square brackets
 *.    - a node name (tag), given in angle brackets
 *   
 *   For example, "mainDiv.[details]" returns the first child with
 *   class="details" of the element with ID "mainDiv".  "form3.<INPUT>"
 *   returns the first INPUT element of form3.
 *   
 *   If 'obj' is any other type, we return it as-is.  This allows passing in
 *   an element reference without first checking its type, since it will
 *   simply be returned unchanged.  This makes it easy to write routines that
 *   accept path names or references as arguments: if the argument is a path
 *   name, it will be translated to an element; if it's already an element,
 *   it will be returned as given.
 *   
 *   If any 'sub' arguments are given, they must be strings giving the same
 *   path notation above.  We'll search for each string in turn as a child
 *   path of the resulting object for the previous argument.  This allows
 *   efficient searching within a part of the DOM tree for which you already
 *   have the root object.  
 */
function $()
{
    // scan each argument
    for (var obj = document.body, ac = 0 ; ac < arguments.length ; ++ac)
    {
        // get this argument
        var arg = arguments[ac];

        // if it's a string, look it up by name
        if (typeof(arg) == "string")
        {
            // break it up into dot-delimited path elements
            var path = arg.split(".");
            
            // find each item in the path
            for (var i = 0 ; i < path.length ; ++i)
            {
                // check the syntax for this element
                var e = path[i];
                if (e.match(/^\[.*\]$/))
                {
                    // search by class name
                    e = e.substr(1, e.length - 2);
                    obj = breadthSearch(
                        obj, function(x) { return x.className == e; });
                }
                else if (e.match(/^\<.*\>$/))
                {
                    // search by tag name
                    e = e.substr(1, e.length - 2);
                    obj = breadthSearch(
                        obj, function(x) { return x.nodeName == e; });
                }
                else
                {
                    // search by ID
                    if (obj == document.body)
                        obj = document.getElementById(e);
                    else
                        obj = breadthSearch(
                            obj, function(x) { return x.id == e; });
                }
                
                // if we didn't find a match at this point, give up
                if (!obj)
                    return null;
            }
        }
        else if (typeof(arg) == "object")
        {
            // the argument is the pre-resolved object
            obj = arg;
        }
        else
        {
            // invalid type
            return null;
        }
    }

    // return what we ended with
    return obj;
}

/*
 *   Do a breadth-first search for a child matching the given condition
 */
function breadthSearch(obj, cond)
{
    // first, search all direct children of 'obj'
    var chi;
    for (chi = obj.firstChild ; chi ; chi = chi.nextSibling)
    {
        if (cond(chi))
            return chi;
    }

    // didn't find it among direct children, so search grandchildren
    for (chi = obj.firstChild ; chi ; chi = chi.nextSibling)
    {
        // skip certain types
        if (chi.nodeName == "SELECT")
            continue;
        
        // search this child's children
        var gc = breadthSearch(chi, cond);
        if (gc)
            return gc;
    }

    // didn't find it
    return null;
}
    
/* ------------------------------------------------------------------------ */
/*
 *   Event handler manipulation.  These routines let you attach and remove
 *   event handler functions.  An element can have any number of attached
 *   handlers; these coexist with one another and with any in-line "onxxx"
 *   handler defined in the element's HTML description.  
 */
function addEventHandler(obj, eventName, func)
{
    obj = $(obj);
    if (obj.addEventListener)
        obj.addEventListener(eventName, func, false);
    else if (obj.attachEvent)
    {
        obj.attachEvent("on" + eventName, func);

        // save it in our private list as well
        if (!obj.$eventList)
            obj.$eventList = [];
        obj.$eventList.push([eventName, func]);
    }
}

function removeEventHandler(obj, eventName, func)
{
    obj = $(obj);
    if (obj.removeEventListener)
        obj.removeEventListener(eventName, func, false);
    else if (obj.detachEvent)
    {
        obj.detachEvent("on" + eventName, func);

        // remove it from our private list
        var l = obj.$eventList;
        if (l)
        {
            for (var i = 0 ; i < l.length ; ++i)
            {
                if (l[i][0] == eventName && l[i][1] == func)
                {
                    l.splice(i, 1);
                    break;
                }
            }
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Synthetic events.  To fire an event, first create the event object with
 *   createEvent(), then send it with sendEvent().
 */

/*
 *   Create an event.  'type' is the class of the event object, taken from
 *   the list below.  'name' is the name of the event, which is the root name
 *   without the "on" prefix.
 *   
 *   'params' is an object giving extra parameters for the event creation.
 *   These vary by event class:
 *   
 *   - bubble: event bubbles up the hierarchy (default=true)
 *.  - cancel: event can be canceled (default=true)
 *.  - view: the window in which the event occurs (default=window)
 *.  - detail: mouse click count, wheel count, etc (default=1)
 *.  - screenX: screen x coordinate (default=0)
 *.  - screenY: screen y coordinate (default=0)
 *.  - clientX: client x coordinate (default=0)
 *.  - clientY: client y coordinate (default=0)
 *.  - ctrlKey: control key setting (default=false)
 *.  - altKey: alt key setting (default=false)
 *.  - shiftKey: shift key setting (default=false)
 *.  - metaKey: meta key setting (default=false)
 *.  - button: mouse button number (default=1)
 *.  - relatedTarget: DOM element related to the mouse event (default=null)
 *   
 *   The event name can be one of the standard browser events, or can be a
 *   custom invented name.  For reference, we list the most common standard
 *   names for the supported event classes below.
 *   
 *   The event types (classes) that we handle are:
 *   
 *   "Event": base event (blur, change, copy, cut, error, focus, input, load,
 *   paste, reset, resize, scroll, select, submit, onload)
 *   
 *   "MouseEvent": mouse event (click, contextmenu, dblclick, drag, drop,
 *   mousedown, mousemove, mouseout, mouseover, mouseup, mousewheel)
 *   
 *   For the above event types, we'll call the class-specific initializer for
 *   the event, using the parameters specified in 'params'.  These event
 *   types are sufficiently cross-browser that we can support them uniformly.
 *   Other classes are not universally supported, so we don't provide
 *   specific handling for them.  We'll return the event object that the
 *   browser gave us, but we won't initialize it.  
 */
function createEvent(type, name, params)
{
    // get a parameter with a default
    var p = function(prop, dflt) {
        return params && (prop in params) ? params[prop] : dflt;
    };
    
    // create the event
    var e;
    if (document.createEvent)
    {
        // firefox, opera, safari... basically everyone but IE
        e = document.createEvent(type);
        switch (type)
        {
        case "Event":
            e.initEvent(name,
                        p("bubble", true),
                        p("cancel", true));
            break;

        case "MouseEvent":
            e.initMouseEvent(name,
                             p("bubble", true),
                             p("cancel", true),
                             p("view", window),
                             p("detail", 1),
                             p("screenX", 0),
                             p("screenY", 0),
                             p("clientX", 0),
                             p("clientY", 0),
                             p("ctrlKey", false),
                             p("altKey", false),
                             p("shiftKey", false),
                             p("metaKey", false),
                             p("button", 1),
                             p("relatedTarget", null));
            break;

        default:
            break;
        }
    }
    else if (document.createEventObject)
    {
        // IE has to do everything its own way, of course...
        e = document.createEventObject();
        e.type = name;
        switch (type)
        {
        case "MouseEvent":
            e.altKey = p("altKey", false);
            e.ctrlKey = p("ctrlKey", false);
            e.shiftKey = p("shiftKey", false);
            e.metaKey = p("metaKey", false);
            e.button = p("button", 1);
            e.clientX = p("clientX", 0);
            e.clientY = p("clientY", 0);
            e.fromElement = p("relatedTarget", null);
            e.toElement = p("relatedTarget", null);
            e.offsetX = 0; // $$$
            e.offsetY = 0; // $$$
            break;
        }
    }
    else
    {
        // no event creation function availble
        e = null;
    }

    return e;
}

/*
 *   Send an event 
 */
function sendEvent(ele, ev)
{
    if (ele.fireEvent)
    {
        // the IE way - set the target
        ev.srcElement = ele;

        // for mouseover and mouseout, set the to/from element properly
        if (ev.type == "mouseover")
            ev.toElement = ele;
        else if (ev.type == "mouseout")
            ev.fromElement = ele;

        // fire the event
        try
        {
            return ele.fireEvent("on" + ev.type, ev);
        }
        catch (e)
        {
            // IE doesn't allow synthesized events; it throws an "invalid
            // argument" error if we attempt it.  Fall back on manually
            // calling the event handlers.
            for (ev.cancelBubble = false ; ele && !ev.cancelBubble ;
                 ele = ele.parentNode)
            {
                // dispatch to our custom handlers in this element
                var l = ele.$eventList;
                if (l)
                {
                    l.forEach(function(item) {
                        if (item[0] == ev.type)
                            item[1](ev);
                    });
                }

                // dispatch to the in-line handler for this element
                if (("on" + ev.type) in ele)
                    ele["on" + ev.type](ev);
            }
        }
    }
    else if (ele.dispatchEvent)
    {
        // firefox, safari, opera... IE-bar
        return ele.dispatchEvent(ev);
    }
    else
    {
        return false;
    }
}
    

/* ------------------------------------------------------------------------ */
/*
 *   Add a keyboard event handler to an object.  This sets up an event
 *   handler based on our cross-browser keyboard package, so you only need to
 *   write a single event handler for the combined keypress and keydown
 *   events.  
 */
function addKeyHandler(obj, func, ctx)
{
    // look up the element, if given by name
    obj = $(obj);

    // set up the keydown and keypress delegation functions
    var kd = function(ev) { return $kd(ev, func, ctx); };
    var kp = function(ev) { return $kp(ev, func, ctx); };

    // set up a record of the function, for later removal if desired
    if (!obj.keyHandlers)
        obj.keyHandlers = [];
    obj.keyHandlers.push({ func: func, kd: kd, kp: kp });

    // add the delegation handlers
    addEventHandler(obj, "keydown", kd);
    addEventHandler(obj, "keypress", kp);
}

/*
 *   Remove a keyboard event handler 
 */
function removeKeyHandler(obj, func, ctx)
{
    // look up the element, if given by name
    obj = $(obj);

    // find the list of handlers we've defined for this element
    var kh = obj.keyHandlers;
    if (kh)
    {
        // search for the user function
        for (var i = 0 ; i < kh ; ++i)
        {
            // check for a match to the user function
            if (kh[i].func == func && kh[i].ctx == ctx)
            {
                // found it - remove the delegation handlers
                removeEventHandler(obj, "keydown", kh[i].kd);
                removeEventHandler(obj, "keypress", kh[i].kp);

                // remove this element from the handler list
                kh.splice(i, 1);

                // no need to keep looking
                break;
            }
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Abstract event handler object.  This simplifies handling for code that's
 *   conditional on some external event or process completing.  Use the
 *   whenDone() method to invoke a callback if and when the event has
 *   occurred: if the event has already occurred, we'll invoke the callback
 *   immediately, otherwise we'll queue it up to be invoked when the event
 *   occurs.
 *   
 *   The 'ele' and 'eventName' arguments are optional.  If you provide them,
 *   we'll automatically link the AbsEvent to the given native browser event
 *   on the given element.  To do this, we'll set up an event listener for
 *   the given event on the given element that invokes the AbsEvent's fire()
 *   method when the native event occurs.
 *   
 *   If you don't provide 'ele' and 'eventName', you must manually call
 *   fire() to trigger the AbsEvent when appropriate.
 *   
 *   There are two main uses for AbsEvent:
 *   
 *   - External events, such as browser events or Flash movie events.  For
 *   this type of use, you can supply the 'ele' and 'eventName' arguments to
 *   set up a handler automatically, OR you can write your own event handler,
 *   which calls the AbsEvent object's fire() method when the native event
 *   occurs.  (It might seem redundant to have AbsEvent at all for native
 *   events.  The value of AbsEvent is that it easily lets you attach
 *   multiple handlers to the event, and you don't have to worry about
 *   whether or not the event has happened yet when you install a handler: if
 *   the event has already happened, the handler will be invoked
 *   immediately.)
 *   
 *   - Pseudo-threads.  This is a long-running task that you break up into a
 *   series of steps, and then carry out the steps gradually using timeouts.
 *   The idea is to allow the UI to remain responsive while the long task is
 *   being executed.  For this, use the startThread() method to initiate the
 *   task.  
 */
function AbsEvent(ele, eventName)
{
    // we're not done yet, and we have no callbacks queued yet
    this.isDone = false;
    this.cb = [];

    // set up our event listener, if applicable
    if (ele && eventName)
    {
        var ae = this;
        addEventHandler(ele, eventName, function() { ae.fire(); });
    }

    // return the new object
    return this;
}

/*
 *   Invoke a callback if and when the event has occurred.  If the event has
 *   already occurred, we'll invoke the callback immediately; otherwise we'll
 *   enqueue it to be invoked when the event fires.
 *   
 *   If 'hurry' is true, and this is a pseudo-thread, we'll set the thread's
 *   step interval to zero so the thread runs to completion as soon as
 *   possible.  This won't affect external events for obvious reasons.  
 */
AbsEvent.prototype.whenDone = function(cb, hurry)
{
    // check to see if the event has already fired
    if (this.isDone)
    {
        // the event has fired already - invoke the callback immediately
        cb();
    }
    else
    {
        // the event hasn't fired yet - queue the callback for later
        this.cb.push(cb);

        // if the caller wants the thread to finish as soon as possible,
        // set the step delay interval to zero
        if (hurry)
            this.interval = 0;
    }
};

/*
 *   Completion event.  The caller must invoke this when our external event
 *   occurs.  
 */
AbsEvent.prototype.fire = function()
{
    // note that the event has fired
    this.isDone = true;

    // invoke each queued callback
    for (var i = 0 ; i < this.cb.length ; ++i)
        this.cb[i]();

    // clear the callback list
    this.cb = [];
};

/*
 *   Start a pseudo-thread.  This sets up a long-running task to be performed
 *   in little chunks that run intermittently while the browser UI runs.  We
 *   accomplish this by executing the chunks with timeouts.
 *   
 *   thread(step) is the thread function to call.  We will invoke this
 *   periodically on a timer.  'step' starts at 0 and is incremented on each
 *   call.  The function should perform one chunk of work and then return
 *   true if we should continue calling it, false if the whole task is
 *   finished.
 *   
 *   interval is the optional timer interval in milliseconds.  If not
 *   supplied, we'll use a default of 25 sms.  
 */
AbsEvent.prototype.startThread = function(thread, interval)
{
    // reset the event
    this.isDone = false;

    // set the time-slice interval
    this.interval = (interval ? interval : 25);

    // for statistics, note the starting time
    this.startTime = (new Date()).getTime();

    // make the first call
    this.stepThread(thread, 0);
}

AbsEvent.prototype.stepThread = function(thread, step)
{
    // If the interval is zero, run the thread to completion without delay.
    // Otherwise do just one step, then schedule the next step after the
    // delay interval.
    if (this.interval == 0)
    {
        // no scheduling delay - run the thread to completion
        while (thread(step++)) ;

        // signal the completion event
        this.fire();
    }
    else if (thread(step++))
    {
        // the thread wants to continue running - schedule the next step
        var e = this;
        setTimeout(function() { e.stepThread(thread, step); },
                   this.interval);
    }
    else
    {
        // the thread is done - signal the completion event
        this.fire();
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Utility functions 
 */

/*
 *   Get the current text selection on the page.  This returns a string
 *   containing the text of the highlighted selection range, if any. 
 */
function getSelText()
{
    // try it the Netscape way
    var sel = window.getSelection && window.getSelection();
    if (sel)
        return sel.toString();
    
    var sel = document.getSelection && document.getSelection();
    if (sel)
        return sel;

    // try it the IE way
    sel = document.selection;
    sel = sel && sel.createRange();
    sel = sel && sel.text;

    // return whatever we found
    return sel;
}

/* 
 *   Get the current selection range in a given element. 
 */
function getSelRangeInEle(ele)
{
    // if they gave us the element name, look it up
    ele = $(ele);
    if (!ele)
        return null;

    // for IE, use the selection range object for the document
    if (document.selection)
    {
        // IE - use a TextRange object, adjusted to be element-relative
        var r, r2, r3;
        try
        {
            if (ele.nodeName == "INPUT" && ele.type.toLowerCase() == "text"
                || ele.nodeName == "TEXTAREA")
            {
                ele.focus();
                r = document.selection.createRange();
                r2 = ele.createTextRange();
                r3 = ele.createTextRange();
            }
            else
            {
                r = document.selection.createRange();
                r2 = r.duplicate();
                r3 = r.duplicate();
                try { r2.moveToElementText(ele); } catch (exc) { }
            }
            r2.setEndPoint('EndToEnd', r);
            try { r3.moveToElementText(ele); } catch (exc) { }

            // make sure it actually overlaps 'ele'
            if (r.compareEndPoints('StartToEnd', r3) <= 0
                && r.compareEndPoints('EndToStart', r3) >= 0)
            {
                // figure the relative start and end points
                var s = r2.text.length - r.text.length;
                var e = s + r.text.length;
                
                // return the range, limiting to the ele's range
                return {
                    start: Math.max(s, 0),
                    end: Math.min(e, r3.text.length)
                };
            }
            else
                return null;
        }
        catch (exc)
        {
        }
    }

    // for other browsers, INPUT and TEXTAREA elements have a special
    // way of handling this
    if (ele.selectionStart || ele.selectionStart == '0')
        return { start: ele.selectionStart, end: ele.selectionEnd };

    // for other browsers, try the window selection range
    var r = window.getSelection();
    if (r)
    {
        // scan the subranges
        for (var i = 0 ; i < r.rangeCount ; ++i)
        {
            // check this subrange
            var sr = r.getRangeAt(i);
            if (rangeIntersectsEle(sr, ele))
            {
                var start, end;

                // set up a range encompassing the target element
                var sr2 = sr.cloneRange();
                sr2.setStart(ele, 0);
                sr2.setEndAfter(ele);

                // figure the starting point
                if (sr.compareBoundaryPoints(Range.START_TO_START, sr2) <= 0)
                    start = 0;
                else
                    start = sr.startOffset;

                // figure the ending point
                if (sr.compareBoundaryPoints(Range.END_TO_END, sr2) >= 0)
                    end = sr2.toString().length;
                else
                    end = sr.endOffset;

                // return the result
                return { start: start, end: end };
            }
        }
    }

    // we didn't find a selection
    return null;
}

function rangeIntersectsEle(range, ele)
{
    // if they gave us the element name, look it up
    ele = $(ele);

    var r = ele.ownerDocument.createRange();
    try {
        r.selectNode(ele);
    } catch (e) {
        r.selectNodeContents(ele);
    }

    return range.compareBoundaryPoints(Range.END_TO_START, r) < 0
        && range.compareBoundaryPoints(Range.START_TO_END, r) > 0;
}

// Set the selection range in a given element.  If the range is omitted,
// we'll select the whole contents of the element, if we can figure
// the range.
function setSelRangeInEle(ele, range)
{
    // if they gave us the element name, look it up
    ele = $(ele);

    // if the range isn't supplied, synthesize the range to cover
    // the whole element
    if (!range)
    {
        // if it's an INPUT or TEXTAREA, use its value
        if ((ele.nodeName == "INPUT" && ele.type == "text")
            || ele.nodeName == "TEXTAREA")
        {
            // it's a text control - use its value string as the range
            range = { start: 0, end: ele.value.length };
        }
        else if (typeof(ele.innerText) == "string")
        {
            // for anything else, use its inner text length
            range = { start: 0, end: ele.innerText.length };
        }
        else
        {
            // can't figure a range
            range = { start: 0, end: 0 };
        }
    }

    // check for browser variations
    if (ele.createTextRange || document.body.createTextRange)
    {
        // IE - we have to do this indirectly through a TextRange object
        var r;
        if (ele.createTextRange)
            r = ele.createTextRange();
        else
        {
            r = document.body.createTextRange();
            r.moveToElementText(ele);
        }
        r.collapse(true);
        r.moveEnd('character', range.end);
        r.moveStart('character', range.start);
        r.select();
    }
    else if (ele.setSelectionRange)
    {
        // non-IE - there's a method that does exactly what we want
        ele.setSelectionRange(range.start, range.end);
    }
}

// Replace the selection in the given control with the given text
function replaceSelRange(ele, txt, selectNewText)
{
    // if they gave us the element name, look it up
    ele = $(ele);

    // get the current selection range
    var r = getSelRangeInEle(ele);
    if (r)
    {
        // make sure we're replacing ONLY the text in this element, by
        // explicitly selecting this range (the range within 'ele' might
        // only have been a subset of a larger selection)
        setSelRangeInEle(ele, r);

        // replace the selection range with the new text
        if (ele.nodeName == "INPUT" || ele.nodeName == "TEXTAREA")
        {
            // replace the text in the element value
            ele.value = ele.value.substr(0, r.start) 
                        + txt
                        + ele.value.substr(r.end);
        }
        else
        {
            // get the selection range object
            if (window.getSelection)
            {
                // non-IE - get the selectionRange for the selection
                var rr = window.getSelection();
                if (rr)
                {
                    // create the selectionRange
                    rr = rr.createRange();

                    // delete its contents
                    rr.deleteFromDocument();

                    // make sure that actually happened (Opera is buggy)
                    if (!window.getSelection().isCollapsed)
                        document.execCommand("Delete");

                    // insert the new text
                    if (rr.rangeCount > 0)
                        rr.getRangeAt(0).insertNode(
                            document.createTextNode(txt));
                }
            }
            else if (document.selection)
            {
                // IE - replace the selection text selection
                var rr = document.selection.createRange();
                rr.text = txt;
            }
            else
                return;
        }
            
        // select the new text if desired, or move the selection to the
        // end of the new text if not
        var range = {
            start: selectNewText ? r.start : r.start + txt.length,
            end: r.start + txt.length
        };
        setSelRangeInEle(ele, range);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Add a new <select> option.  'before' is the index of the item to insert
 *   the new item before; null inserts at the end of the list.  
 */
function addSelectOption(sel, label, val, before)
{
    // create the option
    var opt = new Option(label, val);

    // add it at the selected location
    if (before != null)
    {
        // insert before the given index
        try
        {
            // try it the IE way first: use an option reference for 'before'
            sel.add(opt, sel.options[before]);
        }
        catch (e)
        {
            // that didn't work - try it the standard (non-IE) way, with
            // the numeric index
            sel.add(opt, before);
        }
    }
    else
    {
        // insert at the end of the list
        try
        {
            // try it the IE way, with no extra argument
            sel.add(opt);
        }
        catch (e)
        {
            // that didn't work, so do it the standard way, with a null index
            sel.add(opt, null);
        }
    }
}

/*
 *   Add a new <select> option, sorting it alphabetically into the existing
 *   list. 
 */
function addSelectOptionSort(sel, label, val)
{
    // search the existing <option> list to find the insertion position
    for (var i = 0 ; i < sel.length ; ++i)
    {
        // if the new item sorts before this item, insert here
        var cur = sel.options[i].text;
        if (label.toLowerCase().localeCompare(cur.toLowerCase()) < 0)
            break;
    }

    // add the new option at the position we figured
    addSelectOption(sel, label, val, i < sel.length ? i : null);
}

/*
 *   Set a <select>'s current selection by value.  This looks for an <option>
 *   child of the <select> with the given value, and sets the <select>'s
 *   current selection index to that of the first matching <option>.  Returns
 *   the newly selected index if we find a match, null if not.  
 */
function setSelectByValue(sel, val)
{
    // find the value in the option list
    var i = findSelectOptionByValue(sel, val);

    // if we found it, select it
    if (i != null)
        sel.selectedIndex = i;

    // return what we found
    return i;
}

/*
 *   Find the select option with the given value 
 */
function findSelectOptionByValue(sel, val)
{
    // scan all <option> children
    for (var i = 0 ; i < sel.length ; ++i)
    {
        // check this option
        if (sel.options[i].value == val)
            return i;
    }

    // didn't find it
    return null;
}

/* ------------------------------------------------------------------------ */
/* 
 *   Get the event, given the event parameter - if we actually have an event
 *   parameter we'll just return it, otherwise we'll use the window event
 *   object.  (This weirdness is necessary because of browser variations.) 
 */
function getEvent(e)
{
    return e || window.event;
}

function getEventCopy(e)
{
    return getEvent(e).copy(false);
}

/* 
 *   cancel the "bubble" for an event 
 */
function cancelBubble(ev)
{
    ev = getEvent(ev);
    if (ev.stopPropagation)
        ev.stopPropagation();
    else
        ev.cancelBubble = true;
}

/*
 *   cancel the default browser processing for an event 
 */
function preventDefault(ev)
{
    ev = getEvent(ev);
    if (ev.preventDefault)
        ev.preventDefault();
    else
        ev.returnValue = false;
}

/*
 *   Get the event target: this is the DOM element where the event occurred.
 *   For example, for a mouse click event, this is the DOM element at the
 *   mouse position at the time of the click.  This routine is needed to
 *   account for browser variations.  
 */
function getEventTarget(e)
{
    // get the actual event
    e = getEvent(e);

    // get the target from the event object - this can be either the 'target'
    // or 'srcElement' property, depending on the browser
    var t = e.target || e.srcElement;

    // this weirdness is a work-around for a bug in some Safari versions
    if (t && t.nodeType == 3)
        t = t.parentNode;

    // got it
    return t;
}

/* 
 *   Get the key code from a keyboard event.  Note that this returns the key
 *   code, not the character.  The key code is useful for handling special
 *   keys (cursor arrows, etc).  This routine is needed to smooth out browser
 *   variations.
 *   
 *   (Note that this function isn't needed if you're using the $kd/$kp
 *   functions, which is what we recommend due to their greater portability.
 *   This function is only useful if you're doing keypress/keydown handling
 *   directly.)  
 */
function getEventKey(e)
{
    // get the actual event
    e = getEvent(e);

    // get the event - this varies by browser, naturally
    if (window.event)
        return e.keyCode;
    else
        return e.which;
}

/* 
 *   Get the mouse coordinates of an event, expressed in document-relative
 *   coordinates.  Accounts for browser variations.
 */
function getEventCoords(e)
{
    // get the actual event
    e = getEvent(e);

    // Some browsers use pageX/pageY, while others user clientX/clientY.
    var x, y;
    if (typeof(e.pageX) == 'number')
    {
        // We've got pageX and pageY, so this is easy - all browsers that
        // provide pageX and pageY use them consistently to mean
        // document-relative coordinates, so they tell us exactly what
        // we want to know.
        x = e.pageX;
        y = e.pageY;
    }
    else
    {
        // We don't have pageX/pageY, so we'll have to fall back on clientX
        // and clientY...
        x = e.clientX;
        y = e.clientY;

        // ...which is trickier than it looks.  The problem is that some
        // browsers provide document-relative coordinates in these properties,
        // while others provide coordinates relative to the client window.
        // For the latter, we need to adjust for scrolling to get our
        // desired document coordinates.
        if (!(BrowserInfo.opera
              || window.ScriptEngine && ScriptEngine().indexOf('InScript') != -1
              || BrowserInfo.konqueror))
        {
            // Okay, it's a browser that gives us client-window-relative
            // coordinates, so we have to adjust for scrolling.
            var scrollPos = getScrollPos();
            x += scrollPos.x;
            y += scrollPos.y;
        }
    }

    // return the x,y coordinates
    return { x: x, y: y };
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the target-relative rectangle of the event.  This returns a
 *   rectangle structure consisting of 'x' and 'y' set to the offset WITHIN
 *   THE TARGET OBJECT of the mouse position in the event, and 'width' and
 *   'height' set to the dimensions of the target object.  
 */
function getTargetRelativeRect(e)
{
    // get the event coordinates relative to the target object *
    return getObjectRelativeRect(e, getEventTarget(e));
}

/*
 *   Get the object-relative rectangle of an event.  This returns a rectangle
 *   structure consisting of 'x' and 'y' set to the offset within 'obj' of
 *   the mouse position of the event, and 'width' and 'height' set to the
 *   dimensions of 'obj'.  
 */
function getObjectRelativeRect(e, obj)
{
    // get the real event
    e = getEvent(e);

    // get the event coordinates, in document coordinates
    var evtpos = getEventCoords(e);

    // get the object rectangle
    var objpos = getObjectRect(obj);

    // We have the event and target element positions now, both in the
    // same coordinate system (document-relative): so calculating the
    // event location relative to the target element is just a matter
    // of calculating the difference.  Return the target-relative x
    // position, the target-relative y position, the target width,
    // the target height, and the target object.
    return { x: evtpos.x - objpos.x, y: evtpos.y - objpos.y,
             width: objpos.width, height: objpos.height,
             obj: obj };
}

/*
 *   Is the mouse position of the event within the given object's bounds?
 *   This determines if the event occurred within the given object, directly
 *   or indirectly.  ("Indirectly" within the object means that the event is
 *   within the object's bounds, but also within the bounds of a child of the
 *   object.  In this case, the event target will be the innermost child
 *   containing the event mouse point.  It's often useful to know if an event
 *   occurs within an object or any of its children, which merely looking at
 *   the target object won't tell you.)  
 */
function isEventInObject(e, obj)
{
    // get the object-relative event coordinates
    var r = getObjectRelativeRect(e, obj);

    // if x is between 0 and the width of the object, and y is
    // between 0 and the height of the object, we're within the object
    return (r.x >= 0
            && r.x <= r.width
            && r.y >= 0
            && r.y <= r.height);
}

/*
 *   Determine if 'chi' is a child of 'par' 
 */
function isChildElement(par, chi)
{
    for (var e = chi ; e ; e = e.parentNode)
    {
        if (e == par)
            return true;
    }
    return false;
}

/* ------------------------------------------------------------------------ */
/*
 *   Track mouse movement to simulate a button with a custom object.  The
 *   custom object uses styles to display normal, hover, and active button
 *   states.  You can use either text colors/effects (like hyperlinks do) or
 *   different background images (for graphical buttons).
 *   
 *   To use this, set the element's onmousedown and onmouseover events to
 *   point to this method, passing 'this' for 'ele'.
 *   
 *   The element must have a custom class definition for its style, plus two
 *   extra classes.  If the base class is called 'myclass', you should also
 *   define 'myclassH' for the hovering version, and 'myclassA' for the
 *   active (clicked) version.  We'll automatically add the 'myclassH' or
 *   'myclassA' class to the element's class list: so the full class while
 *   hovering is 'myclass myclassH', and while active 'myclass myclassA'.  
 */
var trackButtonDown = null, trackButtonHover = null;
function trackButton(ev, ele)
{
    // ignore it if we're tracking another element
    if (trackButtonDown && trackButtonDown != ele)
        return true;

    // get the real event object
    ev = getEvent(ev);

    // get the object's base class
    var baseClass = ele.className.split(" ")[0];

    // check the event type
    if (ev.type == "mousedown" && trackButtonDown == null)
    {
        // enter the Active state
        ele.className = baseClass + " " + baseClass + "A";

        // watch for mouse-up events on the document (we have to track the
        // whole document, since we won't get the event in the button itself
        // if the mouse is outside the element when the button is released)
        trackButtonDown = ele;
        addEventHandler(document, "mouseup", trackButtonUp);

        // suppress text selection in IE during this tracking
        addEventHandler(document, "selectstart", trackButtonSelect);

        // don't let this go up to any parent controls
        cancelBubble(ev);
        preventDefault(ev);
        return false;
    }
    else if (ev.type == "mouseover")
    {
        // enter the Hovering state, or the Active state if the
        // button is down
        ele.className = baseClass + " " + baseClass +
                        (trackButtonDown == ele ? "A" : "H");

        // watch for mouse-out, if we're not already
        if (!trackButtonHover || !isEventInObject(ev, trackButtonHover))
        {
            addEventHandler(ele, "mouseout", trackButtonOut);
            trackButtonHover = ele;
        }

        // don't handle it further
        return false;
    }

    // use default handling
    return true;
}
function trackButtonUp(ev)
{
    ev = getEvent(ev);
    var ele = trackButtonDown;
    
    // switch to the base class or Hover class, as appropriate
    var baseClass = ele.className.split(" ")[0];
    ele.className = baseClass +
                    (trackButtonHover ? " " + baseClass + "H" : "");

    // we're finished with the tracking
    removeEventHandler(document, "mouseup", trackButtonUp);
    removeEventHandler(document, "selectstart", trackButtonSelect);
    trackButtonDown = null;
}
function trackButtonSelect(ev)
{
    return false;
}
function trackButtonOut(ev)
{
    ev = getEvent(ev);
    var ele = trackButtonHover;

    // switch to the base class
    ele.className = ele.className.split(" ")[0];

    // we're finished with hovering for this control
    removeEventHandler(ele, "mouseout", trackButtonOut);
    trackButtonHover = null;
}

/* ------------------------------------------------------------------------ */
/* 
 *   Get the current scroll position in this document (the way of getting
 *   this information varies by browser).  
 */
function getScrollPos()
{
    // try the BODY element first
    var base;
    if ((base = document.body) != null
        && (base.scrollLeft || base.scrollTop))
        return { x: base.scrollLeft, y: base.scrollTop };

    // no good; try the document element
    if ((base = document.documentElement) != null
        && (base.scrollLeft || base.scrollTop))
        return { x: base.scrollLeft, y: base.scrollTop };

    // still couldn't find the position - use zeroes
    return { x: 0, y: 0 };
}

/*
 *   Get the window rectangle.  This returns a rectangle giving the bounds of
 *   the window, relative to itself.  This means that the 'x' and 'y' offsets
 *   are always zero; the actual information returned is thus the dimensions
 *   of the window, as the width and height of the rectangle.  
 */
function getWindowRect()
{
    var wid = 1000000, ht = 1000000;
    
    // This is one of those things that varies a lot by browser.
    // Most browsers support window.innerWidth/Height, but of course
    // IE has its own weird way.  What's more, IE has two different
    // ways, depending on version.  And to make matters even worse,
    // the innerWidth/Height properties aren't even consistent across
    // the browsers that support them - some include the scrollbar
    // width in the size, some don't.  We don't want the scrollbars
    // to count because they're not part of the area we can use
    // for drawing.  The solution seems to be to try all of the
    // different methods, and use the smallest non-zero, non-null,
    // non-undefined result.  Each browser seems to have at least
    // one way getting the area sans scrollers, which is the smallest
    // of the various values we'd get.

    // try innerHeight on the window (this is the most common approach,
    // used by almost everyone except IE, but often includes the area
    // covered by scrollbars)
    var x = window.innerWidth, y = window.innerHeight;
    if (typeof(x) == "number" && x > 0)
    {
        wid = x;
        ht = y;
    }

    // try clientHeight on the document (this works on most versions of
    // IE for Windows, and also gets us the sans-scrollbar sizes on some
    // others)
    x = document.documentElement.clientWidth;
    y = document.documentElement.clientHeight;
    if (typeof(x) == "number" && x > 0)
    {
        // keep the smallest so far
        wid = Math.min(wid, x);
        ht = Math.min(ht, y);
    }

    // For the width, also try the clientWidth on the body (this works on
    // IE for Mac and some IE for Win, and gives us sans-scrollbar sizes
    // for many others). However, DON'T use this for the height unless
    // we don't have any height at all so far, since it might only tell
    // us the content height, not the window height.
    x = document.body.clientWidth;
    y = document.body.clientHeight;
    if (typeof(x) == "number" && x > 0)
    {
        wid = Math.min(wid, x);
        if (ht == 1000000)
            ht = y;
    }

    // return the result
    return { x: 0, y: 0, width: wid, height: ht };
}

/*
 *   Get the size of the contents of the given window 
 */
function getContentRect(win)
{
    // we don't know the height or width yet
    var ht = 0, wid = 0;
    
    // get the document body element
    var doc = win.document;
    var body = win.document.body || win.document.documentElement;
    if (!body)
        return { x: 0, y: 0, width: 0, height: 0 };

    // Add a probe division to find the bottom of the existing normal
    // flow elements.  This is necessary in cases where the document only
    // has text elements (without any divisions or other block-level
    // elements wrapping them), because text elements don't make position
    // information available to javascript.  The probe will be inserted
    // just below all of the existing normal-flow content, so its top
    // position gives us the height of the current normal-flow content.
    var probe = doc.createElement("DIV");
    body.appendChild(probe);
    if (probe.offsetTop > ht)
        ht = probe.offsetTop;

    // done with the probe for now
    body.removeChild(probe);

    // Now scan the direct children of the BODY node, and note the maximum
    // right and bottom coordinates.  This will pick up normal-flow items
    // that are actual elements (not #text), as well as floating items and
    // position:absolute items that won't figure into our probe positioning.
    var chi = body.childNodes;
    for (var i = 0 ; i < chi.length ; ++i)
    {
        // get the child and check the type
        var c = chi[i];
        if (c.nodeType == 3)
        {
            // It's a #text node, which means we can't measure its bounding
            // box directly.  Instead, wrap the same text in a SPAN and
            // measure the SPAN's width.
            probe = doc.createElement("SPAN");
            body.insertBefore(probe, body.firstChild);
            probe.innerHTML = c.nodeValue.replace('&', '&amp;')
                              .replace('<', '&gt;');

            // note the width
            if (probe.offsetLeft + probe.offsetWidth > wid)
                wid = probe.offsetLeft + probe.offsetWidth;

            // done with the probe
            body.removeChild(probe);
        }
        else
        {
            // Normal element - measure its bounding box and check to see
            // if it sets a new high-water mark in width or height.
            var rc = getObjectRect(chi[i]);
            if (rc.x + rc.width > wid)
                wid = rc.x + rc.width;
            if (rc.y + rc.height > ht)
                ht = rc.y + rc.height;
        }
    }

    // return the outer boundary rectangle
    return { x: 0, y: 0, width: wid, height: ht };
}

/*
 *   Get the bounds of an object, in document coordinates.  
 */
function getObjectRect(obj)
{
    // return nothing if there's no object
    obj = $(obj);
    if (!obj)
        return null;

    // IE has a special way of doing this
    if (obj.getBoundingClientRect)
    {
        // we need to do this in a try-catch due to a somewhat mysterious
        // problem that occurs with long-running Ajax requests; see
        // http://bugdb.tads.org/view.php?id=134
        var r = null;
        try
        {
            r = obj.getBoundingClientRect();
        }
        catch (e)
        {
            r = { top: 0, left: 0,
                  right: obj.offsetWidth, bottom: obj.offsetHeight };
        }
        var de = document.documentElement;
        var dx = de.scrollLeft, dy = de.scrollTop;
        if (dx == 0 && dy == 0)
        {
            de = document.body;
            dx = de.scrollLeft;
            dy = de.scrollTop;
        }
        return { x: r.left + dx, y: r.top + dy,
                 width: r.right - r.left, height: r.bottom - r.top };
    }

    // get the size of the target element (in pixels)
    var twid = obj.offsetWidth;
    var tht = obj.offsetHeight;

    // Calculate the document-relative coordinates of the (upper left corner
    // of the) target element.  The only information we can get at directly
    // is the container-relative position, so start with that...
    var tx = obj.offsetLeft;
    var ty = obj.offsetTop;

    // ...and now work our way up the parent chain, adding in the
    // container-relative offset of each container.  Keep going until
    // we reach the outermost container (the Body element).
    for (var par = obj.offsetParent ; par != null && par != document.body ;
         par = par.offsetParent)
    {
        tx += par.offsetLeft;
        ty += par.offsetTop;
    }

    // return an object representing the rectangle
    return { x: tx, y: ty, width: twid, height: tht };
}

/*
 *   move an element to a given document-relative location
 */
function moveObject(obj, x, y)
{
    // Find the containing box element (i.e., the parent element with
    // position absolute, relative, or fixed.  Note that IE, as always,
    // has a different way of getting the computed style than the rest
    // of the browsers.
    var parent;
    for (parent = obj.parentNode ; parent != null && parent != document ;
         parent = parent.parentNode)
    {
        var s = parent.currentStyle
                || (document.defaultView
                    && document.defaultView.getComputedStyle
                    && document.defaultView.getComputedStyle(parent, ""));
        if (s)
            s = s.position;
        if (s == "absolute" || s == "relative" || s == "fixed")
            break;
    }
    if (parent == document)
        parent = null;

    // get the parent adjustments
    var dx = 0, dy = 0;
    if (parent)
    {
        var prc = getObjectRect(parent);
        dx = prc.x;
        dy = prc.y;
    }

    // move the object
    if (x != null)
        obj.style.left = (x - dx) + "px";
    if (y != null)
        obj.style.top = (y - dy) + "px";
}

/*
 *   Get the value for a given style of a given element.  This returns the
 *   actual computed style, even if the element doesn't have an explicit
 *   setting for the style.  For example, this can be used to identify the
 *   current font of a given element.
 *   
 *   The property name is given in CSS format: font-name, border-top, etc.  
 */
function getStyle(ele, prop)
{
    // if the element is given by name, get the object
    ele = $(ele);

    // if we're asking for 'font' on IE, we need to build the overall
    // string manually
    if (prop == "font")
    {
        return getStyle(ele, "font-style")
            + " " + getStyle(ele, "font-variant")
            + " " + getStyle(ele, "font-weight")
            + " " + getStyle(ele, "font-size")
            + "/" + getStyle(ele, "line-height")
            + " " + getStyle(ele, "font-family");
    }

    // check for the various ways the computed style is represented in
    // different browsers
    if (ele.currentStyle)
    {
        // try it with the name as given
        var v = ele.currentStyle[prop];
        
        // if that didn't work, we might be on IE, which requires the
        // javascript-style font naming convention instead of CSS: so
        // font-name becomes fontName, etc
        var ieProp = prop.replace(/-([a-z])/g, function(m, lc) {
            return lc.toUpperCase(); });
        return ele.currentStyle[ieProp];
    }
    else if (window.getComputedStyle)
        return document.defaultView.getComputedStyle(ele, null)
            .getPropertyValue(prop);

    // no style information is available in this browser, apparently
    return null;
}


/* ------------------------------------------------------------------------ */
/* 
 *   Initialize the XmlFrame.  This is a hack to work around the (apparently
 *   well-known) Safari perma-throbber bug.  The bug is that Safari animates
 *   its spinning wheel throbber continuously as long as there's an
 *   outstanding XMLHttpRequest.  This is undesirable for a page that uses a
 *   publish/subscribe scheme for contra-flow events and requests (from the
 *   server to the client), because this scheme basically always has an
 *   outstanding request, which makes Safari throb its throbber all the time.
 *   At best it's visually distracting; at worst it's confusing, by making it
 *   look like the browser is stuck loading some last bit of content that
 *   never arrives.
 *   
 *   I've seen a few mentions of this bug on the Web, and some oblique
 *   references to a known fix, but I haven't been able to track down any
 *   actual details on what the fix would be.  My intuition was that Safari
 *   probably only spins for requests in the top-level window, and
 *   empirically this seems to be the case.  If the request comes from an
 *   iframe, Safari doesn't seem to spin its throbber.
 *   
 *   But how exactly do you create a request in an iframe?  The key seems to
 *   be the location of the script where the "new XMLHttpRequest()" happens.
 *   In particular, if a script in an iframe creates the request object, it
 *   doesn't matter who uses it after that - presumably the browser is
 *   tagging the object with the frame where it was created, and runs the
 *   throbber or not accordingly.
 *   
 *   So the hack is to create an invisible iframe, and define the script that
 *   creates our XMLHttpRequests objects in the iframe.  The iframe has the
 *   same domain as the main document, so we can freely call between scripts
 *   in the main doc and the iframe.  The iframe's contents aren't even a
 *   separate http resource - we define them in-line by writing to the iframe
 *   document directly from out script here.  
 */ 
function initXmlFrame()
{
    // create an invisible iframe 
    var f = document.createElement("iframe");
    f.id = "XmlFrame";
    f.style.display = "none";
    document.body.appendChild(f);

    // get its document object
    var d = getIFrameDoc(f);

    // give it a script that sets up a window function to create a request
    d.open();
    d.write("<html><body><script type='text/javascript'>\r\n<!--\r\n"
            + "window.createXmlRequest = function()"
            + "{ return new XMLHttpRequest(); }"
            + "\r\n-->\r\n</" + "script></body></html>");
    d.close();
}

/*
 *   Get the document object for a given IFRAME element 
 */
function getIFrameDoc(fr)
{
    // IE and Mozilla have a contentWindow; Safari only has a contentDocument.
    // Opera 7 has contentDOcument, but actually returns a window!
    var doc = fr.contentWindow || fr.contentDocument;

    // if the object is a window, it'll have a document property pointing
    // to the doc object; if not, it must already be the doc object
    return doc.document || doc;

    // old way - might not be compatible with Opera 7
    // var w = fr.contentWindow;
    // return w.contentDocument || w.document
}

/*
 *   Create an XMLHttpRequest object, adjusting for the various browsers
 */
function createXmlReq()
{
    // try creating the request
    var req = null;
    try
    {
        // try the Safari sub-frame hack first
        req = $("XmlFrame").contentWindow.createXmlRequest();
    }
    catch (e)
    {
        try
        {
            // no good on the Safari hack - try again with the base window
            req = new XMLHttpRequest();
        }
        catch(e)
        {
            // no good on XMLHttpRequest - use IE's unique approach
            try
            {
                req = new ActiveXObject("Msxml2.XMLHTTP");
            }
            catch(e)
            {
                req = new ActiveXObject("Microsoft.XMLHTTP");
            }
        }
    }

    // return the request object 
    return req;
}

// Send an AJAX request to the server.
//
// 'resource' is the URL to the object on the server that you're
// sending the request to.  Browser security restrictions generally
// require this to be in the same domain as the current page, so you
// should usually only use the resource portion of the URL, without
// an "http://domain/" prefix.  The TADS Web UI convention is to
// start all command paths with the prefix "/webui/".
//
// 'typ' is the server request type.  Use ServerRequest.Command for
// a user-initiated action where we have to wait for a reply from
// the server to complete the UI response; we display our little
// "throbber" animation whenever a Command request is in progress,
// to give the user a visual indication that we know we owe them
// a response and that we're just taking our time thinking about it.
// Use Server.Subscription for a request for a notification.  We
// do NOT show any visual indication of this type of request, since
// a subscription request is specifically not expected to return
// immediately; the whole point is to get notified when the subject
// event occurs, which could take an indefinite amount of time, and
// it might not even be a foregone conclusion that the event will
// occur at all.
//
// 'cbfunc' is a function to call when the event completes.  This
// function is called as 'cbfunc(req, xmlDoc)', where 'req' is the
// Javascript XMLHttpRequest object representing the request, and
// 'xmlDoc' is the document element of the XML reply from the server.
// The callback is only invoked if the request succeeds.  We require
// all requests to return XML replies, so it's an error if we get
// a reply without an XML body, meaning the callback is never invoked
// without an xmlDoc value.  'cbfunc' can be omitted or null, in
// which case we simply ignore any successful reply from the request.
//
// 'postData' is an optional string containing the contents to send
// with a POST request.  If this is null, we'll execute a GET request
// with no body; if this is provided, we'll execute a POST instead
// and send the given data as the content body.
//
// 'postDataType' is an optional string with the content-type value
// for the posted data.  If this is omitted, x-www-form-urlencoded
// is used by default.
//
function serverRequest(resource, typ, cbfunc, postData, postDataType)
{
    // create a new request object, and send the request
    (new ServerRequest(resource, typ, cbfunc, postData, postDataType)).send();
}

// our server request tracker object constructor
function ServerRequest(resource, typ, cbfunc, postData, postDataType)
{
    // save the parameters
    this.resource = resource;
    this.reqType = typ || ServerRequest.Command;
    this.cbFunc = cbfunc;
    this.postData = (postData ? postData : null);
    this.postDataType = postDataType;
    this.reqID = ServerRequest.nextID++;
    this.aborted = false;

    // it hasn't succeeded or failed yet
    this.succeeded = this.failed = false;

    // we haven't tried the request yet
    this.tries = 0;
}

// Server request types
ServerRequest.Command = 1;      // command sent from the UI to the server
ServerRequest.Subscription = 2; // subscription to event or other update

// Request ID.  For debugging purposes, give each request a sequential
// ID.  This is different from the "sequence number" in that the sequence
// number is incremented per XMLHttpRequest, whereas the request ID is
// tied to a ServerRequest object.
ServerRequest.nextID = 1;

// Sequence number.  This is a parameter we add to all request URLs to
// ensure that each request is unique, to prevent the browser from using
// a cached reply to a past request as the answer to a new request.  This
// really shouldn't be necessary, because the server *should* set the
// reply headers to indicate that the reply is not to be cached, and the
// browser *should* obey those headers.  But some browsers are buggy.
// We work around this by making each URL unique, using a sequence number
// parameter added to the other URL parameters.  The browser uses the URL
// as the cache key, so adding a different sequence number to each request's
// URL effectively eliminates caching by ensuring that no two requests ever
// have the exact same URL.
ServerRequest.sequenceNum = (new Date()).getTime();

// Number of outstanding Command requests.  While command requests are
// pending, we show a visible "waiting" indicator so that the user knows
// why the UI is unresponsive.
ServerRequest.pendingCommandCnt = 0;

// list of open ServerRequest objects
var openServerRequests = [];

// Send the request.
ServerRequest.prototype.send = function()
{
    // create a request
    var req = this.req = createXmlReq();

    // if we got a request, send it
    if (req)
    {
        // set up the response callback
        var sr = this;
        req.onreadystatechange = function() { sr.xmlEvent(req); }

        // Add a sequence number parameter to the URL.  We presume the
        // server will ignore this, as it's just an extra parameter that
        // it's not looking for; this is simply to make certain the URL
        // is distinct from any past URLs we've ever sent, so that the
        // browser will actually send the request to the server rather
        // than using a cached reply from a past request.
        var resource = this.resource;
        resource += (resource.indexOf("?") < 0 ? "?" : "&")
                    + "TADSWseqno=" + ServerRequest.sequenceNum++;

        // open the request
        req.open(this.postData ? "POST" : "GET", resource, true);

        // if this is a command request, start up the waiting indicator
        var w;
        if (this.reqType == ServerRequest.Command
            && (w = $win("main.statusline")) && w.NetThrobber)
            w.NetThrobber.onStartRequest();

        // if we're posting data, set the content type to form data
        if (this.postData)
            req.setRequestHeader(
                "Content-type",
                this.postDataType || "application/x-www-form-urlencoded");

        // Send it.  If this is the first try, send it immediately.
        // If this is a retry, send it after a delay.  Use longer delays
        // for repeated retries; if it's some kind of temporary network
        // or server glitch as we're assuming, but we keep failing, we
        // might just need more time for whatever it is to clear up.
        var postData = this.postData;
        var f = function()
        {
            // send the request
            req.send(postData);

            // add it to our open request list
            openServerRequests.push(sr);
        }
        if (this.tries == 0)
        {
            // first try - send immediately
            f();
        }
        else
        {
            // retry - send with a delay according to the number of retries,
            // plus a small random component (just in case the problem is
            // server contention with another client; the random component
            // helps prevent us from retrying in lockstep with the other guy)
            var delays = [0, 500, 1000, 2000, 5000, 5000];
            var r = Math.floor(Math.random()*201) - 100;
            setTimeout(f, (this.tries < delays.length
                           ? delays[this.tries] : 10000) + r);
        }
    }
    else
    {
        alert("This " + selfNoun
              + " requires support for XML Requests (sometimes "
              + "called \"Ajax\" requests).  It appears that your "
              + "browser doesn't support this feature, or that "
              + "you've disabled it.  To continue, you'll need to "
              + "upgrade your browser (or simply re-enable this feature, "
              + "if you've disabled it in the browser settings).");
    }
}

// Abort a request
ServerRequest.prototype.abort = function()
{
    // flag the request as aborted
    this.aborted = true;

    // abort the XMLHttpRequest
    if (this.req)
        this.req.abort();
}

// XML event callback.  The browser invokes this when the status of a
// request changes.
ServerRequest.prototype.xmlEvent = function(req)
{
    // check the request status
    if (req.readyState == 4)
    {
        // if this is a command request, turn off the waiting indicator
        var w;
        if (this.reqType == ServerRequest.Command
            && (w = $win("main.statusline"))
            && w.NetThrobber)
            w.NetThrobber.onEndRequest();

        // remove the request from our open request list
        var idx = openServerRequests.indexOf(this);
        if (idx >= 0)
            openServerRequests.splice(idx, 1);
        
        // the request has completed - retrieve the XML document
        var xmlr = req.responseXML;
        if (xmlr)
            xmlr = xmlr.documentElement;

        // if we got the XML document, we've completed successfully
        if (xmlr && req.status == 200)
        {
            // Successful completion - flag it as such
            this.succeeded = true;

            // If this was a retry, an earlier try failed, so we logged
            // errors for the request.  Mark those errors as now
            // automatically resolved.
            if (this.tries > 0)
                $win().unlogError(this);

            // invoke the user callback
            if (this.cbFunc)
                this.cbFunc(req, xmlr);
        }
        else if (this.aborted)
        {
            // the request was aborted
        }
        else
        {
            // get the status text, if available
            try { var statusText = req.statusText; }
            catch (e) { statusText = ""; }

            // We got an error from the server.  Build a human-readable
            // list of details for the error.
            var detail = "Request ID: " + this.reqID
                         + "<br>Resource: " + this.resource.htmlify()
                         + "<br>HTTP Status: " + req.status
                         + " " + statusText.htmlify()
                         + (req.responseText
                            ? "<br>Response:<br><div class=\"errorSubDetail\">"
                            + req.responseText.htmlify()
                             .replace(/\n/g, "<br>")
                            + "</div>"
                            : "");

            // set up a default error message
            var msg = "An error occurred communicating with the server."
                      + errorRecoveryAction;

            // assume we won't retry this request
            var retry = false;

            // Check for server disconnection in standalone mode on
            // Windows.  On Windows, the standalone interpreter uses
            // the IE window.external interface to provide access to
            // this information.  The disconnect flag tells us when the
            // user terminates the server program explicitly, such as
            // via the Workbench debugger.  If we're not running on the
            // Windows interpreter in standalone mode, this interface
            // won't be available, so we'll harmlessly skip this test
            // on other platforms.
            //
            // Otherwise, check the HTTP status and the reply body to
            // determine what went wrong.
            //
            if (window.external
                && window.external.IsConnected
                && !window.external.IsConnected())
            {
                // The server program has been explicitly terminated,
                // such as with a Terminate Game command in the Workbench
                // debugger.
                this.failed = true;
                msg = "The program has been terminated.";
            }
            else if (req.status == 503
                     && req.statusText.match(/\(Shutting Down\)/))
            {
                // Normally, the server won't shut down until all clients
                // disconnect.  However, there are ways to force it to shut
                // down externally, such as via a Terminate Game command
                // in the debugger.  When the server shuts down while
                // requests are still pending, it bounces the pending
                // requests with a 503 error with "(Shutting Down)" in
                // the status text.
                this.failed = true;
                msg = "The program has been terminated.";
            }
            else if (req.status != 408
                     && req.status >= 400 && req.status <= 499)
            {
                // Most 400 status codes indicate that the request was
                // transmitted properly but is not acceptable to the
                // server.  There's no reason to retry these requests.
                this.failed = true;
                msg = "The server rejected a network request from the "
                      + "client. This could be due to a programming "
                      + "error in the " + selfNoun + "'s display logic."
                      + errorRecoveryAction;
            }
            else if (req.status == 500 || req.status == 501)
            {
                // 500 (internal server error) and 501 (not implemented)
                // indicate server-side errors that are most likely
                // permanent.
                this.failed = true;
                msg = "An error occurred on the server. This could be "
                      + "due to a programming error in the "
                      + selfNoun + "."
                      + errorRecoveryAction;
            }
            else if (req.status != 200 || !req.responseText)
            {
                // No response text, so it could be a network or server
                // glitch, which could be temporary.  Try resending the
                // request, unless we've already tried it several times,
                // in which case it's probably not going to clear up by
                // itself.
                if (this.tries++ < 5)
                {
                    msg = "A (possibly temporary) network error occurred "
                          + "communicating with the server. "
                          + ["", "", "(Second attempt)", "(Third attempt)",
                             "(Fourth attempt)", "(Fifth attempt)"]
                          [this.tries];
                    retry = true;
                }
                else
                {
                    // we don't need to retry this request
                    this.failed = true;
                }
            }
            else
            {
                // ill-formed reply - probably a server problem
                this.failed = true;
                msg = "The server sent an incorrectly formatted response. "
                      + "This can be caused by a garbled network "
                      + "transmission, or by a bug in the " + selfNoun + "."
                      + errorRecoveryAction;
            }

            // log the error
            $win().logError(msg, detail, this);

            // if desired, retry the request
            if (retry)
                this.send();
        }
    }
}

// Cancel all outstanding requests.  We call this when the window is
// about to be unloaded.  This is particularly important for refresh,
// because request objects from the previous incarnation of the document
// can linger after the new document is loaded, and this can cause
// various kinds of confusion.
function cancelServerRequests()
{
    openServerRequests.forEach(function(sr) { sr.abort(); });
}


/* ------------------------------------------------------------------------ */
/*
 *   Find a window by path.  This traverses the window (iframe) tree starting
 *   at the top-level window.  Path name elements are separated by periods.
 *   For example, "main.statusline" is the standard statusline window.  If no
 *   path is given, simply retrieves the top-level window.  
 */
function $win(path)
{
    // find the top-level window
    var w;
    for (w = window ; w.parent && w.parent != w ; w = w.parent) ;

    // if there's no path, get the top-level window
    if (!path)
        return w;

    // ask the top-level window to resolve the path
    return w.windowFromAbsPath(path);
}


/* ------------------------------------------------------------------------ */
/*
 *   XML helpers 
 */

// Get the list of children of an XML node with the given name
function xmlChild(parent, name)
{
    return parent.getElementsByTagName(name);
}

// Get the text value of a child of an XML node
function xmlChildText(parent, name)
{
    // return the text contents of the child node of interest
    return xmlNodeText(xmlChild(parent, name));
}

// Get the text value of a given node
function xmlNodeText(node)
{
    // if it's an array, get the first element
    if (node && node.length)
        node = node[0];

    // if we found the node, return its text 
    if (node && node.firstChild)
    {
        // Some browsers (e.g., Firefox) break up text nodes into 4k chunks,
        // so we need to check for multiple text children and paste
        // everything back together.  Make a list of the child nodes.
        for (var s = [], c = node.firstChild ; c ; c = c.nextSibling)
            s.push(c.data);

        // append them together and return the result
        return s.join("");
    }
    else
    {
        // no child node or no text - return an empty string
        return "";
    }
}

// Does the given parent have the given child?
function xmlHasChild(parent, name)
{
    var child = parent.getElementsByTagName(name);
    return child && child.length;
}

/*
 *   General xml path parser.  This takes an XML node and a path string, and
 *   returns the value of the child or attribute specified by the path.  The
 *   path has these forms:
 *   
 *.    ele      - a single child element description, as defined below
 *.    ele/path - evaluate xpath(ele, path)
 *   
 *   'ele' descriptions:
 *   
 *.    tag      - first child node with tag name 'tag'
 *.    tag[n]   - nth child node with tag name 'tag'
 *.    tag[*]   - list of all children with tag name 'tag'
 *   
 *   The final path element can have a modifier:
 *
 *.    .attr    - get the value of attribute 'attr'
 *.    #text    - get the text contents of the node
 */
function xpath(node, path)
{
    // if the path isn't already an array, split it by '/' element
    if (typeof(path) == "string")
        path = path.split("/");

    // pull the first element out of the list
    var ele = path.shift();

    // if this is the last element, check for modifiers
    var mod = null, idx;
    if (path.length == 0 && (idx = ele.search(/\..+$|#text$/)) >= 0)
    {
        // separate the modifier and the element name
        mod = ele.substr(idx);
        ele = ele.substr(0, idx);
    }

    // check for a [*] or [n] index; if it's not there, the default is zero
    var nodeIdx = 0;
    if ((idx = ele.search(/\[\*\]/)) >= 0)
    {
        // they want an array of all of the matching nodes
        nodeIdx = -1;
        ele = ele.substr(0, idx);
    }
    else if ((idx = ele.search(/\[\d+\]$/)) >= 0)
    {
        // get the index value, and remove it from the element name
        nodeIdx = parseInt(ele.substr(idx+1));
        ele = ele.substr(0, idx);
    }

    // find the given child
    node = xmlChild(node, ele);

    // if we didn't find it, return failure
    if (!node)
        return null;

    // if the index is out of bounds, return failure
    if (nodeIdx >= node.length)
        return null;

    // if they want the node array, return the array
    if (nodeIdx == -1)
    {
        // build a true array (rather than an XML node list object)
        for (var arr = [], i = 0 ; i < node.length ; ++i)
            arr.push(node[i]);

        // use the array
        node = arr;
    }
    else
    {
        // pull out the indexed node
        node = node[nodeIdx];
    }

    // if there's anything left, evaluate the rest of the path
    if (path.length)
        return xpath(node, path);

    // That's the end of the path.  If we have a modifier, apply the
    // modifier.
    if (mod)
    {
        if (mod == "#text")
        {
            // they want the text value of the node
            if (nodeIdx == -1)
                return node.map(function(ele) { return xmlNodeText(ele); });
            else
                return xmlNodeText(node);
        }
        else if (mod.substr(0, 1) == '.')
        {
            // they want an attribute - get the attribute object
            var attrName = mod.substr(1);
            if (nodeIdx == -1)
            {
                return node.map(function(ele) {
                    var attr = ele.attributes.getNamedItem(attrName);
                    return attr ? attr.value : null;
                });
            }
            else
            {
                var attr = node.attributes.getNamedItem(attrName);
                return attr ? attr.value : null;
            }
        }
    }

    // no modifier - just return the node itself
    return node;
}


/* ------------------------------------------------------------------------ */
/*
 *   Handle a command hyperlink.
 */
function gamehref(ev, href, win, ele)
{
    // send this to the target window, if found
    if (win = $win(win))
        return win.gamehref(ev, href, null, ele);

    // otherwise, we can't handle it - just ignore it
    return false;
}

/* ------------------------------------------------------------------------ */
/*
 *   Log a debugging message 
 */
function debugLog(msg)
{
    // send this to our parent window, if we have one
    if (window.parent && window.parent != window)
        window.parent.debugLog(msg);
}

function debugLogLn(msg)
{
    debugLog(msg + "<br><hr>");
}

/*
 *   It's sometimes helpful during development to see a stack trace in the
 *   debug log, especially    This is pretty user-unfriendly information, though, so we
 *   omit it by default.  If you do want to enable it during your development
 *   and testing process, add this code to the functions above, or simply add
 *   it directly wherever you'd like to see a stack trace.
 *   
 *.    debugLog(
 *.      getStackTrace().map(
 *.         function(ele) {return ele.desc.htmlify();}).join("<br>")
 *.      + "<br><br>");
 */


/* ------------------------------------------------------------------------ */
/*
 *   Show a custom dialog.  See main.js for details on the parameters object.
 */
function showDialog(params)
{
    // send this to the top window
    $win().showDialog(params);
}

/*
 *   Show an alert dialog using the in-line dialog manager. 
 */
function $alert(message, title)
{
    showDialog({
        title: title ? title : "Message",
        contents: message.htmlify()
    });
}

/*
 *   Show a yes/no confirmation dialog using the in-line dialog manager.  On
 *   closing the dialog, we'll call func(true) if they clicked Yes,
 *   func(false) if No.  
 */
function $confirm(message, func, title)
{
    showDialog({
        title: title ? title : "Confirm",
        contents: message.htmlify(),
        buttons: [
            { name: "Yes", val: true, key: "Y" },
            { name: "No", val: false, isCancel: true, key: "N" }
        ],
        dismiss: function(dlg, btn) {
            func(btn && btn.val);
            return true;
        }
    });
}

/* ------------------------------------------------------------------------ */
/*
 *   Alpha (transparency) control.  'pct' is a percentage opacity value from
 *   0 to 100.  
 */
function setAlpha(ele, pct)
{
    ele = $(ele);
    if (ele.filters)
    {
        // IE's typically overwrought approach
        pct = Math.floor(pct);
        if (ele.filters.alpha)
        {
            // already have an alpha filter - set it directly
            ele.filters.alpha.opacity = pct;
        }
        else
        {
            // No alpha yet - add it via the style.  In IE8+, the filter
            // name requires the full "progid:DXImageTransform..." notation;
            // otherwise that notation isn't allowed, and we have to use
            // the simple "alpha" name.
            var fn = (BrowserInfo.ie && BrowserInfo.ie >= 8
                      ? "progid:DXImageTransform.Microsoft.Alpha" : "alpha");
            var f = fn + "(opacity=" + pct + ")";

            // if we have any filters, append this to the existing filter
            // style string, otherwise just set the initial filter style
            if (ele.filters.length > 0)
                ele.style.filter += " " + f;
            else
                ele.style.filter = f;
        }
    }

    // everyone else (including IE9+) uses standard CSS 'opacity'
    try { ele.style.opacity = "" + pct/100; } catch(exc) { }
}

// active fader threads
var faderThreads = [];

/*
 *   Fade an object in or out over a given time interval.  'dir' is 1 for
 *   fading in, -1 for fading out.  't' is the time interval in milliseconds
 *   for the overall fade.  'doneFunc' is an optional function to call when
 *   the fade is completed.  
 */
function startFade(ele, dir, t, doneFunc)
{
    // get the actual element
    ele = $(ele);

    // kill any existing fader thread for this element
    var oldThread = killFaderThread(ele);

    // figure the interval length based on the total fade time
    var interval = (t < 50 ? 5 : t < 250 ? 10 : 25);

    // calculate how many of our intervals fit into t
    var intervals = t/interval;

    // if we have no more than one interval, simply finish the fade now
    if (intervals < 2)
    {
        setAlpha(ele, dir > 0 ? 100 : 0);
        if (doneFunc)
            doneFunc();
        return;
    }

    // start at full on or off
    var start = dir > 0 ? 0 : 100;

    // ...unless a fade was in progress, in which case start at the
    // actual current alpha
    if (oldThread)
        start = oldThread.alpha;

    // get the amount of fade per interval
    var step = dir * (100 / intervals);

    // create a tracker object for this element and add it to our list
    var t = {ele: ele, doneFunc: doneFunc, alpha: start,
             interval: interval, step: step};
    faderThreads.push(t);

    // start the fade timer running
    t.timer = setTimeout(function() { fadeFunc(t); }, 0);;
}

function fadeFunc(t)
{
    // calculate the next value
    t.alpha += t.step;

    // limit the result to 0..100
    t.alpha = (t.alpha < 0 ? 0 : t.alpha > 100 ? 100 : t.alpha);

    // set the new alpha
    setAlpha(t.ele, t.alpha);

    // check to see if we're still going
    if (t.step > 0 ? t.alpha < 100 : t.alpha > 0)
    {
        // more to do - set the timer for the next step
        setTimeout(function() { fadeFunc(t); }, t.interval);
    }
    else
    {
        // done - remove our thread from the list
        for (var i = 0 ; i < faderThreads.length ; ++i)
        {
            if (faderThreads[i] == t)
            {
                faderThreads.splice(i, 1);
                break;
            }
        }

        // call the 'done' callback, if there is one
        if (t.doneFunc)
            t.doneFunc();
    }
}

function killFaderThread(ele)
{
    // look for an existing fader for this element
    for (var i = 0 ; i < faderThreads.length ; ++i)
    {
        // if this is the one, kill this thread
        var t = faderThreads[i];
        if (t.ele == ele)
        {
            // cancel the timer
            clearTimeout(t.timer);

            // remove the thread from the list
            faderThreads.splice(i, 1);

            // if there's a finisher callback, invoke it now
            if (t.doneFunc)
                t.doneFunc();

            // return the thread object
            return t;
        }
    }

    // didn't find a fader for this element
    return null;
}


/* ------------------------------------------------------------------------ */
/*
 *   Image cache.  We use this to pre-load images that we might need if the
 *   network connection is lost (e.g., error dialog background images), and
 *   for images that are needed in UI responses and thus must be immediately
 *   ready to display without a network delay to load them.  
 */
var imageCache = [];
function cacheImage(name)
{
    // create an Image object for the image
    var i = new Image();

    // load it
    i.src = name;

    // save it in our cache list
    imageCache.push(i);
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the installed font list.
 *   
 *   On systems with Flash, this uses our custom Flash object to retrieve the
 *   actual list of installed fonts.  Where Flash isn't available, we'll use
 *   a canned list of common fonts, but we'll filter it (using some CSS
 *   kludgery) to include only the fonts actually present, so we'll return a
 *   subset of installed fonts.  
 */
function getFontList()
{
    // forward the request to the top-level winodw, which has the flash
    // object and supporting code
    return $win().getFontList();
}

/*
 *   Rebuild the font list.  This starts the build process and returns
 *   immediately.  The build process runs in the background; when complete,
 *   we call the callback.  
 */
function buildFontList(doneFunc)
{
    $win().TADS_swf.buildFontList(doneFunc);
}


/* ------------------------------------------------------------------------ */
/*
 *   Parse an HTML tag attribute list.  Returns an object giving the
 *   attribute values keyed by attribute ID.  
 */
function parseTagAttrs(str)
{
    // create an empty object to hold the attributes
    var attrs = { };

    // parse the string
    while (str)
    {
        // match the next attribute
        var m = str.match(/^\s+([a-z0-9]+)(=([^\s]+|"[^"]+"|'[^']+'))?/i);
        if (!m)
            break;

        // get the attribute value
        var val = RegExp.$3;

        // if it's quoted, remove the quotes
        if (val.charAt(0) == '"' || val.charAt(0) == '\'')
            val = val.substr(1, val.length - 2);

        // store the attribute
        attrs[RegExp.$1] = val;

        // remove the bit we just parsed
        str = str.substr(m[0].length);
    }

    // return the attribute list
    return attrs;
}

/* ------------------------------------------------------------------------ */
/*
 *   CSS color parsing 
 */

// parse a color in CSS format, either "#rrggbb" or "rgb(r,g,b)", returning
// the integer rrggbb value
function parseColor(val)
{
    // remove spaces
    val = val.replace(/\s/g, "");

    // check the various formats - #rgb, #rrggbb, rgb(r,g,b)
    var r, g, b;
    if (val.match(/^rgb\((\d+),(\d+),(\d+)\)/i))
    {
        // rgb(r,g,b) decimal format
        r = parseInt(RegExp.$1);
        g = parseInt(RegExp.$2);
        b = parseInt(RegExp.$3);
    }
    else if (val.match(/^#[0-9a-f]{3}$/i))
    {
        // #rgb format - expand into #rrggbb
        r = parseInt(val.substr(1) + val.substr(1), 16);
        g = parseInt(val.substr(2) + val.substr(2), 16);
        b = parseInt(val.substr(3) + val.substr(3), 16);
    }
    else if (val.match(/^#[0-9a-f]{4,6}/i))
    {
        // #rrggbb format
        return parseInt(val.substr(1, 6), 16);
    }
    else if (val.match(/^[0-9a-f]+/))
    {
        // rrggbb format (without the #)
        return parseInt(val.substr(1, 6), 16);
    }
    else
    {
        // unknown format
        return 0;
    }

    // combine the components into the result value
    return ((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff);
}

// unparse a color in combined integer format
function unparseColor(color)
{
    return unparseCColor((color >> 16) & 0xff,
                        (color >> 8) & 0xff,
                        color & 0xff);
}

// unparse a color in component format into a CSS #rrggbb string
function unparseCColor(r, g, b)
{
    // combine the RGB values into an integer
    var color = ((r << 16) | (g << 8) | b);

    // convert it to a hex string
    color = color.toString(16);

    // add the "#" and leading zeros to pad to 6 digits
    return "#000000".substr(0, 7 - color.length) + color;
}

// generate the canonical #rrggbb format for a color in any CSS format
function canonicalizeColor(color)
{
    return unparseColor(parseColor(color));
}



/* ------------------------------------------------------------------------ */
/*
 *   Flash player plug-in version detection.  Returns the version string if
 *   the flash player plug-in is installed, null otherwise.  
 */
function getFlashPlayerVersion()
{
    // First, try the plugin list, which everyone except IE uses.  If
    // that fails, and we're not on a Mac, try the IE ActiveX loader.
    if (navigator.plugins && navigator.plugins.length > 0)
    {
        var p;
        if (navigator.mimeTypes
            && (p = navigator.mimeTypes['application/x-shockwave-flash'])
            && (p = p.enabledPlugin)
            && p.description)
            return p.description;
    }
    else if (navigator.appVersion.indexOf("Mac") == -1 && window.execScript)
    {
        // possible class IDs
        var clid = ["ShockwaveFlash.ShockwaveFlash.7",
                    "ShockwaveFlash.ShockwaveFlash.6",
                    "ShockwaveFlash.ShockwaveFlash"];

        // try each class ID
        for (var i = 0 ; i < clid.length ; ++i)
        {
            var obj;
            try
            {
                // try loading this class ID
                obj = new ActiveXObject(clid[i]);

                // got it - return its version
                obj.AllowScriptAccess = "always";
                return obj.GetVariable("$version");
            }
            catch (exc)
            {
                // failed to load the current class ID
                obj = null;
            }
        }
    }
    
    // if we get here, it means that we've exhausted our detection
    // alternatives without finding anything, so return failure
    return null;
}


/* ------------------------------------------------------------------------ */
/*
 *   Cross-browser keyboard event handling.  This package smooths out most of
 *   the differences between browsers for keyboard events, by providing a
 *   uniform event model and key naming scheme across browsers, while still
 *   retaining full information on special keys, modifiers, and so on.
 *   
 *   To set up a keyboard event handler with this package, DON'T write
 *   keypress and keydown handlers.  Instead, for each element where you want
 *   to handle keyboard events, write a single key handler function with this
 *   interface:
 *   
 *.    function myOnKey(desc, ctx)
 *   
 *   This function will be called once per keystroke with details on the key
 *   pressed and the character generated, if any.  'desc' is a descriptor
 *   object with information on the key event, as describe below, and 'ctx'
 *   is the context argument you specify in the onkeydown and onkeypress
 *   specifiers.
 *   
 *   To install your custom event handler function, add onkeydown and
 *   onkeypress attributes to the HTML element where you want to intercept
 *   the key events, as follows:
 *   
 *.    <input onkeydown="javascript:return $kd(event, myOnKey, myCtx);"
 *.           onkeypress="javascript:return $kp(event, myOnKey, myCtx);">
 *   
 *   'myOnKey' is the name of your custom key event handler function.  The
 *   rest is boilerplate that you should copy as shown.  'myCtx' is an
 *   optional context argument for your function; if included, it's simply
 *   passed as the second argument to myOnKey().
 *   
 *   The descriptor object 'desc' has the following properties:
 *   
 *.    keyName:  a string giving the DOM 3 key identifier for the
 *.              keyboard key pressed
 *.    keyLoc:   0 for a regular key, 1 for the left key of a paired
 *.              modifier key (e.g., Left Ctrl), 2 for the right key
 *.              of a paired modifier key, 3 for a numeric keypad key
 *.    ch:       a string giving the character generated by the key, or
 *.              null for a special key
 *.    modifier: 0 for ordinary keys; 1 if the key is a temporpary
 *.              modifier key that applies while held down (Shift, Ctrl,
 *.              Alt, Meta, Option, Windows), 2 if the key is a modifier
 *.              lock key that toggles a keyboard mode (Caps Lock,
 *.              Num Lock, Scroll Lock)
 *.    shiftKey: true if the Shift key was pressed, false if not
 *.    ctrlKey:  true if the Ctrl key was pressed, false if not
 *.    altKey:   true if the Alt key was pressed, false if not
 *.    metaKey:  true if the Meta key was pressed, false if not (on
 *.              Macs, the Command key is the Meta key)
 *.    keyCode:  the raw key code generated by the browser
 *.    event:    the original javascript event object for the event
 *   
 *   Your function should return true or false: true means that the browser
 *   should continue to the default processing, false means that the browser
 *   should not perform its default processing.  For example, for an <INPUT>
 *   control, returning false blocks the browser from inserting the character
 *   for the keystroke into the field.
 *   
 *   You can find the full set of DOM key codes at
 *   http://www.w3.org/TR/DOM-Level-3-Events/#keyset-keyidentifiers.  Here's
 *   a brief list of the most common keys:
 *   
 *.    Up, Down, Left, Right - cursor arrows
 *.    Home, End, Insert, PageUp, PageDown - cursor navigation
 *.    U+007F - Delete (Unicode value for Delete)
 *.    U+0008 - Backspace
 *.    U+0009 - Tab
 *.    U+001B - Escape
 *.    Enter - Return/Enter
 *.    U+0041 - "A" (Unicode value for capital A; likewise for other letters)
 *.    U+0033 - 3/# key (Unicode for the unshifted symbol on the key)
 *.    F1 - function key F1 (likewise F2, F3, ...)
 *   
 *   Note that the keyLoc value will sometimes return 0 for both versions of
 *   a modifier key.  This means that the current browser doesn't distinguish
 *   the left/right versions.  keyLoc will also return 0 for the numeric
 *   keypad on Opera, which doesn't provide enough information to distinguish
 *   the numeric keypad from the regular top row digit keys.  You should
 *   therefore avoid using the keypad digit keys for special commands, unless
 *   you provide equivalents somewhere else on the keyboard for Opera users.
 *   
 *   The 'keyCode' value is browser-specific - it's simply the code passed in
 *   from the browser in the original event object.  You shouldn't need to
 *   look at this in most cases, and indeed you shouldn't *want* to look at
 *   it - it's incredibly hard to interpret properly because each browser has
 *   its own numbering scheme, and some browsers have even changed schemes in
 *   new versions.  This is a big part of why you'd want to use this package
 *   to start with.  We include the raw key code really just for edge cases
 *   that exceed the capabilities of this package, where we might have
 *   discarded some information that we couldn't represent portably.
 *   
 *   The 'event' object also varies considerably by browser.  For the most
 *   part, you shouldn't need or want to look at this, since a major part of
 *   this package's point is to provide you with the same information in a
 *   somewhat more sane cross-browser format.  However, as with the 'keyCode'
 *   value, there might be occasional situations where you'll need some
 *   original event information that we discarded.  Use the event object as a
 *   last resort when you can't get what you need from 'desc'.
 *   
 *   Another complication using 'keyCode' and/or 'event' is that your event
 *   handler can be invoked on either the keydown or the keypress phase, so
 *   you'll have to be careful to interpret 'keyCode' and 'event' using the
 *   right context.  'keyCode' in particular often has different meanings in
 *   the two phases.  You can get the current phase from desc.event.type.
 *   
 *   Credits:
 *   
 *   Much of the functionality implemented here is based on the browser data
 *   collected by Jan Wolter and documented in "JavaScript Madness: Keyboard
 *   Events": http://unixpapa.com/js/key.html.  
 */

// Key Code to key name mapping for the most common assignments.  Some
// browsers differ in some places, which we'll handle with exception
// tables.  Each entry consists of:
//
//    [keyID, isSpecial, location, ctlChar, isMod]
//
// keyID - the key identifier per the DOM 3 specification.
//
// isSpecial - 0 if this is a "regular" key, which generates a character
// code.  1 if this is a "special" key OTHER THAN a modifier key.  A
// special key is a key that doesn't generate a character code.  These
// include the cursor arrows, other cursor keys (Home, End, etc), paging
// keys (Page Up, Page Down), function keys, etc.  The traditional ASCII
// control keys (Enter, Backspace, Tab, Escape, Delete) are NOT special,
// because they do generate character codes, even though those characters
// aren't printable.  isSpecial is 2 for "modifier" keys.  Modifiers are
// non-character keys that combine with other keys to change their
// meanings: Shift, Ctrl, Alt, Option, Meta, Windows, Apple/Command.
// Like other specials, these don't generate character codes.  We
// distinguish them in the table because they have slightly different
// behavior from other special keys in some browsers.
//
// location - the key location, per the DOM 3 specification: 0 for ordinary
// keys on the main keyboard, 1 for the left version of a key mirrored on
// both sides of the keyboard (such as Shift or Ctrl), 2 for the right
// version of a mirrored key, and 3 for a key on the numeric keypad.  The
// key location can only be reliably determined from the DOM 2 event data
// in a handful of cases, but we'll use this to pass along the information
// when we can infer it.  For cases where it's not possible to distinguish
// the various possible versions of a key, we'll just use '0'.
//
// ctlChar - a few browsers (e.g., Mozilla 5) treat the ASCII control
// characters (Enter, Tab, Backspace, Escape) as special characters, and
// don't generate character codes for them.  This element gives the ASCII
// control character for these cases, so that we can plug them in manually
// when needed.
//
// isMod - 0 for ordinary keys, 1 for temporary modifiers keys (Shift,
// Ctrl, Alt, Meta, Option, Windows), 2 for shift lock keys that toggle
// keyboard modes (Caps Lock, Num Lock, Scroll Lock).
//

var KeyMapper = {
    commonKeyCodeMap: {
        65:  ["U+0041", 0, 0, 0, 0], // A
        66:  ["U+0042", 0, 0, 0, 0], // B
        67:  ["U+0043", 0, 0, 0, 0], // C
        68:  ["U+0044", 0, 0, 0, 0], // D
        69:  ["U+0045", 0, 0, 0, 0], // E
        70:  ["U+0046", 0, 0, 0, 0], // F
        71:  ["U+0047", 0, 0, 0, 0], // G
        72:  ["U+0048", 0, 0, 0, 0], // H
        73:  ["U+0049", 0, 0, 0, 0], // I
        74:  ["U+004A", 0, 0, 0, 0], // J
        75:  ["U+004B", 0, 0, 0, 0], // K
        76:  ["U+004C", 0, 0, 0, 0], // L
        77:  ["U+004D", 0, 0, 0, 0], // M
        78:  ["U+004E", 0, 0, 0, 0], // N
        79:  ["U+004F", 0, 0, 0, 0], // O
        80:  ["U+0050", 0, 0, 0, 0], // P
        81:  ["U+0051", 0, 0, 0, 0], // Q
        82:  ["U+0052", 0, 0, 0, 0], // R
        83:  ["U+0053", 0, 0, 0, 0], // S
        84:  ["U+0054", 0, 0, 0, 0], // T
        85:  ["U+0055", 0, 0, 0, 0], // U
        86:  ["U+0056", 0, 0, 0, 0], // V
        87:  ["U+0057", 0, 0, 0, 0], // W
        88:  ["U+0058", 0, 0, 0, 0], // X
        89:  ["U+0059", 0, 0, 0, 0], // Y
        90:  ["U+005A", 0, 0, 0, 0], // Z
        49:  ["U+0031", 0, 0, 0, 0], // 1
        50:  ["U+0032", 0, 0, 0, 0], // 2
        51:  ["U+0033", 0, 0, 0, 0], // 3
        52:  ["U+0034", 0, 0, 0, 0], // 4
        53:  ["U+0035", 0, 0, 0, 0], // 5
        54:  ["U+0036", 0, 0, 0, 0], // 6
        55:  ["U+0037", 0, 0, 0, 0], // 7
        56:  ["U+0038", 0, 0, 0, 0], // 8
        57:  ["U+0039", 0, 0, 0, 0], // 9
        48:  ["U+0030", 0, 0, 0, 0], // 0
        32:  ["U+0020", 0, 0, 0, 0], // space
        13:  ["Enter", 0, 0, "\015", 0],  // Enter/Return
        9:   ["U+0009", 1, 0, "\011", 0], // Tab
        27:  ["U+001B", 0, 0, "\033", 0], // Escape
        8:   ["U+0008", 1, 0, "\010", 0], // Backspace
        16:  ["Shift", 2, 0, 0, 1],  // Shift
        17:  ["Control", 2, 0, 0, 1], //Control (Ctrl)
        18:  ["Alt", 2, 0, 0, 1],    // Alt
        19:  ["Pause", 1, 0, 0, 0],  // Pause/Break
        20:  ["CapsLock", 2, 0, 0, 2], // Caps Lock
        144: ["NumLock", 2, 0, 0, 2],  // Num Lock
        145: ["ScrollLock", 2, 0, 0, 2], // Scroll Lock
        37:  ["Left", 1, 0, 0, 0],   // Left cursor arrow
        38:  ["Up", 1, 0, 0, 0],     // Up cursor arrow
        39:  ["Right", 1, 0, 0, 0],  // Right cursor arrow
        40:  ["Down", 1, 0, 0, 0],   // Down cursor arrow
        45:  ["Insert", 1, 0, 0, 0], // Insert
        46:  ["U+007F", 1, 0, "\177", 0], // Delete
        36:  ["Home", 1, 0, 0, 0],   // Home
        35:  ["End", 1, 0, 0, 0],    // End
        33:  ["PageUp", 1, 0, 0, 0], // Page Up (PgUp)
        34:  ["PageDown", 1, 0, 0, 0],  // Page Down (PgDn)
        112: ["F1", 1, 0, 0, 0],     // F1
        113: ["F2", 1, 0, 0, 0],     // F2
        114: ["F3", 1, 0, 0, 0],     // F3
        115: ["F4", 1, 0, 0, 0],     // F4
        116: ["F5", 1, 0, 0, 0],     // F5
        117: ["F6", 1, 0, 0, 0],     // F6
        118: ["F7", 1, 0, 0, 0],     // F7
        119: ["F8", 1, 0, 0, 0],     // F8
        120: ["F9", 1, 0, 0, 0],     // F9
        121: ["F10", 1, 0, 0, 0],     // F10
        122: ["F11", 1, 0, 0, 0],     // F11
        123: ["F12", 1, 0, 0, 0],     // F12
        188: ["U+002C", 0, 0, 0, 0],  // "," (comma)
        190: ["U+002E", 0, 0, 0, 0],  // "." (period)
        191: ["U+002F", 0, 0, 0, 0],  // "/" (slash)
        192: ["U+0060", 0, 0, 0, 0],  // "`" (back quote/grave accent)
        219: ["U+005B", 0, 0, 0, 0],  // "[" (left square bracket)
        220: ["U+005C", 0, 0, 0, 0],  // "\" (backslash)
        221: ["U+005D", 0, 0, 0, 0],  // "]" (right square bracket)
        222: ["U+0027", 0, 0, 0, 0],  // "'" (apostrophe)
        110: ["U+002E", 0, 3, 0, 0],  // Keypad "."
        96:  ["U+0030", 0, 3, 0, 0],  // Keypad "0"
        97:  ["U+0031", 0, 3, 0, 0],  // Keypad "1"
        98:  ["U+0032", 0, 3, 0, 0],  // Keypad "2"
        99:  ["U+0033", 0, 3, 0, 0],  // Keypad "3"
        100: ["U+0034", 0, 3, 0, 0],  // Keypad "4"
        101: ["U+0035", 0, 3, 0, 0],  // Keypad "5"
        102: ["U+0036", 0, 3, 0, 0],  // Keypad "6"
        103: ["U+0037", 0, 3, 0, 0],  // Keypad "7"
        104: ["U+0038", 0, 3, 0, 0],  // Keypad "8"
        105: ["U+0039", 0, 3, 0, 0],  // Keypad "9"
        107: ["U+002B", 0, 3, 0, 0],  // Keypad "+"
        109: ["U+002D", 0, 3, 0, 0],  // Keypad "-"
        106: ["U+002A", 0, 3, 0, 0],  // Keypad "*"
        111: ["U+002F", 0, 3, 0, 0],  // Keypad "/"
        12:  ["U+0035", 1, 3, 0, 0],  // Keypad "5" with NumLock off
        91:  ["Win", 1, 1, 0, 1],     // Left "Windows" key (Windows)
        92:  ["Win", 1, 2, 0, 1],     // Right "Windows" key (Windows)
        93:  ["Props", 1, 0, 0, 0]    // "Properties" key (Windows)
    },

    // Mozilla key mappings - these are used by Gecko-based browsers
    mozKeyMap: {
        59:  ["U+003B", 0, 0, 0, 0],  // ";" (semicolon)
        61:  ["U+003D", 0, 0, 0, 0],  // "=" (equals sign)
        109: ["U+002D", 0, 0, 0, 0],  // "-" (hyphen)
        224: ["Meta", 1, 0, 0, 1]     // Apple/Command key (Mac OS)
    },

    // Key code mappings special to Gecko 1.09-based browsers
    moz109KeyMap: {
        107: ["U+003D", 0, 0, 0, 0] // "=" (equals sign) - in older
                              // versions, this was the code for the
                              // keypad +, but in 1.09 it seems to be 
                              // overloaded as the ordinary [=+] key
    },

    // Key code mappings special to IE
    ieKeyMap: {
        186: ["U+003B", 0, 0, 0, 0],  // ";" (semicolon)
        187: ["U+003D", 0, 0, 0, 0],  // "=" (equals sign)
        189: ["U+002D", 0, 0, 0, 0]   // "-" (hyphen)
    },

    // Key mappings special to Opera 8.0-9.27 Win, Opera <9.5 Linux/Mac
    operaKeyMap: {
        59:  ["U+003B", 0, 0, 0, 0],  // ";" (semicolon)
        61:  ["U+003D", 0, 0, 0, 0],  // "=" (equals sign)
        44:  ["U+002C", 0, 0, 0, 0],  // "," (comma)
        46:  ["U+002E", 0, 0, 0, 0],  // "." (period)
        47:  ["U+002F", 0, 0, 0, 0],  // "/" (slash)
        96:  ["U+0060", 0, 0, 0, 0],  // "`" (backquote/grave accent)
        91:  ["U+005B", 0, 0, 0, 0],  // "[" (left square bracket)
        92:  ["U+005C", 0, 0, 0, 0],  // "\" (backslash)
        93:  ["U+005D", 0, 0, 0, 0],  // "]" (right square bracket)
        39:  ["U+0027", 0, 0, 0, 0],  // "'" (apostrophe)
        17:  ["Control", 2, 0, 0, 1], // Control/Ctrl ; Apple/Command key on Mac
        219: ["Win", 2, 1, 0, 1],     // Left "Windows" key (Windows)
        220: ["Win", 2, 2, 0, 1],     // Right "Windows" key (Windows)
        144: ["NumLock", 1, 0, 0, 2], // Num Lock
        8:   ["U+0008", 0, 0, "\010", 0], // Backspace isn't special on Opera
        9:   ["U+0009", 0, 0, "\011", 0]  // Tab
    },

    // Key mappings special to Opera >=9.5 and Opera 7 win.  These Opera
    // versions use the Gecko map (mozKeyMap), with the exceptions listed here.
    opera95KeyMap: {
        43:  ["U+002B", 0, 3, 0, 0],  // Keypad "+"
        42:  ["U+002A", 0, 3, 0, 0],  // Keypad "*"
        47:  ["U+002F", 0, 3, 0, 0],  // Keypad "/"
        17:  ["Control", 2, 0, 0, 1], // Control/Ctrl ; Apple/Command key on Mac
        219: ["Win", 2, 1, 0, 1],     // Left "Windows" key (Windows)
        220: ["Win", 2, 2, 0, 1],     // Right "Windows" key (Windows)
        144: ["NumLock", 1, 0, 0, 2], // Num Lock
        8:   ["U+0008", 0, 0, "\010", 0], // Backspace isn't special on Opera
        9:   ["U+0009", 0, 0, "\011", 0]  // Tab
    },

    // WebKit key map.  WebKit uses the same codes as IE, with one slight
    // exception: Tab, Escape, and Backspace are treated as special keys,
    // in that they don't generate keypress events.
    webKitKeyMap: {
        9:   ["U+0009", 1, 0, "\011", 0], // Tab
        27:  ["U+001B", 1, 0, "\033", 0], // Escape
        8:   ["U+0008", 1, 0, "\010", 0]  // Backspace
    },

    // "Pseudo-ASCII" mappings special to Opera < 9.5 Linux & Mac,
    // and Konqueror
    pseudoAsciiKeyMap: {
    
    },

    // browser keyCode for the last keydown event
    kdCode: null,

    // our key map entry for the last keydown event
    kdInfo: null,

    // Should we suppress the next keydown event?  For certain keys, we
    // need to call the user event handler in keydown in order to get in
    // at the right point to cancel browser default handling, if the user
    // handler should want to do so.  In many cases, the browsers in
    // question don't send keypress events at all for the keys in question.
    // However, there are a few cases where the browser will send both
    // events, but since we do the handling in keydown, we need this flag
    // to tell our keypress handler not to repeat the user handler call.
    suppressKeypress: false,

    // Opera timeout for processing potential Win keys (codes 219, 220)
    operaWinKeyTimeout: null,

    // deferred keypress info, for workaround for Opera double keypad events
    deferredKeyPressInfo: null
};

// General onkeydown processor.
function $kd(ev, func, ctx)
{
    // get the actual event object
    ev = ev || window.event;

    // get the key code, which is always in event.keyCode
    var keyCode = KeyMapper.kdCode = ev.keyCode;
    
    // browser info
    var brID = BrowserInfo.browser;
    var brVer = BrowserInfo.version;
    var brOS = BrowserInfo.OS;

    // The *location* of the key code is the same in all browsers, but
    // that's a deceptive bit of uniformity, because the *meaning* of
    // the value stored there varies from one browser to the next, and
    // even from one version to the next of the same browser.  To map
    // the key code to our portable key names, we need to run a bunch
    // of special-case tests for the various browsers.
    //
    // Start with the base mappings that most browsers agree on.
    keyInfo = KeyMapper.commonKeyCodeMap[keyCode];

    // Now check for browser-specific exceptions
    var xkey = null;
    if (brID == "IE")
    {
        // use the IE key codes
        xkey = KeyMapper.ieKeyMap[keyCode];
    }
    else if (BrowserInfo.webKit)
    {
        // use the IE key codes, with the webkit exceptions
        xkey = KeyMapper.webKitKeyMap[keyCode];
        if (!xkey)
            xkey = KeyMapper.ieKeyMap[keyCode];
    }
    else if (BrowserInfo.gecko)
    {
        // if it's 1.09, use the 1.09 exceptions first
        if (BrowserInfo.gecko == 1.09)
            xkey = KeyMapper.moz109KeyMap[keyCode];

        // use the standard Mozilla key codes if there wasn't an override
        if (!xkey)
            xkey = KeyMapper.mozKeyMap[keyCode];
    }
    else if ((brID == "Opera" && brVer >= 9.05)
             || (brID == "Opera" && brVer >= 7 && brVer < 8
                 && brOS == "Windows"))
    {
        // these Opera versions *mostly* use mozKeyMap entries, except for
        // the exceptions in the opera95KeyMap (keypad and branded keys)
        xkey = KeyMapper.opera95KeyMap[keyCode];
        if (!xkey)
            xkey = KeyMapper.mozKeyMap[keyCode];
    }
    else if ((brID == "Opera" && brVer < 9.05)
             || brID == "Konqueror")
    {
        // these versions use the "pseudo-ASCII" key map
        xkey = pseudoAsciiKeyMap[keyCode];
    }

    // Opera on Mac gives us "Control" for the Command/Apple key, when
    // it should really give us "Meta"
    if (brID == "Opera" && brOS == "Mac" && keyCode == 17)
        xkey = ["Meta", 1, 0, 0, 1];

    // if we found an exception entry, it overrides the base mapping
    if (xkey)
        keyInfo = xkey;

    // save the canonical key code information so that it's accessible in
    // onkeypress, and presume that we'll process the keypress if it comes
    KeyMapper.kdInfo = keyInfo;
    KeyMapper.suppressKeypress = false;

    // If we're on Opera, and this looks like a Windows key, we have a
    // bit of a problem.  Opera overloads the same codes for Win left,right
    // with the ordinary "[", "\" keys.  There's a difference, though:
    // the Win keys don't generate keypress events, while the [ \ keys do.
    // So if we get to keypress for this key, we'll know it's a regular
    // key rather than a Win key.  The snag is that we can only tell that
    // we have a Win key when we *don't* get a keypress event.  This might
    // seem like it falls into the "it's impossible to prove a negative"
    // category, but we can deal with this via a timeout, because Opera
    // will queue the keypress immediately on our return.  Now, once the
    // timeout expires without the keypress arriving, we have another
    // problem, which is that it's too late to cancel the keydown event
    // if the user callback wishes to do so.  We'll accept that as a
    // limitation; this isn't too terrible, since the Win keys are
    // basically OS-level hotkeys, so it's not unreasonable to expect
    // that they're uncancellable anyway.
    if (brID == "Opera" && (keyCode == 219 || keyCode == 220))
    {
        // Set a timeout to call the callback if a keypress doesn't arrive
        // first, which would indicate that it's not a Win key after all
        var t = setTimeout(function() {
            callKeyFunc(func, keyInfo, null, ev, keyCode, ctx);
        }, 0);

        // remember the key info and callback ID, in case the keypress occurs
        KeyMapper.operaWinKeyTimeout = { keyCode: keyCode, timeout: t }

        // done with this event
        return false;
    }
    else
    {
        // clear the timeout record
        KeyMapper.operaWinKeyTimeout = null;
    }

    // if we don't have any key information, proceed to keypress; no
    // use calling the callback now, as we have nothing to tell it
    if (!keyInfo)
        return true;

    // Most browsers call both onkeydown and onkeypress for every key,
    // regular and special.  Because we won't have the character code
    // (if any) until the onkeypress call, the simplest approach for
    // browsers that call both events for all keys is to return now,
    // and let our onkeypress handler call the user callback.
    //
    // There's a wrinkle, though: some browsers don't call onkeypress
    // for special keys.  For these browsers, then, we have to call the
    // callback now, rather than waiting for onkeypress.  Fortunately
    // there's no lost information doing this, since special keys don't
    // have character codes anyway.  (The specific browsers are IE,
    // WebKit >= 525, and Konqueror 3.2.)
    //
    // You might wonder why we don't just call the callback for special
    // keys here, regardless of browser.  The reason is that a bunch of
    // the other browsers (those that do call onkeypress for special
    // keys) have a different quirk: they don't call onkeydown for
    // auto-repeated special keys.  So in order to generate our events
    // for auto-repeated specials, we need to wait until keypress for
    // the affected browsers.  (The specific browsers are Opera,
    // Konqueror, WebKit < 525, Mac Gecko, and some Linux Gecko
    // versions.)
    //
    // Note that there's an overlapping group that calls both onkeypress
    // and onkeydown for special keys, including auto-repeated special
    // keys.  For those browsers, we can call our callback for special
    // keys either here or in onkeypress.  We'll choose to make the call
    // here only for browsers where we know we're not going to get
    // onkeypress calls at all, so that we don't have to make an extra
    // test to suppress onkeypress calls that we've already handled.
    //
    // If this is a shift key, we also won't get keypress events for Gecko,
    // WebKit >= 525, or Opera >= 10.10.
    //
    // On Windows, also call the callback immediately for Ctrl or Alt
    // used with regular keys.  Some of the Windows browsers don't send
    // keypress events for these.
    var specialsNow = (brID == "IE"
                       || (BrowserInfo.webKit && BrowserInfo.webKit >= 525)
                       || (brID == "Konqueror" && brVer == 3.02));
    var shiftsNow = (BrowserInfo.gecko
                     || (BrowserInfo.webKit && BrowserInfo.webKit >= 525)
                     || (brID == "Opera" && brVer > 9.50));
    var ctlsNow = (brOS == "Windows"
                   && (brID == "IE"
                       || BrowserInfo.gecko
                       || BrowserInfo.webKit));
    var altsNow = (brOS == "Windows"
                   && (brID == "IE"
                       || BrowserInfo.webKit));

    if ((specialsNow && keyInfo[1])
        || (shiftsNow && keyInfo[1] == 2)
        || (ctlsNow && ev.ctrlKey && keyInfo[1] == 0)
        || (altsNow && ev.altKey && keyInfo[1] == 0))
    {
        // it's a special key, and we're on a browser that doesn't call
        // onkeypress for special keys - invoke the user callback now
        
        // use the associated mapped event for special keys
        var ch = null;
        if (keyInfo[3])
            ch = keyInfo[3];

        // flag that we're suppressing any subsequent keypress for this key
        KeyMapper.suppressKeypress = true;

        // call the user callback
        return callKeyFunc(func, keyInfo, ch, ev, keyCode, ctx);
    }
    else if (brOS == "Windows" && (brID == "IE" || brID == "Opera")
             && keyInfo && !keyInfo[1]
             && (ev.ctrlKey || ev.altKey))
    {
        // process Ctrl and Alt keys in keydown for IE and Opera
        return callKeyFunc(func, keyInfo, null, ev, keyCode, ctx);
    }
    else
    {
        // proceed to onkeypress
        return true;
    }
}

// General onkeypress processor
function $kp(ev, func, ctx)
{
    // get the actual event object
    ev = ev || window.event;

    // if we're suppressing the keypress event, ignore it
    if (KeyMapper.suppressKeypress)
    {
        KeyMapper.suppressKeypress = false;
        return true;
    }

    // browser info
    var brID = BrowserInfo.browser;
    var brVer = BrowserInfo.version;
    var brOS = BrowserInfo.OS;

    // In keypress, the ASCII character for the key is either in
    // event.which or event.charCode.
    var ch;
    if (KeyMapper.kdInfo && KeyMapper.kdInfo[1])
    {
        // We've already mapped it to a special key, so ignore any
        // character information.  Some browsers incorrectly set
        // charCode for some or all special keys.
        ch = null;

        // Exception: if we're on Opera, check event.which to see if it's
        // a regular or special key.  If we thought it was special, but it
        // turns out that it has a character code, it must be a key where
        // we couldn't tell the difference in keydown.
        if (brID == "Opera" && ev.which)
        {
            // assume we'll use the character code from event.which
            ch = String.fromCharCode(ev.which);

            // check the key code
            switch (KeyMapper.kdCode)
            {
            case 45:
                // it's actually "-_" or "Keypad -", depending on version
                if (brVer >= 9.05
                    || (brVer >= 7 && brVer < 8 && brOS == "Windows"))
                    KeyMapper.kdInfo = ["U+002D", 0, 3, 0, 0];
                else
                    KeyMapper.kdInfo = ["U+002D", 0, 0, 0, 0];
                break;

            case 219:
            case 220:
                // it's really [{ or \|
                KeyMapper.kdInfo =
                    [KeyMapper.kdCode == 219 
                     ? "U+005B" : "U+005C", 0, 0, 0, 0];

                // cancel the Win key timeout, if applicable
                var ot = KeyMapper.operaWinKeyTimeout;
                if (ot && ot.keyCode == KeyMapper.kdCode)
                {
                    clearTimeout(ot.timeout);
                    KeyMapper.operaWinKeyTimeout = null;
                }
                break;

            case 78:
                // it's really N
                KeyMapper.kdInfo = ["U+004E", 0, 0, 0, 0];
                break;

            default:
                // It's not one of the special re-translated keys, so
                // it's not a regular key after all.  Opera has some
                // weirdness where it sets event.which to a non-zero
                // value for some special keys, so just ignore that.
                ch = null;
                break;
            }
        }
    }
    else if (ev.which == null)
    {
        // no event.which, so this must be IE - the char is in keyCode
        ch = String.fromCharCode(ev.keyCode);
    }
    else if (ev.which != 0 && ev.charCode != 0)
    {
        // we have a non-zero event.which and a non-zero charCode,
        // so the char is in 'which'
        ch = String.fromCharCode(ev.which);
    }
    else
    {
        // otherwise, this must be a special key, so there is no
        // character code
        ch = null;
    }

    // work around some Opera bugs
    if (brID == "Opera")
    {
        // Check for a weird special case: if the character is '.',
        // and the key code is 78, it's the keypad '.' rather than
        // the "N" key.
        if (KeyMapper.kdCode == 78 && ch == '.')
            KeyMapper.kdInfo = ["U+002E", 0, 3, 0, 0];
        
        // Opera has a particularly strange bug where it generates
        // doubled keypress events for the / * - + keys on the numeric
        // keypad.  Empirically, the first keypress of the pair is the
        // erroneous one, in that the browser does its default
        // processing on the second keypress.  Opera queues both
        // keypress events at the same time, which is useful for
        // our workaround, because it means that we can set all of
        // the cancellation flags on the first keypress without
        // blocking or otherwise affecting the second one.  So,
        // when we see the first of the pair, we'll remember it
        // internally, and then cancel it.  That will let us recognize
        // the second of the pair and process it as the actual event.
        if (KeyMapper.kdInfo
            && KeyMapper.kdInfo[2] == 3
            && [42, 43, 45, 47].indexOf(KeyMapper.kdCode) != -1
            && !(KeyMapper.deferredKeyPressInfo
                 && KeyMapper.deferredKeyPressInfo[2] == 3
                 && KeyMapper.kdInfo[0] == KeyMapper.deferredKeyPressInfo[0]))
        {
            // remember the deferred key press for next time - when
            // we see the repeated event, we'll know to let it through
            KeyMapper.deferredKeyPressInfo = KeyMapper.kdInfo;
            
            // Cancel the bubble and prevent any default processing.
            // The entire existence of this event is a bug, so try our
            // best to make it look like it never happened.
            cancelBubble(ev);
            preventDefault(ev);
            return false;
        }

        // we're no longer waiting for a deferred keypress event
        KeyMapper.deferredKeyPressInfo = null;
    }

    // Gecko on Windows generates alphabetic characters for Alt+letter
    // combinations.  These should actually be treated as special characters.
    if ((BrowserInfo.gecko || brID == "Opera") && brOS == "Windows"
        && ev.altKey && ch)
        ch = null;
        
    // if there's no character code, but we have an explicit character code
    // override with our mapping, apply it
    if (ch == null && KeyMapper.kdInfo && KeyMapper.kdInfo[3])
        ch = KeyMapper.kdInfo[3];

    // we don't have any deferred keypress
    KeyMapper.deferredKeypressInfo = null;

    // don't send Ctrl+Alpha events on Windows, since we've already
    // processed those as though they were special keys
    if (KeyMapper.kdCode >= 65 && KeyMapper.kdCode <= 90
        && ev.ctrlKey
        && brOS == "Windows")
        return true;

    // invoke the handler
    return callKeyFunc(func, KeyMapper.kdInfo, ch, ev, KeyMapper.kdCode, ctx);
}

// call the common user event handler for onkeydown/onkeypress
function callKeyFunc(func, keyInfo, ch, ev, keyCode, ctx)
{
    // if we have no key info, key code, or character, ignore it
    if (!ch && !keyInfo)
        return true;

    // if it's Ctrl+Alpha, figure the control character
    if (!ch
        && keyCode >= 65 && keyCode <= 90
        && ev.ctrlKey && !ev.altKey)
        ch = String.fromCharCode(keyCode - 64);

    // set up the key descriptor
    var desc = {
        // key name, location, and generated character
        keyName: keyInfo && keyInfo[0],
        keyLoc: keyInfo && keyInfo[2],
        ch: ch,

        // shift key status
        shiftKey: ev.shiftKey,
        ctrlKey: ev.ctrlKey,
        altKey: ev.altKey,
        metaKey: ev.metaKey,
        
        // modifier key flag
        modifier: keyInfo && keyInfo[4],

        // define the DOM 3 property names as well
        keyIdentifier: keyInfo && keyInfo[0],
        keyLocation: keyInfo && keyInfo[2],

        // include the raw browser key code and event object
        keyCode: keyCode,
        event: ev
    };

    // for Mac IE, Command sets Ctrl, whereas it should set Meta
    if (BrowserInfo.brID == "IE" && BrowserInfo.OS == "Mac")
    {
        desc.metaKey = ev.ctrlKey;
        desc.ctrlKey = false;
    }

    // call the user function and return the result
    return func(desc, ctx);
}

/* ------------------------------------------------------------------------ */
/*
 *   Browser detector.  Call BrowserInfo.init() during page load to do the
 *   sensing work, then refer to BrowserInfo.browser for the browser ID, or
 *   use the various browser-specific properties to test for the respective
 *   browsers.
 *   
 *   This object identifies the specific browser, and separately lets you
 *   determine if it's based on one of the common browser core libraries
 *   (WebKit, Gecko), and if so the core library version.  The core library
 *   identification is useful because many special-case quirks are features
 *   of the library rather than the specific browser, and will apply to any
 *   browser based on that library.  Code to work around (or exploit) such a
 *   quirk is simpler and more robust if it can just check a library version
 *   rather than enumerating the individual browsers based on the library.
 *   
 *   Credits: adapted from http://www.quirksmode.org/js/detect.html.  
 */
var BrowserInfo = {
    // browser ID string, version number (as a float), and OS platform
    browser: "Unknown",
    version: 0,
    OS: "Unknown",

    // Specific browser ID properties.  We'll set the appropriate property
    // for the detected browser to its version number, as a float.
    opera: false,
    safari: false,
    firefox: false,
    ie: false,
    konqueror: false,

    // WebKit version, if this is a WebKit-based browser
    webKit: false,

    // Gecko version, if this is a Gecko-based browser
    gecko: false,

    // initialize
    init: function()
    {
        // get the UA string
        var ua = navigator.userAgent;

        // identify the browser
        this.browser = this.searchString(this.dataBrowser);

        // identify the browser version
        this.version = this.searchVersion(ua)
                       || this.searchVersion(navigator.appVersion)
                       || 0;

        // identify the OS platform
        this.OS = this.searchString(this.dataOS) || "Unknown";

        // determine if it's WebKit-based, and note the version if so
        var idx = ua.indexOf("AppleWebKit/");
        if (idx != -1)
            this.webKit = this.parseVsnFloat(ua.substr(idx + 12));

        // detremine if it's a Gecko-based browser
        idx = ua.indexOf("Gecko/"), idx2 = ua.indexOf("rv:");
        if (idx != -1 && idx2 != -1)
            this.gecko = this.parseVsnFloat(ua.substr(idx2 + 3));

        // set properties for some specific browser types
        this.opera = this.versionIf("Opera");
        this.safari = this.versionIf("Safari");
        this.firefox = this.versionIf("Firefox");
        this.ie = this.versionIf("IE");
        this.konqueror = this.versionIf("Konqueror");

        // if we're on Opera, customize the text adjustment method
        if (this.opera)
        {
            // opera doesn't render unicode typographical spaces, so replace
            // them with ordinary spaces
            this.adjustText = function(txt) {
                return txt.replace(/\u2002/g, ' ');
            };
        }
    },

    versionIf: function(browser)
    {
        // if the ID matches our detected browser, return the version number
        return (this.browser == browser ? this.version : false);
    },
    
    searchString: function(data)
    {
        // search the item list
        for (var i = 0 ; i < data.length ; i++)
        {
            var dataString = data[i].src || navigator.userAgent;
            this.versionSearchString = data[i].vsnKey || data[i].id;
            if (dataString.indexOf(data[i].key) != -1)
                return data[i].id;
        }

        // not found
        return null;
    },
    
    searchVersion: function(dataString)
    {
        var index = dataString.indexOf(this.versionSearchString);
        if (index >= 0)
            return this.parseVsnFloat(dataString.substring(
                index + this.versionSearchString.length + 1));
        else
            return 0;
    },
    
    dataBrowser: [
        { key: "Chrome", id: "Chrome" },
        { key: "OmniWeb", vsnKey: "OmniWeb/", id: "OmniWeb" },
        { src: navigator.vendor, key: "Apple", id: "Safari", vsnKey: "Version" },
        { key: "Opera", id: "Opera" },
        { src: navigator.vendor, key: "iCab", id: "iCab" },
        { src: navigator.vendor, key: "KDE", id: "Konqueror" },
        { key: "Firefox", id: "Firefox" },
        { src: navigator.vendor, key: "Camino", id: "Camino" },
        { key: "Netscape", id: "Netscape" },
        { key: "MSIE", id: "IE", vsnKey: "MSIE" },
        { key: "Gecko", id: "Gecko", vsnKey: "rv" },
        { key: "Mozilla", id: "Netscape", vsnKey: "Mozilla" }
    ],

    dataOS: [
        { src: navigator.platform, key: "Win", id: "Windows" },
        { src: navigator.platform, key: "Mac", id: "Mac" },
        { src: navigator.userAgent, key: "iPhone", id: "iPhone/iPod" },
        { src: navigator.platform, key: "Linux", id: "Linux" }
    ],

    // Parse a version number as a floating point value.  This parses a
    // dotted version number string "x.y.z.w" as though it were a floating
    // point value, effectively pulling out the "x" as the integer portion
    // and the "y" as the fractional portion, ignoring any addition elements.
    // We do one extra bit of processing, though: if "y" is only one digit
    // long, we treat it as though it were "x.0y", so that we treat version
    // "1.9" as less than "1.10".  This doesn't generalize indefinitely,
    // but it covers the 
    parseVsnFloat: function(str)
    {
        return parseFloat(str.replace(
            /^(\d+\.)(\d)([^\d]|$)/, function(m, x, y, z) {
            return x + "0" + y + z;
        }));
    },

    // perform browser-specific adjustments to text from the server
    adjustText: function(txt) { return txt; }
};

/* ------------------------------------------------------------------------ */
/*
 *   Create a combo box for insertion into a form 
 */
function createCombo(id, fldSize, options)
{
    var optCnt;
    if (typeof(options) == "string")
    {
        var m = options.match(/<option/gi);
        optCnt = m ? m.length : 0;
    }
    else
    {
        optCnt = options.length;
        options = options.map(function(ele) {
            ele = ele.htmlify();
            return "<option value=\"" + ele + "\">" + ele + "</option>";
        }).join("");
    }

    var s = "<select class=\"comboPopup\" style=\"margin: 0px; padding:0px;\" "
            +   "size=\"" + (optCnt < 5 ? 5 : optCnt < 20 ? optCnt : 20)
            +   "\" onkeydown=\"javascript:"
            +        "return $kd(event, comboSelKey, this);\" "
            +   "onkeypress=\"javascript:"
            +        "return $kp(event, comboSelKey, this);\" "
            +   "onclick=\"javascript:return comboSelClick(event, this);\" "
            +   "onchange=\"javascript:comboApplySel(event, this);\">"
            +     options
            + "</select>"

            + "<div class=\"combobox\" style=\""
            +   (BrowserInfo.webKit ? "padding: 0px 0px 2px 0px;" :
                 BrowserInfo.opera ? "padding: 0px 0px 1px 0px;" : "")
            +   "\">"

            // This is an IE bug workaround - don't ask me why it works.
            // The bug is triggered by this specific series of elements:
            // div.dialogMarginDiv, which sets the margins; then the
            // div.combobox, which is an inline block (via the IE zoom:1
            // hack), then the input as an inline element.  For whatever
            // reason, IE makes the <input> "inherit" the dialogMarginDiv's
            // left and right margins.  Just left and just right; not top
            // or bottom.  I can't find any way to override this via CSS;
            // and in fact the IE Developer Toolbar reports that the margins
            // are zeroed out as they should be, so the CSS cascade isn't
            // the problem.  The bug is pretty clearly deep in the IE layout
            // manager; my guess is that this is some block-vs-inline
            // weirdness that's making the layout manager seek the nearest
            // block-type parent and inheriting its horizontal layout.
            // There seem to be two ways to fix it: one is to wrap the
            // INPUT in a fresh DIV with zero margins; the layout seems
            // to grab that new DIV parent's margins as my theory predicts.
            // The other way is to put a &zwsp; just before the INPUT.
            // That's basically an invisible space character, and my guess
            // is that the text element is unambiguously inline, which
            // slaps IE out of its daze about what horizontal mode it's in.
            // I'm going with the &zwsp; approach because it's one less
            // condition.
            + (BrowserInfo.ie ? "&zwsp;" : "")
            // or:  + (BrowserInfo.ie ? "<div style='margin:0;'>" : "",
            // with a compensating </div> after the <input>

            +   "<input type=\"text\" size=\"" + fldSize
            +      "\" id=\"" + id + "\" style=\"margin:0px;\" "
            +      "\" onkeydown=\"javascript:"
            +         "return $kd(event, comboFldKey, this);\" "
            +      "onkeypress=\"javascript:"
            +         "return $kp(event, comboFldKey, this);\">"

            +   "<a href=\"#\" onmousedown=\"javascript:"
            +      "return comboArrowClick(event, this);\" "
            +     "onclick=\"javascript:"
            +       "return comboArrowClick(event, this);\">"
            +       "<img src=\"/webuires/comboarrow.gif\">"
            +   "</a>"

            + "</div>";

    return s;
}

// given one of the combo elements (input, select, arrow), get another
function comboGetElement(ele, target)
{
    // start at the combo wrapper DIV
    var eleToDiv = {
        "A": ele.parentNode,
        "INPUT": ele.parentNode,
        "SELECT": ele.nextSibling,
        "DIV": ele
    };
    var div = eleToDiv[ele.nodeName];

    // now traverse from the DIV to the desired other element
    var divToTarget = {
        "A": div.lastChild,
        "INPUT": div.lastChild.previousSibling,
        "SELECT": div.previousSibling,
        "DIV": div
    };
    return divToTarget[target];
}

var closeComboOnClick = null;
function comboArrowClick(ev, arrow)
{
    // get the objects
    var sel = comboGetElement(arrow, "SELECT");
    var fld = comboGetElement(arrow, "INPUT");
    var div = comboGetElement(arrow, "DIV");
    var vis = (sel.style.display == "block");
    ev = getEvent(ev);

    // if it's not already showing, display the popup
    if (!vis)
    {
        // display the popup
        var divRc = getObjectRect(div);
        moveObject(sel, divRc.x, divRc.y + divRc.height);
        sel.style.width = divRc.width + "px";
        sel.style.display = "block";
        
        // make it a spring-loaded popup
        openPopup(ev, sel, null, fld, arrow);

        // if the current field value is in the option list, select that item
        setSelectByValue(sel, fld.value);

        // don't close it on a subsequent click
        closeComboOnClick = null;
    }
    else
    {
        // already showing - on mouse down, note that we should close
        // it on the following click
        if (ev.type == "mousedown")
            closeComboOnClick = sel;
        else if (closeComboOnClick == sel)
            closePopup(sel);
    }

    // don't process the hyperlink
    preventDefault(ev);
    return false;
}

function comboSelClick(ev, sel)
{
    closePopup(sel);
    return false;
}

function comboFldKey(desc, fld)
{
    var dir = 1;
    switch (desc.keyName)
    {
    case "Up":
        dir = -1;
        // fall through...
    case "Down":
        // Up/Down in the field scan through the list elements
        // without popping up the list

        // find the current field value in the list
        var sel = comboGetElement(fld, "SELECT");
        var i = findSelectOptionByValue(sel, fld.value);

        // if we didn't find the value, start below the start of the list
        if (i == null)
            i = -1;

        // move to the next/previous option according to the arrow key
        i += dir;

        // if this is in range, apply this selection
        if (i >= 0 && i < sel.length)
        {
            // set the value, and explicitly synthesize a change event for it
            fld.value = sel.options[i].value;
            sendEvent(fld, createEvent("Event", "change"));

            // select the field contents
            setSelRangeInEle(fld);
        }

        // done with the key
        cancelBubble(desc.event);
        preventDefault(desc.event);
        return false;
    }

    // use the default handling
    return true;
}

function comboSelKey(desc, sel)
{
    switch (desc.keyName)
    {
    case "Enter":
        // enter the current selection into the field
        comboApplySel(desc.event, sel);

        // fall through to the leave-the-field handling

    case "U+0009":
        // close the popup and swallow the key
        closePopup(sel);
        cancelBubble(desc.event);
        preventDefault(desc.event);
        return false;

    default:
        // do the default handling, but bypass any enclosing handler
        cancelBubble(desc.event);
        return true;
    }
}

function comboApplySel(event, sel)
{
    // save the current <select> value in the <input> object
    var fld = comboGetElement(sel, "INPUT");
    fld.value = sel.value;

    // Synthesize an onchange event for the field.  The browser won't
    // generate a real onchange for us since it thinks we're changing
    // the field value programmatically.  But this really is a user
    // action on our aggregated combo object, so we do want the event.
    sendEvent(fld, createEvent("Event", "change"));
}

/* ------------------------------------------------------------------------ */
/*
 *   Apply CSS styles to this window.  'tab' is an object serving as a table:
 *   each property key is a CSS selector (converted to upper-case, to allow
 *   for case-insensitive matching), and each corresponding value is a list
 *   of [prop, value] entries, where 'prop' is a CSS style property name (
 *   using the javascript version of the naming convention: 'fontFamily'
 *   instead of 'font-family', for example), and 'value' is a string giving
 *   the value for that style property.  
 */
function applyCSS(tab)
{
    // run through all style sheets
    for (var i = 0, ss = document.styleSheets ; i < ss.length ; ++i)
    {
        // get the current style sheet
        var s = ss[i];

        // get the rule collection for this style sheet - note that we
        // have to try the properties "cssRules" and "rules", since the
        // name varies by browser
        var rr = s.cssRules || s.rules;

        // run through the rules in this style sheet
        for (var j = 0 ; j < rr.length ; ++j)
        {
            // get this rule
            var r = rr[j];

            // get its selector - if it has one, check it against the table
            var sel = r.selectorText;
            if (sel)
            {
                // if this selector is in our table, apply the styles from
                // the table to the rule
                var tabEntry = tab[r.selectorText.toUpperCase()];
                if (tabEntry)
                {
                    // got a match - apply each style from the table
                    tabEntry.forEach(function(e) {
                        r.style[e[0]] = e[1];
                    });
                }
            }
        }
    }
}

