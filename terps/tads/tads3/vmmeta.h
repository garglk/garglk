/* $Header: d:/cvsroot/tads/tads3/VMMETA.H,v 1.2 1999/05/17 02:52:28 MJRoberts Exp $ */

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmmeta.h - metaclass dependency table
Function
  The metaclass dependency table establishes the correspondence between
  metaclass index numbers and metaclasses.  The load image file contains
  a list of the metaclasses that the program requires, identifying each
  required metaclass with its universally unique identifier string.  At
  load time, we scan this list in the load image and create a dependency
  table.
Notes
  
Modified
  12/01/98 MJRoberts  - Creation
*/

#ifndef VMMETA_H
#define VMMETA_H

#include <stdlib.h>

#include "t3std.h"
#include "vmtype.h"
#include "vmglob.h"
#include "vmobj.h"
#include "tcprstyp.h"


/* ------------------------------------------------------------------------ */
/*
 *   Metaclass table entry.  This contains information on the metaclass at
 *   one index in the table.  
 */
struct vm_meta_entry_t
{
    /* the class descriptor object for the metaclass */
    class CVmMetaclass *meta_;

    /* 
     *   name of metaclass as read from image file (this is important for
     *   rewriting an image file during preinit, since it ensures that we
     *   store only the version number required by the compiled image
     *   file, not the version number actually implemented in the preinit
     *   interpreter) 
     */
    char *image_meta_name_;

    /* 
     *   the IntrinsicClass object (a CVmObjClass object) that represents
     *   this class 
     */
    vm_obj_id_t class_obj_;

    /*
     *   Property translation list.  The translation list is an array of
     *   internal function numbers, indexed by property ID.  However, not
     *   all property ID's are necessarily represented in the index -
     *   instead, the index contains entries only from a minimum property
     *   ID, and only contains a specified number of entries.  So, given
     *   property ID 'prop_id', we find the internal function number for
     *   the property at index (prop_id - min_prop_id) in the table.
     *   
     *   We use an unsigned short (16 bits) for each internal function
     *   number - this limits the number of functions per metaclass to
     *   65535, which should be more than adequate while keeping memory
     *   usage within reason.  
     */
    vm_prop_id_t min_prop_;
    size_t prop_xlat_cnt_;
    unsigned short *prop_xlat_;

    /*
     *   Function table.  This is the reverse of the property translation
     *   list: this table gives the property ID for each of the metaclass
     *   functions.  The first entry is the property of function index 1,
     *   the second entry is function index 2, and so on. 
     */
    vm_prop_id_t *func_xlat_;
    size_t func_xlat_cnt_;

    /* relese storage */
    void release_mem()
    {
        /* if this entry has a property table, delete it */
        if (prop_xlat_ != 0)
        {
            /* free it */
            t3free(prop_xlat_);
            
            /* forget the pointer */
            prop_xlat_ = 0;
        }

        /* if this entry has a function translation table, delete it */
        if (func_xlat_ != 0)
        {
            /* free it */
            t3free(func_xlat_);
            
            /* forget the pointer */
            func_xlat_ = 0;
        }

        /* relesae our stored name */
        lib_free_str(image_meta_name_);
    }

    /* store the metaclass name as loaded from the image file */
    void set_meta_name(const char *nm)
    {
        /* allocate space and store the name */
        image_meta_name_ = lib_copy_str(nm);
    }

    /*
     *   Add a property to the table, given the property ID and the
     *   1-based index of the corresponding function in the metaclass's
     *   internal function table.  
     */
    void add_prop_xlat(vm_prop_id_t prop, ushort func_idx)
    {
        /* 
         *   Add the translation entry from property ID to function index
         *   - note that we store this as a 1-based function table index
         *   value, because we reserve function index 0 to indicate that
         *   the property is invalid and has no function translation 
         */
        prop_xlat_[prop - min_prop_] = func_idx;
        
        /* 
         *   Add the translation entry from function index to property ID.
         *   Note that we must adjust the 1-based function index down one
         *   to get the index into our internal array.
         */
        func_xlat_[func_idx - 1] = prop;
    }

    /*
     *   Get the translation for a given property ID.  This returns zero
     *   if the property does not map to a valid function index for the
     *   metaclass, or the 1-based index of the function in the
     *   metaclass's "vtable" if the property is defined.
     */
    unsigned short xlat_prop(vm_prop_id_t prop) const
    {
        /*
         *   Check to see if the property is represented in the table.  If
         *   it's not within the range of properties in the table, there
         *   is no translation for the property.  
         */
        if (prop < min_prop_ || prop >= min_prop_ + prop_xlat_cnt_)
        {
            /* the property isn't in the table, so it has no translation */
            return 0;
        }
        
        /* return the function index from the table */
        return prop_xlat_[prop - min_prop_];
    }

    /*
     *   Translate a function index to a property ID.  The function index
     *   is given as a 1-based index, for consistency with the return
     *   value from xlat_prop(). 
     */
    vm_prop_id_t xlat_func(unsigned short func_idx) const
    {
        /* 
         *   if it's zero, or it's greater than the number of functions in
         *   our table, it has no property translation 
         */
        if (func_idx == 0 || func_idx > func_xlat_cnt_)
            return VM_INVALID_PROP;

        /* 
         *   return the entry from our table for the given index, adjusted
         *   to a 1-based index value 
         */
        return func_xlat_[func_idx - 1];
    }
};


/* ------------------------------------------------------------------------ */
/*
 *   The metaclass table.  One global instance of this table is generally
 *   created when the image file is loaded.  
 */
class CVmMetaTable
{
public:
    /* 
     *   Create a table with a given number of initial entries.  The table
     *   may be expanded in the future if necessary, but if the caller can
     *   predict the maximum number of entries required, we can
     *   preallocate the table at its final size and thus avoid the
     *   overhead and memory fragmentation of expanding the table.  
     */
    CVmMetaTable(size_t init_entries);

    /* delete the table */
    ~CVmMetaTable();

    /* clear all existing entries from the table */
    void clear();

    /* 
     *   Build a translation table from runtime metaclass index to the
     *   compiler's internal TC_META_xxx scheme.  The table is simply an
     *   array of TC_META_xxx values indexed by metaclass index number.  The
     *   caller is responsible for freeing the returned memory via t3free()
     *   when done with it.  
     */
    static tc_metaclass_t *build_runtime_to_compiler_id_table(VMG0_);
    
    /* 
     *   Add an entry to the table, given the metaclass identifier (a
     *   string giving the universally unique name for the metaclass).
     *   Fills in the next available slot.  Throws an error if the
     *   metaclass is not present in the system.
     *   
     *   If min_prop and max_prop are both VM_INVALID_PROP, the metaclass
     *   has no properties in the translation table.  
     */
    void add_entry(const char *metaclass_id, size_t func_cnt,
                   vm_prop_id_t min_prop, vm_prop_id_t max_prop);

    /* add an entry to the table given the registration table index */
    void add_entry(const char *metaclass_id, uint idx, size_t func_cnt,
                   vm_prop_id_t min_prop, vm_prop_id_t max_prop);

    /* 
     *   Add an entry to the table given the registration table index, but
     *   only if the metaclass at this index isn't already defined in the
     *   dependency table; if the metaclass is already in the dependency
     *   table, this function has no effect.
     */
    void add_entry_if_new(uint reg_table_idx, size_t func_cnt,
                          vm_prop_id_t min_prop, vm_prop_id_t max_prop);

    /*
     *   Get the dependency table index for a metaclass, given the
     *   registration table index.  If the metaclass is not present in the
     *   metaclass table, we'll return -1. 
     */
    int get_dependency_index(uint reg_table_idx) const
        { return reverse_map_[reg_table_idx]; }

    /*
     *   Get the entry for a given external metaclass ID.  This returns the
     *   table entry if the metaclass is loaded, null if not.  If the name is
     *   given with a "/version" suffix, we'll return null if the loaded
     *   version is older than the specified version.  
     */
    vm_meta_entry_t *get_entry_by_id(const char *id) const;

    /* 
     *   get the depencency table entry for a metaclass, given the
     *   registration table index 
     */
    vm_meta_entry_t *get_entry_from_reg(int reg_idx)
    {
        /* translate the registration table index into a dependency index */
        int dep_idx = get_dependency_index(reg_idx);

        /* 
         *   if we have a valid dependency index, return it; otherwise,
         *   return null 
         */
        if (dep_idx >= 0)
            return &table_[dep_idx];
        else
            return 0;
    }

    /*
     *   Given a registration table index for a metaclass and a property
     *   ID, retrieve the metaclass function vector index for the property
     *   according to the image file's property mapping for the metaclass.
     *   Returns zero if the property is not in the mapping, 1 for the
     *   first function in the vector, 2 for the second function, and so
     *   on.
     *   
     *   Note that we use the registration table index, NOT the dependency
     *   index, because this function is provided for use by the metaclass
     *   itself, which only has convenient access to its registration
     *   index.  
     */
    int prop_to_vector_idx(uint reg_table_idx, vm_prop_id_t prop)
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

    /* get the entry at a given dependency table index */
    vm_meta_entry_t *get_entry(int idx) { return &table_[idx]; }

    /* get the total number of entries in the table */
    size_t get_count() const { return count_; }

    /* 
     *   Invoke the VM-stack-based constructor for the metaclass at the
     *   given index to create a new instance of that metaclass.  Throws
     *   an error if no such entry is defined.  Returns the object ID of
     *   the constructed object.  
     */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr,
                                  uint idx, uint argc);

    /* invoke a static property of a metaclass */
    int call_static_prop(VMG_ vm_val_t *result, const uchar **pc_ptr,
                         uint idx, uint *argc, vm_prop_id_t prop);

    /*
     *   Create an object with the given ID and load the object from the
     *   image file.
     */
    void create_from_image(VMG_ uint idx, vm_obj_id_t id,
                           const char *ptr, size_t siz);

    /* 
     *   create an object in preparation for loading the object from a
     *   saved state file 
     */
    void create_for_restore(VMG_ uint idx, vm_obj_id_t id);

    /* 
     *   write the dependency table to a file, for later reloading with
     *   read_from_file() 
     */
    void write_to_file(class CVmFile *fp);

    /* 
     *   Read the dependency table from a file, as previously written by
     *   write_to_file().  Returns zero on success, or an error code on
     *   failure.  We will entirely clear the current table before
     *   loading, so the new table will replace the existing table.  
     */
    int read_from_file(class CVmFile *fp);

    /* rebuild the image file (for use after preinitialization) */
    void rebuild_image(class CVmImageWriter *writer);

    /*
     *   Create an IntrinsicClass object for each metaclass that doesn't
     *   already have an IntrinsicClass object.  This ensures that every
     *   intrinsic class in use has an associated IntrinsicClass instance,
     *   even if the compiler didn't generate one. 
     */
    void create_intrinsic_class_instances(VMG0_);

    /* forget IntrinsicClass objects we created at startup */
    void forget_intrinsic_class_instances(VMG0_);

private:
    /* 
     *   Ensure we have space for a given number of entries, allocating
     *   more if necessary.  If we must allocate more space, we'll
     *   increase the current allocation size by at least the given
     *   increment (more if necessary to bring it up to the required
     *   size).  
     */
    void ensure_space(size_t entries, size_t increment);

    /* the table array */
    vm_meta_entry_t *table_;

    /* 
     *   reverse dependency map - each entry in this table is the
     *   dependency table index of the metaclass at the given registration
     *   table index:
     *   
     *   reserve_map[registration_table_index] = dependency_table_index
     *   
     *   This information lets us determine if a given metaclass is in the
     *   current dependency table at all, and if so what it's index is.
     *   We store -1 in each entry that doesn't appear in the dependency
     *   table.  
     */
    int *reverse_map_;
    
    /* number of entries defined in the table */
    size_t count_;

    /* number of entries allocated for the table */
    size_t alloc_;
};


#endif /* VMMETA_H */
