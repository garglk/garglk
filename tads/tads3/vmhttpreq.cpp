#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2010 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmhttpreq.cpp - CVmObjHTTPRequest object
Function
  
Notes
  
Modified
   MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "t3std.h"
#include "vmhttpreq.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmtype.h"
#include "vmglob.h"
#include "vmobj.h"
#include "vmundo.h"
#include "vmmcreg.h"
#include "vmfile.h"
#include "vmbif.h"
#include "vmmeta.h"
#include "vmstack.h"
#include "vmrun.h"
#include "vmstr.h"
#include "vmstrbuf.h"
#include "vmlst.h"
#include "vmlookup.h"
#include "utf8.h"
#include "vmnet.h"
#include "vmpredef.h"
#include "vmbytarr.h"
#include "charmap.h"
#include "vmfilobj.h"
#include "vmdatasrc.h"
#include "vmcset.h"
#include "vmimport.h"



/* ------------------------------------------------------------------------ */
/*
 *   Allocate an extension structure 
 */
vm_httpreq_ext *vm_httpreq_ext::alloc_ext(VMG_ CVmObjHTTPRequest *self)
{
    /* calculate how much space we need */
    size_t siz = sizeof(vm_httpreq_ext);

    /* allocate the memory */
    vm_httpreq_ext *ext = (vm_httpreq_ext *)G_mem->get_var_heap()->alloc_mem(
        siz, self);

    /* we don't have a system message object yet */
    ext->req = 0;

    /* we have no reply cookies yet */
    ext->cookies = 0;

    /* return the new extension */
    return ext;
}

/* ------------------------------------------------------------------------ */
/*
 *   CVmObjHTTPRequest object statics 
 */

/* metaclass registration object */
static CVmMetaclassHTTPRequest metaclass_reg_obj;
CVmMetaclass *CVmObjHTTPRequest::metaclass_reg_ = &metaclass_reg_obj;

/* function table */
int (CVmObjHTTPRequest::*CVmObjHTTPRequest::func_table_[])(
    VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc) =
{
    &CVmObjHTTPRequest::getp_undef,
    &CVmObjHTTPRequest::getp_getServer,
    &CVmObjHTTPRequest::getp_getVerb,
    &CVmObjHTTPRequest::getp_getQuery,
    &CVmObjHTTPRequest::getp_parseQuery,
    &CVmObjHTTPRequest::getp_getQueryParam,
    &CVmObjHTTPRequest::getp_getHeaders,
    &CVmObjHTTPRequest::getp_getCookie,
    &CVmObjHTTPRequest::getp_getCookies,
    &CVmObjHTTPRequest::getp_getFormFields,
    &CVmObjHTTPRequest::getp_getBody,
    &CVmObjHTTPRequest::getp_getClientAddress,
    &CVmObjHTTPRequest::getp_setCookie,
    &CVmObjHTTPRequest::getp_sendReply,
    &CVmObjHTTPRequest::getp_startChunkedReply,
    &CVmObjHTTPRequest::getp_sendReplyChunk,
    &CVmObjHTTPRequest::getp_endChunkedReply,
    &CVmObjHTTPRequest::getp_sendReplyAsync
};

/* ------------------------------------------------------------------------ */
/*
 *   CVmObjHTTPRequest intrinsic class implementation 
 */

/*
 *   construction
 */
CVmObjHTTPRequest::CVmObjHTTPRequest(VMG_ int)
{
    /* allocate our extension structure */
    ext_ = (char *)vm_httpreq_ext::alloc_ext(vmg_ this);
}

/*
 *   create 
 */
vm_obj_id_t CVmObjHTTPRequest::create(
    VMG_ int in_root_set, TadsHttpRequest *req, vm_obj_id_t server)
{
    /* create the object - we have no gc-obj refs of any kind */
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);

    /* requests are inherently transient */
    G_obj_table->set_obj_transient(id);

    /* create the C++ object and get its extension */
    CVmObjHTTPRequest *r = new (vmg_ id) CVmObjHTTPRequest(vmg_ TRUE);
    vm_httpreq_ext *ext = r->get_ext();

    /* save the system request object */
    if ((ext->req = req) != 0)
        req->add_ref();

    /* save the server object */
    ext->server = server;

    /* return the new object ID */
    return id;
}

/*
 *   create dynamically using stack arguments 
 */
vm_obj_id_t CVmObjHTTPRequest::create_from_stack(
    VMG_ const uchar **pc_ptr, uint argc)
{
    /* 
     *   we can't be created with 'new' - we can only be created via the
     *   getNetEvent() intrinsic function 
     */
    err_throw(VMERR_BAD_DYNAMIC_NEW);

    /* not reached, but the compiler might not know that */
    AFTER_ERR_THROW(return VM_INVALID_OBJ;)
}

/* 
 *   notify of deletion 
 */
void CVmObjHTTPRequest::notify_delete(VMG_ int /*in_root_set*/)
{
    /* free our additional data, if we have any */
    vm_httpreq_ext *ext = get_ext();
    if (ext != 0)
    {
        /* release the message object */
        if (ext->req != 0)
            ext->req->release_ref();

        /* delete the cookies */
        if (ext->cookies != 0)
            delete ext->cookies;
        
        /* delete the extension */
        G_mem->get_var_heap()->free_mem(ext_);
    }
}

/*
 *   Mark references 
 */
void CVmObjHTTPRequest::mark_refs(VMG_ uint state)
{
    /* if we have a server object, mark it */
    vm_httpreq_ext *ext = get_ext();
    if (ext->server != VM_INVALID_OBJ)
        G_obj_table->mark_all_refs(ext->server, state);
}

/* 
 *   set a property 
 */
void CVmObjHTTPRequest::set_prop(VMG_ class CVmUndo *undo,
                                 vm_obj_id_t self, vm_prop_id_t prop,
                                 const vm_val_t *val)
{
    /* no settable properties - throw an error */
    err_throw(VMERR_INVALID_SETPROP);
}

/* 
 *   get a property 
 */
int CVmObjHTTPRequest::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
                                vm_obj_id_t self, vm_obj_id_t *source_obj,
                                uint *argc)
{
    
    /* translate the property into a function vector index */
    uint func_idx = G_meta_table->prop_to_vector_idx(
        metaclass_reg_->get_reg_idx(), prop);
    
    /* call the appropriate function */
    if ((this->*func_table_[func_idx])(vmg_ self, retval, argc))
    {
        *source_obj = metaclass_reg_->get_class_obj(vmg0_);
        return TRUE;
    }
    
    /* inherit default handling from our base class */
    return CVmObject::get_prop(vmg_ prop, retval, self, source_obj, argc);
}

/* 
 *   load from an image file 
 */
void CVmObjHTTPRequest::load_from_image(VMG_ vm_obj_id_t self,
                                        const char *ptr, size_t siz)
{
    /* load our image data */
    load_image_data(vmg_ ptr, siz);

    /* 
     *   save our image data pointer in the object table, so that we can
     *   access it (without storing it ourselves) during a reload 
     */
    G_obj_table->save_image_pointer(self, ptr, siz);
}

/*
 *   reload from the image file 
 */
void CVmObjHTTPRequest::reload_from_image(VMG_ vm_obj_id_t self,
                                          const char *ptr, size_t siz)
{
    /* load our image data */
    load_image_data(vmg_ ptr, siz);
}

/*
 *   load or reload data from the image 
 */
void CVmObjHTTPRequest::load_image_data(VMG_ const char *ptr, size_t siz)
{
    /* free our existing extension, if we have one */
    notify_delete(vmg_ FALSE);

    /* allocate the extension */
    vm_httpreq_ext *ext = vm_httpreq_ext::alloc_ext(vmg_ this);
    ext_ = (char *)ext;
}


/* 
 *   save to a file 
 */
void CVmObjHTTPRequest::save_to_file(VMG_ class CVmFile *fp)
{
    /* we're inherently transient, so we should never do this */
    assert(FALSE);
}

/* 
 *   restore from a file 
 */
void CVmObjHTTPRequest::restore_from_file(VMG_ vm_obj_id_t self,
                                          CVmFile *fp, CVmObjFixup *fixups)
{
    /* we're transient, so this should never happen */
}

/*
 *   Create a new validated ASCII string.  This converts any 8-bit characters
 *   to '?' to ensure that the result is well-formed UTF-8.  
 */
static void new_ascii_string(VMG_ vm_val_t *ret, const char *str, size_t len)
{

    /* create and return a string for it */
    vm_obj_id_t strid = CVmObjString::create(vmg_ FALSE, str, len);
    ret->set_obj(strid);

    /* 
     *   We expected a plain ASCII string as input, so any character outside
     *   of the 7-bit ASCII range (0-127) is invalid.  Scan the string and
     *   convert any invalid characters to '?' characters.  If we didn't do
     *   this, any 8-bit characters could potentially trigger VM crashes,
     *   since we'd try to interpret them as multi-byte UTF-8 character
     *   components even though they might not form valid UTF-8 sequences.
     *   Eliminating any 8-bit characters up front thus protects us against
     *   errant or malicious clients by ensuring that we have a well-formed
     *   UTF-8 string in all cases (since any string containing only plain
     *   ASCII characters is inherently a well-formed UTF-8 string).  
     */
    CVmObjString *strp = (CVmObjString *)vm_objp(vmg_ strid);
    for (char *p = strp->cons_get_buf() ; len != 0 ; ++p, --len)
    {
        /* convert anything outside of plain ASCII (0-127) to '?' */
        if (*(uchar *)p > 127)
            *p = '?';
    }
}
static void new_ascii_string(VMG_ vm_val_t *ret, const char *str)
{
    return new_ascii_string(vmg_ ret, str, strlen(str));
}

/*
 *   get the server (the HTTPServer object)
 */
int CVmObjHTTPRequest::getp_getServer(VMG_ vm_obj_id_t self,
                                      vm_val_t *retval, uint *oargc)
{
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* return the server object */
    retval->set_obj_or_nil(get_ext()->server);

    /* handled */
    return TRUE;
}

/*
 *   get the HTTP verb
 */
int CVmObjHTTPRequest::getp_getVerb(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *oargc)
{
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* return a validated ASCII string for the verb in the request */
    new_ascii_string(vmg_ retval, get_ext()->req->verb->get());

    /* handled */
    return TRUE;
}

/*
 *   get the query string
 */
int CVmObjHTTPRequest::getp_getQuery(VMG_ vm_obj_id_t self,
                                     vm_val_t *retval, uint *oargc)
{
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* create a validated ASCII string for the query string */
    new_ascii_string(vmg_ retval, get_ext()->req->resource_name->get());

    /* handled */
    return TRUE;
}

/* encoding modes for new_parsed_str */
#define ENCMODE_NONE   0                                     /* no encoding */
#define ENCMODE_URL    1              /* URL encoding - translate %xx codes */
#define ENCMODE_FORM   2        /* form-data: translate %xx, and + to space */
#define ENCMODE_MIME   3               /* MIME header: translate ?= strings */

/* 
 *   Create a parsed URL string value.  This converts %xx sequences to bytes,
 *   and ensures that the results are well-formed UTF-8 characters. 
 */
static void new_parsed_str(VMG_ vm_val_t *ret, const char *str, size_t len,
                           int encmode)
{
    char *src, *dst;

    /* create the raw string */
    ret->set_obj(CVmObjString::create(vmg_ FALSE, str, len));
    CVmObjString *s = (CVmObjString *)vm_objp(vmg_ ret->val.obj);

    /* get the string buffer */
    char *buf = s->cons_get_buf();
    char *bufend = buf + len;

    /* 
     *   if desired, decode url-encoding: run through the string, converting
     *   '%xx' sequences to bytes, and '+' characters to spaces
     */
    if (encmode == ENCMODE_URL || encmode == ENCMODE_FORM)
    {
        for (src = dst = buf ; src < bufend ; )
        {
            /* if this is a '%xx' sequence, convert it */
            if (*src == '%' && src + 2 < bufend
                && is_xdigit(*(src+1)) && is_xdigit(*(src+2)))
            {
                /* generate the character value */
                int c = (value_of_xdigit(*(src+1)) << 4)
                        | value_of_xdigit(*(src+2));

                /* skip the three-character %xx sequence */
                src += 3;

                /* 
                 *   if this is an encoded CR, and an LF follows, decode the
                 *   whole sequence to a single '\n' 
                 */
                if (c == '\r'
                    && src + 2 < bufend
                    && memicmp(src, "%0a", 3) == 0)
                {
                    /* change to '\n' */
                    c = '\n';

                    /* skip the in the source */
                    src += 3;
                }
                
                /* set the character in the output */
                *dst++ = (char)(unsigned char)c;
            }
            else if (*src == '+'
                     && (encmode == ENCMODE_FORM || encmode == ENCMODE_URL))
            {
                /* for form data, translate + to space */
                *dst++ = ' ';
                ++src;
            }
            else if ((*src == '\r' || *src == '\n')
                     && encmode == ENCMODE_FORM)
            {
                /* for form data, strip out newlines */
                ++src;
            }
            else
            {
                /* ordinary character - copy as-is */
                *dst++ = *src++;
            }
        }
        
        /* 
         *   update the length - the string might have contracted, since
         *   '%xx' sequences might have turned into single bytes 
         */
        len = dst - buf;
        bufend = dst;
        s->cons_set_len(len);
    }
    else if (encmode == ENCMODE_MIME)
    {
        // $$$ decode the MIME ?= encoding
    }

    /* ensure that the buffer has only well-formed UTF-8 characters */
    CCharmapToUni::validate(buf, bufend - buf);
}

/* add a parsed query string element to a lookup table */
static void add_parsed_val(VMG_ CVmObjLookupTable *tab, const vm_val_t *key,
                           const char *str, size_t len, int encmode)
{
    /* stack the key for gc protection */
    G_stk->push(key);

    /* create a parsed string value to add to the table */
    vm_val_t val;
    new_parsed_str(vmg_ &val, str, len, encmode);
    G_stk->push(&val);

    /* add or extend the table entry */
    vm_val_t oldval;
    const char *oldstr;
    if (tab->index_check(vmg_ &oldval, key)
        && (oldstr = oldval.get_as_string(vmg0_)) != 0)
    {
        /* 
         *   it's already in the table - make it a comma-separated list by
         *   appending ", val" to the existing key 
         */

        /* get the parsed string we're adding */
        const char *newstr = val.get_as_string(vmg0_);

        /* allocate the combined string as len1 + len2 + 2 (for the ", ") */
        vm_val_t cval;
        size_t clen = vmb_get_len(oldstr) + vmb_get_len(newstr) + 2;
        cval.set_obj(CVmObjString::create(vmg_ FALSE, clen));
        CVmObjString *cstr = (CVmObjString *)vm_objp(vmg_ cval.val.obj);

        /* build the new string */
        t3sprintf(cstr->cons_get_buf(), clen, "%.*s, %.*s",
                  (int)vmb_get_len(oldstr), oldstr + VMB_LEN,
                  (int)vmb_get_len(newstr), newstr + VMB_LEN);

        /* use the combined string as the value */
        G_stk->discard();
        val = cval;
        G_stk->push(&val);
    }

    /* it's not in the table yet - add it */
    tab->set_or_add_entry(vmg_ key, &val);

    /* discard the gc protection */
    G_stk->discard(2);
}

/* add a parsed query string element under an integer key */
static void add_parsed_val(VMG_ CVmObjLookupTable *tab, int key,
                           const char *str, size_t len, int encmode)
{
    /* set up a vm_val_t with the integer key value */
    vm_val_t keyval;
    keyval.set_int(key);

    /* add the value to the table */
    add_parsed_val(vmg_ tab, &keyval, str, len, encmode);
}

/* add a parsed query string element under a string key */
static void add_parsed_val(VMG_ CVmObjLookupTable *tab,
                           const char *keystr, size_t keylen,
                           const char *str, size_t len,
                           int encmode)
{
    /* set up the key as a parsed string */
    vm_val_t key;
    new_parsed_str(vmg_ &key, keystr, keylen, encmode);

    /* add the value */
    add_parsed_val(vmg_ tab, &key, str, len, encmode);
}

/*
 *   Parse query parameters.  This can be used for the part of a URL query
 *   string after the first '?', or for a message body with content-type
 *   application/x-www-form-urlencoded, since they have identical formats.  
 */
static void parse_query_params(VMG_ CVmObjLookupTable *tab, const char *p,
                               int encmode)
{
    /* parse the arguments */
    while (*p != '\0')
    {
        /* the end of this argument is at the next '&' */
        const char *amp = strchr(p, '&');
        if (amp == 0)
            amp = p + strlen(p);

        /* find the '=', if any */
        const char *eq = strchr(p, '=');
        if (eq == 0 || eq > amp)
            eq = amp;

        /* add the resource name under the name key */
        add_parsed_val(vmg_ tab, p, eq - p,
                       eq + 1, eq == amp ? 0 : amp - eq - 1, encmode);

        /* move on to the next delimiter */
        p = amp;
        if (*p != '\0')
            ++p;
    }
}

/*
 *   parse the query string into the resource name and parameter list
 */
int CVmObjHTTPRequest::getp_parseQuery(VMG_ vm_obj_id_t self,
                                       vm_val_t *retval, uint *oargc)
{
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* retrieve the query string from the request */
    TadsHttpRequest *req = get_ext()->req;
    const char *q = req->resource_name->get();
    size_t ql = strlen(q);

    /* create a lookup table to hold the result; push it for gc protection */
    retval->set_obj(CVmObjLookupTable::create(vmg_ FALSE, 16, 32));
    G_stk->push(retval);
    CVmObjLookupTable *tab =
        (CVmObjLookupTable *)vm_objp(vmg_ retval->val.obj);

    /* 
     *   the resource name is the part of the query up to the first '?', or
     *   the entire string if there's no '?' 
     */
    const char *qq = strchr(q, '?');
    if (qq == 0)
        qq = q + ql;

    /* add the resource name under key [1] */
    add_parsed_val(vmg_ tab, 1, q, qq - q, ENCMODE_URL);

    /* parse the query parameters, starting after the '?' */
    if (*qq == '?')
        parse_query_params(vmg_ tab, qq + 1, ENCMODE_URL);

    /* done with the gc protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   parse the query string and return a specified parametre
 */
int CVmObjHTTPRequest::getp_getQueryParam(VMG_ vm_obj_id_t self,
                                          vm_val_t *retval, uint *oargc)
{
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the target parameter name */
    const char *paramName = G_stk->get(0)->get_as_string(vmg0_);
    if (paramName == 0)
        err_throw(VMERR_STRING_VAL_REQD);

    /* get the length and buffer from the string */
    size_t paramLen = vmb_get_len(paramName);
    paramName += VMB_LEN;

    /* retrieve the query string from the request */
    TadsHttpRequest *req = get_ext()->req;
    const char *q = req->resource_name->get();
    size_t ql = strlen(q);

    /* presume we won't find the parameter we're looking for */
    retval->set_nil();

    /* scan the parameters, if any */
    for (const char *qq = strchr(q, '?') ; qq != 0 && *qq != '\0' ; )
    {
        /* skip this separator character */
        ++qq;

        /* find the end of this parameter */
        const char *amp = strchr(qq, '&');
        if (amp == 0)
            amp = q + ql;

        /* find the value portion, if present */
        const char *eq = strchr(qq, '=');
        if (eq == 0 || eq > amp)
            eq = amp;

        /* 
         *   Compare this to the name we're looking for.  Note that URLs are
         *   either plain ASCII or UTF-8, so we can just do the comparison on
         *   a byte-by-byte basis as our paramName string is also UTF-8.  
         */
        if ((size_t)(eq - qq) == paramLen
            && memcmp(qq, paramName, paramLen) == 0)
        {
            /* this is the one - create the value string */
            new_parsed_str(vmg_ retval, eq + 1,
                           eq == amp ? 0 : amp - eq - 1, ENCMODE_URL);

            /* done with our search */
            break;
        }

        /* move on to the next separator */
        qq = amp;
    }

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/*
 *   get the headers
 */
int CVmObjHTTPRequest::getp_getHeaders(VMG_ vm_obj_id_t self,
                                       vm_val_t *retval, uint *oargc)
{
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* retrieve the headers from the request */
    TadsHttpRequest *req = get_ext()->req;
    TadsHttpRequestHeader *hdrs = req->hdr_list;

    /* create a lookup table to hold the result; push it for gc protection */
    retval->set_obj(CVmObjLookupTable::create(vmg_ FALSE, 32, 64));
    G_stk->push(retval);
    CVmObjLookupTable *tab =
        (CVmObjLookupTable *)vm_objp(vmg_ retval->val.obj);

    /* 
     *   The first item in the list is the special "request line," with the
     *   HTTP verb and resource name; it's not really a header, as it doesn't
     *   have a name/value pair, so we file it under the key [1]. 
     */
    add_parsed_val(vmg_ tab, 1, hdrs->name, strlen(hdrs->name), ENCMODE_NONE);

    /* add the rest as key/value pairs */
    for (hdrs = hdrs->nxt ; hdrs != 0 ; hdrs = hdrs->nxt)
        add_parsed_val(vmg_ tab, hdrs->name, strlen(hdrs->name),
                       hdrs->value, strlen(hdrs->value), ENCMODE_NONE);

    /* done with the gc protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   get an individual cookie
 */
int CVmObjHTTPRequest::getp_getCookie(VMG_ vm_obj_id_t self,
                                      vm_val_t *retval, uint *oargc)
{
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* retrieve the headers from the request */
    TadsHttpRequest *req = get_ext()->req;
    TadsHttpRequestHeader *hdrs = req->hdr_list;

    /* get the cookie name argument */
    const char *cname = G_stk->get(0)->get_as_string(vmg0_);
    if (cname == 0)
        err_throw(VMERR_BAD_TYPE_BIF);

    /* presume we won't find a match */
    retval->set_nil();

    /* scan for "Cookie" headers */
    for (hdrs = hdrs->nxt ; hdrs != 0 ; hdrs = hdrs->nxt)
    {
        /* if this is a Cookie: header, process it */
        if (stricmp(hdrs->name, "cookie") == 0)
        {
            /* scan the list of cookie name=value pairs */
            for (const char *p = hdrs->value ; *p != '\0' ; )
            {
                /* skip leading whitespace */
                for ( ; isspace(*p) ; ++p) ;

                /* the cookie name ends at the '=', ';', ',', or null */
                const char *name = p;
                for ( ; *p != '\0' && strchr("=;,", *p) == 0 ; ++p) ;
                size_t namelen = p - name;

                /* if we found the '=', scan the value */
                const char *val = "";
                size_t vallen = 0;
                if (*p == '=')
                {
                    /* skip the '=' and scan for the end of the cookie */
                    for (val = ++p ; *p != '\0' && strchr(" \t;,", *p) == 0 ;
                         ++p) ;
                    vallen = p - val;
                }

                /* if this matches the name we're looking for, return it */
                if (namelen == vmb_get_len(cname)
                    && memcmp(name, cname + VMB_LEN, namelen) == 0)
                {
                    /* it's a match - return the value as a string */
                    new_parsed_str(vmg_ retval, val, vallen, ENCMODE_NONE);

                    /* stop looking */
                    goto done;
                }
                
                /* scan ahead to the next non-delimiter */
                for ( ; *p != '\0' && strchr(" \t;,", *p) != 0 ; ++p) ;
            }
        }
    }

done:
    /* done with the arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/*
 *   get the cookies
 */
int CVmObjHTTPRequest::getp_getCookies(VMG_ vm_obj_id_t self,
                                       vm_val_t *retval, uint *oargc)
{
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* retrieve the headers from the request */
    TadsHttpRequest *req = get_ext()->req;
    TadsHttpRequestHeader *hdrs = req->hdr_list;

    /* create a lookup table to hold the result; push it for gc protection */
    retval->set_obj(CVmObjLookupTable::create(vmg_ FALSE, 32, 64));
    G_stk->push(retval);
    CVmObjLookupTable *tab =
        (CVmObjLookupTable *)vm_objp(vmg_ retval->val.obj);

    /* scan for "Cookie" headers */
    for (hdrs = hdrs->nxt ; hdrs != 0 ; hdrs = hdrs->nxt)
    {
        /* if this is a Cookie: header, process it */
        if (stricmp(hdrs->name, "cookie") == 0)
        {
            /* scan the list of cookie name=value pairs */
            for (const char *p = hdrs->value ; *p != '\0' ; )
            {
                /* skip leading whitespace */
                for ( ; isspace(*p) ; ++p) ;

                /* the cookie name ends at the '=', ';', ',', or null */
                const char *name = p;
                for ( ; *p != '\0' && strchr("=;,", *p) == 0 ; ++p) ;
                size_t namelen = p - name;

                /* if we found the '=', scan the value */
                const char *val = "";
                size_t vallen = 0;
                if (*p == '=')
                {
                    /* skip the '=' and scan for the end of the cookie */
                    for (val = ++p ; *p != '\0' && strchr(" \t;,", *p) == 0 ;
                         ++p) ;
                    vallen = p - val;
                }

                /* add this cookie to the table */
                add_parsed_val(vmg_ tab, name, namelen, val, vallen,
                               ENCMODE_NONE);

                /* scan ahead to the next non-delimiter */
                for ( ; *p != '\0' && strchr(" \t;,", *p) != 0 ; ++p) ;
            }
        }
    }

    /* done with the gc protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   Find an attribute token in a content-type or similar header string.  An
 *   attribute token is separated from preceding text by a ';', and has the
 *   form "name=value".  The value can be in quotes, or can just be a token.
 */
static int parse_attr_token(const char *str, const char *attr_name,
                            const char *&val, size_t &vallen)
{
    /* assume we won't find anything */
    val = 0;
    vallen = 0;

    /* note the name length */
    size_t attr_name_len = strlen(attr_name);

    /* scan the string for attribute tokens, starting at the first ';' */
    for (const char *p = strchr(str, ';') ; p != 0 ; p = strchr(p, ';'))
    {
        /* skip spaces */
        for (++p ; isspace(*p) ; ++p) ;

        /* check to see if this is the desired attribute name */
        if (memicmp(p, attr_name, attr_name_len) != 0)
            continue;

        /* skip the name and any subsequent spaces, and look for the '=' */
        for (p += attr_name_len ; isspace(*p) ; ++p) ;
        if (*p != '=')
            continue;

        /* skip the '=' and subsequent spaces */
        for (++p ; isspace(*p) ; ++p) ;

        /* 
         *   Okay, we're at the attribute value.  If there's a quote mark,
         *   the value is everything from here to the close quote.  Otherwise
         *   it's everything up to the next space or semicolon.  
         */
        if (*p == '"')
        {
            /* skip the open quote, and find the close quote */
            for (val = ++p ; *p != '\0' && *p != '"' ; ++p) ;
        }
        else
        {
            /* it's from here to the next separator character */
            val = p;
            for ( ; *p != '\0' && strchr(" \t;,", *p) == 0 ; ++p) ;
        }

        /* note the length, and return success */
        vallen = p - val;
        return TRUE;
    }

    /* didn't find it - return failure */
    return FALSE;
}

/*
 *   Match two attribute tokens 
 */
static int match_attrs(const char *a, size_t alen,
                       const char *b, size_t blen)
{
    /* if the lengths don't match, it's not a match */
    if (alen != blen)
        return FALSE;

    /* if both are empty strings, it's a match */
    if (alen == 0 && blen == 0)
        return TRUE;

    /* compare the strings */
    return memcmp(a, b, alen) == 0;
}

/*
 *   send a cookie header
 */
int vm_httpreq_cookie::send(TadsServerThread *t)
{
    /* send the Set-Cookie header */
    return (t->send("Set-Cookie: ")
            && t->send(name)
            && t->send("=")
            && t->send(val)
            && t->send("\r\n"));
}

/*
 *   set a cookie in the reply
 */
int CVmObjHTTPRequest::getp_setCookie(VMG_ vm_obj_id_t self,
                                      vm_val_t *retval, uint *oargc)
{
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(2);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the cookie name and value */
    const char *cname = G_stk->get(0)->get_as_string(vmg0_);
    const char *cval = G_stk->get(1)->get_as_string(vmg0_);

    /* check that we got string values */
    if (cname == 0 || cval == 0)
        err_throw(VMERR_BAD_TYPE_BIF);

    /* get the name and value length and buffer */
    size_t cname_len = vmb_get_len(cname), cval_len = vmb_get_len(cval);
    cname += VMB_LEN;
    cval += VMB_LEN;

    /* get a null-terminated copy of the value, and parse out the 'path' */
    char *cbuf = lib_copy_str(cval, cval_len);
    const char *cpath, *cdom;
    size_t cpathlen, cdomlen;
    parse_attr_token(cbuf, "path", cpath, cpathlen);
    parse_attr_token(cbuf, "domain", cdom, cdomlen);

    /* search for an existing cookie with this name */
    vm_httpreq_cookie *cookie;
    for (cookie = get_ext()->cookies ; cookie != 0 ; cookie = cookie->nxt)
    {
        /* check for a match */
        if (strlen(cookie->name) == cname_len
            && memcmp(cookie->name, cname, cname_len) == 0)
        {
            /* parse the current value's path and domain attributes */
            const char *vpath, *vdom;
            size_t vpathlen, vdomlen;
            parse_attr_token(cookie->val, "path", vpath, vpathlen);
            parse_attr_token(cookie->val, "domain", vdom, vdomlen);

            /*   
             *   Check to see if the path and domain match.  If not, this
             *   counts as a distinct cookie.  
             */
            if (!match_attrs(vpath, vpathlen, cpath, cpathlen)
                || !match_attrs(vdom, vdomlen, cdom, cdomlen))
                continue;

            /* it's already defined - redefine it with the new value */
            lib_free_str(cookie->val);
            cookie->val = lib_copy_str(cval, cval_len);
            break;
        }
    }

    /* done with the allocated value copy */
    lib_free_str(cbuf);

    /* if we didn't replace an existing entry, create a new one */
    if (cookie == 0)
    {
        /* create the entry */
        cookie = new vm_httpreq_cookie(cname, cname_len, cval, cval_len);

        /* link it into our list */
        cookie->nxt = get_ext()->cookies;
        get_ext()->cookies = cookie;
    }

    /* done with the arguments */
    G_stk->discard(argc);

    /* no return value */
    retval->set_nil();

    /* handled */
    return TRUE;
}

/*
 *   Find the charset property in a Content-Type header.  Fills in 'cs' with
 *   a mapper object if we find a text content type, otherwise nil.  In the
 *   nil case we set cs->val.obj = VM_INVALID_OBJ, so you can use that field
 *   where a vm_obj_id_t is required in either case.  We return the file mode
 *   to use for a file opened on this content: VMOBJFILE_MODE_TEXT if we find
 *   a character set, VMOBJFILE_MODE_RAW if not.  
 */
static int get_charset_map(VMG_ const char *ct, vm_val_t *cs)
{
    /* presume we won't create a mapper */
    cs->set_nil();
    cs->val.obj = VM_INVALID_OBJ;

    /* get the Content-Type header */
    if (ct != 0
        && (memcmp(ct, "text/", 5) == 0
            || memcmp(ct, "application/x-www-form-urlencoded", 33) == 0))
    {
        /* 
         *   It's a text content type, so we'll use a text mapping.  Scan the
         *   content-type string for a "charset" parameter.  
         */
        const char *cname;
        size_t cnamelen;
        if (parse_attr_token(ct, "charset", cname, cnamelen))
        {
            /* got it - try loading the character mapper for the set name */
            cs->set_obj(CVmObjCharSet::create(vmg_ FALSE, cname, cnamelen));
        }

        /* if we didn't create a mapper, or it's not valid, use ASCII */
        if (cs->typ == VM_NIL
            || ((CVmObjCharSet *)vm_objp(vmg_ cs->val.obj))
               ->get_to_uni(vmg0_) == 0)
        {
            /* create a us-ascii mapper */
            cs->set_obj(CVmObjCharSet::create(vmg_ FALSE, "us-ascii", 8));
        }

        /* indicate that they should use text mode on the file */
        return VMOBJFILE_MODE_TEXT;
    }
    else
    {
        /* it's not a text content type - use raw mode */
        return VMOBJFILE_MODE_RAW;
    }
}

/*
 *   StringRef data source.  This is a simple subclass of the basic string
 *   data source; the only specialization is that we keep a reference to the
 *   StringRef object, and release it on close. 
 */
class CVmStringRefSource: public CVmStringSource
{
public:
    CVmStringRefSource(StringRef *s)
        : CVmStringSource(s->get(), s->getlen())
    {
        /* keep a reference on the source */
        this->s = s;
        s->add_ref();
    }

    CVmStringRefSource(StringRef *s, long start_ofs, long len)
        : CVmStringSource(s->get() + start_ofs, len)
    {
        /* keep a reference on the source */
        this->s = s;
        s->add_ref();
    }

    virtual CVmDataSource *clone(VMG_ const char * /*mode*/)
        { return new CVmStringRefSource(s, mem - s->get(), memlen); }

    ~CVmStringRefSource()
    {
        /* release our reference on the underlying source */
        s->release_ref();
    }

protected:
    /* our underlying StringRef object */
    StringRef *s;
};

/*
 *   throw a network error 
 */
static void throw_net_err(VMG_ const char *msg, int sock_err)
{
    /* 
     *   Determine the exception class, based on the error code:
     *   
     *.  - ECONNRESET -> SocketDisconnectException
     *.  - everything else -> the base NetException
     */
    vm_obj_id_t cl = VM_INVALID_OBJ;
    switch (sock_err)
    {
    case OS_ECONNRESET:
    case OS_ECONNABORTED:
        cl = G_predef->net_reset_exception;
        break;
    }

    /* 
     *   if we didn't pick a special class for the error, or the class we
     *   selected wasn't imported, use the base NetException class by default
     */
    if (cl == VM_INVALID_OBJ)
        cl = G_predef->net_exception;

    /* throw new exceptionClass(msg, errno) */
    int argc = 1;
    if (sock_err != 0)
    {
        G_stk->push_int(vmg_ sock_err);
        ++argc;
    }
    G_interpreter->push_string(vmg_ msg);
    G_interpreter->throw_new_class(vmg_ cl, argc, msg);
}

/*
 *   Seek the next multi-part boundary 
 */
static const char *next_boundary(const char *startp, const char *endp,
                                 const char *b, size_t blen, int need_crlf)
{
    /* scan for the next boundary */
    for (const char *p = startp ; p + 2 + blen <= endp ; ++p)
    {
        /* check for a match */
        if (p[0] != '-' || p[1] != '-' || memcmp(p+2, b, blen) != 0)
            continue;

        /* if we need a newline prefix, check that it's there */
        if (need_crlf && (p < startp + 2 || p[-2] != '\r' || p[-1] != '\n'))
            continue;

        /* it's a match - return the part after the '--' prefix */
        return p + 2;
    }

    /* didn't find it */
    return 0;
}

/*
 *   Parse multi-part data.
 */
static void parse_multipart(VMG_ CVmObjLookupTable *tab, StringRef *body,
                            const char *ct)
{
    /* 
     *   find the "boundary" attribute in the content-type string; if it's
     *   not present, we can't go on, since this is required to parse the
     *   content sections 
     */
    const char *bndry;
    size_t bndry_len;
    if (!parse_attr_token(ct, "boundary", bndry, bndry_len))
        return;

    /* scan the sections */
    const char *p = body->get();
    const char *endp = p + body->getlen();
    for (p = next_boundary(p, endp, bndry, bndry_len, FALSE) ; p != 0 ; )
    {
        /* no error for the section yet */
        const char *errmsg = 0;

        /* skip ahead to the end of the boundary string */
        p += bndry_len;

        /* if we're at a '--', we're done */
        if (*p == '-' && *(p+1) == '-')
            break;

        /* otherwise, we should be at a newline; if not, give up */
        for ( ; *p == ' ' || *p == '\t' ; ++p) ;
        if (*p++ != '\r' || *p++ != '\n')
            break;

        /* 
         *   Parse the headers for this section.  To do this, we'll need to
         *   make a separate writable copy of the header section - scan ahead
         *   for the double CR-LF.
         */
        const char *hdr_start = p;
        if ((p = strstr(p, "\r\n\r\n")) == 0)
            break;

        /* skip the double CR-LF to get to the section body */
        const char *sbody = (p += 4);

        /* find the boundary at the end of this section */
        if ((p = next_boundary(p, endp, bndry, bndry_len, TRUE)) == 0)
            break;

        /* 
         *   Note the length of the body - if exclude.  The body excludes the
         *   CR-LF and the "--" preceding the boundary marker, so deduct four
         *   bytes from the current position.  
         */
        long sbody_len = p - sbody - 4;

        /* make a copy of the header section */
        StringRef *hdr_copy = new StringRef(hdr_start, sbody - 4 - hdr_start);

        /* parse the headers */
        TadsHttpRequestHeader *hdrs = 0, *hdrs_tail = 0;
        TadsHttpRequestHeader::parse_headers(
            hdrs, hdrs_tail, TRUE, hdr_copy, 0);

        /* get the content-disposition and content-type */
        const char *scd = hdrs->find("content-disposition");
        const char *sct = hdrs->find("content-type");
        const char *fldname, *filename;
        size_t fldname_len, filename_len;
        vm_val_t fldval, keyval;
        int has_fname;

        /* 
         *   Use text/plain as the default content type.  Since we send text
         *   out in utf-8 mode, assume that responses come back in utf-8
         *   mode. 
         */
        if (sct == 0)
            sct = "text/plain; charset=utf-8";

        /* make sure the content-disposition is "form-data" */
        if (scd == 0 || memcmp(scd, "form-data", 9) != 0)
            goto section_done;

        /* 
         *   Get the "name" attribute - this is <INPUT> field's NAME value.
         *   This is the table key for this section, so if it's not set we
         *   can't do anything with this section. 
         */
        if (!parse_attr_token(scd, "name", fldname, fldname_len))
            goto section_done;

        /* 
         *   Get the "filename" attribute.  If this is an uploaded file, this
         *   gives the name of the client-side file being uploaded.  If it's
         *   not present, this is a data <INPUT> field rather than a file
         *   upload, so the content body is simply the field value.  
         *   
         *   If the filename is empty and the file content has zero length,
         *   it means that the user didn't select a file in this field.  In
         *   this case, use nil in place of the FileUpload object.  
         */
        has_fname = parse_attr_token(scd, "filename", filename, filename_len);
        if (has_fname && filename_len != 0 && sbody_len != 0)
        {
            /* 
             *   There's a "filename" attribute, so this is a file upload.
             *   We'll represent this as a FileUpload object, with a
             *   string-sourced File representing the body.
             *   
             *   If it's a text content type, we'll open the file in Text
             *   mode with the character mapping specifed in the content type
             *   parameter.  Otherwise, we'll open it in raw mode.  Start by
             *   checking the mode implied by the content type parameter.  
             */
            vm_val_t charset;
            int mode = get_charset_map(vmg_ sct, &charset);
            int textmode = (mode == VMOBJFILE_MODE_TEXT);

            /* save the character set for gc protection */
            G_stk->push(&charset);

            /* create the memory data source for the file */
            CVmStringRefSource *ds = new CVmStringRefSource(
                body, sbody - body->get(), sbody_len);

            /* create the file object */
            vm_val_t file;
            file.set_obj(CVmObjFile::create(
                vmg_ FALSE, 0, textmode ? charset.val.obj : VM_INVALID_OBJ,
                ds, mode, VMOBJFILE_ACCESS_READ, textmode));

            /* 
             *   discard the gc protection for the character set (it's safely
             *   ensconced in the file now), and push the new file object 
             */
            G_stk->discard();
            G_stk->push(&file);

            /* make sure the FileUpload is defined */
            if (G_predef->file_upload_cl == VM_INVALID_OBJ)
            {
                errmsg = "Parsing form fields: FileUpload class is missing";
                goto section_done;
            }

            /* call FileUpload(file, contentType, filename) */
            G_interpreter->push_string(vmg_ filename, filename_len);
            G_interpreter->push_string(vmg_ sct);
            G_interpreter->push(&file);
            vm_objp(vmg_ G_predef->file_upload_cl)->create_instance(
                vmg_ G_predef->file_upload_cl, 0, 3);

            /* the new object is in R0 */
            fldval = *G_interpreter->get_r0();

            /* discard the earlier gc protection; push the field value */
            G_stk->discard(2);
            G_stk->push(&fldval);
        }
        else if (has_fname)
        {
            /*
             *   We have a filename attribute, but it's empty, and there's no
             *   content body.  Use nil as the value.  
             */
            fldval.set_nil();
            G_stk->push(&fldval);
        }
        else
        {
            /* 
             *   There's no filename attribute, so this is a simple data
             *   field, with the section content body as the field value.  
             */
            new_parsed_str(vmg_ &fldval, sbody, sbody_len, ENCMODE_NONE);
            G_stk->push(&fldval);
        }

        /* create the field name string as the table key */
        new_parsed_str(vmg_ &keyval, fldname, fldname_len, ENCMODE_MIME);
        G_stk->push(&keyval);

        /* save the key/value pair in the table */
        tab->set_or_add_entry(vmg_ &keyval, &fldval);

        /* discard gc protection */
        G_stk->discard(2);

    section_done:
        /* free the headers */
        delete hdrs;
        hdr_copy->release_ref();

        /* if there's an error, throw it */
        if (errmsg != 0)
            throw_net_err(vmg_ errmsg, 0);
    }
}


/*
 *   get the posted form fields
 */
int CVmObjHTTPRequest::getp_getFormFields(VMG_ vm_obj_id_t self,
                                          vm_val_t *retval, uint *oargc)
{
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* retrieve the headers from the request */
    TadsHttpRequest *req = get_ext()->req;
    TadsHttpRequestHeader *hdrs = req->hdr_list;
    StringRef *body = req->body;

    /* if the body was too large, return 'overflow' */
    if (req->overflow)
    {
        retval->set_obj(CVmObjString::create(vmg_ FALSE, "overflow", 8));
        return TRUE;
    }

    /* if there's no message body, there are no form fields */
    if (body == 0)
    {
        retval->set_nil();
        return TRUE;
    }

    /* find the content type header */
    const char *ct = hdrs->find("content-type");
    int typ;
    if (ct != 0)
    {
        /* check what we have */
        if (memicmp(ct, "application/x-www-form-urlencoded", 33) == 0)
            typ = 0;
        else if (memicmp(ct, "multipart/form-data", 19) == 0)
            typ = 1;
        else
        {
            /* it's not a posted format we recognize - return nil */
            retval->set_nil();
            return TRUE;
        }
    }

    /* create a lookup table to hold the result; push it for gc protection */
    retval->set_obj(CVmObjLookupTable::create(vmg_ FALSE, 32, 64));
    G_stk->push(retval);
    CVmObjLookupTable *tab =
        (CVmObjLookupTable *)vm_objp(vmg_ retval->val.obj);

    /* parse the content type that we found */
    if (typ == 0)
    {
        /* 
         *   URL-encoded form data.  This is the same format as a URL query
         *   string, so we can just apply that parser to the message body. 
         */
        parse_query_params(vmg_ tab, body->get(), ENCMODE_FORM);
    }
    else if (typ == 1)
    {
        /* 
         *   Multipart form-data.  Invoke our generic multipart parser.  
         */
        parse_multipart(vmg_ tab, body, ct);
    }

    /* done with the gc protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   get the request body 
 */
int CVmObjHTTPRequest::getp_getClientAddress(VMG_ vm_obj_id_t self,
                                             vm_val_t *retval, uint *oargc)
{
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* retrieve the request */
    TadsHttpRequest *req = get_ext()->req;
    TadsServerThread *t = req->thread;

    /* set up the return list, using address information from the thread */
    G_interpreter->push_int(vmg_ t->get_client_port());
    G_interpreter->push_string(vmg_ t->get_client_ip());
    retval->set_obj(CVmObjList::create_from_stack(vmg_ 0, 2));
    
    /* handled */
    return TRUE;
}

/*
 *   get the request body
 */
int CVmObjHTTPRequest::getp_getBody(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *oargc)
{
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* retrieve the headers from the request */
    TadsHttpRequest *req = get_ext()->req;
    TadsHttpRequestHeader *hdrs = req->hdr_list;
    StringRef *body = req->body;

    /* if the body was too large, return 'overflow' */
    if (req->overflow)
    {
        retval->set_obj(CVmObjString::create(vmg_ FALSE, "overflow", 8));
        return TRUE;
    }

    /* if there's no body, return nil */
    if (body == 0)
    {
        retval->set_nil();
        return TRUE;
    }

    /* create a data source for the body */
    CVmStringRefSource *sbody = new CVmStringRefSource(body);

    /* for a text content type, get the character mapper */
    vm_val_t charset;
    int mode = get_charset_map(vmg_ hdrs->find("content-type"), &charset);
    G_stk->push(&charset);

    /* create a File object to access the body data source */
    retval->set_obj(CVmObjFile::create(
        vmg_ FALSE, 0, charset.val.obj, sbody, mode,
        VMOBJFILE_ACCESS_READ, TRUE));

    /* discard the gc protection */
    G_stk->discard();
                                       
    /* handled */
    return TRUE;
}

/*
 *   look up a standard status code by number 
 */
static const char *look_up_status(int n, size_t &len)
{
    /* message table for the standard status codes */
    static struct
    {
        int n;
        const char *msg;
    } msgs[] =
    {
        { 100, "100 Continue" },
        { 101, "101 Switching Protocols" },
        { 102, "102 Processing" },
        { 200, "200 OK" },
        { 201, "201 Created" },
        { 202, "202 Accepted" },
        { 203, "203 Non-Authoritative Information" },
        { 204, "204 No Content" },
        { 205, "205 Reset Content" },
        { 206, "206 Partial Content" },
        { 207, "207 Multi-Status" },
        { 300, "300 Multiple Choices" },
        { 301, "301 Moved Permanently" },
        { 302, "302 Found" },
        { 303, "303 See Other" },
        { 304, "304 Not Modified" },
        { 305, "305 Use Proxy" },
        { 306, "306 Switch Proxy" },
        { 307, "307 Temporary Redirect" },
        { 400, "400 Bad Request" },
        { 401, "401 Unauthorized" },
        { 402, "402 Payment Required" },
        { 403, "403 Forbidden" },
        { 404, "404 Not Found" },
        { 405, "405 Method Not Allowed" },
        { 406, "406 Not Acceptable" },
        { 407, "407 Proxy Authentication Required" },
        { 408, "408 Request Timeout" },
        { 409, "409 Conflict" },
        { 410, "410 Gone" },
        { 411, "411 Length Required" },
        { 412, "412 Precondition Failed" },
        { 413, "413 Request Entity Too Large" },
        { 414, "414 Request-URI Too Long" },
        { 415, "415 Unsupported Media Type" },
        { 416, "416 Requested Range Not Satisfiable" },
        { 417, "417 Expectation Failed" },
        { 418, "418 I'm a teapot" },
        { 421, "421 There are too many connections from your internet address" },
        { 422, "422 Unprocessable Entity" },
        { 423, "423 Locked" },
        { 424, "424 Failed Dependency" },
        { 425, "425 Unordered Collection" },
        { 426, "426 Upgrade Required" },
        { 449, "449 Retry With" },
        { 450, "450 Blocked by Windows Parental Controls" },
        { 500, "500 Internal Server Error" },
        { 501, "501 Not Implemented" },
        { 502, "502 Bad Gateway" },
        { 503, "503 Service Unavailable" },
        { 504, "504 Gateway Timeout" },
        { 505, "505 HTTP Version Not Supported" },
        { 506, "506 Variant Also Negotiates" },
        { 507, "507 Insufficient Storage" },
        { 509, "509 Bandwidth Limit Exceeded" },
        { 510, "510 Not Extended" },
        { 530, "530 User access denied" }
    };

    /* search the list */
    for (int i = 0 ; i < countof(msgs) ; ++i)
    {
        /* check for a match to the given status code */
        if (msgs[i].n == n)
        {
            /* it's a match - set the length and return the message */
            len = strlen(msgs[i].msg);
            return msgs[i].msg;
        }
    }

    /* didn't find a match - throw an error */
    err_throw(VMERR_BAD_VAL_BIF);
    AFTER_ERR_THROW(return 0);
}

/*
 *   look up an HTTP status message by code number 
 */
static const char *get_status_arg(VMG_ int body_status_code,
                                  int argn, int argc, size_t &len)
{
    /* if a body status code was given, it overrides the status argument */
    if (body_status_code != 0)
        return look_up_status(body_status_code, len);

    /* if the argument isn't present, use the default "200 OK" */
    if (argn >= argc)
    {
        len = 6;
        return "200 OK";
    }
    
    /* check the type of the argument value */
    const vm_val_t *argp = G_stk->get(argn);
    const char *status;
    switch (argp->typ)
    {
    case VM_INT:
        /* it's an integer - look up the full message */
        return look_up_status(argp->val.intval, len);

    case VM_NIL:
        /* nil - use the default 200 */
        len = 6;
        return "200 OK";
        
    default:
        /* it's not an integer, so it has to be a string */
        if ((status = argp->get_as_string(vmg0_)) == 0)
            err_throw(VMERR_STRING_VAL_REQD);
        
        /* get the length and return the buffer pointer */
        len = vmb_get_len(status);
        return status + VMB_LEN;
    }
}
    
/*
 *   send headers 
 */
static void send_custom_headers(VMG_ TadsServerThread *t,
                                const vm_val_t *headers)
{
    /* send each header */
    int cnt = headers->ll_length(vmg0_);
    for (int i = 1 ; i <= cnt ; ++i)
    {
        /* get this list element */
        vm_val_t ele;
        headers->ll_index(vmg_ &ele, i);

        /* it has to be a string */
        const char *h = ele.get_as_string(vmg0_);
        if (h == 0)
            err_throw(VMERR_STRING_VAL_REQD);

        /* send the header string and the CR-LF */
        if (!t->send(h + VMB_LEN, vmb_get_len(h)) || !t->send("\r\n"))
            throw_net_err(vmg_ "HTTPRequest: error sending headers",
                          t->last_error());
    }
}

/* error info for HTTPReplySender::send() */
struct http_reply_err
{
    /* OS_Exxx error number (see osifcnet.h) */
    int sock_err;

    /* descriptive message (as a static const string - not allocated) */
    const char *msg;

    /* set the error - returns FALSE status code for caller to return */
    int set(int err, const char *msg)
    {
        this->sock_err = err;
        this->msg = msg;
        return FALSE;
    }

    int set(TadsServerThread *t, const char *msg)
        { return set(t->last_error(), msg); }
};

/*
 *   Body argument descriptor.  This handles the various different source
 *   types we handle for body values - strings, ByteArrays, StringBuffers. 
 */
struct bodyArg
{
    /* load from the stack */
    bodyArg(VMG_ int argn)
    {
        /* first clear everything out */
        none = FALSE;
        str = 0;
        stralo = FALSE;
        strbuf = 0;
        bytarr = 0;
        len = 0;
        status_code = 0;
        init_err = 0;
        datasource = 0;
        gv = 0;

        /* get the argument value pointer out of the stack */
        srcval = *G_stk->get(argn);

        /* try the various types */
        if ((str = srcval.get_as_string(vmg0_)) != 0)
        {
            /* it's a string - get the length and buffer pointer */
            len = vmb_get_len(str);
            str += VMB_LEN;
        }
        else if (srcval.typ == VM_OBJ
                 && CVmObjByteArray::is_byte_array(vmg_ srcval.val.obj))
        {
            /* it's a byte array */
            bytarr = (CVmObjByteArray *)vm_objp(vmg_ srcval.val.obj);
            len = bytarr->get_element_count();
        }
        else if (srcval.typ == VM_OBJ
                 && CVmObjStringBuffer::is_string_buffer_obj(
                     vmg_ srcval.val.obj))
        {
            /* it's a string buffer object */
            strbuf = (CVmObjStringBuffer *)vm_objp(vmg_ srcval.val.obj);
            len = strbuf->utf8_length();
        }
        else if (srcval.typ == VM_OBJ
                 && CVmObjFile::is_file_obj(vmg_ srcval.val.obj))
        {
            /* it's a file object - cast it */
            file = (CVmObjFile *)vm_objp(vmg_ srcval.val.obj);

            /* note the size */
            len = file->get_file_size(vmg0_);

            /* 
             *   Get the data source and file spec.  Use the file spec as the
             *   source object, not the File itself; it's the spec that we
             *   want to keep around, since we're directly accessing a data
             *   source based on the spec rather than the File.
             */
            datasource = file->get_datasource();
            file->get_filespec(&srcval);

            /* it's an error if the file isn't open or valid */
            if (len < 0)
                init_err = VMERR_READ_FILE;
        }
        else if (srcval.typ == VM_INT)
        {
            /* 
             *   Integer value - treat this as an HTTP status code.  The
             *   content body is a generated HTML page for the status code. 
             */
            status_code = (int)srcval.val.intval;

            /* generate the HTML page */
            size_t len;
            const char *msg = look_up_status(status_code, len);

            /* make sure it's a valid status code */
            if (len == 0)
                err_throw(VMERR_BAD_VAL_BIF);

            /* generate the page */
            str = t3sprintf_alloc("<html><title>%s</title>"
                                  "<body><h1>%s</h1></body></html>",
                                  msg, strchr(msg, ' ') + 1);
            stralo = TRUE;
        }
        else if (srcval.typ == VM_NIL)
        {
            /* no content body */
            none = TRUE;
        }
        else
        {
            /* it's a type we can't handle */
            init_err = VMERR_BAD_TYPE_BIF;
        }
    }

    ~bodyArg()
    {
        /* free the string, if we allocated it */
        if (stralo)
            lib_free_str((char *)str);
    }

    /* is this a text resource? */
    int is_text(VMG0_)
    {
        /* check the type */
        if (str != 0 || strbuf != 0)
        {
            /* strings and string buffers are always sent as text */
            return TRUE;
        }
        else if (bytarr != 0)
        {
            /* byte arrays are always sent as binary */
            return FALSE;
        }
        else if (file != 0)
        {
            /* files are sent according to their mode */
            return file->get_file_mode(vmg0_) == VMOBJFILE_MODE_TEXT;
        }
        else
        {
            /* there shouldn't be anything else, but just in case... */
            return FALSE;
        }
    }

    /*
     *   prepare for async use 
     */
    void init_async(VMG0_)
    {
        /* if we have a data source, clone it */
        if (datasource != 0)
            datasource = datasource->clone(vmg_ "rb");

        /* 
         *   If we have a string buffer or byte array, make a private copy.
         *   These objects aren't designed to be thread-safe, so we can't
         *   access them from the background thread.
         */
        if (strbuf != 0 || bytarr != 0)
        {
            /* allocate the string buffer */
            char *buf = lib_alloc_str(len);

            /* extract the contents into the buffer */
            int32_t ofs = 0;
            extract(buf, ofs, len);
            
            /* use this as the string buffer */
            str = buf;
            stralo = TRUE;

            /* forget the original object */
            strbuf = 0;
            bytarr = 0;

            /* we don't even need gc protection for the original any more */
            srcval.set_nil();
        }

        /* 
         *   if the source value is an object, create a VM global for gc
         *   protection as long as the background thread is running 
         */
        if (srcval.typ == VM_OBJ)
        {
            gv = G_obj_table->create_global_var();
            gv->val = srcval;
        }
    }

    /* clean up from async use */
    void term_async(VMG0_)
    {
        /* delete our data source */
        if (datasource != 0)
            delete datasource;

        /* delete our VM global */
        if (gv != 0)
            G_obj_table->delete_global_var(gv);
    }

    /*
     *   Send the body data
     */
    int send(TadsServerThread *t, http_reply_err *err)
    {
        /* send the data according to the source type */
        if (str != 0)
        {
            /* send the string's contents */
            if (!t->send(str, (size_t)len))
                return err->set(t, "error sending content body");
        }
        else if (bytarr != 0 || strbuf != 0)
        {
            /* send the byte array or string buffer contents */
            char buf[1024];
            int32_t ofs = 0;
            for (;;)
            {
                /* get the next chunk */
                int32_t len = sizeof(buf);
                const char *p = extract(buf, ofs, len);

                /* if we're out of material, we're done */
                if (len == 0)
                    break;

                /* send this chunk */
                if (!t->send(p, (size_t)len))
                    return err->set(t, "error sending content body");
            }
        }
        else if (datasource != 0)
        {
            /* seek to the start of the file */
            datasource->seek(0, OSFSK_SET);

            /* copy the file to the socket */
            for (;;)
            {
                /* get the next chunk */
                char buf[1024];
                int32_t len = sizeof(buf);
                len = datasource->readc(buf, len);

                /* if we're out of material, we're done */
                if (len == 0)
                    break;

                /* send this chunk */
                if (!t->send(buf, (size_t)len))
                    return err->set(t, "error sending content body");
            }
        }

        /* success */
        return TRUE;
    }

    /* 
     *   Extract a portion of the contents.  If possible, we'll simply return
     *   a direct pointer to the buffer containing the data; if this isn't
     *   possible, we'll copy the data to the caller's buffer.
     *   
     *   On input, 'ofs' is the starting offset of the extraction.  The
     *   meaning varies according to the underlying data type, so this should
     *   always be either 0 (for the start of the string) or the output value
     *   of a previous call to this routine.  'ofs' is an in-out variable: on
     *   return, we've updated it to reflect the starting offset of the next
     *   character after the chunk we extract here.
     *   
     *   'len' is also an in-out variable.  On input, this is the size in
     *   bytes of the buffer.  On output, it reflects the actual number of
     *   bytes copied to the buffer.  This might be smaller than the
     *   requested size (never larger), because (a) we won't copy past the
     *   end of the data source, and (b) some data sources will stop short of
     *   the end of the buffer if filling the buffer exactly would require
     *   copying a fractional utf-8 character.  Note, however, that we make
     *   no guarantee that a fractional character won't be copied: some data
     *   sources won't do this, but others will.  
     */
    const char *extract(char *buf, int32_t &ofs, int32_t &len)
    {
        /* assume we'll have to copy into the caller's buffer */
        const char *ret = buf;

        /* determine what to do based on the source type */
        if (str != 0)
        {
            /* limit the length to the actual source length */
            if (len > this->len)
                len = this->len;

            /* get a pointer to the requested substring */
            ret = str + ofs;

            /* move the offset past the requested portion */
            ofs += len;
        }
        else if (strbuf != 0)
        {
            /* 
             *   for a string buffer, we have to translate to utf8 - this
             *   updates 'idx' and 'len' per our interface
             */
            strbuf->to_utf8(buf, ofs, len);
        }
        else if (bytarr != 0)
        {
            /* 
             *   It's a byte array - copy bytes to the caller's buffer.  Note
             *   that we use 0-based offsets, and the byte array uses 1-based
             *   offsets, so adjust accordingly. 
             */
            len = bytarr->copy_to_buf((unsigned char *)buf, ofs + 1, len);

            /* adjust the offset for the actual copied size */
            ofs += len;
        }
        else if (file != 0)
        {
            /* 
             *   It's a File object - read bytes from the file into the
             *   caller's buffer. 
             */
            datasource->seek(ofs, OSFSK_SET);
            len = datasource->readc(buf, len);

            /* adjust the offset for the actual copied size */
            ofs += len;
        }
        else
        {
            /* didn't read anything */
            len = 0;
        }

        /* return the result pointer */
        return ret;
    }

    /* true -> no content body */
    int none;

    /* construction error code, if applicable */
    int init_err;

    /* if it's a string, the string */
    const char *str;

    /* flag: we allocated the string */
    int stralo;

    /* if it's a string buffer, the string buffer */
    CVmObjStringBuffer *strbuf;

    /* if it's a byte array, the byte array */
    CVmObjByteArray *bytarr;

    /* if it's a File object, the File object and data source */
    CVmObjFile *file;
    CVmDataSource *datasource;

    /* source value */
    vm_val_t srcval;

    /* if it's an HTTP status code, the status code; otherwise 0 */
    int status_code;

    /* length in bytes of the underlying data source */
    int32_t len;

    /* VM global, for async use */
    vm_globalvar_t *gv;
};

/*
 *   Compare two strings, skipping spaces in the source string. 
 */
static int eq_skip_sp(const char *src, size_t len, const char *ref)
{
    /* skip leading spaces */
    utf8_ptr srcp((char *)src);
    for ( ; len != 0 && is_space(srcp.getch()) ; srcp.inc(&len)) ;

    /* keep going until we run out of one string or the other */
    for (utf8_ptr srcp((char *)src) ; len != 0 && *ref != '\0' ; ++ref)
    {
        /* 
         *   if this source character is a space, match any run of
         *   whitespace; otherwise we must match exactly 
         */
        wchar_t c = to_lower(srcp.getch());
        if (*ref == ' ' && is_space(c))
        {
            /* 
             *   we have a whitespace match - skip any additional whitespace
             *   in the source string 
             */
            for (srcp.inc(&len) ; len != 0 && is_space(srcp.getch()) ;
                 srcp.inc(&len)) ;
        }
        else if (*ref == c)
        {
            /* we have a match - skip one source character */
            srcp.inc(&len);
        }
        else
        {
            /* this character doesn't match, so the whole match fails */
            return FALSE;
        }
    }

    /* 
     *   if we ran out of the reference string, we have a match; otherwise we
     *   must have run out of the source string first, in which case we don't
     *   match because we have additional unmatched reference characters 
     */
    return (*ref == '\0');
}

/*
 *   send the cookie headers 
 */
static void send_cookies(VMG_ TadsServerThread *t, vm_httpreq_cookie *c)
{
    /* send each cookie in the list */
    for ( ; c != 0 ; c = c->nxt)
    {
        if (!c->send(t))
        {
            /* failed - throw an error */
            throw_net_err(vmg_ "error sending cookies", t->last_error());
        }
    }
}

/*
 *   send a reply 
 */
int CVmObjHTTPRequest::getp_sendReply(
    VMG_ vm_obj_id_t self, vm_val_t *retval, uint *oargc)
{
    return common_sendReply(vmg_ self, retval, oargc, TRUE);
}

/*
 *   send a reply asynchronously
 */
int CVmObjHTTPRequest::getp_sendReplyAsync(
    VMG_ vm_obj_id_t self, vm_val_t *retval, uint *oargc)
{
    return common_sendReply(vmg_ self, retval, oargc, FALSE);
}

/* reply sender thread */
class HTTPReplySenderThread: public OS_Thread
{
public:
    HTTPReplySenderThread(class HTTPReplySender *sender);
    ~HTTPReplySenderThread();

    virtual void thread_main();

    class HTTPReplySender *sender;
};

/* custom header list */
struct http_header
{
    static http_header *create(VMG_ const vm_val_t *lstval)
    {
        /* start with an empty list */
        http_header *head = 0, **tailp = &head;
        
        /* parse the value list */
        int cnt = lstval->ll_length(vmg0_);
        for (int i = 1 ; i <= cnt ; ++i)
        {
            /* get this list element */
            vm_val_t ele;
            lstval->ll_index(vmg_ &ele, i);
            
            /* it has to be a string */
            const char *h = ele.get_as_string(vmg0_);
            if (h == 0)
                err_throw(VMERR_STRING_VAL_REQD);

            /* add a new header */
            *tailp = new http_header(h + VMB_LEN, vmb_get_len(h));
            tailp = &(*tailp)->nxt;
        }

        /* return the list head */
        return head;
    }

    http_header(const char *str, size_t len)
    {
        /* allocate the string, with two extra bytes for the CR-LF */
        this->len = len + 2;
        this->str = lib_alloc_str(this->len);

        /* copy the string */
        memcpy(this->str, str, len);

        /* add the CR-LF */
        this->str[len] = '\r';
        this->str[len+1] = '\n';

        /* we're not in a list yet */
        nxt = 0;
    }

    ~http_header()
    {
        lib_free_str(str);
        if (nxt != 0)
            delete nxt;
    }

    /* string and length */
    char *str;
    size_t len;

    /* next in list */
    http_header *nxt;
};

/*
 *   Asynchronous HTTP reply result event 
 */
class TadsHttpReplyResult: public TadsEventMessage
{
public:
    TadsHttpReplyResult(VMG_ TadsHttpRequest *req, vm_globalvar_t *reqvar,
                        bodyArg *body, int sock_err, const char *msg)
        : TadsEventMessage(0)
    {
        /* save the VM globals for use in the main thread */
        this->vmg = VMGLOB_ADDR;

        /* remember our parameters */
        this->req = req;
        this->reqvar = reqvar;
        this->body = body;
        this->sock_err = sock_err;
        this->msg = lib_copy_str(msg);
    }

    ~TadsHttpReplyResult()
    {
        /* establish global variable access */
        VMGLOB_PTR(vmg);

        /* done with the request object */
        if (req != 0)
            req->release_ref();

        /* done with the request object ID global */
        if (reqvar != 0)
            G_obj_table->delete_global_var(reqvar);

        /* done with 'body' - clean up from async mode and delete it */
        if (body != 0)
        {
            body->term_async(vmg0_);
            delete body;
        }

        /* free the message text */
        lib_free_str(msg);
    }

    /* prepare a bytecode event object when we're read from the queue */
    virtual vm_obj_id_t prep_event_obj(VMG_ int *argc, int *evt_type)
    {
        /* push the socket error */
        G_stk->push()->set_int(sock_err);

        /* push the result message */
        G_stk->push_string(vmg_ msg, msg != 0 ? strlen(msg) : 0);

        /* push the request object ID */
        if (reqvar != 0)
            G_stk->push(&reqvar->val);
        else
            G_stk->push()->set_nil();

        /* relay our added argument count to the caller */
        *argc = 3;

        /* set the event type to NetEvReplyDone (6 - see include/tadsnet.h) */
        *evt_type = 6;

        /* the event subclass is NetReplyDoneEvent */
        return G_predef->net_reply_done_event;
    }

    /* VM globals, for use in main thread only */
    vm_globals *vmg;

    /* the request object for the request we replied to */
    TadsHttpRequest *req;

    /* VM global for the request object value */
    vm_globalvar_t *reqvar;

    /* the content body of the reply */
    bodyArg *body;

    /* socket error from reply data transfer */
    int sock_err;

    /* result message text */
    char *msg;
};


/*
 *   Reply sender object.  This encapsulates a reply's data and the code to
 *   send the reply.  This can be used to send a reply synchronously or
 *   asynchronously.
 */
class HTTPReplySender: public CVmRefCntObj
{
    friend class HTTPReplySenderThread;
    
public:
    HTTPReplySender()
    {
        vmg = 0;
        queue = 0;
        reqid = VM_INVALID_OBJ;
        req = 0;
        reqvar = 0;
        status = 0;
        status_len = 0;
        headers = 0;
        body = 0;
        cont_type = 0;
        cont_type_len = 0;
        cookies = 0;
        async = FALSE;
    }

    void set_params(VMG_ vm_obj_id_t reqid, TadsHttpRequest *req,
                    const char *status, size_t status_len,
                    const vm_val_t *headers, bodyArg *body,
                    const char *cont_type, size_t cont_type_len,
                    vm_httpreq_cookie *cookies)
    {
        /* 
         *   Save the values.  For reference types (string buffers, etc),
         *   just keep pointers to the originals; for synchronous replies,
         *   the caller will block on the send, so these will stay around for
         *   the send.  For asynchronous (threaded) replies, we'll make
         *   private copies when we start the new thread. 
         */
        this->reqid = reqid;
        this->req = req;
        this->status = status;
        this->status_len = status_len;
        this->headers = 0;
        this->body = body;
        this->cont_type = cont_type;
        this->cont_type_len = cont_type_len;
        this->cookies = cookies;

        /* parse the headers list */
        if (headers != 0)
            this->headers = http_header::create(vmg_ headers);

        /* add a reference on the request object */
        req->add_ref();
    }

    ~HTTPReplySender()
    {
        /* if we're in async mode, delete our private parameter data */
        if (async)
        {
            lib_free_str((char *)status);
            lib_free_str((char *)cont_type);
            if (cookies != 0)
                delete cookies;
        }

        /* delete the headers (we always copy these) */
        if (headers != 0)
            delete headers;

        /* release the net event queue */
        if (queue != 0)
            queue->release_ref();

        /* done with the request object and VM global */
        if (req != 0)
            req->release_ref();
        if (reqvar != 0)
        {
            VMGLOB_PTR(vmg);
            G_obj_table->delete_global_var(reqvar);
        }

        /* done with the body object */
        if (body != 0)
            delete body;
    }

    /* 
     *   send the reply; returns true on success, false on failure (and fills
     *   in '*err' with error information on failure) 
     */
    int send(http_reply_err *err)
    {
        /* presume success */
        int ok = TRUE;
        
        /*
         *   We've gathered all of the information, so send the reply.  Start
         *   with the status code.  
         */
        TadsServerThread *t = req->thread;
        if (!t->send("HTTP/1.1 ")
            || !t->send(status, status_len)
            || !t->send("\r\n"))
            ok = err->set(t, "error sending status");

        /* send the custom headers, if any */
        if (ok && headers != 0)
        {
            for (http_header *h = headers ; ok && h != 0 ; h = h->nxt)
            {
                if (!t->send(h->str, h->len))
                    ok = err->set(t, "error sending headers");
            }
        }

        /* if there's a body, send Content-Type and Content-Length headers */
        if (ok && !body->none)
        {
            /* if it's a "text/" type, add a utf-8 "charset" parameter */
            const char *charset = "";
            if (cont_type_len > 5 && memicmp(cont_type, "text/", 5) == 0)
                charset = "; charset=utf-8";

            /* send the standard headers: Content-Type, Content-Length */
            char hbuf[256];
            t3sprintf(hbuf, sizeof(hbuf),
                      "Content-Type: %.*s%s\r\n"
                      "Content-Length: %lu\r\n",
                      (int)cont_type_len, cont_type, charset,
                      (ulong)body->len);
            if (!t->send(hbuf))
                ok = err->set(t, "error sending headers");
        }

        /* send the cookies */
        for (vm_httpreq_cookie *c = cookies ; ok && c != 0 ; c = c->nxt)
        {
            if (!c->send(t))
                ok = err->set(t, "error sending cookies");
        }

        /* send the blank line at the end of the headers */
        if (ok && !t->send("\r\n"))
            ok = err->set(t, "error sending headers");

        /* send the reply body */
        if (ok && !body->send(t, err))
            ok = FALSE;
        
        /* mark the request as completed */
        req->complete();

        /* return the status indication */
        return ok;
    }

    /* reply error codes */
    static const int ErrThread = -8;                /* thread launch failed */

    /* start a thread to send the reply asynchronously */
    void start_thread(VMG0_)
    {
        /* save the globals for use in the 'done' event */
        vmg = VMGLOB_ADDR;

        /* remember the net event queue, and record our reference on it */
        if ((queue = G_net_queue) != 0)
            queue->add_ref();

        /* 
         *   remove the original parametres from our member variables before
         *   we set async mode, in case we throw an error in the course of
         *   making copies - we wouldn't want to delete the originals, as
         *   those are owned by the main thread
         */
        const char *ostatus = status; status = 0;
        const char *ocont_type = cont_type; cont_type = 0;
        vm_httpreq_cookie *ocookies = cookies; cookies = 0;

        /* note that we're now in async mode */
        async = TRUE;
        
        /* install copies in our member variables */
        status = lib_copy_str(ostatus, status_len);
        cont_type = lib_copy_str(ocont_type, cont_type_len);
        if (ocookies != 0)
            cookies = ocookies->clone();

        /* initialize async mode in the body object */
        body->init_async(vmg0_);

        /* 
         *   create a global variable for the request object ID, to keep it
         *   around until the request is completed 
         */
        reqvar = G_obj_table->create_global_var();
        reqvar->val.set_obj(reqid);

        /* create and launch a thread to handle the data transfer */
        HTTPReplySenderThread *t = new HTTPReplySenderThread(this);
        if (!t->launch())
        {
            /* 
             *   the thread failed - post a 'finished' event with an error,
             *   since the thread can't do this on its own if it never runs
             */
            post_done_event(0, "unable to launch thread");
        }

        /* we're done with the thread */
        t->release_ref();
    }

protected:
    /* handle the main thread action */
    void thread_main()
    {
        /* send the reply */
        http_reply_err err;
        if (send(&err))
        {
            /* post the successful 'finished' event */
            post_done_event(0, 0);
        }
        else
        {
            /* post the error */
            post_done_event(err.sock_err, err.msg);
        }
    }

    /* post the "finished" event for an asynchronous reply */
    void post_done_event(int sock_err, const char *msg)
    {
        /* establish the global context */
        VMGLOB_PTR(vmg);

        /* create the result event */
        TadsHttpReplyResult *evt = new TadsHttpReplyResult(
            vmg_ req, reqvar, body, sock_err, msg);
        
        /* queue the event (this transfers ownership to the queue) */
        queue->post(evt);

        /* forget the items that the event object took ownership of */
        req = 0;
        body = 0;
        reqvar = 0;
    }

    /* 
     *   VM globals (we need these for the "finished" event object) - note
     *   that the background thread isn't allowed to access VM globals since
     *   they're not thread safe and thus are restricted to the main thread
     */
    vm_globals *vmg;

    /* network event queue */
    TadsMessageQueue *queue;

    /* request object ID (an HTTPRequest object) */
    vm_obj_id_t reqid;
    vm_globalvar_t *reqvar;

    /* the underlying network request object */
    TadsHttpRequest *req;

    /* status string */
    const char *status;
    size_t status_len;

    /* headers */
    http_header *headers;

    /* content body */
    bodyArg *body;

    /* content type string */
    const char *cont_type;
    size_t cont_type_len;

    /* cookie list head */
    vm_httpreq_cookie *cookies;
    
    /* 
     *   Are we sending the reply asynchronously?  If this is true, the
     *   parameters that point to memory buffers are private copies. 
     */
    int async;
};

HTTPReplySenderThread::HTTPReplySenderThread(class HTTPReplySender *sender)
{
    (this->sender = sender)->add_ref();
}

HTTPReplySenderThread::~HTTPReplySenderThread()
{
    sender->release_ref();
}

void HTTPReplySenderThread::thread_main()
{
    sender->thread_main();
}

/* 
 *   Common handler for sendReply and sendReplyAsync.
 */
int CVmObjHTTPRequest::common_sendReply(
    VMG_ vm_obj_id_t self, vm_val_t *retval, uint *oargc, int wait)
{
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(1, 3);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the request */
    TadsHttpRequest *req = get_ext()->req;

    /* if the request is already completed, this is an error */
    if (req->completed)
        throw_net_err(vmg_ "request already completed", 0);

    /* get the 'body' argument (leave it on the stack for gc protection) */
    bodyArg *body = new bodyArg(vmg_ 0);

    /* no reply sender yet */
    HTTPReplySender *sender = 0;

    err_try
    {
        /* get the content type, if present */
        const char *cont_type = 0;
        size_t cont_type_len = 0;
        if (body->init_err != 0)
        {
            err_throw(body->init_err);
        }
        else if (body->status_code != 0)
        {
            /* we're using a generated HTML reply, so it's always text/html */
            cont_type = "text/html";
            cont_type_len = 9;
        }
        else if (body->none)
        {
            /* no body -> no content type */
            cont_type = 0;
            cont_type_len = 0;
        }
        else if (argc >= 2 && G_stk->get(1)->typ != VM_NIL)
        {
            /* retrieve the content type - this must be a string */
            cont_type = G_stk->get(1)->get_as_string(vmg0_);
            if (cont_type == 0)
                err_throw(VMERR_STRING_VAL_REQD);

            /* get the length and buffer pointer */
            cont_type_len = vmb_get_len(cont_type);
            cont_type += VMB_LEN;
        }
        else
        {
            /* 
             *   There's no content type argument, so we need to infer the
             *   type from the 'body' argument.  First, extract the initial
             *   section of the content to check for common signatures.  
             */
            char buf[512];
            int32_t ofs = 0, len = sizeof(buf);
            const char *bufp = body->extract(buf, ofs, len);

            /* check to see if the body is text or binary */
            if (body->is_text(vmg0_))
            {
                /* 
                 *   it's text data, so check for tell-tale signs of HTML and
                 *   XML, after skipping leading whitespace and HTML-style
                 *   comments 
                 */
                utf8_ptr p((char *)bufp);
                size_t rem = (size_t)len;
                while (rem != 0)
                {
                    /* if we're at a comment starter, skip the comment */
                    if (rem >= 4 && memcmp(p.getptr(), "<!--", 4) == 0)
                    {
                        /* skip to the closing sequence */
                        for ( ; rem != 0 ; p.inc(&rem))
                        {
                            if (rem >= 3 && memcmp(p.getptr(), "-->", 3) == 0)
                            {
                                /* skip it and stop looking */
                                p.inc(&rem);
                                p.inc(&rem);
                                p.inc(&rem);
                                break;
                            }
                        }

                        /* continue scanning */
                        continue;
                    }

                    /* if we're at any whitespace character, just skip it */
                    if (t3_is_whitespace(p.getch()))
                    {
                        p.inc(&rem);
                        continue;
                    }

                    /* otherwise stop scanning */
                    break;
                }

                /* check for <HTML or <?XML prefixes */
                if ((rem > 5 && memicmp(p.getptr(), "<html", 5) == 0)
                    || eq_skip_sp(p.getptr(), rem, "<!doctype html "))
                {
                    /* it looks like HTML */
                    cont_type = "text/html";
                }
                else if (rem > 5 && memicmp(p.getptr(), "<?xml", 5) == 0)
                {
                    /* it looks like XML */
                    cont_type = "text/xml";
                }
                else
                {
                    /* it doesn't look like HTML or XML, so use plain text */
                    cont_type = "text/plain";
                }
            }
            else
            {
                /* 
                 *   Treat it as binary data.  Check the first few bytes to
                 *   see if the file signature matches a known media type.  
                 */
                if (len > 10
                    && (unsigned char)bufp[0] == 0xff
                    && (unsigned char)bufp[1] == 0xd8
                    && memcmp(bufp+6, "JFIF", 4) == 0)
                {
                    /* it's a JPEG image */
                    cont_type = "image/jpeg";
                }
                else if (len > 6
                         && (memcmp(bufp, "GIF87a", 6) == 0
                             || memcmp(bufp, "GIF89a", 6) == 0))
                {
                    /* it's a GIF image */
                    cont_type = "image/gif";
                }
                else if (len > 6 && memcmp(bufp, "\211PNG\r\n\032\n", 6) == 0)
                {
                    /* it's a PNG image */
                    cont_type = "image/png";
                }
                else if (len > 3 && memcmp(bufp, "ID3", 3) == 0)
                {
                    /* MP3 file */
                    cont_type = "audio/mpeg";
                }
                else if (len > 14 && memcmp(
                    bufp, "OggS\000\002\000\000\000\000\000\000\000\000", 14)
                         == 0)
                {
                    /* Ogg Vorbis audio */
                    cont_type = "application/ogg";
                }
                else if (len > 4 && memcmp(bufp, "MThD", 4) == 0)
                {
                    /* MIDI */
                    cont_type = "audio/midi";
                }
                else if (len > 8
                         && (memcmp(bufp, "FWS", 3) == 0
                             || memcmp(bufp, "CWS", 3) == 0))
                {
                    /* shockwave flash */
                    cont_type = "application/x-shockwave-flash";
                }
                else
                {
                    /* we don't recognize it; use generic binary */
                    cont_type = "application/octet-stream";
                }
            }

            /* get the length of the content type string we selected */
            cont_type_len = strlen(cont_type);
        }

        /* get the result code, if present */
        size_t status_len;
        const char *status = get_status_arg(vmg_ body->status_code,
                                            2, argc, status_len);

        /* get the headers argument */
        vm_val_t *headers = 0;
        if (argc >= 4 && G_stk->get(3)->typ != VM_NIL)
        {
            /* get the headers value */
            headers = G_stk->get(3);

            /* make sure it's a list */
            if (!headers->is_listlike(vmg0_))
                err_throw(VMERR_LIST_VAL_REQD);
        }

        /* set up a reply sender object */
        sender = new HTTPReplySender();
        sender->set_params(
            vmg_ self, req, status, status_len, headers, body,
            cont_type, cont_type_len, get_ext()->cookies);

        /* the sender now owns our 'body' object */
        body = 0;

        /* send the reply, either directly or in a new thread */
        if (wait)
        {
            /* waiting for completion - send in the current thread */
            http_reply_err err;
            if (!sender->send(&err))
                throw_net_err(vmg_ err.msg, err.sock_err);
        }
        else
        {
            /* sending asynchronously - send in a new thread */
            sender->start_thread(vmg0_);
        }
    }
    err_finally
    {
        /* done with the body */
        if (body != 0)
            delete body;

        /* done with the reply sender */
        if (sender != 0)
            sender->release_ref();
    }
    err_end;

    /* discard arguments */
    G_stk->discard(argc);

    /* no result */
    retval->set_nil();
    
    /* handled */
    return TRUE;
}

/*
 *   start a chunked reply
 */
int CVmObjHTTPRequest::getp_startChunkedReply(
    VMG_ vm_obj_id_t self, vm_val_t *retval, uint *oargc)
{
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(1, 3);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the request */
    TadsHttpRequest *req = get_ext()->req;

    /* if the request is already completed, this is an error */
    if (req->completed)
        throw_net_err(vmg_ "request already completed", 0);

    /* get the content type argument */
    const char *cont_type = G_stk->get(0)->get_as_string(vmg0_);
    if (cont_type == 0)
        err_throw(VMERR_STRING_VAL_REQD);

    /* get the length and buffer pointer */
    size_t cont_type_len = vmb_get_len(cont_type);
    cont_type += VMB_LEN;

    /* get the result code, if present */
    size_t status_len;
    const char *status = get_status_arg(vmg_ 0, 1, argc, status_len);

    /* get the headers argument */
    const vm_val_t *headers = 0;
    if (argc >= 3)
    {
        /* get the headers value */
        headers = G_stk->get(2);

        /* make sure it's a list */
        if (!headers->is_listlike(vmg0_))
            err_throw(VMERR_LIST_VAL_REQD);
    }

    /*
     *   Send the headers, starting with the status code 
     */
    TadsServerThread *t = req->thread;
    if (!t->send("HTTP/1.1 ")
        || !t->send(status, status_len)
        || !t->send("\r\n"))
        throw_net_err(vmg_ "error sending status", t->last_error());

    /* send the custom headers, if any */
    if (headers != 0)
        send_custom_headers(vmg_ t, headers);

    /* send the cookies, if any */
    send_cookies(vmg_ t, get_ext()->cookies);

    /* send the standard headers: Content-Type, Transfer-Encoding */
    char hbuf[256];
    t3sprintf(hbuf, sizeof(hbuf),
              "Content-Type: %.*s\r\n"
              "Transfer-Encoding: chunked\r\n"
              "\r\n",
              (int)cont_type_len, cont_type);
    if (!t->send(hbuf))
        throw_net_err(vmg_ "error sending headers", t->last_error());

    /* discard arguments */
    G_stk->discard(argc);

    /* no result */
    retval->set_nil();

    /* handled */
    return TRUE;
}

/*
 *   send a piece of a chunked reply
 */
int CVmObjHTTPRequest::getp_sendReplyChunk(VMG_ vm_obj_id_t self,
                                           vm_val_t *retval, uint *oargc)
{
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the request and thread */
    TadsHttpRequest *req = get_ext()->req;
    TadsServerThread *t = req->thread;

    /* if the request is already completed, this is an error */
    if (req->completed)
        throw_net_err(vmg_ "request already completed", 0);

    /* retrieve the 'body' argument */
    bodyArg *body = new bodyArg(vmg_ 0);

    err_try
    {
        /* make sure we initialized the body correctly */
        if (body->init_err != 0)
            err_throw(body->init_err);
        
        /* send the length prefix */
        char lbuf[20];
        t3sprintf(lbuf, sizeof(lbuf), "%lx\r\n", (long)body->len);
        if (!t->send(lbuf))
            throw_net_err(vmg_ "error sending length prefix", t->last_error());

        /* send the body */
        http_reply_err err;
        if (!body->send(t, &err))
            throw_net_err(vmg_ err.msg, err.sock_err);

        /* send the CR-LF suffix */
        if (!t->send("\r\n"))
            throw_net_err(vmg_ "error sending suffix", t->last_error());
    }
    err_finally
    {
        /* done with the body */
        delete body;
    }
    err_end;

    /* discard arguments */
    G_stk->discard(argc);

    /* no result */
    retval->set_nil();

    /* handled */
    return TRUE;
}

/*
 *   finish a chunked reply
 */
int CVmObjHTTPRequest::getp_endChunkedReply(VMG_ vm_obj_id_t self,
                                            vm_val_t *retval, uint *oargc)
{
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0, 1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the request and the thread */
    TadsHttpRequest *req = get_ext()->req;
    TadsServerThread *t = req->thread;

    /* if the request is already completed, this is an error */
    if (req->completed)
        throw_net_err(vmg_ "request already completed", 0);

    /* send the "0" length prefix to indicate we're done */
    if (!t->send("0\r\n"))
        throw_net_err(vmg_ "error sending end-of-stream", t->last_error());

    /* get the headers argument */
    const vm_val_t *headers = 0;
    if (argc >= 1)
    {
        /* get the headers value */
        headers = G_stk->get(0);

        /* make sure it's a list */
        if (!headers->is_listlike(vmg0_))
            err_throw(VMERR_LIST_VAL_REQD);

        /* send the headers */
        send_custom_headers(vmg_ t, headers);
    }

    /* send the closing CR-LF */
    if (!t->send("\r\n"))
        throw_net_err(vmg_ "error sending suffix", t->last_error());

    /* mark the request as completed */
    req->complete();

    /* discard arguments */
    G_stk->discard(argc);

    /* no result */
    retval->set_nil();

    /* handled */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/*
 *   HTTP request object 
 */

/* 
 *   Prepare the event object.  This creates an HTTPRequest object (the
 *   intrinsic class representing an incoming HTTP request), and sets up a
 *   NetRequestEvent (the byte-code object representing a request) to wrap
 *   it.  
 */
vm_obj_id_t TadsHttpRequest::prep_event_obj(VMG_ int *argc, int *evt_type)
{
    /* get the HTTPServer object that created the listener thread */
    vm_obj_id_t srv_obj = thread->get_listener()->get_server_obj();

    /* create and push the HTTPRequest object */
    G_interpreter->push_obj(
        vmg_ CVmObjHTTPRequest::create(vmg_ FALSE, this, srv_obj));

    /* we added one argument to the constructor */
    *argc = 1;

    /* the event type is NetEvRequest (1 - see include/tadsnet.h) */
    *evt_type = 1;

    /* the event subclass is NetRequestEvent */
    return G_predef->net_request_event;
}

