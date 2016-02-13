/* 
 *   Copyright (c) 2002 by Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmpat.cpp - regular-expression compiled pattern object
Function
  
Notes
  
Modified
  08/27/02 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <os.h>
#include "vmtype.h"
#include "vmobj.h"
#include "vmmeta.h"
#include "vmglob.h"
#include "vmregex.h"
#include "vmpat.h"
#include "vmstack.h"
#include "vmbif.h"
#include "vmbiftad.h"
#include "vmfile.h"
#include "vmrun.h"


/* ------------------------------------------------------------------------ */
/*
 *   Statics 
 */
static CVmMetaclassPattern metaclass_reg_obj;
CVmMetaclass *CVmObjPattern::metaclass_reg_ = &metaclass_reg_obj;

/* function table */
int (CVmObjPattern::
     *CVmObjPattern::func_table_[])(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *argc) =
{
    &CVmObjPattern::getp_undef,
    &CVmObjPattern::getp_get_str
};

/* ------------------------------------------------------------------------ */
/*
 *   create 
 */
CVmObjPattern::CVmObjPattern(VMG_ re_compiled_pattern *pat,
                             const vm_val_t *src_str)
{
    /* allocate my extension data */
    ext_ = (char *)G_mem->get_var_heap()
           ->alloc_mem(sizeof(vmobj_pat_ext), this);

    /* remember my source data */
    set_orig_str(src_str);

    /* remember the compiled pattern */
    set_pattern(pat);
}

/* ------------------------------------------------------------------------ */
/*
 *   notify of deletion 
 */
void CVmObjPattern::notify_delete(VMG_ int in_root_set)
{
    /* free my extension data */
    if (ext_ != 0)
    {
        /* 
         *   Free my pattern, if I've compiled it.  (Note that we must not
         *   call get_pattern() here, because doing so would unnecessarily
         *   create a pattern if we haven't already done so - that would be
         *   stupid, because the only reason we're asking for it is so that
         *   we can delete it.)  
         */
        if (get_ext()->pat != 0)
            CRegexParser::free_pattern(get_ext()->pat);

        /* free the extension */
        if (!in_root_set)
            G_mem->get_var_heap()->free_mem(ext_);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Create from the stack 
 */
vm_obj_id_t CVmObjPattern::create_from_stack(VMG_ const uchar **pc_ptr,
                                             uint argc)
{
    const char *strval;
    re_status_t stat;
    re_compiled_pattern *pat;
    vm_obj_id_t id;

    /* check arguments */
    if (argc != 1)
        err_throw(VMERR_WRONG_NUM_OF_ARGS);

    /* retrieve the string, but leave it on the stack */
    strval = G_stk->get(0)->get_as_string(vmg0_);
    if (strval == 0)
        err_throw(VMERR_STRING_VAL_REQD);

    /* compile the string */
    stat = G_bif_tads_globals->rex_parser->compile_pattern(
        strval + VMB_LEN, vmb_get_len(strval), &pat);

    /* if we failed to compile the pattern, throw an error */
    if (stat != RE_STATUS_SUCCESS)
        err_throw(VMERR_BAD_TYPE_BIF);

    /* create a new pattern object to hold the pattern */
    id = vm_new_id(vmg_ FALSE, TRUE, FALSE);
    new (vmg_ id) CVmObjPattern(vmg_ pat, G_stk->get(0));

    /* discard arguments */
    G_stk->discard();

    /* return the new object */
    return id;
}

/* ------------------------------------------------------------------------ */
/* 
 *   set a property 
 */
void CVmObjPattern::set_prop(VMG_ class CVmUndo *,
                             vm_obj_id_t, vm_prop_id_t,
                             const vm_val_t *)
{
    /* we have no properties to set */
    err_throw(VMERR_INVALID_SETPROP);
}

/* ------------------------------------------------------------------------ */
/*
 *   Get a property 
 */
int CVmObjPattern::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                            vm_obj_id_t self, vm_obj_id_t *source_obj,
                            uint *argc)
{
    uint func_idx;
    
    /* translate the property into a function vector index */
    func_idx = G_meta_table
               ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop);

    /* call the function, if we found it */
    if ((this->*func_table_[func_idx])(vmg_ self, val, argc))
    {
        *source_obj = metaclass_reg_->get_class_obj(vmg0_);
        return TRUE;
    }

    /* not found - inherit default handling */
    return CVmObject::get_prop(vmg_ prop, val, self, source_obj, argc);
}


/* ------------------------------------------------------------------------ */
/*
 *   Mark references 
 */
void CVmObjPattern::mark_refs(VMG_ uint state)
{
    const vm_val_t *valp;
    
    /* if our source value is an object reference, mark it */
    if (get_ext() != 0
        && (valp = get_orig_str())->typ == VM_OBJ
        && valp->val.obj != VM_INVALID_OBJ)
    {
        /* it's a reference, so mark it */
        G_obj_table->mark_all_refs(valp->val.obj, state);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Load from an image file 
 */
void CVmObjPattern::load_from_image(VMG_ vm_obj_id_t self, const char *ptr,
                                    size_t len)
{
    /* if we don't already have an extension, allocate one */
    if (ext_ == 0)
        ext_ = (char *)G_mem->get_var_heap()
               ->alloc_mem(sizeof(vmobj_pat_ext), this);

    /* get our source value */
    vmb_get_dh(ptr, &get_ext()->str);

    /* 
     *   We haven't compiled our pattern yet.  Note that it might not be
     *   possible to obtain the text of our string at this point, because it
     *   might be another object and thus might not have been loaded yet.
     *   So, note that we have no pattern yet, and request post-load
     *   initialization, so that we can compile our pattern after we know all
     *   of the other objects have been loaded.  
     */
    set_pattern(0);
    G_obj_table->request_post_load_init(self);
}

/* ------------------------------------------------------------------------ */
/*
 *   Perform post-load initialization: we compile our pattern here.  Note
 *   that we need to wait until now to compile our pattern, since our source
 *   string could be another object, which isn't guaranteed to have been
 *   loaded until we get here.  
 */
void CVmObjPattern::post_load_init(VMG_ vm_obj_id_t self)
{
    /* make sure the original string object is initialized */
    const vm_val_t *origval = get_orig_str();
    if (origval->typ == VM_OBJ)
        G_obj_table->ensure_post_load_init(vmg_ origval->val.obj);

    /* get the string value */
    const char *strval = get_orig_str()->get_as_string(vmg0_);
    if (strval != 0)
    {
        /* if we already have a compiled pattern, delete it */
        if (get_ext()->pat != 0)
            CRegexParser::free_pattern(get_ext()->pat);

        /* compile the pattern and store the result */
        G_bif_tads_globals->rex_parser->compile_pattern(
            strval + VMB_LEN, vmb_get_len(strval), &get_ext()->pat);
    }
}

/* ------------------------------------------------------------------------ */
/* 
 *   save to a file 
 */
void CVmObjPattern::save_to_file(VMG_ class CVmFile *fp)
{
    /* write the source string reference */
    char buf[VMB_DATAHOLDER];
    vmb_put_dh(buf, get_orig_str());
    fp->write_bytes(buf, VMB_DATAHOLDER);
}

/* ------------------------------------------------------------------------ */
/* 
 *   restore from a file 
 */
void CVmObjPattern::restore_from_file(VMG_ vm_obj_id_t self,
                                      class CVmFile *fp, CVmObjFixup *fixups)
{
    /* if we don't already have an extension, allocate one */
    if (ext_ == 0)
        ext_ = (char *)G_mem->get_var_heap()
               ->alloc_mem(sizeof(vmobj_pat_ext), this);

    /* read the source string reference */
    char buf[VMB_DATAHOLDER];
    fp->read_bytes(buf, VMB_DATAHOLDER);

    /* fix it up */
    fixups->fix_dh(vmg_ buf);

    /* remember it in our extension */
    vmb_get_dh(buf, &get_ext()->str);

    /* 
     *   clear out our pattern and request post-load initialization - we
     *   can't necessarily compile it yet, because we might not have loaded
     *   the source string data, so just make a note that we need to compile
     *   it the next time we need it 
     */
    set_pattern(0);
    G_obj_table->request_post_load_init(self);
}

/* ------------------------------------------------------------------------ */
/* 
 *   property evaluator - get my original string 
 */
int CVmObjPattern::getp_get_str(VMG_ vm_obj_id_t self,
                                vm_val_t *retval, uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* retrieve my original string value */
    *retval = *get_orig_str();

    /* handled */
    return TRUE;
}
