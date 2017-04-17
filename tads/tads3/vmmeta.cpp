#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/VMMETA.CPP,v 1.3 1999/07/11 00:46:58 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmmeta.cpp - metaclass dependency table
Function
  
Notes
  
Modified
  12/01/98 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>

#include "t3std.h"
#include "vmtype.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmglob.h"
#include "vmmeta.h"
#include "vmobj.h"
#include "vmmcreg.h"
#include "vmfile.h"
#include "vmintcls.h"


/* ------------------------------------------------------------------------ */
/*
 *   Create the metaclass dependency table with a given number of initial
 *   entries 
 */
CVmMetaTable::CVmMetaTable(size_t init_entries)
{
    vm_meta_reg_t *entry;
    uint cnt;
    uint i;

    /* allocate space for our entries */
    if (init_entries != 0)
    {
        /* allocate the space */
        table_ = (vm_meta_entry_t *)
                 t3malloc(init_entries * sizeof(table_[0]));
    }
    else
    {
        /* we have no entries */
        table_ = 0;
    }

    /* no entries are defined yet */
    count_ = 0;

    /* remember the allocation size */
    alloc_ = init_entries;

    /* count the number of registered metaclasses */
    for (entry = G_meta_reg_table, cnt = 0 ; entry->meta != 0 ;
         ++entry, ++cnt) ;

    /* 
     *   allocate the reverse dependency table -- this table lets us get
     *   the dependency index of a metaclass given its registration table
     *   index 
     */
    reverse_map_ = (int *)t3malloc(cnt * sizeof(reverse_map_[0]));

    /* 
     *   set each element of the table to -1, since we have no dependency
     *   table entries yet 
     */
    for (i = 0 ; i < cnt ; ++i)
        reverse_map_[i] = -1;
}

/* ------------------------------------------------------------------------ */
/*
 *   Delete the table 
 */
CVmMetaTable::~CVmMetaTable()
{
    /* clear the table */
    clear();
    
    /* free the table, if we ever allocated one */
    if (table_ != 0)
        t3free(table_);

    /* free the reverse map */
    t3free(reverse_map_);
}

/* ------------------------------------------------------------------------ */
/*
 *   Clear all entries from the table 
 */
void CVmMetaTable::clear()
{
    size_t i;
    
    /* delete all of the property tables in the entries */
    for (i = 0 ; i < count_ ; ++i)
        table_[i].release_mem();
    
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
void CVmMetaTable::ensure_space(size_t entries, size_t increment)
{
    /* if we don't have enough space, allocate more */
    if (entries >= alloc_)
    {
        size_t new_size;
        
        /* increase the allocation size by the given increment */
        alloc_ += increment;

        /* if it's still too small, bump it up to the required size */
        if (alloc_ < entries)
            alloc_ = entries;

        /* compute the new size */
        new_size = alloc_ * sizeof(table_[0]);
        
        /* 
         *   if we have a table already, reallocate it at the larger size;
         *   otherwise, allocate a new table 
         */
        if (table_ != 0)
            table_ = (vm_meta_entry_t *)t3realloc(table_, new_size);
        else
            table_ = (vm_meta_entry_t *)t3malloc(new_size);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   Add an entry to the table 
 */
void CVmMetaTable::add_entry(const char *metaclass_id, size_t func_cnt,
                             vm_prop_id_t min_prop, vm_prop_id_t max_prop)
{
    vm_meta_reg_t *entry;
    uint idx;
    const char *vsn;
    size_t name_len;

    /* find the version suffix in the metaclass name, if any */
    vsn = lib_find_vsn_suffix(metaclass_id, '/', "000000", &name_len);
    
    /* ensure we have space for one more entry */
    ensure_space(count_ + 1, 5);

    /* look up the metaclass by name */
    for (idx = 0, entry = G_meta_reg_table ; entry->meta != 0 ;
         ++entry, ++idx)
    {
        const char *entry_vsn;
        size_t entry_name_len;
        
        /* find the version number in this entry */
        entry_vsn = lib_find_vsn_suffix((*entry->meta)->get_meta_name(),
                                        '/', "000000", &entry_name_len);
        
        /* see if this is a match */
        if (name_len == entry_name_len
            && memcmp(metaclass_id, (*entry->meta)->get_meta_name(),
                      name_len) == 0)
        {
            /* 
             *   make sure the version provided in the VM is at least as
             *   high as the requested version 
             */
            if (strcmp(vsn, entry_vsn) > 0)
            {
                /* 
                 *   throw the error; this is a version mismatch error, so
                 *   flag it as a version error and include the metaclass
                 *   dependency information 
                 */
                err_throw_a(VMERR_METACLASS_TOO_OLD, 4,
                            ERR_TYPE_TEXTCHAR, metaclass_id,
                            ERR_TYPE_TEXTCHAR, entry_vsn,
                            ERR_TYPE_METACLASS, metaclass_id,
                            ERR_TYPE_VERSION_FLAG);
            }
            
            /* add this entry */
            add_entry(metaclass_id, idx, func_cnt, min_prop, max_prop);

            /* we're done */
            return;
        }
    }

    /* 
     *   We didn't find it.  This is probably an out-of-date VM issue, so
     *   flag it as a version error, and include the metaclass dependency
     *   information.  
     */
    err_throw_a(VMERR_UNKNOWN_METACLASS, 3,
                ERR_TYPE_TEXTCHAR, metaclass_id,
                ERR_TYPE_METACLASS, metaclass_id,
                ERR_TYPE_VERSION_FLAG);
}

/* ------------------------------------------------------------------------ */
/*
 *   Add an entry to the table for a given registration table index
 */
void CVmMetaTable::add_entry(const char *metaclass_id,
                             uint idx, size_t func_cnt,
                             vm_prop_id_t min_prop, vm_prop_id_t max_prop)
{
    /* get the registration table entry from the index */
    vm_meta_reg_t *entry = &G_meta_reg_table[idx];
    
    /* remember the defining class object */
    table_[count_].meta_ = *entry->meta;

    /* save the name */
    table_[count_].set_meta_name(metaclass_id);

    /* calculate the number of entries in the table */
    size_t prop_xlat_cnt;
    if (min_prop == VM_INVALID_PROP || max_prop == VM_INVALID_PROP)
        prop_xlat_cnt = 0;
    else
        prop_xlat_cnt = max_prop - min_prop + 1;

    /* set the property count */
    table_[count_].min_prop_ = min_prop;
    table_[count_].prop_xlat_cnt_ = prop_xlat_cnt;

    /* set the function count */
    table_[count_].func_xlat_cnt_ = func_cnt;

    /* we don't know the intrinsic class's representative object yet */
    table_[count_].class_obj_ = VM_INVALID_OBJ;

    /* allocate space for the property table, if we have any translations */
    if (prop_xlat_cnt != 0)
    {
        size_t siz;

        /* calculate the table size */
        siz = prop_xlat_cnt * sizeof(table_[count_].prop_xlat_[0]);
        
        /* allocate the table */
        table_[count_].prop_xlat_ = (unsigned short *)t3malloc(siz);

        /* 
         *   initialize each entry in the table to zero - this will ensure
         *   that each entry that is never set to a valid function index
         *   will properly yield function index zero, which is defined as
         *   the "not found" invalid function index 
         */
        memset(table_[count_].prop_xlat_, 0, siz);
    }
    else
    {
        /* there are no properties to translate, so we don't need a table */
        table_[count_].prop_xlat_ = 0;
    }

    /* allocate the function-to-property translation table */
    if (func_cnt != 0)
    {
        size_t i;
        vm_prop_id_t *prop_ptr;
        
        /* allocate the table */
        table_[count_].func_xlat_ =
            (vm_prop_id_t *)t3malloc(func_cnt * sizeof(vm_prop_id_t));

        /* clear the table - mark each entry as an invalid property ID */
        for (i = 0, prop_ptr = table_[count_].func_xlat_ ;
             i < func_cnt ; ++i, ++prop_ptr)
            *prop_ptr = VM_INVALID_PROP;
    }
    else
    {
        /* no properties, so we don't need a table */
        table_[count_].func_xlat_ = 0;
    }

    /* 
     *   store the reverse mapping for this entry, so that we can find the
     *   dependency entry given the registration index 
     */
    reverse_map_[idx] = count_;

    /* count the new entry */
    ++count_;
}

/* ------------------------------------------------------------------------ */
/*
 *   Add an entry to the dependency table only if it doesn't already
 *   appear in the dependency table.  We add the entry based on its
 *   registration table index. 
 */
void CVmMetaTable::add_entry_if_new(uint reg_table_idx, size_t func_cnt,
                                    vm_prop_id_t min_prop,
                                    vm_prop_id_t max_prop)
{
    /* 
     *   if the entry already has a valid dependency table index, as
     *   obtained from the reverse mapping, we need not add it again 
     */
    if (reverse_map_[reg_table_idx] != -1)
        return;

    /* add the entry */
    add_entry((*G_meta_reg_table[reg_table_idx].meta)->get_meta_name(),
              reg_table_idx, func_cnt, min_prop, max_prop);
}


/* ------------------------------------------------------------------------ */
/*
 *   Look up an entry by external ID 
 */
vm_meta_entry_t *CVmMetaTable::get_entry_by_id(const char *id) const
{
    /* find the version suffix in the metaclass name, if any */
    size_t name_len;
    const char *vsn = lib_find_vsn_suffix(id, '/', "000000", &name_len);

    /* look up the metaclass by name */
    size_t i;
    vm_meta_entry_t *entry;
    for (i = 0, entry = table_ ; i < count_ ; ++entry, ++i)
    {
        /* find the version number in this entry */
        size_t entry_name_len;
        const char *entry_vsn = lib_find_vsn_suffix(
            entry->image_meta_name_, '/', "000000", &entry_name_len);

        /* see if this is a match */
        if (name_len == entry_name_len
            && memcmp(id, entry->image_meta_name_, name_len) == 0)
        {
            /* 
             *   we found a match to the name; make sure the version is at
             *   least as high as requested 
             */
            if (strcmp(vsn, entry_vsn) <= 0)
            {
                /* the loaded version is at least as new, so return it */
                return entry;
            }
            else
            {
                /* it's too old, so it's effectively not available */
                return 0;
            }
        }
    }

    /* didn't find a match */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Invoke the VM-stack-based constructor for the metaclass at the given
 *   index 
 */
vm_obj_id_t CVmMetaTable::create_from_stack(VMG_ const uchar **pc_ptr,
                                            uint idx, uint argc)
{
    /* make sure the entry is defined */
    if (idx >= count_)
        err_throw(VMERR_BAD_METACLASS_INDEX);
    
    /* invoke the appropriate constructor */
    return table_[idx].meta_->create_from_stack(vmg_ pc_ptr, argc);
}

/* ------------------------------------------------------------------------ */
/*
 *   Call a static property in the metaclass at the given index 
 */
int CVmMetaTable::call_static_prop(VMG_ vm_val_t *result,
                                   const uchar **pc_ptr, uint idx,
                                   uint *argc, vm_prop_id_t prop)
{
    /* make sure the entry is defined */
    if (idx >= count_)
        err_throw(VMERR_BAD_METACLASS_INDEX);

    /* invoke the appropriate static property evaluator */
    return table_[idx].meta_->call_stat_prop(vmg_ result, pc_ptr, argc, prop);
}

/* ------------------------------------------------------------------------ */
/*
 *   Create an object with the given ID and load the object from the image
 *   file. 
 */
void CVmMetaTable::create_from_image(VMG_ uint idx, vm_obj_id_t id,
                                     const char *ptr, size_t siz)
{
    /* make sure the entry is defined */
    if (idx >= count_)
        err_throw(VMERR_BAD_METACLASS_INDEX);

    /* create the object table entry in the memory manager */
    G_obj_table->alloc_obj_with_id(id, TRUE);

    /* invoke the appropriate constructor */
    table_[idx].meta_->create_for_image_load(vmg_ id);

    /* load the object */
    vm_objp(vmg_ id)->load_from_image(vmg_ id, ptr, siz);
}

/*
 *   Create an object of the given metaclass with the given ID, in
 *   preparation for restoring the object's data from saved state
 *   information.  This doesn't fill in the object with the saved state
 *   data, but merely creates the object.
 *   
 *   The caller is responsible for having allocated the object ID before
 *   calling this function.  
 */
void CVmMetaTable::create_for_restore(VMG_ uint idx, vm_obj_id_t id)
{
    /* make sure the entry is defined in our table of metaclasses */
    if (idx >= count_)
        err_throw(VMERR_BAD_METACLASS_INDEX);
    
    /* 
     *   Invoke the appropriate constructor.  Note that the caller must
     *   already have allocated the object ID, so we simply use the given ID
     *   without further consideration.  
     */
    table_[idx].meta_->create_for_restore(vmg_ id);
}


/* ------------------------------------------------------------------------ */
/*
 *   Write the table to a file.  
 */
void CVmMetaTable::write_to_file(CVmFile *fp)
{
    size_t i;

    /* write the number of entries */
    fp->write_uint2(get_count());

    /* write each entry */
    for (i = 0 ; i < get_count() ; ++i)
    {
        const char *nm;
        const vm_meta_entry_t *entry;
        ushort j;

        /* get the entry */
        entry = get_entry(i);

        /* get this entry's name */
        nm = entry->image_meta_name_;

        /* write the length of the name, followed by the name */
        fp->write_uint2(strlen(nm));
        fp->write_bytes(nm, strlen(nm));

        /* write our associated IntrinsicClass object's ID */
        fp->write_uint4(entry->class_obj_);

        /* 
         *   Write the property table information - write the number of
         *   function entries, and the minimum and maximum property ID's. 
         */
        fp->write_uint2(entry->func_xlat_cnt_);
        fp->write_uint2(entry->min_prop_);
        fp->write_uint2(entry->min_prop_ + entry->prop_xlat_cnt_);

        /* 
         *   Write out the property translation table.  The function
         *   translation table will always be smaller than (at worst, it
         *   will be the same size as) the property translation table;
         *   both tables contain the same information, so since we only
         *   need to write out one or the other, write out the smaller of
         *   the two.
         *   
         *   Note that xlat_func() requires a 1-based function index, so
         *   run our counter from 1 to the function table count.  
         */
        for (j = 1 ; j <= entry->func_xlat_cnt_ ; ++j)
            fp->write_uint2(entry->xlat_func(j));
    }
}

/*
 *   Read the table from a file. 
 */
int CVmMetaTable::read_from_file(CVmFile *fp)
{
    size_t cnt;
    size_t i;

    /* clear the existing table */
    clear();

    /* read the number of entries */
    cnt = fp->read_uint2();

    /* read the entries */
    for (i = 0 ; i < cnt ; ++i)
    {
        char buf[256];
        size_t len;
        vm_prop_id_t min_prop;
        vm_prop_id_t max_prop;
        ushort func_cnt;
        ushort j;
        vm_meta_entry_t *entry;
        vm_obj_id_t class_obj;

        /* read the length of this entry, and make sure it's valid */
        len = fp->read_uint2();
        if (len > sizeof(buf) - 1)
            return VMERR_SAVED_META_TOO_LONG;

        /* read the name and null-terminate it */
        fp->read_bytes(buf, len);
        buf[len] = '\0';

        /* read the associated IntrinsicClass object */
        class_obj = (vm_obj_id_t)fp->read_uint4();

        /* read the property table description */
        func_cnt = (ushort)fp->read_uint2();
        min_prop = (vm_prop_id_t)fp->read_uint2();
        max_prop = (vm_prop_id_t)fp->read_uint2();

        /* add this entry */
        add_entry(buf, func_cnt, min_prop, max_prop);

        /* get the new entry's record */
        entry = get_entry(i);

        /* set the class ID */
        entry->class_obj_ = class_obj;

        /* 
         *   Read the property mappings.  We stored the function table
         *   entries, so we simply need to load those entries.  Note that
         *   the function table has a 1-based index, so run our counter
         *   from 1 to the function count. 
         */
        for (j = 1 ; j <= func_cnt ; ++j)
            entry->add_prop_xlat((short)fp->read_int2(), j);
    }

    /* success */
    return 0;
}

#if 0 // moved inline, since it's small and used a lot
/*
 *   Get the function vector index for a property given the metaclass's
 *   registration table index. 
 */
int CVmMetaTable::prop_to_vector_idx(uint reg_table_idx, vm_prop_id_t prop)
{
    /* get the entry for the registration table index */
    vm_meta_entry_t *entry = get_entry_from_reg(reg_table_idx);

    /* 
     *   if there's no entry for this registration index, there's
     *   obviously no metaclass function mapping for the property 
     */
    if (entry == 0)
        return 0;

    /* return the property translation */
    return entry->xlat_prop(prop);
}
#endif

/*
 *   Create an IntrinsicClass object for each metaclass that doesn't
 *   already have one. 
 */
void CVmMetaTable::create_intrinsic_class_instances(VMG0_)
{
    size_t idx;
    vm_meta_entry_t *entry;
    
    /* go through our table */
    for (idx = 0, entry = table_ ; idx < count_ ; ++idx, ++entry)
    {
        /* if this entry has no associated class object, create one for it */
        if (entry->class_obj_ == VM_INVALID_OBJ)
        {
            /* 
             *   create the class object - it will automatically register
             *   itself to fill in the entry 
             */
            CVmObjClass::create_dyn(vmg_ idx);
        }

        /* 
         *   Add this object to the machine globals, if it's not a root
         *   object.  Any dynamically-created intrinsic class instance must
         *   be made a machine global because it will always be reachable as
         *   the class object for the metaclass, but the metclass won't trace
         *   the object during garbage collection, so we have to set up a
         *   global to make sure the gc knows it's always reachable.
         */
        G_obj_table->add_to_globals(entry->class_obj_);
    }
}

/*
 *   Forget the IntrinsicClass objects we created dynamically in
 *   create_intrinsic_class_instances().  This simply drops our references
 *   to those objects from the metaclass table.  
 */
void CVmMetaTable::forget_intrinsic_class_instances(VMG0_)
{
    size_t idx;
    vm_meta_entry_t *entry;

    /* go through our table and do class-level initializations */
    for (idx = 0, entry = table_ ; idx < count_ ; ++idx, ++entry)
        entry->class_obj_ = VM_INVALID_OBJ;
}

/* ------------------------------------------------------------------------ */
/*
 *   Build a translation table from run-time metaclass index to compiler
 *   TC_META_xxx ID. 
 */
tc_metaclass_t *CVmMetaTable::build_runtime_to_compiler_id_table(VMG0_)
{
    vm_meta_reg_t *entry;
    int i, cnt;

    /* count the metaclass registration table size */
    for (entry = G_meta_reg_table, cnt = 0 ; entry->meta != 0 ;
         ++entry, ++cnt) ;

    /* 
     *   allocate the translation table - this is simply an array indexed by
     *   metaclass registration index and yielding the corresponding compiler
     *   metaclass ID 
     */
    tc_metaclass_t *xlat = (tc_metaclass_t *)t3malloc(cnt * sizeof(xlat[0]));

    /* scan the registration table for compiler-recognized classes */
    for (entry = G_meta_reg_table, i = 0 ; entry->meta != 0 ; ++entry, ++i)
    {
        /* get its name */
        const char *nm = (*entry->meta)->get_meta_name();

        /* scan for the version number suffix - we'll ignore it if found */
        const char *p;
        for (p = nm ; *p != '\0' && *p != '/' ; ++p) ;

        /* note the length up to the version suffix delimiter */
        size_t len = p - nm;

        /* check for the known names */
        if (len == 11 && memcmp(nm, "tads-object", 11) == 0)
            xlat[i] = TC_META_TADSOBJ;
        else if (len == 11 && memcmp(nm, "dictionary2", 11) == 0)
            xlat[i] = TC_META_DICT;
        else if (len == 18 && memcmp(nm, "grammar-production", 18) == 0)
            xlat[i] = TC_META_GRAMPROD;
        else if (len == 13 && memcmp(nm, "int-class-mod", 13) == 0)
            xlat[i] = TC_META_ICMOD;
        else
            xlat[i] = TC_META_UNKNOWN;
    }

    /* return the translation table */
    return xlat;
}

