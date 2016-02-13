/* $Header$ */

/* 
 *   Copyright (c) 2010 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmhttpreq.h - CVmObjHTTPRequest object
Function
  CVmObjHTTPRequest is the TADS intrinsic class wrapper for our native
  (C++) object TadsHttpRequest.  The native object is part of the vmnet
  layer, which uses reference counting to manage memory.  This VM object
  wrapper brings the object into the VM's garbage collection system, and also
  provides method access to the byte-code program.

  The native network layer's HTTP server creates the C++ TadsHttpRequest
  object when a request comes in from a network client.  The request object
  goes into the message queue, which the main game thread reads via the
  getNetEvent() intrinsic function.  When that function pulls a request
  object off the queue, it synthesizes the appropriate wrapper to return to
  the byte-code program.  That's when CVmObjHTTPRequest is instatiated.  The
  wrapper then keeps track of the VM's reference on the underlying request
  object, so that the request object stays in memory as long as the VM has a
  reference.  Once the VM wrapper goes out of scope and is collected by the
  garbage collector, the wrapper releases its reference on the underlying C++
  object.
Notes
  
Modified
   MJRoberts  - Creation
*/

#ifndef VMHTTPREQ_H
#define VMHTTPREQ_H

#include "t3std.h"
#include "vmtype.h"
#include "vmglob.h"
#include "vmobj.h"
#include "vmundo.h"


/* ------------------------------------------------------------------------ */
/*
 *   Image file data: this intrinsic class is inherently transient, so it's
 *   not stored in an image file.  We thus don't need any image file data
 *   structure.
 */


/* ------------------------------------------------------------------------ */
/*
 *   Our in-memory extension data structure.  This is usually a 'struct'
 *   that contains the same information as in the image file, but using
 *   native types.
 */
struct vm_httpreq_ext
{
    /* allocate the structure */
    static vm_httpreq_ext *alloc_ext(VMG_ class CVmObjHTTPRequest *self);

    /* the network message object */
    class TadsHttpRequest *req;

    /* list of cookies to be sent with the reply */
    struct vm_httpreq_cookie *cookies;

    /* the HTTPServer object for the server that created the request */
    vm_obj_id_t server;
};

/*
 *   Cookie tracker 
 */
struct vm_httpreq_cookie
{
    vm_httpreq_cookie(const char *name, size_t namelen,
                      const char *val, size_t vallen)
    {
        this->name = lib_copy_str(name, namelen);
        this->val = lib_copy_str(val, vallen);
        nxt = 0;
    }

    ~vm_httpreq_cookie()
    {
        lib_free_str(name);
        lib_free_str(val);
        if (nxt != 0)
            delete nxt;
    }

    /* clone this cookie and our list tail */
    vm_httpreq_cookie *clone()
    {
        /* clone myself */
        vm_httpreq_cookie *c = new vm_httpreq_cookie(
            name, strlen(name), val, val != 0 ? strlen(val) : 0);

        /* clone my list tail */
        if (nxt != 0)
            c->nxt = nxt->clone();

        /* return the clone */
        return c;
    }

    /* send the cookie; returns true on success, false on failure */
    int send(class TadsServerThread *t);

    /* name of the cookie */
    char *name;

    /* value (including expiration, path, domain, etc) */
    char *val;

    /* next in list */
    vm_httpreq_cookie *nxt;
};


/* ------------------------------------------------------------------------ */
/*
 *   CVmObjHTTPRequest intrinsic class definition
 */

class CVmObjHTTPRequest: public CVmObject
{
    friend class CVmMetaclassHTTPRequest;
    
public:
    /* metaclass registration object */
    static class CVmMetaclass *metaclass_reg_;
    class CVmMetaclass *get_metaclass_reg() const { return metaclass_reg_; }

    /* am I of the given metaclass? */
    virtual int is_of_metaclass(class CVmMetaclass *meta) const
    {
        /* try my own metaclass and my base class */
        return (meta == metaclass_reg_
                || CVmObject::is_of_metaclass(meta));
    }

    /* is this a CVmObjHTTPRequest object? */
    static int is_vmhttpreq_obj(VMG_ vm_obj_id_t obj)
        { return vm_objp(vmg_ obj)->is_of_metaclass(metaclass_reg_); }

    /* 
     *   Create the request object.  'srv_obj' is the HTTPServer object for
     *   the listener that received the request.  
     */
    static vm_obj_id_t create(VMG_ int in_root_set,
                              class TadsHttpRequest *req,
                              vm_obj_id_t srv_obj);

    /* create dynamically using stack arguments */
    static vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr,
                                         uint argc);

    /* 
     *   call a static property - we don't have any of our own, so simply
     *   "inherit" the base class handling 
     */
    static int call_stat_prop(VMG_ vm_val_t *result,
                              const uchar **pc_ptr, uint *argc,
                              vm_prop_id_t prop)
    {
        return CVmObject::
            call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }

    /* notify of deletion */
    void notify_delete(VMG_ int in_root_set);

    /* set a property */
    void set_prop(VMG_ class CVmUndo *undo,
                  vm_obj_id_t self, vm_prop_id_t prop, const vm_val_t *val);

    /* get a property */
    int get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                 vm_obj_id_t self, vm_obj_id_t *source_obj, uint *argc);

    /* 
     *   receive savepoint notification - we don't keep any
     *   savepoint-relative records, so we don't need to do anything here 
     */
    void notify_new_savept() { }
    
    /* we don't participate in undo */
    void apply_undo(VMG_ struct CVmUndoRecord *) { }
    void discard_undo(VMG_ struct CVmUndoRecord *) { }
    void mark_undo_ref(VMG_ struct CVmUndoRecord *) { }

    /* mark references */
    void mark_refs(VMG_ uint state);

    /* we don't have any weak references */
    void remove_stale_weak_refs(VMG0_) { }
    void remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *) { }

    /* load from an image file */
    void load_from_image(VMG_ vm_obj_id_t self, const char *ptr, size_t siz);

    /* reload from an image file */
    void reload_from_image(VMG_ vm_obj_id_t self,
                           const char *ptr, size_t siz);

    /* rebuild for image file */
    virtual ulong rebuild_image(VMG_ char *buf, ulong buflen);

    /* save to a file */
    void save_to_file(VMG_ class CVmFile *fp);

    /* restore from a file */
    void restore_from_file(VMG_ vm_obj_id_t self,
                           class CVmFile *fp, class CVmObjFixup *fixups);

    /* 
     *   determine if we've been changed since loading - assume we have (if
     *   we haven't, the only harm is the cost of unnecessarily reloading or
     *   saving) 
     */
    int is_changed_since_load() const { return TRUE; }

protected:
    /* get my extension data */
    vm_httpreq_ext *get_ext() const { return (vm_httpreq_ext *)ext_; }

    /* load or reload image data */
    void load_image_data(VMG_ const char *ptr, size_t siz);

    /* create a with no initial contents */
    CVmObjHTTPRequest() { ext_ = 0; }

    /* create with contents ('flag' is just for overloading resolution) */
    CVmObjHTTPRequest(VMG_ int flag);

    /* property evaluator - undefined function */
    int getp_undef(VMG_ vm_obj_id_t, vm_val_t *, uint *) { return FALSE; }

    /* get the server object */
    int getp_getServer(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* get the HTTP verb */
    int getp_getVerb(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* get the query string */
    int getp_getQuery(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* parse the query string into the resource name and parameters */
    int getp_parseQuery(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* parse the query string and find a given parameter by name */
    int getp_getQueryParam(VMG_ vm_obj_id_t self,
                           vm_val_t *retval, uint *argc);

    /* get the request headers */
    int getp_getHeaders(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* get a single cookie */
    int getp_getCookie(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* get the cookies */
    int getp_getCookies(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* set a cookie for the reply */
    int getp_setCookie(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* get the cookies */
    int getp_getFormFields(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* get the request message body */
    int getp_getBody(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* get the client network address */
    int getp_getClientAddress(
        VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* send a reply */
    int getp_sendReply(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* common handler for sendReply and sendReplyAsync */
    int common_sendReply(
        VMG_ vm_obj_id_t self, vm_val_t *retval, uint *oargc, int wait);

    /* start a chunked reply */
    int getp_startChunkedReply(VMG_ vm_obj_id_t self, vm_val_t *retval,
                               uint *argc);

    /* send a piece of a chunked reply */
    int getp_sendReplyChunk(VMG_ vm_obj_id_t self, vm_val_t *retval,
                            uint *argc);

    /* finish a chunked reply */
    int getp_endChunkedReply(VMG_ vm_obj_id_t self, vm_val_t *retval,
                             uint *argc);

    /* send a reply asynchronously */
    int getp_sendReplyAsync(
        VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluation function table */
    static int (CVmObjHTTPRequest::*func_table_[])(VMG_ vm_obj_id_t self,
        vm_val_t *retval, uint *argc);
};


/* ------------------------------------------------------------------------ */
/*
 *   CVmObjHTTPRequest metaclass registration table object 
 */
class CVmMetaclassHTTPRequest: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "http-request/030001"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjHTTPRequest();
        G_obj_table->set_obj_gc_characteristics(id, FALSE, FALSE);
        G_obj_table->set_obj_transient(id);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjHTTPRequest();
        G_obj_table->set_obj_gc_characteristics(id, FALSE, FALSE);
        G_obj_table->set_obj_transient(id);
    }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
        { return CVmObjHTTPRequest::create_from_stack(vmg_ pc_ptr, argc); }
    
    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjHTTPRequest::
            call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
};

#endif /* VMHTTPREQ_H */

/*
 *   Register the class 
 */
VM_REGISTER_METACLASS(CVmObjHTTPRequest)
