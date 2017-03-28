#charset "us-ascii"

/* 
 *   Copyright (c) 2010 Michael J. Roberts.  All Rights Reserved.
 *   
 *   This module defines some useful helper functions for implementing a
 *   TADS game with a Web-based user interface.  
 */

#include <tads.h>
#include <tadsnet.h>
#include <httpsrv.h>
#include <httpreq.h>
#include <strbuf.h>
#include <file.h>

/* ------------------------------------------------------------------------ */
/* 
 *   write a message to the system debug log
 */
#define DbgMsg(msg) t3DebugTrace(T3DebugLog, msg)


/* ------------------------------------------------------------------------ */
/*
 *   Session timeout settings.  Times are in milliseconds.
 */

/* 
 *   Housekeeping interval.  We'll wait at least this long between
 *   housekeeping passes.  
 */
#define HousekeepingInterval  15000

/*
 *   Session startup timeout.  When we first start up, we'll give our first
 *   client this long to establish a connection.  If we don't hear anything
 *   from a client within this interval, we'll assume that the client
 *   exited before it had a chance to set up its first connection, so we'll
 *   terminate the server.
 */
#define SessionStartupTimeout  90000

/*
 *   Ongoing session timeout.  After we've received our first connection,
 *   we'll terminate the server if we go this long without any active
 *   connections.  
 */
#define SessionTimeout  120000

/*
 *   Event request timeout.  After an event request from a client has been
 *   sitting in the queue for longer than this limit, we'll send a "keep
 *   alive" reply to let the client know that we're still here.  The client
 *   will just turn around and send a new request.  This periodic handshake
 *   lets the client know we're still alive, and vice versa.  
 */
#define EventRequestTimeout  90000

/*
 *   Client session timeout.  If we haven't seen a request from a given
 *   client within this interval, we'll assume that the client has
 *   disconnected, and we'll drop the client session state on this end.  
 */
#define ClientSessionTimeout 60000



/* ------------------------------------------------------------------------ */
/*
 *   Some handy Vector extensions
 */
modify Vector
    push(ele) { append(ele); }
    pop() { return getAndRemove(length()); }
    shift() { return getAndRemove(1); }

    getAndRemove(idx)
    {
        /* get the element */
        local ret = self[idx];

        /* remove it */
        removeElementAt(idx);

        /* return the popped element */
        return ret;
    }

    clear()
    {
        if (length() > 0)
            removeRange(1, length());
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   HTTPRequest extensions 
 */
modify HTTPRequest
    /*
     *   Send a reply, catching "socket disconnect" exceptions.  In most
     *   cases, server objects will want to use this method rather than the
     *   native sendReply() so that they won't have to handle disconnect
     *   exceptions manually.
     *   
     *   Disconnect exceptions are common with HTTP: the protocol is
     *   stateless by design, so clients can close their sockets at any
     *   time.  Most modern browsers use HTTP 1.1, which allows the client
     *   to maintain an open socket indefinitely and reuse it serially for
     *   multiple requests, but browsers tend to close these reusable
     *   sockets after a few minutes of inactivity.
     *   
     *   When this routine encounters a disconnected socket, it deletes the
     *   client session record for the request, and then otherwise ignores
     *   the error.  This generally makes the client's socket management
     *   transparent to the server, since if the client is still running
     *   they'll just connect again with a new socket and retry any lost
     *   requests.  
     */
    sendReplyCatch(reply, contType?, stat?, headers?)
    {
        /* catch any errors */
        try
        {
            /* try sending the reply */
            sendReply(reply, contType, stat, headers);
        }
        catch (SocketDisconnectException sde)
        {
            /* notify the associated client session of the disconnect */
            local client = ClientSession.find(self);
            if (client != nil)
                client.checkDisconnect();
        }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Web UI Session object.  This keeps track of miscellaneous items
 *   associated with the game session.  
 */
transient webSession: object
    /*
     *   Get the full URL to the given resource.
     */
    getFullUrl(resname)
    {
        return 'http://<<server.getAddress()>>:<<
            server.getPortNum()>><<resname>>';
    }

    /*
     *   Connect to the UI.  By default, we ask the webMainWin object to
     *   establish a connection, and we save the server object internally
     *   for future reference.  
     */
    connectUI(srv)
    {
        /* connect the UI to the main window object */
        webMainWin.connectUI(srv);

        /* save a reference to the server object */
        server = srv;

        /* 
         *   set the zero point for the connection time - for timeout
         *   purposes, the countdown starts now, since we obviously can't
         *   have a connection before this point 
         */
        lastClientTime = getTime(GetTimeTicks);
    }

    /* 
     *   The session key.  This identifies the server as a whole, and is
     *   essentially an authentication mechanism that lets clients prove
     *   they got our address from an authorized source (rather than just
     *   stumbling across it via a port scan, say).  Clients must hand this
     *   to us on each request, either via a URL query parameter or via a
     *   cookie.  The normal setup (via WebResourceInit) is for the client
     *   to send us the key as a URL parameter on the initial request, at
     *   which point we'll pass it back as a set-cookie, removing the need
     *   for the client to include the key in subsequent URLs.
     *   
     *   The key is just a random number that's long enough that an
     *   interloper couldn't hope to guess it.  We generate this on the
     *   first evaluation, and it remains fixed at that point for as long
     *   as we're running.  
     */
    sessionKey = (sessionKey = generateRandomKey())

    /*
     *   The collaborative session key.  This is a secondary session key
     *   that allows additional users to connect to the session for
     *   collaborative play.  
     */
    collabKey = (collabKey = generateRandomKey())

    /*
     *   Validate a session key sent from the client 
     */
    validateKey(req, query)
    {
        /* get the key, either from the query string or from a cookie */
        local rkey = query['TADS_session'];
        if (rkey == nil)
            rkey = req.getCookie('TADS_session');

        /* if there's no key, it's an error */
        if (rkey == nil)
        {
            /* no key -> bad request */
            req.sendReply(400);
            return nil;
        }

        /* the key has to match either the main key or the collab key */
        if (rkey != sessionKey && rkey != collabKey)
        {
            /* invalid key -> forbidden */
            req.sendReply(403);
            return nil;
        }

        /* the session key is valid */
        return true;
    }

    /* 
     *   The launcher's game ID.  This is the ID passed from the web server
     *   that launched the game, to let us know how the game is identified
     *   in the launcher database.  This is typically an IFDB TUID string.
     */
    launcherGameID = nil

    /*
     *   The launcher's user name.  This is passed from the web server that
     *   launched the game, to let us know the host user's screen name.  We
     *   use this as the user's default screen name in multi-user games.  
     */
    launcherUsername = 'Host'

    /* 
     *   The primary storage server session ID, for the user who launched
     *   the server.  If the user who launched the game logged in to a
     *   cloud storage server, this is the session ID that we use to
     *   transact business with the server on behalf of this logged-in
     *   user.  This token identifies and authenticates the user, but it's
     *   ephemeral and it's only valid for the current game server session,
     *   so it's not quite like a password.  This is the session for the
     *   launch user only; if other collaborative users join, they can get
     *   their own session IDs that will allow them to store files under
     *   their own private user folders on the server.  
     */
    storageSID = nil

    /*
     *   Get the collaborative player launch URL.  This is a URL that the
     *   host can send to other players who wish to join the session as
     *   collaborative users.
     */
    getCollabUrl()
    {
        /* return the main window URL */
        return webSession.getFullUrl(
            '<<webMainWin.vpath>>?TADS_session=<<collabKey>>');
    }

    /* list of active client sessions (ClientSession objects) */
    clientSessions = static new transient Vector()

    /* add a client session */
    addClient(s)
    {
        clientSessions.append(s);
        lastClientTime = getTime(GetTimeTicks);
        everHadClient = true;
    }

    /* remove a client session */
    removeClient(s)
    {
        clientSessions.removeElement(s);
        lastClientTime = getTime(GetTimeTicks);
    }

    /* the HTTPServer object running our web session */
    server = nil

    /*
     *   Run housekeeping tasks.  The network event processor calls this
     *   periodically to let us perform background cleanup tasks.  Returns
     *   the system tick time of the next housekeeping run.  
     */
    housekeeping()
    {
        /* 
         *   if it hasn't been long enough yet since the last housekeeping
         *   run, skip this 
         */
        if (getTime(GetTimeTicks) < hkTime)
            return hkTime;

        /* send keep-alives and check for dead client sessions */
        clientSessions.forEach(function(s)
        {
            /* send keep-alives to aged-out event requests */
            s.sendKeepAlive();

            /* check to see if the client has disconnected */
            s.checkDisconnect();
        });

        /* if there are no client sessions, check for server termination */
        if (clientSessions.length() == 0)
        {
            /* 
             *   There are no clients connected.  If we've exceeded the
             *   maximum interval for running without a client, shut down
             *   the server.  Use the initial connection time limit if
             *   we've never had a client, otherwise use the ongoing idle
             *   time limit.  
             */
            local limit = everHadClient
                ? SessionTimeout : SessionStartupTimeout;

            if (getTime(GetTimeTicks) > lastClientTime + limit)
            {
                /* no clients - shut down */
                throw new QuittingException();
            }
        }
        else
        {
            /* we have a session as of right now */
            lastClientTime = getTime(GetTimeTicks);
        }

        /* update the housekeeping timer */
        return hkTime = getTime(GetTimeTicks) + HousekeepingInterval;
    }

    /* system time (ms ticks) of next scheduled housekeeping pass */
    hkTime = 0

    /* the last time we noticed that we had a client connected */
    lastClientTime = 0

    /* have we ever had a client connection? */
    everHadClient = nil
;

/*
 *   Generate a random key.  This returns a 128-bit random number as a hex
 *   string.  This is designed for ephemeral identifiers, such as session
 *   keys.  
 */
generateRandomKey()
{
    /* generate a long random hex string */
    return rand('xxxxxxxx%-xxxx%-xxxx%-xxxxxxxxxxxx%-xxxx');
}

/* ------------------------------------------------------------------------ */
/*
 *   Client session.  This represents a connection to one browser (or other
 *   client application).  Each browser client is a separate session, so we
 *   create one instance of this class per connected browser.  Note that
 *   browser instances don't necessarily represent different users - a
 *   single user could open multiple browser windows on the same server.
 *   
 *   We identify each browser instance via a session cookie, which we
 *   establish when the client connects.  The browser sends the cookie with
 *   each subsequent request, allowing us to tie the request to the browser
 *   session we previously set up.  
 */
class ClientSession: object
    construct(skey, ssid)
    {
        /* remember my session key and storage server SID */
        storageSID = ssid;

        /* note if we're a secondary (collaborative play) user */
        isPrimary = (skey == webSession.sessionKey);
        
        /* add me to the master list of sessions */
        webSession.addClient(self);

        /* note our last activity time */
        updateEventTime();

        /* create a UI preferences object */
        uiPrefs = new WebUIPrefs(self);
    }

    /* the UI preferences object for this session */
    uiPrefs = nil

    /* 
     *   The client's "screen name" - this is the user-visible name that
     *   we'll show other users to identify commands and chat messages
     *   entered by this client.
     */
    screenName = ''

    /* set the default screen name for a client */
    setDefaultScreenName()
    {
        /* 
         *   If this is the primary, the name is the launching user's
         *   screen name.  If not, it's 'Guest N', where N is the number of
         *   guest connections we have.  
         */
        screenName = (isPrimary ? webSession.launcherUsername :
                      'Guest <<webSession.clientSessions.countWhich(
                          {x: !x.isPrimary})>>');
    }

    /* the client's IFDB user ID (a "TUID"), if logged in to IFDB */
    ifdbTuid = nil

    /* the list of pending event requests from this client */
    pendingReqs = perInstance(new Vector())

    /*
     *   The client's event queue.  When a server-to-client event occurs,
     *   we post it to each current client's queue.  When the client sends
     *   a get-event request, we satisfy it out of this queue.  
     */
    pendingEvts = perInstance(new Vector())

    /*
     *   The client session key.  This identifies the client across
     *   requests.  We send this to the client as a cookie when they
     *   connect, so we get it back on each request.  
     */
    clientKey = perInstance(generateRandomKey())

    /* 
     *   The storage server session key for the user connected to this
     *   session, if any.  We can have multiple users logged in to the game
     *   in collaborative play mode, each with their own separate storage
     *   server session.  This allows each user to have their own private
     *   preference settings, saved games, etc.  
     */
    storageSID = nil

    /*
     *   Am I the primary player?  This is true if the player connected
     *   using the primary session key.  Collaborative players join through
     *   the separate collaborative session key.  
     */
    isPrimary = nil

    /*
     *   Is this session alive?  When we detect that the client has
     *   disconnected, we'll set this to nil.  When waiting for a client in
     *   a modal event loop, this can be used to terminate the wait if the
     *   client disconnects.  
     */
    isAlive = true

    /* 
     *   Last request time, in system ticks (ms).  We use this to determine
     *   how long it's been since we've heard from the client, for timeout
     *   purposes.  This is updated any time we receive a command or event
     *   request from the client, and each time we successfully send an
     *   event reply.  
     */
    lastEventTime = 0

    /* update the last event time for this client */
    updateEventTime() { lastEventTime = getTime(GetTimeTicks); }

    /* class method: broadcast an event message to all connected clients */
    broadcastEvent(msg)
    {
        /* send the event to each client in our active list */
        webSession.clientSessions.forEach({ c: c.sendEvent(msg) });
    }

    /* send an event to this client */
    sendEvent(msg)
    {
        /* enqueue the event, then match it to a request if possible */
        pendingEvts.push(msg);
        processQueues();
    }

    /* receive an event request from the client */
    requestEvent(req)
    {
        /* enqueue the request, then match it to an event if possible */
        pendingReqs.push(new ClientEventRequest(req));
        processQueues();
    }

    /* flush outstanding events for this client */
    flushEvents()
    {
        /* discard any queued events */
        pendingEvts.clear();

        /* send no-op replies to any pending event requests */
        while (pendingReqs.length() > 0)
        {
            /* pull out the first request */
            local req = pendingReqs.shift();

            /* 
             *   Send a reply.  Ignore any errors: the client probably
             *   canceled their side of the socket already, so we'll
             *   probably get a socket error sending the reply.  Since
             *   we're just canceling the request anyway, this is fine. 
             */
            try
            {
                /* send a no-op reply */
                req.req.sendReply('<?xml version="1.0"?><noOp></noOp>',
                                  'text/xml', 200);

                /* update the successful communications time */
                updateEventTime();
            }
            catch (Exception exc)
            {
                /* 
                 *   ignore errors - the client probably canceled their
                 *   side of the socket already, so we'll probably fail
                 *   sending the reply 
                 */
            }
        }
    }

    /* 
     *   Send a keep-alive reply to each pending request from this client
     *   that's been waiting for longer than the timeout interval. 
     *   
     *   Javascript clients in principle will wait indefinitely for an
     *   XmlHttpRequest to complete, but in practice browsers tend to set
     *   fairly long but finite time limits.  If the time limit is exceeded
     *   for a request, the client will fail the request with an error.  To
     *   prevent this, our main event loop (processNetRequests)
     *   periodically calls this routine if no other events have occurred
     *   recently.  We'll clear out the pending event request queue for
     *   each client by sending a no-op reply to each event.  This tells
     *   the client that the server is still alive and connected but has
     *   nothing new to report.  
     */
    sendKeepAlive()
    {
        /* 
         *   Send no-op replies to any requests that have been in the queue
         *   for longer than the maximum event wait interval.  New requests
         *   are added to the end of the queue, so the first item in the
         *   queue is the oldest.  
         */
        local t = getTime(GetTimeTicks);
        while (pendingReqs.length() > 0 && t >= pendingReqs[1].reqTimeout)
        {
            /* pull out the oldest request */
            local req = pendingReqs.shift();

            try
            {
                /* send a no-op reply */
                req.req.sendReply(
                    '<?xml version="1.0"?><event><keepAlive/></event>',
                    'text/xml', 200,
                    ['Cache-control: no-store, no-cache, '
                     + 'must-revalidate, post-check=0, pre-check=0',
                     'Pragma: no-cache',
                     'Expires: Mon, 26 Jul 1997 05:00:00 GMT']);

                /* successful communications, so note the last up time */
                updateEventTime();
            }
            catch (Exception exc)
            {
                /* couldn't send the reply - consider disconnecting */
                checkDisconnect();
            }
        }
    }

    /* broadcast a downloadable file to all clients */
    broadcastDownload(desc)
    {
        /* add the download to all clients */
        webSession.clientSessions.forEach({c: c.addDownload(desc)});
    }

    /* add a download to this client */
    addDownload(desc)
    {
        /* add the download to the table */
        downloads[desc.resName] = desc;
    }

    /* 
     *   Cancel a downloadable file.  Removes the file from the download
     *   list and notifies the client that the file is no longer available.
     */
    cancelDownload(desc)
    {
        /* remove the file from our table */
        downloads.removeElement(desc.resName);

        /* tell the client that the file is no longer available */
        webMainWin.sendWinEventTo(
            '<cancelDownload><<desc.resPath.htmlify()>></cancelDownload>',
            self);
    }

    /* this client's list of downloadable temporary files */
    downloads = perInstance(new LookupTable())

    /* get a list of all of my downloadable files */
    allDownloads() { return downloads.valsToList(); }

    /* process the request and response queues */
    processQueues()
    {
        /* keep going as long as we have requests and responses to pair up */
        while (pendingReqs.length() > 0 && pendingEvts.length() > 0)
        {
            /* pull the first element out of each list */
            local req = pendingReqs.shift();
            local evt = pendingEvts[1];

            /* 
             *   Answer the request with the event.  Since this is an API
             *   request masquerading as a page view, we need to generate a
             *   live reply to each new request to the same virtual
             *   resource path.  So, include some headers to tell the
             *   browser and any proxies not to cache the reply.  
             */
            try
            {
                /* send the reply */
                req.req.sendReply(
                    evt, 'text/xml', 200,
                    ['Cache-control: no-store, no-cache, '
                     + 'must-revalidate, post-check=0, pre-check=0',
                     'Pragma: no-cache',
                     'Expires: Mon, 26 Jul 1997 05:00:00 GMT']);

                /* we've successfully sent the event - discard it */
                pendingEvts.shift();

                /* update the successful communications time */
                updateEventTime();
            }
            catch (SocketDisconnectException sde)
            {
                /* the socket has been disconnected - consider disconnecting */
                checkDisconnect();
            }
            catch (Exception exc)
            {
                /*
                 *   Ignore other errors, since we can just turn around and
                 *   send the event again in response to the next event
                 *   request from the client - no information will be lost,
                 *   since we still have the event in the queue.  
                 */
            }
        }
    }

    /* wait for the queues to empty in preparation for shutting down */
    shutdownWait(timeout)
    {
        /* process network requests until all clients disconnect */
        processNetRequests({: webSession.clientSessions.length() == 0 },
                           timeout);
    }

    /*
     *   Check to see if the client is still alive.  If the client has no
     *   pending event requests, and we haven't heard from the client in
     *   more than the client session timeout interval, assume the client
     *   is no longer connected and kill the session object.
     *   
     *   This should be called whenever a sending a reply to a request
     *   fails with a Socket Disconnect exception.  We also run this
     *   periodically during routine housekeeping to check for clients that
     *   haven't even bothered to send a request.  
     */
    checkDisconnect()
    {
        /* if we don't have any pending requests, kill the session */
        if (pendingReqs.length() == 0
            && getTime(GetTimeTicks) >= lastEventTime + ClientSessionTimeout)
        {
            /* remove the client session from our master list */
            webSession.removeClient(self);

            /* the session is now dead */
            isAlive = nil;
        }
    }

    /*
     *   Class method: forcibly disconnect all clients.  This simply
     *   deletes the list of active clients and deletes any pending events
     *   in their queues.  This doesn't actually terminate their network
     *   connections, but simply clears out any pending work for each
     *   client that we've initiated on the server side.  
     */
    disconnectAll()
    {
        /* delete all pending events for each client */
        webSession.clientSessions.forEach({ s: s.pendingReqs.clear() });

        /* delete the client list */
        webSession.clientSessions.clear();
    }

    /* 
     *   Class method: Find a client session, given the session key or an
     *   HTTPRequest object.  
     */
    find(key)
    {
        /* 
         *   if the key is given as an HTTPRequest, find the key by URL
         *   parameter or cookie 
         */
        if (dataType(key) == TypeObject && key.ofKind(HTTPRequest))
        {
            /* get the request */
            local req = key;

            /* try the URL parameters and cookies */
            if ((key = req.getQueryParam('TADS_client')) == nil
                && (key = req.getCookie('TADS_client')) == nil)
            {
                /* there's no client key, so there's no client */
                return nil;
            }
        }

        /* find the client that matches the key string */
        return webSession.clientSessions.valWhich({ x: x.clientKey == key });
    }

    /*
     *   Get the primary session.  This is the session for the original
     *   initiating user (the "host" in a multi-user game).  
     */
    getPrimary()
    {
        /* scan for a session with the host session key */
        return webSession.clientSessions.valWhich({ x: x.isPrimary });
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Client event request.  Each client session object keeps a queue of
 *   pending event requests, representing incoming "GET /webui/getEvent"
 *   requests that have yet to be answered.
 */
class ClientEventRequest: object
    construct(req)
    {
        self.req = req;
        reqTimeout = getTime(GetTimeTicks) + EventRequestTimeout;
    }

    /* the underlying HTTPRequest object */
    req = nil

    /* 
     *   The system time (ms ticks) when the request times out.  If we
     *   don't have an actual event to send in response by this time, the
     *   housekeeper will generate a no-op reply just to let the client
     *   know that we're still here.  
     */
    reqTimeout = 0
;



/* ------------------------------------------------------------------------ */
/*
 *   A WebResource is a virtual file accessible via the HTTP server.  Each
 *   resource object has a path, which can be given as a simple string that
 *   must be matched exactly, or as a RexPattern object with a regular
 *   expression to be matched.  Each object also has a "processRequest"
 *   method, which the server invokes to answer the request when the path
 *   is matched.  
 */
class WebResource: object
    /*
     *   The virtual path to the resource.  This is the apparent URL path
     *   to this resource, as seen by the client.
     *   
     *   URL paths follow the Unix file system conventions in terms of
     *   format, but don't confuse the virtual path with an actual file
     *   system path.  The vpath doesn't have anything to do with the disk
     *   file system on the server machine or anywhere else.  That's why we
     *   call it "virtual" - it's merely the apparent location, from the
     *   client's perspective.
     *   
     *   When the server receives a request from the client, it looks at
     *   the URL sent by the client to determine which WebResource object
     *   should handle the request.  The server does this by matching the
     *   resource path portion of the URL to the virtual path of each
     *   WebResource, until it finds a WebResource that matches.  The
     *   resource path in the URL is the part of the URL following the
     *   domain, and continuing up to but not including any "?" query
     *   parameters.  The resource path always starts with a slash "/".
     *   For example, for the URL "http://192.168.1.15/test/path?param=1",
     *   the resource path would be "/test/path".
     *   
     *   The virtual path can be given as a string or as a RexPattern.  If
     *   it's a string, a URL resource path must match the virtual path
     *   exactly, including upper/lower case.  If the virtual path is given
     *   as a RexPattern, the URL resource path will be matched to the
     *   pattern with the usual regular expression rules.  
     */
    vpath = ''

    /*
     *   Process the request.  This is invoked when we determine that this
     *   is the highest priority resource object matching the request.
     *   'req' is the HTTPRequest object; 'query' is the parsed query data
     *   as returned by req.parseQuery().  The query information is
     *   provided for convenience, in case the result depends on the query
     *   parameters.  
     */
    processRequest(req, query)
    {
        /* by default, just send an empty HTML page */
        req.sendReply('<html><title>TADS</title></html>', 'text/html', 200);
    }

    /*
     *   The priority of this resource.  If the path is given as a regular
     *   expression, a given request might match more than one resource.
     *   In such cases, the matching resource with the highest priority is
     *   the one that's actually used to process the request.  
     */
    priority = 100

    /*
     *   The group this resource is part of.  This is the object that
     *   "contains" the resource, via its 'contents' property; any object
     *   will work here, since it's just a place to put the contents list
     *   for the resource group.
     *   
     *   By default, we put all resources into the mainWebGroup object.
     *   
     *   The point of the group is to allow different servers to use
     *   different sets of resources, or to allow one server to use
     *   different resource sets under different circumstances.  When a
     *   server processes a request, it does so by looking through the
     *   'contents' list for a group of its choice.  
     */
    group = mainWebGroup

    /*
     *   Determine if this resource matches the given request.  'query' is
     *   the parsed query from the request, as returned by
     *   req.parseQuery().  'req' is the HTTPRequest object representing
     *   the request; you can use this to extract more information from the
     *   request, such as cookies or the client's network address.
     *   
     *   This method returns true if the request matches this resource, nil
     *   if not.
     *   
     *   You can override this to specify more complex matching rules than
     *   you could achieve just by specifying the path string or
     *   RexPattern.  For example, you could make the request conditional
     *   on the time of day, past request history, cookies in the request,
     *   parameters, etc.  
     */
    matchRequest(query, req)
    {
        /* get the query path */
        local qpath = query[1];

        /* by default, we match GET */
        local verb = req.getVerb().toUpper();
        if (verb != 'GET' && verb != 'POST')
            return nil;

        /* if the virtual path a string, simply match the string exactly */
        if (dataType(vpath) == TypeSString)
            return vpath == qpath;

        /* if it's a regular expression, match the pattern */
        if (dataType(vpath) == TypeObject && vpath.ofKind(RexPattern))
            return rexMatch(vpath, qpath) != nil;

        /* we can't match other path types */
        return nil;
    }

    /*
     *   Send a generic request acknowledgment or reply.  This wraps the
     *   given XML fragment in an XML document with the root type given by
     *   the last element in our path name.  If the 'xml' value is omitted,
     *   we send "<ok/>" by default.  
     */
    sendAck(req, xml = '<ok/>')
    {
        /* 
         *   Figure the XML document root element.  If we have a non-empty
         *   path, use the last element of the path (as delimited by '/'
         *   characters).  Otherwise, use a default root of <reply>.  
         */
        local root = 'reply';
        if (dataType(vpath) == TypeSString
            && vpath.length() > 0
            && rexSearch(vpath, '/([^/]+)$') != nil)
            root = rexGroup(1)[3];
        
        /* send the reply, wrapping the fragment in a proper XML document */
        sendXML(req, root, xml);
    }

    /*
     *   Send an XML reply.  This wraps the given XML fragment in an XML
     *   document with the given root element. 
     */
    sendXML(req, root, xml)
    {
        req.sendReply('<?xml version="1.0"?>\<<<root>>><<xml>></<<root>>>',
                      'text/xml', 200);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   A resource file request handler.  This handles a request by sending
 *   the contents of the resource file matching the given name.
 *   
 *   To expose a bundled game resource as a Web object that the client can
 *   access and download via HTTP, simply create an instance of this class,
 *   and set the virtual path (the vpath property) to the resource name.
 *   See coverArtResource below for an example - that object creates a URL
 *   for the Cover Art image so that the browser can download and display
 *   it.
 *   
 *   You can expose *all* bundled resources in the entire game simply by
 *   creating an object like this:
 *   
 *.     WebResourceResFile
 *.         vpath = static new RexPattern('/')
 *.     ;
 *   
 *   That creates a URL mapping that matches *any* URL path that
 *   corresponds to a bundled resource name.  The library intentionally
 *   doesn't provide an object like this by default, as a security measure;
 *   the default configuration as a rule tries to err on the side of
 *   caution, and in this case the cautious thing to do is to hide
 *   everything by default.  There's really no system-level security risk
 *   in exposing all resources, since the only files available as resources
 *   are files you explicitly bundle into the build anyway; but even so,
 *   some resources might be for internal use within the game, so we don't
 *   want to just assume that everything should be downloadable.
 *   
 *   You can also expose resources on a directory-by-directory basis,
 *   simply by specifying a longer path prefix:
 *   
 *.     WebResourceResFile
 *.         vpath = static new RexPattern('/graphics/')
 *.     ;
 *   
 *   Again, the library doesn't define anything like this by default, since
 *   we don't want to impose any assumptions about how your resources are
 *   organized.  
 */
class WebResourceResFile: WebResource
    /* 
     *   Match a request.  A resource file resource matches if we match the
     *   virtual path setting for the resource, and the requested resource
     *   file exists.  
     */
    matchRequest(query, req)
    {
        return inherited(query, req) && resExists(processName(query[1]));
    }

    /* process the request: send the resource file's contents */
    processRequest(req, query)
    {
        /* get the local resource name */
        local name = processName(query[1]);

        /* get the filename suffix (extension) */
        local ext = nil;
        if (rexSearch('%.([^.]+)$', name) != nil)
            ext = rexGroup(1)[3];

        local fp = nil;
        try
        {
            /* open the file in the appropriate mode */
            if (isTextFile(name))
                fp = File.openTextResource(name);
            else
                fp = File.openRawResource(name);
        }
        catch (FileException exc)
        {
            /* send a 404 error */
            req.sendReply(404);
            return;
        }

        /* 
         *   If the file suffix implies a particular mime type, set it.
         *   There are some media types that are significant to browsers,
         *   but which the HTTPRequest object can't infer based on the
         *   contents, so as a fallback infer the media type from the
         *   filename suffix if possible.
         */
        local mimeType = browserExtToMime[ext];

        /* 
         *   Send the file's contents.  Since resource files can be large
         *   (e.g., images or audio files), send the reply asynchronously
         *   so that we don't block other requests while the file is being
         *   downloaded.  Browsers typically download multimedia resources
         *   in background threads so that the UI remains responsive during
         *   large downloads, so we have to be prepared to handle these
         *   overlapped requests while a download proceeds.
         */
        req.sendReplyAsync(fp, mimeType);

        /* done with the file */
        fp.closeFile();
    }

    /* extension to MIME type map for important browser file types */
    browserExtToMime = static [
        'css' -> 'text/css',
        'js' -> 'text/javascript'
    ]

    /*
     *   Process the name.  This takes the path string from the query, and
     *   returns the resource file name to look for.  By default, we simply
     *   return the same name specified by the client, minus the leading
     *   '/' (since resource paths are always relative).  
     */
    processName(n) { return n.substr(2); }

    /*
     *   Determine if the given file is a text file or a binary file.  By
     *   default, we base the determination solely on the filename suffix,
     *   checking the extension against a list of common file types.  
     */
    isTextFile(fname)
    {
        /* get the extension */
        if (rexMatch('.*%.([^.]+)$', fname) != nil)
        {
            /* pull out the extension */
            local ext = rexGroup(1)[3].toLower();

            /* 
             *   check against common binary types - if it's not there,
             *   assume it's text 
             */
            return (binaryExts.indexOf(ext) == nil);
        }
        else
        {
            /* no extension - assume binary */
            return nil;
        }
    }

    /* table of common binary file extensions */
    binaryExts = ['jpg', 'jpeg', 'png', 'mng', 'bmp', 'gif',
                  'mpg', 'mp3', 'mid', 'ogg', 'wav',
                  'pdf', 'doc', 'docx', 'swf',
                  'dat', 'sav', 'bin', 'gam', 't3', 't3v'];
;

/*
 *   The resource handler for our standard library resources.  All of the
 *   library resources are in the /webuires resource folder.  This exposes
 *   everything in that folder as a downloadable Web object.  
 */
webuiResources: WebResourceResFile
    vpath = static new RexPattern('/webuires/')
;

/* the special cover art resource */
coverArtResource: WebResourceResFile
    vpath = static new RexPattern('/%.system/CoverArt%.(jpg|png)$')
;

/*
 *   Session initializer resource.  This is a mix-in class designed to be
 *   used for a special resource that initializes the session.  Mix this
 *   with a WebResource class to set up the initializer.  When you connect
 *   to the client via connectWebUI(), point the client to this resource.  
 *   
 *   There are two elements to setting up the session.  First, we need to
 *   set the program session key as a cookie.  The client obtains this from
 *   the registration mechanism, whose purpose is to launch the game
 *   program and send the connection information back to the client.  The
 *   client sends this to us in the form of a URL parameter, TADS_session.
 *   This key is essentially for authentication, to make sure that the
 *   client that we're talking to is actually the client that launched the
 *   program: only that client would be able to get the key, because we
 *   invent it and send it to the registrar, and the registrar only sends
 *   it back to the client session it's already established on its end.
 *   This prevents port scanners from finding our open port and trying to
 *   crawl our "site" or otherwise access our services.
 *   
 *   The second setup element is to create a client session key.  Whereas
 *   the program session key is for our entire service, the client session
 *   key is specific to this one connection.  If the user opens two browser
 *   windows on this server, each browser needs its own separate client
 *   session so that we can tell the traffic apart.  The client session key
 *   is simply another random key we generate, and again we pass it back to
 *   the client in a cookie.
 *   
 *   The reason we set cookies for both of these session keys is that it
 *   lets the client pass the information back to us on subsequent requests
 *   without having to encode another parameter in every URL.  We set
 *   session cookies in both cases; the program session is for
 *   authentication purposes and so we don't want it to be stored or
 *   shared, and the client session key is explicitly to identify this one
 *   browser session, so it obviously shouldn't be shared across browser
 *   instances or sessions.
 *   
 *   Note that instances should always provide a string (as opposed to a
 *   regular expression) for the virtual path (the 'vpath' property).  We
 *   have to send the path to the browser UI as part of the connection
 *   information, so we need a string we can send rather than a pattern to
 *   match.  
 */
class WebResourceInit: object
    /*
     *   Connect to the client.  The program should call this after
     *   creating its HTTPServer object, which you pass here as 'srv'.
     *   This establishes the client UI connection, generating the path to
     *   the start page.  
     */
    connectUI(srv)
    {
        /* 
         *   point the client to our start page, adding the session key as
         *   a query parameter 
         */
        connectWebUI(srv, '<<vpath>>?TADS_session=<<webSession.sessionKey>>');
    }

    /*
     *   Process the request.  This sets up the program and client session
     *   keys as cookies.  
     */
    processRequest(req, query)
    {
        /* get the session parameter from the query */
        local skey = query['TADS_session'];

        /* set the session cookie in the reply */
        if (skey)
            req.setCookie('TADS_session', '<<skey>>; path=/;');

        /* check for a client session */
        local ckey = req.getCookie('TADS_client');
        if (ckey == nil || ClientSession.find(ckey) == nil)
        {
            /* 
             *   There's no client session ID cookie, or the ID is invalid
             *   or has expired.  Create a new client session.  
             */

            /* get the storage server session ID key from the request */
            local ssid = query['storagesid'];

            /* get the user name */
            local uname = query['username'];

            /* 
             *   if there's no storage server session, and they're
             *   connecting under the primary server session key, use the
             *   primary user's storage server session
             */
            if (ssid == nil && skey == webSession.sessionKey)
                ssid = webSession.storageSID;

            /* create the new session object */
            local client = new transient ClientSession(skey, ssid);

            /* if there's a user name, set it in the session */
            if (uname != nil)
                client.screenName = uname;
            else
                client.setDefaultScreenName();

            /* send the client session ID to the browser as a cookie */
            req.setCookie('TADS_client', '<<client.clientKey>>; path=/;');

            /* if this is a guest, alert everyone to the new connection */
            if (!client.isPrimary)
                webMainWin.postSyntheticEvent('newGuest', client.screenName);
        }

        /* go return the underlying resource */
        inherited(req, query);
    }

    /* the HTPTServer for communicating with the client */
    server = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   A WebResourceGroup is a container for WebResource objects.  When a
 *   server receives a request, it looks in its group list to find the
 *   resource object that will handle the request.  
 */
class WebResourceGroup: object
    /*
     *   Should this group handle the given request?  By default, we say
     *   yes if the server that received the request is associated with
     *   this group via the group's 'server' property.  
     */
    isGroupFor(req)
    {
        /* get the request's server object */
        local srv = req.getServer();

        /* 
         *   if this server matches our 'server' property, or is in the
         *   list of servers if 'server' is a list, we're the group for the
         *   request 
         */
        return (srv == server
                || (dataType(server) == TypeList
                    && server.indexOf(srv) != nil));
    }

    /*
     *   The priority of the group, relative to other groups.  If the same
     *   server matches multiple groups, this allows you to designate which
     *   group has precedence.  A higher value means higher priority.
     */
    priority = 100

    /*
     *   The HTTPServer object or objects this group is associated with.
     *   The general event processor uses this to route a request to the
     *   appropriate resource group, by finding the group that's associated
     *   with the server that received the request.
     *   
     *   To associate a group with multiple servers, make this a list.  
     */
    server = nil

    /* the WebResource objects in the group */
    contents = []

    /*
     *   Process a request.  This looks for the highest priority matching
     *   resource in the group, then hands the request to that resource for
     *   processing. 
     */
    processRequest(req)
    {
        /* parse the query */
        local query = req.parseQuery();

        /* 
         *   Check for the session ID.  The session ID is required either
         *   in a URL parameter or in a cookie.  If it's not present,
         *   reject the request with a "403 Forbidden" error, since the
         *   session is essentially an authentication token to tell us that
         *   the client is in fact the same user that launched the game.
         */
        if (!webSession.validateKey(req, query))
            return;

        /* 
         *   Search our list for the first resource that matches this
         *   request.  The list is initialized in descending priority
         *   order, so the first match we find will be the one with the
         *   highest priority. 
         */
        local match = contents.valWhich({res: res.matchRequest(query, req)});

        /* if we found a match, process it; otherwise return a 404 error */
        if (match != nil)
            match.processRequest(req, query);
        else
            req.sendReply(404);
    }

    /* class property: list of all WebResourceGroup objects */
    all = []
;

/* ------------------------------------------------------------------------ */
/*
 *   The default web resource group.  This is the default container for
 *   WebResource objects. 
 */
mainWebGroup: WebResourceGroup
    /* the default group matches any server, but with low priority */
    isGroupFor(req) { return true; }
    priority = 1
;

/* ------------------------------------------------------------------------ */
/*
 *   At startup, put each WebResource object into the contents list for its
 *   group. 
 */
PreinitObject
    execute()
    {
        /* build the contents list for each resource group */
        forEachInstance(WebResource, function(obj) {

            /* get the group object for the resource */
            local g = obj.group;

            /* if the group doesn't have a contents list yet, create one */
            if (g.contents == nil)
                g.contents = [];

            /* add this resource to the contents list */
            g.contents += obj;
        });

        /* sort each group's contents list in priority order */
        forEachInstance(WebResourceGroup, function(obj) {

            /* sort the group's contents list */
            obj.contents = obj.contents.sort(
                SortDesc, { a, b: a.priority - b.priority });

            /* add this group to the master list of groups */
            WebResourceGroup.all += obj;
        });

        /* sort the groups in descending order of priority */
        WebResourceGroup.all = WebResourceGroup.all.sort(
            SortDesc, { a, b: a.priority - b.priority });
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Guest connection request.  This enables "switchboard" applications on
 *   remote servers that keep track of multi-user game sessions, to show
 *   users available sessions and connect new users to those sessions.
 *   
 *   The first step in setting up a switchboard is for the game server to
 *   register itself with the switchboard by sending a request on startup.
 *   That part is external to us - that's not handled within the game
 *   program but rather within the web server script that launches the
 *   game.  Here, then, we simply assume that this work is already done.
 *   
 *   The second step is that the switchboard needs to check back with the
 *   game server from time to time to see if it's still alive - essentially
 *   a "ping" operation.  We handle that here: if we respond to the
 *   request, we're obviously still alive.
 *   
 *   The third step is that we need to send the switchboard a URL that lets
 *   secondary users ("guests") connect to the game session.  We handle
 *   that here as well: our reply body is the client connection URL.  
 */
guestConnectPage: WebResource
    vpath = '/webui/guestConnect'
    processRequest(req, query)
    {
        /* send the collaborative connection URL */
        req.sendReply(webSession.getCollabUrl(), 'text/plain', 200);
    }
;

/* ------------------------------------------------------------------------ */
/* 
 *   getEvent request.  This is the mechanism we use to "send" events to
 *   the client.  The client sends a getEvent request to us, and we simply
 *   put it in a queue - we don't send back any response immediately.  As
 *   soon as we want to send an event to the client, we go through the
 *   queue of pending getEvent requests, and reply to each one with the
 *   event we want to send.  
 */
eventPage: WebResource
    vpath = '/webui/getEvent'
    processRequest(req, query)
    {
        /* find the client */
        local c = ClientSession.find(req);

        /* if we found the client session object, send it the request */
        if (c != nil)
            c.requestEvent(req);
        else
            req.sendReply(400);
    }

    /* broadcast an event message to each client */
    sendEvent(msg)
    {
        /* build the full XML message */
        msg = '<?xml version="1.0"?><event><<msg>></event>';

        /* send it to each client */
        ClientSession.broadcastEvent(msg);
    }

    /* send an event to a particular client */
    sendEventTo(msg, client)
    {
        /* build the full XML message */
        msg = '<?xml version="1.0"?><event><<msg>></event>';

        /* send it to the given client */
        client.sendEvent(msg);
    }
;

/*
 *   Flush events.  This cancels any pending event requests for the client.
 *   The client can use this after being reloaded to flush any outstanding
 *   event requests from a past incarnation of the page.  
 */
flushEventsPage: WebResource
    vpath = '/webui/flushEvents'
    processRequest(req, query)
    {
        /* find the client */
        local c = ClientSession.find(req);

        /* if we found the client, send it the flush request */
        if (c != nil)
            c.flushEvents();

        /* 
         *   acknowledge the request; it's okay if we didn't find the
         *   client session, since this just means there are no events to
         *   flush for this client 
         */
        sendAck(req);
    }
;

/*
 *   getState request.  The web page can send this to get a full accounting
 *   of the current state of the UI.  It does this automatically when first
 *   loaded, and again when the user manually refreshes the page.
 *   
 *   We handle this by asking the main window to generate its state.  
 */
uiStatePage: WebResource
    vpath = '/webui/getState'
    processRequest(req, query)
    {
        /* get the window making the request */
        local w = webMainWin.winFromPath(query['window']);
        local client = ClientSession.find(req);

        /* if we found the window, send the reply */
        if (w)
        {
            /* send the uiState reply for the window */
            sendXML(req, 'uiState', w.getState(client));
        }
        else
        {
            /* no window - send an error reply */
            req.sendReply(406);
        }
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Process network requests.  Continues until doneFunc() returns true, or
 *   a timeout or error occurs.  If we return because doneFunc() returned
 *   true, we'll return nil.  Otherwise, we'll return the NetEvent that
 *   terminated the wait.  
 */
processNetRequests(doneFunc, timeout?)
{
    /* if there's a timeout, figure the ending time */
    local endTime = (timeout != nil ? getTime(GetTimeTicks) + timeout : nil);

    /* keep going until the 'done' function returns true */
    while (doneFunc == nil || !doneFunc())
    {
        try
        {
            /* get the next housekeeping time */
            local hkTime = webSession.hkTime;
            
            /* 
             *   figure the time to the next timeout - stop at the caller's
             *   ending time, or the next housekeeping time, whichever is
             *   sooner 
             */
            local tf = (endTime != nil && endTime < hkTime ? endTime : hkTime);

            /* figure the time remaining to the next timeout */
            local dt = max(0, tf - getTime(GetTimeTicks));

            /* 
             *   Flush any pending output.  If we're waiting for user
             *   input, this ensures that the last output is visible in the
             *   UI while we await the next user action.  
             */
            flushOutput();

            /* get the next network event */
            local evt = getNetEvent(dt);

            /* see what we have */
            switch (evt.evType)
            {
            case NetEvRequest:
                /*
                 *   This is an incoming network request from a client.
                 *   Check the protocol type of the request object.
                 */
                if (evt.evRequest.ofKind(HTTPRequest))
                {
                    /* 
                     *   HTTP request - process it through the appropriate
                     *   web resource group.  First, find the group that
                     *   wants to handle the request.  
                     */
                    local req = evt.evRequest;
                    local group = WebResourceGroup.all.valWhich(
                        { g: g.isGroupFor(req) });

                    /* 
                     *   if there's a client associated with the request,
                     *   update its last activity time
                     */
                    local client = ClientSession.find(req);
                    if (client != nil)
                        client.updateEventTime();

                    /* if we found a group, let it handle the request */
                    if (group != nil)
                    {
                        try
                        {
                            /* send the request to the group for processing */
                            group.processRequest(req);
                        }
                        catch (Exception exc)
                        {
                            /* 
                             *   Unhandled exception - something went wrong
                             *   in the server, so the appropriate reply is
                             *   500 Internal Server Error.  Send the
                             *   exception message with the reply.  
                             */
                            local msg =
                                'Unhandled exception processing request: '
                                + exc.getExceptionMessage().specialsToText();
                            req.sendReply(msg, 'text/plain', 500);
                        }
                    }
                    else
                    {
                        /* no group - send back a 404 error */
                        req.sendReply(404);
                    }
                }
                break;

            case NetEvTimeout:
                /* 
                 *   Timeout.  Always try running housekeeping on a
                 *   timeout; the housekeeper will ignore this unless the
                 *   time has actually come.
                 */
                hkTime = webSession.housekeeping();

                /* 
                 *   Check for a caller timeout.  If the caller's end time
                 *   has arrived, pass the timeout back to the caller as a
                 *   timeout event.
                 */
                if (endTime != nil && getTime(GetTimeTicks) >= endTime)
                    return evt;

                /* 
                 *   it wasn't a caller timeout, so it must have been an
                 *   internal housekeeping timeout, which we just handled;
                 *   simply continue looping
                 */
                break;

            case NetEvUIClose:
                /* 
                 *   UI Closed.  This tells us that the user has manually
                 *   closed the UI window.  This only happens in the local
                 *   stand-alone configuration, where the "browser" is an
                 *   integrated part of the interpreter application,
                 *   simulating the traditional TADS interpreter setup
                 *   where the whole program is a single application
                 *   running on one machine.  In this setup, closing the UI
                 *   window should dismiss the whole application, since
                 *   that's the convention on virtually all GUIs.
                 */

                /* disconnect all clients */
                ClientSession.disconnectAll();

                /* quit by signaling a "quit game" event */
                throw new QuittingException();
            }
        }
        catch (SocketDisconnectException sdx)
        {
            /* the client has closed its connection; ignore this */
        }
    }

    /* indicate that the 'done' condition was triggered */
    return nil;
}

/* ------------------------------------------------------------------------ */
/*
 *   Web Window tracker.  This is a game object that controls and remembers
 *   the state of a "window" in the browser user interface.  By "window",
 *   we basically mean an HTML page, which might reside at the top level of
 *   the browser itself, or inside an IFRAME element within an enclosing
 *   page.
 *   
 *   Each WebWindow class corresponds to a particular HTML page that we
 *   serve the client.  The HTML page is the expression of the window in
 *   the browser, and the WebWindow object is the expression of the same
 *   information in the game program.  The two are different facets of the
 *   same conceptual UI object.  The reason we need the two separate
 *   expressions is that the server controls everything, but the client has
 *   to do the actual display work, and the two parts of the program speak
 *   different languages - the server is TADS, and the client is HTML.
 *   
 *   The WebWindow object on the server lets us easily reconstruct the UI
 *   state in a newly opened browser window, or when the user performs a
 *   page refresh.  This object's job is to send information to the client
 *   on demand that allows the client to display the page in its current
 *   state.
 *   
 *   Note that a given WebWindow/HTML page combination can be used more
 *   than once within the same UI.  The pages defined in the library are
 *   designed to be generic and reusable, so you might use the same window
 *   class more than once for different purposes within the UI.  The
 *   library pages can also be subclassed, by subclassing the WebWindow
 *   object and creating a customized copy of the corresponding HTML page
 *   resource.  
 */
class WebWindow: WebResourceResFile
    /*
     *   The URL path to the window's HTML definition file, as seen by the
     *   browser.  For the pre-defined library window types, we expose the
     *   HTML file in the root of the URL namespace - e.g., "/main.htm".
     *   The files are actually stored in the /webuires folder, but we
     *   expose them to the browser as though they were in the root folder
     *   to make embedded object references on the pages simpler.  The
     *   browser figures the path to an embedded object relative to the
     *   containing page, so by placing the containing page in the root
     *   folder, embedded object paths don't have to worry about
     *   referencing parent folders.  
     */
    vpath = nil

    /* 
     *   The window's actual source location, as a resource path.  A given
     *   WebWindow subclass corresponds to a particular HMTL page, since
     *   the class and the page are facets of the same conceptual object
     *   (one facet is the browser expression, the other is the game
     *   program expression).  
     */
    src = nil

    /* process a request path referencing me into my actual resource path */
    processName(n) { return src; }

    /*
     *   Resolve a window path name.  For container windows, this should
     *   search the sub-windows for the given path.  By default, we match
     *   simply if the path matches our name.
     */
    winFromPath(path)
    {
        return path == name ? self : nil;
    }

    /*
     *   Flush the window.  This sends any buffered text to the UI. 
     */
    flushWin() { }

    /*
     *   Write text to the window.  Subclasses with stream-oriented APIs
     *   must override this.  
     */
    write(txt) { }

    /*
     *   Clear the window.  Subclasses must override this. 
     */
    clearWindow() { }

    /*
     *   Get the window's current state.  This returns a string containing
     *   an XML fragment that describes the state of the window.  This
     *   information is sent to the HTML page when the browser asks for the
     *   current layout state when first loaded or when the page is
     *   refreshed.  The XML format for each subclass is specific to the
     *   Javascript on the class's HTML page.  
     */
    getState(client) { return ''; }

    /* send an event related to this window to all clients */
    sendWinEvent(evt)
    {
        /* 
         *   send the event message, adding a <window> parameter to
         *   identify the source
         */
        eventPage.sendEvent('<window><<pathName>></window><<evt>>');
    }

    /* send a window event to a specific client */
    sendWinEventTo(evt, client)
    {
        eventPage.sendEventTo('<window><<pathName>></window><<evt>>', client);
    }

    /* specialsToHtml context */
    sthCtx = perInstance(new SpecialsToHtmlState())

    /* the name of this window */
    name = nil

    /* the full path name of this window, in "win.sub.sub" format */
    pathName = nil
;

/* ------------------------------------------------------------------------ */
/*
 *   Layout Window.  This is a specialized Web Window tracker for our
 *   layout page type, which is displayed using the resource file
 *   webuires/layout.htm.  This page is designed as a container of more
 *   specialized sub-window pages; its job is to divide up the window space
 *   into IFRAME elements that display the sub-windows, and to manage the
 *   geometry of the IFRAMEs.
 *   
 *   The layout page is primarily designed to be the top-level page of the
 *   web UI.  The idea is to set up a layout page as the navigation URL for
 *   the browser, so the layout page fills the browser window.  You then
 *   arrange your functional windows within the layout page - a command
 *   window, a status line window, etc.  This arrangement is similar to
 *   banner window in HTML TADS, but IFRAMEs are considerably more
 *   flexible; for example, they don't have to tile the main window, and
 *   you can size them in the full range of units CSS provides.
 *   
 *   Layout windows aren't limited to the top level, though.  Since you can
 *   put any HTML page within an IFRAME, you can put another layout window
 *   within an IFRAME, to further subdivide the space inside the IFRAME.  
 */
class WebLayoutWindow: WebWindow
    /*
     *   Resolve a window path name 
     */
    winFromPath(path)
    {
        /* get the first element and the rest of the path */
        local idx = path.find('.');
        if (idx == nil)
            idx = path.length() + 1;

        /* pull out the first element and the rest */
        local head = path.substr(1, idx - 1);
        local tail = path.substr(idx + 1);

        /* if the first element doesn't match our name, it's not a match */
        if (head != name)
            return nil;

        /* if that's the end of the path, we have our match */
        if (tail == '')
            return self;

        /* match the rest of the path against our children */
        foreach (local w in frames)
        {
            local match = w[1].winFromPath(tail);
            if (match != nil)
                return match;
        }

        /* no match */
        return nil;
    }

    /*
     *   Create a new window within the layout.  This creates an IFRAME in
     *   the browser, laid out according to the 'pos' argument, and
     *   displays the given window object within the frame.
     *   
     *   If the window already exists, this updates the window with the new
     *   layout settings.
     *   
     *   'win' is a WebWindow object that will be displayed within the
     *   IFRAME.  This method automatically loads the HTML resource from
     *   the WebWindow into the new IFRAME.
     *   
     *   'name' is the name of the window.  Each window within a layout
     *   must have a distinct name.  This allows you to refer to the
     *   dimensions of other windows in 'pos' parameters.  The name should
     *   be alphanumeric.
     *   
     *   'pos' is the layout position for the new frame.  This is a string
     *   in this format: 'left, top, width, height', where 'left' is the
     *   horizontal position of the top left corner, 'top' is the vertical
     *   position of the top left corner, 'width' is the width of the
     *   window, and 'height' is the height.  Each element can be specified
     *   as a Javascript-style arithmetic expression.  Within the
     *   expression, you can use a mix of any of the following:
     *   
     *.   123   - a number, representing a number of pixels on the display
     *.   5em   - 5 'em' units, relative to the main BODY font in the window
     *.   5en   - 5 'en' units in the main BODY font
     *.   5ex   - 5 'ex' units in the main BODY font
     *.   window.width - the width in pixels of the enclosing window
     *.   window.height - the height in pixels of the enclosing window
     *.   50%   - percentage of the width or height of the enclosing window
     *.   content.width - the width in pixels of the contents of the frame
     *.   content.height - the height in pixels of the contents of the frame
     *.   x.left   - horizontal coordinate of leftmost edge of window 'x'
     *.   x.right  - horizontal coordinate of rightmost edge of window 'x'
     *.   x.top    - vertical coordinate of top edge of window 'x'
     *.   x.bottom - vertical coordinate of bottom edge of window 'x'
     *.   x.width  - width in pixels of window 'x'
     *.   x.height - height in pixels of window 'x'
     *   
     *   The "window" dimensions refer to the *enclosing* window.  If this
     *   layout window is the main page of the UI, this is simply the
     *   browser window itself.  For a layout window nested within another
     *   frame, this is the enclosing frame.
     *   
     *   Percentage units apply to the enclosing window.  When a percentage
     *   is used in the 'left' or 'width' slot, it applies to the width of
     *   the enclosing window; in the 'top' or 'height' slot, it applies to
     *   the height.
     *   
     *   The "content" dimensions refer to the contents of the frame we're
     *   creating.  This is the size of the contents as actually laid out
     *   in the browser.
     *   
     *   "x.left" and so on refer to the dimensions of other frames *within
     *   this same layout window*.  'x' is the name of another window
     *   within the same layout, as specified by the 'name' argument given
     *   when the window was created.  
     */
    createFrame(win, name, pos)
    {
        /* set the window's internal name to its full path name */
        win.name = name;
        win.pathName = self.name + '.' + name;

        /* add the window to our list */
        frames[name] = [win, pos];

        /* notify the UI */
        sendWinEvent('<subwin>'
                     + '<name><<name>></name>'
                     + '<pos><<pos>></pos>'
                     + '<src><<win.vpath.htmlify()>></src>'
                     + '</subwin>');
    }

    /*
     *   Flush this window.  For a layout window, we simply flush each
     *   child window.  
     */
    flushWin()
    {
        /* flush each child window */
        frames.forEach({w: w[1].flushWin()});
    }

    /*
     *   Get the state. 
     */
    getState(client)
    {
        /* build an XML fragment describing the list of frames */
        local s = '';
        frames.forEachAssoc(function(name, info) {
            s += '<subwin><name><<name>></name>'
                + '<pos><<info[2]>></pos>'
                + '<src><<info[1].vpath.htmlify()>></src>'
                + '</subwin>';
        });

        /* return the completed state object */
        return s;
    }

    /* 
     *   The table of active frames within this layout.  This table is
     *   keyed by window name; each entry is a list of [win, pos], where
     *   'win' is the WebWindow object for the window, and 'pos' is its
     *   position parameter.  
     */
    frames = perInstance(new LookupTable(16, 32))

    /* my virtual path and the actual resource file location */
    vpath = '/layoutwin.htm'
    src = 'webuires/layoutwin.htm'
;

/* ------------------------------------------------------------------------ */
/*
 *   Command Window.  This object keeps track of the state of command
 *   window within the web UI.  
 */
class WebCommandWin: WebWindow
    /*
     *   Write to the window 
     */
    write(txt)
    {
        /* add the text to the output buffer */
        outbuf.append(txt);
    }

    /*
     *   Flush the buffers 
     */
    flushWin()
    {
        /* get the current output buffer */
        local txt = toString(outbuf).specialsToHtml(sthCtx);

        /* add it to the state buffer (text since last input) */
        textbuf.append(txt);

        /* send the text to the client via an event */
        sendWinEvent('<say><<txt.htmlify()>></say>');

        /* we've now processed the pending output buffer */
        outbuf.deleteChars(1);
    }

    /*
     *   Read a line of input in this window.  Blocks until the reply is
     *   received.  Returns nil on timeout.  
     */
    getInputLine(timeout?)
    {
        /* flush buffered output */
        flushWin();
        
        /* clear out the last input */
        lastInput = nil;
        lastInputClient = nil;
        lastInputReady = nil;
        
        /* set the UI state */
        mode = 'inputLine';
        isInputOpen = true;
        
        /* send the inputLine event to the client */
        sendWinEvent('<inputLine/>');
        
        /* process network events until we get our input or we time out */
        processNetRequests(
            {: lastInputReady || webMainWin.syntheticEventReady() },
            timeout);

        /* move the current textbuf contents to the scrollback list */
        textbufToScrollback(lastInput);

        /* back to 'working' mode */
        mode = 'working';

        /* check the result */
        if (lastInput != nil)
        {
            /* we got a reply - mark the input line as closed in the UI */
            isInputOpen = nil;

            /* reset to the start of the line in the output context */
            sthCtx.resetLine();

            /* remember the source of the command */
            webMainWin.curCmdClient = lastInputClient;

            /* return the Line Input event */
            return [InEvtLine, lastInput];
        }
        else if (webMainWin.syntheticEventReady())
        {
            /* there's a synthetic event available - return it */
            return webMainWin.getSyntheticEvent();
        }
        else
        {
            /* we didn't get a reply, so we timed out */
            return [InEvtTimeout, ''];
        }
    }

    /*
     *   Cancel an input line that was interrupted by a timeout 
     */
    cancelInputLine(reset)
    {
        /* if the input line is open, send the cancel event */
        if (isInputOpen)
        {
            /* send the cancel event to the client */
            sendWinEvent('<cancelInputLine reset="<<
                reset ? 'yes' : 'no'>>" />');

            /* the input line is closed */
            isInputOpen = nil;

            /* reset to the start of the line in the output context */
            sthCtx.resetLine();
        }
    }

    /*
     *   Get the state of this command window 
     */
    getState(client)
    {
        return '<mode><<mode>></mode><scrollback>'
            + scrollback.join()
            + '<sbitem><text><<toString(textbuf).htmlify()>></text></sbitem>'
            + '</scrollback>';
    }

    /*
     *   Receive input from the client 
     */
    receiveInput(req, query)
    {
        /* remember the text */
        lastInput = query['txt'];

        /* remember the source of the input */
        lastInputClient = ClientSession.find(req);

        /* set the input-ready flag so we exit the modal input loop */
        lastInputReady = true;

        /* get the user who entered the command */
        local user = (lastInputClient != nil
                      ? lastInputClient.screenName : '');

        /* tell any other windows listening in about the new input */
        sendWinEvent('<closeInputLine>'
                     + (lastInput != nil ?
                        '<text><<lastInput.htmlify()>></text>' : '')
                     + '<user><<user.htmlify()>></user>'
                     + '</closeInputLine>');

        /* reset to the start of the line in the output context */
        sthCtx.resetLine();
    }

    /*
     *   Clear the window 
     */
    clearWindow()
    {
        /* flush the output buffer */
        flushWin();

        /* 
         *   clear the transcript - if the user refreshes the browser
         *   window, we want to show a blank window
         */
        textbuf.deleteChars(1);
        scrollback.clear();

        /* send a clear window event */
        sendWinEvent('<clearWindow/>');

        /* reset to the start of the line in the output context */
        sthCtx.resetLine();
    }

    /*
     *   Move the current text buffer contents to the scrollback list.  If
     *   this would make the scrollback list exceed the limit, we'll drop
     *   the oldest item.
     *   
     *   'cmd' is the command line text of the last input.  We include this
     *   in the srollback list with special tagging so that the UI can
     *   display it in a custom style, if it wants.  
     */
    textbufToScrollback(cmd)
    {
        /* get the current buffer, tagged as a <text> entry in the list */
        local t = '<text><<toString(textbuf).htmlify()>></text>';

        /* add the command line tagged as <input> */
        if (cmd != nil)
            t += '<input><<cmd.htmlify()>></input>';
        
        /* add the buffer to the scrollback list */
        scrollback.append('<sbitem><<t>></sbitem>');

        /* if this pushes us past the limit, drop the oldest item */
        if (scrollback.length() > scrollbackLimit)
            scrollback.shift();

        /* clear text buffer */
        textbuf.deleteChars(1);
    }

    /*
     *   Show a "More" prompt 
     */
    showMorePrompt()
    {
        /* flush the output buffer */
        flushWin();
        
        /* send a "More" prompt event to the UI */
        sendWinEvent('<morePrompt/>');
        mode = 'morePrompt';
        moreMode = true;

        /* process events until More mode is done */
        processNetRequests({: !moreMode });

        /* return the default mode */
        mode = 'working';
    }

    /* 
     *   receive notification from the client that the user has responded
     *   to the More prompt, ending the pause
     */
    endMoreMode()
    {
        /* no longer in More mode */
        moreMode = nil;
    }

    /* main window text buffer since last input read */
    textbuf = perInstance(new StringBuffer(4096))

    /*
     *   Scrollback list.  After each input, we add the contents of
     *   'textbuf' to this list.  If this pushes the list past the limit,
     *   we drop the oldest item.  This is used to reconstruct a reasonable
     *   amount of scrollback history when a new client connects, or when
     *   an existing client refreshes the page.  
     */
    scrollback = perInstance(new Vector())

    /*
     *   The scrollback limit, as a number of command inputs.  Each input
     *   interaction adds one item to the scrollback list.  When the number
     *   of items in the list exceeds the limit set here, we drop the
     *   oldest item.  
     */
    scrollbackLimit = 10

    /* pending output buffer, since last flush */
    outbuf = perInstance(new StringBuffer(4096))

    /* the text of the last input line we received from the client */
    lastInput = nil

    /* is input ready? */
    lastInputReady = nil

    /* client session who sent the last input line */
    lastInputClient = nil

    /* 
     *   Is an input line open?  This is true between sending an
     *   <inputLine> event and either getting a reply, or explicitly
     *   sending a close or cancel event. 
     */
    isInputOpen = nil

    /* flag: we're in More mode */
    moreMode = nil

    /* 
     *   Current UI mode.  This is 'working' if the program is running and
     *   in the process of computing and/or generating output; 'inputLine'
     *   if we're waiting for the user to enter a line of input;
     *   'morePrompt' if we're showing a "More" prompt.  
     */
    mode = 'working'

    /* my virtual path, and the actual resource file location */
    vpath = '/cmdwin.htm'
    src = 'webuires/cmdwin.htm'
;

/* 
 *   input-line event page 
 */
inputLinePage: WebResource
    vpath = '/webui/inputLine'
    processRequest(req, query)
    {
        /* find the window */
        local w = webMainWin.winFromPath(query['window']);

        /* dispatch to the window if we found it */
        if (w != nil)
        {
            /* send the input to the window */
            w.receiveInput(req, query);

            /* acknowledge the request */
            sendAck(req);
        }
        else
            req.sendReply(406);
    }
;

/*
 *   "More" prompt done event page
 */
morePromptDonePage: WebResource
    vpath = '/webui/morePromptDone'
    processRequest(req, query)
    {
        /* find the target winodw */
        local w = webMainWin.winFromPath(query['window']);
        if (w != nil)
        {
            /* release More mode in the server window */
            w.endMoreMode();

            /* acknowledge the request */
            sendAck(req);
        }
        else
            req.sendReply(406);
    }
;


/* ------------------------------------------------------------------------ */
/*
 *   Set Preferences command
 */
setPrefsPage: WebResource
    vpath = '/webui/setPrefs'
    processRequest(req, query)
    {
        /* get the request body */
        local f = req.getBody();
        if (f == nil)
        {
            errorReply(req, 'Error saving preferences: no data received');
            return;
        }
        else if (f == 'overflow')
        {
            errorReply(req, 'Error saving preferences: message too large');
            return;
        }

        /* get the client session for the request */
        local cli = ClientSession.find(req);
        if (cli == nil)
        {
            errorReply(req, 'Error saving preferences: missing session');
            return;
        }

        /* process the file through the profile reader */
        cli.uiPrefs.readSettings(f);

        /* save the updated settings */
        cli.uiPrefs.saveSettings();

        /* done with the file */
        f.closeFile();

        /* done - acknowledge the request */
        sendAck(req);
    }

    /* send an error as the reply to a request, formatted into XML */
    errorReply(req, msg)
    {
        sendXML(req, 'reply', '<error><<msg.htmlify()>></error>');
    }
;

/*
 *   UI Settings list.  This represents a named UI settings profile in the
 *   Web UI.  A profile is a list of name/value pairs.
 *   
 *   Most of the name keys are style IDs defined in the javascript for on
 *   the UI side - see main.js.  These style IDs are arbitrary keys we
 *   define to identify UI elements - "mainFont" for the main font name,
 *   "statusBkg" for the status-line window's background color, etc.  Each
 *   style ID generally corresponds to a dialog control widget in the
 *   preferences dialog in the javascript UI, and also corresponds to one
 *   or more CSS style selectors.  The mapping from style ID to CSS is
 *   defined in the UI javascript (see prefsMapper in main.js).
 *   
 *   The non-style key "profileName" is the user-visible name of this
 *   profile.  Internally, we refer to profiles using ID values, which are
 *   arbitrary identifiers generated by the UI when it creates a new
 *   profile (it currently uses integer keys).  
 */
class WebUIProfile: object
    construct(id)
    {
        self.profileID = id;
        self.settings = new LookupTable();
    }

    /* set a preference item in the profile */
    setItem(id, val)
    {
        settings[id] = val;
    }

    /* call a callback for each style: func(id, val) */
    forEach(func)
    {
        settings.forEachAssoc(func);
    }

    /* internal ID of the profile */
    profileID = ''

    /* table of style value strings, keyed by style ID */
    settings = nil
;

/*
 *   Web UI preferences.  This object contains the in-memory version of the
 *   display style preferences file.
 *   
 *   Each client session has its own copy of this object, because each
 *   client can be associated with a different user, and each user has
 *   their own preferences file.  
 */
class WebUIPrefs: object
    construct(c)
    {
        /* remember our client session object */
        clientSession = c;

        /* load the initial settings from the user's config file */
        loadSettings();
    }

    /* read the settings file */
    loadSettings()
    {
        /* open the preferences file; do nothing if that fails */
        local f = openSettingsFile(FileAccessRead);
        if (f == nil)
            return;

        /* read the settings from the file */
        readSettings(f);

        /* done with the file */
        f.closeFile();
    }

    /* read settings from a file */
    readSettings(f)
    {
        /* set up our table of profiles */
        local pros = profileTab = new LookupTable();

        /* we don't have a current profile selection yet */
        curProfile = nil;

        /* read the file */
        for (;;)
        {
            /* read the next line */
            local l = f.readFile();
            if (l == nil)
                break;

            /* if it's a valid line, process it */
            if (rexMatch(curProPat, l) != nil)
            {
                /* current profile setting */
                curProfile = rexGroup(1)[3];
            }
            else if (rexMatch(proItemPat, l) != nil)
            {
                /* style item definition - pull out the parts */
                local proid = rexGroup(1)[3];
                local key = rexGroup(2)[3];
                local val = rexGroup(3)[3];

                /* if the profile isn't in the table yet, add it */
                local pro = pros[proid];
                if (pro == nil)
                    pros[proid] = pro = new WebUIProfile(proid);

                /* add this item to the table */
                pro.setItem(key, val);
            }
        }
    }

    /* current profile ID pattern - current-profile:xxx */
    curProPat = static new RexPattern('current-profile=([^\n]*)\n?$')

    /* setting ID pattern for profile items - nnn.xxx=yyy */
    proItemPat = static new RexPattern('([^.]+)%.([^=]+)=([^\n]*)\n?$')

    /* save the current settings to the user's config file */
    saveSettings()
    {
        /* if there's no profile table, there's nothing to write */
        if (profileTab == nil)
            return;

        try
        {
            /* open the preferences file; do nothing if that fails */
            local f = openSettingsFile(FileAccessWrite);
            if (f == nil)
                return;
            
            /* write the current profile ID */
            f.writeFile('current-profile=<<curProfile>>\n');
            
            /* write the profiles */
            profileTab.forEachAssoc(function(id, pro) 
            {
                /* write each element of this profile */
                pro.forEach(function(key, val)
                {
                    /* write this profile item */
                    f.writeFile('<<id>>.<<key>>=<<val>>\n');
                });
            });

            /* done with the file */
            f.closeFile();
        }
        catch (Exception e)
        {
            /* 
             *   couldn't save the file; this isn't fatal, so just log an
             *   error event 
             */
            webMainWin.postSyntheticEvent(
                'logError', 'An error occurred saving your setting changes.
                    (Details: <<e.getExceptionMessage()>>)');
        }
    }

    /* open the settings file */
    openSettingsFile(access)
    {
        /* get the filename; abort if we don't have a file */
        local name = getSettingsFile();
        if (name == nil)
            return nil;

        /* open the file */
        try
        {
            /* open the file and return the handle */
            return File.openTextFile(name, access, 'ascii');
        }
        catch (Exception exc)
        {
            /* failed to open the file */
            return nil;
        }
    }

    /* get the settings file path */
    getSettingsFile()
    {
        /* if we're in local stand-alone mode, use the local Web UI file */
        if (getLaunchHostAddr() == nil)
            return WebUIPrefsFile;

        /* if there's a storage server session, the file is on the server */
        if (clientSession.storageSID != nil)
            return '~<<clientSession.storageSID>>/special/2';

        /* 
         *   We're in client/server mode, but there's no storage server.
         *   In this mode, we don't have any server-side location to store
         *   files, so we can't save or restore the configuration.
         */
        return nil;
    }

    /* get the current settings as XML, to send to the web UI */
    getXML()
    {
        /* if there's no profile table, there are no settings to return */
        if (profileTab == nil)
            return '';

        /* create a buffer for the results */
        local s = new StringBuffer();

        /* add the current profile */
        s.append('<currentProfile><<curProfile>></currentProfile>');

        /* add each profile's contents */
        profileTab.forEachAssoc(function(id, pro)
        {
            /* open this profile section */
            s.append('<profile><id><<id>></id>');

            /* add each style element */
            pro.forEach(function(id, val) {
                s.append('<item>'
                         + '<id><<id.htmlify()>></id>'
                         + '<value><<val.htmlify()>></value>'
                         + '</item>');
            });

            /* close this profile's section */
            s.append('</profile>');
        });

        /* return the result as an XML string */
        return toString(s);
    }

    /* the client session for this preference list */
    clientSession = nil

    /* 
     *   profile table - this is a LookupTable of WebUIProfile objects
     *   keyed by profile name 
     */
    profileTab = nil

    /* current active profile selected by the user */
    curProfile = nil
;


/* ------------------------------------------------------------------------ */
/*
 *   Status line window 
 */
class WebStatusWin: WebWindow
    /* my request path and actual resource path */
    vpath = '/statwin.htm'
    src = 'webuires/statwin.htm'

    /* 
     *   Set the room and score/turns portions of the status line.  This
     *   sets the left side of the status line to the 'room' text (which
     *   can contain HTML markups), and the right side to the the
     *   score/turns values, if present.  If the turn counter is omitted
     *   but the score value is present, we'll just show the score value;
     *   otherwise we'll format these as "score/turns".  If no score value
     *   is present, we'll leave the right side blank.  
     */
    setStatus(room, score?, turns?)
    {
        /* set up the room text in the left portion */
        local msg = '<div class="statusleft"><<room>></div>';

        /* 
         *   format the right side: 'score/turns', 'score', or empty,
         *   depending on what the caller specified 
         */
        local rt = (score != nil ? toString(score) : '')
            + (turns != nil ? '/' + turns : '');

        /* if there's a right side, wrap it with right alignment */
        if (rt != '')
            msg += '<div class="statusright"><<rt>></div>';

        /* 
         *   our left/right divisions are floats, which some browsers don't
         *   count against the container size; so add a clear:all division
         *   to make sure we count the height properly 
         */
        msg += '<div style="clear: both;"></div>';

        /* set the text */
        setStatusText(msg);
    }

    /* 
     *   Set the text of the status line.  This sets the entire status
     *   window to the given HTML text, without any additional formatting.
     */
    setStatusText(msg)
    {
        /* setting new text, so reset the stream context */
        sthCtx.resetState();

        /* set the new text */
        txt_.deleteChars(1);
        txt_.append(msg.specialsToHtml(sthCtx));

        /* note that we have new text to send to the UI */
        deltas_ = true;
    }

    /* add text to the status line */
    write(msg)
    {
        /* add the text */
        txt_.append(msg.specialsToHtml(sthCtx));

        /* note that we have new text to send to the UI */
        deltas_ = true;
    }

    /* clear the window */
    clearWindow()
    {
        setStatusText('');
    }

    /* flush pending text to the window */
    flushWin()
    {
        /* if we have any deltas since the last flush, send changes */
        if (deltas_)
        {
            /* 
             *   Send a set-text event, along with a resize.  The resize
             *   ensures that the status line adjusts to the current
             *   content size, assuming the window is using automatic
             *   content-height sizing.  
             */
            sendWinEvent(
                '<text><<toString(txt_).htmlify()>></text><resize/>');

            /* reset the deltas counter */
            deltas_ = nil;
        }
    }

    /*
     *   Refigure the window size.  The status line is generally set up to
     *   be automatically sized to its contents, which requires that we
     *   tell the UI when it's time to recalculate the layout to reflect
     *   the current contents after a change.  
     */
    resize() { sendWinEvent('<resize/>'); }

    /* get the current state to send to the browser */
    getState(client)
    {
        return '<text><<toString(txt_).htmlify()>></text>';
    }

    /* do we have any deltas since the last flush? */
    deltas_ = nil

    /* the current status message */
    txt_ = perInstance(new StringBuffer(512))
;

/* ------------------------------------------------------------------------ */
/*
 *   The standard "main window" of our user interface.  This is the game
 *   object that represents the default initial HTML page that the player's
 *   web browser connects to.  We build this out of three base classes:
 *   
 *   - WebResourceInit, because this is the starting page that the browser
 *   initially connects to.  This class does the initial handshaking to set
 *   up the session.
 *   
 *   - WebResourceResFile, because we store the HTML for the page in a
 *   resource file.  This class does the work of sending the resource
 *   file's contents to the browser.
 *   
 *   - WebLayoutWindow, because the default main page is also a layout
 *   window, which is basically a container for IFRAME elements where we
 *   plug in the sub-windows that make up the game's user interface.
 *   
 *   Games can customize the front page in any way they like.  If you want
 *   to customize the HTML of the main page, you can substitute a different
 *   HTML (.htm) file, and change the processName() method to return the
 *   name of that file.  If you want to use something other than a layout
 *   window as the front page, you can simply replace this whole class.  
 */
transient webMainWin: WebResourceInit, WebLayoutWindow, WebResourceResFile
    /* 
     *   match the webuires directory path as the URL path, but map this to
     *   main.htm as the underlying resource name 
     */
    vpath = '/'
    processName(n) { return 'webuires/main.htm'; }

    /* the top window is always called "main" */
    name = 'main'
    pathName = 'main'

    /* the window title */
    title = 'TADS'

    /* set the window title */
    setTitle(title)
    {
        /* remember the title */
        self.title = title;

        /* update the browser */
        sendWinEvent('<setTitle><<title.htmlify()>></setTitle>');
    }

    /*
     *   Client session for current command line input.  Certain modal
     *   interactions, such as file dialogs, are directed only to the
     *   client that initiated the current command.  
     */
    curCmdClient = nil

    /* get the state */
    getState(client)
    {
        /* get the inherited layout state */
        local s = inherited(client);

        /* add the preference settings */
        s += '<prefs><<client.uiPrefs.getXML()>></prefs>';

        /* add the window title */
        s += '<title><<title.htmlify()>></title>';

        /* add the input event, input file, and input dialog states */
        s += inputEventState;
        s += inputDialogState;
        s += menuSysState;

        /* if this is the command client, include the file dialog state */
        if (client == curCmdClient)
            s += fileDialogState;

        /* add the downloadable file list */
        s += client.allDownloads().mapAll(
            { x: x.isReady
              ? '<offerDownload><<x.resPath.htmlify()>></offerDownload>'
              : '' }
            ).join();

        /* return the result */
        return s;
    }

    /* wait for an input event */
    getInputEvent(timeout)
    {
        /* flush buffered output */
        flushWin();

        /* send the get-event request to the client */
        inputEventState = '<getInputEvent/>';
        inputEventResult = nil;
        sendWinEvent(inputEventState);

        /* process network events until we get an input event */
        processNetRequests({: inputEventResult != nil }, timeout);

        /* check what happened */
        if (inputEventResult)
        {
            /* we got an input event - return it */
            return inputEventResult;
        }
        else
        {
            /* 
             *   There's no result, so we timed out.  Tell the UI to cancel
             *   the input event mode.  
             */
            inputEventState = '';
            sendWinEvent('<cancelInputEvent/>');

            /* return a timeout event */
            return [InEvtTimeout];
        }
    }

    /* receive an input event */
    receiveInputEvent(req, query)
    {
        local typ = query['type'];
        local param = query['param'];

        switch (typ)
        {
        case 'key':
            inputEventResult = [InEvtKey, param];
            break;

        case 'href':
            inputEventResult = [InEvtHref, param];
            break;

        default:
            inputEventResult = [InEvtEof];
            break;
        }
    }

    /* show the file selector dialog */
    getInputFile(prompt, dialogType, fileType, flags)
    {
        /* mappings from FileTypeXxx to storage manager type codes */
        local typeMap = [
            FileTypeLog -> 'log',
            FileTypeData -> 'dat',
            FileTypeCmd -> 'cmd',
            FileTypeText -> 'txt',
            FileTypeBin -> 'bin',
            FileTypeT3Image -> 't3',
            FileTypeT3Save -> 't3v',
            * -> '*'];

        /* get the storage server session for the command client */
        local sid = curCmdClient.storageSID;

        /* 
         *   If we have a session, get the URL to the storage server file
         *   selection dialog.  Note that we don't specify the session in
         *   the URL, since the storage server separately maintains the
         *   session with the client via a cookie.  
         */
        local url = nil;
        if (sid != nil)
            url = getNetStorageURL(
                'fileDialog?filetype=<<typeMap[fileType]>>'
                + '&dlgtype=<<dialogType == InFileOpen ? 'open' : 'save'>>'
                + '&prompt=<<prompt.urlEncode()>>'
                + '&ret=<<webSession.getFullUrl('/webui/inputFile')>>');

        /* 
         *   if there's no storage server, try using a client PC upload or
         *   download 
         */
        if (url == nil)
            return getInputFileFromClient(prompt, dialogType, fileType, flags);

        /* 
         *   set up the file dialog state description (store it in a
         *   property, so that we can send it again as part of a state
         *   request if the client window is reloaded from scratch) 
         */
        fileDialogState =
            '<fileDialog>'
            +   '<prompt><<prompt.htmlify()>></prompt>'
            +   '<dialogType><<dialogType == InFileOpen
                ? 'open' : 'save'>></dialogType>'
            +   '<fileType><<typeMap[fileType]>></fileType>'
            +   '<url><<url.htmlify()>></url>'
            +   '</fileDialog>';

        /* 
         *   Presume that the dialog will be canceled.  If the user clicks
         *   the close box on the dialog, the dialog will be dismissed
         *   without ever sending us a result.  This counts as a
         *   cancellation, so it's the default result.  
         */
        fileDialogResult = [InFileCancel];

        /* send the request to the current command client */
        sendWinEventTo(fileDialogState, curCmdClient);

        /* process network events until the dialog is dismissed */
        processNetRequests(
            {: fileDialogState == '' || !curCmdClient.isAlive });

        /* return the dialog result */
        return fileDialogResult;
    }

    /*
     *   Get an input file from the client PC.  We'll attempt to upload or
     *   download a file from/to the client PC, using a local temporary
     *   file for the actual file operations.  This is a special form of
     *   the input file dialog that we use when we're not connected to a
     *   storage server.  
     */
    getInputFileFromClient(prompt, dialogType, fileType, flags)
    {
        /* use the appropriate procedure for Open vs Save */
        if (dialogType == InFileOpen)
        {
            /*
             *   Opening an existing file.  Show a dialog asking the user
             *   to select a file to upload.  If the user uploads a file,
             *   we'll use the local temp file created by the upload as the
             *   result of the input dialog.  
             */
            fileDialogState =
                '<uploadFileDialog>'
                + '<prompt><<prompt.htmlify()>></prompt>'
                + '</uploadFileDialog>';

            /* presume the user will cancel the dialog */
            fileDialogResult = [InFileCancel];

            /* send the request to the UI */
            sendWinEventTo(fileDialogState, curCmdClient);

            /* process network events until the dialog is dismissed */
            processNetRequests(
                {: fileDialogState == '' || !curCmdClient.isAlive });

            /* return the file dialog result */
            return fileDialogResult;
        }
        else if (dialogType == InFileSave)
        {
            /*
             *   Saving to a new file.  The caller is asking the user to
             *   choose a name for a file that the caller *intends* to
             *   create, but hasn't yet created.  HTTP can only do a
             *   download as a transaction, though - we can offer an
             *   existing file to the user to download, and the user will
             *   choose the location for the download as part of that
             *   transaction.  We don't have that file yet because the
             *   standard protocol is to ask the user for the name first,
             *   then write the selected file.
             *   
             *   So, we have to play a little game.  We don't ask the user
             *   to save anything right now, because there's no file to
             *   offer for download yet.  Instead, we silently generate a
             *   local temporary file name and return that to our caller.
             *   Our caller will create the temp file and write its data.
             *   
             *   The trick is that we wrap this temp file name in a
             *   DownloadTempFile object.  When the caller finishes
             *   writing, it will call closeFile() on the temp file.  The
             *   system will pass that along to use by calling closeFile()
             *   on our DownloadTempFile object.  At that point, we'll have
             *   a completed temporary file, so we'll pop up a download box
             *   at that point offering the file for download to the user.
             *   The user can then select a local file on the client side,
             *   and we'll send the temporary file's contents as the
             *   download to store in the client file.  
             */
            return [InFileSuccess,
                    tempFileDownloadPage.addFile(fileType, curCmdClient)];
        }
        else
        {
            /* unknown dialog type */
            return [InFileFailure];
        }
    }

    /*
     *   Offer a file for download to the client.  'file' is a
     *   DownloadTempFile object previously created by a call to
     *   inputFile().  
     */
    offerDownload(file)
    {
        /* 
         *   send the "offer download" event to the client - the client
         *   will display an iframe with the file as an "attachment", which
         *   will trigger the browser to offer it as a download 
         */
        sendWinEventTo(
            '<offerDownload><<file.resPath.htmlify()>></offerDownload>',
            curCmdClient);
    }

    /* receive a file selection from the file selector dialog */
    receiveFileSelection(req, query)
    {
        /* get the filename from the query */
        local f = query['file'];
        local desc = query['desc'];
        local cancel = (query['cancel'] != nil);
        local err = query['error'];

        /* 
         *   Set the file dialog result list. This uses the same format as
         *   the native inputFile() routine: the first element is the
         *   status code (Success, Failure, Cancel); on success, the second
         *   element is the filename string.  We add two bits of 
         */
        if (err != nil)
            fileDialogResult = [InFileFailure, err];
        else if (cancel || f == nil || f == '')
            fileDialogResult = [InFileCancel];
        else
        {
            /* we got a filename - add the SID prefix if necessary */
            if (rexMatch('~[^/]+/', f) == nil)
                f = '~<<curCmdClient.storageSID>>/<<f>>';
            
            /* success - return the filename */
            fileDialogResult = [InFileSuccess, f];

            /* if there's a description string, return that as well */
            if (desc != nil)
                fileDialogResult += desc;
        }
    }

    /* receive notification that the file dialog has been closed */
    inputFileDismissed()
    {
        /* clear the file dialog state */
        fileDialogState = '';
    }

    /* receive a file upload from the file upload dialog */
    receiveFileUpload(req, query)
    {
        /* get the file from the request */
        local fields = req.getFormFields(), file;
        if (fields == 'overflow')
        {
            /* error */
            fileDialogResult = [InFileFailure, libMessages.webUploadTooBig];
        }
        else if (fields != nil && (file = fields['file']) != nil)
        {
            /* got it - save the contents to a local temporary file */
            local tmpfile = new TemporaryFile();
            local fpTmp = nil;
            try
            {
                /* open the temp file */
                fpTmp = File.openRawFile(tmpfile, FileAccessWrite);

                /* make sure the input file is in raw binary mode */
                local fpIn = file.file;
                fpIn.setFileMode(FileModeRaw);

                /* copy the contents */
                fpTmp.writeBytes(fpIn);

                /* close the temp file */
                fpTmp.closeFile();

                /* 
                 *   Set the dialog result to the temp file.  The system
                 *   will automatically delete the underlying file system
                 *   object when the garbage collector deletes the
                 *   TemporaryFile.  
                 */
                fileDialogResult = [InFileSuccess, tmpfile];
            }
            catch (Exception exc)
            {
                /* error - the dialog failed */
                fileDialogResult = [InFileFailure, exc.getExceptionMessage()];

                /* close and delete the temporary file */
                if (fpTmp != nil)
                    fpTmp.closeFile();
                if (tmpfile != nil)
                    tmpfile.deleteFile();
            }
        }
        else
        {
            /* no file - consider it a cancellation */
            fileDialogResult = [InFileCancel];
        }

        /* the dialog is dismissed */
        fileDialogState = '';
    }

    /* show a generic inputDialog dialog */
    getInputDialog(icon, prompt, buttons, defaultButton, cancelButton)
    {
        /* 
         *   if one of the standard button sets was selected, turn it into
         *   a list of localized button names
         */
        switch (buttons)
        {
        case InDlgOk:
            buttons = [libMessages.dlgButtonOk];
            break;

        case InDlgOkCancel:
            buttons = [libMessages.dlgButtonOk, libMessages.dlgButtonCancel];
            break;

        case InDlgYesNo:
            buttons = [libMessages.dlgButtonYes, libMessages.dlgButtonNo];
            break;

        case InDlgYesNoCancel:
            buttons = [libMessages.dlgButtonYes, libMessages.dlgButtonNo,
                       libMessages.dlgButtonCancel];
            break;
        }

        /* get a suitable localized title corresponding to the icon tyep */
        local title = [InDlgIconNone -> libMessages.dlgTitleNone,
                       InDlgIconWarning -> libMessages.dlgTitleWarning,
                       InDlgIconInfo -> libMessages.dlgTitleInfo,
                       InDlgIconQuestion -> libMessages.dlgTitleQuestion,
                       InDlgIconError -> libMessages.dlgTitleError][icon];
        
        /* build the dialog xml description */
        inputDialogState =
            '<inputDialog>'
            +  '<icon><<icon>></icon>'
            +  '<title><<title>></title>'
            +  '<prompt><<prompt.htmlify>></prompt>'
            +  '<buttons>'
            +     buttons.mapAll({b: '<button><<b.htmlify()>></button>'})
                  .join('')
            +  '</buttons>'
            +  '<defaultButton><<defaultButton>></defaultButton>'
            +  '<cancelButton><<cancelButton>></cancelButton>'
            + '</inputDialog>';

        /* send the request to the client */
        inputDialogResult = nil;
        sendWinEvent(inputDialogState);

        /* process network events until we get an answer */
        processNetRequests({: inputDialogResult != nil });

        /* return the dialog result */
        return inputDialogResult;
    }

    /* receive a selection from the input dialog */
    receiveInputDialog(req, query)
    {
        /* note the result button */
        inputDialogResult = toInteger(query['button']);

        /* if we didn't get a valid button index, select button 1 */
        if (inputDialogResult == 0)
            inputDialogResult = 1;

        /* the dialog is no longer open */
        inputDialogState = '';
    }

    /*
     *   Post a synthetic event.  A synthetic event looks like a regular UI
     *   or network event, but is generated internally instead of being
     *   delivered from the underlying browser or network subsystems.
     *   
     *   'id' is a string giving the event type.  The remaining parameters
     *   are up to each event type to define.  
     */
    postSyntheticEvent(id, [params])
    {
        /* add it to the synthetic event queue */
        synthEventQueue.append([id] + params);
    }

    /* is a synthetic event ready? */
    syntheticEventReady() { return synthEventQueue.length() > 0; }

    /* pull the next synthetic event from the queue */
    getSyntheticEvent() { return synthEventQueue.shift(); }

    /* 
     *   file dialog state - this is the XML describing the currently open
     *   file dialog; if the dialog isn't open, this is an empty string 
     */
    fileDialogState = ''

    /* 
     *   file dialog result - this is a result list using the same format
     *   as the native inputFile() function 
     */
    fileDialogResult = nil

    /* 
     *   input dialog state - this is the XML describing an input dialog
     *   while a dialog is running, or an empty string if not 
     */
    inputDialogState = ''

    /* input dialog result - this is the button number the user selected */
    inputDialogResult = nil

    /* input event state */
    inputEventState = ''

    /* input event result */
    inputEventResult = nil

    /* menuSys state - menu system state (maintained by the menu module) */
    menuSysState = ''

    /* 
     *   Synthetic event queue.  This is a vector of synthetic events, set
     *   up in the [type, params...] format that the system inputEvent()
     *   function and related functions use.  The 'type' code for a
     *   synthetic evente is a string instead of the numeric identifier
     *   that the system functions use.  
     */
    synthEventQueue = static new transient Vector()
;

/* ------------------------------------------------------------------------ */
/*
 *   Temporary file download page.  This page serves temporary files
 *   created via inputFile() as HTTP downloads to the client.  
 */
transient tempFileDownloadPage: WebResource
    vpath = static new RexPattern('/clienttmp/')
    processRequest(req, query)
    {
        /* 
         *   look up the file - the key in our table is the ID string after
         *   the /clienttmp/ path prefix 
         */
        local id = query[1].substr(12);
        local client = ClientSession.find(req);
        local desc = client.downloads[id];

        /* check for cancellation */
        if (query['cancel'] != nil)
        {
            /* 
             *   acknowledge the request - do this before we cancel the
             *   file, since removing the file will remove the link that
             *   fired this request (the order probably isn't a big deal
             *   one way or the other, and we probably can't really control
             *   it anyway as the browser might process the ack and the
             *   event out of order, but just in case) 
             */
            sendAck(req);

            /* if we found the descriptor, cancel it */
            if (desc != nil)
                client.cancelDownload(desc);

            /* done */
            return;
        }

        /* if we didn't find it, return failure */
        if (desc == nil)
        {
            req.sendReply(404);
            return;
        }

        /* open the file */
        local fp = nil;
        try
        {
            fp = File.openRawFile(desc.tempFileName, FileAccessRead);
        }
        catch (Exception exc)
        {
            /* couldn't send the file - send a 404 */
            req.sendReply(404);
            return;
        }

        /* set up the download file headers */
        local headers = [
            'Content-Disposition: attachment; filename=<<desc.resName>>'
            ];

        try
        {
            /* send the file's contents */
            req.sendReply(fp, desc.mimeType, 200, headers);
        }
        catch (Exception exc)
        {
            /* ignore errors */
        }

        /* done with the file */
        fp.closeFile();

        /* 
         *   We've at least tried sending the file, so remove it from the
         *   download list.  This was just a temp file, so there's no need
         *   to keep it around even if we ran into an error sending it; if
         *   the send failed, the user can just repeat the operation that
         *   generated the file.  
         */
        client.cancelDownload(desc);
    }

    /* add a file to our list of downloadable files */
    addFile(fileType, client)
    {
        /* 
         *   Generate a server-side name template based on the file type.
         *   The name doesn't matter to us, but browsers will display it to
         *   the user, and many browsers use the server-side name as the
         *   default name for the newly downloaded file in the "Save File"
         *   dialog.  Many browsers also use the suffix to determine the
         *   file type, ignoring any Content-Type headers.  
         */
        local tpl = [FileTypeLog -> 'Script#.txt',
                     FileTypeData -> 'File#.dat',
                     FileTypeCmd -> 'Command#.txt',
                     FileTypeText -> 'File#.txt',
                     FileTypeBin -> 'File#.bin',
                     FileTypeT3Image -> 'Story#.t3',
                     FileTypeT3Save -> 'Save#.t3v',
                     * -> 'File#'][fileType];

        /* figure the mime type based on the file type */
        local mimeType = [FileTypeLog -> 'text/plain',
                          FileTypeCmd -> 'text/plain',
                          FileTypeT3Image -> 'application/x-t3vm-image',
                          FileTypeT3Save -> 'application/x-t3vm-state',
                          * -> 'application/octet-stream'][fileType];

        /* replace the '#' in the template with the next ID value */
        tpl = tpl.findReplace('#', toString(nextID++));

        /* create a new table entry */
        local desc = new DownloadTempFile(tpl, mimeType);

        /* add this download to each client */
        client.addDownload(desc);

        /* return the descriptor */
        return desc;
    }

    /* next available ID */
    nextID = 1
;

/* 
 *   Downloadable temporary file descriptor.  We create this object when
 *   the program calls inputFile() to ask for a writable file.  This lets
 *   the caller create and write a temporary file on the server side; when
 *   the caller is done with the file, we'll offer the file for download to
 *   the client through the UI.  
 */
class DownloadTempFile: object
    construct(res, mimeType)
    {
        tempFileName = new TemporaryFile();
        resName = res;
        resPath = '/clienttmp/<<res>>';
        timeCreated = getTime(GetTimeTicks);
        self.mimeType = mimeType;
    }

    /*
     *   File spec interface.  This allows the DownloadTempFile to be used as
     *   though it were a filename string.
     *   
     *   When the object is passed to one of the File.open methods, or to
     *   saveGame(), setScriptFile(), etc., the system will call our
     *   getFilename() method to determine the actual underlying file.
     *   We'll return our temporary file object.
     *   
     *   When the underlying file is closed, the system calls our
     *   closeFile() method to notify us.  
     */
    getFilename() { return tempFileName; }
    closeFile()
    {
        /* mark the file as ready for download */
        isReady = true;

        /* offering the file to the client as an HTTP download */
        webMainWin.offerDownload(self);
    }

    /* TemporaryFile object for the local temp file */
    tempFileName = nil

    /* root resource name, and full resource path */
    resName = nil
    resPath = nil

    /* MIME type */
    mimeType = nil

    /* creation timestamp, as a system tick count value */
    timeCreated = 0

    /* is the file ready for download? */
    isReady = nil

    /* this is a web temp file */
    isWebTempFile = true
;

/* ------------------------------------------------------------------------ */
/*
 *   Input event page.  The client javascript does a GET on this resource
 *   to send us an input event.  
 */
inputEventPage: WebResource
    vpath = '/webui/inputEvent'
    processRequest(req, query)
    {
        /* send the event to the main window object */
        webMainWin.receiveInputEvent(req, query);

        /* acknowledge the request */
        sendAck(req);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Input dialog event page.  The web UI sends a GET to this page when the
 *   user selects a button in an input dialog.  
 */
inputDialogPage: WebResource
    vpath = '/webui/inputDialog'
    processRequest(req, query)
    {
        /* send the event to the main window */
        webMainWin.receiveInputDialog(req, query);

        /* acknowledge the request */
        sendAck(req);
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   File dialog event page.  This page is used by the IFDB Storage Server
 *   file dialog to return information to the game UI.  The IFDB dialog
 *   page can't itself perform scripting actions on the enclosing dialog
 *   frame, since it's being served from a different domain - browsers
 *   prohibit cross-domain scripting for security reasons.  The IFDB dialog
 *   must therefore navigate back to a page within the game server domain
 *   in order to return information through scripting.  This is that page:
 *   when the IFDB page is ready to return information, it navigates its
 *   frame to this page, passing the return values in the request
 *   parameters.  Since this page is served by the game server, within the
 *   game server domain, the browser allows it to use scripting actions on
 *   its enclosing frame.  We finish the job by dismissing the dialog in
 *   the UI.
 */
inputFilePage: WebResource
    vpath = '/webui/inputFile'
    processRequest(req, query)
    {
        /* set the file selection in the main window */
        webMainWin.receiveFileSelection(req, query);

        /* 
         *   We have our response, so dismiss the file dialog.  Do this by
         *   sending back a page with script instructions to close the
         *   containing dialog window.
         */
        req.sendReply(
            '<html><body>'
            + '<script type="text/javascript">'
            + 'window.parent.dismissDialogById("inputFile");'
            + '</script>'
            + '</body></html>');
    }
;

/*
 *   Cancel the input dialog.  This is called from the UI directly to
 *   cancel the file selection, when the user closes the dialog through the
 *   enclosing main page UI rather than from within the dialog.  This is
 *   useful if the dialog page fails to load, for example.
 *   
 *   Note: the upload file dialog also uses this.  The upload dialog is
 *   basically a variation on the regular input file dialog.  
 */
inputFileCancel: WebResource
    vpath = '/webui/inputFileDismissed'
    processRequest(req, query)
    {
        /* note that the dialog has been dismissed */
        webMainWin.inputFileDismissed();

        /* acknowledge the request */
        sendAck(req);
    }
;

/*
 *   Receive results from the input file dialog 
 */
uploadFilePage: WebResource
    vpath = '/webui/uploadFileDialog'
    processRequest(req, query)
    {
        /* send the request to the main window */
        webMainWin.receiveFileUpload(req, query);

        /* send back a script to dismiss the dialog */
        req.sendReply(
            '<html><body>'
            + '<script type="text/javascript">'
            + 'window.parent.dismissDialogById("uploadFile");'
            + '</script>'
            + '</body></html>');
    }
;

/* ------------------------------------------------------------------------ */
/*
 *   Receive the client's screen name setting 
 */
setScreenNamePage: WebResource
    vpath = '/webui/setScreenName'
    processRequest(req, query)
    {
        /* set the name in the client session */
        local cli = ClientSession.find(req);
        if (cli != nil)
            cli.screenName = query['name'];

        /* acknowledge the request */
        sendAck(req);
    }
;

