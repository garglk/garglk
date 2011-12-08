#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 1999, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmfunc.cpp - T3 VM function header operations
Function
  
Notes
  
Modified
  11/24/99 MJRoberts  - Creation
*/

#include "vmfunc.h"
#include "vmpool.h"
#include "vmglob.h"
#include "vmtype.h"
#include "vmobj.h"
#include "vmbif.h"
#include "vmstr.h"
#include "vmdynfunc.h"


/* ------------------------------------------------------------------------ */
/*
 *   Initialize from a code pool address 
 */
CVmFuncPtr::CVmFuncPtr(VMG_ pool_ofs_t ofs)
{
    p_ = (const uchar *)G_code_pool->get_ptr(ofs);
}

/*
 *   Initialize from a vm_val_t 
 */
int CVmFuncPtr::set(VMG_ const vm_val_t *val)
{
    vm_val_t invoker;
    
    /* we might need to re-resolve from an invoker, so loop until resolved */
    for (;;)
    {
        /* check the type */
        switch (val->typ)
        {
        case VM_OBJ:
        case VM_OBJX:
            /* object - check to see if it's invokable */
            if (vm_objp(vmg_ val->val.obj)->get_invoker(vmg_ &invoker))
            {
                /* it's invokable - re-resolve using the invoker */
                val = &invoker;
            }
            else
            {
                /* other object types are not function pointers */
                set((uchar *)0);
                return FALSE;
            }
            break;
            
        case VM_FUNCPTR:
        case VM_CODEOFS:
            /* function pointer - get the address from the code pool */
            set((const uchar *)G_code_pool->get_ptr(val->val.ofs));
            return TRUE;
            
        case VM_CODEPTR:
            /* direct code pointer - the value contains the address */
            set((const uchar *)val->val.ptr);
            return TRUE;
            
        case VM_BIFPTR:
        case VM_BIFPTRX:
            /* built-in function pointer */
            {
                /* 
                 *   decode the function set information from the value, and
                 *   look up the bif descriptor in the registration table 
                 */
                const vm_bif_desc *desc = G_bif_table->get_desc(
                    val->val.bifptr.set_idx, val->val.bifptr.func_idx);
                
                /* fail if there's no descriptor */
                if (desc == 0)
                    return FALSE;
                
                /* point to the synthetic header */
                set((const uchar *)desc->synth_hdr);
            }
            return TRUE;

        default:
            /* other types are invalid */
            set((uchar *)0);
            return FALSE;
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   Get a vm_val_t pointer to this function
 */
int CVmFuncPtr::get_fnptr(VMG_ vm_val_t *v)
{
    /* 
     *   Try translating the method address to a code pool offset.  If that
     *   succeeds, it's a regular static function.  
     */
    pool_ofs_t entry_ofs;
    if (G_code_pool->get_ofs((const char *)p_, &entry_ofs))
    {
        /* it's a regular function call */
        v->set_fnptr(entry_ofs);
        return TRUE;
    }

    /* 
     *   It's not a static function pointer, so it must be a dynamic
     *   function.  A DynamicFunc stores a prefix header before the start of
     *   our method header, containing the DynamicFunc object ID.  Read the
     *   object ID, and return it as a VM_OBJ value.  
     */
    vm_obj_id_t obj = CVmDynamicFunc::get_obj_from_prefix(vmg_ p_);
    if (obj != VM_INVALID_OBJ)
    {
        v->set_obj(obj);
        return TRUE;
    }

    /* we couldn't find any valid object */
    v->set_nil();
    return FALSE;
}


/* ------------------------------------------------------------------------ */
/* 
 *   Set up an exception table pointer for this function.  Returns true if
 *   successful, false if there's no exception table.  
 */
int CVmFuncPtr::set_exc_ptr(CVmExcTablePtr *exc_ptr) const
{
    /* set the exception pointer based on my address */
    return exc_ptr->set(p_);
}

/*
 *   Set up a debug table pointer for this function.  Returns true if
 *   successful, false if there's no debug table.  
 */
int CVmFuncPtr::set_dbg_ptr(CVmDbgTablePtr *dbg_ptr)const
{
    /* set the debug pointer based on my address */
    return dbg_ptr->set(p_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Get the symbol data from a local variable symbol in the frame table
 */
const char *CVmDbgFrameSymPtr::get_symptr(VMG0_) const
{
    /* get a pointer to the symbol field in this record */
    const char *symp = (const char *)p_ + G_dbg_lclsym_hdr_size;

    /* 
     *   if it's stored inline, this points directly to the symbol data;
     *   otherwise it's a constant pool offset pointing to the symbol data 
     */
    if (is_sym_inline())
        return symp;
    else
        return G_const_pool->get_ptr(osrp4(symp));
}

void CVmDbgFrameSymPtr::get_str_val(VMG_ vm_val_t *val) const
{
    /* get a pointer to the symbol field in this record */
    const char *symp = (const char *)p_ + G_dbg_lclsym_hdr_size;

    /* check whether the symbol string is in-line or in the constant pool */
    if (is_sym_inline())
    {
        /* it's inline, so we need to create a string object for it */
        val->set_obj(CVmObjString::create(vmg_ FALSE, symp + 2, osrp2(symp)));
    }
    else
    {
        /* it's in the constant pool, so we can just return a VM_SSTR */
        val->set_sstring(osrp4(symp));
    }
}

/* ------------------------------------------------------------------------ */
/* 
 *   is this frame nested in the given frame? 
 */
int CVmDbgFramePtr::is_nested_in(VMG_ const class CVmDbgTablePtr *dp, int i)
{
    /* scan our parent list for 'i' */
    for (int parent = get_enclosing_frame() ; parent != 0 ; )
    {
        /* if this parent is 'i', we're inside 'i' */
        if (parent == i)
            return TRUE;

        /* set up a pointer to the parent frame */
        CVmDbgFramePtr fp;
        dp->set_frame_ptr(vmg_ &fp, parent);

        /* get its enclosing frame */
        parent = fp.get_enclosing_frame();
    }

    /* it's not a nested frame */
    return FALSE;
}
