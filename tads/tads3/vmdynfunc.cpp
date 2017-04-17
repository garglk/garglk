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
  vmdynfunc.cpp - DynamicFunc implementation
Function
  
Notes
  
Modified
  12/13/09 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "t3std.h"
#include "os.h"
#include "charmap.h"
#include "vmdynfunc.h"
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
#include "vmlst.h"
#include "vmstr.h"
#include "vmrun.h"
#include "vmgram.h"
#include "vmdict.h"
#include "vmtobj.h"
#include "vmpredef.h"
#include "vmimport.h"
#include "vmfunc.h"
#include "vmfref.h"
#include "utf8.h"
#include "vmop.h"
#include "tctarg.h"
#include "tcglob.h"
#include "tctok.h"
#include "tcprs.h"
#include "tcmain.h"
#include "resload.h"
#include "tchost.h"
#include "vmhost.h"
#include "tcgen.h"
#include "vmimage.h"
#include "vmpool.h"



/* ------------------------------------------------------------------------ */
/*
 *   Allocate an extension structure 
 */
vm_dynfunc_ext *vm_dynfunc_ext::alloc_ext(
    VMG_ CVmDynamicFunc *self, size_t bytecode_len, int obj_ref_cnt)
{
    /* 
     *   Calculate how much space we need: we need the base structure, plus
     *   space for the object references, plus the dynamic object header, and
     *   the byte code array.  
     */
    size_t siz = sizeof(vm_dynfunc_ext)
                 + (obj_ref_cnt - 1)*sizeof_field(vm_dynfunc_ext, obj_refs[0])
                 + VMCO_PREFIX_LENGTH
                 + bytecode_len;
    
    /* allocate the memory */
    vm_dynfunc_ext *ext = (vm_dynfunc_ext *)G_mem->get_var_heap()->alloc_mem(
        siz, self);
    
    /* remember the sizes */
    ext->obj_ref_cnt = obj_ref_cnt;
    ext->bytecode_len = bytecode_len;

    /* we don't have any source text yet */
    ext->src.set_nil();

    /* we don't have a method context object yet */
    ext->method_ctx.set_nil();

    /* return the new extension */
    return ext;
}

/* ------------------------------------------------------------------------ */
/*
 *   DynamicFunc statics 
 */

/* metaclass registration object */
static CVmMetaclassDynamicFunc metaclass_reg_obj;
CVmMetaclass *CVmDynamicFunc::metaclass_reg_ = &metaclass_reg_obj;

/* function table */
int (CVmDynamicFunc::*CVmDynamicFunc::func_table_[])(
    VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc) =
{
    &CVmDynamicFunc::getp_undef,
    &CVmDynamicFunc::getp_get_source
};

/* ------------------------------------------------------------------------ */
/*
 *   DynamicFunc metaclass implementation 
 */

/*
 *   construction
 */
CVmDynamicFunc::CVmDynamicFunc(VMG_ vm_obj_id_t self, const vm_val_t *src,
                               size_t bytecode_len, int obj_ref_cnt)
{
    /* allocate our extension structure */
    vm_dynfunc_ext *ext = vm_dynfunc_ext::alloc_ext(
        vmg_ this, bytecode_len, obj_ref_cnt);

    /* store it in the object */
    ext_ = (char *)ext;
    
    /* save the source value */
    if (src != 0)
        ext->src = *src;

    /* set up the code object prefix header */
    vmb_put_objid(ext->get_prefix_ptr(), self);
}

/*
 *   create 
 */
vm_obj_id_t CVmDynamicFunc::create(VMG_ int in_root_set,
                                   vm_obj_id_t globals,
                                   vm_obj_id_t locals,
                                   vm_obj_id_t macros,
                                   const vm_val_t *srcval,
                                   const char *src, size_t src_len)
{
    /* get the compiler interface */
    CVmDynamicCompiler *comp = CVmDynamicCompiler::get(vmg0_);

    /* compile the code and create a code object */
    CVmDynCompResults results;
    vm_obj_id_t id = comp->compile(
        vmg_ in_root_set, globals, locals, macros,
        srcval, src, src_len, DCModeAuto, 0, &results);

    /* if an error occurred, throw the error */
    if (id == VM_INVALID_OBJ)
        results.throw_error(vmg0_);

    /* return the new ID */
    return id;
}


/*
 *   create 
 */
vm_obj_id_t CVmDynamicFunc::create(VMG_ int in_root_set,
                                   vm_obj_id_t globals, vm_obj_id_t locals,
                                   vm_obj_id_t macros,
                                   const vm_val_t *src)
{
    /* get the source value as a string */
    const char *src_txt = src->get_as_string(vmg0_);

    /* if it's not a string value, it's an error */
    if (src_txt == 0)
        err_throw(VMERR_BAD_TYPE_BIF);

    /* create the object */
    return create(vmg_ in_root_set, globals, locals, macros,
                  src, src_txt + VMB_LEN, vmb_get_len(src_txt));
}

/*
 *   Create dynamically using stack arguments.  This takes the source code as
 *   a string, compiles it, and creates a DynamicFunc encapsulating the byte
 *   code.  
 */
vm_obj_id_t CVmDynamicFunc::create_from_stack(
    VMG_ const uchar **pc_ptr, uint argc)
{
    /* check arguments */
    if (argc < 1 || argc > 4)
        err_throw(VMERR_WRONG_NUM_OF_ARGS);

    /* get the source code argument (leave on stack for gc protection) */
    const vm_val_t *src = G_stk->get(0);

    /* get the global symbol table object, if any */
    vm_obj_id_t globals = VM_INVALID_OBJ;
    if (argc >= 2)
    {
        /* it has to be an object or nil */
        if (G_stk->get(1)->typ == VM_OBJ)
            globals = G_stk->get(1)->val.obj;
        else if (G_stk->get(1)->typ != VM_NIL)
            err_throw(VMERR_BAD_TYPE_BIF);
    }

    /* get the local symbol table object, if any */
    vm_obj_id_t locals = VM_INVALID_OBJ;
    if (argc >= 3)
    {
        /* it has to be an object or nil */
        if (G_stk->get(2)->typ == VM_OBJ)
            locals = G_stk->get(2)->val.obj;
        else if (G_stk->get(2)->typ != VM_NIL)
            err_throw(VMERR_BAD_TYPE_BIF);
    }

    /* check for macros */
    vm_obj_id_t macros = VM_INVALID_OBJ;
    if (argc >= 4)
    {
        /* it has to be an object or nil */
        if (G_stk->get(3)->typ == VM_OBJ)
            macros = G_stk->get(3)->val.obj;
        else if (G_stk->get(3)->typ != VM_NIL)
            err_throw(VMERR_BAD_TYPE_BIF);
    }

    /* allocate the object ID and create the object */
    vm_obj_id_t id = create(vmg_ FALSE, globals, locals, macros, src);

    /* discard arguments */
    G_stk->discard(argc);

    /* return the new ID */
    return id;
}

/* 
 *   notify of deletion 
 */
void CVmDynamicFunc::notify_delete(VMG_ int /*in_root_set*/)
{
    /* free our additional data, if we have any */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);
}

/*
 *   Retrieve a DynamicFunc ID from a bytecode method prefix 
 */
vm_obj_id_t CVmDynamicFunc::get_obj_from_prefix(VMG_ const uchar *p)
{
    /* 
     *   A DynamicFunc stores a method header prefix just before the start of
     *   the header, and the prefix stores the associated DynamicFunc ID.
     *   Read the prefix header.
     *   
     *   (Technically, this has some risk of reading outside the bounds of
     *   valid program memory, because we don't know the origin of 'p'.
     *   Callers should never attempt calling this routine with arbitrary
     *   pointers.  Only pass in pointers that are known to refer to valid
     *   method headers, AND that aren't part of the static Code Pool.  The
     *   only other kind of valid method header pointer is a DynamicFunc, so
     *   as long as callers obey these rules this dereference should be
     *   safe.)  
     */
    vm_obj_id_t id = vmb_get_objid((const char *)p - VMCO_PREFIX_LENGTH);

    /* verify with the memory manager that this is a valid object ID */
    if (!G_obj_table->is_obj_id_valid(id))
        return VM_INVALID_OBJ;

    /* it's a valid object; make sure it's really of type DynamicFunc */
    if (!is_dynfunc_obj(vmg_ id))
        return VM_INVALID_OBJ;

    /* 
     *   It's a valid DynamicFunc: verify that it's really the one it claims
     *   to be.  If it is, the object's bytecode pointer should match 'p'.  
     */
    CVmDynamicFunc *co = (CVmDynamicFunc *)vm_objp(vmg_ id);
    if (co->get_ext()->get_bytecode_ptr() == (const char *)p)
    {
        /* we have a winner! */
        return id;
    }

    /* it's not our object */
    return VM_INVALID_OBJ;
}

/* 
 *   get a property 
 */
int CVmDynamicFunc::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
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
 *   Invoke 
 */
int CVmDynamicFunc::get_invoker(VMG_ vm_val_t *val)
{
    /* 
     *   Return a pointer to the start of our byte code.  The actual method
     *   header starts after our prefix.  
     */
    if (val != 0)
        val->set_codeptr(get_ext()->get_bytecode_ptr());

    /* we're invokable */
    return TRUE;
}

/* 
 *   Index the object.
 *   
 *   A DynamicFunc is an invokee, and as such it can be referenced by
 *   generated code for special internal operations.  We use indexing to
 *   retrieve special information:
 *   
 *   [1] is the 'self' object or method context object for the enclosing
 *   stack frame.  The compiler uses this to establish the method context for
 *   a dynamic method that's compiled in the context of an enclosing frame,
 *   in analogy to an anonymous function that accesses its enclosing lexical
 *   scope's method context.  
 */
int CVmDynamicFunc::index_val_q(VMG_ vm_val_t *result,
                                vm_obj_id_t self,
                                const vm_val_t *index_val)
{
    /* check the type */
    if (index_val->typ == VM_INT)
    {
        /* it's an integer index - check the value */
        switch (index_val->val.intval)
        {
        case 1:
            /* return our method context */
            *result = get_ext()->method_ctx;
            return TRUE;

        default:
            /* other values are not allowed */
            break;
        }
    }

    /* if we didn't handle it already, it's an invalid index */
    err_throw(VMERR_INDEX_OUT_OF_RANGE);
    AFTER_ERR_THROW(return TRUE;)
}

/* 
 *   load from an image file 
 */
void CVmDynamicFunc::load_from_image(VMG_ vm_obj_id_t self,
                                     const char *ptr, size_t siz)
{
    /* load our image data */
    load_image_data(vmg_ self, ptr, siz);

    /* 
     *   save our image data pointer in the object table, so that we can
     *   access it (without storing it ourselves) during a reload 
     */
    G_obj_table->save_image_pointer(self, ptr, siz);
}

/*
 *   reload from the image file 
 */
void CVmDynamicFunc::reload_from_image(VMG_ vm_obj_id_t self,
                                       const char *ptr, size_t siz)
{
    /* load our image data */
    load_image_data(vmg_ self, ptr, siz);
}

/*
 *   load or reload data from the image 
 */
void CVmDynamicFunc::load_image_data(VMG_ vm_obj_id_t self,
                                     const char *ptr, size_t siz)
{
    /* free our existing extension, if we have one */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);

    /* read the bytecode length */
    size_t bytecode_len = osrp2(ptr);
    ptr += 2;

    /* read the object reference count */
    int obj_ref_cnt = osrp2(ptr);
    ptr += 2;

    /* allocate the extension */
    vm_dynfunc_ext *ext = vm_dynfunc_ext::alloc_ext(
        vmg_ this, bytecode_len, obj_ref_cnt);
    ext_ = (char *)ext;

    /* save the method context object */
    vmb_get_dh(ptr, &ext->method_ctx);
    ptr += VMB_DATAHOLDER;

    /* save the source string value */
    vmb_get_dh(ptr, &ext->src);
    ptr += VMB_DATAHOLDER;

    /* set up the dynamic code prefix header */
    vmb_put_objid(ext->get_prefix_ptr(), self);

    /* copy the byte code */
    memcpy(ext->get_bytecode_ptr(), ptr, bytecode_len);
    ptr += bytecode_len;

    /* load the object reference offsets */
    for (int i = 0 ; i < obj_ref_cnt ; ++i, ptr += 2)
        ext->obj_refs[i] = osrp2(ptr);
}


/* 
 *   save to a file 
 */
void CVmDynamicFunc::save_to_file(VMG_ class CVmFile *fp)
{
    char buf[VMB_DATAHOLDER];
    vm_dynfunc_ext *ext = get_ext();

    /* write our source code length */
    fp->write_uint2(ext->bytecode_len);

    /* write our object reference count */
    fp->write_uint2(ext->obj_ref_cnt);

    /* write the method context object */
    vmb_put_dh(buf, &ext->method_ctx);
    fp->write_bytes(buf, VMB_DATAHOLDER);

    /* write our source object value */
    vmb_put_dh(buf, &ext->src);
    fp->write_bytes(buf, VMB_DATAHOLDER);

    /* write the bytecode data */
    fp->write_bytes(ext->get_bytecode_ptr(), ext->bytecode_len);

    /* write the object reference list */
    for (int i = 0 ; i < ext->obj_ref_cnt ; ++i)
        fp->write_uint2(ext->obj_refs[i]);
}

/* 
 *   restore from a file 
 */
void CVmDynamicFunc::restore_from_file(VMG_ vm_obj_id_t self,
                                       CVmFile *fp, CVmObjFixup *fixups)
{
    char buf[VMB_DATAHOLDER];
    
    /* free our existing extension, if we have one */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);

    /* read the bytecode length and object reference count */
    size_t bytecode_len = fp->read_uint2();
    int obj_ref_cnt = fp->read_uint2();

    /* allocate the extension */
    vm_dynfunc_ext *ext = vm_dynfunc_ext::alloc_ext(
        vmg_ this, bytecode_len, obj_ref_cnt);
    ext_ = (char *)ext;

    /* read the method context object */
    fp->read_bytes(buf, VMB_DATAHOLDER);
    fixups->fix_dh(vmg_ buf);
    vmb_get_dh(buf, &ext->method_ctx);

    /* read the source object */
    fp->read_bytes(buf, VMB_DATAHOLDER);
    fixups->fix_dh(vmg_ buf);
    vmb_get_dh(buf, &ext->src);

    /* set up the dynamic object code prefix header */
    vmb_put_objid(ext->get_prefix_ptr(), self);

    /* read the bytecode */
    char *bc = ext->get_bytecode_ptr();
    fp->read_bytes(bc, bytecode_len);

    /* read the object references, fixing each one up in the bytecode */
    for (int i = 0 ; i < obj_ref_cnt ; ++i)
    {
        /* read and save the offset of this reference */
        uint ofs = ext->obj_refs[i] = fp->read_uint2();

        /* fix up the object ID in the bytecode data */
        fixups->fix_vmb_obj(vmg_ bc + ofs);
    }
}

/*
 *   mark references 
 */
void CVmDynamicFunc::mark_refs(VMG_ uint state)
{
    /* 
     *   Get my extension, and only proceed if it's non-empty.  For most
     *   object types this isn't an issue, because the only time we create
     *   empty objects of most types is during loading, when gc is disabled.
     *   But we do create long-lived empty code objects in the course of
     *   compilation, because we need a placeholder for the object ID ahead
     *   of the compilation.  
     */
    vm_dynfunc_ext *ext = get_ext();
    if (ext != 0)
    {
        /* mark my context object as referenced */
        if (ext->method_ctx.typ == VM_OBJ)
            G_obj_table->mark_all_refs(ext->method_ctx.val.obj, state);

        /* mark my source code string as referenced */
        if (ext->src.typ == VM_OBJ)
            G_obj_table->mark_all_refs(ext->src.val.obj, state);

        /* mark each object reference in the bytecode stream */
        char *bc = ext->get_bytecode_ptr();
        for (int i = 0 ; i < ext->obj_ref_cnt ; ++i)
        {
            /* 
             *   Read the object ID from the bytecode.  The obj_refs[] array
             *   entry isn't the actual reference, but rather the offset of
             *   the reference in the bytecode stream.  The reference itself
             *   is a portable OBJECT_ID field.  
             */
            vm_obj_id_t ref = vmb_get_objid(bc + ext->obj_refs[i]);
            G_obj_table->mark_all_refs(ref, state);
        }
    }
}


/* 
 *   property evaluator - get the source code string
 */
int CVmDynamicFunc::getp_get_source(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* return the source code object */
    *retval = get_ext()->src;

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Custom host interface.  This host interface captures error messages
 *   generated during compilation so that we can return them to the program.
 */
class CTcHostIfcDynComp: public CTcHostIfc
{
public:
    CTcHostIfcDynComp()
    {
        /* allocate an initial buffer */
        erralo_ = 1024;
        errmsg_ = (char *)t3malloc(erralo_);

        /* initially clear the message buffer */
        reset_messages();
    }

    ~CTcHostIfcDynComp()
    {
        /* free our buffer */
        t3free(errmsg_);
    }

    /* clear the message buffer */
    void reset_messages()
    {
        errmsg_[0] = '\0';
        p_ = errmsg_;
    }

    /* get the text of the first error message since we cleared the buffer */
    const char *get_error_msg() const { return errmsg_; }

    /*
     *   CTcHostIfc interface 
     */

    /* display an informational message */
    virtual void v_print_msg(const char *, va_list)
    {
        /* ignore informational messages */
    }

    /* display a process step message */
    virtual void v_print_step(const char *, va_list)
    {
        /* ignore process step messages */
    }

    /* display an error message */
    virtual void v_print_err(const char *msg, va_list args)
    {
        /* format the message */
        char *f = t3vsprintf_alloc(msg, args);
        if (f == 0)
            return;
        
        /* get the length */
        size_t len = strlen(f);
        
        /* expand our buffer if necessary */
        size_t curlen = p_ - errmsg_;
        size_t need = curlen + len + 1;
        if (need > erralo_)
        {
            erralo_ = need + 1024;
            char *newbuf = (char *)t3realloc(errmsg_, erralo_);
            if (newbuf == 0)
                return;
            
            /* switch to the new buffer */
            errmsg_ = newbuf;
            p_ = errmsg_ + curlen;
        }
        
        /* append the message */
        memcpy(p_, f, len + 1);
        
        /* bump the write pointer */
        p_ += len;

        /* done with the allocated message text */
        t3free(f);
    }

private:
    /* our error message buffer */
    char *errmsg_;

    /* allocated buffer size */
    size_t erralo_;

    /* pointer to next available byte of message buffer */
    char *p_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Dynamic compiler symbol table interface.  This provides the compiler
 *   with global symbols specified via an indexable object.  This is usually
 *   a LookupTable, but any object that can be indexed by symbol name string
 *   will work.  The actual compiler symbols retrieved from
 *   t3GetGlobalSymbols() can be used, or the program can provide its own
 *   custom symbol table.
 *   
 *   We implement the symbol table as a view of the user-code object, with
 *   caching in our own hash table.  To match the interface, we have to
 *   create a CTcSymbol object for each symbol we successfully look up.
 *   Rather than creating redundant copies of these objects, we cache them in
 *   our own hash table.  
 */
class CVmDynFuncSymtab: public CTcPrsSymtab
{
public:
    /* initialize with a source object */
    CVmDynFuncSymtab(VMG_ vm_obj_id_t globals, vm_obj_id_t locals)
        : CTcPrsSymtab(0)
    {
        /* remember the globals */
        gptr_ = VMGLOB_ADDR;

        /* remember my symbol table objects */
        globals_ = globals;
        locals_ = locals;
    }

    /* find a symbol - implementation of CTcPrsDbgSymtab interface */
    virtual CTcSymbol *find_direct(const textchar_t *sym, size_t len)
    {
        /* first, check the cache to see if we've already looked it up */
        CTcSymbol *ret = CTcPrsSymtab::find_direct(sym, len);
        if (ret != 0)
            return ret;

        /* 
         *   Didn't find it - check the user-code tables.
         */

        /* try our local symbol table */
        ret = find_local(sym, len);

        /* if that didn't work, try our global symbol table */
        if (ret == 0)
            ret = find_global(sym, len);

        /* if we found a symbol, add it to the cache */
        if (ret != 0)
            CTcPrsSymtab::add_entry(ret);

        /* return the symbol */
        return ret;
    }

    /* add an entry */
    virtual void add_entry(CTcSymbol *sym)
    {
        /* flag an error */
        G_tok->log_error(TCERR_RT_CANNOT_DEFINE_GLOBALS,
                         (int)sym->get_sym_len(), sym->get_sym());
    }
    
private:
    /* look up a symbol in our local symbol table */
    CTcSymbol *find_local(const textchar_t *sym, size_t len)
    {
        /* if there's no user-code table, there's nothing to search */
        if (locals_ == VM_INVALID_OBJ)
            return 0;

        /* establish access to globals */
        VMGLOB_PTR(gptr_);

        /* 
         *   if it's a StackFrameDesc object, look up the symbol; otherwise
         *   try to treat it as a list-like object, and check each element of
         *   the list 
         */
        if (CVmObjFrameDesc::is_framedesc_obj(vmg_ locals_))
        {
            /* look up the value in our single table */
            return find_local(sym, len, locals_);
        }
        else
        {
            /* assume it's a list - set up an object value */
            vm_val_t lt;
            lt.set_obj(locals_);
            
            /* check each list element */
            int n = lt.ll_length(vmg0_);
            for (int i = 1 ; i <= n ; ++i)
            {
                /* get this element */
                vm_val_t ele;
                lt.ll_index(vmg_ &ele, i);

                /* if it's a StackFrameDesc object, check it */
                if (ele.typ == VM_OBJ
                    && CVmObjFrameDesc::is_framedesc_obj(vmg_ ele.val.obj))
                {
                    /* look up the value in this table */
                    CTcSymbol *ret = find_local(sym, len, ele.val.obj);

                    /* if we found it, return the value */
                    if (ret != 0)
                        return ret;
                }
            }

            /* we didn't find a value */
            return 0;
        }
    }

    /* find a symbol in a given local table */
    CTcSymbol *find_local(const textchar_t *sym, size_t len, vm_obj_id_t fref)
    {
        /* establish access to globals */
        VMGLOB_PTR(gptr_);

        /* get the frame descriptor object */
        CVmObjFrameDesc *fp = (CVmObjFrameDesc *)vm_objp(vmg_ fref);

        /* look up the symbol */
        CVmDbgFrameSymPtr info;
        if (fp->find_local(vmg_ sym, len, &info))
        {
            /* return a new dynamic local symbol */
            return new CTcSymDynLocal(
                sym, len, FALSE, fp->get_frame_ref_id(),
                fp->get_frame_ref(vmg0_)->get_var_frame_index(&info),
                info.is_ctx_local() ? info.get_ctx_arr_idx() : 0);
        }

        /* didn't find it - return failure */
        return 0;
    }

    /* look up a symbol in our global table */
    CTcSymbol *find_global(const textchar_t *sym, size_t len)
    {
        /* if there's no user-code table, there's nothing to search */
        if (globals_ == VM_INVALID_OBJ)
            return 0;

        /* presume we won't find a symbol */
        CTcSymbol *ret = 0;
        
        /* establish access to globals */
        VMGLOB_PTR(gptr_);

        /* create a string for the symbol name; stack it for gc protection */
        vm_val_t symstr;
        symstr.set_obj(CVmObjString::create(vmg_ FALSE, sym, len));
        G_stk->push(&symstr);

        /* look up the symbol string in the table */
        vm_val_t symval;
        vm_objp(vmg_ globals_)->index_val_ov(vmg_ &symval, globals_, &symstr);
        G_stk->push(&symval);

        /* check what we found */
        switch (symval.typ)
        {
        case VM_OBJ:
            /* it's an object - map it to CTcSymObj or CTcSymMetaclass */
            if (CVmObjClass::is_intcls_obj(vmg_ symval.val.obj))
            {
                /* 
                 *   it's an IntrinsicClass object - this represents a
                 *   metaclass, so map it to a CTcSymMetaclass symbol 
                 */
                CVmObjClass *cl = (CVmObjClass *)vm_objp(vmg_ symval.val.obj);
                ret = new CTcSymMetaclass(sym, len, FALSE,
                                          cl->get_meta_idx(),
                                          symval.val.obj);
            }
            else if (vm_objp(vmg_ symval.val.obj)->get_invoker(vmg_ 0))
            {
                /* 
                 *   it's an invokable object - create a function-like object
                 *   symbol for it 
                 */
                ret = new CTcSymFuncObj(sym, len, FALSE, symval.val.obj,
                                        FALSE, TC_META_UNKNOWN, 0);
            }
            else
            {
                /* 
                 *   It's not an IntrinsicClass object, so it's a regular
                 *   CTcSymObj symbol.  Check for special metaclasses that
                 *   the compiler is aware of.  
                 */
                tc_metaclass_t meta = TC_META_UNKNOWN;

                if (CVmObjTads::is_tadsobj_obj(vmg_ symval.val.obj))
                    meta = TC_META_TADSOBJ;
                else if (CVmObjGramProd::is_gramprod_obj(vmg_ symval.val.obj))
                    meta = TC_META_GRAMPROD;
                else if (CVmObjDict::is_dictionary_obj(vmg_ symval.val.obj))
                    meta = TC_META_DICT;
                else if (CVmObjIntClsMod::is_intcls_mod_obj(vmg_ symval.val.obj))
                    meta = TC_META_ICMOD;

                /* create the object symbol */
                ret = new CTcSymObj(sym, len, FALSE, symval.val.obj,
                                    FALSE, meta, 0);
            }
            break;

        case VM_PROP:
            /* it's a property ID - map to a CTcSymProp */
            ret = new CTcSymProp(sym, len, FALSE, symval.val.prop);
            break;

        case VM_FUNCPTR:
            /* it's a function pointer */
            {
                /* get the method header, for the parameter information */
                CVmFuncPtr fp(vmg_ symval.val.ofs);

                /* set up the resolved function symbol */
                ret = new CTcSymFunc(
                    sym, len, FALSE,
                    fp.get_min_argc(), fp.get_opt_argc(),
                    fp.is_varargs(), TRUE,
                    FALSE, FALSE, FALSE, TRUE);

                /* we know the absolute code address - set it */
                ((CTcSymFunc *)ret)->set_abs_addr(symval.val.ofs);
            }
            break;

        case VM_ENUM:
            /* enumerated constant */
            ret = new CTcSymEnum(sym, len, FALSE,
                                 symval.val.enumval, FALSE);
            break;

        case VM_BIFPTR:
            /* built-in function pointer */
            ret = 0;
            {
                /* decode the function pointer */
                ushort set_idx = symval.val.bifptr.set_idx;
                ushort func_idx = symval.val.bifptr.func_idx;
                
                /* get the function descriptor */
                const vm_bif_desc *desc =
                    G_bif_table->get_desc(set_idx, func_idx);

                /* if there's a valid descriptor, set up the symbol */
                if (desc != 0 && desc->func != 0)
                {
                    /* found it - set up the symbol */
                    ret = new CTcSymBif(sym, len, FALSE,
                                        set_idx, func_idx, TRUE,
                                        desc->min_argc,
                                        desc->min_argc + desc->opt_argc,
                                        desc->varargs);
                }
            }
            break;

        default:
            /* 
             *   it's either not in the table or it's not a type we can map
             *   to a compiler symbol - return 'not found' 
             */
            ret = 0;
            break;
        }

        /* done with the gc protection */
        G_stk->discard(2);

        /* return the symbol */
        return ret;
    }

    /* VM globals */
    vm_globals *gptr_;

    /* the local and global symbol table objects */
    vm_obj_id_t globals_;
    vm_obj_id_t locals_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Dynamic compiler macro table.  This works a lot like our global symbol
 *   table, to provide program-defined macros to the tokenizer during dynamic
 *   compilation.  
 */
class CVmDynFuncMacros:  public CTcMacroTable
{
public:
    CVmDynFuncMacros(VMG_ vm_obj_id_t tab)
        : hash_(512, new CVmHashFuncCS(), TRUE)
    {
        /* remember the globals */
        globals_ = VMGLOB_ADDR;

        /* remember my symbol table object */
        tab_ = tab;
    }

    /* find an entry */
    CVmHashEntry *find(const char *sym, size_t len)
    {
        CVmHashEntry *entry;
        
        /* first, check to see if it's our underlying table */
        entry = hash_.find(sym, len);
        if (entry != 0)
            return entry;

        /* if there's no user-code table, fail */
        if (tab_ == VM_INVALID_OBJ)
            return 0;

        /* establish access to globals */
        VMGLOB_PTR(globals_);

        /* create a string for the symbol name; stack it for gc protection */
        vm_val_t symstr;
        symstr.set_obj(CVmObjString::create(vmg_ FALSE, sym, len));
        G_stk->push(&symstr);

        /* look up the symbol string in the table */
        vm_val_t symval;
        vm_objp(vmg_ tab_)->index_val_ov(vmg_ &symval, tab_, &symstr);
        G_stk->push(&symval);

        /* it has to be a list, and it has to have at least three elements */
        const char *lst = symval.get_as_list(vmg0_);
        if (lst != 0 && vmb_get_len(lst) >= 3)
        {
            vm_val_t ele, subele;
            const char *exp, *arglst;
            uint flags = 0;
            int argc;
            const char **argv = 0;
            size_t *argvlen = 0;
            int i;
            
            /* the first element is the expansion string */
            CVmObjList::index_list(vmg_ &ele, lst, 1);
            if ((exp = ele.get_as_string(vmg0_)) == 0)
                goto done;

            /* the second element is the argument list */
            CVmObjList::index_list(vmg_ &ele, lst, 2);
            if ((arglst = ele.get_as_list(vmg0_)) == 0)
                goto done;

            /* figure the argument count */
            argc = vmb_get_len(arglst);

            /* build the argument vector */
            if (argc != 0)
            {
                /* allocate the string and length arrays */
                argv = new const char*[argc];
                argvlen = new size_t[argc];

                /* populate the arrays */
                for (i = 0 ; i < argc ; ++i)
                {
                    /* retrieve this argument name string */
                    CVmObjList::index_list(vmg_ &subele, arglst, i+1);
                    const char *n = subele.get_as_string(vmg0_);
                    if (n == 0)
                        goto done;

                    /* add it to the string and length arrays */
                    argv[i] = n + VMB_LEN;
                    argvlen[i] = vmb_get_len(n);
                }
            }

            /* the third element is the flags */
            CVmObjList::index_list(vmg_ &ele, lst, 3);
            if (!ele.is_numeric(vmg0_))
                goto done;
            flags = ele.num_to_int(vmg0_);

            /* create the entry */
            entry = new CTcHashEntryPpDefine(
                sym, len, TRUE, (flags & 1) != 0, argc, (flags & 2) != 0,
                argv, argvlen, exp + VMB_LEN, vmb_get_len(exp));

            /* add the entry to our cache */
            hash_.add(entry);

        done:
            /* free the argument list */
            if (argv != 0)
                delete [] argv;
            if (argvlen != 0)
                delete [] argvlen;
        }

        /* done with our gc protection */
        G_stk->discard(2);

        /* return what we found */
        return entry;
    }

    /* add an entry - ignore */
    void add(CVmHashEntry *) { }

    /* remove an entry - ignore */
    void remove(CVmHashEntry *) { }

    /* enumerate entries - ignore */
    void enum_entries(void (*)(void *, class CVmHashEntry *), void *) { }

    /* dump to a file - ignore */
    void debug_dump() { }

private:
    /* VM globals */
    vm_globals *globals_;

    /* the user LookupTable object */
    vm_obj_id_t tab_;

    /* our cache of symbols we've already translated */
    CVmHashTable hash_;
};


/* ------------------------------------------------------------------------ */
/*
 *   Grammar production alternative parser - compiler interface for dynamic
 *   compilation 
 */
class CTcGramAltFuncsDyn: public CTcGramAltFuncs
{
public:
    /* initialize with the parser object */
    CTcGramAltFuncsDyn(CTcPrsSymtab *symtab) { this->symtab = symtab; }

    /* look up a property - look up or add to the global symbols */
    CTcSymProp *look_up_prop(const CTcToken *tok, int show_err)
    {
        /* look up the symbol */
        CTcSymProp *prop = (CTcSymProp *)symtab->find(
            tok->get_text(), tok->get_text_len());

        /* if it's already defined, make sure it's a property */
        if (prop != 0)
        {
            /* make sure it's a property */
            if (prop->get_type() != TC_SYM_PROP)
            {
                /* log an error if appropriate */
                if (show_err)
                {
                    G_tok->log_error(
                        TCERR_REDEF_AS_PROP,
                        (int)tok->get_text_len(), tok->get_text());
                }

                /* we can't use the symbol, so forget it */
                prop = 0;
            }
        }
        else
        {
            /* 
             *   undefined - we can't add it for dynamic compilation, so log
             *   an error 
             */
            if (show_err)
            {
                G_tok->log_error(
                    TCERR_UNDEF_SYM,
                    (int)tok->get_text_len(), tok->get_text());
            }
        }

        /* return the property */
        return prop;
    }

    /* declare a grammar production symbol */
    CTcGramProdEntry *declare_gramprod(const char *txt, size_t len)
    {
        /* 
         *   we can't create symbols in the dynamic compiler - log an error
         *   and return failure 
         */
        G_tok->log_error(TCERR_UNDEF_SYM, (int)len, txt);
        return 0;
    }

    /* 
     *   Check an enum for use as a production token.  For dynamic
     *   compilation, we don't enforce the 'enum token' requirement that the
     *   static compiler applies, because we don't have any information on
     *   the 'enum token' attribute in the basic run-time symbol table. 
     */
    void check_enum_tok(class CTcSymEnum *) { }

    /* 
     *   EOF in the middle of an alternative - when we're parsing a
     *   stand-alone grammar rule list, the rule list is the whole input
     *   text, so EOF just means we've reached the end 
     */
    void on_eof(int * /*err*/) { }

    /* the parser's global symbol table */
    CTcPrsSymtab *symtab;
};


/* ------------------------------------------------------------------------ */
/*
 *   Dynamic compiler interface to the VM. 
 */
class CVmDynFuncVMIfc: public CTcVMIfc
{
public:
    CVmDynFuncVMIfc(VMG0_)
    {
        /* remember the globals */
        vmg = VMGLOB_ADDR;

        /* we haven't saved any objects on the stack yet */
        gc_cnt = 0;
    }

    /* create a live object in the VM */
    virtual tctarg_obj_id_t new_obj(tctarg_obj_id_t cls)
    {
        /* establish access to globals */
        VMGLOB_PTR(vmg);

        /* create a TadsObject instance */
        int argc = 0;
        if (cls != VM_INVALID_OBJ)
        {
            G_stk->push_obj(vmg_ cls);
            ++argc;
        }
        vm_obj_id_t id = CVmObjTads::create_from_stack(vmg_ 0, argc);

        /* push it for gc protection */
        G_stk->push_check()->set_obj(id);
        ++gc_cnt;

        /* return the new ID */
        return id;
    }

    /* validate an object value */
    virtual int validate_obj(tctarg_obj_id_t obj)
    {
        VMGLOB_PTR(vmg);
        return G_obj_table->is_obj_id_valid(obj);
    }

    /* add a property value to an object */
    virtual void set_prop(tctarg_obj_id_t obj, tctarg_prop_id_t prop,
                          const CTcConstVal *c)
    {
        /* establish access to globals */
        VMGLOB_PTR(vmg);

        /* set up a value based on the constant value */
        vm_val_t val;
        switch (c->get_type())
        {
        case TC_CVT_TRUE:
            val.set_true();
            break;

        case TC_CVT_INT:
            val.set_int(c->get_val_int());
            break;

        case TC_CVT_SSTR:
            val.set_obj(CVmObjString::create(
                vmg_ FALSE, c->get_val_str(), c->get_val_str_len()));
            break;

        case TC_CVT_OBJ:
            val.set_obj(c->get_val_obj());
            break;

        case TC_CVT_PROP:
            val.set_propid((vm_prop_id_t)c->get_val_prop());
            break;

        case TC_CVT_ENUM:
            val.set_enum(c->get_val_enum());
            break;

        case TC_CVT_FLOAT:
            val.set_obj(CVmObjBigNum::create(
                vmg_ FALSE, c->get_val_float(), c->get_val_float_len()));
            break;
                
        case TC_CVT_LIST:
        case TC_CVT_FUNCPTR:
        case TC_CVT_ANONFUNCPTR:
            // $$$ implement later if necessary
            val.set_nil();
            break;

        case TC_CVT_NIL:
        case TC_CVT_UNK:
        default:
            val.set_nil();
            break;
        }

        /* set the property in the object */
        vm_objp(vmg_ obj)->set_prop(vmg_ G_undo, obj, prop, &val);
    }

    /* validate a property value */
    virtual int validate_prop(tctarg_prop_id_t prop)
    {
        VMGLOB_PTR(vmg);
        return prop <= G_image_loader->get_last_prop(vmg0_);
    }

    /* validate a built-in function pointer */
    virtual int validate_bif(uint set_index, uint func_index)
    {
        VMGLOB_PTR(vmg);
        return G_bif_table->validate_entry(set_index, func_index);
    }

    /* validate a constant pool list address */
    virtual int validate_pool_str(uint32_t ofs)
    {
        /* establish globals */
        VMGLOB_PTR(vmg);

        /* validate the starting offset and full length prefix */
        if (!G_const_pool->validate_ofs(ofs)
            || !G_const_pool->validate_ofs(ofs+1))
            return FALSE;

        /* get the length */
        size_t len = vmb_get_len(G_const_pool->get_ptr(ofs));

        /* make sure the end of the string is valid as well */
        if (!G_const_pool->validate_ofs(ofs + VMB_LEN + len - 1))
            return FALSE;

        /* success */
        return TRUE;
    }

    /* validate a constant pool string address */
    virtual int validate_pool_list(uint32_t ofs)
    {
        /* establish globals */
        VMGLOB_PTR(vmg);

        /* validate the starting offset and full length prefix */
        if (!G_const_pool->validate_ofs(ofs)
            || !G_const_pool->validate_ofs(ofs+1))
            return FALSE;

        /* get the length */
        const char *p = G_const_pool->get_ptr(ofs);
        size_t len = vmb_get_len(p);
        p += VMB_LEN;

        /* make sure the end of the string is valid as well */
        if (!G_const_pool->validate_ofs(
            ofs + VMB_LEN + (len*VMB_DATAHOLDER) - 1))
            return FALSE;

        /* validate that each element is valid */
        for ( ; len != 0 ; p += VMB_DATAHOLDER, --len)
        {
            if (*p < 0 || *p >= (int)VM_FIRST_INVALID_TYPE)
                return FALSE;
        }

        /* success */
        return TRUE;
    }

    /* discard items pushed on the stack for gc protection */
    void discard()
    {
        VMGLOB_PTR(vmg);
        G_stk->discard(gc_cnt);
    }

protected:
    /* number of objects we've pushed for gc protection */
    int gc_cnt;
    
    /* VM globals */
    vm_globals *vmg;
};


/* ------------------------------------------------------------------------ */
/*
 *   Dynamic compiler results object 
 */

/* throw an error based on the results */
void CVmDynCompResults::throw_error(VMG0_)
{
    /* push the message text as the exception constructor argument */
    if (msgbuf != 0)
    {
        /* push the message as a string object */
        G_stk->push()->set_obj(CVmObjString::create(
            vmg_ FALSE, msgbuf, strlen(msgbuf)));

        /* done with the message buffer */
        free_msgbuf();
    }
    else
    {
        /* no message - push a nil argument */
        G_stk->push()->set_nil();
    }
    
    /* throw the error */
    G_interpreter->throw_new_class(vmg_ G_predef->compiler_exc, 1,
                                   "code compilation failed");
}


/* ------------------------------------------------------------------------ */
/*
 *   Dynamic Compiler Interface 
 */


/*
 *   Construction 
 */
CVmDynamicCompiler::CVmDynamicCompiler(VMG0_)
{
    /* create our host interface */
    hostifc_ = new CTcHostIfcDynComp();

    /* initialize the compiler */
    CTcMain::init(hostifc_, G_host_ifc->get_sys_res_loader(), "utf8");

    /* 
     *   set up the compiler size globals from the corresponding load image
     *   size records, so that dynamically generated byte code conforms to
     *   the image file settings 
     */
    G_sizes.mhdr = G_interpreter->get_funchdr_size();
    G_sizes.exc_entry = G_exc_entry_size;
    G_sizes.dbg_hdr = G_dbg_hdr_size;
    G_sizes.dbg_line = G_line_entry_size;
    G_sizes.lcl_hdr = G_dbg_lclsym_hdr_size;
    G_sizes.dbg_fmt_vsn = G_dbg_fmt_vsn;
    G_sizes.dbg_frame = G_dbg_frame_size;

    /* point the compiler to the loaded metaclass table */
    G_metaclass_tab = G_meta_table;
}

/*
 *   Destruction 
 */
CVmDynamicCompiler::~CVmDynamicCompiler()
{
    /* terminate the compiler */
    CTcMain::terminate();

    /* delete our host interface */
    delete hostifc_;
}

/*
 *   Get or create the global singleton instance. 
 */
CVmDynamicCompiler *CVmDynamicCompiler::get(VMG0_)
{
    /* if there's no compiler object, create one */
    if (G_dyncomp == 0)
        G_dyncomp = new CVmDynamicCompiler(vmg0_);

    /* return the instance */
    return G_dyncomp;
}

/*
 *   Compile source code 
 */
vm_obj_id_t CVmDynamicCompiler::compile(
    VMG_ int in_root_set,
    vm_obj_id_t globals, vm_obj_id_t locals, vm_obj_id_t macros,
    const vm_val_t *srcval, const char *src, size_t srclen,
    CVmDynCompMode mode, CVmDynCompDebug *dbg, CVmDynCompResults *results)
{
    vm_obj_id_t coid = 0;
    CTcPrsSymtab *old_global_symtab;
    CTcMacroTable *old_macros;
    CVmDynFuncMacros *new_macros = 0;
    int pushcnt = 0;
    CTcPrsNode *node = 0;
    int frame_has_self = FALSE;
    CVmObjFrameDesc *framedesc = 0;
    CVmObjFrameRef *frameref = 0;
    
    /* 
     *   save the parser memory pool state, so we can reset it when we're
     *   done (this allows us to discard any parser memory we allocate while
     *   we're working - we only need it while compiling, and can discard it
     *   when we're done) 
     */
    tcprsmem_state_t prsmem_state;
    G_prsmem->save_state(&prsmem_state);

    /* 
     *   Set the local symbol table context.  If the caller provided a
     *   debugger context, use the debugger symbol table; otherwise there's
     *   no local symbol context. 
     */
    CTcPrsDbgSymtab *old_symtab = G_prs->set_debug_symtab(
        dbg != 0 ? dbg->symtab : 0);

    /* don't allow #define or other directives for dynamic compilation */
    G_tok->enable_pp(FALSE);

    /* enable the special debugger syntax if it's for the debugger */
    G_prs->set_debug_expr(dbg != 0);

    /* set up a dynamic VM interface for the compiler */
    CVmDynFuncVMIfc vmifc Pvmg0_P;
    G_vmifc = &vmifc;

    /* 
     *   If there's no debugger context, set up the global symbol table and
     *   the new macro table.
     */
    if (dbg == 0)
    {
        /* install a new symbol table; remember the old one to restore */
        old_global_symtab = G_prs->set_global_symtab(
            new CVmDynFuncSymtab(vmg_ globals, locals));

        /* install a new macro table; remember the old one to restore */
        old_macros = G_tok->set_defines_table(
            new_macros = new CVmDynFuncMacros(vmg_ macros));
    }
    
    /* presume no error will occur */
    int errcode = 0, errflag = FALSE;
    results->err = 0;
    results->free_msgbuf();

    /* presume we won't generate a statement node */
    CTPNCodeBody *cb = 0;

    /* catch any errors that occur during parsing or code generation */
    err_try
    {
        /* 
         *   If there's a local stack frame object, note if it has a method
         *   context ('self', etc - it's sufficient to check that 'self' is
         *   not nil).  If so, code defined with 'function()' uses the method
         *   context of the enclosing stack frame, in analogy to an anonymous
         *   function using its enclosing lexical scope's method context.  If
         *   we have multiple stack frames, consider only the innermost.  
         */
        if (locals != VM_INVALID_OBJ)
        {
            /* presume we have just a single frame object */
            vm_obj_id_t frame = locals;

            /* 
             *   if it's not in fact a stack frame object, and it's
             *   list-like, it's a collection of frames ordered from
             *   innermost to outermost: so retrieve the first element to get
             *   the innermost frame 
             */
            CVmObject *localp = vm_objp(vmg_ locals);
            if (!CVmObjFrameDesc::is_framedesc_obj(vmg_ locals)
                && localp->is_listlike(vmg_ locals)
                && localp->ll_length(vmg_ locals) > 0)
            {
                /* 
                 *   it's a list - retrieve its first element; if that's an
                 *   object, use it as the frame object 
                 */
                vm_val_t ele, v1;
                v1.set_int(1);
                localp->index_val_ov(vmg_ &ele, locals, &v1);
                if (ele.typ == VM_OBJ)
                    frame = ele.val.obj;
            }

            /* if we now have a frame object, check its 'self' */
            if (CVmObjFrameDesc::is_framedesc_obj(vmg_ frame))
            {
                /* get the frame object, suitably cast */
                framedesc = (CVmObjFrameDesc *)vm_objp(vmg_ frame);
                frameref = framedesc->get_frame_ref(vmg0_);

                /* get 'self' from the frame */
                vm_val_t self;
                frameref->get_self(vmg_ &self);

                /* if that's not nil, we have a method context */
                frame_has_self = (self.typ != VM_NIL);
            }
        }

        /* reset the compiler error counters for the new parse */
        G_tcmain->reset_error_counts();

        /* clear any old error messages in our host interface */
        hostifc_->reset_messages();

        /* point the tokenizer to the source string */
        G_tok->set_source_buf(src, srclen);

        /* read the first token */
        G_tok->next();

        /* parse the source code */
        switch (mode)
        {
        case DCModeExpression:
            /* parse an expression */
            if ((node = G_prs->parse_expr()) != 0)
            {
                /* check some extra information for the debugger */
                // $$$ does this need to be deferred until after folding
                //     constants and adjust_for_dyn???
                if (dbg != 0 && node != 0)
                {
                    /* the debugger wants to know if it's an lvalue */
                    dbg->is_lval = node->check_lvalue_resolved(
                        G_prs->get_global_symtab());
                }
                
                /* wrap it in a 'return' and a code body */
                cb = new CTPNCodeBody(G_prs->get_global_symtab(), 0,
                                      new CTPNStmReturn(node),
                                      0, 0, FALSE, FALSE, 0, 0,
                                      dbg != 0 ? dbg->self_valid : FALSE, 0);

                /* mark it as a dynamic function */
                cb->set_dyn_func(TRUE);
            }
            break;

        case DCModeAuto:
            /* 
             *   Auto-sensing mode - parse a function or expression based on
             *   what the string looks like.
             *   
             *   If the first token is 'function', we have a function
             *   definition of the form "function(args) { body }".  This is
             *   compiled as an ordinary function with no name.
             *   
             *   If the first token is 'method', we have a method definition
             *   of the form "method(args) { body }".  This is compiled as a
             *   floating method definition with no name.
             *   
             *   Otherwise, we have an expression.  We compile this as an
             *   expression, then wrap it in a "return" statement and a
             *   function code body with zero arguments.  
             */
            if (G_tok->cur() == TOKT_FUNCTION)
            {
                /* skip the 'function' token */
                G_tok->next();

                /* parse the code body */
                cb = G_prs->parse_code_body(FALSE, FALSE, frame_has_self,
                                            0, 0, 0, 0, 0, 0, &errflag, 0,
                                            TCPRS_CB_NORMAL, 0, 0, 0, 0);

                /* mark it as a dynamic function */
                cb->set_dyn_func(TRUE);
            }
            else if (G_tok->cur() == TOKT_METHOD)
            {
                /* skip the 'method' token */
                G_tok->next();

                /* parse the code body */
                cb = G_prs->parse_code_body(FALSE, FALSE, TRUE,
                                            0, 0, 0, 0, 0, 0, &errflag, 0,
                                            TCPRS_CB_NORMAL, 0, 0, 0, 0);

                /* mark it as a dynamic method */
                cb->set_dyn_method(TRUE);
            }
            else
            {
                /* parse an expression */
                if ((node = G_prs->parse_expr()) != 0)
                {
                    /* wrap it in a 'return' an a zero-argument code body */
                    cb = new CTPNCodeBody(G_prs->get_global_symtab(), 0,
                                          new CTPNStmReturn(node),
                                          0, 0, FALSE, FALSE, 0, 0,
                                          dbg != 0 ? dbg->self_valid : FALSE,
                                          0);

                    /* mark it as a dynamic function */
                    cb->set_dyn_func(TRUE);
                }
            }
            break;

        case DCModeGramAlt:
            /* parse a grammar alternative list */
            {
                /* set up a dummy symbol to represent the match object */
                CTcSymObj *gram_obj = new CTcSymObj(
                    ".anon", 5, FALSE, VM_INVALID_OBJ,
                    FALSE, TC_META_TADSOBJ, 0);

                /* set up a production to hold the list */
                CTcGramProdEntry *prod =
                    new (G_prsmem) CTcGramProdEntry(gram_obj);

                /* parse the alternative list */
                CTcGramPropArrows arrows;
                CTcGramAltFuncsDyn funcs(G_prs->get_global_symtab());
                G_prs->parse_gram_alts(
                    &errflag, gram_obj, prod, &arrows, &funcs);

                /* 
                 *   if the parse was successful, send the parsed data back
                 *   to the caller 
                 */
                if (!errflag && G_tcmain->get_error_count() == 0)
                    results->save_grammar(vmg_ prod->get_alt_head(), &arrows);
            }
            break;
        }

        /* 
         *   if we didn't get a parse node out of the deal, or any compiler
         *   errors occurred, fail 
         */
        if (cb == 0 || errflag || G_tcmain->get_error_count() != 0)
            goto compilation_done;

        /* assign an object ID for CodeBody for the main code we parsed */
        coid = vm_new_id(vmg_ in_root_set, TRUE, FALSE);
        cb->set_dyn_obj_id(coid);

        /* 
         *   create a dummy object for it - we'll replace it with a real
         *   object when actually generating the code 
         */
        new (vmg_ coid) CVmDynamicFunc();

        /* save it on the stack for GC protection */
        G_stk->push_check()->set_obj(coid);
        ++pushcnt;

        /*
         *   Allocate object IDs for the nested code bodies.  Each nested
         *   code body requires its own separate CodeBody object.  Nested
         *   code bodies are typically referenced from the main code body,
         *   and might cross-reference one another, so in order to generate
         *   code for any of the code bodies, we have to pre-assign an object
         *   ID to *all* code bodies first.
         */
        for (CTPNStmTop *ts = G_prs->get_first_nested_stm() ; ts != 0 ;
             ts = ts->get_next_stm_top())
        {
            /* allocate and assign the object ID */
            vm_obj_id_t id = vm_new_id(vmg_ in_root_set, TRUE, FALSE);
            new (vmg_ id) CVmDynamicFunc();
            ts->set_dyn_obj_id(id);

            /* save it on the stack for GC protection */
            G_stk->push_check()->set_obj(id);
            ++pushcnt;
        }

        /* generate the code for the main object */
        if (!gen_code_body(vmg_ cb, srcval, dbg))
            goto compilation_done;

        /*
         *   If necessary, store the method context in the object.  This is
         *   required if this is defined with 'function' rather than
         *   'method', and it references 'self' or other method context
         *   variables, and there's a local frame to use for the method
         *   context. 
         */
        if (frame_has_self
            && cb->is_dyn_func()
            && (cb->self_referenced() || cb->full_method_ctx_referenced()))
        {
            /* 
             *   We need the method context.  The context is simply the
             *   'self' object if the rest of the context wasn't referenced,
             *   otherwise it's the context object suitable for the LOADCTX
             *   opcode.  
             */
            vm_val_t mctx;
            if (cb->full_method_ctx_referenced())
            {
                /* we need the full method context object for LOADCTX */
                frameref->create_loadctx_obj(vmg_ &mctx);
            }
            else
            {
                /* the context is simply the 'self' object */
                frameref->get_self(vmg_ &mctx);
            }

            /* save the method context in the DynamicFunc */
            ((CVmDynamicFunc *)vm_objp(vmg_ coid))->set_method_ctx(&mctx);
        }

        /* generate each nested statement code body */
        for (CTPNStmTop *ts = G_prs->get_first_nested_stm() ; ts != 0 ;
             ts = ts->get_next_stm_top())
        {
            if (!gen_code_body(vmg_ ts, 0, dbg))
                goto compilation_done;
        }

    compilation_done: ;
    }
    err_catch(exc)
    {
        /* note the error code */
        errcode = exc->get_error_code();
        
        /* if there were no logged errors, use the error from the exception */
        if (G_tcmain->get_error_count() == 0)
        {
            /* delete any old message text */
            results->free_msgbuf();

            /* format the message into the result object */
            const char *msg = tcerr_get_msg(errcode, FALSE);
            results->msgbuf = err_format_msg(msg, exc);
        }

        /* we don't have a valid object */
        coid = VM_INVALID_OBJ;
    }
    err_end;

    /* if a compilation error occurred, return failure */
    if (errcode != 0 || errflag)
    {
        /* parsing failed - stop now */
        coid = VM_INVALID_OBJ;
        goto done;
    }
    else if (G_tcmain->get_error_count() != 0 || coid == VM_INVALID_OBJ)
    {
        /* parse errors were reported - retrieve the first and stop */
        errcode = G_tcmain->get_first_error();
        coid = VM_INVALID_OBJ;
        goto done;
    }

done:
    /* restore the original symbol table and macros in the parser */
    G_prs->set_debug_symtab(old_symtab);

    /* reset the code generator streams */
    G_ds->reset();
    G_cs->reset();
    G_os->reset();

    /* restore the old global symbol and macro tables */
    if (dbg == 0)
    {
        G_prs->set_global_symtab(old_global_symtab);
        G_tok->set_defines_table(old_macros);
    }

    /* delete the macro table if we created one */
    if (new_macros != 0)
        delete new_macros;

    /* reset the parser and tokenizer */
    G_prs->reset();
    G_tok->reset();

    /* we're done with the compiler's working state */
    G_prsmem->reset(&prsmem_state);

    /* 
     *   if there were logged compiler errors, and we haven't already
     *   returned an exception message, return the captured compiler error
     *   messages from the host interface 
     */
    if (G_tcmain->get_error_count() != 0 && results->msgbuf == 0)
        results->msgbuf = lib_copy_str(hostifc_->get_error_msg());

    /* return the error code result */
    results->err = errcode;

    /* discard our GC protection, plus the vmifc's */
    G_stk->discard(pushcnt);
    vmifc.discard();

    /* return the DynamicFunc we created, if any */
    return coid;
}

/*
 *   Generate code for a parsed code body.  Parsing a single block of source
 *   code might yield multiple parsed code bodies, because anonymous
 *   functions and other nested structures require separate top-level code
 *   bodies.  
 */
int CVmDynamicCompiler::gen_code_body(
    VMG_ CTPNStmTop *node, const vm_val_t *srcval, CVmDynCompDebug *dbg)
{
    int i;
    CTcIdFixup *fixup;

    /* clear the object fixup list - we want just this object's fixups */
    G_objfixup = 0;

    /* our code will start at the current code stream write pointer */
    ulong start_ofs = G_cs->get_ofs();

    /* fold constants */
    if (node->fold_constants(G_prs->get_global_symtab()) == 0)
        return FALSE;

    /* 
     *   Make adjustments for dynamic compilation.  If the caller provided
     *   debugger evaluation options, use the dynamic adjustment options
     *   specified there; otherwise use defaults.  
     */
    tcpn_dyncomp_info di;
    if (node->adjust_for_dyn(dbg != 0 ? &dbg->di : &di) == 0)
        return FALSE;

    /* 
     *   Set the code generation mode.  If we have debugger options, set
     *   debug mode; otherwise set dynamic compilation mode.  
     */
    if (dbg != 0)
        G_cg->set_debug_eval(dbg->di.speculative,
                             dbg->di.stack_level);
    else
        G_cg->set_dyn_eval();

    /* 
     *   Keep object fixups - we need these to find object references in the
     *   generated bytecode.  We don't need property or enum fixups, since
     *   we're building against a loaded image file with all of those types
     *   already resolved.  
     */
    G_keep_objfixups = TRUE;
    
    /* make the global symbol table active for code generation */
    G_cs->set_symtab(G_prs->get_global_symtab());
    
    /* generate the code */
    node->gen_code(FALSE, FALSE);

    /* if any errors occurred during code generation, return failure */
    if (G_tcmain->get_error_count() != 0)
        return FALSE;

    /*
     *   Go through the object fixup list and count up references to objects
     *   that aren't in the root set.  We need to keep a list of these
     *   references in the generated code object, because the code object
     *   needs to manage them like any other object manages its live
     *   references: mark them during garbage collection, fix them up during
     *   save/restore, etc.  
     */
    int refcnt = 0;
    for (fixup = G_objfixup ; fixup != 0 ; fixup = fixup->nxt_)
    {
        /* if it refers to a non-root-set object, count it */
        if (!G_obj_table->is_obj_in_root_set((vm_obj_id_t)fixup->id_))
            ++refcnt;
    }

    /* figure the bytecode length for allocating the DynamicFunc instance */
    size_t bytecode_rem = (size_t)(G_cs->get_ofs() - start_ofs);
    
    /* the caller already allocated our object ID - retrieve it */
    vm_obj_id_t coid = node->get_dyn_obj_id();

    /* create the C++ object */
    CVmDynamicFunc *co = new (vmg_ coid) CVmDynamicFunc(
        vmg_ coid, srcval, bytecode_rem, refcnt);

    /* save the object reference list */
    for (i = 0, fixup = G_objfixup ; fixup != 0 ; fixup = fixup->nxt_)
    {
        /* if it refers to a non-root-set object, save it */
        if (!G_obj_table->is_obj_in_root_set((vm_obj_id_t)fixup->id_))
            co->get_ext()->obj_refs[i++] = fixup->ofs_ - start_ofs;
    }

    /* copy the code */
    char *bc = co->get_ext()->get_bytecode_ptr();
    for (size_t blkofs = 0 ; bytecode_rem != 0 ; )
    {
        /* get the next block */
        ulong blklen;
        const char *blkp = G_cs->get_block_ptr(
            start_ofs + blkofs, bytecode_rem, &blklen);

        /* copy the data */
        memcpy(bc + blkofs, blkp, blklen);

        /* advance past the copied data */
        blkofs += blklen;
        bytecode_rem -= blklen;
    }

    /* success */
    return TRUE;
}

