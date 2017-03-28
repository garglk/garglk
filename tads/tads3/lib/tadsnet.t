#charset "us-ascii"

#include <tads.h>
#include <tadsnet.h>

/* 
 *   Copyright (c) 1999, 2006 Michael J. Roberts
 *   
 *   This file is part of TADS 3
 *   
 *   This file defines classes and properties used with the tads-net
 *   intrinsic function set.  If you're using this function set, you should
 *   include this source file in your build by adding it to your project
 *   makefile.
 */

/* include the tads-net intrinsic function set interface definition */
#include <tadsnet.h>

/* ------------------------------------------------------------------------ */
/*
 *   A NetEvent instance describes an event read via the getNetEvent()
 *   function.
 *   
 *   In most cases, this base class will not be instantiated directly.
 *   getNetEvent() will always construct the appropriate subclass for the
 *   specific type of event being generated, if that subclass is defined in
 *   the game program.  However, it's possible that the game won't define
 *   all necessary subclasses.  For example, a game written for version 1
 *   of the networking package wouldn't include new subclasses added in
 *   version 2, because those subclasses weren't defined at the time the
 *   game was written.  When getNetEvent() needs to instantiate a subclass
 *   that isn't defined in the game program, it will instead create a base
 *   NetEvent object, which will simply store the subclass-specific
 *   arguments as a list.  This could be useful for debugging purposes,
 *   because it will at least let the programmer inspect the event details
 *   with the interactive debugger.  
 */
class NetEvent: object
    /*
     *   The event type.  This is a NetEvXxx value (see tadsnet.h)
     *   indicating which type of event this is.    
     */
    evType = nil

    /* 
     *   Construction.  getNetEvent() only constructs this object directly
     *   when the subclass it's looking for isn't defined in the game
     *   program.  
     */
    construct(t, [args])
    {
        evType = t;
        evArgs = args;
    }

    /*
     *   Extra event-specific arguments.  This is primarily for debugging
     *   purposes, since it's only used when getNetEvent() needs to
     *   construct a NetEvent subclass that isn't defined in the game.  In
     *   this case, the absence of a subclass definition in the game
     *   presumably means that the game isn't written to handle the type of
     *   event generated (for example, because it was written for an older
     *   interpreter version that didn't have the event type).  
     */
    evArgs = nil
;

/*
 *   Network Request Event.  This type of event occurs when a server (such
 *   as an HTTPServer object) receives a request from a network client.
 *   
 *   The evRequest member contains a request object describing the network
 *   request.  The class of this object depends on the type of server that
 *   received the request.  For example, for an HTTP server, this will be
 *   an HTTPRequest object.  To reply to the request, use the appropriate
 *   method(s) in the request object - for details, see the specific
 *   request classes for the server types you create in your program.
 */
class NetRequestEvent: NetEvent
    /* construction */
    construct(t, req)
    {
        inherited(t, req);
        evRequest = req;
    }

    evType = NetEvRequest

    /*
     *   The request object.  When the event type is NetEvRequest, this
     *   contains a request object describing the request.  The class of
     *   the request object varies according to the server type; you can
     *   use ofKind() to check which type of request it is.  For example,
     *   for an HTTP request, this will be an object of class HTTPRequest.
     */
    evRequest = nil
;

/*
 *   Network Timeout Event.  getNetEvent() returns this type of event when
 *   the timeout interval expires before any actual event occurs.  
 */
class NetTimeoutEvent: NetEvent
    evType = NetEvTimeout
;

/*
 *   Network Reply event.  This type of event occurs when we receive a
 *   reply to a network request made with sendNetRequest().
 */
class NetReplyEvent: NetEvent
    /* construction */
    construct(t, id, status, body, headers, loc)
    {
        inherited(t, id, body, headers, loc);
        statusCode = status;
        requestID = id;
        replyBody = body;
        replyHeadersRaw = headers;
        redirectLoc = loc;

        /* parse the headers into a lookup table keyed by header name */
        if (headers != nil)
        {
            /* create the lookup table */
            local ht = replyHeaders = new LookupTable();

            /* split the headers at the CR-LF separators */
            headers = headers.split('\r\n');

            /* the first line of the headers is actually the HTTP status */
            if (headers.length() > 1)
            {
                /* save the status line */
                httpStatusLine = headers[1];
                headers = headers.sublist(2);
            }
            
            /* process the rest of the headers */
            for (local h in headers)
            {
                /* split the header at the ":", and trim spaces */
                h = h.split(':', 2).mapAll(
                    { s: rexReplace('^<space>+|<space>+$', s, '') });
                
                /* 
                 *   If it looks like a header, add it to the table.  If
                 *   the header is repeated, append it to the previous
                 *   value with a comma delimiter. 
                 */
                if (h.length() == 2)
                {
                    local name = h[1].toLower(), val = h[2];
                    if (ht.isKeyPresent(name))
                        val = '<<ht[name]>>, <<val>>';
                    ht[name] = val;
                }
            }
        }
    }

    /* our default event type is NetEvReply */
    evType = NetEvReply

    /*
     *   The request identifier.  This is the ID value provided by the
     *   caller in the call to sendNetRequest(), so that the caller can
     *   relate the reply back to the corresponding request.
     */
    requestID = nil

    /*
     *   The network status code.  This is an integer value indicating
     *   whether the request was successful or failed with an error.  A
     *   negative value is a low-level TADS error indicating that the
     *   request couldn't be sent to the server, or that a network error
     *   occurred receiving the reply:
     *   
     *.    -1    - out of memory
     *.    -2    - couldn't connect to host
     *.    -3    - other network/socket error
     *.    -4    - invalid parameters
     *.    -5    - error reading the content data to send to the server
     *.    -6    - error saving the reply data received from the server
     *.    -7    - error retrieving reply headers
     *.    -8    - error starting background thread
     *.    -100  - other TADS/network error
     *   
     *   A positive value means that the network transaction itself was
     *   successful, and reflects the status information returned by the
     *   network server that handled the request.  This must be interpreted
     *   according to the protocol used to send the request:
     *   
     *   - For HTTP requests, the value is an HTTP status code.  A code in
     *   the 200 range generally indicates success, while other ranges
     *   generally indicate errors.
     */
    statusCode = nil

    /* the content body from the reply */
    replyBody = nil

    /* 
     *   the HTTP headers from the reply, as a lookup table indexed by
     *   header name 
     */
    replyHeaders = nil

    /* the HTTP status string (the first line of the headers) */
    httpStatusLine = nil

    /* 
     *   the HTTP headers from the reply, in the raw text format - this is
     *   simply a string of all the headers, separated by CR-LF (\r\n)
     *   sequences 
     */
    replyHeadersRaw = nil

    /* 
     *   Redirect location, if applicable.  By default, this will be nil
     *   whether or not a redirection took place, because sendNetRequest()
     *   normally follows redirection links transparently, returning only
     *   the final result from the final server we're redirected to.
     *   However, you can override automatic redirection with an option
     *   flag (NetReqNoRedirect) when calling sendNetRequest().  When that
     *   option is selected, the function won't follow redirection links at
     *   all, but will instead simply return the redirect information as
     *   the result from the request.  When that happens, this property is
     *   set to a string giving the target of the redirect.  You can then
     *   follow the redirect manually, if desired, by sending a new request
     *   to the target given here.
     */
    redirectLoc = nil
;

/*
 *   Network Reply Done event.  This type of event occurs when an
 *   asynchronous network reply (such as HTTPRequest.sendReplyAsync())
 *   completes.
 */
class NetReplyDoneEvent: NetEvent
    /* construction */
    construct(t, req, err, msg)
    {
        inherited(t, req, err, msg);
        requestObj = req;
        socketErr = err;
        errMsg = msg;
    }

    /* our default event type is NetEvReplyDone */
    evType = NetEvReplyDone

    /* 
     *   The object representing the request we replied to.  For HTTP
     *   requests, this is an HTTPRequest object.
     */
    requestObj = nil

    /* was the reply successfully sent? */
    isSuccessful() { return errMsg == nil; }

    /*
     *   The socket error, if any.  If the reply failed due to a network
     *   error, this contains the error number.  If no network error
     *   occurred, this is zero.
     */
    socketErr = 0

    /*
     *   Error message, if any.  If the reply failed, this contains a
     *   string with a description of the error that occurred.  If the
     *   reply was sent successfully, this is nil.
     */
    errMsg = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   A FileUpload represents a file uploaded by a network client via a
 *   protocol server, such as an HTTPServer.
 *   
 *   When your program is acting as a network server, a FileUpload object
 *   represents a file received from the client.  For example,
 *   HTTPRequest.getFormFields() returns a FileUpload object to represent
 *   each <INPUT TYPE=FILE> field in the posted form.
 *   
 *   When your program acts as a network client (via sendNetRequest), you
 *   can create use FileUpload to post file attachments to posted forms.
 */
class FileUpload: object
    construct(file, contentType, filename)
    {
        self.file = file;
        self.contentType = contentType;
        self.filename = filename;
    }

    /*
     *   The file data.
     *   
     *   When you create the FileUpload object for use with
     *   sendNetRequest() to post form data, you must use a string or
     *   ByteArray value for this property.
     *   
     *   When the FileUpload is created by HTTPRequest.getFormFields(),
     *   this property contains a File object with the uploaded content.
     *   This is open for read-only access.  If the contentType parameter
     *   is a text type ("text/html", "text/plain", etc), and the
     *   interpreter recognizes the character set parameter in the
     *   contentType, the file is in Text mode (FileModeText) with the
     *   appropriate character mapper in effect.  Otherwise, the file is in
     *   raw binary mode (FileModeRaw).  If you need the file to be opened
     *   in a different mode, you can use setFileMode() on the file to
     *   change the mode.  
     */
    file = nil

    /*
     *   The content type.  This a string giving the MIME type specified by
     *   the client with the upload.  This is the full content-type string,
     *   including any attributes, such "charset" for a text type.  This
     *   can be nil if the client doesn't specify a content-type at all.
     *   
     *   It's important to recognize that this information is supplied by
     *   the client, and is NOT validated by the protocol server.  At best
     *   you should consider it a suggestion, and at worst a malicious lie.
     *   The client could be innocently mistaken about the type, or could
     *   even be intentionally misrepresenting it.  You should always
     *   validate the actual contents, rather than relying on the client's
     *   description of the format; in particular, be careful not to assume
     *   that expected data fields are present, in the valid range, etc.
     */
    contentType = nil

    /*
     *   The client-side filename, if specified.  This is a string giving
     *   the name of the file on the client machine.  This generally has no
     *   particular meaning to the server, since we can't infer anything
     *   about the directory structure or naming conventions on an
     *   arbitrary client.  However, this might be useful for reference,
     *   such as showing information about the upload in a user interface.
     *   It's sometimes also marginally useful to know the suffix
     *   (extension) for making further guesses about the content type -
     *   although as with the content-type, you can't rely upon this, but
     *   can only use it as a suggestion from the client.
     *   
     *   The client won't necessarily specify a filename at all, in which
     *   case this will be nil.  
     */
    filename = nil
;
    

/* ------------------------------------------------------------------------ */
/*
 *   A NetException is the base class for network errors. 
 */
class NetException: Exception
    construct(msg?, errno?)
    {
        if (errMsg != nil)
            errMsg = 'Network error: <<msg>>';
        if (errno != nil)
            errMsg += ' (system error code <<errno>>)';
    }
    displayException() { "<<errMsg>>"; }

    /* a descriptive error message provided by the system */
    errMsg = 'Network error'
;

/*
 *   A NetSafetyException is thrown when the program attempts to perform a
 *   network operation that isn't allowed by the current network safety
 *   level settings.  The user controls the safety level; the program can't
 *   override this.  
 */
class NetSafetyException: NetException
    errMsg = 'Network operation prohibited by user-specified '
             + 'network safety level'
;

/*
 *   A SocketDisconnectException is thrown when attempting to read or write
 *   a network socket that's been closed, either by us or by the peer (the
 *   computer on the other end of the network connection).  If we didn't
 *   close the socket on this side, this error usually means simply that
 *   the peer program has terminated or otherwise disconnected, so we
 *   should consider the conversation terminated.  
 */
class SocketDisconnectException: NetException
    errMsg = 'Network socket disconnected by peer or closed'
;

/* export the objects and properties used in the tads-net function set */
export NetEvent 'TadsNet.NetEvent';
export NetRequestEvent 'TadsNet.NetRequestEvent';
export NetTimeoutEvent 'TadsNet.NetTimeoutEvent';
export NetReplyEvent 'TadsNet.NetReplyEvent';
export NetReplyDoneEvent 'TadsNet.NetReplyDoneEvent';
export NetException 'TadsNet.NetException';
export SocketDisconnectException 'TadsNet.SocketDisconnectException';
export NetSafetyException 'TadsNet.NetSafetyException';
export FileUpload 'TadsNet.FileUpload';
export file 'TadsNet.FileUpload.file';
export contentType 'TadsNet.FileUpload.contentType';
export filename 'TadsNet.FileUpload.filename';

