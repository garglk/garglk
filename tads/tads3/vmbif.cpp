#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/vmbif.cpp,v 1.3 1999/07/11 00:46:59 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmbif.cpp - built-in function set table implementation
Function
  
Notes
  
Modified
  12/05/98 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>

#include "t3std.h"
#include "utf8.h"
#include "charmap.h"
#include "vmtype.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmglob.h"
#include "vmbif.h"
#include "vmbifreg.h"
#include "vmstr.h"
#include "vmobj.h"
#include "vmrun.h"
#include "vmcset.h"
#include "vmfilnam.h"


/* ------------------------------------------------------------------------ */
/*
 *   Create the function set table with a given number of initial entries 
 */
CVmBifTable::CVmBifTable(size_t init_entries)
{
    /* allocate space for our entries */
    if (init_entries != 0)
    {
        /* allocate the space */
        table_ = (vm_bif_entry_t **)
                 t3malloc(init_entries * sizeof(table_[0]));
        names_ = (char **)
                 t3malloc(init_entries * sizeof(names_[0]));
    }
    else
    {
        /* we have no entries */
        table_ = 0;
        names_ = 0;
    }

    /* no entries are defined yet */
    count_ = 0;

    /* remember the allocation size */
    alloc_ = init_entries;
}

/* ------------------------------------------------------------------------ */
/*
 *   Delete the table 
 */
CVmBifTable::~CVmBifTable()
{
    /* free the table, if we ever allocated one */
    if (table_ != 0)
        t3free(table_);

    /* free the function set names list, if we allocated it */
    if (names_ != 0)
        t3free(names_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Clear all entries from the table 
 */
void CVmBifTable::clear(VMG0_)
{
    size_t i;

    /* detach each function set */
    if (table_ != 0)
    {
        for (i = 0 ; i < count_ ; ++i)
        {
            if (table_[i] != 0)
                table_[i]->unload_image(vmg_ i);
        }
    }

    /* delete the 'names' entries, if we allocated any */
    if (names_ != 0)
    {
        /* free each element */
        for (i = 0 ; i < count_ ; ++i)
            lib_free_str(names_[i]);
    }

    /* 
     *   Reset the entry counter.  Note that this doesn't affect any
     *   allocation; we keep a separate count of the number of table slots
     *   we have allocated.  Table slots don't have any additional
     *   associated memory, so we don't need to worry about cleaning
     *   anything up at this point.  
     */
    count_ = 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Ensure that we have space for a given number of entries 
 */
void CVmBifTable::ensure_space(size_t entries, size_t increment)
{
    /* if we don't have enough space, allocate more */
    if (entries >= alloc_)
    {
        size_t new_table_size;
        size_t new_names_size;

        /* increase the allocation size by the given increment */
        alloc_ += increment;

        /* if it's still too small, bump it up to the required size */
        if (alloc_ < entries)
            alloc_ = entries;

        /* compute the new sizes */
        new_table_size = alloc_ * sizeof(table_[0]);
        new_names_size = alloc_ * sizeof(names_[0]);

        /* 
         *   if we have a table already, reallocate it at the larger size;
         *   otherwise, allocate a new table 
         */
        if (table_ != 0)
        {
            table_ = (vm_bif_entry_t **)t3realloc(table_, new_table_size);
            names_ = (char **)t3realloc(names_, new_names_size);
        }
        else
        {
            table_ = (vm_bif_entry_t **)t3malloc(new_table_size);
            names_ = (char **)t3malloc(alloc_ * new_names_size);
        }
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Add an entry to the table 
 */
void CVmBifTable::add_entry(VMG_ const char *func_set_id)
{
    vm_bif_entry_t *entry;
    const char *vsn;
    size_t name_len;

    /* ensure we have space for one more entry */
    ensure_space(count_ + 1, 5);

    /* find the version suffix in the name, if any */
    vsn = lib_find_vsn_suffix(func_set_id, '/', "000000", &name_len);

    /* look up the function set by name */
    for (entry = G_bif_reg_table ; entry->func_set_id != 0 ; ++entry)
    {
        const char *entry_vsn;
        size_t entry_name_len;

        /* find the version number in this entry */
        entry_vsn = lib_find_vsn_suffix(entry->func_set_id, '/', "000000",
                                        &entry_name_len);

        /* see if this is a match */
        if (name_len == entry_name_len
            && memcmp(func_set_id, entry->func_set_id, name_len) == 0)
        {
            /* 
             *   make sure the version provided in the VM is at least as
             *   high as the requested version 
             */
            if (strcmp(vsn, entry_vsn) > 0)
            {
                /* it's out of date - throw "function set too old" */
                err_throw_a(VMERR_FUNCSET_TOO_OLD, 4,
                            ERR_TYPE_TEXTCHAR, func_set_id,
                            ERR_TYPE_TEXTCHAR, entry_vsn,
                            ERR_TYPE_FUNCSET, func_set_id,
                            ERR_TYPE_VERSION_FLAG);
            }

            /* 
             *   It's a match - add the new entry.  Simply keep a pointer
             *   to the static table entry. 
             */
            table_[count_] = entry;

            /* store the new name element as well */
            names_[count_] = lib_copy_str(func_set_id);

            /* complete the linkage to the image file */
            entry->link_to_image(vmg_ count_);

            /* count the new entry */
            ++count_;

            /* we're done */
            return;
        }
    }

    /* we didn't find it - handle it according to our resolution mode */
    add_entry_unresolved(vmg_ func_set_id);
}

/* ------------------------------------------------------------------------ */
/*
 *   Look up an entry by name 
 */
const vm_bif_entry_t *CVmBifTable::get_entry(const char *name)
{
    /* find the version suffix in the name, if any */
    size_t namelen;
    const char *vsn = lib_find_vsn_suffix(name, '/', "000000", &namelen);

    /* scan the table */
    for (size_t i = 0 ; i < count_ ; ++i)
    {
        /* find the version number in this entry */
        size_t enamelen;
        const char *evsn = lib_find_vsn_suffix(
            names_[i], '/', "000000", &enamelen);

        /* compare names */
        if (namelen == enamelen && memcmp(name, names_[i], namelen) == 0)
        {
            /* the names match - compare versions */
            if (strcmp(vsn, evsn) <= 0)
            {
                /* 
                 *   the loaded version is at least as new as the requested
                 *   version, so we have a match 
                 */
                return table_[i];
            }
            else
            {
                /* 
                 *   this function set is loaded, but the linked version is
                 *   older than the requested version, so it counts as not
                 *   loaded 
                 */
                return 0;
            }
        }
    }

    /* didn't find a match */
    return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   Validate an entry 
 */
int CVmBifTable::validate_entry(uint set_index, uint func_index)
{
    /* validate that the set index is in range */
    if (set_index >= count_)
        return FALSE;

    /* get the function set, and make sure it's valid */
    vm_bif_entry_t *entry = table_[set_index];
    if (entry == 0)
        return FALSE;

    /* validate that the function index is in range for the set */
    if (func_index > entry->func_count)
        return FALSE;
    
    /* make sure there's a function pointer in the descriptor */
    vm_bif_desc *desc = &entry->func[func_index];
    if (desc->func == 0)
        return FALSE;

    /* it's a valid entry */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Function Set helper functions 
 */

/* 
 *   check arguments; throws an error if the argument count doesn't match
 *   the given value 
 */
void CVmBif::check_argc(VMG_ uint argc, uint needed_argc)
{
    if (argc != needed_argc)
        err_throw(VMERR_WRONG_NUM_OF_ARGS);
}

/* 
 *   check arguments; throws an error if the argument count is outside of
 *   the given range 
 */
void CVmBif::check_argc_range(VMG_ uint argc, uint argc_min, uint argc_max)
{
    if (argc < argc_min || argc > argc_max)
        err_throw(VMERR_WRONG_NUM_OF_ARGS);
}

/*
 *   return a string value 
 */
void CVmBif::retval_str(VMG_ const char *str)
{
    retval_str(vmg_ str, strlen(str));
}

void CVmBif::retval_str(VMG_ const char *str, size_t len)
{
    vm_obj_id_t str_obj;

    /* create a string to hold the return value */
    str_obj = CVmObjString::create(vmg_ FALSE, str, len);

    /* return the string */
    retval_obj(vmg_ str_obj);
}

/*
 *   return a string value that came from the UI character set
 */
void CVmBif::retval_ui_str(VMG_ const char *str)
{
    retval_ui_str(vmg_ str, strlen(str));
}

void CVmBif::retval_ui_str(VMG_ const char *str, size_t len)
{
    /* make a string object from the UI string, and return it */
    retval_obj(vmg_ str_from_ui_str(vmg_ str, len));
}

/*
 *   create a new string object from a string in the UI character set 
 */
vm_obj_id_t CVmBif::str_from_ui_str(VMG_ const char *str)
{
    return str_from_ui_str(vmg_ str, strlen(str));
}

vm_obj_id_t CVmBif::str_from_ui_str(VMG_ const char *str, size_t len)
{
    /* figure out how much space we need */
    char *outp = 0;
    size_t outlen = 0;
    outlen = G_cmap_from_ui->map(0, &outlen, str, len);

    /* allocate a string of that size */
    vm_obj_id_t str_id = CVmObjString::create(vmg_ FALSE, outlen);
    CVmObjString *str_obj = (CVmObjString *)vm_objp(vmg_ str_id);

    /* map the string into the new string buffer */
    outp = str_obj->cons_get_buf();
    G_cmap_from_ui->map(&outp, &outlen, str, len);

    /* return the new string object */
    return str_id;
}

/*
 *   return an object value
 */
void CVmBif::retval_obj(VMG_ vm_obj_id_t obj)
{
    if (obj == VM_INVALID_OBJ)
        G_interpreter->get_r0()->set_nil();
    else
        G_interpreter->get_r0()->set_obj(obj);
}

/*
 *   return a property value 
 */
void CVmBif::retval_prop(VMG_ vm_prop_id_t prop)
{
    G_interpreter->get_r0()->set_propid(prop);
}

/*
 *   return an integer value 
 */
void CVmBif::retval_int(VMG_ long val)
{
    G_interpreter->get_r0()->set_int(val);
}

/*
 *   return true 
 */
void CVmBif::retval_true(VMG0_)
{
    G_interpreter->get_r0()->set_true();
}

/*
 *   return nil 
 */
void CVmBif::retval_nil(VMG0_)
{
    G_interpreter->get_r0()->set_nil();
}

/*
 *   return a boolean value - nil if false, true if true 
 */
void CVmBif::retval_bool(VMG_ int val)
{
    G_interpreter->get_r0()->set_logical(val);
}

/*
 *   return a function pointer value 
 */
void CVmBif::retval_fnptr(VMG_ pool_ofs_t ofs)
{
    G_interpreter->get_r0()->set_fnptr(ofs);
}

/*
 *   return a value 
 */
void CVmBif::retval(VMG_ const vm_val_t *val)
{
    *G_interpreter->get_r0() = *val;
}

/*
 *   return a built-in function pointer 
 */
void CVmBif::retval_bifptr(VMG_ ushort set_idx, ushort func_idx)
{
    G_interpreter->get_r0()->set_bifptr(set_idx, func_idx);
}

/*
 *   return the value at top of stack 
 */
void CVmBif::retval_pop(VMG0_)
{
    G_stk->pop(G_interpreter->get_r0());
}

/* ------------------------------------------------------------------------ */
/*
 *   Pop a string value 
 */
const char *CVmBif::pop_str_val(VMG0_)
{
    /* pop the value */
    vm_val_t val;
    G_stk->pop(&val);

    /* interpret it as a string */
    return get_str_val(vmg_ &val);
}

/* get a string value */
const char *CVmBif::get_str_val(VMG_ const vm_val_t *val)
{
    CVmObject *obj;
    const char *p;

    /* see what we have */
    switch(val->typ)
    {
    case VM_SSTRING:
        /* it's a constant string - get the constant pool pointer */
        return G_const_pool->get_ptr(val->val.ofs);

    case VM_OBJ:
        /* get the object */
        obj = vm_objp(vmg_ val->val.obj);

        /* get the string value, if it has one */
        p = obj->get_as_string(vmg0_);

        /* if it has a string value, return it */
        if (p != 0)
            return p;

        /* we didn't get a string value */
        break;

    default:
        /* other types don't have a string value */
        break;
    }

    /* if we got here, the value isn't a string */
    err_throw(VMERR_STRING_VAL_REQD);
    AFTER_ERR_THROW(return 0;)
}

/* ------------------------------------------------------------------------ */
/*
 *   Pop a list value
 */
const char *CVmBif::pop_list_val(VMG0_)
{
    vm_val_t val;
    CVmObject *obj;
    const char *p;

    /* pop the value */
    G_stk->pop(&val);

    /* see what we have */
    switch(val.typ)
    {
    case VM_LIST:
        /* it's a constant list - get the constant pool pointer */
        return G_const_pool->get_ptr(val.val.ofs);

    case VM_OBJ:
        /* get the object */
        obj = vm_objp(vmg_ val.val.obj);

        /* get the list value, if it has one */
        p = obj->get_as_list();

        /* if it has a list value, return it */
        if (p != 0)
            return p;

        /* we didn't get a list value */
        break;

    default:
        /* other types don't have a list value */
        break;
    }

    /* if we got here, the value isn't a list */
    err_throw(VMERR_LIST_VAL_REQD);
    AFTER_ERR_THROW(return 0;)
}

/* ------------------------------------------------------------------------ */
/*
 *   Pop a string into a buffer, and null-terminate the result. 
 */
void CVmBif::pop_str_val_buf(VMG_ char *buf, size_t buflen)
{
    /* pop the top value from the stack and copy its string value */
    vm_val_t val;
    G_stk->pop(&val);
    get_str_val_buf(vmg_ buf, buflen, &val);
}

/*
 *   Get a string value into a buffer, null-terminating the result 
 */
void CVmBif::get_str_val_buf(VMG_ char *buf, size_t buflen,
                             const vm_val_t *val)
{
    /* pop the string value */
    const char *strp = get_str_val(vmg_ val);

    /* 
     *   get the length, but limit it to our buffer size, less one byte
     *   for null termination 
     */
    size_t copy_len = vmb_get_len(strp);
    if (copy_len > buflen - 1)
        copy_len = utf8_ptr::s_trunc(strp + VMB_LEN, buflen - 1);

    /* copy the string */
    memcpy(buf, strp + VMB_LEN, copy_len);

    /* null-terminate the result */
    buf[copy_len] = '\0';
}


/* ------------------------------------------------------------------------ */
/*
 *   Pop a filename value.  The source value can be string or FileName
 *   object. 
 */
void CVmBif::pop_fname_val(VMG_ char *buf, size_t buflen)
{
    get_fname_val(vmg_ buf, buflen, G_stk->get(0));
    G_stk->discard(1);
}

void CVmBif::get_fname_val(VMG_ char *buf, size_t buflen, const vm_val_t *val)
{
    CVmObjFileName *fn;
    const char *str;
    if ((fn = vm_val_cast(CVmObjFileName, val)) != 0)
    {
        /* get the FileName object's path as a local filename string */
        get_str_val_fname(vmg_ buf, buflen, fn->get_path_string());
    }
    else if ((str = val->get_as_string(vmg0_)) != 0)
    {
        /* get the string value as a local filename string */
        get_str_val_fname(vmg_ buf, buflen, str);
    }
    else
    {
        err_throw(VMERR_BAD_VAL_BIF);
    }
}

/*
 *   Pop a string into a buffer, translating the string into the filename
 *   character set and null-terminating the result.  
 */
void CVmBif::pop_str_val_fname(VMG_ char *buf, size_t buflen)
{
    /* pop the string and translate to a filename */
    get_str_val_fname(vmg_ buf, buflen, pop_str_val(vmg0_));
}

/*
 *   Get a string into a buffer, translting the string into the filename
 *   character set and null-terminating the result. 
 */
void CVmBif::get_str_val_fname(VMG_ char *buf, size_t buflen, const char *str)
{
    /* get the length */
    size_t copy_len = vmb_get_len(str);

    /* 
     *   map it into the local filename character set and store the result
     *   in the output buffer - reserve one byte for the null termination
     *   byte 
     */
    copy_len = G_cmap_to_fname->map_utf8(buf, buflen - 1,
                                         str + VMB_LEN, copy_len, 0);

    /* null-terminate the result */
    buf[copy_len] = '\0';
}

/*
 *   Pop a string into a buffer, translating the string into the user
 *   interface character set and null-terminating the result.  
 */
char *CVmBif::pop_str_val_ui(VMG_ char *buf, size_t buflen)
{
    /* pop the string value */
    const char *strp = pop_str_val(vmg0_);

    /* decode and skip the length prefix */
    size_t copy_len = vmb_get_len(strp);
    strp += VMB_LEN;

    /* 
     *   if they didn't allocate any space for the buffer, allocate one on
     *   the caller's behalf; otherwise map the string into the caller's
     *   buffer
     */
    if (buflen == 0)
    {
        /* the caller didn't provide a buffer, so allocate one */
        copy_len = G_cmap_to_ui->map_utf8_alo(&buf, strp, copy_len);
    }
    else
    {
        /* the caller provided a buffer, so use it for the mapping result */
        copy_len = G_cmap_to_ui->map_utf8(buf, buflen - 1, strp, copy_len, 0);
    }

    /* null-terminate the result */
    buf[copy_len] = '\0';

    /* return the buffer */
    return buf;
}

/* ------------------------------------------------------------------------ */
/*
 *   Pop an integer value 
 */
int CVmBif::pop_int_val(VMG0_)
{
    /* pop a number */
    vm_val_t val;
    G_interpreter->pop_int(vmg_ &val);

    /* return the value */
    return (int)val.val.intval;
}

int CVmBif::pop_int_or_nil(VMG_ int defval)
{
    /* check for nil */
    if (G_stk->get(0)->typ == VM_NIL)
    {
        /* it's nil - discard the nil and return the default value */
        G_stk->discard();
        return defval;
    }
    else
    {
        /* not nil - return the integer value */
        return pop_int_val(vmg0_);
    }
}

/*
 *   Pop a long integer value 
 */
int32_t CVmBif::pop_long_val(VMG0_)
{
    /* pop a number */
    vm_val_t val;
    G_interpreter->pop_int(vmg_ &val);

    /* return the value */
    return val.val.intval;
}

/*
 *   Pop a true/nil logical value 
 */
int CVmBif::pop_bool_val(VMG0_)
{
    /* pop a value */
    vm_val_t val;
    G_stk->pop(&val);

    /* check the type */
    switch(val.typ)
    {
    case VM_NIL:
        /* nil - interpret this as false */
        return FALSE;

    case VM_TRUE:
        /* true */
        return TRUE;

    case VM_INT:
        /* integer - return true if it's nonzero */
        return (val.val.intval != 0);

    default:
        /* anything else is unacceptable */
        err_throw(VMERR_BAD_TYPE_BIF);

        /* 
         *   (for the compiler's benefit, which doesn't know err_throw
         *   doesn't return and thus might want to warn about our failure to
         *   return a value) 
         */
        AFTER_ERR_THROW(return FALSE;)
    }
}

/*
 *   Pop an object reference value
 */
vm_obj_id_t CVmBif::pop_obj_val(VMG0_)
{
    /* pop an object reference */
    vm_val_t val;
    G_interpreter->pop_obj(vmg_ &val);

    /* return the value */
    return val.val.obj;
}

/*
 *   Pop a property ID value 
 */
vm_prop_id_t CVmBif::pop_propid_val(VMG0_)
{
    /* pop a property ID */
    vm_val_t val;
    G_interpreter->pop_prop(vmg_ &val);

    /* return the value */
    return val.val.prop;
}

/*
 *   Pop a CharacterSet argument.  This can also be given as a string with
 *   the name of a character set, in which case we'll create a CharacterSet
 *   object for the given name.
 */
vm_obj_id_t CVmBif::pop_charset_obj(VMG0_)
{
    /* pop the argument */
    vm_val_t val;
    G_stk->pop(&val);

    /* get the character set object from the argument value */
    return get_charset_obj(vmg_ &val);
}

vm_obj_id_t CVmBif::get_charset_obj(VMG_ int stk_idx)
{
    /* get the character set object from the value at the stack index */
    return get_charset_obj(vmg_ G_stk->get(stk_idx));
}

vm_obj_id_t CVmBif::get_charset_obj(VMG_ const vm_val_t *val)
{
    const char *str;

    /* 
     *   check to see if it's a CharacterSet object; if it's not, it must be
     *   a string giving the character set name 
     */
    if (val->typ == VM_OBJ && CVmObjCharSet::is_charset(vmg_ val->val.obj))
    {
        /* it's a CharacterSet object - return it */
        return val->val.obj;
    }
    else if ((str = val->get_as_string(vmg0_)) != 0)
    {
        /* it's a string - create a mapper for the given character set name */
        return CVmObjCharSet::create(
            vmg_ FALSE, str + VMB_LEN, vmb_get_len(str));
    }
    else if (val->typ == VM_NIL)
    {
        /* nil simply means no character set */
        return VM_INVALID_OBJ;
    }
    else
    {
        /* invalid argument */
        err_throw(VMERR_BAD_TYPE_BIF);
        AFTER_ERR_THROW(return VM_INVALID_OBJ);
    }
}


