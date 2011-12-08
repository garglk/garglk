/* $Header$ */

/* 
 *   Copyright (c) 2010 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmhttpsrv.h - CVmObjHTTPServer object
Function
  
Notes
  
Modified
  04/22/10 MJRoberts  - Creation
*/

#ifndef VMHTTPSRV_H
#define VMHTTPSRV_H

#include "t3std.h"
#include "vmtype.h"
#include "vmglob.h"
#include "vmobj.h"
#include "vmundo.h"
#include "vmnet.h"


/* ------------------------------------------------------------------------ */
/*
 *   HTTP server objects have no image file representation, because these
 *   objects are inherently transient.  
 */


/* ------------------------------------------------------------------------ */
/*
 *   Our in-memory extension data structure.  This is usually a 'struct'
 *   that contains the same information as in the image file, but using
 *   native types.
 */
struct vm_httpsrv_ext
{
    /* allocate the structure */
    static vm_httpsrv_ext *alloc_ext(VMG_ class CVmObjHTTPServer *self);

    /* the listener object */
    class TadsListener *l;

    /* original requested listener address */
    char *addr;
};


/* ------------------------------------------------------------------------ */
/*
 *   CVmObjHTTPServer intrinsic class definition
 */

class CVmObjHTTPServer: public CVmObject
{
    friend class CVmMetaclassHTTPServer;
    
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

    /* is this a CVmObjHTTPServer object? */
    static int is_httpsrv_obj(VMG_ vm_obj_id_t obj)
        { return vm_objp(vmg_ obj)->is_of_metaclass(metaclass_reg_); }

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
    
    /* apply an undo record - we don't keep any undo */
    void apply_undo(VMG_ struct CVmUndoRecord *) { }
    
    /* discard an undo record */
    void discard_undo(VMG_ struct CVmUndoRecord *) { }
    
    /* mark our undo record references - we don't have undo or references */
    void mark_undo_ref(VMG_ struct CVmUndoRecord *) { }

    /* mark our references - we don't have any to mark */
    void mark_refs(VMG_ uint) { }

    /* remove weak references - we don't have any weak refs */
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

    /* 
     *   Get my listening address and port number.  'host' is the host name
     *   original specified in the constructor, and 'ip' is the listener IP
     *   address in decimal 1.2.3.4 notation.  'port' is the actual listening
     *   port number.  The 'host' and 'ip' strings must be deleted by the
     *   caller via delete[].
     */
    int get_listener_addr(char *&addr, char *&ip, int &port) const;

protected:
    /* get my extension data */
    vm_httpsrv_ext *get_ext() const
        { return (vm_httpsrv_ext *)ext_; }

    /* load or reload image data */
    void load_image_data(VMG_ const char *ptr, size_t siz);

    /* create a with no initial contents */
    CVmObjHTTPServer() { ext_ = 0; }

    /* create with contents ('flag' is just for overloading resolution) */
    CVmObjHTTPServer(VMG_ int flag);

    /* property evaluator - undefined function */
    int getp_undef(VMG_ vm_obj_id_t, vm_val_t *, uint *) { return FALSE; }

    /* shut down the server */
    int getp_shutdown(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* get the port number */
    int getp_get_port(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* get the original listening address */
    int getp_get_addr(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* get the listening address in IP format */
    int getp_get_ip(VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc);

    /* property evaluation function table */
    static int (CVmObjHTTPServer::*func_table_[])(VMG_ vm_obj_id_t self,
        vm_val_t *retval, uint *argc);
};


/* ------------------------------------------------------------------------ */
/*
 *   CVmObjHTTPServer metaclass registration table object 
 */
class CVmMetaclassHTTPServer: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "http-server/030000"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjHTTPServer();
        G_obj_table->set_obj_gc_characteristics(id, FALSE, FALSE);
        G_obj_table->set_obj_transient(id);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjHTTPServer();
        G_obj_table->set_obj_gc_characteristics(id, FALSE, FALSE);
        G_obj_table->set_obj_transient(id);
    }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
        { return CVmObjHTTPServer::create_from_stack(vmg_ pc_ptr, argc); }
    
    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjHTTPServer::
            call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
};

#endif /* VMHTTPSRV_H */

/*
 *   Register the class 
 */
VM_REGISTER_METACLASS(CVmObjHTTPServer)
