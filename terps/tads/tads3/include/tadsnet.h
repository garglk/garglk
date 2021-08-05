#charset "us-ascii"
#pragma once

/* 
 *   Copyright (c) 1999, 2006 Michael J. Roberts
 *   
 *   This file is part of TADS 3
 *   
 *   This header defines the tads-net intrinsic function set.
 *   
 *   The tads-net function set provides network input/output.  In particular,
 *   this implements an HTTP server that can be used to implement a
 *   browser-based user interface.  The network layer could also be used for
 *   other purposes, such as implementing a multi-user game server.
 *   
 *   The first step in setting up a network server is to create a suitable
 *   server object, such as an HTTPServer.  The server opens a network port
 *   on the local machine and waits for incoming connection requests from
 *   clients, such as from Web browsers on remote machines.  When a client
 *   connects to the server's port, the server creates a separate thread to
 *   handle the new connection, then goes back to listening for additional
 *   connections.  The connection thread handles network communications with
 *   the client; when the client sends a request to the server, the thread
 *   posts an event message to the network message queue.
 *   
 *   The next step for the program, then, is simply to go into a loop reading
 *   and handling incoming network messages.  Once the server is set up, the
 *   program is basically driven by the network client.  Call netEvent() to
 *   read the next message, then interpret the contents, process the message,
 *   and send a reply.  Once that's done, go back to the start of the loop to
 *   handle the next request message.  The program continues running until a
 *   suitable network event occurs to terminate it, such as the user typing
 *   QUIT into the UI.  
 */


/*
 *   tads-net - the TADS Network function set
 */
intrinsic 'tads-net/030001'
{
    /*
     *   Connect to the Web UI client.  This connects the Web browser client
     *   to the game's HTTP server.  'server' is the HTTPServer object where
     *   the Web UI is found, and 'path' is the URL path of the game's main
     *   UI start page.  This will tell the client to navigate to the given
     *   start path on the given server.
     *   
     *   Web-based games must call this after starting the HTTP server that
     *   provides the game's Web UI.  This should be called as quickly as
     *   possible after starting up, because clients might time out and miss
     *   the connection data if it takes too long to get this back to them.  
     */
    connectWebUI(server, path);

    /*
     *   Read an event from the network message queue.  When a listener
     *   receives a connection request from a client, it creates a network
     *   server thread to handle the connection; then when the server thread
     *   receives data from the client, it packages the data into an event
     *   message and places it in the queue.  This function retrieves the
     *   next message from the queue.
     *   
     *   If 'timeout' is omitted or nil, the function waits indefinitely; it
     *   doesn't return until a network event occurs.  If a timeout is given,
     *   it gives the maximum time in milliseconds that the function should
     *   wait for an event; if the timeout expires before any events arrive,
     *   the function returns a timeout event.
     *   
     *   The return value is a NetEvent instance describing the event.  
     */
    getNetEvent(timeout?);

    /*
     *   Get the local network host name.  This is the name (or a name) that
     *   other computers can use to connect to this computer across the
     *   network.
     *   
     *   Some computers have multiple host names, since a single machine can
     *   have more than one network adapter.  If this is the case, this
     *   function returns the default host name if there is such a concept,
     *   otherwise it just picks one of the names arbitrarily.
     *   
     *   If there's no host name at all, this returns nil.
     *   
     *   The host name returned here isn't usually a "global" name for the
     *   computer, but is a name that the local operating system recognizes
     *   as itself for networking purposes.  In particular, this usually
     *   won't be a full internet "x.y.com" domain name.  This name thus
     *   isn't something you can advertise to arbitrary other machines across
     *   the Internet and expect them to be able to connect.  Instead, this
     *   name is primarily useful for telling the operating system which
     *   network adapter to use when opening a listening port.  For example,
     *   you can use this name in the HTTPServer constructor.  
     */
    getHostName();

    /*
     *   Get the local host's IP address.  This is the IP address (or an IP
     *   address) that other computers can use to connect to this computer
     *   via the network.  IP addresses are the basic addressing scheme of
     *   the Internet.
     *   
     *   Some computers have multiple IP addresses, since a single machine
     *   can have more than one network adapter.  If this is the case, this
     *   function returns the default IP address if there is one, otherwise
     *   it selects one of the machine's IP addresses arbitrarily.
     *   
     *   If the IP address can't be retrieved (for example, because the
     *   machine has no network adapter installed), this returns nil.
     */
    getLocalIP();

    /*
     *   Get the URL to the storage server.  When running in Web mode, the
     *   interpreter is generally configured to store files on a separate
     *   "storage server" rather than on the game server.  This function
     *   retrieves the interpreter's configuration data and returns the
     *   storage server URL.
     *   
     *   'resource' is a string giving the resource (page) name on the server
     *   that you wish to access.  This can contain query parameters
     *   (introduced by a "?"  symbol), if desired.  The return value is the
     *   full URL to the requested page on the server.
     */
    getNetStorageURL(resource);

    /*
     *   Get the host address that the user used to launch the game.  For
     *   standard client/server TADS Web play, this is the network address
     *   that you should specify as the listening address when setting up the
     *   HTTPServer object.
     *   
     *   When we're operating in client/server mode, the interpreter is
     *   launched by an external Web server, in response to an incoming
     *   request from a client.  In order to establish our own connection
     *   with the client, we need to know the address that the client used to
     *   connect to the external Web server in the first place.  The Web
     *   server passes this address to the interpreter as part of the launch
     *   information, and the interpreter makes the address available to the
     *   TADS program here.
     *   
     *   If there's no launch address, this returns nil.  This means that the
     *   user launched the interpreter directly, from the local desktop or
     *   command shell.  In this case, it means that the user wants to run
     *   the game locally, rather than from a remote client machine.  Simply
     *   use "localhost" as the networking binding address in this case.  
     */
    getLaunchHostAddr();

    /*
     *   Send a network request to a remote server.  This initiates
     *   processing the request and immediately returns; the process of
     *   setting up the network connection to the remote server, sending the
     *   request data, and receiving the reply proceeds asynchronously while
     *   the program continues running.  When the request completes (or fails
     *   due to an error), a NetEvent of type NetEvReply is queued.
     *   
     *   'id' is a user-defined identifier for the request.  This can be any
     *   value, but is typically an object that you create to keep track of
     *   the request and process the reply.  TADS doesn't use this value
     *   itself, but simply hangs onto it while the request is being
     *   processed, and then stores it in the NetEvent object generated when
     *   the request completes.  This lets you relate the reply event back to
     *   the request, so that you know which request it applies to.
     *   
     *   'url' is a string giving the URL of the resource.  This starts with
     *   a protocol name that specifies which protocol to use.  The possible
     *   protocols are:
     *   
     *   - HTTP: the URL has the form 'http://server:port/resource'.  It can
     *   also start with 'https://' for a secure HTTP connection.  The port
     *   number is optional; if omitted, the default port is 80 for regular
     *   HTTP, or 443 for HTTPS.
     *   
     *   Currently, the only protocol supported is HTTP.  An error occurs if
     *   another protocol is specified.
     *   
     *   Additional parameters depend on the protocol.
     *   
     *   HTTP Additional Parameters:
     *   
     *   'verb' - a string giving the HTTP verb for the request (GET, POST,
     *   HEAD, PUT, etc).
     *   
     *   'options' - optional; a bitwise combination of NetReqXXX option
     *   flags specifying special settings.  Omit this argument or pass 0 to
     *   use the default settings.
     *   
     *   'headers' - optional; a string giving any custom headers to include
     *   with the request.  The system automatically generates any required
     *   headers for the type of request, but you can add your own custom
     *   headers with this parameter.  The headers must be specified using
     *   the standard 'name: value' format.  Use '\r\n' to separate multiple
     *   headers.  If you don't need to specify any custom headers, pass nil
     *   or simply omit this argument.
     *   
     *   'body' - optional; a string or ByteArray giving the content body of
     *   the request, if any.  This is suitable for verbs such as PUT and
     *   POST.  For verbs that don't send any content with the request, pass
     *   nil or simply omit the argument.
     *   
     *   'bodyType' - optional; a string giving the MIME type of the content
     *   body.  If this is omitted, a default MIME type is assumed according
     *   to the type of 'body': for a string, "text/plain; charset=utf-8";
     *   for a ByteArray, "appliation/octet-stream".
     *   
     *   This routine has no return value, since the request is processed
     *   asynchronously.  The result can't be determined until the
     *   corresponding NetEvReqReply event occurs, at which point you can
     *   inspect that NetEvent object to find out if the request was
     *   successful, and if so retrieve the reply data.
     */
    sendNetRequest(id, url, ...);
}

/* ------------------------------------------------------------------------ */
/*
 *   sendNetRequest option flags for HTTP requests.
 */

/* 
 *   DO NOT follow "redirect" (301) results.  By default (i.e., without this
 *   option flag), if the server sends back a 301 HTTP status code
 *   ("permanently moved"), we'll automatically follow the link to the new
 *   location and return the result from that new request.  This is
 *   transparent to the caller; the caller just sees the final result from
 *   the final server we're redirected to.  If this flag is specified, we
 *   DON'T follow a redirection, but instead simply stop and return the 301
 *   result.  The caller can inspect the reply headers to get the redirection
 *   link.
 */
#define NetReqNoRedirect    0x0001


/* ------------------------------------------------------------------------ */
/*
 *   Network event types.  This value is used in the evType property of
 *   NetEvent objects to indicate the type of the event. 
 */

/* 
 *   Request event.  This type of event contains a request from the network
 *   client.  For example, for an HTTP client, the event contains an HTTP
 *   request, such as a GET.  The request is represented as an object; the
 *   class depends on the type of server and request.  
 */
#define NetEvRequest     1

/* 
 *   Timeout.  netEvent() returns this type of event when the interval
 *   expires before any actual network events occur.  
 */
#define NetEvTimeout     2

/*
 *   Debugger interrupt.  netEvent() returns this type of event when the user
 *   commands the debugger to interrupt the program and take control.  If the
 *   program is waiting for a network event, this causes the netEvent() call
 *   to return with this type of event.
 *   
 *   In a normal event-loop type of program, you can simply ignore this event
 *   and loop back for a new event immediately.  The purpose of this event
 *   type is to force netEvent() to stop waiting and return to the byte-code
 *   caller, so that the debugger can pause execution at a valid byte-code
 *   program location.  Once that's accomplished, there's nothing more to do
 *   with this event type; simply discard it and get the next event when the
 *   user resumes execution.  
 */
#define NetEvDebugBreak  3

/*
 *   UI Closed.  This type of event occurs when the user manually closes the
 *   Web UI, ONLY in a stand-alone local configuration.  This is the special
 *   configuration where the browser UI is integrated into the interpreter
 *   application, simulating the traditional TADS interpreter environment
 *   with the Web UI, and everything's running on a single machine.  When the
 *   user closes the Web UI in this environment, the game should usually just
 *   terminate, since the user has effectively dismissed the application.
 *   
 *   In the true client/server configuration, where the user is running an
 *   ordinary Web browser on a separate machine, we have no way of knowing
 *   that the user has closed the browser window.  The best we can detect is
 *   a dropped network connection, but we can't assume this is due to the
 *   window closing, because it could happen for other reasons (such as a
 *   temporary network interruption).  In the full client/server
 *   configuration, we have to be more subtle in determining that the user
 *   intends to quit the application, generally with inactivity timers.  
 */
#define NetEvUIClose     4

/*
 *   Network reply.  This type of event occurs when a network request
 *   initiated by sendNetRequest() completes, or fails with an error.  The
 *   event object contains the status of the request and, if successful, the
 *   result information sent back from the server.
 */
#define NetEvReply   5

/*
 *   Reply data transfer finished.  This type of event occurs when an
 *   asynchronous reply to a request is completed.  An asynchronous HTTP
 *   request reply can be sent with HTTPRequest.sendReplyAsync().  The event
 *   object contains information on whether or not the reply was successfully
 *   sent to the client.  For HTTP transactions, this is largely advisory,
 *   since there's no recovery action the server can take when an error
 *   occurs sending a reply; there's no provision in HTTP for a server to
 *   initiate contact with a client, so it's up to the client to handle it,
 *   such as by retrying the request.
 */
#define NetEvReplyDone    6

