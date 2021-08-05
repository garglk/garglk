#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) <year> Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmhttpsrv.cpp - CVmObjHTTPServer object
Function
  
Notes
  
Modified
  04/22/10 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "t3std.h"
#include "vmhttpsrv.h"
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
#include "utf8.h"
#include "vmimport.h"
#include "vmpredef.h"
#include "vmhost.h"


/* ------------------------------------------------------------------------ */
/*
 *   Allocate an extension structure 
 */
vm_httpsrv_ext *vm_httpsrv_ext::alloc_ext(
    VMG_ CVmObjHTTPServer *self)
{
    /* calculate how much space we need */
    size_t siz = sizeof(vm_httpsrv_ext);

    /* allocate the memory */
    vm_httpsrv_ext *ext = (vm_httpsrv_ext *)
                          G_mem->get_var_heap()->alloc_mem(siz, self);

    /* we don't have a listener or binding address yet */
    ext->l = 0;
    ext->addr = 0;

    /* return the new extension */
    return ext;
}

/* ------------------------------------------------------------------------ */
/*
 *   CVmObjHTTPServer object statics 
 */

/* metaclass registration object */
static CVmMetaclassHTTPServer metaclass_reg_obj;
CVmMetaclass *CVmObjHTTPServer::metaclass_reg_ = &metaclass_reg_obj;

/* function table */
int (CVmObjHTTPServer::*CVmObjHTTPServer::func_table_[])(
    VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc) =
{
    &CVmObjHTTPServer::getp_undef,
    &CVmObjHTTPServer::getp_shutdown,
    &CVmObjHTTPServer::getp_get_addr,
    &CVmObjHTTPServer::getp_get_ip,
    &CVmObjHTTPServer::getp_get_port
};

/* ------------------------------------------------------------------------ */
/*
 *   CVmObjHTTPServer intrinsic class implementation 
 */

/*
 *   construction
 */
CVmObjHTTPServer::CVmObjHTTPServer(VMG_ int)
{
    /* allocate our extension structure */
    ext_ = (char *)vm_httpsrv_ext::alloc_ext(vmg_ this);
}

/*
 *   create dynamically using stack arguments 
 */
vm_obj_id_t CVmObjHTTPServer::create_from_stack(
    VMG_ const uchar **pc_ptr, uint argc)
{
    vm_obj_id_t id;
    const char *errmsg;

    /* 
     *   parse arguments - new HTTPServer(addr?, port?) 
     */
    if (argc > 3)
        err_throw(VMERR_WRONG_NUM_OF_ARGS);

    /* check for the address - use "localhost" by default */
    char addr[256] = "localhost";
    if (argc >= 1)
    {
        /* get the address - this must be a string or nil */
        if (G_stk->get(0)->typ == VM_NIL)
            G_stk->discard();
        else
            CVmBif::pop_str_val_buf(vmg_ addr, sizeof(addr));
    }

    /* check for a port - use 0 by default */
    int port = 0;
    if (argc >= 2)
    {
        /* get the port number - this must be an integer or nil */
        if (G_stk->get(0)->typ == VM_NIL)
            G_stk->discard();
        else
            port = CVmBif::pop_int_val(vmg0_);
    }

    /* check for an upload size limit */
    int32_t ulim;
    if (argc >= 3)
    {
        if (G_stk->get(0)->typ == VM_NIL)
            G_stk->discard();
        else
            ulim = CVmBif::pop_long_val(vmg0_);
    }

    /* get the network safety level, to determine if this is allowed */
    int client_level, server_level;
    G_host_ifc->get_net_safety(&client_level, &server_level);

    /* check the network server safety level */
    switch (server_level)
    {
    case VM_NET_SAFETY_MINIMUM:
        /* all access allowed - proceed */
        break;

    case VM_NET_SAFETY_LOCALHOST:
        /* 
         *   localhost only - if the host isn't 'localhost' or '127.0.0.1',
         *   don't allow it 
         */
        if (stricmp(addr, "localhost") != 0
            && stricmp(addr, "127.0.0.1") != 0)
            goto access_error;

        /* allow it */
        break;

    case VM_NET_SAFETY_MAXIMUM:
    access_error:
        /* no access allowed */
        errmsg = "prohibited network access";
        G_interpreter->push_string(vmg_ errmsg);
        G_interpreter->throw_new_class(vmg_ G_predef->net_safety_exception,
                                       1, errmsg);
        AFTER_ERR_THROW(break;)
    }

    /* 
     *   allocate the object ID - this type of construction never creates a
     *   root object 
     */
    id = vm_new_id(vmg_ FALSE, FALSE, FALSE);

    /* http servers are inherently transient */
    G_obj_table->set_obj_transient(id);

    /* create the object */
    CVmObjHTTPServer *l = new (vmg_ id) CVmObjHTTPServer(vmg_ TRUE);

    /* get the extension */
    vm_httpsrv_ext *ext = l->get_ext();

    /* create the listener thread object */
    TadsHttpListenerThread *ht = new TadsHttpListenerThread(
        id, G_net_queue, ulim);

    /* launch the listener */
    ext->l = TadsListener::launch(addr, (ushort)port, ht);

    /* we're done with the thread object (the listener took it over) */
    ht->release_ref();

    /* if the launch failed, throw an error */
    if (ext->l == 0)
    {
        /* throw a network error */
        const char *errmsg = "Unable to start HTTP server";
        G_interpreter->push_string(vmg_ errmsg);
        G_interpreter->throw_new_class(vmg_ G_predef->net_exception, 1, errmsg);
    }

    /* remember my requested binding address */
    ext->addr = lib_copy_str(addr);

    /* return the new ID */
    return id;
}

/* 
 *   notify of deletion 
 */
void CVmObjHTTPServer::notify_delete(VMG_ int /*in_root_set*/)
{
    /* free our additional data, if we have any */
    vm_httpsrv_ext *ext = get_ext();
    if (ext != 0)
    {
        /* shut down the listener */
        if (ext->l != 0)
        {
            /* tell it to terminate */
            ext->l->shutdown();

            /* 
             *   Notify the listener thread that we're being deleted, so that
             *   it can drop its reference on us.  The listener thread object
             *   keeps a reference on us, but it isn't itself a
             *   garbage-collected object, so its reference on us won't
             *   prevent us from being deleted.  We therefore need to
             *   manually remove its reference on us when we're about to be
             *   deleted.  
             */
            ((TadsHttpListenerThread *)ext->l->get_thread())
                ->detach_server_obj();
            
            /* we're done with the listener object */
            ext->l->release_ref();
        }

        /* delete our address string */
        lib_free_str(ext->addr);

        /* delete the extension */
        G_mem->get_var_heap()->free_mem(ext_);
    }
}

/* 
 *   set a property 
 */
void CVmObjHTTPServer::set_prop(VMG_ class CVmUndo *undo,
                                vm_obj_id_t self, vm_prop_id_t prop,
                                const vm_val_t *val)
{
    /* no settable properties - throw an error */
    err_throw(VMERR_INVALID_SETPROP);
}

/* 
 *   get a property 
 */
int CVmObjHTTPServer::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
                               vm_obj_id_t self, vm_obj_id_t *source_obj,
                               uint *argc)
{
    uint func_idx;
    
    /* translate the property into a function vector index */
    func_idx = G_meta_table
               ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop);
    
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
void CVmObjHTTPServer::load_from_image(VMG_ vm_obj_id_t self,
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
void CVmObjHTTPServer::reload_from_image(VMG_ vm_obj_id_t self,
                                         const char *ptr, size_t siz)
{
    /* load our image data */
    load_image_data(vmg_ ptr, siz);
}

/*
 *   load or reload data from the image 
 */
void CVmObjHTTPServer::load_image_data(VMG_ const char *ptr, size_t siz)
{
    /* free our existing extension, if we have one */
    notify_delete(vmg_ FALSE);

    /* allocate the extension */
    vm_httpsrv_ext *ext = vm_httpsrv_ext::alloc_ext(vmg_ this);
    ext_ = (char *)ext;
}


/* 
 *   save to a file 
 */
void CVmObjHTTPServer::save_to_file(VMG_ class CVmFile *fp)
{
    /* we're inherently transient, so we should never do this */
    assert(FALSE);
}

/* 
 *   restore from a file 
 */
void CVmObjHTTPServer::restore_from_file(VMG_ vm_obj_id_t self,
                                         CVmFile *fp, CVmObjFixup *fixups)
{
    /* http servers are always transient, so we should never do this */
}

/*
 *   Shut down the server 
 */
int CVmObjHTTPServer::getp_shutdown(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *oargc)
{
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0, 1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* check arguments */
    int wait = FALSE;
    if (argc >= 1)
        wait = CVmBif::pop_bool_val(vmg0_);

    /* if we have a listener, tell it to shut down */
    vm_httpsrv_ext *ext = get_ext();
    if (ext != 0 && ext->l != 0)
    {
        /* add a reference on the listener while we're using it */
        ext->l->add_ref();
        
        /* shut down the server */
        ext->l->shutdown();

        /* if we're waiting, wait for the listener thread to exit */
        if (wait)
            ext->l->get_thread()->wait();

        /* 
         *   Return true if the thread is done, nil if it's still running.
         *   The main listener thread waits for all of its launched server
         *   threads to exit before it itself exits, so once the listener
         *   thread is done, we can be sure that all of its launched threads
         *   are done as well. 
         */
        retval->set_logical(ext->l->get_thread()->test());

        /* done with the listener */
        ext->l->release_ref();
    }
    else
    {
        /* there is no server, so it's already shut down */
        retval->set_true();
    }

    /* handled */
    return TRUE;
}

/*
 *   Internal get-port interface
 */
int CVmObjHTTPServer::get_listener_addr(
    char *&addr, char *&ip, int &port) const
{
    /* if we have a listener, get its port number */
    vm_httpsrv_ext *ext = get_ext();
    if (ext != 0 && ext->l != 0 && ext->addr != 0
        && ext->l->get_thread()->get_local_addr(ip, port))
    {
        /* got the IP and port - also return the constructor address string */
        addr = lib_copy_str(ext->addr);
        return TRUE;
    }
    else
    {
        /* the address isn't available */
        return FALSE;
    }
}


/*
 *   Get the port number
 */
int CVmObjHTTPServer::getp_get_port(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *oargc)
{
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* return the port number */
    char *addr, *ip;
    int port;
    if (get_listener_addr(addr, ip, port))
    {
        retval->set_int(port);
        lib_free_str(addr);
        lib_free_str(ip);
    }
    else
        retval->set_nil();

    /* handled */
    return TRUE;
}

/*
 *   Get the listener address 
 */
int CVmObjHTTPServer::getp_get_addr(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *oargc)
{
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* if we have an address string saved, return it */
    vm_httpsrv_ext *ext = get_ext();
    if (ext != 0 && ext->addr != 0)
    {
        /* got it - return the string value */
        G_interpreter->push_string(vmg_ ext->addr);
        G_stk->pop(retval);
    }
    else
    {
        /* no address value - return nil */
        retval->set_nil();
    }

    /* handled */
    return TRUE;
}

/*
 *   Get the listener IP address 
 */
int CVmObjHTTPServer::getp_get_ip(VMG_ vm_obj_id_t self,
                                  vm_val_t *retval, uint *oargc)
{
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* return the IP address */
    char *ip, *addr;
    int port;
    if (get_listener_addr(addr, ip, port))
    {
        /* got it - return the string value */
        G_interpreter->push_string(vmg_ ip);
        G_stk->pop(retval);

        /* free the source strings */
        lib_free_str(addr);
        lib_free_str(ip);
    }
    else
    {
        /* no IP address - return nil */
        retval->set_nil();
    }

    /* handled */
    return TRUE;
}

