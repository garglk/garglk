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
    {
        /* clear the table to delete the entries in the 'names_' array */
        clear();

        /* free the table */
        t3free(names_);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Clear all entries from the table 
 */
void CVmBifTable::clear()
{
    /* delete the 'names' entries, if we allocated any */
    if (names_ != 0)
    {
        size_t i;

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
void CVmBifTable::add_entry(const char *func_set_id)
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
                err_throw_a(VMERR_FUNCSET_TOO_OLD, 2,
                            ERR_TYPE_TEXTCHAR, func_set_id,
                            ERR_TYPE_TEXTCHAR, entry_vsn);

            /* 
             *   It's a match - add the new entry.  Simply keep a pointer
             *   to the static table entry. 
             */
            table_[count_] = entry;

            /* store the new name element as well */
            names_[count_] = lib_copy_str(func_set_id);

            /* count the new entry */
            ++count_;

            /* we're done */
            return;
        }
    }

    /* we didn't find it - handle it according to our resolution mode */
    add_entry_unresolved(func_set_id);
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
    char *outp;
    size_t outlen;
    vm_obj_id_t str_id;
    CVmObjString *str_obj;

    /* figure out how much space we need */
    outp = 0;
    outlen = 0;
    outlen = G_cmap_from_ui->map(0, &outlen, str, len);

    /* allocate a string of that size */
    str_id = CVmObjString::create(vmg_ FALSE, outlen);
    str_obj = (CVmObjString *)vm_objp(vmg_ str_id);

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

/* ------------------------------------------------------------------------ */
/*
 *   Pop a string value 
 */
const char *CVmBif::pop_str_val(VMG0_)
{
    vm_val_t val;
    CVmObject *obj;
    const char *p;

    /* pop the value */
    G_stk->pop(&val);

    /* see what we have */
    switch(val.typ)
    {
    case VM_SSTRING:
        /* it's a constant string - get the constant pool pointer */
        return G_const_pool->get_ptr(val.val.ofs);

    case VM_OBJ:
        /* get the object */
        obj = vm_objp(vmg_ val.val.obj);

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

        /* if it has a liist value, return it */
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
    const char *strp;
    size_t copy_len;

    /* pop the string value */
    strp = pop_str_val(vmg0_);

    /* 
     *   get the length, but limit it to our buffer size, less one byte
     *   for null termination 
     */
    copy_len = vmb_get_len(strp);
    if (copy_len > buflen - 1)
        copy_len = utf8_ptr::s_trunc(strp + VMB_LEN, buflen - 1);

    /* copy the string */
    memcpy(buf, strp + VMB_LEN, copy_len);

    /* null-terminate the result */
    buf[copy_len] = '\0';
}


/* ------------------------------------------------------------------------ */
/*
 *   Pop a string into a buffer, translating the string into the filename
 *   character set and null-terminating the result.  
 */
void CVmBif::pop_str_val_fname(VMG_ char *buf, size_t buflen)
{
    const char *strp;
    size_t copy_len;

    /* pop the string value */
    strp = pop_str_val(vmg0_);

    /* get the length */
    copy_len = vmb_get_len(strp);

    /* 
     *   map it into the local filename character set and store the result
     *   in the output buffer - reserve one byte for the null termination
     *   byte 
     */
    copy_len = G_cmap_to_fname->map_utf8(buf, buflen - 1,
                                         strp + VMB_LEN, copy_len, 0);

    /* null-terminate the result */
    buf[copy_len] = '\0';
}

/*
 *   Pop a string into a buffer, translating the string into the user
 *   interface character set and null-terminating the result.  
 */
char *CVmBif::pop_str_val_ui(VMG_ char *buf, size_t buflen)
{
    const char *strp;
    size_t copy_len;

    /* pop the string value */
    strp = pop_str_val(vmg0_);

    /* get the length */
    copy_len = vmb_get_len(strp);

    /* 
     *   if they didn't allocate any space for the buffer, allocate one on
     *   the caller's behalf 
     */
    if (buflen == 0)
    {
        /* figure out how much space we need */
        buflen = G_cmap_to_ui->map_utf8(0, 0, strp + VMB_LEN, copy_len, 0);

        /* add space for null termination */
        buflen += 1;

        /* allocate the buffer */
        buf = (char *)t3malloc(buflen);

        /* if that failed, return null */
        if (buf == 0)
            return 0;
    }

    /* 
     *   map it into the local UI character set and store the result in
     *   the output buffer - reserve one byte for the null termination
     *   byte 
     */
    copy_len = G_cmap_to_ui->map_utf8(buf, buflen - 1,
                                      strp + VMB_LEN, copy_len, 0);

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
    vm_val_t val;

    /* pop a number */
    G_interpreter->pop_int(vmg_ &val);

    /* return the value */
    return (int)val.val.intval;
}

/*
 *   Pop a long integer value 
 */
int CVmBif::pop_long_val(VMG0_)
{
    vm_val_t val;

    /* pop a number */
    G_interpreter->pop_int(vmg_ &val);

    /* return the value */
    return val.val.intval;
}

/*
 *   Pop a true/nil logical value 
 */
int CVmBif::pop_bool_val(VMG0_)
{
    vm_val_t val;

    /* pop a value */
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
    vm_val_t val;

    /* pop an object reference */
    G_interpreter->pop_obj(vmg_ &val);

    /* return the value */
    return val.val.obj;
}

/*
 *   Pop a property ID value 
 */
vm_prop_id_t CVmBif::pop_propid_val(VMG0_)
{
    vm_val_t val;

    /* pop a property ID */
    G_interpreter->pop_prop(vmg_ &val);

    /* return the value */
    return val.val.prop;
}

