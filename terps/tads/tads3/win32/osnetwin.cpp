/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  osnetwin.cpp - TADS networking and threading package - windows version
Function
  
Notes
  
Modified
  04/10/10 MJRoberts  - Creation
*/


#include "t3std.h"
#include "os.h"
#include "osifcnet.h"
#include "vmnet.h"
#include "vmisaac.h"
#include "vmfile.h"


/* ------------------------------------------------------------------------ */
/*
 *   Globals 
 */

/* master thread list */
TadsThreadList *G_thread_list = 0;



/* ------------------------------------------------------------------------ */
/*
 *   Package initialization and termination 
 */

/* have we initialized the WinSock layer yet? */
static int winsock_inited = FALSE;

/*
 *   package initialization 
 */
void os_net_init(TadsNetConfig *)
{
    /* if we haven't initialized winsock yet, do so now */
    if (!winsock_inited)
    {
        /* initialize the winsock library */
        WSADATA wsa_data;
        if (WSAStartup(MAKEWORD(2, 2), &wsa_data))
            winsock_inited = TRUE;
    }

    /* create the master thread list */
    if (G_thread_list == 0)
        G_thread_list = new TadsThreadList();
}

/*
 *   package cleanup 
 */
void os_net_cleanup()
{
    /* if we have a master thread list, wait for threads to exit */
    if (G_thread_list != 0)
    {
        /* wait for threads to exit */
        G_thread_list->wait_all(500);

        /* done with the thread list */
        G_thread_list->release_ref();
        G_thread_list = 0;
    }

    /* close winsock if we initialized it */
    if (winsock_inited)
    {
        /* shut down WinSock */
        WSACleanup();
        
        /* flag it */
        winsock_inited = FALSE;
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Local host information 
 */

int os_get_hostname(char *buf, size_t buflen)
{
    return !gethostname(buf, buflen);
}

int os_get_local_ip(char *buf, size_t buflen, const char *host)
{
    /* 
     *   Get the local host information.  Note that 'host' can be null, which
     *   specifies that we should get the default IP address; it so happens
     *   that gethostbyname() will get the default host info if 'host' is
     *   null, so this does just what we want.  
     */
    struct hostent *local_host = gethostbyname(host);
    lib_strcpy(buf, buflen,
               inet_ntoa(*(struct in_addr *)*local_host->h_addr_list));

    /* success */
    return TRUE;
}

/* ------------------------------------------------------------------------ *
/*
 *   Waitable objects
 */
int OS_Waitable::multi_wait(int cnt, OS_Waitable **objs,
                            unsigned long timeout)
{
    /* set up an array of event handles */
    HANDLE *handles = new HANDLE[cnt + 1];
    
    /* copy the handles */
    for (int i = 0 ; i < cnt ; ++i)
        handles[i] = objs[i]->h;
    
    /* figure the ending time */
    DWORD t_end = GetTickCount() + timeout;
    
    /* loop until we get a signaled event handle or error */
    int ret;
    for (;;)
    {
        /* figure the current timeout as the delta to the ending time */
        if (timeout != OS_FOREVER)
        {
            /* if the timeout has already expired, stop now */
            DWORD t_now = GetTickCount();
            if (t_now > t_end)
            {
                ret = OSWAIT_TIMEOUT;
                break;
            }
            
            /* figure the interval remaining */
            timeout = t_end - t_now;
        }
        
        /* do the wait */
        DWORD result = MsgWaitForMultipleObjects(
            cnt, handles, FALSE, timeout, QS_ALLEVENTS);
        
        /* interpret the result */
        if (result == WAIT_OBJECT_0 + cnt)
        {
            /* 
             *   we have messages in the message queue - process them, then
             *   continue looping to wait for one of the caller's events 
             */
            process_messages();
        }
        else if (result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + cnt)
        {
            /* one of our handles is in the signaled state */
            ret = OSWAIT_EVENT + (result - WAIT_OBJECT_0);
            break;
        }
        else if (result == WAIT_TIMEOUT)
        {
            /* timed out */
            ret = OSWAIT_TIMEOUT;
            break;
        }
        else
        {
            /* anything else is an error */
            ret = OSWAIT_ERROR;
            break;
        }
    }
    
    /* done with the handle array */
    delete [] handles;
    
    /* return the result */
    return ret;
}


/* ------------------------------------------------------------------------ */
/*
 *   Threads 
 */

OS_Thread::OS_Thread()
{
    /* add myself to the master thread list */
    if (G_thread_list != 0)
        G_thread_list->add(this);

    /* we haven't failed to launch yet */
    failed = FALSE;
}

OS_Thread::~OS_Thread()
{
    /* clean up the master thread list for our deletion */
    if (G_thread_list != 0)
        G_thread_list->clean();
}
