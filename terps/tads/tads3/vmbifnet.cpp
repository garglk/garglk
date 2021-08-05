#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 2005 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmbifnet.cpp - TADS networking function set
Function
  
Notes
  
Modified
  04/20/2010 MJRoberts  - Creation
*/

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "t3std.h"
#include "os.h"
#include "utf8.h"
#include "charmap.h"
#include "vmbifnet.h"
#include "vmtype.h"
#include "vmstack.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmglob.h"
#include "vmpool.h"
#include "vmobj.h"
#include "vmstr.h"
#include "vmlst.h"
#include "vmrun.h"
#include "vmhost.h"
#include "vmconsol.h"
#include "vmvec.h"
#include "vmnet.h"
#include "vmimport.h"
#include "vmpredef.h"
#include "vmhttpsrv.h"
#include "vmdatasrc.h"
#include "vmbytarr.h"
#include "vmfilobj.h"
#include "vmcset.h"
#include "vmlookup.h"
#include "vmtobj.h"
#include "vmlog.h"


/* ------------------------------------------------------------------------ */
/*
 *   NetEvent.evType values 
 */
#define VMBN_EVT_REQUEST    1
#define VMBN_EVT_TIMEOUT    2
#define VMBN_EVT_DBGBRK     3


/* ------------------------------------------------------------------------ */
/*
 *   Static initialization 
 */
void CVmBifNet::attach(VMG0_)
{
    /* initialize the network layer */
    os_net_init(G_net_config);

    /* create the network message queue */
    G_net_queue = new TadsMessageQueue(0);
}

/*
 *   Static cleanup 
 */
void CVmBifNet::detach(VMG0_)
{
    /* done with the message queue - flush it and release our reference */
    G_net_queue->flush();
    G_net_queue->release_ref();
    G_net_queue = 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Connect to the Web UI
 */
void CVmBifNet::connect_ui(VMG_ uint oargc)
{
    /* check arguments */
    check_argc(vmg_ oargc, 2);

    /* get the server object */
    vm_val_t *srv = G_stk->get(0);
    if (srv->typ != VM_OBJ
        || !CVmObjHTTPServer::is_httpsrv_obj(vmg_ srv->val.obj))
        err_throw(VMERR_BAD_TYPE_BIF);

    /* get the object pointer properly cast */
    CVmObjHTTPServer *srvp = (CVmObjHTTPServer *)vm_objp(vmg_ srv->val.obj);

    /* get the URL path */
    const char *path = G_stk->get(1)->get_as_string(vmg0_);
    if (path == 0)
        err_throw(VMERR_BAD_TYPE_BIF);

    /* make a null-terminated copy of the path */
    char *pathb = lib_copy_str(path + VMB_LEN, vmb_get_len(path));

    /* get the server network address information */
    char *errmsg = 0;
    char *addr = 0, *ip = 0;
    int port;
    int ok = srvp->get_listener_addr(addr, ip, port);

    /* 
     *   If we don't have a network configuration yet, create one.  This
     *   notifies other subsystems that we have an active web UI; for
     *   example, the presence of a network UI disables the regular console
     *   UI, since all UI actions have to go through the web UI once it's
     *   established.
     *   
     *   The interpreter startup creates a network configuration if it
     *   receives command-line information telling it that the game was
     *   launched by a Web server in response to an incoming client request.
     *   When the user launches the game in stand-alone mode directly from
     *   the operating system shell, there's no Web server launch
     *   information, so the startup code doesn't create a net config object.
     *   So, if we get here and find we don't have this object, it means that
     *   we're running in local standalone mode.  
     */
    if (G_net_config == 0)
        G_net_config = new TadsNetConfig();

    /* connect */
    if (ok)
    {
        /* connect */
        ok = osnet_connect_webui(vmg_ addr, port, pathb, &errmsg);
    }
    else
    {
        /* couldn't get the server address */
        errmsg = lib_copy_str(
            "No address information available for HTTPServer");
    }

    /* free strings */
    lib_free_str(pathb);
    lib_free_str(addr);
    lib_free_str(ip);

    /* if there's an error, throw it */
    if (!ok)
    {
        G_interpreter->push_string(vmg_ errmsg);
        lib_free_str(errmsg);
        G_interpreter->throw_new_class(vmg_ G_predef->net_exception, 1,
                                       "Error connecting to Web UI");
    }

    /* no return value */
    retval_nil(vmg0_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Get a network event
 */
void CVmBifNet::get_event(VMG_ uint oargc)
{
    /* check arguments */
    check_argc_range(vmg_ oargc, 0, 1);

    /* get the timeout, if present */
    long timeout = OS_FOREVER;
    if (oargc >= 1)
    {
        if (G_stk->get(0)->typ == VM_NIL)
            G_stk->discard();
        else
            timeout = pop_int_val(vmg0_);
    }

    /* we don't know the NetEvent subclass yet, so presume the base class */
    vm_obj_id_t ev_cl = G_predef->net_event;
    int argc;

    /* get the next message */
    TadsMessage *msg = 0;
    int wret = G_net_queue->wait(vmg_ timeout, &msg);

    /* cast the message */
    TadsEventMessage *evtmsg = cast_tads_message(TadsEventMessage, msg);

    /* make sure we release the message before exiting */
    err_try
    {
        /* check the wait result */
        const char *err;
        switch (wret)
        {
        case OSWAIT_EVENT + 0:
            /* we got a message */
            if (evtmsg != 0)
            {
                /* ask the message to set up a new NetRequestEvent */
                int evt_code;
                ev_cl = evtmsg->prep_event_obj(vmg_ &argc, &evt_code);
            
                /* add the event type argument */
                G_interpreter->push_int(vmg_ evt_code);
                ++argc;
            }
            else
            {
                /* unrecognized message type */
                err = "getNetEvent(): unexpected message type in queue";
                goto evt_error;
            }
            break;

        case OSWAIT_EVENT + 1:
            /* 'quit' event - return an error */
            err = "getNetEvent(): queue terminated";
            goto evt_error;

        case OSWAIT_EVENT + 2:
            /* debug break - return a NetEvent with the interrupt code */
            argc = 1;
            G_interpreter->push_int(vmg_ VMBN_EVT_DBGBRK);
            break;
            
        case OSWAIT_TIMEOUT:
            /* the timeout expired - return a NetTimeoutEvent */
            ev_cl = G_predef->net_timeout_event;
            argc = 1;
            G_interpreter->push_int(vmg_ VMBN_EVT_TIMEOUT);
            break;

        case OSWAIT_ERROR:
        default:
            /* the wait failed */
            err = "getNetEvent(): error waiting for request message";

        evt_error:
            /* on error, throw a NetException describing the problem */
            G_interpreter->push_string(vmg_ err);
            G_interpreter->throw_new_class(
                vmg_ G_predef->net_exception, 1, err);
            AFTER_ERR_THROW(break;)
        }
        
        /* 
         *   if the specific event type subclass isn't defined, fall back on
         *   the base NetEvent class 
         */
        if (ev_cl == VM_INVALID_OBJ)
            ev_cl = G_predef->net_event;

        /* 
         *   construct the NetEvent subclass, or a generic list if we don't
         *   even have the NetEvent class defined 
         */
        if (ev_cl != VM_INVALID_OBJ)
            vm_objp(vmg_ ev_cl)->create_instance(vmg_ ev_cl, 0, argc);
        else
            retval_obj(vmg_ CVmObjList::create_from_stack(vmg_ 0, argc));
    }
    err_finally
    {
        /* we're done with the message object */
        if (msg != 0)
            msg->release_ref();
    }
    err_end;
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the local host name
 */
void CVmBifNet::get_hostname(VMG_ uint oargc)
{
    /* check arguments */
    check_argc(vmg_ oargc, 0);

    /* check the network safety levels */
    int client_level, server_level;
    G_host_ifc->get_net_safety(&client_level, &server_level);

    /* 
     *   If the network safety level doesn't allow outside network access for
     *   either client or server, return "localhost".  If they don't allow
     *   any network access at all, return nil.
     *   
     *   If there's a host name defined in the web configuration, return that
     *   host name.  Otherwise, return the default host name from the OS.  
     */
    if (client_level >= VM_NET_SAFETY_MAXIMUM
        && server_level >= VM_NET_SAFETY_MAXIMUM)
    {
        /* no network access is allowed - return nil */
        retval_nil(vmg0_);
    }
    else if (client_level >= VM_NET_SAFETY_LOCALHOST
        && server_level >= VM_NET_SAFETY_LOCALHOST)
    {
        /* localhost access only - return "localhost" */
        retval_str(vmg_ "localhost");
    }
    else
    {
        /* 
         *   the network safety level allows outside network access, so we
         *   can return the actual host name - get it from the system
         */
        char buf[256];
        if (os_get_hostname(buf, sizeof(buf)))
        {
            /* got it - return the string */
            retval_ui_str(vmg_ buf);
        }
        else
        {
            /* no host name available - return nil */
            retval_nil(vmg0_);
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the local host IP address
 */
void CVmBifNet::get_host_ip(VMG_ uint oargc)
{
    /* check arguments */
    check_argc(vmg_ oargc, 0);

    /* check the network safety levels */
    int client_level, server_level;
    G_host_ifc->get_net_safety(&client_level, &server_level);

    /* 
     *   If the network safety level doesn't allow outside network access for
     *   either client or server, return "localhost".  If they don't allow
     *   any network access at all, return nil.  
     */
    if (client_level >= VM_NET_SAFETY_MAXIMUM
        && server_level >= VM_NET_SAFETY_MAXIMUM)
    {
        /* no network access is allowed - return nil */
        retval_nil(vmg0_);
    }
    else if (client_level >= VM_NET_SAFETY_LOCALHOST
             && server_level >= VM_NET_SAFETY_LOCALHOST)
    {
        /* localhost access only - return the standard localhost IP address */
        retval_str(vmg_ "127.0.0.1");
    }
    else
    {
        /* retrieve the host IP address for the default host */
        char buf[256];
        if (os_get_local_ip(buf, sizeof(buf), 0))
        {
            /* got it - return the string */
            retval_ui_str(vmg_ buf);
        }
        else
        {
            /* no host name available - return nil */
            retval_nil(vmg0_);
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Get the network storage server URL. 
 */

/* get the storage server URL */
void CVmBifNet::get_storage_url(VMG_ uint oargc)
{
    /* check arguments */
    check_argc(vmg_ oargc, 1);

    /* set a default nil return in case we can't build the path */
    retval_nil(vmg0_);

    /* get the resource name */
    const char *page = G_stk->get(0)->get_as_string(vmg0_);
    if (page == 0)
        err_throw(VMERR_STRING_VAL_REQD);

    /* get the resource name length and buffer pointer */
    size_t pagelen = vmb_get_len(page);
    page += VMB_LEN;

    /* if there's a network configuration, build the resource path */
    const char *host = 0, *rootpath = 0;
    if (G_net_config != 0)
    {
        /* get the storage server host name and root path */
        host = G_net_config->get("storage.domain");
        rootpath = G_net_config->get("storage.rootpath", "/");
    }

    /* we must have a host name to proceed */
    if (host != 0)
    {
        /* build the full string */
        G_interpreter->push_stringf(vmg_ "http://%s%s%.*s",
                                    host, rootpath,
                                    (int)pagelen, page);

        /* pop it into R0 */
        G_stk->pop(G_interpreter->get_r0());
    }

    /* discard arguments */
    G_stk->discard();
}


/* ------------------------------------------------------------------------ */
/*
 *   Get the launching host address
 */
void CVmBifNet::get_launch_host_addr(VMG_ uint oargc)
{
    /* check arguments */
    check_argc(vmg_ oargc, 0);

    /* get the launch host name from the network configuration */
    const char *host = (G_net_config != 0
                        ? G_net_config->get("hostname")
                        : 0);

    /* if there's a host name, return it, otherwise return nil */
    if (host != 0)
        retval_str(vmg_ host);
    else
        retval_nil(vmg0_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Get a configuration variable
 */
#if 0 // leave this out for now
void CVmBifNet::get_net_config(VMG_ uint oargc)
{
    /* check arguments */
    check_argc(vmg_ oargc, 1);

    /* get the variable name */
    char name[256];
    pop_str_val_buf(vmg_ name, sizeof(name));

    /* look up the name */
    const char *val = 0;
    if (G_net_config != 0)
        val = G_net_config->get(name);

    /* if we found a value, return it; otherwise return nil */
    if (val != 0)
        retval_str(vmg_ val);
    else
        retval_nil(vmg0_);
}
#endif


/* ------------------------------------------------------------------------ */
/*
 *   Send a network request - result event. 
 */
class TadsHttpReqResult: public TadsEventMessage
{
public:
    TadsHttpReqResult(VMG_  vm_globalvar_t *idg, int status,
                      CVmDataSource *reply, char *hdrs, char *loc)
        : TadsEventMessage(0)
    {
        /* save the VM globals */
        vmg = VMGLOB_ADDR;

        /* save the ID global */
        this->idg = idg;

        /* remember the status */
        this->status = status;

        /* take ownership of the reply, headers, and location objects */
        this->reply = reply;
        this->hdrs = hdrs;
        this->loc = loc;
    }

    ~TadsHttpReqResult()
    {
        /* establish the global context */
        VMGLOB_PTR(vmg);

        /* delete our ID global */
        G_obj_table->delete_global_var(idg);

        /* delete the reply object */
        if (reply != 0)
            delete reply;

        /* free the reply headers */
        if (hdrs != 0)
            delete [] hdrs;

        /* free the redirect location string, if we have one */
        if (loc != 0)
            lib_free_str(loc);
    }

    /* prepare a bytecode event object when we're read from the queue */
    virtual vm_obj_id_t prep_event_obj(VMG_ int *argc, int *evt_type)
    {
        /* push the location string (or nil) */
        G_interpreter->push_string(vmg_ loc, loc != 0 ? strlen(loc) : 0);

        /* push the header string (or nil) */
        G_interpreter->push_string(vmg_ hdrs, hdrs != 0 ? strlen(hdrs) : 0);

        /* if we have reply data, push it as a File object */
        if (reply != 0)
        {
            /*
             *   Look for a content-type header, to determine the format of
             *   the reply body.  If the header starts with "text/", it's a
             *   text type; otherwise it's a binary type.
             */
            int mode = VMOBJFILE_MODE_RAW;
            const char *charset = "ISO-8859-1";
            size_t cslen = 10;
            const char *ct = find_header("content-type");
            if (ct != 0 && memcmp(ct, "text/", 5) == 0)
            {
                /* it's a textual type */
                mode = VMOBJFILE_MODE_TEXT;

                /* look for a 'charset' attribute */
                for (;;)
                {
                    /* scan ahead to the next ';' before end of line */
                    for ( ; *ct != '\0' && *ct != '\r'
                            && *ct != '\n' && *ct != ';' ; ++ct) ;
                    if (*ct != ';')
                        break;

                    /* skip the ';' and spaces */
                    for (ct += 1 ; *ct != '\0' && is_space(*ct) ; ++ct) ;

                    /* check for "charset" */
                    if (memcmp(ct, "charset=", 8) == 0)
                    {
                        /* note the "charset" attribute, and find the end */
                        for (ct += 8, charset = ct ;
                             *ct != '\r' && *ct != '\n' && *ct != ';'
                                 && !is_space(*ct) ; ++ct) ;

                        /* note the character set name length */
                        cslen = ct - charset;

                        /* look no further */
                        break;
                    }
                }
            }

            /* if it's a text-mode file, create a character mapper */
            vm_obj_id_t csobj = VM_INVALID_OBJ;
            if (mode == VMOBJFILE_MODE_TEXT)
                csobj = CVmObjCharSet::create(vmg_ FALSE, charset, cslen);

            /* seek to the beginning of the reply stream */
            reply->seek(0, OSFSK_SET);

            /* create a File object from the Data Source */
            vm_obj_id_t fileobj = CVmObjFile::create(
                vmg_ FALSE, 0, csobj, reply, mode,
                VMOBJFILE_ACCESS_READ, TRUE);
            
            /* push the file object */
            G_interpreter->push_obj(vmg_ fileobj);

            /* 
             *   the File object owns the data source, which owns the reply
             *   stream, so forget about the reply stream
             */
            reply = 0;
        }
        else
        {
            /* no reply data - push nil */
            G_interpreter->push_nil(vmg0_);
        }

        /* push the network status code */
        G_interpreter->push_int(vmg_ status);

        /* push the caller's ID value */
        G_stk->push(&idg->val);

        /* relay our added argument count to the caller */
        *argc = 5;

        /* set the event type to NetEvReply (5 - see include/tadsnet.h) */
        *evt_type = 5;

        /* the event subclass is NetReplyEvent */
        return G_predef->net_reply_event;
    }

protected:
    /* find a header */
    const char *find_header(const char *name)
    {
        /* remember the name length */
        size_t namelen = strlen(name);

        /* scan the headers */
        for (const char *p = hdrs ; p != 0 ; p = next_header(p))
        {
            /* skip spaces */
            for ( ; *p != '\0' && is_space(*p) ; ++p) ;

            /* check for a match */
            if (memicmp(p, name, namelen) == 0)
            {
                /* skip the name and any subsequent spaces */
                for (p += namelen ; *p != '\0' && is_space(*p) ; ++p) ;

                /* make sure we have a ':' */
                if (*p == ':')
                {
                    /* skip the ':' and subsequent spaces */
                    for (p += 1 ; *p != '\0' && is_space(*p) ; ++p) ;

                    /* this is the start of the header value */
                    return p;
                }
            }
        }

        /* didn't find it */
        return 0;
    }

    /* find the next header */
    const char *next_header(const char *p)
    {
        /* scan ahead to the next \r\n sequence */
        for ( ; *p != '\0' ; ++p)
        {
            /* if this is a CR-LF sequence, it's the end of this header */
            if (*p == '\r' && *(p+1) == '\n')
            {
                /* skip spaces */
                for (p += 2 ; *p != '\0' && is_space(*p) ; ++p) ;

                /* if this was a blank line, we're out of headers */
                if (*p == '\0' || (*p == '\r' && *(p+1) == '\n'))
                    return 0;

                /* this is the next header */
                return p;
            }
        }

        /* no more headers */
        return 0;
    }

    /* VM globals */
    vm_globals *vmg;

    /* 
     *   VM global variable for the ID value.  We need to hang on to the ID
     *   value until the request completes, since we have to pass it back to
     *   the program via the NetEvent we generate at completion.  Since the
     *   thread isn't a garbage-collected object, we have to explicitly
     *   register our reference on the ID value via a VM global variable.
     */
    vm_globalvar_t *idg;

    /* HTTP status (or OS_HttpClient::ErrXxx code, if negative) */
    int status;

    /* the reply content body */
    CVmDataSource *reply;

    /* reply headers */
    char *hdrs;

    /* redirect location */
    char *loc;
};

/* ------------------------------------------------------------------------ */
/*
 *   Request option flags 
 */
#define NetReqNoRedirect    0x0001

/* form body lookup table iterator context */
struct iter_body_table_ctx
{
    iter_body_table_ctx(class HttpReqThread *self, const vm_rcdesc *rc)
        : self(self), rc(rc) { }
    class HttpReqThread *self;
    const vm_rcdesc *rc;
};

/* ------------------------------------------------------------------------ */
/*
 *   Send a network request - HTTP client thread.  This is the worker thread
 *   we launch to asynchronously send an HTTP request to a remote server.
 */
class HttpReqThread: public OS_Thread
{
public:
    /* 
     *   Set up the thread.  All string parameters are provided in internal
     *   string format, with VMB_LEN length prefixes.  
     */
    HttpReqThread(VMG_ const vm_val_t *id, const char *url,
                  const char *verb, int32_t options,
                  const char *hdrs, vm_val_t *body, const char *body_type,
                  const vm_rcdesc *rc)
    {
        /* save the VM globals */
        vmg = VMGLOB_ADDR;

        /* add a reference on the net event queue */
        if ((queue = G_net_queue) != 0)
            queue->add_ref();

        /* create a global for the ID value */
        idg = G_obj_table->create_global_var();
        idg->val = *id;

        /* save the option flags */
        this->options = options;

        /* set up to parse the URL */
        size_t urll = vmb_get_len(url);
        url += VMB_LEN;

        /* note whether the scheme is plain http or https */
        https = (urll >= 8 && memcmp(url, "https://", 8) == 0);

        /* we don't have any URL pieces yet */
        host = 0;
        resource = 0;
        port = (https ? 443 : 80);

        /* skip the scheme prefix */
        size_t pfx = (urll >= 7 && memcmp(url, "http://", 7) == 0 ? 7 :
                      urll >= 8 && memcmp(url, "https://", 8) == 0 ? 8 : 0);
        url += pfx, urll -= pfx;

        /* the host name is the part up to the ':', '/', or end of string */
        const char *start;
        for (start = url ; urll > 0 && *url != ':' && *url != '/' ;
             ++url, --urll) ;

        /* save the host name */
        host = lib_copy_str(start, url - start);

        /* parse the port, if present */
        if (urll > 0 && *url == ':')
        {
            /* parse the port number string */
            for (--urll, start = ++url, port = 0 ; urll > 0 && *url != '/' ;
                 ++url, --urll)
            {
                if (is_digit(*url))
                    port = port*10 + value_of_digit(*url);
            }
        }

        /* 
         *   The rest (including the '/') is the resource string.  If there's
         *   nothing left, the resource is implicitly '/'. 
         */
        if (urll > 0)
            resource = lib_copy_str(url, urll);
        else
            resource = lib_copy_str("/");

        /* make null-terminated copies of the various strings */
        this->verb = lib_copy_str(verb + VMB_LEN, vmb_get_len(verb));
        this->hdrs = (hdrs == 0 ? 0 :
                      lib_copy_str(hdrs + VMB_LEN, vmb_get_len(hdrs)));

        /* if there's a content body, set up the payload */
        this->body = 0;
        if (body != 0)
        {
            /* set up an empty payload */
            this->body = new OS_HttpPayload();
            
            /* 
             *   Convert the body to a stream payload, if it's a string or
             *   ByteArray.  If it's a LookupTable, it's a set of form
             *   fields.
             */
            if (body->typ == VM_OBJ
                && CVmObjLookupTable::is_lookup_table_obj(vmg_ body->val.obj))
            {
                /* 
                 *   The body is a LookupTable, so we have a set of form
                 *   fields to encode as HTML form data. 
                 */
                
                /* cast the object value */
                CVmObjLookupTable *tab =
                    (CVmObjLookupTable *)vm_objp(vmg_ body->val.obj);
                
                /* add each key in the table as a form field */
                iter_body_table_ctx ctx(this, rc);
                tab->for_each(vmg_ iter_body_table, &ctx);
            }
            else
            {
                /* 
                 *   It's not a lookup table, so it must be bytes to upload,
                 *   as a string or ByteArray.
                 */
                const char *def_mime_type = 0;
                CVmDataSource *s = val_to_ds(vmg_ body, &def_mime_type);
                
                /* presume we're going to use the default mime type */
                const char *mime_type = def_mime_type;
                size_t mime_type_len =
                    (mime_type != 0 ? strlen(mime_type) : 0);

                /* if the caller specified a mime type, use it instead */
                if (body_type != 0)
                {
                    mime_type = body_type + VMB_LEN;
                    mime_type_len = vmb_get_len(body_type);
                }
                
                /* add the stream */
                this->body->add(
                    "", 0, "", 0, mime_type, mime_type_len, s);
            }
        }
    }

    ~HttpReqThread()
    {
        /* establish the global context */
        VMGLOB_PTR(vmg);

        /* delete copied strings */
        lib_free_str(host);
        lib_free_str(resource);
        lib_free_str(verb);
        lib_free_str(hdrs);

        /* delete the content body */
        if (body != 0)
            delete body;

        /* release the message queue */
        if (queue != 0)
            queue->release_ref();

        /* delete our ID global, if we still own it */
        if (idg != 0)
            G_obj_table->delete_global_var(idg);

    }

    /* main thread entrypoint */
    void thread_main()
    {
        /* if they want redirections, set up to receive the location */
        char *loc = 0;
        char **locp = ((options & NetReqNoRedirect) != 0 ? &loc : 0);

        /* set up a memory stream object to hold the reply */
        CVmMemorySource *reply = new CVmMemorySource(128);

        /* no reply headers yet */
        char *reply_hdrs = 0;

        /* figure the request option flags */
        int rflags = 0;
        if (https)
            rflags |= OS_HttpClient::OptHTTPS;

        /* get the length of the custom headers, if we have any */
        size_t hdrs_len = (hdrs != 0 ? strlen(hdrs) : 0);

        /* send the request */
        int status = OS_HttpClient::request(
            rflags, host, port, verb, resource, hdrs, hdrs_len, body,
            reply, &reply_hdrs, locp, 0);
        
        /* 
         *   if the status is negative, a system-level error occurred, so
         *   there's no reply data - delete and forget the reply stream
         */
        if (status < 0)
        {
            delete reply;
            reply = 0;
        }
        
        /* 
         *   Always post a result event, even on failure, since the event is
         *   the way we transmit the status information back to the caller.
         *   Note that the event object takes ownership of the reply stream,
         *   headers string, and location string, and will free the
         *   underlying memory when the event object is deleted.
         */
        post_result(status, reply, reply_hdrs, loc);
    }

    /* post the result */
    void post_result(int status, CVmDataSource *reply, char *hdrs, char *loc)
    {
        /* establish the global context */
        VMGLOB_PTR(vmg);

        /* create the result event */
        TadsHttpReqResult *evt = new TadsHttpReqResult(
            vmg_ idg, status, reply, hdrs, loc);

        /* post the event (this transfers ownership to the queue) */
        queue->post(evt);

        /* the result event now owns our ID global 'idg', so forget it */
        idg = 0;
    }

protected:
    /*
     *   Convert a string or ByteArray object to a data stream 
     */
    static CVmDataSource *val_to_ds(VMG_ const vm_val_t *val,
                                    const char **def_mime_type)
    {
        /* try it as a string */
        const char *bstr = val->get_as_string(vmg0_);
        if (bstr != 0)
        {
            /* assume this is text/plain */
            *def_mime_type = "text/plain; charset=utf-8";

            /* create a stream with a private copy of the string */
            return new CVmPrivateStringSource(
                bstr + VMB_LEN, vmb_get_len(bstr));
        }
        else if (val->typ == VM_OBJ
                 && CVmObjByteArray::is_byte_array(vmg_ val->val.obj))
        {
            /* assume this is binary data */
            *def_mime_type = "application/octet-stream";

            /* cast the ByteArray object value */
            CVmObjByteArray *barr =
                (CVmObjByteArray *)vm_objp(vmg_ val->val.obj);
            
            /* create a stream copy of the byte array's contents */
            unsigned long arrlen = barr->get_element_count();
            CVmDataSource *s = new CVmMemorySource(arrlen);
            
            /* copy the array's contents to the stream */
            const size_t BLOCKLEN = 1024;
            char block[BLOCKLEN];
            for (unsigned long idx = 1 ; idx <= arrlen ; idx += BLOCKLEN)
            {
                /* read as much as remains */
                unsigned long rem = arrlen - idx + 1;
                size_t cur = (rem < BLOCKLEN ? (size_t)rem : BLOCKLEN);
                
                /* copy the data */
                barr->copy_to_buf((unsigned char *)block, idx, cur);
                s->write(block, cur);
            }

            /* 
             *   rewind the stream to the start, so that the network code
             *   reads the entire contents from the beginning 
             */
            s->seek(0, OSFSK_SET);
            
            /* return the stream */
            return s;
        }
        else
        {
            /* we can't convert other types */
            return 0;
        }
    }

    /* 
     *   Lookup Table for_each callback for building a form payload.  When
     *   the caller specifies the body as a lookup table, we'll iterate over
     *   the table's contents using this callback, which adds each field to
     *   the payload object.
     */
    static void iter_body_table(
        VMG_ const vm_val_t *key, const vm_val_t *val, void *ctx0)
    {
        /* get the context */
        iter_body_table_ctx *ctx = (iter_body_table_ctx *)ctx0;

        /* the key must be a string giving the field name */
        const char *field_name = key->get_as_string(vmg0_);
        if (field_name == 0)
            return;

        /* 
         *   the value must be either a string giving the simple form field
         *   value, or a FileUpload object giving a file to upload 
         */
        if (val->is_instance_of(vmg_ G_predef->file_upload_cl))
        {
            /* 
             *   It's a FileUpload object.  The 'file' property is the
             *   content object, which must be provided as a string or
             *   ByteArray; the 'filename' and 'contentType' properties are
             *   strings giving us details on how to treat the content data. 
             */

            /* retrieve the property values we're interested in */
            vm_val_t file_val, ctype_val, fname_val;
            G_interpreter->get_prop(
                vmg_ &file_val, val, G_predef->file_upload_fileObj, 0,
                ctx->rc);
            G_interpreter->get_prop(
                vmg_ &ctype_val, val, G_predef->file_upload_contentType, 0,
                ctx->rc);
            G_interpreter->get_prop(
                vmg_ &fname_val, val, G_predef->file_upload_filename, 0,
                ctx->rc);

            /* wrap the 'file' value in a datasource */
            const char *def_ctype = 0;
            CVmDataSource *src = val_to_ds(vmg_ &file_val, &def_ctype);

            /* if we have a contentType string, override the default */
            const char *ctype = ctype_val.get_as_string(vmg0_);
            size_t ctype_len;
            if (ctype != 0)
            {
                /* they specified a content type - use their value */
                ctype_len = vmb_get_len(ctype);
                ctype += VMB_LEN;
            }
            else
            {
                /* use the default */
                ctype = def_ctype;
                ctype_len = strlen(def_ctype);
            }

            /* get the filename string, if they provided one */
            const char *fname = fname_val.get_as_string(vmg0_);
            size_t fname_len = (fname != 0 ? vmb_get_len(fname) : 0);
            fname += (fname != 0 ? VMB_LEN : 0);

            /* add the field */
            ctx->self->body->add(
                field_name + VMB_LEN, vmb_get_len(field_name),
                fname, fname_len, ctype, ctype_len, src);
        }
        else
        {
            /* get a string interpretation of the value */
            vm_val_t new_str;
            const char *field_val = val->cast_to_string(vmg_ &new_str);
            if (field_val == 0)
                return;

            /* add the field to the payload */
            ctx->self->body->add_tstr(field_name, field_val);
        }
    }

    /* VM globals */
    vm_globals *vmg;

    /* event message queue */
    TadsMessageQueue *queue;

    /* option flags */
    int32_t options;

    /* parsed URL information */
    int https;
    char *host;
    char *resource;
    unsigned short port;

    /* the verb */
    char *verb;

    /* custom headers */
    char *hdrs;

    /* the content body */
    OS_HttpPayload *body;

    /* 
     *   VM global variable for the ID value.  We need to hang on to the ID
     *   value until the request completes, since we have to pass it back to
     *   the program via the NetEvent we generate at completion.  Since the
     *   thread isn't a garbage-collected object, we have to explicitly
     *   register our reference on the ID value via a VM global variable.
     */
    vm_globalvar_t *idg;
};


/* ------------------------------------------------------------------------ */
/*
 *   Match a domain name in a URL to the given string 
 */
static int match_url_domain(const char *url, size_t urll, const char *domain)
{
    /* note the length of the domain name we're matching */
    size_t domainl = strlen(domain);

    /* scan to the end of the scheme name */
    for ( ; urll > 0 ; ++url, --urll)
    {
        /* the scheme name ends in "://" */
        if (urll > 3 && memcmp(url, "://", 3) == 0)
        {
            /* skip the :// sequence */
            url += 3;
            urll -= 3;

            /* 
             *   we have a match if the domain name follows, and either it's
             *   the whole rest of the string, or the next character after
             *   the domain name is '/' or ':' 
             */
            return (urll >= domainl
                    && memicmp(url, domain, domainl) == 0
                    && (urll == domainl
                        || url[domainl] == ':'
                        || url[domainl] == '/'));
        }
    }

    /* didn't find the end of the scheme name - fail */
    return FALSE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Send a network request
 */
void CVmBifNet::send_net_request(VMG_ uint argc)
{
    /* check arguments - we need (id, url), and after that it varies */
    check_argc_range(vmg_ argc, 2, 32767);

    /* 
     *   set up a recursive VM context, in case we need it (we might, for
     *   evaluating FileUpload properties in a form body lookup table) 
     */
    vm_rcdesc rc(vmg_ "sendNetRequest", CVmBifNet::bif_table, 6, 
                 G_stk->get(0), argc);

    /* get the network safety levels */
    int client_level, server_level;
    G_host_ifc->get_net_safety(&client_level, &server_level);

    /* if the client safety level doesn't allow client operations, fail */
    if (client_level >= VM_NET_SAFETY_MAXIMUM)
        err_throw(VMERR_NETWORK_SAFETY);

    /* retrieve the ID value - any type is allowed here */
    vm_val_t *id = G_stk->get(0);

    /* retrieve the URL - this must be a string */
    const char *url = get_str_val(vmg_ G_stk->get(1));
    size_t urll = vmb_get_len(url);
    url += VMB_LEN;

    /* get the image URL, for logging purposes */
    const char *image_url = "<image URL unknown>";
    if (G_net_config != 0)
        image_url = G_net_config->get("image.url", "<image URL unknown>");

    /* map the URL to the local file character set */
    char *lurl;
    size_t lurll = G_cmap_to_file->map_utf8_alo(&lurl, url, urll);
    
    /* log the request */
    vm_log_fmt(vmg_ "client request to %.*s by %s",
               (int)lurll, lurl, image_url);

    /* done with the local version of the URL */
    t3free(lurl);

    /* check the protocol */
    if (urll >= 7
        && (memcmp(url, "http://", 7) == 0
            || memcmp(url, "https://", 8) == 0))
    {
        /* HTTP or HTTPS */

        /* 
         *   if we're restricted to local access, verify that the domain is
         *   "localhost" or "127.0.0.1"
         */
        if (client_level == VM_NET_SAFETY_LOCALHOST)
        {
            /* get a pointer to the domain */
            if (!match_url_domain(url, urll, "localhost")
                && !match_url_domain(url, urll, "127.0.0.1"))
                err_throw(VMERR_NETWORK_SAFETY);
        }

        /* get the verb - this must be a string */
        const char *verb = get_str_val(vmg_ G_stk->get(2));

        /* get the option flags, if present */
        int32_t options = 0;
        if (argc >= 4 && G_stk->get(3)->typ != VM_NIL)
            options = G_stk->get(3)->num_to_int(vmg0_);

        /* get the headers, if present */
        const char *hdrs = 0;
        if (argc >= 5 && G_stk->get(4)->typ != VM_NIL)
            hdrs = get_str_val(vmg_ G_stk->get(4));

        /* get the content body, if present */
        vm_val_t *body = 0;
        if (argc >= 6 && G_stk->get(5)->typ != VM_NIL)
        {
            /* get the body value */
            body = G_stk->get(5);

            /* 
             *   verify the body type - this must be a string, byte array, or
             *   lookup table 
             */
            if (body->get_as_string(vmg0_) != 0
                || (body->typ == VM_OBJ
                    && (CVmObjByteArray::is_byte_array(vmg_ body->val.obj)
                        || CVmObjLookupTable::is_lookup_table_obj(
                            vmg_ body->val.obj))))
            {
                /* acceptable body type */
            }
            else
            {
                /* invalid body type */
                err_throw(VMERR_BAD_TYPE_BIF);
            }
        }

        /* get the body's MIME type, if present */
        const char *body_type = 0;
        if (argc >= 7 && G_stk->get(6)->typ != VM_NIL)
            body_type = get_str_val(vmg_ G_stk->get(6));

        /* create the request processor thread */
        HttpReqThread *th = new HttpReqThread(
            vmg_ id, url - VMB_LEN, verb, options, hdrs, body, body_type, &rc);

        /* launch the thread */
        if (!th->launch())
        {
            /* 
             *   We couldn't launch the thread - post a result event
             *   indicating failure.  We have to post the result event
             *   ourselves because (a) a result event always has to be
             *   posted, even when the request fails, as it's the way that we
             *   transmit all status information back to the caller, and (b)
             *   the thread would normally post the result, but it won't be
             *   doing so because it never got started.
             */
            th->post_result(OS_HttpClient::ErrThread, 0, 0, 0);
        }

        /* done with the thread object */
        th->release_ref();
    }
    else
    {
        /* unknown protocol - it's an error */
        err_throw(VMERR_BAD_VAL_BIF);
    }

    /* pop the arguments */
    G_stk->discard(argc);

    /* there's no return value, but return nil to clear out any old data */
    retval_nil(vmg0_);
}
