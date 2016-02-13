/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  osifcnet.h - TADS operating system interfaces for networking and threading
Function
  Virtualizes OS APIs for networking and threading.

  The style of the interface defined here is different from the basic
  osifc.h function interfaces.  Instead, the interfaces are defined via C++
  classes.  This is a more convenient way to represent the facilities in this
  package, because they all involve resources (network sockets, thread
  handles, event handles, etc) that have associated sets of operations -
  which maps very well onto objects and methods.

  We could have used C++'s abstract class mechanism to define a set of
  public interfaces here that are implemented by system .cpp files.  However,
  that approach isn't very efficient, since it involves run-time virtual
  method dispatch that truly isn't necessary in this situation (since a given
  build will be for a single OS, so only that one OS's concrete classes can
  ever be invoked in that build).  Furthermore, as we learned with HTML TADS,
  that's a somewhat confusing and high-overhead way to set things up for
  porting.

  So, instead, we define dummy skeletons for the classes, showing the
  public interfaces that must be implemented.  These aren't the real
  definitions - we #ifdef them out so that they're not actually compiled.
  The real definitions must be provided by each OS implementation instead.
  Simply copy the skeleton from this file into your OS file, define the
  methods listed, and add any additional methods and/or variables that you
  need for your OS version.  You can also add OS-specific base classes as
  needed.  This approach gives you full flexibility in defining the concrete
  classes, while giving the portable code a predictable set of interfaces.
Notes
  The network API defined here is for the TADS-in-a-browser setup.  This
  includes both the client/server configuration, where the user interface is
  presented through a web browser on a client machine, but the TADS VM runs
  on a remote server, and the two talk across an internet connection via
  HTTP; and the stand-alone browser configuration, where both the browser and
  the TADS web server are running on the same machine.  The latter is really
  a special case of the former: it's actually just a software bundle that
  launches the web server and the browser at once, and transparentl to the
  user, so that the game looks like a simple stand-alone interpreter app to
  the user.

  For traditional interpreter builds, the network API isn't used, so no
  implementation is required.
Modified
  04/04/10 MJRoberts  - Creation
*/

#ifndef OSIFCNET_H
#define OSIFCNET_H

#include "vmglob.h"


/*
 *   Intialize the networking and threading package.  This must be called
 *   once at program startup.  The OS isn't required to do anything here;
 *   it's just a chance for the OS layer to do any required static/global
 *   initialization.  If the OS implementation uses the global thread list
 *   (see TadsThreadList in vmnet.h), it should create the global list object
 *   here.  
 */
void os_net_init(class TadsNetConfig *config);

/*
 *   Clean up the networking and threading package.  This must be called once
 *   just before program termination.  The OS isn't required to do anything
 *   here; it's just a chance for the OS layer to clean up resources before
 *   quitting.
 *   
 *   If the OS implementation uses the global thread list (see
 *   CTadsThreadList in vmnet.h), it can use this opportunity to do a
 *   wait_all() on the thread list to allow background threads to terminate
 *   gracefully, then it should release its reference on the thread list
 *   object.  
 */
void os_net_cleanup();

/*
 *   Get the local host name, if possible.  This should return the EXTERNAL
 *   network name of the local computer (i.e., it shouldn't return
 *   "localhost", but rather the name that other machines on the network can
 *   use to connect to this machine).  Copies the host name to the buffer;
 *   returns true if successful, false if not.  
 */
int os_get_hostname(char *buf, size_t buflen);

/*
 *   Get the IP address for the given local host name, if possible.  On
 *   success, fills in 'buf' with the IP address in the standard decimal
 *   format ("1.2.3.4") and returns true.  On failure, returns false.
 *   
 *   'host' is a host name for an adapter on the local machine.  This
 *   function resolves the external IP address for the host name, which is
 *   the address that other machines on the network use to connect to this
 *   host on the given adapter.  Note that this should only be the external
 *   address as far as this machine is concerned.  For example, if the
 *   machine is connected to a LAN, and the LAN contains a NAT router that's
 *   connected to the public Internet, this routine does NOT go so far as to
 *   resolve the external address outside the NAT router - it merely returns
 *   the machine's local address on the LAN.
 *   
 *   If 'host' is null, this should return the default adapter's IP address,
 *   if there is such a thing.  On a machine with a single adapter, the
 *   default is that single adapter; on a machine with multiple adapters, if
 *   there's a local convention for designating which one is the default, use
 *   the local convention, otherwise the implementation can choose the
 *   default according to its own criteria, or simply pick one arbitrarily.  
 */
int os_get_local_ip(char *buf, size_t buflen, const char *host);


/*
 *   Special timeout value for an infinite wait.  When this is passed as a
 *   timeout value to the wait() functions, the wait continues indefinitely,
 *   until the desired event occurs.  
 */
/* #define OS_FOREVER  (define per OS) */

/*
 *   Event wait results.
 */
#define OSWAIT_TIMEOUT   -1       /* timeout expired before the event dired */
#define OSWAIT_ERROR     -2              /* wait failed with a system error */
#define OSWAIT_EVENT      0            /* got the event we were waiting for */
              /* for a multi-wait, OSWAIT_EVENT+n means the nth event fired */
#define OSWAIT_USER     1000    /* user-defined code can use this and above */

/*
 *   Socket error code.  This is the value returned by OS_Socket::send() and
 *   recv() when an error occurs, in lieu of a valid length value.  Define
 *   per operating system.  
 */
/* #define OS_SOCKET_ERROR (-1) */

/*
 *   Error values for OS_Socket::last_error().  Define these per OS.
 *   
 *   OS_EWOULDBLOCK - the error code returned from accept(), send(), and
 *   recv() when the socket is in non-blocking mode AND the call can't do any
 *   work without blocking.  This tells the caller that they should continue
 *   with asynchronous processing for a while and poll again later.
 *   
 *   OS_ECONNRESET - returned from send(), recv(), and others when the
 *   network peer (on the other side of the socket) has closed its end of the
 *   connection.  This means that further communications through the socket
 *   are not possible, since the other side isn't paying attention any more.
 *   
 *   OS_ECONNABORTED - returned from send(), recv(), and others when the
 *   local host has terminated the connection (i.e., some lower-level
 *   software in the network stack - not the application - has decided on its
 *   own to close the socket).  This can happen in various situations, such
 *   as when the peer fails to acknowledge a transmission from our side.
 *   From the application's perspective, this is usually equivalent to
 *   OS_ECONNRESET, since in either case we've lost the ability to
 *   communicate with the client due to some kind of interruption outside of
 *   the local machine's control.
 */
/* #define OS_EWOULDBLOCK   EWOULDBLOCK */
/* #define OS_ECONNRESET    ECONNRESET */
/* #define OS_ECONNABORTED  ECONNABORTED */

/*
 *   Protected counter.  This object maintains an integer count that can be
 *   incremented and decremented.  The inc/dec operations are explicitly
 *   atomic, meaning they're thread-safe: it's not necessary to use any
 *   additional access serialization mechanism to perform an increment or
 *   decrement.  
 */
#if 0     /* skeleton definition - make a copy for the actual OS definition */
class OS_Counter
{
public:
    /* initialize with a starting count value */
    OS_Counter(long c = 1);

    /* get the current counter value */
    long get() const;

    /* 
     *   Increment/decrement.  These return the post-update result.
     *   
     *   The entire operation must be atomic.  In machine language terms, the
     *   fetch and set must be locked against concurrent access by other
     *   threads or other CPUs.  
     */
    long inc();
    long dec();
};
#endif

/* ------------------------------------------------------------------------ */
/*
 *   IMPORTANT: in your OS-specific header, #include "vmrefcnt.h" here,
 *   immediately after defining class OS_Counter.
 *   
 *   vmrefcnt.h defines generic classes that are used as base classes by OS
 *   interface classes below.  
 */
/* ------------------------------------------------------------------------ */

/*
 *   Waitable object.  This is the base class for objects that can be used in
 *   a multi-object wait.  A waitable object has two states: ready and not
 *   ready.  Waiting for the object blocks the waiting thread as long as the
 *   object is in the not-ready state, and releases the thread (allows it to
 *   continue running) as soon as the object enters the rady state.  Waiting
 *   for an object that's already in the ready state simply allows the
 *   calling thread to continue immediately without blocking.  
 */
#if 0     /* skeleton definition - make a copy for the actual OS definition */
class OS_Waitable
{
public:
    OS_Waitable();
    virtual ~OS_Waitable();

    /* 
     *   Wait for the object, with a maximum wait of 'timeout' milliseconds.
     *   Blocks the calling thread until the object is in its ready state, OR
     *   the timeout interval elapses, whichever comes first.  At that point
     *   the thread is released and this function returns.  If the object is
     *   already in the ready state when this is called, the method returns
     *   immediately.
     *   
     *   If 'timeout' is OS_FOREVER, the wait is indefinite - it continues
     *   until the event occurs, no matter how long that takes.
     *   
     *   Returns an OSWAIT_xxx code to indicate what happened.  If the object
     *   is already in the ready state, returns OSWAIT_EVENT immediately.  A
     *   timeout value of zero returns OSWAIT_TIMEOUT immediately (without
     *   blocking) if the object isn't ready.  
     */
    int wait(unsigned long timeout = OS_FOREVER);

    /*
     *   Test to see if the object is ready, without blocking.  Returns true
     *   if the object is ready, false if not.  If the object is ready,
     *   waiting for the object would immediately release the thread.
     *   
     *   Note that testing some types of objects "consumes" the ready state
     *   and resets the object to not-ready.  This is true of auto-reset
     *   event objects.  
     */
    int test();

    /*
     *   Wait for multiple objects.  This blocks until at least one object in
     *   the list is in the ready state, at which point it returns
     *   OSWAIT_OBJECT+N, where N is the array index in 'objs' of an object
     *   that's ready.  If the timeout (given in milliseconds) expires before
     *   any objects are ready, we return OSWAIT_TIMEOUT.  If the timeout is
     *   omitted, we don't return until an object is ready.
     *   
     *   If multiple objects become ready at the same time, this picks one
     *   and returns its index.
     *   
     *   If any of the objects are auto-reset event objects, the signaled
     *   state of the *returned* object is affected the same way as it would
     *   be for an ordinary wait().  The signaled states of any other objects
     *   involved in the multi_wait are not affected.  For example, if you
     *   multi_wait for three auto-reset events, and all three start off in
     *   the signaled state, multi_wait will arbitrarily choose one as the
     *   returned event, and only that one chosen event will be reset by the
     *   multi_wait.  
     */
    static int multi_wait(int cnt, OS_Waitable **objs,
                          unsigned long timeout = OS_FOREVER);
};
#endif

/*
 *   Event.  This is a waitable object with two states: signaled and
 *   unsignaled.  In the unsignaled state, a thread waiting for the event
 *   blocks until the event becomes signaled, at which point the thread is
 *   released.  In the signaled state, a thread waiting for the event simply
 *   continues running without blocking.
 *   
 *   There are two types of event objects: auto-reset and manual-reset.
 *   
 *   An auto-reset event automatically reverts to the unsignaled state as
 *   soon as ONE thread waiting for the event is released.  This means that
 *   if multiple threads are waiting for the event, signaling the event will
 *   only release a single thread; the others will continue blocking until
 *   the event is signaled again, at which point one more thread will be
 *   released.  An auto-reset event balances signals and waits: one signal
 *   releases one waiting thread.  If the event is signaled twice while no
 *   threads are waiting, the next two waits will immediately be released,
 *   and the third wait will block.
 *   
 *   A manual-reset event simply stays signaled once signaled, no matter how
 *   many threads are waiting for it.  The event reverts to unsignaled only
 *   when someone manually calls reset().  
 */
#if 0     /* skeleton definition - make a copy for the actual OS definition */
class OS_Event: public OS_Waitable, public CVmRefCntObj
{
public:
    /* construct */
    OS_Event(int manual_reset);

    /*
     *   Signal the event.  In the case of a manual-reset event, this
     *   releases all threads waiting for the event, and leaves the event in
     *   a signaled state until it's explicitly reset.  For an auto-reset
     *   event, this releases only one thread waiting for the event and then
     *   automatically resets the event.  
     */
    void signal();

    /*
     *   Reset the event.  For a manual-reset event, this returns the event
     *   to the unsignaled state.  This has no effect for an auto-reset
     *   event.  
     */
    void reset();
};
#endif

/*
 *   Mutex - mutual exclusion object.  This is used to protect shared
 *   resources from concurrent access by multiple threads.
 *   
 *   You create a mutex to protect a designated set of shared resources; a
 *   typical example is that you create a mutex for each instance of a class,
 *   to protect the member variables of each instance.  When a thread is
 *   about to start a series of operation on the shared variables, and that
 *   series of operations is required to be atomic, the thread first locks
 *   the mutex associated with the variables (e.g., if you're about to make
 *   some changes to an object's member variables, you lock the object's
 *   mutex first).  Only one thread can hold a mutex lock at a time, so once
 *   you have the mutex lock, you know that any other thread attempting the
 *   same operation would be blocked on its own lock attempt as long as
 *   you're holding the lock, ensuring that no other thread will attempt to
 *   modify the same variables.  When you're done with the atomic operation,
 *   you release the mutex to allow other threads to access the variables
 *   again.
 *   
 *   The OS implementation for mutexes should be as lightweight as possible.
 *   It's more efficient in terms of thread concurrency if callers can create
 *   very granular mutexes, to protect specific resources.  This means that
 *   callers will want to create many mutexes in a typical system, so the
 *   cost of creating lots of mutexes shouldn't outweigh the efficiency gain
 *   of using lots of them.
 *   
 *   Care should be taken to avoid deadlocks when locking more than one mutex
 *   at a time.  The simplest technique for avoiding deadlocks is to use a
 *   fixed order for acquiring any given set of mutexes.  For example, if you
 *   have two mutexes A and B, make sure all threads that acquire both will
 *   always acquire A first, then B.  This ensures that a thread that
 *   successfully acquires A and then wants to acquire B will not deadlock
 *   with another thread that already locked B and wants to acquire A.  Since
 *   the other thread will also always try to lock A before B, the first
 *   thread can be assured that once it has A, no one else who wants both A
 *   and B can already have B, since anyone else wanting both would
 *   necessarily already have A, which the first thread knows can't be the
 *   case because it acquired A.  
 */
#if 0     /* skeleton definition - make a copy for the actual OS definition */
class OS_Mutex: public CVmRefCntObj
{
public:
    OS_Mutex();

    ~OS_Mutex();

    /* 
     *   Lock the mutex.  Blocks until the mutex is available. 
     */
    void lock();

    /* 
     *   Test the mutex: if the mutex is available, returns true; if not,
     *   returns false.  Doesn't block in either case.  If the return value
     *   is true, the mutex is acquired as though lock() had been called, so
     *   the caller must use release() when done.  
     */
    int test();

    /* 
     *   Unlock the mutex.  This can only be used after a successful lock()
     *   or test().  This releases our lock and makes the mutex available to
     *   other threads.  
     */
    void unlock();
};
#endif

/*
 *   "Core" socket.  This is the common base class of data sockets and
 *   listener sockets.
 *   
 *   In the standard Unix socket API, there's really just one kind of socket
 *   object, and its usage as a data or listener socket depends on the way
 *   it's set up.  In OO terms, though, the data and listener sockets are
 *   really separate subclasses, with a small part of their interfaces in
 *   common.  We've extracted the common parts into this base class.
 *   
 *   Our socket classes in aggregate are just a thin layer implementing the
 *   same model as the Unix socket API.  Most modern operating systems have
 *   socket libraries modeled closely on the Unix socket API, so this should
 *   be easily implementable on most systems.  We can't use the Unix socket
 *   API directly, because other systems do tend to have idiosyncracies,
 *   despite their overall similarity to the Unix API.  Windows is a
 *   particularly good (?) example: a lot of the basic structure of the API
 *   is the same as in Unix, but with a bunch of gratuitous name changes; and
 *   some of the more advanced features, such as non-blocking mode, are
 *   handled rather differently because of deeper differences in the OS.  
 */
#if 0     /* skeleton definition - make a copy for the actual OS definition */
class OS_CoreSocket: public CVmRefCntObj, public OS_Waitable
{
public:
    OS_CoreSocket();

    /* deleting the object closes the underlying system socket */
    ~OS_CoreSocket() { close(); }

    /* 
     *   Close the socket.  This releases any system resources associated
     *   with the socket, including any network ports it's using. 
     */
    void close();
    
    /* 
     *   Set the socket to non-blocking mode.
     *   
     *   In non-blocking mode, any network operation on the socket will
     *   return immediately with OS_SOCKET_ERROR if the operation can't be
     *   completed immediately, and thus would block pending the arrival of
     *   data or completion of a transmission.  Specifically:
     *   
     *   - reading a data socket (recv(), in the Unix API) would block if
     *   there are zero bytes available to be read in the inbound buffer
     *   
     *   - writing a data socket (send() in Unix) would block if the outbound
     *   buffer is too full to accommodate all of the bytes being sent
     *   
     *   - listening for a new incoming connection request (accept() in Unix)
     *   would block if there are no pending connection requests
     *   
     *   If one of these functions returns OS_SOCKET_ERROR, the caller should
     *   check the specific error code by calling last_error().  This will
     *   return OS_EWOULDBLOCK when the cause of the error is one of the
     *   above conditions that would normally cause the call to block.
     *   
     *   After an OS_EWOULDBLOCK occurs, the caller can do one of two things.
     *   First, it can simply carry on with other operations; since the
     *   thread isn't blocked waiting for the network operation to complete,
     *   it can do whatever else it wants in the meantime.  The thread can
     *   "poll" the socket by attempting the operation again from time to
     *   time to see if it's finally ready.  Second, if the thread doesn't
     *   have any other business to transact, it can wait for the socket to
     *   become ready for the desired operation using the socket's
     *   OS_Waitable interface.  That blocks the thread the same way that the
     *   same network operation functions would in the default blocking mode,
     *   but provides two extra capabilities that the regular network
     *   functions don't: you can set a timeout so that the thread will only
     *   block for a certain maximum interval, and you can use the multi-wait
     *   interface so that other events of interest will also wake up the
     *   thread if they should occur before the socket is ready.
     *   
     *   Note that the default mode when you create a new socket is blocking
     *   mode.  In blocking mode, any of the above blocking conditions do not
     *   cause an error: instead, they simply block the thread until the
     *   network operation completes.  
     */
    void set_non_blocking();

    /*
     *   In non-blocking mode, the caller must manually reset the socket's
     *   event waiting status after each successful wait.  
     */
    void reset_event();

    /* 
     *   Get the last error for a network operation (for a data socket, send
     *   and receive; for a listener socket, accept).  Returns an OS_Exxx
     *   value.  
     */
    int last_error() const;

    /* 
     *   Get my port number.  For a listener socket, this is the port on
     *   which we're listening; for a regular data socket, this is the port
     *   number on our side (the local side) of the connection.  Returns 0 if
     *   the port hasn't been opened yet. 
     */
    unsigned short get_port_num();

    /* 
     *   Get this socket's local IP address.  This returns the IP address of
     *   the network adapter on our side (the local side) of the connection.
     *   For a listening socket, this is the IP address that clients use to
     *   initiate a connection to this server.
     *   
     *   The returned string is allocated.  The caller must free the string
     *   when done with it, using lib_free_str().  
     */
    char *get_ip_addr() const { return lib_copy_str(local_ip); }

    /*
     *   Get the IP address for the given host.  The return value is an
     *   allocated buffer that the caller must delete with 'delete[]'.  If
     *   the host IP address is unavailable, returns null.
     *   
     *   A null or empty hostname gets the default external IP address for
     *   the local machine.  This does NOT return "localhost" or "127.0.0.1",
     *   but rather the external IP that other machines on the same local
     *   area network can use to connect to this machine.  Note that this
     *   address is valid on our LAN, but not necessarily on the broader
     *   Internet, since we could be behind a NAT firewall that assigns local
     *   IP addresses within the LAN.  
     */
    static char *get_host_ip(const char *hostname);
};
#endif

/*
 *   Data socket.  This is a subclass of the core socket that's used for
 *   basic data transmission.  A data socket is a full-duplex communication
 *   channel between this process and another process, which might be running
 *   on the same host or on a remote host connected via a network.
 *   
 *   On the server side, sockets are created by OS_Listener::accept().
 *   
 *   Currently, there is no interface for creating a client-side socket.
 *   Instead, we provide a request-level API for initiating HTTP requests
 *   from the client side (see class OS_HttpRequest).
 */
#if 0     /* skeleton definition - make a copy for the actual OS definition */
class OS_Socket: public OS_CoreSocket
{
public:
    /*
     *   Construction.  Note that callers do not generally create socket
     *   objects directly; instead, these objects are created by other
     *   osifcnet system classes and returned to user code to access.  For
     *   example, server sockets are created by OS_Listener::accept().
     */
    OS_Socket();

    /* 
     *   Send bytes.  Returns the number of bytes sent, or OS_SOCKET_ERROR if
     *   an error occurs.
     *   
     *   When the socket is in non-blocking mode, and the buffer is too full
     *   to accept the complete write, this writes nothing and returns
     *   immediately with an error; last_error() will return OS_EWOULDBLOCK.
     */
    int send(const char *buf, size_t len);

    /* 
     *   Receive bytes.  Returns the number of bytes received.  If the return
     *   value is 0, it means that the peer has closed its end of the socket
     *   and all buffered data have already been received.  If the return
     *   value is negative, it indicates an error; the specific error code
     *   can be obtained from last_error().
     *   
     *   This doesn't necessarily fulfill the complete read request.  If
     *   there are fewer bytes immediately available than 'len', this reads
     *   all of the bytes immediately available and returns, indicating the
     *   number of bytes actually transfered.  This is the case regardless of
     *   the blocking mode of the socket.
     *   
     *   When the socket is in non-blocking mode, and there are no bytes
     *   available, this returns immediately with an error, and last_error()
     *   will return OS_EWOULDBLOCK.  
     */
    int recv(char *buf, size_t len);

    /*
     *   Get the IP address and port for our side of the connection.  'ip'
     *   receives an allocated string with the IP address string in decimal
     *   notation ("192.168.115").  We fill in 'port' with our port number.
     *   Returns true on success, false on failure.  If we return success,
     *   the caller is responsible for freeing the 'ip' string with
     *   lib_free_str().  
     */
    int get_local_addr(char *&ip, int &port);

    /*
     *   Get the IP address and port for the network peer to which we're
     *   connected.  'ip' receives an allocated string with the IP address
     *   string in decimal notation ("192.168.1.15").  We fill in 'port' with
     *   the port number on the remote host.  Returns true on success, false
     *   on failure.  If we return success, the caller is responsible for
     *   freeing the allocated 'ip' string via lib_free_str().  
     */
    int get_peer_addr(char *&ip, int &port);
};
#endif

/*
 *   Listener.  This class represents a network listener socket, which is
 *   used to wait for and accept new incoming connections from clients.
 */
#if 0     /* skeleton definition - make a copy for the actual OS definition */
class OS_Listener: public OS_CoreSocket
{
public:
    /* 
     *   Construction.  This sets up the object, but doesn't open a listener
     *   port.  Call open() to actually open the port.  
     */
    OS_Listener();

    /* 
     *   Open the listener on the given port number.  This can be used to
     *   create a server on a well-known port, but it has the drawback that
     *   ports can't be shared, so this will fail if another process is
     *   already using the same port number.
     *   
     *   Returns true on success, false on failure.  
     */
    int open(const char *hostname, unsigned short port_num);

    /*
     *   Open the listener port, with the port number assigned by the system.
     *   The system will select an available port and assign it to this
     *   listener, so this avoids contention for specific port numbers.
     *   However, it requires some separate means of communicating the port
     *   number to clients, since there's no way for them to know the port
     *   number in advance.
     *   
     *   Returns true on success, false on failure.  
     */
    int open(const char *hostname);

    /*
     *   Accept the next incoming connection.  If the listener is in blocking
     *   mode (the default), and there are no pending requests, this blocks
     *   until the next pending request arrives.  In non-blocking mode, this
     *   returns immediately with a null socket if no requests are pending,
     *   and last_error() indicates OS_EWOULDBLOCK.  
     */
    OS_Socket *accept();
};
#endif


/*
 *   Thread.
 *   
 *   To define the code to execute for a thread, you subclass this class and
 *   define a concrete implementation for thread_main().  The thread
 *   automatically terminates when thread_main() returns.
 *   
 *   When used as a Waitable object, a thread is in in the "ready" state when
 *   the thread's thread_main() has returned.  In other words, waiting for a
 *   thread means that you're waiting for the thread to complete.  
 */
#if 0     /* skeleton definition - make a copy for the actual OS definition */
class OS_Thread: public CVmWeakRefable, public OS_Waitable
{
    /* the TadsThreadList class must be a friend, if we use it */
    friend class TadsThreadList;
    
public:
    /* 
     *   Construction.  This doesn't actually launch the thread; that can't
     *   happen in the base class constructor because the thread entrypoint
     *   might be reached before the construction is completed.  Instead,
     *   call launch() to start the system-level thread running.
     *   
     *   If the OS uses the TadsThreadList master list of threads, each new
     *   thread should be registered here through the ThreadList global.
     *   This is optional; it's for the OS layer's benefit, so if the OS
     *   layer doesn't need it, you can omit it.  
     */
    OS_Thread() { TadsThreadList::add(this); }

    /* 
     *   Destruction.
     *   
     *   If the OS uses the ThreadList master list of threads, you should
     *   clean up the master thread list here.  The thread list keeps a weak
     *   reference to the thread, and the weak reference is automatically
     *   cleared when we're deleted, so the thread list simply needs to scan
     *   for dead weak refs and clean them up.  
     */
    ~OS_Thread() { TadsThreadList::clean(); }

    /*
     *   Launch the system thread, and set it up to start running in
     *   thread_main().  Returns true on success, false if the thread
     *   couldn't be started.
     *   
     *   When this returns, the thread may or may not have actually started
     *   executing thread_main().  On most operating systems it's up to the
     *   OS scheduler to decide when to give CPU time to the thread, so the
     *   thread's execution status depends on the OS and on current CPU
     *   conditions in the system.  If the caller needs to know that the
     *   thread has started executing, or that the thread has reached some
     *   particular point in its execution, it can easily coordinate this by
     *   creating an event object that it shares with the thread: the thread
     *   signals the event when the desired milestone has been reached, and
     *   the caller waits for the event after creating the thread, ensuring
     *   that the caller won't proceed until the thread has complete the
     *   desired startup steps.  
     */
    int launch();

    /*
     *   Thread entrypoint routine.  Callers must subclass this class and
     *   provide a concrete definition for this method.  This method is
     *   called at thread entry. 
     */
    virtual void thread_main() = 0;
};
#endif

/*
 *   HTTP client operations
 */
class OS_HttpClient
{
public:
    /*
     *   Perform an HTTP request on a given resource, returning the resource
     *   contents.
     *   
     *   'opts' is a combination of OptXxx option flags (see below).
     *   
     *   'verb' is the HTTP verb to perform; this is usually GET, HEAD, POST,
     *   or PUT.  'resource' is the full URL to the resource to be retrieved.
     *   'portno' is the port number (usually 80).  'ua' is an optional User
     *   Agent string; if this is null, we'll use a default UA string
     *   identifying ourselves as the generic TADS HTTP client.
     *   
     *   'send_headers' is an optional string of custom headers to send with
     *   the request.  The headers are appended to the headers that we
     *   automatically generate.  The string must be in the standard "Name:
     *   Value" format, with headers separated by CR-LF sequences.  The
     *   overall string can end wtih one or more CR-LF sequences but isn't
     *   required to.  Pass null if no custom headers are to be sent.
     *   'send_headers_len' is the length in bytes of this string.
     *   
     *   If 'payload' is non-null, it contains the payload data to send with
     *   the request.  This is for use with POST and PUT to specify the data
     *   to send with the request.  For a POST, the payload can be a mix of
     *   form fields and file items.  If there are any file items, we'll send
     *   the POST data in the multipart/form-data format; otherwise we'll
     *   send as application/x-www-form-urlencoded.  For any verb other than
     *   POST, the payload (if present at all) is limited to a single file
     *   item.
     *   
     *   Special exception: for POST, if the payload consists of a single
     *   payload item with an empty string ("") as the name, we'll assume
     *   that the caller did all of the necessary POST encoding already, and
     *   we'll send that payload item as the entire content body of the
     *   request WITHOUT any further encoding.  This allows the caller to
     *   hand-code an x-www-form-urlencoded or multipart/form-data body, or
     *   to use an entirely different encoding for the body.  In other words,
     *   we treat a POST payload with a single unnamed payload item exactly
     *   as we treat a payload for PUT or any other non-POST verb.
     *   
     *   Returns the HTTP status code from the server, or an ErrXxx code
     *   defined below if an error occurred on the client side.  The ErrXxx
     *   codes are all negative, which distinguishes them from HTTP status
     *   codes.
     *   
     *   If the server sends back a reply body, we'll store the contents of
     *   the reply in the 'reply' object.  If provided, this must be a
     *   writable stream object.  'reply' can alternatively be null, in which
     *   case we'll simply discard any reply body.  If the request doesn't
     *   return any reply body, but 'reply' is non-null, we'll simply leave
     *   'reply' unchanged.  Note that we don't rewind or truncate the
     *   'reply' stream object, so the caller has to prepare the object in
     *   the desired initial conditions before calling this method.
     *   
     *   'headers' is optional.  If this is non-null, we'll allocate a buffer
     *   for the headers and store the pointer to the buffer in '*headers'.
     *   This buffer will contain the raw CR-LF delimited header lines from
     *   the reply, with a null terminator.  The caller must delete this
     *   memory with delete[].  The header buffer will be returned only if
     *   the request makes it far enough to retrieve headers; if the call
     *   fails at the socket connection level, for example, no headers are
     *   returned.
     *   
     *   'location' is optional.  If this is non-null, AND the request result
     *   is a 301 code ("resource moved"), we'll allocate a copy of the
     *   redirected URL from the HTTP reply and hand it back via *location.
     *   The caller is responsible for freeing this buffer when done with it
     *   via lib_free_str().
     *   
     *   If 'location' is null, and the resource has moved, we'll
     *   automatically follow the redirection link and retrieve the resource
     *   at the redirected location.  Any redirection is thus transparent to
     *   the caller when 'location' is null.  
     */
    static int request(int opts, const char *host, unsigned short portno,
                       const char *verb, const char *resource,
                       const char *send_headers, size_t send_headers_len,
                       class OS_HttpPayload *payload,
                       class CVmDataSource *reply, char **reply_headers,
                       char **location, const char *ua);

    /* option flags for request() */
    static const int OptHTTPS = 0x0001;                 /* use https scheme */

    /* non-HTTP status codes for request() */
    static const int ErrNoMem = -1;                        /* out of memory */
    static const int ErrNoConn = -2;            /* couldn't connect to host */
    static const int ErrNet = -3;             /* other network/socket error */
    static const int ErrParams = -4;                  /* invalid parameters */
    static const int ErrReadFile = -5;  /* can't read payload file contents */
    static const int ErrWriteFile = -6;       /* error writing reply stream */
    static const int ErrGetHdrs = -7;     /* error retrieving reply headers */
    static const int ErrThread = -8;               /* error starting thread */
    static const int ErrOther = -100;                        /* other error */
};

/* HTTP request payload item */
class OS_HttpPayloadItem
{
public:
    /* create as a name/value pair for a form field for a POST */
    OS_HttpPayloadItem(const char *name, const char *val);
    OS_HttpPayloadItem(const char *name, size_t name_len,
                       const char *val, size_t val_len);

    /* 
     *   Create as a file upload for PUT or POST.  'filename' is the nominal
     *   name for the upload - it doesn't have to correspond to an actual
     *   file system object.  The actual payload contents will be taken from
     *   the stream, NOT from a disk file.
     *   
     *   Since 'filename' is only used on the server side to identify the
     *   upload, it's normally a root name only, stripped of any path prefix.
     *   A local path on the client won't mean anything to the server, so
     *   there's no value in sending it, and some people consider doing so a
     *   security risk in that it unnecessarily exposes information about the
     *   local system's directory structure across the network.  
     */
    OS_HttpPayloadItem(const char *name,
                       const char *filename, const char *mime_type,
                       class CVmDataSource *contents);
    OS_HttpPayloadItem(const char *name, size_t name_len,
                       const char *filename, size_t filename_len,
                       const char *mime_type, size_t mime_type_len,
                       class CVmDataSource *contents);

    /* delete */
    ~OS_HttpPayloadItem();

    void init();

    /* field name, for a POST form field or POST form file upload */
    char *name;

    /* 
     *   Field value, for a simple POST form field; for a file, this is the
     *   nominal filename to send to the server.  
     */
    char *val;

    /* data stream for a PUT file or a POST form file upload */
    class CVmDataSource *stream;

    /* MIME type, for a PUT file or a POST form file upload */
    char *mime_type;

    /* next item in the list */
    OS_HttpPayloadItem *nxt;
};

/*
 *   HTTP request payload data.  Use this to package the uploaded data for an
 *   HTTP PUT or POST via OS_HttpClient::request().  
 */
class OS_HttpPayload
{
public:
    OS_HttpPayload();
    ~OS_HttpPayload();

    /* add a simple name/value form field */
    void add(const char *name, const char *val);

    /* add a simple name/value field, in tads-string (VMB_LEN prefix) format */
    void add_tstr(const char *name, const char *val);

    /* add a simple file upload */
    void add(const char *name, const char *filename, const char *mime_type,
             class CVmDataSource *contents);
    void add(const char *name, size_t name_len,
             const char *filename, size_t filename_len,
             const char *mime_type, size_t mime_type_len,
             class CVmDataSource *contents);

    /* add an item */
    void add(OS_HttpPayloadItem *item);

    /* get the number of payload items */
    int count_items() const;

    /* 
     *   Is this a multipart payload?  This returns true if any of the fields
     *   contain file uploads, false if they're all simple form fields.  A
     *   POST consisting entirely of simple form fields is normally sent with
     *   MIME type application/x-www-form-urlencoded, whereas a POST
     *   containing uploaded file data requires multipart/formdata. 
     */
    int is_multipart() const;

    /* 
     *   Create an application/x-www-form-urlencoded buffer from the payload.
     *   This is only applicable when there are no file upload fields.  The
     *   returned string is an allocated buffer that the caller is
     *   responsible for freeing when done with it, via t3free().  
     */
    char *urlencode(size_t &len) const;

    /* get the Nth item */
    OS_HttpPayloadItem *get(int n) const;

protected:
    /* list of payload items */
    OS_HttpPayloadItem *first_item, *last_item;
};


/* ------------------------------------------------------------------------ */
/*
 *   Forward declarations
 */
class TadsThreadList;


/* ------------------------------------------------------------------------ */
/*
 *   Include the appropriate system-specific header 
 */

#if defined(GARGOYLE)
#include "osnetdum.h"

#elif defined(_WIN32)
#include "osnetwin.h"

#elif defined(UNIX) || defined(FROBTADS)
#include "osnetunix.h"

#else
#include "osnetdum.h"

#endif


/* ------------------------------------------------------------------------ */
/*
 *   Connect to the Web UI.  The game calls this after it starts its HTTP
 *   server through which it will present its Web UI.  This function's job is
 *   to get the client connected to the new HTTP server - in other words, to
 *   navigate the client's browser to the game's HTTP server and load the
 *   game's start page.
 *   
 *   'message_queue' is an optional message queue to receive notifications
 *   about events in the UI after it's connected.  If this is null, no
 *   notifications are sent.  If provided, the UI will send the following
 *   event messages to the queue as appropriate:
 *   
 *   - For a stand-alone local configuration (where the browser UI is an
 *   integrated part of the interpreter application - see below), a
 *   TadsUICloseEvent is posted when the user manually closes the UI window.
 *   For the local configuration, the game will usually want to consider this
 *   to be an "end of file" on the input, causing the game to terminate.
 *   
 *   'addr' is the network address of the server (as a decimal IP string,
 *   e.g. "192.168.1.25", or as a DNS host name), and 'port' is the port
 *   number on which the server is listening.  The address will be sent to
 *   the client, and the client will use it to connect to the game, so the
 *   address must be in a form that the client can use to reach our server.
 *   For a client within the same LAN, the local IP address will work.  This
 *   won't work in cases where we're behind a NAT firewall and the client is
 *   on the other side of the firewall, though: in those cases, you have to
 *   be sure to use the external Internet IP address.  In most cases this
 *   isn't something the game will have to worry about, since the game
 *   generally just takes its listener binding address from the Web server
 *   that launched it - so assuming the Web server is configured correctly,
 *   the game will usually have the right address.
 *   
 *   'path' is the path to the starting UI page; this is an absolute path on
 *   the server, such as "/webui.htm".  'errmsg' is a pointer to a pointer to
 *   be filled in with an allocated error message string if the operation
 *   fails.  'errmsg' must be set to a string allocated with lib_alloc_str()
 *   or lib_copy_str(); the caller is responsible for freeing this string
 *   with lib_free_str() when done with it.  The function must set '*errmsg'
 *   to null if not used.  Return true on success, false on failure.
 *   
 *   This function could conceivably be implemented any number of ways, but
 *   we foresee two main implementation strategies, for our two main expected
 *   configurations.
 *   
 *   CONFIGURATION 1: STAND-ALONE PLAY
 *   
 *   Stand-alone play is where the whole game, client and server, runs on a
 *   single PC and looks to the user like a single application.  The fact
 *   that there's a network server running is transparent to the user; they
 *   see what looks like the traditional command-line or HTML TADS setup,
 *   where there's just a "t3run <game>" command that opens a game window.
 *   
 *   In this configuration, connecting to the Web UI is easy.  Since both
 *   client and server are running under one roof, we have control over the
 *   client UI.  We simply need to open a Web browser window and navigate it
 *   to the game server's start page.
 *   
 *   Opening the browser window could consist of actually launching a
 *   separate Web browser application and passing in the starting URL.  This
 *   is the simplest approach.  For a more integrated appearance, the
 *   application could instead launch its own custom frame window with an
 *   embedded HTML browser widget filling the interior of the window (the
 *   "client area" in Windows parlance).  In either case, the job is the
 *   same: launch a browser and point it to the game's HTTP server.
 *   
 *   CONFIGURATION 2: CLIENT/SERVER WEB PLAY
 *   
 *   Starting a Web-based TADS game presents us with an interesting problem.
 *   The client initiates the game by connecting to a Web server where the
 *   game is hosted, and navigating to a "launch" page.  The launch page is
 *   actually a server-side program that starts the TADS 3 Interpreter in a
 *   child process, passing the .t3 file name to the interpreter.  The
 *   interpreter loads the game and starts executing it.  The game then sets
 *   up an HTTP server which it will use to present its Web UI.
 *   
 *   That's where our "interesting problem" comes up.  We have a client who
 *   knows how to talk to the "launch" page, and separately we have the
 *   game's HTTP server on an ad hoc port assigned by the operating system.
 *   (For details on why this is an ad hoc port rather than a well-known
 *   port, see the discussion of Network Service Registration below.)  The
 *   problem is that we now need to connect the client to the server.
 *   
 *   The way we solve this is to use that "launch" page as the conduit.  When
 *   the launch page loads, it doesn't immediately send anything back to the
 *   client.  Instead, it stays momentarily silent while it launches the TADS
 *   Interpreter and lets the game start up.  When the launch page starts the
 *   interpreter, it establishes an interprocess communication channel of
 *   some kind between itself and the interpreter - for example, a Unix pipe
 *   would work nicely.  When the game calls vmnet_connect_webui(), we use
 *   this IPC to send our IP address and port number information back to the
 *   launch page.  When the launch page receives this information, it finally
 *   replies to the client.  But rather than sending back just any old HTML,
 *   it sends back a REDIRECT reply, pointing the client to the game server
 *   start page.  When the client receives the redirect, it turns around and
 *   loads the start page from the game server.
 *   
 *   ---
 *   
 *   The implementation of this function is specific to both the
 *   CONFIGURATION and the SYSTEM.  For example, there might be two separate
 *   Unix implementations: one for stand-alone play that opens an integrated
 *   frame window containing an embedded browser widget; and the other for
 *   Web server use that writes the information to stdout for the "launch"
 *   page script to read.
 *   
 *   ---
 *   
 *   This routine should reuse the same UI display resources if repeated
 *   calls are made.  For example, in the stand-alone configuration, if we've
 *   already opened a local browser window in a previous call, and that
 *   window is still open, we should simply navigate that existing window to
 *   the new location rather than opening a second window.
 *   
 *   The caller should invoke osnet_disconnect_webui() before terminating the
 *   program.  However, calls to osnet_connect_webui() are NOT required to be
 *   matched by an equal number of calls to the disconnect function.  It's
 *   sufficient to call disconnect once after calling connect several times.
 */
int osnet_connect_webui(VMG_ const char *addr, int port, const char *path,
                        char **errmsg);

/*
 *   Disconnect from the Web UI.  This is used at program termination to
 *   notify the Web UI that the server program is no longer running, to allow
 *   it to update or remove its UI, and to release system resources.
 *   
 *   If there's no Web UI currently active, this routine should simply return
 *   without doing anything.  It must be harmless to call this routine with
 *   no previous calls to osnet_connect_webui().
 *   
 *   For a stand-alone configuration, this should disconnect from the Web
 *   UI's window.  If the Web UI is running as a separate process, this
 *   should clean up any IPC channel between the processes, and should update
 *   the UI to indicate that the game has ended.  For example, the UI window
 *   could show a message to this effect in its title bar or status line, or
 *   could add a message to the text display.
 *   
 *   For a client/server web-play configuration, this usually doesn't do
 *   anything on the UI side, since the UI is a separate browser (on a
 *   separate machine) that the user launched and which we're not responsible
 *   for closing.  However, this routine can still take the opportunity to
 *   free up any local resources allocated is osnet_connect_webui().
 *   
 *   If 'close' is true, the web UI should be visually removed.  If it's
 *   running in a separate process, the process can be terminated.  If
 *   'close' is false, a separate process can be left running for the user to
 *   close manually.  Explicitly closing the UI is appropriate for debuggers
 *   and other "container" environments where it's reasonable to expect that
 *   closing the container environment would close the UI window as well.
 *   Leaving the UI open is appropriate for a stand-alone intepreter in a GUI
 *   environment; even though the game engine has terminated, we usually want
 *   to leave the final state of the UI displayed until the user manually
 *   closes it.  This gives the user a chance to read any final messages the
 *   game displays before terminating.  
 */
void osnet_disconnect_webui(int close);

/*
 *   Run the file selector dialog in the *local standalone mode* Web UI.
 *   
 *   This is a substitute for the regular os_askfile() routine that the
 *   interpreter calls when we're running in the local standalone mode.  This
 *   mode is a hybrid of the Web UI and the local interpreter console UI: it
 *   uses a web browser as the UI, but it uses the local file system for
 *   storage.  The file selection dialog is at the intersection of these two
 *   axes: the file dialog is a UI element, and our primary UI is the
 *   browser, but in this configuration the dialog accesses the local file
 *   system and should present the same UI as it would for the a local
 *   application.  That's why we need a custom function to handle this.
 *   
 *   Generally, the implementation should invoke a standard local file
 *   selector dialog, but show it within the UI context of the Web UI window.
 *   In the Windows implementation, for example, we run the Web UI in a child
 *   process that's a customized browser window, so we handle this function
 *   by sending a message to the child UI process asking it to display a file
 *   dialog on our behalf.  The child process then sends us back a reply
 *   containing the result of the dialog.
 *   
 *   This can be stubbed out with an empty implementation on platforms that
 *   don't have a standalone Web UI mode.  
 */
int osnet_askfile(const char *prompt, char *fname_buf, int fname_buf_len,
                  int prompt_type, int file_type);


#endif /* OSIFCNET_H */
