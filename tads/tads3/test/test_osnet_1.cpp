#include <stdio.h>
#include <stdlib.h>

#include "t3std.h"
#include "os.h"
#include "osifc.h"
#include "osifcnet.h"
#include "vmnet.h"
#include "vmrun.h"
#include "vmglob.h"

/* our set of events */
static OS_Event *events[5];
static OS_Event *quit_event;

/* test thread */
class TestThread: public OS_Thread
{
public:
    TestThread(int n, unsigned long timeout)
    {
        /* remember our statistics */
        this->n = n;
        this->timeout = timeout;

        /* add a reference to each event we use */
        quit_event->add_ref();
        for (int i = 0 ; i < 3 ; ++i)
            events[i+n]->add_ref();
    }

    ~TestThread()
    {
        /* release our event references */
        quit_event->release_ref();
        for (int i = 0 ; i < 3 ; ++i)
            events[i+n]->release_ref();
    }
    
    int n;
    unsigned long timeout;
    
    void thread_main()
    {
        OS_Waitable *w[4];

        /* 
         *   set up our list of events - we wait for ABC + our thread index
         *   (so thread 0 waits for ABC, 1 for BCD, 2 for CDE) 
         */
        for (int i = 0 ; i < 3 ; ++i)
            w[i] = events[i+n];

        /* we also wait for the 'quit' event */
        w[3] = quit_event;

        printf("t%d: entering\n", n);
        for (;;)
        {
            printf("t%d: waiting for %c, %c, %c\n", n,
                   'A' + n, 'B' + n, 'C' + n);
            int ret = OS_Waitable::multi_wait(4, w, timeout);
            if (ret == OSWAIT_TIMEOUT)
                printf("t%d: timeout\n", n);
            else if (ret == OSWAIT_ERROR)
                printf("t%d: error\n", n);
            else if (ret >= OSWAIT_EVENT && ret <= OSWAIT_EVENT + 2)
                printf("t%d: got event %c\n", n, 'A' + n + ret - OSWAIT_EVENT);
            else if (ret == OSWAIT_EVENT + 3)
            {
                printf("t%d: got quit event\n", n);
                break;
            }
            else
                printf("t%d: unknown wait status %d\n", n, ret);
        }

        /* done */
        printf("t2: exiting\n");
    }
};

/* socket handler thread */
class SocketThread: public OS_Thread
{
public:
    SocketThread(OS_Socket *s)
    {
        /* keep a reference on the socket */
        this->s = s;
        s->add_ref();
    }

    ~SocketThread()
    {
        /* release our socket reference */
        s->close();
        s->release_ref();
    }

    OS_Socket *s;

    void thread_main()
    {
        printf("socket handler %lx: entering\n", (long)s);

        /* read the socket in non-blocking mode */
        s->set_non_blocking();

        /* wait for the socket and the 'quit' event */
        OS_Waitable *w[2];
        w[0] = quit_event;
        w[1] = s;

        /* read from the socket until it closes */
        for (int done = FALSE ; !done ; )
        {
            /* wait for the next event */
            int ret = OS_Waitable::multi_wait(2, w);
            switch (ret)
            {
            case OSWAIT_EVENT:
                /* quit event */
                printf("socket handler %lx: got quit event\n", (long)s);
                done = TRUE;
                break;

            case OSWAIT_EVENT + 1:
                /* socket event - reset the event */
                s->reset_event();

                /* read the socket until we run out incoming data */
                for (;;)
                {
                    char buf[128];
                    int len = s->recv(buf, sizeof(buf));
                    if (len == OS_SOCKET_ERROR)
                    {
                        if (s->last_error() == OS_EWOULDBLOCK)
                        {
                            printf("socket handler %lx: WOULDBLOCK\n",
                                   (long)s);
                        }
                        else
                        {
                            printf("socket handler %lx: recv error %d\n",
                                   (long)s, s->last_error());
                            done = TRUE;
                        }
                        break;
                    }
                    else if (len == 0)
                    {
                        printf("socket handler %lx: connection closed\n",
                               (long)s);
                        done = TRUE;
                        break;
                    }

                    printf("socket handler %lx: recv bytes [%.*s]\n",
                           (long)s, len, buf);
                }
                break;

            default:
                printf("socket handler %lx: error in wait: %d\n",
                       (long)s, ret);
                done = TRUE;
                break;
            }
        }
        
        printf("socket handler %lx: exiting\n", (long)s);
    }
};

/* listener thread */
class ListenerThread: public OS_Thread
{
public:
    ListenerThread(const char *hostname)
    {
        this->hostname = hostname;
        quit_event->add_ref();
    }

    ~ListenerThread()
    {
        quit_event->release_ref();
    }

    const char *hostname;
    
    void thread_main()
    {
        printf("listener: entering\n");

        /* set up our listener */
        OS_Listener *l = new OS_Listener();
        if (l->open(hostname))
        {
            /* show our port information */
            char *ip;
            int port;
            if (l->get_local_addr(ip, port))
            {
                printf("listener: listening on %s:%d\n", ip, port);
                lib_free_str(ip);
            }
            else
                printf("listener: unable to get local address information\n");

            /* set non-blocking mode */
            l->set_non_blocking();

            /* wait for connections; stop on the 'quit' signal */
            OS_Waitable *w[2];
            w[0] = quit_event;
            w[1] = l;

            /* keep going until we get the 'quit' signal */
            for (;;)
            {
                /* wait for an incoming connection */
                int ret = OS_Waitable::multi_wait(2, w);
                if (ret == OSWAIT_EVENT)
                {
                    /* quit event */
                    printf("listener: got quit event\n");
                    break;
                }
                else if (ret == OSWAIT_EVENT + 1)
                {
                    /* a request is ready */
                    printf("listener: port is ready\n");

                    /* reset the listener event */
                    l->reset_event();

                    /* check for a request */
                    OS_Socket *s = l->accept();
                    if (s == 0)
                    {
                        /* no request - check for a WOULDBLOCK condition */
                        if (l->last_error() == OS_EWOULDBLOCK)
                        {
                            /* no request is ready - keep waiting */
                            printf("listener: EWOULDBLOCK\n");
                            continue;
                        }

                        /* for any other error, abort */
                        printf("listener: error %d in accept()\n",
                               l->last_error());
                        break;
                    }

                    /* got a request */
                    printf("listener: got a connection request\n");

                    /* launch a thread to read the socket */
                    OS_Thread *t = new SocketThread(s);
                    if (!t->launch())
                        printf("listener: failed to launch socket thread\n");

                    /* done with the thread and socket */
                    t->release_ref();
                    s->release_ref();
                }
                else
                {
                    printf("listener: error in multi_wait: %d\n", ret);
                    break;
                }
            }

            /* done with the listener port */
            l->close();
            l->release_ref();
        }
        else
            printf("listener: listen() failed\n");

        /* done */
        printf("listener: exiting\n");
    }
};

/* dummy entrypoint for HTTPRequest intrinsic class construction */
vm_obj_id_t TadsHttpRequest::prep_event_obj(VMG_ int *, int *)
{
    return VM_INVALID_OBJ;
}

/* dummy entrypoints for exception throwing from vmnet.cpp */
void CVmRun::push_string(VMG_ const char *, size_t) { }
void CVmRun::push_stringf(VMG_ const char *, ...) { }
void CVmRun::throw_new_rtesub(VMG_ vm_obj_id_t, uint, int) { }

/* main program entrypoint */
int main(int argc, char **argv)
{
    OS_Thread *t[4];
    int i;
    static unsigned long timeouts[] = { 10000, 27000, 71000 };
    const char *hostname = "localhost";

    /* show usage */
    printf("usage: osnettest1 <hostname>\n"
           "hostname defaults to 'localhost' if '-' or missing\n");

    /* get the listener host name, if present */
    if (argc >= 2)
    {
        hostname = argv[1];
        ++argv;
        --argc;

        if (strcmp(hostname, "-") == 0)
            hostname = "localhost";
    }

    /* initialize the network/thread package */
    os_net_init(0);

    /* create the events */
    printf("main: creating events\n");
    for (i = 0 ; i < 5 ; ++i)
        events[i] = new OS_Event(FALSE);

    /* 
     *   create the 'quit' event - this is manual-reset, because once it's
     *   signaled, we want all threads to see the signal and quit 
     */
    quit_event = new OS_Event(TRUE);

    /* launch the threads */
    printf("main: launching threads\n");
    for (i = 0 ; i < 3 ; ++i)
    {
        t[i] = new TestThread(i, timeouts[i]);
        if (!t[i]->launch())
            printf("thread %i failed to launch\n" ,i);
    }

    /* launch the listener thread */
    t[i] = new ListenerThread(hostname);
    if (!t[i]->launch())
        printf("listener thread failed to launch\n");
    ++i;

    /* show instructions */
    printf("Enter one or more events to fire, A-E.  For example, to fire\n"
           "A and B, type AB.\n"
           "In addition, Q is the 'Quit' event.\n\n");

    /* process input */
    while (!quit_event->test())
    {
        char buf[128];
        if (fgets(buf, sizeof(buf), stdin) == 0)
            break;

        for (char *p = buf ; *p != '\0' ; ++p)
        {
            if (*p >= 'A' && *p <= 'E')
                events[*p - 'A']->signal();
            if (*p >= 'a' && *p <= 'e')
                events[*p - 'a']->signal();

            if (*p == 'Q' || *p == 'q')
                quit_event->signal();
        }
    }

    /* release our events */
    quit_event->release_ref();
    for (i = 0 ; i < 5 ; ++i)
        events[i]->release_ref();

    /* wait for threads to exit */
    for (i = 0 ; i < 4 ; ++i)
    {
        printf("main: waiting for thread %d to exit...\n", i);
        t[i]->wait();
        t[i]->release_ref();
        printf("main: thread %d exited!\n", i);
    }

    /* clean up the network/thread package */
    os_net_cleanup();
}

