/* $Header$ */

/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  osnetwin.h - TADS networking and threading implementation for Windows
Function
  
Notes
  
Modified
  04/05/10 MJRoberts  - Creation
*/

#ifndef OSNETWIN_H
#define OSNETWIN_H

/* include the windows system headers */
#include <WinSock2.h>
#include <Windows.h>
#include <WinInet.h>
#include <io.h>

/* include the base header */
#include "osifcnet.h"

/* base t3 header */
#include "t3std.h"

/* ------------------------------------------------------------------------ */
/* 
 *   the FOREVER timeout value 
 */
#define OS_FOREVER INFINITE


/* ------------------------------------------------------------------------ */
/*
 *   Host callback functions.  These functions must be defined by the host
 *   application for use by the Windows OS layer.  
 */

/*
 *   Process a Windows message.  The network "wait" functions invoke this
 *   when messages arrive while waiting for a network event, to ensure that a
 *   long network wait doesn't freeze the UI.
 *   
 *   This should process the message through the same handling that it would
 *   receive in the application's main event loop.  Console-mode
 *   configurations that don't have event loops can simply provide an empty
 *   stub for this.  
 */
void osnet_host_process_message(MSG *msg);


/* ------------------------------------------------------------------------ */
/*
 *   Foreground window negotiation.  Windows doesn't allow processes to bring
 *   themselves to the foreground unilaterally, but it does allow a
 *   foreground application to explicitly yield the foreground.  We use our
 *   pipe control channel to allow Workbench to control whether Workbench or
 *   the Web UI is in the foreground.  
 */
void osnet_webui_yield_foreground();
void osnet_webui_to_foreground();

/* AllowSetForegroundWindow cover (since it's only in Win2k+) */
BOOL tads_AllowSetForegroundWindow(DWORD pid);

/* unlink USER32 (for AllowSetForegroundWindow) */
void tads_unlink_user32();


/* ------------------------------------------------------------------------ */
/*
 *   Protected counter.  This object maintains an integer count that can be
 *   incremented and decremented.  The inc/dec operations are explicitly
 *   atomic, meaning they're thread-safe: it's not necessary to use any
 *   additional access serialization mechanism to perform an increment or
 *   decrement.  
 */
class OS_Counter
{
public:
    /* initialize with a starting count value */
    OS_Counter(long c = 1) { cnt = c; }

    /* get the current counter value */
    long get() const { return cnt; }

    /* 
     *   Increment/decrement.  These return the post-update result.
     *   
     *   The entire operation must be atomic.  In machine language terms, the
     *   fetch and set must be locked against concurrent access by other
     *   threads or other CPUs.
     */
    long inc() { return InterlockedIncrement(&cnt); }
    long dec() { return InterlockedDecrement(&cnt); }

private:
    /* reference count */
    LONG cnt;
};

/* ------------------------------------------------------------------------ */
/*
 *   Include the reference-counted object base classes. 
 */
#include "vmrefcnt.h"

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
class OS_Waitable
{
public:
    OS_Waitable()
    {
        /* no handle yet */
        h = 0;
    }

    virtual ~OS_Waitable()
    {
        /* close the handle if we have one */
        if (h != 0)
            CloseHandle(h);
    }

    /* 
     *   Wait for the object, with a maximum wait of 'timeout' milliseconds.
     *   Returns an OSWAIT_xxx code to indicate what happened.  
     */
    virtual int wait(unsigned long timeout = OS_FOREVER)
    {
        /* if there's no handle, it's an error */
        if (h == 0)
            return OSWAIT_ERROR;

        /* do a multi-wait for this one object */
        OS_Waitable *lst[] = { this };
        return multi_wait(1, lst, timeout);
    }

    /*
     *   Test to see if the object is ready, without blocking.  Returns true
     *   if the object is ready, false if not.  If the object is ready,
     *   waiting for the object would immediately release the thread.
     *   
     *   Note that testing some types of objects "consumes" the ready state
     *   and resets the object to not-ready.  This is true of auto-reset
     *   event objects.  
     */
    virtual int test()
    {
        /* if there's no object handle, consider it not ready */
        if (h == 0)
            return FALSE;

        /* wait for the object with a zero timeout */
        return WaitForSingleObject(h, 0) == WAIT_OBJECT_0;
    }

    /*
     *   Wait for multiple objects.  This blocks until at least one event in
     *   the list is signaled, at which point it returns OSWAIT_OBJECT+N,
     *   where N is the array index in 'events' of the event that fired.  If
     *   the timeout (given in milliseconds) expires before any events fire,
     *   we return OSWAIT_TIMEOUT.  If the timeout is omitted, we don't
     *   return until an event fires.  
     */
    static int multi_wait(int cnt, OS_Waitable **objs,
                          unsigned long timeout = OS_FOREVER);

    /* get my system object handle */
    HANDLE get_handle() const { return h; }

protected:
    /*
     *   Process messages.  We call this when messages are available in the
     *   queue for one of our wait functions.
     */
    static void process_messages()
    {
        /* keep going until no more messages are immediately available */
        for (;;)
        {
            /* if no messages are available, we're done */
            MSG msg;
            if (!PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE))
                break;

            /* get the message */
            if (GetMessage(&msg, 0, 0, 0) <= 0)
            {
                /* Quit message or error - stop now */
                break;
            }

            /* send the message to the host app for processing */
            osnet_host_process_message(&msg);
        }
    }

    /* 
     *   The waitable handle.  All waitable objects on Windows are
     *   represented by standard Windows handles.  
     */
    HANDLE h;
};

/* ------------------------------------------------------------------------ */
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
class OS_Event: public OS_Waitable, public CVmRefCntObj
{
public:
    /* construct */
    OS_Event(int manual_reset)
    {
        /* remember the type of object we're creating */
        this->manual_reset = manual_reset;

        /* 
         *   If it's a manual reset event, create an Event object; otherwise
         *   create a Semaphore object.  
         */
        this->h = (manual_reset
                   ? CreateEvent(0, TRUE, FALSE, 0)
                   : CreateSemaphore(0, 0, 0x7FFFFFFF, 0));
    }

    /*
     *   Signal the event.  In the case of a manual-reset event, this
     *   releases all threads waiting for the event, and leaves the event in
     *   a signaled state until it's explicitly reset.  For an auto-reset
     *   event, this releases only one thread waiting for the event and then
     *   automatically resets the event.  
     */
    void signal()
    {
        if (manual_reset)
            SetEvent(h);
        else
            ReleaseSemaphore(h, 1, 0);
    }

    /*
     *   Reset the event.  For a manual-reset event, this returns the event
     *   to the unsignaled state.  This has no effect for an auto-reset
     *   event.  
     */
    void reset()
    {
        if (manual_reset)
            ResetEvent(h);
    }

private:
    /* 
     *   Is this a manual-reset event?  True means that this is a manual
     *   event: signaling the event will release all threads waiting for the
     *   event, and the event will remain signaled until it's explicitly
     *   cleared.  False means that this is an auto-reset event: signaling
     *   the event releases only ONE thread waiting for the event, and as
     *   soon as the thread is released, the system automatically clears the
     *   event, so no more threads will be released until the next signal.  
     */
    int manual_reset;
};

/* ------------------------------------------------------------------------ */
/*
 *   Mutex.  
 */
class OS_Mutex: public CVmRefCntObj
{
public:
    OS_Mutex()
    {
        /* create the system mutex object */
        h = CreateMutex(0, FALSE, 0);
    }

    ~OS_Mutex()
    {
        /* destroy the mutex */
        if (h != 0)
            CloseHandle(h);
    }

    /* 
     *   Lock the mutex.  Blocks until the mutex is available. 
     */
    void lock() { WaitForSingleObject(h, INFINITE); }

    /* 
     *   Test the mutex: if the mutex is available, returns true; if not,
     *   returns false.  Doesn't block in either case.  If the return value
     *   is true, the mutex is acquired as though lock() had been called, so
     *   the caller must use release() when done.  
     */
    int test() { return WaitForSingleObject(h, 0) == WAIT_OBJECT_0; }

    /* 
     *   Unlock the mutex.  This can only be used after a successful lock()
     *   or test().  This releases our lock and makes the mutex available to
     *   other threads.  
     */
    void unlock() { ReleaseMutex(h); }
    
private:
    /* the underlying system mutex handle */
    HANDLE h;
};

/* ------------------------------------------------------------------------ */
/* 
 *   Socket error indicator - returned from OS_Socket::send() and recv() if
 *   an error occurs, in lieu of a valid length value.  
 */
#define OS_SOCKET_ERROR  SOCKET_ERROR

/*
 *   Error values for OS_Socket::last_error(). 
 */
#define OS_EWOULDBLOCK  WSAEWOULDBLOCK             /* send/recv would block */
#define OS_ECONNRESET   WSAECONNRESET           /* connection reset by peer */
#define OS_ECONNABORTED WSAECONNABORTED   /* connection aborted on this end */

/* ------------------------------------------------------------------------ */
/*
 *   Core Socket.  This is the common subclass of data sockets and listener
 *   sockets.  
 */
class OS_CoreSocket: public CVmRefCntObj, public OS_Waitable
{
public:
    OS_CoreSocket()
    {
        /* no socket, event, or error yet */
        s = INVALID_SOCKET;
        evt = 0;
        err = 0;
    }

    virtual ~OS_CoreSocket()
    {
        /* close our socket */
        close();
    }

    /* 
     *   Set the socket to non-blocking mode.  In this mode, a send/recv on
     *   the socket will return immediately with OS_SOCKET_ERROR if the
     *   socket's data buffer is empty on read or full on write.  By default,
     *   these calls "block": they don't return until at least one byte is
     *   available to read or there's buffer space for all of the data being
     *   written.
     */
    void set_non_blocking()
    {
        /* create the event object */
        h = evt = WSACreateEvent();

        /* 
         *   Windows automatically sets the socket to non-blocking mode when
         *   it has an associated event object for the relevant operations.
         *   So, to activate the non-blocking mode, associate our event with
         *   Read, Write, and Close events on the socket.  
         */
        WSAEventSelect(s, evt, non_blocking_event_bits());
    }

    /*
     *   In non-blocking mode, the caller must manually reset the socket's
     *   event waiting status after each successful wait.
     */
    void reset_event()
    {
        if (evt != 0)
            WSAResetEvent(evt);
    }

    /* 
     *   Get the last error for a send/receive operation.  Returns an OS_Exxx
     *   value.  
     */
    int last_error() const { return err; }

    /* close the socket */
    void close()
    {
        /* close the socket, if it's still open */
        if (s != INVALID_SOCKET)
        {
            closesocket(s);
            s = INVALID_SOCKET;
        }

        /* destroy the socket event, if applicable */
        if (evt != 0)
        {
            WSACloseEvent(evt);
            evt = 0;
            h = 0;
        }
    }

    /*
     *   Get the IP address and port for our side of the connection.  'ip'
     *   receives an allocated string with the IP address string in decimal
     *   notation ("192.168.115").  We fill in 'port' with our port number.
     *   Returns true on success, false on failure.  If we return success,
     *   the caller is responsible for freeing the 'ip' string with
     *   lib_free_str().  
     */
    int get_local_addr(char *&ip, int &port)
    {
        /* set up the address structure */
        sockaddr_storage addr;
        int len = sizeof(addr);

        /* get the peer address - return null if this fails */
        if (getsockname(s, (sockaddr *)&addr, &len))
            return FALSE;

        /* parse the address information */
        return parse_addr(addr, len, ip, port);
    }

    /*
     *   Get the IP address and port for the network peer to which we're
     *   connected.  'ip' receives an allocated string with the IP address
     *   string in decimal notation ("192.168.1.15").  We fill in 'port' with
     *   the port number on the remote host.  Returns true on success, false
     *   on failure.  If we return success, the caller is responsible for
     *   freeing the allocated 'ip' string via lib_free_str().
     */
    int get_peer_addr(char *&ip, int &port)
    {
        /* set up the address structure */
        sockaddr_storage addr;
        int len = sizeof(addr);

        /* get the peer address - return null if this fails */
        if (getpeername(s, (sockaddr *)&addr, &len))
            return FALSE;

        /* parse the address */
        return parse_addr(addr, len, ip, port);
    }

    /*
     *   Get the IP address for the local host.  The return value is an
     *   allocated buffer that the caller must delete with 'delete[]'.  If
     *   the host IP address is unavailable, returns null.  
     */
    static char *get_host_ip(const char *hostname)
    {
        /* 
         *   Get the local host information.  On Windows, we can provide an
         *   empty string as the host name to tell the system to look the
         *   name up for us.  
         */
        struct hostent *local_host = gethostbyname(hostname);
        const char *a =
            local_host != 0
            ? inet_ntoa(*(struct in_addr *)*local_host->h_addr_list) : 0;

        /* if it's null, return null */
        if (a == 0)
            return 0;

        /* make an allocated copy of the string */
        char *buf = new char[strlen(a) + 1];
        strcpy(buf, a);

        /* return the allocated copy */
        return buf;
    }

protected:
    /* create a socket object wrapping an existing OS socket handle */
    OS_CoreSocket(SOCKET s)
    {
        /* remember the socket */
        this->s = s;

        /* no error or non-blocking-mode event object yet */
        err = 0;
        evt = 0;
    }

    /* parse an address */
    int parse_addr(sockaddr_storage &addr, int len, char *&ip, int &port)
    {
        /* presume failure */
        ip = 0;
        port = 0;

        /* check which family we have */
        if (addr.ss_family == AF_INET)
        {
            /* get the ip address structure */
            struct sockaddr_in *s = (struct sockaddr_in *)&addr;

            /* get the port and the IP name string */
            port = ntohs(s->sin_port);
            char *ipp = inet_ntoa(s->sin_addr);

            /* if we didn't get a name string, abort */
            if (ipp == 0)
                return FALSE;

            /* make the caller's copy of the return string */
            ip = lib_copy_str(ipp);

            /* success */
            return TRUE;
        }
        else
        {
            /* we don't handle other protocol families */
            return FALSE;
        }
    }

    /* 
     *   The non-blocking events bits for this socket type.  This is a
     *   combination of FD_xxs bits for WSAEventSelect(). 
     */
    virtual DWORD non_blocking_event_bits() const = 0;

    /* our underlying system socket handle */
    SOCKET s;

    /* last read/write socket error code */
    int err;

    /* for a non-blocking socket, the socket event object */
    WSAEVENT evt;
};

/* ------------------------------------------------------------------------ */
/*
 *   Data socket. 
 */
class OS_Socket: public OS_CoreSocket
{
    friend class OS_Listener;

public:
    /* 
     *   Send bytes.  Returns the number of bytes sent, or OS_SOCKET_ERROR if
     *   an error occurs.  
     */
    int send(const char *buf, size_t len)
    {
        /* send the bytes and note the result */
        int ret = ::send(s, buf, len, 0);

        /* if an error occurred, note it */
        err = (ret == SOCKET_ERROR ? WSAGetLastError() : 0);

        /* return the result */
        return ret;
    }

    /* 
     *   Receive bytes.  If the return value is >0, it's the number of bytes
     *   received.  If 0, it means that the peer has closed its end of the
     *   socket.  If <0, it indicates an error; the specific error code can
     *   be obtained from last_error().  
     */
    int recv(char *buf, size_t len)
    {
        /* read the bytes and note the result */
        int ret = ::recv(s, buf, len, 0);

        /* if an error occurred, note it */
        err = (ret == SOCKET_ERROR ? WSAGetLastError() : 0);

        /* return the result */
        return ret;
    }

protected:
    /* wrap an existing system socket in an OS_Socket object */
    OS_Socket(SOCKET s, struct sockaddr_in *addr, size_t addrlen)
        : OS_CoreSocket(s)
    {
        /* save the socket address, if present */
        if (addr != 0)
        {
            this->addr = (struct sockaddr_in *)osmalloc(addrlen);
            this->addrlen = addrlen;
            memcpy(this->addr, addr, addrlen);
        }
        else
        {
            this->addr = 0;
            this->addrlen = 0;
        }
    }

    /* 
     *   get the non-blocking event bits - for a data socket, set events for
     *   read, write, and closing the socket 
     */
    virtual DWORD non_blocking_event_bits() const
        { return FD_READ | FD_WRITE | FD_CLOSE; }

    /* network address of peer */
    struct sockaddr_in *addr;
    size_t addrlen;
};

/* ------------------------------------------------------------------------ */
/*
 *   Listener.  This class represents a network listener socket, which is
 *   used to wait for and accept new incoming connections from clients. 
 */
class OS_Listener: public OS_CoreSocket
{
public:
    /* 
     *   Construction.  This sets up the object, but doesn't open a listener
     *   port.  Call open() to actually open the port. 
     */
    OS_Listener() { }

    ~OS_Listener() { }

    /* 
     *   Open the listener on the given port number.  This can be used to
     *   create a server on a well-known port, but it has the drawback that
     *   ports can't be shared, so this will fail if another process is
     *   already using the same port number.
     *   
     *   Returns true on success, false on failure.  
     */
    int open(const char *hostname, unsigned short port_num)
    {
        /* create our socket */
        if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
            return FALSE;

        /* bind to the given host address */
        char *ip = get_host_ip(hostname);
        if (ip == 0)
            return FALSE;
        
        /* 
         *   set up the address request structure to bind to the requested
         *   port on the local address we just figured 
         */
        struct sockaddr_in saddr;
        saddr.sin_family = AF_INET;
        saddr.sin_addr.s_addr = inet_addr(ip);
        saddr.sin_port = htons(port_num);

        /* done with the IP address */
        delete [] ip;

        /* try binding */
        if (bind(s, (SOCKADDR *)&saddr, sizeof(saddr)))
            return FALSE;

        /* put the socket into the 'listening' state */
        if (listen(s, SOMAXCONN))
            return FALSE;
        
        /* success */
        return TRUE;
    }

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
    int open(const char *hostname) { return open(hostname, 0); }

    /*
     *   Accept the next incoming connection.  If the listener is in blocking
     *   mode (the default), and there are no pending requests, this blocks
     *   until the next pending request arrives.  In non-blocking mode, this
     *   returns immediately with a null socket if no requests are pending,
     *   and last_error() indicates OS_EWOULDBLOCK.  
     */
    OS_Socket *accept()
    {
        /* accept a connection on the underlying system socket */
        struct sockaddr_in addr;
        int addrlen = sizeof(addr);
        SOCKET snew = ::accept(s, (SOCKADDR *)&addr, &addrlen);

        /* check to see if we got a socket */
        if (snew != INVALID_SOCKET)
        {
            /* success - clear the error memory */
            err = 0;

            /* 
             *   remove the parent event association from the socket -
             *   winsock sockets essentially inherit the parent event
             *   settings, but we'll want different event handling for this
             *   new http socket 
             */
            WSAEventSelect(snew, 0, 0);

            /* 
             *   Set the "linger" option on close.  This helps ensure that we
             *   transmit any error response to a request that causes the
             *   server thread to abort.  Without this option, winsock will
             *   sometimes terminate the connection before transmitting the
             *   final response.
             *   
             *   NB - on second thought, I removed this; the winsock
             *   documentation advises against using this for non-blocking
             *   sockets, because it can prevent them from ever closing.  It
             *   seems better to set the "graceful close" option (linger
             *   off).  
             */
            //struct linger ls = { 1, 1 }; // linger for up to one second
            struct linger ls = { 0, 0 };   // graceful close, no wait
            setsockopt(snew, SOL_SOCKET, SO_LINGER,
                       (const char *)&ls, sizeof(ls));
            
            /* wrap the socket in an OS_Socket and return the object */
            return new OS_Socket(snew, &addr, addrlen);
        }
        else
        {
            /* failed - remember the system error code */
            err = WSAGetLastError();

            /* return failure */
            return 0;
        }
    }

protected:
    /* 
     *   get the non-blocking event bits - for a listener socket, set events
     *   for accept and close
     */
    virtual DWORD non_blocking_event_bits() const
        { return FD_ACCEPT | FD_CLOSE; }
};


/* ------------------------------------------------------------------------ */
/*
 *   Thread.  Callers subclass this to define the thread entrypoint routine.
 */
class OS_Thread: public CVmWeakRefable, public OS_Waitable
{
    friend class TadsThreadList;

public:
    OS_Thread();
    ~OS_Thread();

    /* launch the system thread */
    int launch()
    {
        /* add a reference on behalf of the system thread */
        add_ref();

        /* create the system thread */
        DWORD tid;
        h = CreateThread(0, 0, &sys_thread_main, this, 0, &tid);

        /* if we failed to launch the thread, clean up */
        if (h == 0)
        {
            /* 
             *   release the extra reference for the system thread, since the
             *   thread won't be around to claim it 
             */
            release_ref();

            /* mark ourselves as done for 'wait' and 'test' */
            failed = TRUE;
        }

        /* if we got a handle, we launched the thread successfully */
        return h != 0;
    }

    /*
     *   Thread entrypoint routine.  Callers must subclass this class and
     *   provide a concrete definition for this method.  This method is
     *   called at thread entry. 
     */
    virtual void thread_main() = 0;

    /*
     *   Wait and test return true if the thread has exited, or we failed to
     *   launch the thread. 
     */
    int wait(unsigned long timeout = OS_FOREVER)
    {
        /* if the thread failed to launch, consider it done */
        if (failed)
            return OSWAIT_EVENT;

        /* otherwise, wait for the thread */
        return OS_Waitable::wait(timeout);
    }
    int test()
    {
        /* if the thread failed to launch, consider it done */
        if (failed)
            return TRUE;

        /* otherwise do the normal test */
        return OS_Waitable::test();
    }

private:
    /* 
     *   Static entrypoint.  This is the routine the system calls directly;
     *   we then invoke the virtual method with the subclassed thread main. 
     */
    static DWORD WINAPI sys_thread_main(void *ctx)
    {
        /* cast the context to the 'this' pointer */
        OS_Thread *thread = (OS_Thread *)ctx;

        /* invoke the subclasses entrypoint */
        thread->thread_main();

        /* release the system thread's reference on the thread object */
        thread->release_ref();

        /* return a generic result code */
        return 0;
    }

    /* the thread failed to launch */
    int failed;
};

/* master thread list global */
extern class TadsThreadList *G_thread_list;


#endif /* OSNETWIN_H */
