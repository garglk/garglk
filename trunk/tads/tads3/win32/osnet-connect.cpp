#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  osnet-connect.cpp - Win32 command-line UI connection
Function
  Launches a local Web browser to connect to the Web UI
Notes
  
Modified
  05/12/10 MJRoberts  - Creation
*/

#include <string.h>
#include <stdlib.h>
#include "t3std.h"
#include "os.h"
#include "osifcnet.h"
#include "vmnet.h"
#include "vmvsn.h"
#include "vmglob.h"
#include "vmimage.h"
#include "osnet-comm.h"


/* ------------------------------------------------------------------------ */
/* 
 *   The Web UI communicator thread - specialization for the VM side of the
 *   channel.  
 */
class WebUICommThreadVM: public WebUICommThread
{
public:
    WebUICommThreadVM(TadsMessageQueue *mq, HANDLE proc, HANDLE pipe)
        : WebUICommThread(proc, pipe)
    {
        /* remember the message queue object */
        (msg_queue = mq)->add_ref();
    }

    ~WebUICommThreadVM()
    {
        /* release the message queue */
        msg_queue->release_ref();
    }

    virtual void process_request(int id, char *cmd)
    {
        /* check for close/disconnect messages */
        if (memcmp(cmd, "disconnect", 10) == 0
            || memcmp(cmd, "close", 5) == 0)
        {
            /* post a UI Close event */
            if (msg_queue != 0)
                msg_queue->post(new TadsUICloseEvent(0));
        }

        /* inherit the base class handling */
        WebUICommThread::process_request(id, cmd);
    }

    /* the application-wide network message queue */
    TadsMessageQueue *msg_queue;
};

static WebUICommThreadVM *comm_thread = 0;


/* ------------------------------------------------------------------------ */
/*
 *   Launch a local "tadsweb" customized browser as a separate process, and
 *   navigate it to the given start page on our internal HTTP server.  This
 *   handles the Web UI connection when we're running in the local
 *   stand-alone configuration, where the user launches the game from the
 *   Windows desktop or command line.  
 */
static int launch_tadsweb(VMG_ const char *addr, int port, const char *path,
                          char **errmsg)
{
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    char dir[OSFNMAX], exe[OSFNMAX];
    char *rootname;
    int ok;
    HANDLE pipe = INVALID_HANDLE_VALUE;
    char pipe_name[128];
    char cmdline[1024];

    /* presume failure, but we don't have an error message yet */
    ok = FALSE;
    *errmsg = 0;

    /* initialize the process descriptor structures */
    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);

    /* if the URL path starts with a '/', skip it */
    if (path != 0 && *path == '/')
        ++path;

    /* 
     *   if there's already a process running, try connecting the current
     *   process to the new path 
     */
    if (comm_thread != 0)
    {
        /* try sending a 'connect' message */
        char *reply;
        ok = (comm_thread->send_cmd(
                2500, reply, "connect http://%s:%d/%s", addr, port, path)
              && strcmp(reply, "ok") == 0);

        /* free the reply message */
        lib_free_str(reply);

        /* check the result */
        if (ok)
        {
            /* success - we're now showing the new UI */
            return TRUE;
        }
        else
        {
            /* 
             *   the re-connect attempt failed; shut down the old UI and
             *   proceed to start a new one 
             */
            osnet_disconnect_webui(TRUE);
        }
    }

    /* get the current executable directory */
    GetModuleFileName(0, dir, sizeof(dir));

    /* 
     *   build the full path to the tadsweb executable, looking in the same
     *   directory that contains the current process's executable 
     */
    rootname = os_get_root_name(dir);
    *(rootname != 0 ? rootname : dir) = '\0';
    os_build_full_path(exe, sizeof(exe), dir, "tadsweb.exe");

    /* open a named pipe for communication with the server */
    sprintf(pipe_name, "\\\\.\\pipe\\tadsweb.%lx",
            (long)GetCurrentProcessId());
    pipe = CreateNamedPipe(
        pipe_name,
        PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE
        | FILE_FLAG_OVERLAPPED | FILE_FLAG_WRITE_THROUGH,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
        1, 4096, 4096, 0, 0);

    /* check to make sure we created the pipe */
    if (pipe == INVALID_HANDLE_VALUE)
        goto done;

    /* set up the command line */
    _snprintf(cmdline, sizeof(cmdline),
              " url=http://%s:%d/%s pipe=%s ppid=%ld",
              addr, port, path, pipe_name, (long)GetCurrentProcessId());

    /* try launching the process */
    if (CreateProcess(exe, cmdline, 0, 0, TRUE,
                      0, 0, 0, &si, &pi))
    {
        char *reply = 0;
        
        /* 
         *   success 
         */

        /* set the success status */
        ok = TRUE;

        /* we don't need the webui's main thread handle */
        CloseHandle(pi.hThread);

        /* if there's a previous communicator thread, forget it */
        if (comm_thread != 0)
            comm_thread->release_ref();

        /* create a new communicator thread */
        comm_thread = new WebUICommThreadVM(G_net_queue, pi.hProcess, pipe);

        /* set up an overlapped I/O wait for the client to connect */
        OVERLAPPED ov;
        memset(&ov, 0, sizeof(ov));
        ov.hEvent = CreateEvent(0, TRUE, FALSE, 0);
        if (ConnectNamedPipe(pipe, &ov)
            || GetLastError() == ERROR_PIPE_CONNECTED)
        {
            /* success/already connected */
        }
        else if (GetLastError() == ERROR_IO_PENDING)
        {
            /* wait for that to complete, but not too long */
            ok = (WaitForSingleObject(ov.hEvent, 30000) == WAIT_OBJECT_0);
        }
        else
        {
            /* failed */
            ok = FALSE;
        }

        /* done with the overlapped I/O event */
        CloseHandle(ov.hEvent);

        /* if the client didn't start up, abort */
        if (!ok)
        {
            *errmsg = lib_copy_str("Timed out waiting for client to connect");
            goto done;
        }

        /* launch the thread */
        if (!comm_thread->launch())
        {
            /* couldn't launch the thread, so release the thread object */
            comm_thread->release_ref();
            comm_thread = 0;

            /* flag the error and abort */
            ok = FALSE;
            *errmsg = lib_copy_str("Unable to launch monitor thread");
            goto done;
        }

        /* send it the TADS version string */
        if (comm_thread->send_cmd(500, reply, "tadsver %s", T3VM_VSN_STRING))
            lib_free_str(reply);

        /* send the game directory */
        if (comm_thread->send_cmd(500, reply,
                                  "gamedir %s", G_image_loader->get_path()))
            lib_free_str(reply);

        /* send the default saved game extension, if any */
        const char *ext = os_get_save_ext();
        if (ext != 0 && comm_thread->send_cmd(500, reply, "saveext %s", ext))
            lib_free_str(reply);
    }
    else
    {
        /* failed */
        *errmsg = lib_copy_str("Unable to open game window");
    }

done:
    /* if an error occurred, clean up partial resources */
    if (!ok)
    {
        /* if we opened the pipe, close it */
        if (pipe != INVALID_HANDLE_VALUE)
            CloseHandle(pipe);

        /* if we created a process, close the handle */
        if (pi.hProcess != 0)
            CloseHandle(pi.hProcess);
    }

    /* return the status indication */
    return ok;
}


/*
 *   Connect to the client UI.  A Web-based game calls this after starting
 *   its internal HTTP server, to send instructions back to the client on how
 *   the client UI can connect to the game.
 *   
 *   There are two very different configurations we can run in:
 *   
 *   - Local, stand-alone mode.  This is the "old fashioned" configuration
 *   where everything's installed on the local PC: the user has a copy of the
 *   TADS Player Kit installed, and has a downloaded copy of the game on the
 *   local hard disk.  In this mode, everything should behave as much as
 *   possible like the conventional Windows version of TADS.  To launch the
 *   game, the user double-clicks the game file on the Windows desktop, or
 *   enters a t3run command line.  There's no browser involved in the launch
 *   process, so to connect to the UI, we actually have to create the UI.  In
 *   principle, this *could* simply launch a regular Web browser, but that's
 *   not what our implementation does.  Instead, to provide a more integrated
 *   appearance to the UI, we launch a special TADS window that's basically
 *   an IE control wrapped in a custom app frame.  This lets us decorate the
 *   app frame so that it looks roughly like a traditional HTML TADS window.
 *   
 *   - Client/server mode.  In this mode, the local machine is set up with
 *   t3run plus a conventional Web server (e.g., Apache), with the TADS web
 *   launch scripts installed.  The user is on a remote machine.  The user
 *   opens a Web browser and connects to our Apache server by requesting (via
 *   HTTP) our launch page.  The launch page is typically our t3launch.php
 *   script.  That page executes t3run (that's us) as a subprocess.  Before
 *   t3launch.php sends a reply back to the client, it waits to hear from the
 *   t3run subprocess (that's us again): we have to send the game connection
 *   information, so that t3launch.php can relay the information back to the
 *   client as its reply to the launch page request.  The php page sends the
 *   address back as an HTTP 301 redirect, so the client automatically
 *   navigates to the game's start page.
 *   
 *   We can tell which mode we're running in by checking for a "hostname"
 *   variable in the net config object.  If there's no hostname variable
 *   setting, we're in local mode.  If there's a hostname setting, we're in
 *   client/server mode, and the hostname gives the address of the network
 *   adapter our HTTP listener binds to.  
 */
int osnet_connect_webui(VMG_ const char *addr, int port, const char *path,
                        char **errmsg)
{
    /* get the host name from the network configuration, if any */
    const char *hostname = (G_net_config != 0
                            ? G_net_config->get("hostname") : 0);
    
    /*
     *   If we have host name in the network configuration, we're running in
     *   Web server mode - the host name parameter means that we were
     *   launched by a web server in response to an incoming launch request
     *   from a remote client.  In this mode, we need to send the connection
     *   information back to the web server's launcher, which is our parent
     *   process; we send the information back via stdout.
     *   
     *   If we don't have a host name, it means that the user must have
     *   launched the interpreter from the Windows desktop or command shell,
     *   in which case they want to run the game as a local, stand-alone
     *   application.  In local mode, we have to display our own user
     *   interface.  
     */
    if (hostname != 0)
    {
        /* 
         *   Web server mode.  We were launched by the local Web server in
         *   response to a new game request from a Web client.  Our parent
         *   process is the Web server running the php launch page.  The php
         *   page has a pipe connection to our stdout.  Send the start page
         *   information back to the php page simply by writing the
         *   information to stdout.  
         */
        printf("connectWebUI:http://%s:%d%s\n", addr, port, path);
        fflush(stdout);

#if 0
        // $$$ for testing only
        if (strcmp(hostname, "localhost") == 0)
        {
            char buf[4096];
            _snprintf(buf, sizeof(buf), "http://%s:%d%s\n", addr, port, path);
            ShellExecute(0, "open", buf, 0, 0, SW_SHOWNORMAL);
        }
#endif

        /* success */
        *errmsg = 0;
        return TRUE;
    }
    else
    {
        /*
         *   Local stand-alone mode.  In this mode, we have to launch our own
         *   user interface, since the user directly ran the TADS interpreter
         *   from the Windows desktop or command shell, and so far we haven't
         *   presented any UI.  Our UI in this mode is the "tadsweb" custom
         *   browser.  This is similar to an ordinary browser, but it has a
         *   customized frame (menus, toolbar, etc) that makes it look more
         *   like a regular HTML TADS window, and it has a special control
         *   channel (via a pipe) that tightly couples it to the intepreter.
         */
        return launch_tadsweb(vmg_ addr, port, path, errmsg);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Disconnect from the web UI, in preparation for program termination 
 */
void osnet_disconnect_webui(int close)
{
    /* if the web ui process is running, send it a shutdown message */
    if (comm_thread != 0)
    {
        /* send a 'shutdown' message */
        comm_thread->disconnect(close);

        /* wait for the comm thread to finish (but not forever) */
        comm_thread->wait(15000);

        /* we're done with the thread */
        comm_thread->release_ref();
        comm_thread = 0;
    }

    /* unload dynamically linked libraries */
    tads_unlink_user32();
}

/* ------------------------------------------------------------------------ */
/*
 *   Negotiate foreground status for the Web UI 
 */

/* ask the Web UI to yield the foreground to the current process */
void osnet_webui_yield_foreground()
{
    if (comm_thread != 0)
    {
        /* send the request to yield the foreground to the current PID */
        char *reply;
        if (comm_thread->send_cmd(2500, reply,
                                  "yield-fg %ld", GetCurrentProcessId()))
            lib_free_str(reply);
    }
}

/* bring the Web UI to the foreground */
void osnet_webui_to_foreground()
{
    if (comm_thread != 0)
    {
        /* explicitly yield the foreground to the webui process */
        tads_AllowSetForegroundWindow(GetProcessId(comm_thread->proc));

        /* tell the web ui to set itself as the foreground window */
        char *reply;
        if (comm_thread->send_cmd(1000, reply, "to-fg"))
            lib_free_str(reply);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Quote the askfile prompt - escape commas and percent signs with '%'
 */
static size_t quote_askfile_prompt(char *dst, const char *src)
{
    size_t len;
    for (len = 0 ; *src != '\0' ; ++src)
    {
        switch (*src)
        {
        case ',':
        case '%':
            /* we need to quote these */
            len += 2;
            if (dst != 0)
            {
                *dst++ = '%';
                *dst++ = *src;
            }
            break;

        default:
            /* everything else goes as is */
            len += 1;
            if (dst != 0)
                *dst++ = *src;
            break;
        }
    }

    /* null-terminate the string */
    if (dst != 0)
        *dst = '\0';

    /* return the length */
    return len;
}


/* ------------------------------------------------------------------------ */
/*
 *   Run the askfile dialog in the Web UI process.  This sends a pipe request
 *   to the Web UI process asking it to show the file selector, and returns
 *   the result.  
 */
int osnet_askfile(const char *prompt, char *fname_buf, int fname_buf_len,
                  int prompt_type, int file_type)
{
    /* presume failure */
    int ret = 1;

    if (comm_thread != 0)
    {
        /* allocate space for the quoted prompt, and quote it */
        size_t qplen = quote_askfile_prompt(0, prompt);
        char *qprompt = lib_alloc_str(qplen);
        quote_askfile_prompt(qprompt, prompt);
        
        /* send the request */
        char *reply;
        if (comm_thread->send_cmd(
            OS_FOREVER, reply,
            "askfile %s,%d,%d", qprompt, prompt_type, file_type))
        {
            /* 
             *   Got a reply.  The return code is the first character of the
             *   buffer, as a digit giving the OS_AFE_xxx value.  
             */
            ret = reply[0] - '0';

            /* the rest is the filename string */
            lib_strcpy(fname_buf, fname_buf_len, reply + 1);

            /* free the reply */
            lib_free_str(reply);
        }
        
        /* delete the allocated buffer */
        lib_free_str(qprompt);
    }

    /* return the result */
    return ret;
}


