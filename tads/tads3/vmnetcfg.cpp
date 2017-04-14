/* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmnet.cpp - TADS 3 network configuration
Function
  Implements the network configuration object for TADS 3.
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
#include "vmerr.h"
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
#include "vmdatasrc.h"


/* ------------------------------------------------------------------------ */
/*
 *   Configuration file reader 
 */

TadsNetConfig::~TadsNetConfig()
{
    /* delete our variables */
    TadsNetConfigVar *cur, *nxt;
    for (cur = first_var ; cur != 0 ; cur = nxt)
    {
        nxt = cur->getnext();
        delete cur;
    }
}

/*
 *   load a configuration file 
 */
void TadsNetConfig::read(osfildef *fp, CVmMainClientIfc *clientifc)
{
    /* read the file line by line */
    for (int linenum = 1 ; ; ++linenum)
    {
        /* read the next line */
        char buf[512];
        if (osfgets(buf, sizeof(buf), fp) == 0)
            break;

        /* skip leading whitespace */
        char *p;
        for (p = buf ; isspace(*p) ; ++p) ;

        /* skip blank lines and comments */
        if (*p == '\0' || *p == '#')
            continue;

        /* the variable name is the first token on the line */
        char *name = p;

        /* find the end of the name: the next space or '=' */
        for ( ; *p != '\0' && !isspace(*p) && *p != '=' ; ++p) ;
        char *name_end = p;

        /* skip spaces */
        for ( ; isspace(*p) ; ++p) ;

        /* make sure we stopped at a '=' */
        if (*p != '=')
        {
            char *msg = t3sprintf_alloc(
                "Missing '=' in web config file at line %d", linenum);
            clientifc->display_error(0, 0, msg, FALSE);
            t3free(msg);
            continue;
        }

        /* null-terminate the name */
        *name_end = '\0';

        /* skip spaces after the '=' */
        for (++p ; isspace(*p) ; ++p) ;

        /* the value starts here */
        char *val = p;

        /* 
         *   the value is the rest of the line, minus trailing spaces and
         *   newlines 
         */
        for (p += strlen(p) ;
             p > val && (isspace(*(p-1))
                         || *(p-1) == '\n' || *(p-1) == '\r') ;
             --p) ;

        /* null-terminate the value */
        *p = '\0';

        /* add the variable */
        TadsNetConfigVar *var = set(name, val);

        /* check that this is a variable name we recognize */
        static const char *known_vars[] = {
            "serverid",
            "storage.domain",
            "storage.rootpath",
            "storage.apikey",
            "watchdog"
        };

        int found = FALSE;
        for (size_t i = 0 ; i < countof(known_vars) ; ++i)
        {

            if (var->matchname(known_vars[i]))
            {
                found = TRUE;
                break;
            }
        }

        /* warn if we didn't find it among the known variables */
        if (!found)
        {
            char *msg = t3sprintf_alloc(
                "Warning: unknown variable name '%s' in web config file "
                "at line %d\n", name, linenum);
            clientifc->display_error(0, 0, msg, FALSE);
            t3free(msg);
        }
    }
}

/*
 *   set a variable 
 */
TadsNetConfigVar *TadsNetConfig::set(const char *name, const char *val)
{
    /* check for an existing variabel */
    TadsNetConfigVar *var = getvar(name);
    if (var != 0)
    {
        /* found it - set the new value */
        var->setval(val);
    }
    else
    {
        /* didn't find it - add a new variable */
        var = new TadsNetConfigVar(name, val);
        if (last_var != 0)
            last_var->setnext(var);
        else
            first_var = var;
        last_var = var;
    }

    /* return the variable */
    return var;
}

/* 
 *   retrieve a variable's value 
 */
const char *TadsNetConfig::get(const char *name) const
{
    TadsNetConfigVar *var = getvar(name);
    return (var != 0 ? var->getval() : 0);
}

/*
 *   look up a variable 
 */
TadsNetConfigVar *TadsNetConfig::getvar(const char *name) const
{
    /* scan the variable list for the given name */
    for (TadsNetConfigVar *cur = first_var ; cur != 0 ; cur = cur->getnext())
    {
        /* if this one's name matches, return it */
        if (cur->matchname(name))
            return cur;
    }

    /* didn't find it */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Check a storage server API reply 
 */
void vmnet_check_storagesrv_reply(VMG_ int htmlstat, CVmDataSource *reply,
                                  const char *headers)
{
    /* retrieve and check the status code */
    vmnet_check_storagesrv_stat(
        vmg_ vmnet_get_storagesrv_stat(vmg_ htmlstat, reply, headers));
}

/*
 *   Check a storage server status code returned by
 *   vmnet_get_storagesrv_reply().  On failure, throw an error; on success,
 *   simply return.  In either case, we'll free the status code buffer. 
 */
void vmnet_check_storagesrv_stat(VMG_ char *stat)
{
    /* if it's not "OK", throw an error */
    if (memcmp(stat, "OK ", 3) != 0)
    {
        /* 
         *   Error.  Push the error code/message text string as the argument
         *   to the runtime error constructor, then discard our copy of the
         *   buffer.  
         */
        G_interpreter->push_string(vmg_ stat);
        t3free(stat);

        /* throw a StorageServerError */
        G_interpreter->throw_new_rtesub(
            vmg_ G_predef->storage_server_error, 1,
            VMERR_STORAGE_SERVER_ERR);
    }
    else
    {
        /* success - discard the message text */
        t3free(stat);
    }
}

/*
 *   Get the storage server API reply code and message.  Returns an allocated
 *   buffer that the caller must free with t3free().  The first
 *   space-delimited token in the return buffer is the code, and the rest is
 *   the human-readable error message.  For HTTP or network errors, the code
 *   is simply the numeric error code (positive for HTTP status codes,
 *   negative for internal network errors), with no message text.  
 */
char *vmnet_get_storagesrv_stat(VMG_ int htmlstat, CVmDataSource *reply,
                                const char *headers)
{
    /* check the HTML status */
    if (htmlstat == 200)
    {
        /* 
         *   The HTML transaction succeeded - check the reply.  Start with
         *   the headers, if provided.
         */
        if (headers != 0)
        {
            /* find the X-IFDBStorage-Status header */
            for (const char *p = headers ; ; p += 2)
            {
                /* are we at our header? */
                if (memcmp(p, "X-IFDBStorage-Status:", 21) == 0)
                {
                    /* this is our header - skip spaces and get the value */
                    for (p += 21 ; isspace(*p) ; ++p) ;

                    /* find the end of the line or end of the headers */
                    const char *nl = strstr(p, "\r\n");
                    if (nl == 0)
                        nl = p + strlen(p);

                    /* return the message text */
                    return lib_copy_str(p, nl - p);
                }

                /* not our header - skip to the end of the line */
                p = strstr(p, "\r\n");
                if (p == 0)
                    break;
            }
        }

        /* 
         *   We didn't find the header, so check the reply body.  Read the
         *   first line of the reply, since this contains the result code.  
         */
        reply->seek(0, OSFSK_SET);
        char *txt = reply->read_line_alo();

        /* remove the trailing newline */
        size_t l;
        if ((l = strlen(txt)) != 0 && txt[l-1] == '\n')
            txt[--l] = '\0';

        /* return the message text */
        return txt;
    }
    else
    {
        /* 
         *   HTML or network error.  Return a message containing the numeric
         *   status as the error code, with no text message. 
         */
        return t3sprintf_alloc("%d ", htmlstat);
    }
}
