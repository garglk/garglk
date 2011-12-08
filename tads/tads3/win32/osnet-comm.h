/* $Header$ */

/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  osnet-comm.h - Win32 Web UI communicator thread
Function
  Interprocess communication server for the Web UI
Notes
  This module defines some helper classes for interprocess communications
  between the TADS VM and the Web UI.  These two parts of the system run as
  separate processes, so they need a way to coordinate their activities.  For
  the most part, the browser control (in the Web UI) and the byte code
  program (on the VM side) communicate via HTTP to create the presentation
  and handle user input events.  However, the VM and the Web UI need to talk
  directly, at a "meta" level, to coordinate certain application-level
  activities between the two processes.  We do this via a pair of pipes that
  the VM creates and hands to the UI process via handle inheritance.  This
  module implements a message-based protocol on the pipes, and provides a
  convenient, high-level API to send and receive these messages.
Modified
  05/26/10 MJRoberts  - Creation
*/

#ifndef OSNET_COMM_H
#define OSNET_COMM_H

#include "osifcnet.h"
#include "vmnet.h"


/* ------------------------------------------------------------------------ */
/*
 *   Web UI message.  This message represents a request from this process to
 *   our peer process; we queue these to the web UI communicator thread,
 *   which sends them across the pipe and reads the replies.  
 */
class WebUIMessage: public TadsMessage
{
public:
    WebUIMessage(int serial, char *msg) : TadsMessage("WebUIMessage", 0)
    {
        /* take ownership of the command message */
        this->msg = msg;

        /* no reply yet */
        reply = 0;

        /* set our serial number */
        this->serial = serial;
    }

    ~WebUIMessage()
    {
        /* done with the command message and reply */
        lib_free_str(msg);
        lib_free_str(reply);
    }

    /* the request message */
    char *msg;

    /* the reply from the webui process */
    char *reply;

    /* the message serial number, for matching it up with the reply */
    int serial;
};

/* ------------------------------------------------------------------------ */
/* 
 *   Web UI communications thread.  This thread monitors the incoming pipe
 *   for requests from the webui to self.  
 */
class WebUICommThread: public OS_Thread
{
public:
    WebUICommThread(HANDLE proc, HANDLE pipe)
    {
        /* remember the various handles */
        this->proc = proc;
        this->pipe = pipe;

        /* create our message queue */
        queue = new TadsMessageQueue(0);

        /* create the 'quit' event */
        quit = new OS_Event(TRUE);

        /* set the initial message serial number */
        wmsg_serial = 0;

        /* nothing in the pending reply queue yet */
        msg_pending = 0;

        /* initialize the overlapped read structure for the pipe */
        memset(&pipe_ov, 0, sizeof(pipe_ov));
        pipe_ov.hEvent = CreateEvent(0, TRUE, FALSE, 0);

        /* not yet disconnecting */
        disconnecting = FALSE;
    }

    virtual ~WebUICommThread()
    {
        /* 
         *   flush the outgoing pipe to allow the client to read any pending
         *   data before we disconnect
         */
        FlushFileBuffers(pipe);
        DisconnectNamedPipe(pipe);

        /* cancel any outstanding read request */
        CancelIo(pipe);
        
        /* close our handles */
        CloseHandle(proc);
        CloseHandle(pipe);
        CloseHandle(pipe_ov.hEvent);

        /* clear and release the queue */
        queue->flush();
        queue->release_ref();

        /* release the quit event */
        quit->release_ref();
    }

    /*
     *   Disconnect 
     */
    void disconnect(int close_ui)
    {
        /* 
         *   If we've already received a disconnect request from the other
         *   side, there's no need to echo it back, and in fact it's
         *   problematic to do so, since the other side is busy waiting for
         *   us to reply to its request.  
         */
        if (disconnecting)
            return;

        /* send a "disconnect" request */
        char *reply;
        send_cmd(10000, reply, close_ui ? "close" : "disconnect");
        lib_free_str(reply);

        /* signal the 'quit' event */
        quit->signal();
    }

    /* 
     *   Process a request.  This must be overridden in each subclass to
     *   handle the specific request types for this end of the pipe.
     *   
     *   "id" is an integer giving the request ID.  This is an arbitrary
     *   serial number that we must send back to the peer with the reply;
     *   this allows the peer to match up the reply to its original request.
     *   
     *   "cmd" is the command text, as specified by the client.  The syntax
     *   is up to each subclass to define.  For convenience, this routine may
     *   modify the command buffer in place, provided it doesn't add bytes
     *   past the end of the existing string.  
     */
    virtual void process_request(int id, char *cmd)
    {
        /* check for messages we handle in the base class */
        if (strcmp(cmd, "disconnect") == 0 || strcmp(cmd, "close") == 0)
        {
            /* disconnect - note that we've received this request */
            disconnecting = TRUE;

            /* send an acknowledgment */
            send_reply(id, "ok");

            /* signal the 'quit' event to shut ourselves down */
            quit->signal();
        }
    }

    /* main thread entrypoint */
    void thread_main()
    {
        /* start the first non-blocking read on the pipe */
        start_read_pipe();
        
        /* keep going until the process terminates */
        for (int done = FALSE ; !done ; )
        {
            /* wait for the read pipe or process termination */
            HANDLE hlst[] = {
                proc,
                queue->get_event_obj()->get_handle(),
                quit->get_handle(),
                pipe_ov.hEvent
            };
            switch (WaitForMultipleObjects(4, hlst, FALSE, INFINITE))
            {
            case WAIT_OBJECT_0:
                /* the process has terminated */
                done = TRUE;
                break;

            case WAIT_OBJECT_0 + 1:
                /* there's a message in our request queue */
                process_message();
                break;

            case WAIT_OBJECT_0 + 2:
                /* quit event */
                done = TRUE;
                break;

            case WAIT_OBJECT_0 + 3:
                /* the pipe is ready - go read it */
                finish_read_pipe();

                /* schedule the next read */
                start_read_pipe();
                break;

            default:
                /* anything else is an error - abort */
                flag_error("error waiting in main monitor thread");
                done = TRUE;
                break;
            }
        }
    }

    /* flag an error */
    void flag_error(const char *msg)
    {
    }

    /*
     *   Send a command to the peer and await the reply 
     */
    int send_cmd(unsigned long timeout, char *&reply, const char *msg, ...)
    {
        /* presume there will be no reply */
        reply = 0;
        
        /* if the peer isn't running, don't bother */
        if (!peer_running())
            return FALSE;
        
        /* format the message */
        va_list args;
        va_start(args, msg);
        char *buf = t3vsprintf_alloc(msg, args);
        va_end(args);
        
        /* queue the message (the message object takes over the buffer) */
        WebUIMessage *m = new WebUIMessage(++wmsg_serial, buf);
        int ok = queue->send(m, timeout);
        
        /* check what happened */
        if (ok)
        {
            /* success - make a copy of the reply for the caller */
            reply = lib_copy_str(m->reply);
        }
        else
        {
            /* 
             *   failed - notify the comm thread that we're no longer
             *   interested in the message, so that it can release any
             *   references it has to the message object 
             */
            abandon(m);
        }
        
        /* done with the message */
        m->release_ref();
        
        /* return the status indication */
        return ok;
    }

    /*
     *   Send a reply to a request from the peer 
     */
    int send_reply(int id, const char *reply, ...)
    {
        /* if the peer isn't running, don't bother */
        if (!peer_running())
            return FALSE;

        /* format the message */
        va_list args;
        va_start(args, reply);
        char *mbuf = t3vsprintf_alloc(reply, args);
        va_end(args);

        /* format the overall message: "reply ID <text>" */
        char *buf = t3sprintf_alloc("reply %d %s", id, mbuf);
        size_t buflen = strlen(buf);

        /* write the reply to the pipe */
        DWORD actual;
        int ok = (WriteFile(pipe, buf, buflen, &actual, 0)
                  && actual == buflen);

        /* done with the formatted message buffers */
        t3free(buf);
        t3free(mbuf);

        /* return the status indication */
        return ok;
    }

    /* is the peer process running? */
    int peer_running()
    {
        /* it's running if the process handle isn't signaled yet */
        return proc != INVALID_HANDLE_VALUE
            && WaitForSingleObject(proc, 0) == WAIT_TIMEOUT;
    }

    /* process a message */
    void process_message()
    {
        WebUIMessage *uimsg;
        
        /* retrieve the message */
        TadsMessage *msg = queue->get();

        /* check the type */
        if ((uimsg = cast_tads_message(WebUIMessage, msg)) != 0)
        {
            /* build the full request: "serial command" */
            char *req = t3sprintf_alloc("%d %s", uimsg->serial, uimsg->msg);
            size_t reqlen = strlen(req);

            /* write the request to the pipe */
            DWORD actual;
            if (WriteFile(pipe, req, reqlen, &actual, FALSE)
                && actual == reqlen)
            {
                /* 
                 *   We successfully wrote the request to the pipe.  Now we
                 *   just sit back and wait for the other side to send back a
                 *   reply tagged with this message ID.  In order to find the
                 *   message object later, though, we need to add it to our
                 *   list of messages pending replies.  
                 */
                uimsg->nxt = msg_pending;
                msg_pending = uimsg;

                /* add a reference for the reply list */
                uimsg->add_ref();
            }
            else
            {
                /* 
                 *   failed to write the request - send back a failure reply
                 *   immediately 
                 */
                flag_error("error in pipe send");
                uimsg->reply = lib_copy_str("error in pipe send");
                uimsg->complete();
            }

            /* done with the request text */
            t3free(req);
        }

        /* we're done with the message */
        if (msg != 0)
            msg->release_ref();
    }

    /* schedule a read on the pipe */
    void start_read_pipe()
    {
        /* reset the pipe read event */
        ResetEvent(pipe_ov.hEvent);

        /* do the read */
        if (!ReadFile(pipe, pipe_in_buf, sizeof(pipe_in_buf)-1, 0, &pipe_ov)
            && GetLastError() != ERROR_IO_PENDING)
        {
            /* read failed - signal the quit event */
            DWORD err = GetLastError();
            flag_error("error in pipe read");
            quit->signal();
        }
    }

    /* complete a read on the pipe */
    void finish_read_pipe()
    {
        /* get the result information */
        DWORD actual;
        GetOverlappedResult(pipe, &pipe_ov, &actual, TRUE);

        /* null-terminate the message */
        char *msg = pipe_in_buf;
        msg[actual] = '\0';
        
        /* check the message syntax */
        if (memcmp(msg, "reply ", 6) == 0)
        {
            /* get the ID */
            int id = atoi(msg + 6);
            
            /* find the reply text, after the ID */
            char *p;
            for (p = msg + 6 ; *p != '\0' && !isspace(*p) ; ++p) ;
            for ( ; isspace(*p) ; ++p) ;
            
            /* it's a reply to a message we sent - process it */
            process_reply(id, p);
        }
        else
        {
            /* 
             *   It's a request message.  It has the syntax "ID command",
             *   where "ID" is the serial number of the message, which we
             *   have to send back in the reply to allow our peer to match it
             *   up with the request; and "command" is the command text for
             *   the request.  
             */
            
            /* get the ID */
            int id = atoi(msg);
            
            /* find the command, which follows a space after the ID */
            char *cmd;
            for (cmd = msg ; *cmd != '\0' && !isspace(*cmd) ; ++cmd) ;
            for ( ; isspace(*cmd) ; ++cmd) ;

            /* process the request */
            process_request(id, cmd);
        }
    }

    /* process a reply to a message we sent */
    void process_reply(int id, char *txt)
    {
        /* find the message in the pending reply list (removing it) */
        WebUIMessage *msg = remove_pending(id);

        /* if we found it, send the reply */
        if (msg != 0)
        {
            /* copy the reply text into the message */
            msg->reply = lib_copy_str(txt);

            /* signal it as completed */
            msg->complete();

            /* we're done with the message */
            msg->release_ref();
        }
    }

    /* abandon a message */
    void abandon(TadsMessage *msg)
    {
        /* remove the message from the reply wait list */
        WebUIMessage *uimsg = cast_tads_message(WebUIMessage, msg);
        if (uimsg != 0)
        {
            /* remove it from the pending list */
            if (remove_pending(uimsg->serial) != 0)
            {
                /* it was in the list, so remove the list's reference */
                uimsg->release_ref();
            }
        }

        /* remove the message from the queue as well */
        queue->abandon(msg);
    }

    /* remove a message from the pending reply list */
    WebUIMessage *remove_pending(int id)
    {
        /* find the message in our pending list */
        WebUIMessage *msg, *prv;
        for (prv = 0, msg = msg_pending ; msg != 0 ;
             prv = msg, msg = (WebUIMessage *)msg->nxt)
        {
            /* check the ID */
            if (msg->serial == id)
            {
                /* found it - unlink it from the pending list */
                if (prv != 0)
                    prv->nxt = msg->nxt;
                else
                    msg_pending = (WebUIMessage *)msg->nxt;

                /* it's no longer in any list */
                msg->nxt = 0;

                /* return it */
                return msg;
            }
        }

        /* didn't find it */
        return 0;
    }

    /* webui process handle */
    HANDLE proc;

    /* pipe for communicating with peer */
    HANDLE pipe;

    /* pipe overlapped read structure */
    OVERLAPPED pipe_ov;

    /* input pipe read buffer */
    char pipe_in_buf[4096];
    
    /* self->webui request message queue */
    TadsMessageQueue *queue;

    /* list of messages pending replies */
    WebUIMessage *msg_pending;

    /* 'quit' event */
    OS_Event *quit;

    /* WebUIMessage serial number */
    int wmsg_serial;

    /* flag: we've received a disconnect request from our peer */
    int disconnecting;
};

#endif /* OSNET_COMM_H */

