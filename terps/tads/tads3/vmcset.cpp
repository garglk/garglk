/* 
 *   Copyright (c) 2001, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmcset.cpp - T3 CharacterSet metaclass
Function
  
Notes
  
Modified
  06/06/01 MJRoberts  - Creation
*/

#include <stdlib.h>
#include "vmtype.h"
#include "vmobj.h"
#include "vmglob.h"
#include "vmcset.h"
#include "vmbif.h"
#include "vmfile.h"
#include "vmerrnum.h"
#include "vmerr.h"
#include "vmstack.h"
#include "vmmeta.h"
#include "vmrun.h"
#include "charmap.h"
#include "vmstr.h"
#include "vmpredef.h"
#include "vmrun.h"
#include "vmhost.h"


/* ------------------------------------------------------------------------ */
/*
 *   statics 
 */

/* metaclass registration object */
static CVmMetaclassCharSet metaclass_reg_obj;
CVmMetaclass *CVmObjCharSet::metaclass_reg_ = &metaclass_reg_obj;

/* function table */
int (CVmObjCharSet::
     *CVmObjCharSet::func_table_[])(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *argc) =
{
    &CVmObjCharSet::getp_undef,
    &CVmObjCharSet::getp_get_name,
    &CVmObjCharSet::getp_is_known,
    &CVmObjCharSet::getp_is_mappable,
    &CVmObjCharSet::getp_is_rt_mappable
};


/* ------------------------------------------------------------------------ */
/*
 *   Create from stack 
 */
vm_obj_id_t CVmObjCharSet::create_from_stack(VMG_ const uchar **pc_ptr,
                                             uint argc)
{
    vm_obj_id_t id;
    vm_val_t *arg1;
    const char *charset_name;

    /* check our arguments */
    if (argc != 1)
        err_throw(VMERR_WRONG_NUM_OF_ARGS);

    /* get the name of the character set */
    arg1 = G_stk->get(0);
    charset_name = arg1->get_as_string(vmg0_);
    if (charset_name == 0)
        err_throw(VMERR_BAD_TYPE_BIF);

    /* create the character set object */
    id = vm_new_id(vmg_ FALSE, FALSE, FALSE);
    new (vmg_ id) CVmObjCharSet(vmg_ charset_name + VMB_LEN,
                                vmb_get_len(charset_name));

    /* discard arguments */
    G_stk->discard(argc);

    /* return the new object */
    return id;
}

/* ------------------------------------------------------------------------ */
/*
 *   Create with no contents 
 */
vm_obj_id_t CVmObjCharSet::create(VMG_ int in_root_set)
{
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
    new (vmg_ id) CVmObjCharSet();
    return id;
}

/*
 *   Create with the given character set name
 */
vm_obj_id_t CVmObjCharSet::create(VMG_ int in_root_set,
                                  const char *charset_name,
                                  size_t charset_name_len)
{
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
    new (vmg_ id) CVmObjCharSet(vmg_ charset_name, charset_name_len);
    return id;
}

/* ------------------------------------------------------------------------ */
/*
 *   Instantiate 
 */
CVmObjCharSet::CVmObjCharSet(VMG_ const char *charset_name,
                             size_t charset_name_len)
{
    /* allocate and initialize our extension */
    ext_ = 0;
    alloc_ext(vmg_ charset_name, charset_name_len);
}

/*
 *   Allocate and initialize our extension 
 */
void CVmObjCharSet::alloc_ext(VMG_ const char *charset_name,
                              size_t charset_name_len)
{
    size_t alloc_size;
    vmobj_charset_ext_t *extp;
    CResLoader *res_ldr;
    
    /* if we already have an extension, delete it */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);

    /* 
     *   compute the size we need - note that we use the one fixed byte of
     *   the structure's name element as the extra byte we need for null
     *   termination of the name 
     */
    alloc_size = sizeof(vmobj_charset_ext_t) + charset_name_len;

    /* allocate space for our extension structure */
    ext_ = (char *)G_mem->get_var_heap()->alloc_mem(alloc_size, this);

    /* cast the extension to our structure type */
    extp = (vmobj_charset_ext_t *)ext_;

    /* store the character set name and length, null-terminating the name */
    extp->charset_name_len = charset_name_len;
    memcpy(extp->charset_name, charset_name, charset_name_len);
    extp->charset_name[charset_name_len] = '\0';

    /* presume we won't be able to load the character sets */
    extp->to_local = 0;
    extp->to_uni = 0;

    /* get the resource loader */
    res_ldr = G_host_ifc->get_sys_res_loader();

    /* if we have a resource loader, load the mappings */
    if (res_ldr != 0)
    {
        /* load the unicode-to-local mapping */
        extp->to_local = CCharmapToLocal::load(res_ldr, extp->charset_name);

        /* load the local-to-unicode mapping */
        extp->to_uni = CCharmapToUni::load(res_ldr, extp->charset_name);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Notify of deletion
 */
void CVmObjCharSet::notify_delete(VMG_ int /*in_root_set*/)
{
    /* release our mapper objects */
    if (ext_ != 0)
    {
        /* release the to-local character mapper */
        if (get_ext_ptr()->to_local != 0)
            get_ext_ptr()->to_local->release_ref();

        /* release the to-unicode character mapper */
        if (get_ext_ptr()->to_uni != 0)
            get_ext_ptr()->to_uni->release_ref();

        /* free our extension */
        G_mem->get_var_heap()->free_mem(ext_);
    }
}

/* ------------------------------------------------------------------------ */
/* 
 *   set a property 
 */
void CVmObjCharSet::set_prop(VMG_ class CVmUndo *,
                             vm_obj_id_t, vm_prop_id_t,
                             const vm_val_t *)
{
    err_throw(VMERR_INVALID_SETPROP);
}

/* ------------------------------------------------------------------------ */
/* 
 *   get a property 
 */
int CVmObjCharSet::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
                            vm_obj_id_t self, vm_obj_id_t *source_obj,
                            uint *argc)
{
    uint func_idx;

    /* translate the property index to an index into our function table */
    func_idx = G_meta_table
               ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop);
    
    /* call the appropriate function */
    if ((this->*func_table_[func_idx])(vmg_ self, retval, argc))
    {
        *source_obj = metaclass_reg_->get_class_obj(vmg0_);
        return TRUE;
    }
    
    /* inherit default handling */
    return CVmObject::get_prop(vmg_ prop, retval, self, source_obj, argc);
}

/* ------------------------------------------------------------------------ */
/*
 *   load from an image file 
 */
void CVmObjCharSet::load_from_image(VMG_ vm_obj_id_t self,
                                    const char *ptr, size_t siz)
{
    /* initialize with the character set name from the image file */
    alloc_ext(vmg_ ptr + VMB_LEN, vmb_get_len(ptr));
}

/* ------------------------------------------------------------------------ */
/* 
 *   save to a file 
 */
void CVmObjCharSet::save_to_file(VMG_ class CVmFile *fp)
{
    /* write the name length */
    fp->write_uint2(get_ext_ptr()->charset_name_len);

    /* write the bytes of the name */
    fp->write_bytes(get_ext_ptr()->charset_name,
                    get_ext_ptr()->charset_name_len);
}

/* 
 *   restore from a file 
 */
void CVmObjCharSet::restore_from_file(VMG_ vm_obj_id_t self,
                                      CVmFile *fp, CVmObjFixup *)
{
    char buf[128];
    size_t len;
    size_t read_len;

    /* read the length of the character set name */
    len = fp->read_uint2();

    /* limit the reading to the length of the buffer */
    read_len = len;
    if (read_len > sizeof(buf))
        read_len = sizeof(buf);

    /* read the name, up to the buffer length */
    fp->read_bytes(buf, read_len);

    /* skip any bytes we couldn't fit in the buffer */
    if (len > read_len)
        fp->set_pos(fp->get_pos() + len - read_len);

    /* initialize from the saved data */
    alloc_ext(vmg_ buf, read_len);
}

/* ------------------------------------------------------------------------ */
/*
 *   Compare for equality 
 */
int CVmObjCharSet::equals(VMG_ vm_obj_id_t self, const vm_val_t *val,
                          int /*depth*/) const
{
    CVmObjCharSet *other;
    const vmobj_charset_ext_t *ext;
    const vmobj_charset_ext_t *other_ext;

    /* if it's a self-reference, it's certainly equal */
    if (val->typ == VM_OBJ && val->val.obj == self)
        return TRUE;

    /* if it's not another character set, it's not equal */
    if (val->typ != VM_OBJ || !is_charset(vmg_ val->val.obj))
        return FALSE;

    /* we know it's another character set - cast it */
    other = (CVmObjCharSet *)vm_objp(vmg_ val->val.obj);

    /* get my extension and the other extension */
    ext = get_ext_ptr();
    other_ext = other->get_ext_ptr();

    /* it's equal if it has the same name (ignoring case) */
    return (ext->charset_name_len == other_ext->charset_name_len
            && memicmp(ext->charset_name, other_ext->charset_name,
                       ext->charset_name_len) == 0);
}

/*
 *   Calculate a hash value 
 */
uint CVmObjCharSet::calc_hash(VMG_ vm_obj_id_t self, int /*depth*/) const
{
    uint hash;
    size_t rem;
    const char *p;

    /* add up the bytes in the array */
    for (hash = 0, rem = get_ext_ptr()->charset_name_len,
         p = get_ext_ptr()->charset_name ;
         rem != 0 ;
         --rem, ++p)
    {
        /* add this character into the hash */
        hash += *p;
    }

    /* return the result */
    return hash;
}

/* ------------------------------------------------------------------------ */
/* 
 *   property evaluator - get the character set name
 */
int CVmObjCharSet::getp_get_name(VMG_ vm_obj_id_t self,
                                 vm_val_t *retval, uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* create a new string for the name */
    retval->set_obj(CVmObjString::create(vmg_ FALSE,
                                         get_ext_ptr()->charset_name,
                                         get_ext_ptr()->charset_name_len));

    /* handled */
    return TRUE;
}

/* 
 *   property evaluator - is known
 */
int CVmObjCharSet::getp_is_known(VMG_ vm_obj_id_t self,
                                 vm_val_t *retval, uint *argc)
{
    static CVmNativeCodeDesc desc(0);
    
    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* 
     *   it's known if both of our character mappers are non-null; if either
     *   is null, the character set is not known on this platform 
     */
    retval->set_logical(get_ext_ptr()->to_local != 0
                        && get_ext_ptr()->to_uni != 0);

    /* handled */
    return TRUE;
}

/* 
 *   property evaluator - check a character or a string for mappability 
 */
int CVmObjCharSet::getp_is_mappable(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *argc)
{
    static CVmNativeCodeDesc desc(1);
    vm_val_t arg;
    const char *str;
    CCharmapToLocal *to_local;

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the local mapping */
    to_local = get_to_local(vmg0_);

    /* get the argument and check what type we have */
    G_stk->pop(&arg);
    if (to_local == 0)
    {
        /* no mapping is available, so it's not mappable */
        retval->set_nil();
    }
    else if ((str = arg.get_as_string(vmg0_)) != 0)
    {
        size_t len;
        utf8_ptr p;

        /* get the length and skip the length prefix */
        len = vmb_get_len(str);
        str += VMB_LEN;

        /* presume every character will be mappable */
        retval->set_true();

        /* check each character for mappability */
        for (p.set((char *)str) ; len != 0 ; p.inc(&len))
        {
            /* check to see if this character is mappable */
            if (!to_local->is_mappable(p.getch()))
            {
                /* 
                 *   The character isn't mappable - this is an
                 *   all-or-nothing check, so if one isn't mappable we
                 *   return false.  Set the nil return and stop looking.
                 */
                retval->set_nil();
                break;
            }
        }
    }
    else if (arg.typ == VM_INT)
    {
        /* 
         *   Check if the integer character value is mappable.  If it's out
         *   of the 16-bit unicode range (0..0xffff), it's not mappable;
         *   otherwise, ask the character mapper. 
         */
        if (arg.val.intval < 0 || arg.val.intval > 0xffff)
        {
            /* it's out of the valid unicode range, so it's not mappable */
            retval->set_nil();
        }
        else
        {
            /* ask the character mapper */
            retval->set_logical(to_local->is_mappable(
                (wchar_t)arg.val.intval));
        }
    }

    /* handled */
    return TRUE;
}

/* 
 *   property evaluator - check a character or a string to see if it has a
 *   round-trip mapping.  A round-trip mapping is one where the unicode
 *   characters can be mapped to the local character set, then back to
 *   unicode, yielding the exact original unicode string.  
 */
int CVmObjCharSet::getp_is_rt_mappable(VMG_ vm_obj_id_t self,
                                       vm_val_t *retval, uint *argc)
{
    static CVmNativeCodeDesc desc(1);
    vm_val_t arg;
    const char *str;
    CCharmapToLocal *to_local;
    CCharmapToUni *to_uni;

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the local and unicode mappings */
    to_local = get_to_local(vmg0_);
    to_uni = get_to_uni(vmg0_);

    /* get the argument and check what type we have */
    G_stk->pop(&arg);
    if (to_local == 0 || to_uni == 0)
    {
        /* no character sets, so it's not mappable */
        retval->set_nil();
    }
    else if ((str = arg.get_as_string(vmg0_)) != 0)
    {
        size_t len;
        utf8_ptr p;

        /* get the length and skip the length prefix */
        len = vmb_get_len(str);
        str += VMB_LEN;

        /* presume every character will be mappable */
        retval->set_true();

        /* check each character for mappability */
        for (p.set((char *)str) ; len != 0 ; p.inc(&len))
        {
            /* check for round-trip mappability */
            if (!is_rt_mappable(p.getch(), to_local, to_uni))
            {
                /* nope - return false */
                retval->set_nil();
                break;
            }
        }
    }
    else if (arg.typ == VM_INT)
    {
        /* check the integer character for mappability */
        if (arg.val.intval < 0 || arg.val.intval > 0xffff)
        {
            /* it's out of the valid unicode range, so it's not mappable */
            retval->set_nil();
        }
        else
        {
            /* ask the character mapper */
            retval->set_logical(is_rt_mappable(
                (wchar_t)arg.val.intval, to_local, to_uni));
        }
    }

    /* handled */
    return TRUE;
}

/*------------------------------------------------------------------------ */
/*
 *   Determine if a character has a round-trip mapping.  
 */
int CVmObjCharSet::is_rt_mappable(wchar_t c, CCharmapToLocal *to_local,
                                  CCharmapToUni *to_uni)
{
    char lclbuf[16];
    char unibuf[16];
    size_t lcllen;
    size_t unilen;
    char *p;

    /* if there's no local mapping, it's obviously not mappable */
    if (!to_local->is_mappable(c))
        return FALSE;

    /* 
     *   If there's an expansion in the mapping to the local set, then there
     *   can't be a round-trip mapping.  Expansions are inherently one-way
     *   because they produce multiple local characters for a single unicode
     *   character, and the reverse mapping has no way to group those
     *   multiple local characters back into a single unicode character.  
     */
    if (to_local->get_expansion(c, &lcllen) != 0)
        return FALSE;

    /* get the local mapping */
    lcllen = to_local->map_char(c, lclbuf, sizeof(lclbuf));

    /* map it back to unicode */
    p = unibuf;
    unilen = sizeof(unibuf);
    unilen = to_uni->map(&p, &unilen, lclbuf, lcllen);

    /* 
     *   if the unicode mapping is one character that exactly matches the
     *   original input character, then we have a valid round-trip mapping 
     */
    return (unilen == utf8_ptr::s_wchar_size(c)
            && utf8_ptr::s_getch(unibuf) == c);
}

/*------------------------------------------------------------------------ */
/*
 *   Get the unicode-to-local character set mapper 
 */
CCharmapToLocal *CVmObjCharSet::get_to_local(VMG0_) const
{
    /* if there's no mapper, throw an exception */
    if (get_ext_ptr()->to_local == 0)
    {
        /* throw an UnknownCharacterSetException */
        G_interpreter->throw_new_class(vmg_ G_predef->charset_unknown_exc,
                                       0, "unknown character set");
    }

    /* return the mapper */
    return get_ext_ptr()->to_local;
}

/*
 *   Get the local-to-unicode character set mapper 
 */
CCharmapToUni *CVmObjCharSet::get_to_uni(VMG0_) const
{
    /* if there's no mapper, throw an exception */
    if (get_ext_ptr()->to_uni == 0)
    {
        /* throw an UnknownCharacterSetException */
        G_interpreter->throw_new_class(vmg_ G_predef->charset_unknown_exc,
                                       0, "unknown character set");
    }

    /* return the mapper */
    return get_ext_ptr()->to_uni;
}
