/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmnet.cpp - TADS 3 networking
Function
  Implements the networking layer for TADS 3.  This is used for the web
  server configurations.  This is portable code implementing high-level
  network operations, such as an HTTP server, using the low-level socket and
  thread API defined in osifcnet.h.
Notes

Modified
  04/11/10 MJRoberts  - Creation
*/


#include <stdlib.h>
#include <string.h>

#include "t3std.h"
#include "os.h"
#include "osifcnet.h"
#include "vmnet.h"
#include "vmglob.h"
#include "vmtype.h"
#include "vmobj.h"
#include "vmstr.h"
#include "vmlst.h"
#include "vmisaac.h"
#include "sha2.h"
#include "vmpredef.h"
#include "vmhttpreq.h"
#include "vmglob.h"
#include "vmmain.h"
#include "vmpredef.h"
#include "vmerrnum.h"
#include "vmrun.h"
#include "vmfile.h"


/* ------------------------------------------------------------------------ */
/*
 *   Server manager 
 */
TadsServerManager::TadsServerManager()
{
    /* initialize our ISAAC context */
    ic = new isaacctx();
    isaac_init(ic, TRUE);

    /* create our mutex */
    mutex = new OS_Mutex();
}

TadsServerManager::~TadsServerManager()
{
    /* done with our ISAAC context */
    delete ic;

    /* done with our mutex */
    mutex->release_ref();
}

/*
 *   generate a random number 
 */
ulong TadsServerManager::rand()
{
    /* ISAAC needs protection against multi-threaded access */
    mutex->lock();

    /* get a random number */
    ulong r = isaac_rand(ic);

    /* done with the mutex */
    mutex->unlock();

    /* return the number */
    return r;
}

/*
 *   Generate a random ID string 
 */
static inline char nybble2hex(unsigned char c)
{
    c &= 0x0f;
    return (c < 10 ? c + '0' : c - 10 + 'a');
}
char *TadsServerManager::gen_rand_id(void *obj)
{
    /* set up a hashing buffer */
    sha256_ctx s;
    sha256_begin(&s);

    /* add the current date/time to the hash */
    os_time_t timer = os_time(0);
    struct tm *tblock = os_localtime(&timer);
    sha256_hash((unsigned char *)tblock, sizeof(*tblock), &s);

    /* add the system timer to the hash */
    long systime = os_get_sys_clock_ms();
    sha256_hash((unsigned char *)&systime, sizeof(systime), &s);

    /* add the object address to the hash */
    sha256_hash((unsigned char *)obj, sizeof(obj), &s);

    /* add the current stack location to the hash */
    sha256_hash((unsigned char *)&obj, sizeof(void *), &s);

    /* add some random bytes from the operating system */
    unsigned char rbuf[128];
    os_gen_rand_bytes(rbuf, sizeof(rbuf));
    sha256_hash(rbuf, sizeof(rbuf), &s);

    /* compute the hash */
    unsigned char hval[32];
    sha256_end(hval, &s);

    /* convert it to hex, but just keep the low nybbles, for 32 digits */
    char *ret = lib_alloc_str(32);
    int i;
    for (i = 0 ; i < 32 ; ++i)
        ret[i] = nybble2hex(hval[i]);

    /* null-terminate the string */
    ret[i] = '\0';

    /* return the allocated string */
    return ret;
}


/* ------------------------------------------------------------------------ */
/*
 *   Message queue 
 */

/*
 *   wait for a message 
 */
int TadsMessageQueue::wait(VMG_ unsigned long timeout, TadsMessage **msgp)
{
    /* assume we won't return a message */
    *msgp = 0;

    /* 
     *   Wait for a message to arrive in the queue.  The sender will signal
     *   our event upon posting a message.  Note that our assumption that
     *   there's only one reader thread per queue means that we don't have to
     *   worry about another reader swiping the signaled message between the
     *   time the signal went off and the time we read the queue.
     *   
     *   Also stop waiting if the quit event has been signaled, or the debug
     *   event is signaled.  
     */
    OS_Waitable *w[3];
    int cnt = 0;
    w[cnt++] = ev;

    /* add the quit event, if present */
    if (quit_evt != 0)
        w[cnt++] = quit_evt;

    /* add the debug break event, if present */
    OS_Event *brkevt = 0;
    VM_IF_DEBUGGER(if (G_debugger != 0)
        w[cnt++] = brkevt = G_debugger->get_break_event());
    
    /* wait for an event */
    int ret = OS_Waitable::multi_wait(cnt, w, timeout);
    
    /* if the message event fired, retrieve the message */
    if (ret == OSWAIT_EVENT + 0)
        *msgp = get();
    
    /* 
     *   always return +2 for the debug event (we'll have +1 from the
     *   multi_wait if the list doesn't have a quit event) 
     */
    if (w[ret - OSWAIT_EVENT] == brkevt)
        ret = OSWAIT_EVENT + 2;

    /* release the debugger break event, if we got it */
    if (brkevt != 0)
        brkevt->release_ref();
    
    /* return the result */
    return ret;
}


/* ------------------------------------------------------------------------ */
/* 
 *   Custom string object.  This is a convenience class for complex string
 *   construction, which the network servers do a lot of.  
 */
struct NetString
{
    NetString(size_t initsize) { init(initsize); }
    NetString() { init(50); }

    void init(size_t initsize)
    {
        buf = (char *)t3malloc(initsize);
        buf[0] = '\0';
        len = initsize;
        inc = (initsize > 128 ? initsize / 2 : 128);
        wrtidx = 0;
    }

    ~NetString()
    {
        if (buf != 0)
            t3free(buf);
    }

    /* clear the buffer */
    void clear()
    {
        buf[0] = '\0';
        wrtidx = 0;
    }

    /* the buffer, its total size, and the size increment */
    char *buf;
    size_t len;
    size_t inc;

    /* the current buffer index */
    size_t wrtidx;

    /* reserve space for an addition 'b' bytes */
    void reserve(size_t b)
    {
        if (wrtidx + b + 1 > len)
        {
            /* increase by the increment until big enough */
            while (wrtidx + b + 1 > len)
                len += inc;

            /* reallocate at the increased size */
            buf = (char *)t3realloc(buf, len);
        }
    }

    /* append text */
    void append(const char *txt, size_t txtlen)
    {
        /* make sure there's room */
        reserve(txtlen);

        /* copy the text */
        memcpy(buf + wrtidx, txt, txtlen);

        /* adjust the counters */
        wrtidx += txtlen;

        /* 
         *   null terminate (but don't count it - we can overwrite it with a
         *   subsequent append) 
         */
        buf[wrtidx] = '\0';
    }

    void append(const char *txt)
    {
        append(txt, strlen(txt));
    }

    /* append text to a buffer, with sprintf-style formatting */
    void vappendf(const char *fmt, va_list argp)
    {
        /* calculate the amount of space we need */
        int plen = t3vsprintf(0, 0, fmt, argp);

        /* if that worked, format the data */
        if (plen >= 0)
        {
            /* reserve the required space */
            reserve(plen);

            /* format the text */
            int plen = t3vsprintf(buf + wrtidx, len - wrtidx, fmt, argp);

            /* if that worked, advance the buffer pointers */
            if (plen >= 0)
                wrtidx += plen;

            /* null-terminate at the new length in any case */
            buf[wrtidx] = '\0';
        }
    }

    void appendf(const char *fmt, ...)
    {
        va_list argp;

        /* process through our va_list version */
        va_start(argp, fmt);
        vappendf(fmt, argp);
        va_end(argp);
    }

    /* 
     *   append text, converting plain text to valid XML by turning markup
     *   characters into entities 
     */
    void append_xml(const char *txt)
    {
        const char *p, *start;

        /* append in chunks as we find special characters */
        for (p = start = txt ; *p != '\0' ; ++p)
        {
            static const char *xlat[] =
            {
                "&lt;",
                "&gt;",
                "&amp;"
            };
            static const char *specials = "<>&";
            const char *psp = strchr(specials, *p);
            int sp_index = (psp == 0 ? -1 : psp - specials);

            /* if we're at a special character, quote it */
            if (sp_index >= 0)
            {
                /* append the plain chunk up to this point */
                if (p != start)
                    append(start, p - start);

                /* append the entity version of the special character */
                append(xlat[sp_index]);

                /* the next chunk starts at the next character */
                start = p + 1;
            }
        }

        /* append the final chunk */
        if (p != start)
            append(start, p - start);
    }

    /* format then append as XML */
    void vappendf_xml(const char *fmt, va_list argp)
    {
        NetString tmp(512);

        /* format the message */
        tmp.vappendf(fmt, argp);

        /* append as XML */
        append_xml(tmp.buf);
    }

    /* append text, converting plain text to URL encoding */
    void append_urlencode(const char *txt)
    {
        const char *p, *start;

        /* append in chunks as we find special characters */
        for (p = start = txt ; *p != '\0' ; ++p)
        {
            char c = *p;
            if (isalpha(c) || isdigit(c) || strchr("-_.", c) != 0)
            {
                /* it's an ordinary character - keep it as-is */
            }
            else
            {
                /* special character - quote it */

                /* copy the part up to the special character */
                if (p != start)
                    append(start, p - start);

                /* append '+' for a space or a '%' encoding for others */
                if (c == ' ')
                    append("+");
                else
                    appendf("%%%02x", (int)(unsigned char)c);

                /* the next chunk starts at the next character */
                start = p + 1;
            }
        }

        /* append the final chunk */
        if (p != start)
            append(start, p - start);
    }
};


/* ------------------------------------------------------------------------ */
/*
 *   Given an XML buffer, find the end of the <?XML?> header and the start of
 *   the regular XML contents 
 */
const char *TadsXml::strip_xml_header(const char *buf)
{
    const char *p;

    /* skip any initial newlines or whitespace */
    for (p = buf ; isspace(*p) || *p == '\n' || *p == '\r' ; ++p) ;

    /* if it starts with "<?xml", find the end of the directive */
    if (memicmp(p, "<?xml ", 6) == 0)
    {
        int qu;

        /* scan for the matching '>' */
        for (p += 6, qu = 0 ; *p != '\0' ; ++p)
        {
            /* check for quotes */
            if (*p == '"' || *p == '\'')
            {
                /* 
                 *   if we're in a quoted section, and this is the matching
                 *   close quote, leave the quoted section; if we're not in a
                 *   quoted section, this is the opening quote of a quoted
                 *   section 
                 */
                if (qu == 0)
                {
                    /* we weren't in a quoted section, so now we are */
                    qu = *p;
                }
                else if (*p == qu)
                {
                    /* we were in a quoted section, and it's our close quote */
                    qu = 0;
                }
            }
            else if (*p == '?' && *(p+1) == '>')
            {
                /* 
                 *   It's the end of the directive.  Skip the "?>" sequence
                 *   and any subsequent newline characters.  
                 */
                for (p += 2 ; *p == '\n' || *p == '\r' ; ++p) ;

                /* 
                 *   we're now at the start of the XML contents - return the
                 *   current pointer 
                 */
                return p;
            }
        }
    }

    /* 
     *   we either didn't find the start of the <?XML?> directive, or we
     *   couldn't find the end of it - in either case, we don't have a
     *   well-formed directive, so there's nothing to strip: just return the
     *   original buffer 
     */
    return buf;
}

/* ------------------------------------------------------------------------ */
/*
 *   HTTP Server 
 */

/*
 *   construct 
 */
TadsListener::TadsListener(TadsListenerThread *t)
{
    /* remember the thread, and note our reference */
    thread = t;
    t->add_ref();
}

TadsListener::~TadsListener()
{
    /* release our referenced objects */
    thread->release_ref();
}

/*
 *   launch a server 
 */
TadsListener *TadsListener::launch(const char *hostname, ushort portno,
                                   TadsListenerThread *thread)
{
    /* open the network port */
    OS_Listener *l = new OS_Listener();
    if (!l->open(hostname, portno))
    {
        /* couldn't open the port - delete the listener and return failure */
        l->release_ref();
        return 0;
    }

    /* put the port into non-blocking mode */
    l->set_non_blocking();

    /* hand the network port over to the thread - it assumes our reference */
    thread->set_port(l);

    /* launch the listener thread */
    if (!thread->launch())
    {
        /* couldn't launch the thread - delete it and return failure */
        thread->release_ref();
        return 0;
    }

    /* return the new listener object */
    return new TadsListener(thread);
}

/*
 *   notify the server that it's time to shut down
 */
void TadsListener::shutdown()
{
    /* set the 'quit' event in our thread */
    thread->shutdown_evt->signal();
}


/* ------------------------------------------------------------------------ */
/*
 *   Generic listener thread base class. 
 */

/*
 *   Deletion 
 */
TadsListenerThread::~TadsListenerThread()
{
    /* release our port, if we have one */
    if (port != 0)
        port->release_ref();
    
    /* release our quit event and shutdown event */
    quit_evt->release_ref();
    shutdown_evt->release_ref();
    
    /* release our mutex */
    mutex->release_ref();
    
    /* free the error message string */
    lib_free_str(errmsg);

    /* free the password string */
    lib_free_str(password);
    
    /* 
     *   Release all of our server threads.  Note that we don't need to
     *   protect against concurrent access here because we know that no one
     *   has a reference to this object any longer - it's the only way we can
     *   be deleted.  
     */
    TadsServerThread *tcur, *tnxt;
    for (tcur = servers ; tcur != 0 ; tcur = tnxt)
    {
        /* remember the next one, and release this one */
        tnxt = tcur->next_server;
        tcur->release_ref();
    }
}

/*
 *   Thread main 
 */
void TadsListenerThread::thread_main()
{
    /* 
     *   keep going until we get the general application-wide 'quit' signal
     *   or our own private listener shutdown event 
     */
    while (!quit_evt->test() && !shutdown_evt->test())
    {
        /* 
         *   wait for a new connection request OR the quit signal, whichever
         *   comes first 
         */
        OS_Waitable *w[] = { port, quit_evt, shutdown_evt };
        switch (OS_Waitable::multi_wait(3, w))
        {
        case OSWAIT_EVENT + 0:
            /* the port is ready - reset the event */
            port->reset_event();

            /* read connections as long as they're available */
            for (;;)
            {
                /* check for a connection request */
                OS_Socket *s = port->accept();

                /* if we're out of requests, go back to waiting */
                if (s == 0 && port->last_error() == OS_EWOULDBLOCK)
                    break;

                /* check for other errors */
                if (s == 0)
                {
                    /* failed - flag the error and shut down */
                    errmsg = t3sprintf_alloc(
                        "Listener thread failed: error %d from accept()",
                        port->last_error());
                    shutdown_evt->signal();
                    break;
                }

                /* put the socket into non-blocking mode */
                s->set_non_blocking();

                /* create the server thread (it takes over the socket ref) */
                TadsServerThread *st = create_server_thread(s);
                st->thread_id = next_thread_id++;

                /* launch the server thread */
                if (!st->launch())
                {
                    /* failed - flag the error and shut down */
                    errmsg = lib_copy_str("Listener thread failed: "
                                          "couldn't launch server thread");
                    shutdown_evt->signal();

                    /* release our reference on the thread */
                    st->release_ref();

                    /* done */
                    break;
                }

                /* we're done with our reference to the thread */
                st->release_ref();
            }
            break;

        case OSWAIT_EVENT + 1:
            /* 
             *   The quit signal fired - the whole app is terminating.
             *   Signal our internal shutdown event and abort. 
             */
            shutdown_evt->signal();
            break;

        case OSWAIT_EVENT + 2:
            /* shutdown signal fired - the listener is terminating; abort */
            break;
        }
    }

    /* wait for our server threads to exit */
    for (;;)
    {
        TadsServerThread *st;
        
        /* get the first thread from the list */
        mutex->lock();
        if ((st = servers) != 0)
            st->add_ref();
        mutex->unlock();

        /* if we're out of threads, we're done */
        if (st == 0)
            break;

        /* wait for this thread */
        st->wait();

        /* we're done with this thread */
        st->release_ref();
    }
}

/*
 *   Add a thread to our list 
 */
void TadsListenerThread::add_thread(TadsServerThread *t)
{
    /* protect against concurrent access */
    mutex->lock();

    /* link the thread into our list */
    t->next_server = servers;
    servers = t;

    /* keep a reference on it */
    t->add_ref();

    /* done with concurrent access protection */
    mutex->unlock();
}

/*
 *   Remove a thread from our list 
 */
void TadsListenerThread::remove_thread(TadsServerThread *t)
{
    /* protect against concurrent access */
    mutex->lock();

    /* find the thread */
    TadsServerThread *cur, *prv;
    for (cur = servers, prv = 0 ; cur != 0 ;
         prv = cur, cur = cur->next_server)
    {
        /* if this is the one we're looking for, unlink it */
        if (cur == t)
        {
            /* unlink it from the list */
            if (prv != 0)
                prv->next_server = cur->next_server;
            else
                servers = cur->next_server;

            /* release our reference on it */
            t->release_ref();

            /* we're done searching */
            break;
        }
    }

    /* done with concurrent access protection */
    mutex->unlock();
}

/*
 *   generate a human-readable status report of my threads 
 */
void TadsListenerThread::list_threads(NetString *buf)
{
    /* protect against concurrent access */
    mutex->lock();

    /* scan threads */
    for (TadsServerThread *cur = servers ; cur != 0 ; cur = cur->next_server)
    {
        /* report on this thread */
        StringRef *state = cur->get_state();
        buf->appendf("Thread ID=%d (thread object=%lx): %s<br>",
                     cur->thread_id, (unsigned long)cur, state->get());

        /* done with the state string */
        state->release_ref();
    }

    /* done with concurrent access protection */
    mutex->unlock();
}

/* ------------------------------------------------------------------------ */
/*
 *   Generic server thread base class 
 */

void TadsServerThread::thread_main()
{
    /* add this thread to the listener's active server list */
    listener->add_thread(this);

    /* process requests until we encounter an error or Quit signal */
    while (!listener->quit_evt->test()
           && !listener->shutdown_evt->test()
           && process_request()) ;

    /* terminating - close the socket */
    set_run_state("Closing");
    socket->close();

    /* remove myself from the listener's active server list */
    listener->remove_thread(this);

    /* set my final run state */
    set_run_state("Terminated");
}

/*
 *   Read bytes from the other side.  Blocks until there's at least one byte
 *   to read, then reads as many bytes into the buffer as we can without
 *   blocking further.  Aborts if either the listener's "quit" or "shutdown"
 *   event is triggered.
 */
long TadsServerThread::read(char *buf, size_t buflen, long minlen,
                            unsigned long timeout)
{
    /* figure the ending time for the wait */
    unsigned long t = os_get_sys_clock_ms(), t_end = t + timeout;

    /* we haven't read any bytes yet */
    long totlen = 0;

    /* if the caller provided a buffer, we can't read past the buffer */
    if (buf != 0 && minlen > (long)buflen)
        minlen = buflen;
    
    /* keep going until we read some data */
    for (;;)
    {
        int len;
        char ibuf[4096], *dst;
        size_t dstlen;

        /* figure the buffer destination and size to read on this round */
        if (buf == 0)
        {
            /* 
             *   There's no buffer, so read into our internal buffer.  Tead
             *   up to the remaining minimum size, or to our available
             *   internal space, whichever is less.  
             */
            dst = ibuf;
            dstlen = (minlen < sizeof(ibuf) ? minlen : sizeof(ibuf));
        }
        else
        {
            /* 
             *   Read into the caller's buffer, after any data we've read so
             *   far, up to the remaining buffer length. 
             */
            dst = buf + totlen;
            dstlen = buflen - totlen;
        }

        /* read the data */
        set_run_state("Receiving");
        len = socket->recv(dst, dstlen);

        /* if an error occurred, check what happened */
        if (len == OS_SOCKET_ERROR)
        {
            /* presume failure */
            int ok = FALSE;
            
            /* if this is a would-block error, wait for data to arrive */
            if (socket->last_error() == OS_EWOULDBLOCK)
            {
                /* 
                 *   No data available - wait until we receive at least one
                 *   byte, or until the 'quit' event is signaled or a timeout
                 *   occurs.  Figure the next timeout expiration, if we have
                 *   a timeout at all.  
                 */
                if (timeout != OS_FOREVER)
                {
                    /* if we're already past the timeout expiration, fail */
                    t = os_get_sys_clock_ms();
                    if (t > t_end)
                        return -1;

                    /* figure the remaining timeout interval */
                    timeout = t_end - t;
                }

                /* wait */
                set_run_state("Waiting(receive)");
                OS_Waitable *w[] = {
                    socket, listener->quit_evt, listener->shutdown_evt
                };
                if (OS_Waitable::multi_wait(3, w, timeout) == OSWAIT_EVENT + 0)
                {
                    /* the socket is now ready - reset it and keep going */
                    socket->reset_event();
                    ok = TRUE;
                }
            }

            /* if we didn't correct the error, give up */
            if (!ok)
                return -1;
        }
        else if (len == 0)
        {
            /* the socket has been closed - return failure */
            set_run_state("Error(receiving)");
            return -1;
        }
        else if (len >= minlen)
        {
            /* we've satisfied the request - return the bytes */
            set_run_state("Receive completed");
            return totlen + len;
        }
        else
        {
            /* 
             *   We've read some data, but not enough to satisfy the minimum
             *   length request.  Add the current chunk to the total read so
             *   far, and deduct it from the remaining minimum.  
             */
            totlen += len;
            minlen -= len;
        }
    }
}

/*
 *   Send data to the other side.  Blocks until we send all of the data, or
 *   until the Quit event is signaled or we encounter another error.  Returns
 *   true on success, false on failure.  
 */
int TadsServerThread::send(const char *buf, size_t len)
{
    /* keep going until we satisfy the write request or run into trouble */
    while (len != 0)
    {
        /* try sending */
        set_run_state("Sending");
        size_t cur = socket->send(buf, len);

        /* check what happened */
        if (cur == OS_SOCKET_ERROR)
        {
            /* error reading socket - presume failure */
            int ok = FALSE;

            /* if this is a 'would block' error, wait for the socket */
            if (socket->last_error() == OS_EWOULDBLOCK)
            {
                /* wait for the socket to unblock, or for the Quit event */
                set_run_state("Waiting(send)");
                OS_Waitable *w[] = {
                    socket, listener->quit_evt, listener->shutdown_evt
                };
                if (OS_Waitable::multi_wait(3, w) == OSWAIT_EVENT + 0)
                {
                    /* socket is ready - reset it and carry on */
                    socket->reset_event();
                    ok = TRUE;
                }
            }

            /* if we didn't solve the problem, fail */
            if (!ok)
                return FALSE;
        }
        else
        {
            /* adjust the counters for the write */
            buf += cur;
            len -= cur;
        }
    }

    /* we successfully sent all of the requested data */
    set_run_state("Send completed");
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/* 
 *   argument structure for URL parsing 
 */
struct url_param
{
    /* argument name */
    const char *name;

    /* argument value */
    const char *val;
};

struct url_param_alo: url_param
{
    url_param_alo()
    {
        name = 0;
        val = 0;
    }

    url_param_alo * operator =(const url_param &src)
    {
        name = lib_copy_str(src.name);
        val = lib_copy_str(src.val);
        return this;
    }
        
    ~url_param_alo()
    {
        lib_free_str((char *)name);
        lib_free_str((char *)val);
    }
};

#if 0 // not currently used
/* get the value of an argument */
static const char *get_arg_val(const char *name, const char *dflt,
                               const url_param *args, int argc)
{
    int i;

    /* scan for the argument name */
    for (i = 0 ; i < argc ; ++i, ++args)
    {
        if (stricmp(args->name, name) == 0)
            return args->val;
    }

    /* didn't find it */
    return dflt;
}
#endif

/* ------------------------------------------------------------------------ */
/*
 *   HTTP Listener Thread 
 */

/*
 *   construction 
 */
TadsHttpListenerThread::TadsHttpListenerThread(
    vm_obj_id_t srv_obj, TadsMessageQueue *q, long ulim)
    : TadsListenerThread(q->get_quit_evt())
{
    /* remember the HTTPServer object that created the listener */
    this->srv_obj = srv_obj;

    /* save a reference to the queue */
    (queue = q)->add_ref();

    /* save the upload limit */
    upload_limit = ulim;
}

/*
 *   deletion 
 */
TadsHttpListenerThread::~TadsHttpListenerThread()
{
    /* release our queue */
    queue->release_ref();
}

/* ------------------------------------------------------------------------ */
/*
 *   HTTP Server Thread 
 */

/*
 *   Constant strings for some common HTTP status codes we report 
 */
static const char *S_http_400 = "400 Bad Request";

static const char *S_http_500 = "500 Internal Server Error";
static const char *S_http_500_body =
    "<html>"
    "<title>500 Internal Server Error</title>"
    "<body>"
    "<h1>Internal Server Error</h1>"
    "</body>"
    "</html>";

static const char *S_http_503 = "503 Service Unavailable (Shutting Down)";
static const char *S_http_503_quitting_body = 
    "<html>"
    "<title>503 Service Unavailable</title>"
    "<body>"
    "<h1>Service Unavailable</h1>"
    "The server is shutting down and cannot process any more requests."
    "</body>"
    "</html>";

/*
 *   Parse a space-delimited token.  
 */
static void parse_tok(char *&p, char *&tok, size_t &tok_len)
{
    /* skip leading spaces */
    for ( ; isspace(*p) ; ++p) ;

    /* the token starts here */
    tok = p;

    /* 
     *   find the end of the token - it's the end of the string, or the next
     *   whitespace or newline (CR or LF) character 
     */
    for ( ; *p != '\0' && !isspace(*p) && *p != '\n' && *p != '\r' ; ++p) ;

    /* note the length of the token */
    tok_len = p - tok;
}

/*
 *   HTTP server thread - Construction 
 */
TadsHttpServerThread::TadsHttpServerThread(
    TadsHttpListenerThread *l, TadsMessageQueue *q, long ulim, OS_Socket *s)
    : TadsServerThread(l, s)
{
    /* keep a reference to the message queue */
    (queue = q)->add_ref();

    /* remember the upload limit */
    upload_limit = ulim;
}

TadsHttpServerThread::~TadsHttpServerThread()
{
    /* done with the message queue */
    queue->release_ref();
}


/*
 *   Send a simple response, with the given status code and message body.  If
 *   the mesage body is null, we'll just send the response header.  Returns
 *   true if successful, false on error.  
 */
int TadsHttpServerThread::send_simple(
    const char *status_code, const char *mime_type,
    const char *msg_body, size_t msg_len,
    const char *extra_headers)
{
    char hdr[100];

    /* send the response */
    t3sprintf(hdr, sizeof(hdr), "HTTP/1.1 %s\r\n", status_code);
    if (!send(hdr, strlen(hdr)))
        return FALSE;

    /* send the extra headers */
    if (extra_headers != 0)
        send(extra_headers, strlen(extra_headers));

    /* if there's a message body, send it, with a content-length header */
    if (msg_body != 0)
    {
        char buf[200];

        /* build the cache-control and content-length header */
        t3sprintf(buf, sizeof(buf),
                  "Content-Type: %s\r\n"
                  "Content-Length: %lu\r\n"
                  "Cache-control: no-cache\r\n"
                  "Connection: Keep-Alive\r\n"
                  "\r\n",
                  mime_type, (ulong)msg_len);

        /* send the headers, followed by the message body */
        if (!send(buf, strlen(buf))
            || !send(msg_body, msg_len))
            return FALSE;
    }
    else
    {
        /* 
         *   there's no body, so just send a blank line to mark the end of
         *   the header 
         */
        if (!send("\r\n", 2))
            return FALSE;
    }

    /* success */
    return TRUE;
}

/*
 *   Read to one or two newline sequences. 
 */
int TadsHttpServerThread::read_to_nl(StringRef *dst, long ofs,
                                     int init_state, int end_state)
{
    /* newline state: 0 \r -> 1 \n -> 2 \r -> 3 \n -> 4 */
    int nlstate = init_state;

    /* keep going until we find the newline or newline pair */
    for (;;)
    {
        char buf[8192];

        /* scan up to the ending offset */
        const char *p = dst->get() + ofs;
        long endofs = dst->getlen();
        for ( ; ofs < endofs && nlstate != end_state ; ++ofs, ++p)
        {
            if (nlstate == 2 && *p == '\r')
                nlstate = 3;
            else if (*p == '\r')
                nlstate = 1;
            else if ((nlstate == 1 || nlstate == 3) && *p == '\n')
                nlstate += 1;
            else
                nlstate = 0;
        }

        /* if we're in the end state, we're done */
        if (nlstate == end_state)
            break;

        /* 
         *   We didn't find the end sequence, so we need more input.  Read at
         *   least one byte with no timeout, so that we block until
         *   something's available and then read all available bytes.  
         */
        long len = read(buf, sizeof(buf), 1, OS_FOREVER);
        set_run_state("Processing request");

        /* if we got the 'quit' signal, stop now */
        if (len < 0)
            return -1;

        /* append the text to the buffer */
        dst->append(buf, len);
    }

    /* return the buffer offset of the end of the terminating newline */
    return ofs;
}


/* 
 *   Process a request from our HTTP client.  The main server loop calls this
 *   when the socket has data ready to read.  
 */
int TadsHttpServerThread::process_request()
{
    StringRef *hdrs = new StringRef(1024);
    TadsHttpRequestHeader *hdr_list = 0, *hdr_tail = 0;
    TadsHttpRequest *req = 0;
    char *verb;
    size_t verb_len;
    char *resource_name;
    size_t res_name_len;
    int ok = FALSE;
    long ofs;
    char *p, *hbody;
    StringRef *body = 0;
    int overflow = FALSE;
    long hbodylen;
    const char *hcl, *hte;             /* content-length, transfer-encoding */

    /* 
     *   Read the header.  We read data into our buffer until we find a
     *   double CR-LF sequence, indicating the end of the header.  
     */
    if ((ofs = read_to_nl(hdrs, 0, 0, 4)) < 0)
        goto done;

    /* the body, if any, starts after the double line break */
    hbody = hdrs->get() + ofs;
    hbodylen = hdrs->getlen() - ofs;

    /* truncate the headers to the CR-LF-CR-LF sequence */
    hdrs->truncate(ofs - 2);

    /*
     *   Parse the main verb in the header - get the method and the resource
     *   ID.  The format is:
     *   
     *.     <space>* VERB <space>+ RESOURCE <space>+ HTTP-VERSION <CRLF>
     */
    p = hdrs->get();
    parse_tok(p, verb, verb_len);
    parse_tok(p, resource_name, res_name_len);

    /* now parse the remaining headers */
    TadsHttpRequestHeader::parse_headers(hdr_list, hdr_tail, FALSE, hdrs, 0);

    /* 
     *   Check to see if there's a message body.  There is if there's a
     *   content-length or transfer-encoding header.
     */
    hcl = hdr_list->find("content-length");
    hte = hdr_list->find("transfer-encoding");
    if (hcl != 0 || hte != 0)
    {
        /* 
         *   There's a content body.  If there's a content-length field,
         *   pre-allocate a chunk of memory, then read the number of bytes
         *   indicated.  If it's a chunked transfer, read it in pieces.  
         */
        if (hcl != 0)
        {
            /* get the length */
            long hclval = atol(hcl);

            /* if it's non-zero, read the content */
            if (hclval != 0)
            {
                /* if this exceeds the size limit, abort */
                if (upload_limit != 0 && hclval > upload_limit)
                {
                    /* set the overflow flag, discard the input, and abort */
                    overflow = TRUE;
                    read(0, 0, hclval, 5000);
                    goto done_with_upload;
                }

                /* allocate the buffer; it's initially empty */
                body = new StringRef(hclval);

                /* copy any portion of the body we've already read */
                if (hbodylen != 0)
                {
                    /* limit this to the declared size */
                    if (hbodylen > hclval)
                        hbodylen = hclval;

                    /* copy the data */
                    body->append(hbody, hbodylen);

                    /* deduct the remaining size */
                    hclval -= hbodylen;
                }

                /* read the body */
                if (hclval != 0
                    && read(body->getend(), hclval, hclval, 5000) < 0)
                {
                    send_simple(S_http_400, "text/html",
                                "Error receiving request message body");
                    goto done;
                }

                /* set the body length */
                body->addlen(hclval);
            }
        }
        else if (stricmp(hte, "chunked") == 0)
        {
            /* set up a string buffer for the content */
            const long initlen = 32000;
            body = new StringRef(hbodylen > initlen ? hbodylen : initlen);

            /* if we've already read some body text, copy it to the buffer */
            if (hbodylen != 0)
                body->append(hbody, hbodylen);
            
            /* keep going until we reach the end marker */
            for (ofs = 0 ; ; )
            {
                /* read to the first newline */
                long nlofs = read_to_nl(body, ofs, 0, 2);
                if (nlofs < 0)
                    goto done;

                /* get the chunk length */
                long chunklen = strtol(body->get() + ofs, 0, 16);

                /* 
                 *   We're done with the chunk length.  Move any read-ahead
                 *   content down in memory so that it directly abuts the
                 *   preceding chunk, so that when we're done we'll have the
                 *   content assembled into one contiguous piece.  
                 */
                long ralen = body->getlen() - nlofs;
                if (ralen > 0)
                    memmove(body->get() + ofs, body->get() + nlofs,
                            body->getlen() - nlofs);

                /* stop at the end of the read-ahead portion */
                body->truncate(ofs + ralen);

                /* if the chunk length is zero, we're done */
                if (chunklen == 0)
                    break;

                /* check if this would overflow our upload size limit */
                if (upload_limit != 0
                    && !overflow
                    && body->getlen() + chunklen > upload_limit)
                {
                    /* flag the overflow, but keep reading the content */
                    overflow = TRUE;
                }

                /* 
                 *   figure the remaining read size for this chunk, after any
                 *   read-ahead portion 
                 */
                long chunkrem = chunklen - ralen;

                /* 
                 *   if we've already overflowed, read and discard the chunk;
                 *   otherwise read it into our buffer 
                 */
                if (chunkrem + 2 <= 0)
                {
                    /* 
                     *   We've already read ahead by enough to cover the next
                     *   chunk and the newline at the end.  Go in and delete
                     *   the newline (or as much of it as exists).  Start by
                     *   getting the offset of the newline: it's just after
                     *   the current chunk, which starts at 'ofs' and runs
                     *   for 'chunklen'.  
                     */
                    nlofs = ofs + chunklen;

                    /* 
                     *   if we haven't yet read the full newline sequence, do
                     *   so now
                     */
                    if (ralen - chunklen < 2)
                    {
                        long nllen = 2 - (ralen - chunklen);
                        body->ensure(nllen);
                        if (read(body->getend(), nllen, nllen, 30000) < 0)
                        {
                            send_simple(S_http_400, "text/html",
                                        "Error receiving request message chunk");
                            goto done;
                        }
                    }

                    /* 
                     *   if there's anything after the newline, move it down
                     *   to overwrite the newline 
                     */
                    if (ralen - chunklen > 2)
                    {
                        /* move the bytes */
                        memmove(body->get() + nlofs,
                                body->get() + nlofs + 2,
                                ralen - chunklen - 2);

                        /* adjust the size for the closed gap */
                        body->truncate(body->getlen() - 2);
                    }

                    /* resume from after the newline */
                    ofs = nlofs;
                }
                else if (overflow)
                {
                    /* overflow - discard the chunk size (plus the CR-LF) */
                    if (read(0, 0, chunkrem+2, 30000) < 0)
                        goto done;

                    /* clear the buffer */
                    body->truncate(0);
                    ofs = 0;
                }
                else
                {
                    /* ensure the buffer is big enough */
                    body->ensure(chunkrem + 2);

                    /* read the chunk, plus the CR-LF that follows */
                    if (read(body->getend(),
                             chunkrem + 2, chunkrem + 2, 30000) < 0)
                    {
                        send_simple(S_http_400, "text/html",
                                    "Error receiving request message chunk");
                        goto done;
                    }

                    /* count the amount we just read in the buffer length */
                    body->addlen(chunkrem + 2);

                    /* verify that the last two bytes are indeed CR-LF */
                    if (memcmp(body->getend() - 2, "\r\n", 2) != 0)
                    {
                        send_simple(S_http_400, "text/html",
                                    "Error in request message chunk");
                        goto done;
                    }

                    /* 
                     *   move past the chunk for the next read, minus the
                     *   CR-LF that follows the chunk - that isn't part of
                     *   the data, but just part of the protocol 
                     */
                    ofs = body->getlen() - 2;
                    body->truncate(ofs);
                }
            }

            /* 
             *   Read to the closing blank line.  We've just passed one
             *   newline, following the '0' end length marker, so we're in
             *   state 2.  We could have either another newline following, or
             *   "trailers" (more headers, following the content body).
             */
            long nlofs = read_to_nl(body, ofs, 2, 4);
            if (nlofs < 0)
                goto done;

            /* 
             *   if we have more than just the closing blank line, we have
             *   trailers - append them to the headers 
             */
            if (nlofs > ofs + 2)
            {
                TadsHttpRequestHeader::parse_headers(
                    hdr_list, hdr_tail, TRUE, body, ofs);
            }

            /* 
             *   the trailers (if any) and closing newline aren't part of the
             *   content - clip them out of the body 
             */
            body->truncate(ofs);
        }
        else
        {
            /* 
             *   Other combinations of these headers are illegal.  Send an
             *   error and abort. 
             */
            send_simple(S_http_400, "text/html",
                        "<html><title>Bad Request</title>"
                        "<h1>Bad Request</h1>"
                        "This server does not accept the specified "
                        "transfer-encoding.");
            goto done;
        }

    done_with_upload:
        /* done with the upload - check for overflow */
        if (overflow)
        {
            /* release the body, if present */
            if (body != 0)
            {
                body->release_ref();
                body = 0;
            }
        }
    }

    /* create a message object for the request */
    req = new TadsHttpRequest(
        this, verb, verb_len, hdrs, hdr_list, body, overflow,
        resource_name, res_name_len,
        listener->get_shutdown_evt());

    /* we've handed over the header list to the request */
    hdr_list = 0;
    
    /* send it to the main thread, and wait for the reply */
    if (queue->send(req, OS_FOREVER))
    {
        /* 
         *   success - the server will already have sent the reply, so we're
         *   done 
         */
    }
    else if (queue->is_quitting())
    {
        /* failed due to server shutdown */
        send_simple(S_http_503, "text/html", S_http_503_quitting_body);
    }
    else
    {
        /* 'send' failed - return an internal server error */
        send_simple(S_http_500, "text/html", S_http_500_body);
    }
    
    /* we're done with the request */
    req->release_ref();

    /* success */
    ok = TRUE;

done:
    /* release the message body buffer */
    if (body != 0)
        body->release_ref();

    /* release the header buffer */
    hdrs->release_ref();

    /* delete the header list */
    if (hdr_list != 0)
        delete hdr_list;

    /* return the success/failure indication */
    return ok;
}

/* ------------------------------------------------------------------------ */
/*
 *   Complete an HTTP request 
 */
void TadsHttpRequest::complete()
{
    /* 
     *   if the client included a "connection" header with value "close",
     *   close the socket 
     */
    const char *conn = (hdr_list != 0 ? hdr_list->find("connection") : 0);
    if (conn != 0 && stricmp(conn, "close") == 0 && thread != 0)
        thread->close_socket();

    /* mark the message queue object as completed */
    TadsEventMessage::complete();
}

