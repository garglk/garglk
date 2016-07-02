/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmnet.h - TADS 3 networking
Function
  Defines the networking layer for TADS 3.  This is used for the web server
  configurations.  This is portable code implementing high-level network
  operations, such as an HTTP server, using the low-level socket and thread
  API defined in osifcnet.h.
Notes
  
Modified
  04/11/10 MJRoberts  - Creation
*/

#ifndef VMNET_H
#define VMNET_H


/* include this module only if the networking subsystem is enabled */
#ifdef TADSNET

#include <time.h>
#include <wchar.h>

#include "osifcnet.h"
#include "t3std.h"
#include "vmtype.h"
#include "vmglob.h"
#include "vmerr.h"
#include "vmdbg.h"
#include "vmglob.h"
#include "vmstrref.h"


/* ------------------------------------------------------------------------ */
/*
 *   Web server configuration.  We create a global singleton of this
 *   structure when the interpreter reads a web configuration file at
 *   startup.  
 */
class TadsNetConfig
{
public:
    TadsNetConfig()
    {
        first_var = 0;
        last_var = 0;
    }
    
    ~TadsNetConfig();

    /* read a config file */
    void read(osfildef *fp, class CVmMainClientIfc *clientifc);

    /* look up a configuration variable */
    const char *get(const char *name) const;

    /* look up a configuration variable, applying a default if not defined */
    const char *get(const char *name, const char *defval)
    {
        const char *val = get(name);
        return (val != 0 ? val : defval);
    }

    /* does the given configuration variable equal the given string? */
    const int match(const char *name, const char *strval)
    {
        const char *val = get(name, "");
        return strcmp(val, strval) == 0;
    }

    /* 
     *   Does the given configuration variable equal the given integer?  If
     *   the configuration variable isn't defined, returns false.  Otherwise,
     *   converts the configuration string value to an integer with atoi(),
     *   and compares the result to the given value.
     */
    const int match(const char *name, int intval)
    {
        const char *val = get(name);
        return (val != 0 ? atoi(val) == intval : FALSE);
    }

    /* set a variable */
    class TadsNetConfigVar *set(const char *name, const char *val);

protected:
    /* look up a variable */
    class TadsNetConfigVar *getvar(const char *name) const;

    /* head/tail of list of variables */
    class TadsNetConfigVar *first_var;
    class TadsNetConfigVar *last_var;
};

/* variable entry in a TadsNetConfig */
class TadsNetConfigVar
{
public:
    TadsNetConfigVar(const char *name, const char *val)
    {
        this->name = lib_copy_str(name);
        this->val = lib_copy_str(val);
        this->nxt = 0;
    }

    ~TadsNetConfigVar()
    {
        lib_free_str(name);
        lib_free_str(val);
    }
    
    /* get the name/value */
    const char *getname() const { return name; }
    const char *getval() const { return val; }

    /* change the value */
    void setval(const char *val)
    {
        lib_free_str(this->val);
        this->val = lib_copy_str(val);
    }

    /* does our name match the given name? */
    int matchname(const char *name)
        { return stricmp(name, this->name) == 0; }

    /* get/set the next item pointer */
    TadsNetConfigVar *getnext() const { return nxt; }
    void setnext(TadsNetConfigVar *nxt) { this->nxt = nxt; }

protected:
    char *name;
    char *val;
    TadsNetConfigVar *nxt;
};



/* ------------------------------------------------------------------------ */
/*
 *   Check a storage server API reply, and throw an error if appropriate.
 *   Storage server API replies conventionally are plain text buffers of the
 *   form:
 *   
 *      Code Message
 *   
 *   where 'Code' is an alphanumeric token giving an error code, and Message
 *   is a human-readable result message.  On success, the Code token is "OK";
 *   other token strings indicate errors.
 *   
 *   Functions that also return regular body data (such as GETFILE) return
 *   the status message in an X-IFDBStorage-Status header instead of the
 *   body.  If the status header is present, we'll use that rather than
 *   checking the reply body.
 *   
 *   If the HTML status, headers, or reply indicates an error, we'll throw a
 *   storage server run-time error, so this won't return.  On success, we
 *   simply return.  
 */
void vmnet_check_storagesrv_reply(VMG_ int htmlstat, CVmDataSource *reply,
                                  const char *headers);

/*
 *   Retrieve the storage server status from the reply.  This returns an
 *   allocated string buffer containing the status code as the first
 *   space-delimited token, with a printable error message following.
 *   
 *   The status code is "OK" on success.  If the request failed due to a
 *   storage server error, the first token is a non-numeric error ID.  If the
 *   request failed due to an HTTP, the first token is the numeric HTTP
 *   status.  If the request failed due to a lower-level network error, the
 *   code is a negative numeric code giving the internal OS_HttpClient ErrXxx
 *   code.
 *   
 *   The caller must delete the returned buffer with t3free().  
 */
char *vmnet_get_storagesrv_stat(VMG_ int htmlstat, CVmDataSource *reply,
                                const char *headers);

/*
 *   Check the reply code returned by vmnet_get_storagesrv_reply().  On
 *   success, frees the status code buffer and returns.  On failure, frees
 *   the status code buffer and throws a StorageServerError exception.  Note
 *   that we free the buffer in either case.  
 */
void vmnet_check_storagesrv_stat(VMG_ char *stat);


/* ------------------------------------------------------------------------ */
/*
 *   Master thread list.  This is a portable helper object for keeping a list
 *   of threads.  The OS implementation can use this to maintain a list of
 *   all of the threads the program has started.  This is useful mainly so
 *   that the program can wait for background threads to exit on their own
 *   before terminating the overall process.  It could also be useful for
 *   debugging purposes, such as to display a list of active threads on the
 *   console at various times.
 *   
 *   If the OS layer uses this object, OS_Thread should register each new
 *   thread in its constructor, and unregister each outgoing thread in its
 *   destructor.  Doing so is optional - it's purely for the OS layer's
 *   benefit, so if the OS layer doesn't need the master thread list
 *   information, it can ignore this object.  
 */

/*
 *   thread list link 
 */
struct TadsThreadLink
{
    TadsThreadLink(OS_Thread *thread, TadsThreadLink *nxt)
    {
        /* save a weak reference to the thread */
        this->thread = thread->get_weak_ref();
        this->nxt = nxt;
    }

    ~TadsThreadLink()
    {
        thread->release_ref();
    }

    OS_Thread *get_thread()
    {
        return (OS_Thread *)thread->ref();
    }

    /* weak reference to our thread */
    CVmWeakRef *thread;

    /* next link in list */
    TadsThreadLink *nxt;
};


/*
 *   thread list 
 */
class TadsThreadList: public CVmRefCntObj
{
public:
    TadsThreadList()
    {
        mu = new OS_Mutex();
        head = 0;
    }

    ~TadsThreadList()
    {
        mu->release_ref();
    }

    /* 
     *   add a thread to the master list - the OS_Thread constructor must
     *   call this 
     */
    void add(OS_Thread *thread)
    {
        /* lock the list */
        mu->lock();

        /* link the thread into our list */
        head = new TadsThreadLink(thread, head);

        /* done with the list lock */
        mu->unlock();
    }

    /* clean the list - clear dead threads */
    void clean()
    {
        /* lock the list */
        mu->lock();

        /* 
         *   Unlink the thread from the list.  It's possible that it's not in
         *   the list any longer, since wait_all() removes threads before
         *   waiting for them to exit.  
         */
        TadsThreadLink *cur, *prv, *nxt;
        for (prv = 0, cur = head ; cur != 0 ; cur = nxt)
        {
            /* remember the next thread */
            nxt = cur->nxt;
            
            /* check to see if this thread is still alive */
            if (cur->thread->test())
            {
                /* it's still alive - keep it in the list */
                prv = cur;
            }
            else
            {
                /* this thread is gone - remove the link */
                if (prv != 0)
                    prv->nxt = nxt;
                else
                    head = nxt;

                /* delete the link */
                delete cur;
            }
        }

        /* done with the list lock */
        mu->unlock();
    }

    /* 
     *   Wait for all threads to exit.  The 'timeout' (given in milliseconds)
     *   is applied to each individual thread.  Returns true if all threads
     *   exited successfully, false if one or more threads timed out.  
     */
    int wait_all(long timeout)
    {
        /* presume success */
        int ok = TRUE;

        /* keep going until we're out of threads */
        for (;;)
        {
            /* lock the list and pull off the first thread */
            mu->lock();
            TadsThreadLink *link = head;

            /* if we got an element, remove it from the list */
            if (link != 0)
            {
                /* 
                 *   Unlink it from the list.  Note that our local variable
                 *   't' has now assumed the reference count that the list
                 *   previously held on the thread.  
                 */
                head = link->nxt;
            }

            /* done with the list lock */
            mu->unlock();

            /* if there are no more threads, we're done */
            if (link == 0)
                break;

            /* if this thread is still alive, wait for it to exit */
            OS_Thread *t = link->get_thread();
            if (t != 0)
            {
                /* wait for this thread to exit */
                if (t->wait(timeout) != OSWAIT_EVENT)
                {
                    /* 
                     *   the wait timed out or failed - note that we have at
                     *   least one thread that didn't exit successfully
                     *   within the timeout 
                     */
                    ok = FALSE;
                }

                /* we're done with the thread */
                t->release_ref();
            }

            /* delete the link */
            delete link;
        }

        /* return the status result */
        return ok;
    }

protected:
    /* head of thread list */
    TadsThreadLink *head;

    /* list mutex */
    OS_Mutex *mu;
};




/* ------------------------------------------------------------------------ */
/*
 *   Network service registration.
 *   
 *   The usual way that a client finds a server on the Web is via an Internet
 *   address and a "well-known" port number.  For example, any time you
 *   connect to a Web server, you generally connect to port 80, which is the
 *   standard port for HTTP servers.
 *   
 *   TADS games DON'T use these standard well-known ports.  This is an
 *   important part of the design, because it allows many TADS games to run
 *   on a single shared server.  If each game tried to take over the same
 *   well-known port for its network listener, you could only run one game
 *   per machine (or per network adapter, anyway).  Instead of using
 *   well-known ports, then, TADS game servers use port numbers assigned
 *   dynamically by the operating system.  A game can't know its port number
 *   until it's actually running.  Therefore, we need some mechanism to
 *   convey the port number to the client so that the client knows where to
 *   connect.
 *   
 *   Registration is the solution.  When a game starts a server, it registers
 *   that server, by sending its IP address and listening port number to a
 *   registration server.  The registration server makes an entry in its
 *   database of active servers, and then makes this information available to
 *   clients through its own well-known port.  The registration server CAN
 *   use a well-known port because we only need one registration server per
 *   machine.  
 */


/* ------------------------------------------------------------------------ */
/*
 *   Server manager.  This is the global object that manages all of the
 *   servers in the process.  
 */
class TadsServerManager: public CVmRefCntObj
{
public:
    TadsServerManager();
    ~TadsServerManager();

    /* 
     *   Generate a random ID string - this can be used for anything
     *   requiring a universally unique identifier, such as a session ID.
     *   
     *   'obj' is a pointer to the object on whose behalf we're constructing
     *   the string.  This is used to contribute to the randomness of the
     *   result, but can be null if no object is involved.
     *   
     *   This returns an allocated buffer, which the caller must free with
     *   lib_free_str() when done with it.  
     */
    static char *gen_rand_id(void *obj);
    
    /* get a random number */
    ulong rand();

protected:
    /* our ISAAC random number generator */
    struct isaacctx *ic;

    /* resource protection mutex */
    OS_Mutex *mutex;
};


/* ------------------------------------------------------------------------ */
/*
 *   Network listener.  This object creates a thread that binds to a network
 *   port and listens for incoming connections.  
 */
class TadsListener: public CVmRefCntObj
{
public:
    /* 
     *   Create a web server listener.  This binds to the given network port,
     *   then launches the given listener thread to listen for new
     *   connections on the port.
     */
    static TadsListener *launch(const char *hostname, ushort portno,
                                class TadsListenerThread *thread);

    /*
     *   Shut down the listener.  This sends a control message to the
     *   listener thread telling it to shut down.  The listener will close
     *   its network port and exit its thread.
     *   
     *   This returns immediately without waiting for the listener to finish
     *   shutting down.  If you want to wait until the thread has exited,
     *   wait for the thread object.  
     */
    void shutdown();

    /* the listener thread object */
    class TadsListenerThread *get_thread() const { return thread; }

protected:
    /* 
     *   construction - this is protected because callers create this object
     *   via the static method launch() 
     */
    TadsListener(TadsListenerThread *thread);

    /* deletion - protected because we're managed by reference counting */
    ~TadsListener();

    /* my thread object */
    class TadsListenerThread *thread;
};

/*
 *   Listener thread.  This is the generic class for listener threads; it
 *   must be subclassed for each specific type of network object.  
 */
class TadsListenerThread: public OS_Thread
{
    friend class TadsListener;
    friend class TadsServerThread;
    
public:
    /* construction */
    TadsListenerThread(OS_Event *quit_evt)
    {
        /* no port yet */
        port = 0;

        /* no error message yet */
        errmsg = 0;

        /* 
         *   use the caller's quit event, if they supplied one, otherwise
         *   create our own; if we create one, it's sticky - once signaled,
         *   it stays signaled, no matter how many threads are waiting on it
         */
        if (quit_evt == 0)
            this->quit_evt = new OS_Event(TRUE);
        else
            (this->quit_evt = quit_evt)->add_ref();

        /* create our private shutdown event */
        shutdown_evt = new OS_Event(TRUE);

        /* create our resource protection mutex */
        mutex = new OS_Mutex();

        /* we haven't started any server threads yet */
        servers = 0;

        /* note the time we started running */
        os_time(&start_time);

        /* start with thread #1 */
        next_thread_id = 1;

        /* generate a random password */
        password = TadsServerManager::gen_rand_id(this);
    }

    /* destruction */
    ~TadsListenerThread();

    /*
     *   Create the server thread object.  This must be subclassed for each
     *   type of server to create the appropriate thread subclass.  
     */
    virtual class TadsServerThread *create_server_thread(OS_Socket *s) = 0;

    /* get the IP address and port number */
    int get_local_addr(char *&ip, int &portno) const
        { return port != 0 ? port->get_local_addr(ip, portno) : 0; }

    /* set the listener port - we assume the caller's reference */
    void set_port(OS_Listener *p) { port = p; }

    /* main thread entrypoint */
    void thread_main();

    /* 
     *   Check for an error message.  If the server encounters an error it
     *   can't recover from, it'll store a status message and exit the
     *   listener thread. 
     */
    const char *get_errmsg() const { return errmsg; }

    /* generate a human-readable status report listing my threads */
    void list_threads(struct NetString *buf);

    /* get the control password */
    const char *get_password() const { return password; }

    /* get the start time as an ASCII string */
    const char *asc_start_time() const
        { return asctime(os_localtime(&start_time)); }

    /* get the application-wide 'quit' event object */
    OS_Event *get_quit_evt() const { return quit_evt; }

    /* get the listener shutdown event */
    OS_Event *get_shutdown_evt() const { return shutdown_evt; }

protected:
    /* add a thread to our list */
    void add_thread(class TadsServerThread *t);

    /* remove a thread from our list */
    void remove_thread(class TadsServerThread *t);

    /* time the server started running */
    os_time_t start_time;

    /* 
     *   Password: this is a random string generated at startup, to secure
     *   network access to control functions.  The idea is that the server
     *   displays this on its console, so only someone with console access to
     *   the server will be able to also gain remote access to the server
     *   control functions.  
     */
    char *password;

    /* next thread serial number */
    int next_thread_id;

    /* our listener port object */
    OS_Listener *port;

    /* 
     *   Application-wide quit event.  This is the global event object that
     *   signals application termination.  We'll abort any blocking operation
     *   when this event is signaled so that we can promptly terminate the
     *   listener thread when the application is being terminated.
     */
    OS_Event *quit_evt;

    /*
     *   Listener shutdown event.  This is the local event object that lets
     *   our owner tell us to shut down the listener thread.  This only
     *   applies to this thread, not to the rest of the application, so it
     *   can be used to selectively terminate this single listener while
     *   leaving other server threads running. 
     */
    OS_Event *shutdown_evt;

    /* 
     *   error message - if the server encounters an error that it can't
     *   recover from, it will store status information here and shut down 
     */
    char *errmsg;

    /* head of our list of active server threads */
    class TadsServerThread *servers;

    /* mutex protecting our shared resources against concurrent access */
    OS_Mutex *mutex;
};


/*
 *   Base class for server threasds.
 */
class TadsServerThread: public OS_Thread
{
    friend class TadsListenerThread;
    
public:
    TadsServerThread(TadsListenerThread *l, OS_Socket *s)
    {
        /* we take over the socket reference from the caller */
        socket = s;

        /* remember the client's IP address */
        socket->get_peer_addr(client_ip, client_port);

        /* remember the listener thread */
        listener = l;
        l->add_ref();

        /* create our resource protection mutex */
        mutex = new OS_Mutex();

        /* set the initial state string */
        state = new StringRef("Initializing");
    }

    ~TadsServerThread()
    {
        /* release our resources */
        socket->release_ref();
        listener->release_ref();
        mutex->release_ref();
        state->release_ref();
        if (client_ip != 0)
            t3free(client_ip);
    }

    /* get the client IP address */
    const char *get_client_ip() const { return client_ip; }
    int get_client_port() const { return client_port; }

    /* main thread entrypoint */
    void thread_main();

    /*
     *   Process a request.  The main server loop simply calls this
     *   repeatedly as long as it returns true.  This should read a request
     *   from the socket and send the response, then return a success or
     *   failure indication to the caller.  Returns true on success, false if
     *   an unrecoverable error occurs.  A false return tells the server loop
     *   to close the socket and terminate the thread.  
     */
    virtual int process_request() = 0;

    /*
     *   Read data from our peer.  This blocks until at least one byte is
     *   available, then returns as much data as can be read without
     *   blocking.  On success, returns the number of bytes read.  On error,
     *   returns -1.
     *   
     *   If 'buf' is null, we'll read and discard data until we've read
     *   'buflen' bytes.
     *   
     *   'minlen' gives the minimum number of bytes desired.  This can be
     *   less than the buffer length, since we might not know in advance how
     *   many bytes the client will be sending.  We'll read up to the buffer
     *   size, but we won't return until we read the minimum size (or
     *   encounter an error or timeout).  
     */
    long read(char *buf, size_t buflen, long minlen,
              unsigned long timeout = OS_FOREVER);

    /*
     *   Send data to our peer.  This blocks until the request completes, but
     *   we'll abort immediately if the 'quit' event in our listener fires.
     *   Returns true on success, false on error or a 'quit' signal.  
     */
    int send(const char *buf, size_t len);
    int send(const char *buf) { return send(buf, strlen(buf)); }

    /* get the last send/receive error from the socket */
    int last_error() const { return socket->last_error(); }

    /* 
     *   Get the state string.  The caller must release the reference when
     *   done with it. 
     */
    StringRef *get_state() const
    {
        /* make sure no one changes it under us while we're working */
        mutex->lock();

        /* make a copy for the caller */
        StringRef *s = state;
        s->add_ref();

        /* our copy is safe now */
        mutex->unlock();

        /* return the caller's copy */
        return s;
    }

    /* set the current run state */
    void set_run_state(const char *s)
    {
        /* set up a StringRef for the string */
        StringRef *r = new StringRef(s);

        /* set it as the new state */
        set_run_state(r);

        /* done with our reference to the new StringRef */
        r->release_ref();
    }

    void set_run_state(StringRef *s)
    {
        /* lock against concurrent access while working */
        mutex->lock();

        /* release the old state string, and save the new one */
        if (state != 0)
            state->release_ref();
        if ((state = s) != 0)
            s->add_ref();

        /* done with the mutex */
        mutex->unlock();
    }

    /* close my socket */
    void close_socket()
    {
        if (socket != 0)
            socket->close();
    }

protected:
    /* 
     *   Thread ID - this is a serial number set by the listener when we
     *   start up, used for human-readable identification.  It has no other
     *   significance; in particular, it's NOT an operating system thread
     *   handle or ID.  
     */
    int thread_id;

    /* 
     *   our connection socket - this is the two-way network channel we use
     *   to communicate with our client 
     */
    OS_Socket *socket;

    /* our listener */
    TadsListenerThread *listener;

    /* next thread in our listener's list of threads */
    TadsServerThread *next_server;

    /* resource protection mutex */
    OS_Mutex *mutex;

    /* current run state message */
    StringRef *state;

    /* client IP address and port */
    char *client_ip;
    int client_port;
};


/* ------------------------------------------------------------------------ */
/*
 *   HTTP server thread
 */
class TadsHttpServerThread: public TadsServerThread
{
public:
    /* construction */
    TadsHttpServerThread(class TadsHttpListenerThread *l,
                         TadsMessageQueue *q, long ulim, OS_Socket *s);

    /* deletion */
    ~TadsHttpServerThread();

    /* get my listener thread object */
    TadsHttpListenerThread *get_listener() const
        { return (TadsHttpListenerThread *)listener; }

    /* process a request */
    int process_request();

    /* 
     *   Send a simple HTTP response.  Returns true on success, false on
     *   error.  The status code and mime type strings are required.  The
     *   message body is optional; if it's null, we'll simply send the
     *   headers with no body.  If there is a message body, we'll send it
     *   with a Content-Length header.  The extra headers are optional; if
     *   provided, this must be in standard header format, with each header
     *   (INCLUDING the last one) terminated by a CR-LF (\r\n) sequence.  If
     *   this is null we'll only include a set of standard headers describing
     *   the message body (content-type, content-length, cache-control,
     *   connection).  
     */
    int send_simple(const char *status_code, const char *mime_type,
                    const char *msg_body, size_t msg_len,
                    const char *extra_headers);

    /* send a simple message body */
    int send_simple(const char *status_code, const char *mime_type,
                    const char *msg_body)
    {
        return send_simple(status_code, mime_type,
                           msg_body, strlen(msg_body), 0);
    }

    /* send the contents of a file as the message body */
    int send_file(const char *fname, const char *mime_type);

protected:
    /* 
     *   Read until we reach the desired number of newlines.  The states are
     *   0->base, 1->first CR, 2->first LF, 3->second CR, 4->second LF.  To
     *   stop after one contiguous newline, stop at state 2; to stop after
     *   two newlines, stop at state 4.  
     */
    int read_to_nl(StringRef *dst, long startofs,
                   int init_state, int end_state);

    /* the message queue */
    TadsMessageQueue *queue;

    /* upload limit */
    long upload_limit;
};


/* ------------------------------------------------------------------------ */
/*
 *   HTTP server listener 
 */
class TadsHttpListenerThread: public TadsListenerThread
{
public:
    TadsHttpListenerThread(vm_obj_id_t srv_obj,
                           class TadsMessageQueue *q, long upload_limit);

    /* our associated server thread class is TadsHttpServer */
    virtual class TadsServerThread *create_server_thread(OS_Socket *s)
        { return new TadsHttpServerThread(this, queue, upload_limit, s); }

    /* 
     *   Get the HTTPServer object that created the listener.
     *   
     *   For thread safety, this routine should only be called from the main
     *   VM thread.  The garbage collector can delete the server object and
     *   thus invalidate the value returned here.  The GC runs in the main VM
     *   thread, so it's always safe to interrogate this value from the main
     *   thread.
     */
    vm_obj_id_t get_server_obj() const { return srv_obj; }

    /* 
     *   Detach from the HTTPServer object.  The HTTPServer calls this when
     *   it's about to be deleted by the garbage collector.  We have a
     *   reference on the HTTPServer object, but we're not ourselves a
     *   garbage-collected object, so our reference won't keep the HTTPServer
     *   alive.  It can thus be collected while we're still pointing to it.
     *   To deal with this, the HTTPServer lets us know when it's about to be
     *   deleted, so that we can clear our reference.
     *   
     *   This routine should only be called from the main VM thread.  This is
     *   designed to be called from the HTTPServer object's notify_delete()
     *   method, which is called by the garbage collector, which always runs
     *   in the main VM thread.
     */
    void detach_server_obj() { srv_obj = VM_INVALID_OBJ; }

protected:
    ~TadsHttpListenerThread();
    
    /* the message queue we use to handle incoming requests */
    class TadsMessageQueue *queue;

    /* upload limit for each incoming request */
    long upload_limit;

    /* the HTTPServer object that owns the listener */
    vm_obj_id_t srv_obj;
};



/* ------------------------------------------------------------------------ */
/*
 *   XML helper class 
 */
class TadsXml
{
public:
    /* given an XML buffer, find the end of the <?XML?> prefix */
    const char *strip_xml_header(const char *buf);
};


/* ------------------------------------------------------------------------ */
/*
 *   Message queue.  Server threads handle requests by sending messages to
 *   the main thread, via the message queue.  This approach allows the
 *   network connections to be handled by dedicated threads, while keeping
 *   the byte-code program itself single-threaded.  The queue handles the
 *   communications between threads, and also serializes requests so that
 *   they can be serviced by a single thread.  
 */

/* 
 *   Base message object 
 */
class TadsMessage: public CVmRefCntObj
{
public:
    TadsMessage(const char *typ, OS_Event *quit_evt)
    {
        /* remember the message type */
        this->typ = typ;
        
        /* we're not in a queue yet */
        nxt = 0;

        /* set up an event to signal completion of the message */
        ev = new OS_Event(FALSE);
        completed = FALSE;

        /* remember the quit event */
        if ((this->quit_evt = quit_evt) != 0)
            quit_evt->add_ref();
    }
    
    virtual ~TadsMessage()
    {
        /* destroy our completion event */
        ev->release_ref();

        /* forget our quit event */
        if (quit_evt != 0)
            quit_evt->release_ref();
    }

    /* 
     *   wait for completion: returns true on success, false if the wait
     *   failed (due to error, timeout, or Quit) 
     */
    int wait_for_completion(unsigned long timeout)
    {
        /* wait on the event, or the quit event */
        OS_Waitable *w[] = { ev, quit_evt };
        return OS_Waitable::multi_wait(quit_evt != 0 ? 2 : 1, w, timeout)
            == OSWAIT_EVENT + 0;
    }

    /* mark the message as completed */
    void complete()
    {
        /* set the completion event */
        ev->signal();

        /* flag it internally */
        completed = TRUE;
    }

    /* message type - use the name of the class */
    const char *typ;

    /* next message in queue */
    TadsMessage *nxt;

    /* 
     *   completion event - this is signaled when the message has been
     *   processed by the recipient, and if a reply is expected, the reply is
     *   available 
     */
    OS_Event *ev;

    /* 
     *   the 'quit' event - our owner signals this event to tell us to shut
     *   down the server without doing any more processing
     */
    OS_Event *quit_evt;

    /* flag: has completion been signaled yet? */
    int completed;
};

/*
 *   Safely cast an event message
 */
#define cast_tads_message(cls, msg) \
    (msg != 0 && strcmp((msg)->typ, #cls) == 0 ? (cls *)msg : 0)

/*
 *   Network Event message.  This is the base class for messages used within
 *   the TADS 3 getNetEvent() queue for sending messages to the byte-code
 *   program.  
 */
class TadsEventMessage: public TadsMessage
{
public:
    TadsEventMessage(OS_Event *quit_evt)
        : TadsMessage("TadsEventMessage", quit_evt)
        { }

    /*
     *   Set up the event object for the byte-code program.  This pushes the
     *   constructor arguments onto the run-time stack, and returns the
     *   appropriate NetEvent subclass object ID (from G_predef).
     *   
     *   '*argc' is to be filled in with the number of constructor arguments
     *   pushed.  Note that the caller will push one additional constructor
     *   argument giving the request type code (which is common to all
     *   NetEvent subclass constructors), so this routine doesn't have to
     *   push that value.
     *   
     *   '*evt_code' is to be filled in with the event type code.  This is an
     *   integer giving the type of the event, as defined in the TADS header
     *   include/tadsnet.h - see the NetEvXxx code list.
     *   
     *   Each server type must subclass this to generate the suitable
     *   information for the request.  
     */
    virtual vm_obj_id_t prep_event_obj(VMG_ int *argc, int *evt_code) = 0;
};

/* 
 *   Message queue. 
 */
class TadsMessageQueue: public CVmRefCntObj
{
public:
    TadsMessageQueue(OS_Event *quit_evt)
    {
        /* create our mutex object */
        mu = new OS_Mutex();

        /* create our event - this signals when a message arrives */
        ev = new OS_Event(FALSE);

        /* no messages in the queue yet */
        head = tail = 0;

        /* remember my global quit event */
        if ((this->quit_evt = quit_evt) != 0)
            quit_evt->add_ref();
    }

    /* are we in the process of shutting down the server? */
    int is_quitting()
    {
        return quit_evt != 0 && quit_evt->test();
    }

    /* 
     *   Add a message to the queue, without waiting for completion.  This is
     *   a one-way send: posting effectively transfers ownership of the
     *   reference on the message to the queue, and thence to the recipient.
     *   The caller thus can't access the message after posting it - for
     *   their purposes it's effectively gone after they send it.  If the
     *   caller does want to hang onto its own reference to the message after
     *   sending it, just use add_ref() on the message (BEFORE posting, of
     *   course, in keeping with the usual ref-count transaction rules).  
     */
    void post(TadsMessage *m)
    {
        /* synchronize on the mutex */
        mu->lock();

        /* link the message at the end of the queue */
        if (tail != 0)
            tail->nxt = m;
        else
            head = m;
        tail = m;
        m->nxt = 0;

        /* 
         *   if the message doesn't have a 'quit' event, and we do, copy our
         *   quit event to the message 
         */
        if (m->quit_evt == 0 && quit_evt != 0)
            (m->quit_evt = quit_evt)->add_ref();

        /* signal that a message is available in the queue */
        ev->signal();

        /* done with the mutex */
        mu->unlock();
    }

    /* 
     *   Send a message: post it to the queue and wait for completion.
     *   Returns true on success, false if the wait failed (due to error or
     *   timeout).
     *   
     *   Unlike post(), the caller explicitly keeps its own reference to the
     *   message with send(), because the message object also provides
     *   storage for the reply (if any).  
     */
    int send(TadsMessage *m, unsigned long timeout)
    {
        /*
         *   Add a reference to the message on behalf of our caller.  The
         *   caller's original reference is about to be transfered to the
         *   queue (and later to the recipient) via post(), but the caller
         *   also wants to hang on to the message, so it needs a second
         *   reference for its own copy. 
         */
        m->add_ref();

        /* post the message */
        post(m);
        
        /* wait for completion */
        return m->wait_for_completion(timeout);
    }

    /* 
     *   Wait for a message.  Blocks until a message is available in the
     *   queue, or the timeout expires.  If there's a message, removes the
     *   message from the queue and returns OSWAIT_EVENT.  Otherwise, returns
     *   another OSWAIT_xxx code indicating what happened.
     *   
     *   The message queue has a 'quit' event object that's used to signal a
     *   general shutdown.  If this event fires, we'll return OSWAIT_EVENT+1
     *   and fill in *msgp with null.
     *   
     *   The message queue also has a 'debug break' event that's used to
     *   signal that the user wants to pause execution and break into the
     *   debugger.  If this event is signaled, we'll return OSWAIT_EVENT+2
     *   and fill in *msgp with null.
     *   
     *   If we return a message, the caller takes over our reference on it.
     *   The caller is thus responsible for calling release_ref() when done
     *   with the message object.  
     */
    int wait(VMG_ unsigned long timeout, TadsMessage **msgp);

    /* 
     *   Pull the next message out of the queue without waiting.  The caller
     *   assumes our reference on the message, so they must call Release()
     *   when done with it.  
     */
    TadsMessage *get()
    {
        /* synchronize on the mutex */
        mu->lock();

        /* stash the first message for returning to the caller */
        TadsMessage *m = head;

        /* pull it off the queue (if there's a message at all) */
        if (m != 0)
        {
            head = head->nxt;
            if (head == 0)
                tail = 0;
        }

        /* done with the mutex */
        mu->unlock();

        /* return the message */
        return m;
    }

    /*
     *   Abandon a message: release any references the queue has on the given
     *   message.  This can be used if the sender times out waiting for a
     *   response, to ensure that the queue won't keep the message in memory.
     */
    void abandon(TadsMessage *msg)
    {
        /* lock the queue while working */
        mu->lock();

        /* scan the queue for this message */
        TadsMessage *cur, *prv;
        for (cur = head, prv = 0 ; cur != 0 ; prv = cur, cur = cur->nxt)
        {
            /* if this is the message we're looking for, unlink it */
            if (cur == msg)
            {
                /* unlink the message */
                if (prv != 0)
                    prv->nxt = cur->nxt;
                else
                    head = cur->nxt;

                /* if it's the tail, move the tail back one */
                if (cur == tail)
                    tail = prv;

                /* release our reference on the message */
                cur->release_ref();

                /* no need to keep looking */
                break;
            }
        }

        /* done with the queue lock */
        mu->unlock();
    }

    /*
     *   Flush the queue: discard any messages in our list.
     */
    void flush()
    {
        /* keep going until the queue is empty */
        for (;;)
        {
            /* lock the queue, and pull the first element off the list */
            mu->lock();
            TadsMessage *m = head;

            /* if we got an element, unlink it */
            if (m != 0)
            {
                head = m->nxt;
                if (head == 0)
                    tail = 0;
            }

            /* unlock the queue */
            mu->unlock();

            /* if we're out of queue elements, we're done */
            if (m == 0)
                break;

            /* release this message */
            m->release_ref();
        }
    }

    /* get my message arrival event */
    OS_Event *get_event_obj() { return ev; }

    /* get my quit event object */
    OS_Event *get_quit_evt() { return quit_evt; }

protected:
    /* mutex for access to our statics */
    OS_Mutex *mu;

    /* event for signaling a message arrival */
    OS_Event *ev;

    /* 
     *   the server's general 'quit' event - the server signals this event to
     *   tell message queues to shut down without further processing 
     */
    OS_Event *quit_evt;

    /* head/tail of message queue */
    TadsMessage *head, *tail;

    ~TadsMessageQueue()
    {
        /* discard any messages still in the queue */
        flush();

        /* destroy our synchronization resources */
        mu->release_ref();
        ev->release_ref();
        if (quit_evt != 0)
            quit_evt->release_ref();
    }
};


/* ------------------------------------------------------------------------ */
/*
 *   HTTP request header.  
 */
struct TadsHttpRequestHeader
{
    /* 
     *   Initialize given the start of a header line; returns a pointer to
     *   the start of the next header line.  This modifies the buffer in
     *   place: we add a null at the end of the header name, and we add a
     *   null at the newline at the end of the header.  
     */
    TadsHttpRequestHeader(char *&p, int parse)
    {
        /* no next item in the list yet */
        nxt = 0;

        /* the name is always at the start of the line */
        name = p;

        /* presume we won't find a value - set it to blank */
        value = "";

        /* parse out the name and value, if desired */
        if (parse)
        {
            /* 
             *   find the end of the header name - it ends at the first
             *   whitespace or colon character 
             */
            for ( ; *p != '\0' && strchr(":\r\n \t", *p) == 0 ; ++p) ;
            
            /* check what we found */
            if (*p == ':')
            {
                /* found the delimiting colon - null it out and skip spaces */
                for (*p++ = '\0' ; isspace(*p) ; ++p) ;
                
                /* the value is the rest of the line */
                value = p;
            }
            else if (isspace(*p))
            {
                /* stopped at a space - null it out and skip any more spaces */
                for (*p++ = '\0' ; isspace(*p) ; ++p) ;
                
                /* make sure we're at the colon */
                if (*p == ':')
                {
                    /* skip the colon and any subsequent spaces */
                    for (++p ; isspace(*p) ; ++p) ;
                    
                    /* the value starts here */
                    value = p;
                }
            }
        }
        
        /* find the end of the line */
        for ( ; *p != '\0' && (*p != '\r' || *(p+1) != '\n') ; ++p) ;

        /* if we found the CR-LF, null-terminate and skip the pair */
        if (*p == '\r' && *(p+1) == '\n')
        {
            *p = '\0';
            p += 2;
        }
    }

    ~TadsHttpRequestHeader()
    {
        /* delete the rest of the list */
        if (nxt != 0)
            delete nxt;
    }

    /*
     *   Simplify multi-line headers into the single-line equivalents 
     */
    static void fix_multiline_headers(StringRef *hdrs, int start_ofs)
    {
        /*
         *   Look for CR LF <space>+ <^space> sequences, and delete the line
         *   breaks in each one.  
         */
        char *p, *dst, *endp = hdrs->getend();
        for (dst = p = hdrs->get() + start_ofs ; p < endp ; )
        {
            /* check for a line break */
            if (*p == '\r' && *(p+1) == '\n')
            {
                /* 
                 *   if the next line starts with whitespace, it's a
                 *   continuation line; otherwise it's a new header 
                 */
                if (isspace(*(p+2) && *(p+2) != '\r' && *(p+2) != '\n'))
                {
                    /* 
                     *   it's a continuation line - replace the CR-LF with a
                     *   single space, then skip all of the whitespace at the
                     *   start of this line 
                     */
                    *dst++ = ' ';
                    for (p += 2 ; isspace(*p) && *p != '\r' && *p != '\n' ;
                         ++p) ;
                }
                else
                {
                    /* it's not a continuation line, so copy it as-is */
                    *dst++ = *p++;
                    *dst++ = *p++;
                }
            }
            else if (*p == '\r' || *p == '\n')
            {
                /* unpaired CR or LF - convert it to a space */
                *dst++ = ' ';
                ++p;
            }
            else
            {
                /* ordinary character - copy it unchanged */
                *dst++ = *p++;
            }
        }
        
        /* set the new size of the headers */
        hdrs->truncate(dst - hdrs->get());
    }
    
    /* parse headers from a StringRef starting at the given offset */
    static void parse_headers(TadsHttpRequestHeader *&hdr_list,
                              TadsHttpRequestHeader *&hdr_tail,
                              int parse_first,
                              StringRef *str, long ofs)
    {
        /* fix multiline headers */
        fix_multiline_headers(str, ofs);

        /* set up at the starting offset */
        char *p = str->get() + ofs;
        
        /* if we're already at a blank line, there are no headers */
        if (*p == '\0' || *p == '\r' || *p == '\n')
            return;
        
        /* 
         *   if there aren't any headers yet, parse the first one; otherwise
         *   we'll just add to the existing list 
         */
        if (hdr_list == 0)
            hdr_list = hdr_tail = new TadsHttpRequestHeader(p, parse_first);
        
        /* now parse until we find a blank line */
        while (*p != '\r' && *p != '\n' && *p != '\0')
            hdr_tail = hdr_tail->nxt = new TadsHttpRequestHeader(p, TRUE);
    }

    /* find a header in the list following this item */
    const char *find(const char *name)
    {
        /* scan the list, starting with this element */
        for (TadsHttpRequestHeader *h = this ; h != 0 ; h = h->nxt)
        {
            /* if this name matches, return the value */
            if (stricmp(h->name, name) == 0)
                return h->value;
        }
        
        /* didn't find a match */
        return 0;
    }

    /* name - pointer into a buffer owned by the request object */
    const char *name;

    /* value - pointer into a buffer owned by the request object */
    const char *value;

    /* next header in list */
    TadsHttpRequestHeader *nxt;
};

/* ------------------------------------------------------------------------ */
/*
 *   HTTP Request Message. 
 */
class TadsHttpRequest: public TadsEventMessage
{
public:
    TadsHttpRequest(TadsHttpServerThread *t,
                    const char *verb, size_t verb_len,
                    StringRef *hdrs, TadsHttpRequestHeader *hdr_list,
                    StringRef *body, int overflow,
                    const char *resource_name, size_t res_name_len,
                    OS_Event *quit_evt)
        : TadsEventMessage(quit_evt)
    {
        /* remember the server thread */
        if ((this->thread = t) != 0)
            t->add_ref();

        /* remember the verb and resource name */
        this->verb = new StringRef(verb, verb_len);
        this->resource_name = new StringRef(resource_name, res_name_len);

        /* keep a copy of the headers */
        this->headers = hdrs;
        hdrs->add_ref();

        /* keep a copy of the body */
        this->overflow = overflow;
        if ((this->body = body) != 0)
            body->add_ref();

        /* take ownership of the parsed header list */
        this->hdr_list = hdr_list;
    }

    ~TadsHttpRequest()
    {
        /* done with our thread and resource name string references */
        resource_name->release_ref();
        verb->release_ref();
        thread->release_ref();
        headers->release_ref();
        delete hdr_list;
        if (body != 0)
            body->release_ref();
    }

    /* mark the request as completed */
    void complete();

    /* 
     *   Prepare the event object.  This creates an HTTPRequest object (the
     *   intrinsic class representing an incoming HTTP request), and sets up
     *   a NetRequestEvent (the byte-code object representing a request) to
     *   wrap it.  
     */
    virtual vm_obj_id_t prep_event_obj(VMG_ int *argc, int *evt_type);

    /* server thread */
    TadsHttpServerThread *thread;

    /* the HTTP verb */
    StringRef *verb;

    /* the request string */
    StringRef *resource_name;

    /* buffer containing the headers, parsed into null-delimited strings */
    StringRef *headers;

    /* list of starts of the headers in the buffer */
    TadsHttpRequestHeader *hdr_list;

    /* message body, if any */
    StringRef *body;

    /* 
     *   Flag: the message body exceeded the upload size limit.  If this is
     *   set, body is null, since we discard content that's too large. 
     */
    int overflow;
};

/* ------------------------------------------------------------------------ */
/*
 *   UI Close Event.  In the stand-alone local interpreter configuration,
 *   where the browser UI is an integrated part of the interpreter, this
 *   event is sent when the user explicitly closes the UI window.  In most
 *   cases, the game will simply want to terminate when this happens, because
 *   it indicates that the user has effectively dismissed the application.  
 */
class TadsUICloseEvent: public TadsEventMessage
{
public:
    TadsUICloseEvent(OS_Event *quit_evt) : TadsEventMessage(quit_evt) { }
    virtual vm_obj_id_t prep_event_obj(VMG_ int *argc, int *evt_type);
};

#endif /* TADSNET */
#endif /* VMNET_H */

