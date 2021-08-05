#charset "us-ascii"
#pragma once

/*
 *   Copyright 2010 Michael J. Roberts.
 *   
 *   This file is part of TADS 3.
 *   
 *   This module defines the HTTPServer intrinsic class.  This class is used
 *   to set up a network HTTP server, which has a number of potential
 *   applications:
 *   
 *   - Run a game in a client/server configuration, so that the user
 *   interface is running in a Web browser on one machine, and the game
 *   itself is running on a separate server machine on the network.  This
 *   lets the game run on handheld devices that might be too slow to run the
 *   game directly, allows running the game without any software
 *   installation, and makes the game more portable by depending only on a
 *   browser being available rather than requiring a TADS interpreter.
 *   
 *   - Run a game in with a browser-based UI, so that it can take advantage
 *   of HTML DOM and Javascript.  Modern browsers are more powerful than HTML
 *   TADS, so it's possible to create a more elaborate and customized game UI
 *   by running in a browser.
 *   
 *   - Create a multi-user game, by running the game on a server and writing
 *   the game to accept connections from more than one player.  
 */

/*
 *   HTTP Server Object.  This implements a multi-threaded, background server
 *   that runs concurrently with the game program.  The server listens for
 *   and accepts incoming connection requests from clients, and then handles
 *   HTTP protocol transactions with connected clients.  Client requests are
 *   routed to the byte code program via network events, which the program
 *   can retrieve via the getNetEvent() function.
 *   
 *   Construction: to set up an HTTP server, simply create an HTTPServer
 *   object with 'new':
 *   
 *.     local srv = new HTTPServer(hostname, portnum?, maxUploadSize?);
 *   
 *   'hostname' is a string giving the domain name or IP address that the
 *   server will bind to for accepting connections.  For a server that
 *   accepts connections from separate client machines, this is simply the
 *   external IP address of the local machine.  (This is specified as an
 *   argument because some machines have more than one network interface, and
 *   thus have more than one IP address or domain name.)
 *   
 *   'portnum' is the TCP/IP port number wehre the server will listen for
 *   incoming connections.  If this is omitted or nil, the operating system
 *   will automatically select an available port number and assign it to the
 *   server.  Using a specific port number allows you to create a service on
 *   a "well known" port, which makes it easier for clients to find the
 *   service; but a given port can only be used by one server at a time, so
 *   using a pre-selected port number runs the risk that some other process
 *   will already be using the same port.
 *   
 *   'maxUploadSize' is the maximum size in bytes for any single request's
 *   content.  Content sizes over this limit will be rejected.  Some HTTP
 *   requests, such as POST, can include uploaded content from the client,
 *   and the HTTP protocol itself supports essentially unlimited sizes for
 *   these objects.  Uploads consume resources on the server, though, so it's
 *   often desirable to set a size limit to prevent errant or malicious
 *   clients from overwhelming the server with a very large upload.
 *   Depending on the specific function of your server, you might or might
 *   not wish to set a limit.  If you omit this argument or set it to nil,
 *   unlimited upload sizes will be allowed.  Note that this limit applies to
 *   each individual upload separately; it's not a lifetime limit for the
 *   server or for any session.
 *   
 *   Creating an HTTPServer object with 'new' automatically starts the
 *   server.  The object will create a background thread that will listen for
 *   incoming connections on the given network address and port number, so
 *   the server is active as soon as the 'new' finishes.  You can create any
 *   number of servers, as long as they have different port numbers.  When a
 *   connection request is received, the server will accept the connection
 *   and automatically create another background thread to handle requests on
 *   that connection.  Each incoming request will be forwarded to the game
 *   program to handle, via the network message queue.  
 */
intrinsic class HTTPServer 'http-server/030000': Object
{
    /*
     *   Shut down the server.  This immediately disconnects the server from
     *   its network port; no further client connections will be accepted
     *   once the server shuts down.  In addition, all of the server threads
     *   that were started by this server object will be notified to
     *   terminate. 
     *   
     *   If 'wait' is omitted or is nil, the routine sends the shutdown
     *   notification to the main server and to its server threads, then
     *   immediately returns.  This means that one or more of the server's
     *   background threads might continue to run for a while after
     *   shutdown() returns.  The main practical consideration is that the
     *   port number used by the server might not be immediately available
     *   for use by a new server object, since the port won't be closed until
     *   the server actually exits.  
     *   
     *   If 'wait' is true, this routine won't return until all of the server
     *   threads have actually terminated.
     *   
     *   The return value is true if all server threads have terminated, nil
     *   if any server threads are still running.  It's legal to call this
     *   routine repeatedly, so you can make repeated calls to shutdown(nil)
     *   to poll for completion.  This is useful if you need to wait until
     *   the server shuts down to move on to a next step, but you have other
     *   work you can perform in the meantime.  If you don't have any other
     *   work, you can avoid burning CPU time by calling shutdown(true),
     *   which waits (without consuming CPU time) for the server to exit.  
     */
    shutdown(wait?);

    /*
     *   Get the listening address.  This returns a string giving the
     *   original binding address specified when the object was constructed.
     *   This can contain either a host name or an IP address, since either
     *   form can be used in the constructor.  
     */
    getAddress();

    /*
     *   Get the listening IP address.  This returns the numerical IP address
     *   where the server is listening for connections.  
     */
    getIPAddress();

    /*
     *   Get the port number.  This returns an integer giving the TCP/IP
     *   network port number on which this server is listening for incoming
     *   connections.  Clients connect to the port by including it in the
     *   HTTP URL, after the host name.  For example, if the server is on
     *   port 10815, the client would connect to a URL of the form
     *   http://myserver.com:10815/index.htm.  
     */
    getPortNum();
}

