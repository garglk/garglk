#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2001, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmlookup.cpp - LookupTable metaclass implementation
Function
  
Notes
  
Modified
  02/06/01 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "t3std.h"
#include "vmlookup.h"
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
#include "vmiter.h"
#include "vmrun.h"
#include "vmlst.h"
#include "vmvec.h"



/* ------------------------------------------------------------------------ */
/*
 *   LookupTable undo record.  We attach this record to the standard undo
 *   record via the 'ptrval' field.  
 */
struct lookuptab_undo_rec
{
    /* action type */
    enum lookuptab_undo_action action;

    /* key value */
    vm_val_t key;
};


/* ------------------------------------------------------------------------ */
/*
 *   Allocate a new extension structure 
 */
vm_lookup_ext *vm_lookup_ext::alloc_ext(VMG_ CVmObjLookupTable *self,
                                        uint bucket_cnt, uint value_cnt)
{
    size_t siz;
    vm_lookup_ext *ext;

    /* calculate how much space we need */
    siz = sizeof(vm_lookup_ext)
          + (bucket_cnt - 1) * sizeof(ext->buckets[0])
          + value_cnt * sizeof(vm_lookup_val);

    /* allocate the memory */
    ext = (vm_lookup_ext *)G_mem->get_var_heap()->alloc_mem(siz, self);

    /* remember the table sizes */
    ext->bucket_cnt = bucket_cnt;
    ext->value_cnt = value_cnt;

    /* set the default value to nil initially */
    ext->default_value.set_nil();

    /* return the new extension */
    return ext;
}

/*
 *   initialize the buckets and free list 
 */
void vm_lookup_ext::init_ext()
{
    uint i;
    vm_lookup_val **bucketp;
    vm_lookup_val *valp;

    /* all of the buckets are initially empty */
    for (i = bucket_cnt, bucketp = buckets ; i != 0 ; --i, ++bucketp)
        *bucketp = 0;

    /* 
     *   Initialize the value free list.  We suballocate the value entry
     *   structures out of our main allocation block; the available memory
     *   for the structures comes immediately after the last bucket, so
     *   bucketp points to the first value entry. 
     */
    valp = (vm_lookup_val *)(void *)bucketp;
    first_free = valp;
    for (first_free = valp, i = value_cnt ; i != 0 ; --i, ++valp)
    {
        /* link it into the free list */
        valp->nxt = valp + 1;

        /* mark it as empty */
        valp->key.set_empty();
        valp->val.set_empty();
    }

    /* terminate the free list at the last element */
    (valp - 1)->nxt = 0;
}

/*
 *   Reallocate an extension structure, adding more value entries. 
 */
vm_lookup_ext *vm_lookup_ext::expand_ext(VMG_ CVmObjLookupTable *self,
                                         vm_lookup_ext *old_ext,
                                         uint new_value_cnt)
{
    vm_lookup_ext *new_ext;

    /* allocate a new extension structure of the requested size */
    new_ext = alloc_ext(vmg_ self, old_ext->bucket_cnt, new_value_cnt);

    /* copy the old extension data into the new extension */
    new_ext->copy_ext_from(old_ext);

    /* delete the old memory */
    G_mem->get_var_heap()->free_mem(old_ext);

    /* return the new extension */
    return new_ext;
}

/*
 *   Copy extension data 
 */
void vm_lookup_ext::copy_ext_from(vm_lookup_ext *old_ext)
{
    vm_lookup_val **oldbp;
    vm_lookup_val **newbp;
    vm_lookup_val *oldval;
    vm_lookup_val *newval;
    char *oldbase;
    char *newbase;
    uint i;

    /* the bucket counts must be the same in both extensions */
    assert(bucket_cnt == old_ext->bucket_cnt);

    /* we must have at least as many entries as the old one had */
    assert(value_cnt >= old_ext->value_cnt);

    /* copy the default value */
    default_value = old_ext->default_value;

    /* get the new and old base values for faster pointer arithmetic */
    oldbase = (char *)old_ext->idx_to_val(0);
    newbase = (char *)idx_to_val(0);

    /* copy the hash buckets from the old extension to the new extension */
    for (i = old_ext->bucket_cnt, oldbp = old_ext->buckets, newbp = buckets ;
         i != 0 ; --i, ++newbp, ++oldbp)
    {
        /* copy the bucket pointer, adjusting to our local pointer scheme */
        *newbp = (*oldbp == 0
                  ? 0
                  : (vm_lookup_val *)(((char *)*oldbp - oldbase) + newbase));
    }

    /* copy the values from the old extension to the new extension */
    for (i = old_ext->value_cnt, oldval = old_ext->idx_to_val(0),
         newval = idx_to_val(0) ;
         i != 0 ;
         --i, ++oldval, ++newval)
    {
        /* copy this entry */
        newval->key = oldval->key;
        newval->val = oldval->val;
        newval->nxt = (oldval->nxt == 0
                       ? 0
                       : (vm_lookup_val *)(((char *)oldval->nxt - oldbase)
                                           + newbase));
    }

    /* link all of the remaining free values into the free list */
    first_free = newval;
    for (i = value_cnt - old_ext->value_cnt ; i != 0 ; --i, ++newval)
    {
        /* link this value into the free list */
        newval->nxt = newval + 1;

        /* mark this free value as empty */
        newval->key.set_empty();
        newval->val.set_empty();
    }

    /* terminate the free list */
    (newval - 1)->nxt = 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   LookupTable object statics 
 */

/* metaclass registration object */
static CVmMetaclassLookupTable metaclass_reg_obj;
CVmMetaclass *CVmObjLookupTable::metaclass_reg_ = &metaclass_reg_obj;

/* function table */
int (CVmObjLookupTable::
     *CVmObjLookupTable::func_table_[])(VMG_ vm_obj_id_t self,
                                        vm_val_t *retval, uint *argc) =
{
    &CVmObjLookupTable::getp_undef,                                    /* 0 */
    &CVmObjLookupTable::getp_key_present,                              /* 1 */
    &CVmObjLookupTable::getp_remove_entry,                             /* 2 */
    &CVmObjLookupTable::getp_apply_all,                                /* 3 */
    &CVmObjLookupTable::getp_for_each,                                 /* 4 */
    &CVmObjLookupTable::getp_count_buckets,                            /* 5 */
    &CVmObjLookupTable::getp_count_entries,                            /* 6 */
    &CVmObjLookupTable::getp_for_each_assoc,                           /* 7 */
    &CVmObjLookupTable::getp_keys_to_list,                             /* 8 */
    &CVmObjLookupTable::getp_vals_to_list,                             /* 9 */
    &CVmObjLookupTable::getp_get_def_val,                             /* 10 */
    &CVmObjLookupTable::getp_set_def_val,                             /* 11 */
    &CVmObjLookupTable::getp_nthKey,                                  /* 12 */
    &CVmObjLookupTable::getp_nthVal                                   /* 13 */
};


/* ------------------------------------------------------------------------ */
/*
 *   Lookup Table metaclass implementation 
 */

/* 
 *   create a lookup table with a given hash table size and the given
 *   initial entry table size 
 */
CVmObjLookupTable::CVmObjLookupTable(VMG_ size_t bucket_cnt, size_t val_cnt)
{
    vm_val_t empty;

    /* set up an empty value */
    empty.set_empty();
    
    /* allocate our extension structure from the variable heap */
    ext_ = (char *)vm_lookup_ext::alloc_ext(vmg_ this, bucket_cnt, val_cnt);

    /* initialize the extension */
    get_ext()->init_ext();
}    


/*
 *   create 
 */
vm_obj_id_t CVmObjLookupTable::create(VMG_ int in_root_set,
                                      uint bucket_count, uint init_capacity)
{
    vm_obj_id_t id;

    /* allocate the object ID */
    id = vm_new_id(vmg_ in_root_set, TRUE, FALSE);

    /* create the object */
    new (vmg_ id) CVmObjLookupTable(vmg_ bucket_count, init_capacity);

    /* return the new ID */
    return id;
}

/* 
 *   create dynamically using stack arguments 
 */
vm_obj_id_t CVmObjLookupTable::create_from_stack(
    VMG_ const uchar **pc_ptr, uint argc)
{
    vm_obj_id_t id;
    size_t bucket_count;
    size_t init_capacity;
    vm_val_t src_obj;

    /* parse the arguments */
    get_constructor_args(vmg_ argc, &bucket_count, &init_capacity, &src_obj);
    
    /* 
     *   allocate the object ID - this type of construction never creates a
     *   root object 
     */
    id = vm_new_id(vmg_ FALSE, TRUE, FALSE);

    /* create the object */
    new (vmg_ id) CVmObjLookupTable(vmg_ bucket_count, init_capacity);

    /* populate it with the source list if desired */
    ((CVmObjLookupTable *)vm_objp(vmg_ id))->
        populate_from_list(vmg_ &src_obj);

    /* discard the source object gc protection */
    G_stk->discard();

    /* return the new ID */
    return id;
}

/* 
 *   get and check constructor arguments 
 */
void CVmObjLookupTable::get_constructor_args(VMG_ uint argc,
                                             size_t *bucket_count,
                                             size_t *init_capacity,
                                             vm_val_t *src_obj)
{
    /* presume no source object */
    src_obj->set_nil();

    /* check arguments */
    if (argc == 0)
    {
        /* no arguments - they want default parameters */
        *bucket_count = 32;
        *init_capacity = 64;
    }
    else if (argc == 1)
    {
        int cnt;
        
        /* 
         *   One argument - they want to create from a list.  Retrieve the
         *   list or vector object. 
         */
        G_stk->pop(src_obj);

        /* make sure it's a list or vector */
        if (!src_obj->is_listlike(vmg0_)
            || (cnt = src_obj->ll_length(vmg0_)) < 0)
            err_throw(VMERR_BAD_VAL_BIF);

        /* use 1.5x the pair count as the bucket count and capacity */
        *bucket_count = *init_capacity = cnt*3 / 2;
    }
    else if (argc == 2)
    {
        /* 
         *   two arguments - they specified the bucket count and initial
         *   capacity; pop the parameters 
         */
        *bucket_count = (size_t)CVmBif::pop_long_val(vmg0_);
        *init_capacity = (size_t)CVmBif::pop_long_val(vmg0_);
    }
    else
    {
        /* invalid arguments */
        err_throw(VMERR_WRONG_NUM_OF_ARGS);
    }

    /* re-push the source object to protect it from gc */
    G_stk->push(src_obj);

    /* make sure both are positive */
    if (*bucket_count <= 0 || *init_capacity <= 0)
        err_throw(VMERR_BAD_VAL_BIF);
}

/*
 *   Populate the table from a source list or vector.  This should only be
 *   used during construction, because it doesn't save undo.  (It's not
 *   necessary to save undo during construction because the whole object
 *   creation and construction is atomic for undo purposes: if we undo any of
 *   this, we undo the whole creation of the object, so there's no need to
 *   worry about saving old state internal to the object.)  
 */
void CVmObjLookupTable::populate_from_list(VMG_ const vm_val_t *src)
{
    /* copy elements from the source object */
    int cnt = src->ll_length(vmg0_);
    int i;
    for (i = 1 ; i + 1 <= cnt ; )
    {
        vm_val_t key, val;

        /* get the next two elements - they're always key, value pairs */
        src->ll_index(vmg_ &key, i++);
        src->ll_index(vmg_ &val, i++);

        /* set them in the table */
        set_or_add_entry(vmg_ &key, &val);
    }

    /* if there's a single remaining entry, it's the default value */
    if (i <= cnt)
        src->ll_index(vmg_ &get_ext()->default_value, i);
}


/*
 *   Create a copy of this object 
 */
vm_obj_id_t CVmObjLookupTable::create_copy(VMG0_)
{
    vm_obj_id_t id;
    CVmObjLookupTable *new_obj;
    
    /* allocate the object ID */
    id = vm_new_id(vmg_ FALSE, TRUE, FALSE);

    /* allocate a new object */
    new_obj = new (vmg_ id) CVmObjLookupTable();

    /* 
     *   set up the new object with our same sizes, but don't initialize it
     *   - we're going to blast all of our data directly into the new
     *   extension, so we don't need to waste time setting up an empty
     *   initial state 
     */
    new_obj->ext_ = (char *)vm_lookup_ext::alloc_ext(
        vmg_ new_obj, get_bucket_count(), get_entry_count());

    /* copy my data to the next object */
    new_obj->get_ext()->copy_ext_from(get_ext());

    /* return the new object's id */
    return id;
}

/* 
 *   notify of deletion 
 */
void CVmObjLookupTable::notify_delete(VMG_ int /*in_root_set*/)
{
    /* free our additional data, if we have any */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);
}

/* 
 *   set a property 
 */
void CVmObjLookupTable::set_prop(VMG_ class CVmUndo *undo,
                                 vm_obj_id_t self, vm_prop_id_t prop,
                                 const vm_val_t *val)
{
    /* no settable properties - throw an error */
    err_throw(VMERR_INVALID_SETPROP);
}

/* 
 *   get a property 
 */
int CVmObjLookupTable::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
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
    return CVmObjCollection::
        get_prop(vmg_ prop, retval, self, source_obj, argc);
}

/*
 *   apply an undo record 
 */
void CVmObjLookupTable::apply_undo(VMG_ struct CVmUndoRecord *undo_rec)
{
    if (undo_rec->id.ptrval != 0)
    {
        lookuptab_undo_rec *rec;

        /* get our private record from the standard undo record */
        rec = (lookuptab_undo_rec *)undo_rec->id.ptrval;
    
        /* check the action in the record */
        switch(rec->action)
        {
        case LOOKUPTAB_UNDO_NULL:
            /* 
             *   null record, which means that it had a weak reference to an
             *   object that has been deleted - there's no way to reinstate
             *   such records, so ignore it 
             */
            break;
            
        case LOOKUPTAB_UNDO_ADD:
            /* we added the entry, so we must now delete it */
            del_entry(vmg_ &rec->key);
            break;

        case LOOKUPTAB_UNDO_DEL:
            /* we deleted the entry, so we must now add it */
            add_entry(vmg_ &rec->key, &undo_rec->oldval);
            break;

        case LOOKUPTAB_UNDO_MOD:
            /* we modified the entry, so we must change it back */
            mod_entry(vmg_ &rec->key, &undo_rec->oldval);
            break;

        case LOOKUPTAB_UNDO_DEFVAL:
            /* we modified the default value; change it back */
            get_ext()->default_value = undo_rec->oldval;
            break;
        }

        /* discard the private record */
        t3free(rec);

        /* clear the pointer in the main record so we know it's gone */
        undo_rec->id.ptrval = 0;
    }
}

/*
 *   discard extra undo information 
 */
void CVmObjLookupTable::discard_undo(VMG_ CVmUndoRecord *rec)
{
    /* delete our extra information record */
    if (rec->id.ptrval != 0)
    {
        /* free the record */
        t3free((lookuptab_undo_rec *)rec->id.ptrval);

        /* clear the pointer so we know it's gone */
        rec->id.ptrval = 0;
    }
}

/*
 *   Mark undo references.
 */
void CVmObjLookupTable::
   mark_undo_ref(VMG_ struct CVmUndoRecord *undo_rec)
{
    lookuptab_undo_rec *rec;

    /* get our private record from the standard undo record */
    rec = (lookuptab_undo_rec *)undo_rec->id.ptrval;

    /* if the key in the record is an object, mark it referenced */
    if (rec->key.typ == VM_OBJ)
        G_obj_table->mark_all_refs(rec->key.val.obj, VMOBJ_REACHABLE);

    /* if the value in the record is an object, mark it as well */
    if (undo_rec->oldval.typ == VM_OBJ)
        G_obj_table->mark_all_refs(undo_rec->oldval.val.obj,
                                   VMOBJ_REACHABLE);
}

/*
 *   Mark references.  Keys (but not values) are strongly referenced.  
 */
void CVmObjLookupTable::mark_refs(VMG_ uint state)
{
    /* get my extension */
    vm_lookup_ext *ext = get_ext();

    /* mark the default value */
    const vm_val_t *val = &ext->default_value;
    if (val->typ == VM_OBJ)
        G_obj_table->mark_all_refs(val->val.obj, state);

    /* run through my buckets */
    vm_lookup_val **bp = ext->buckets;
    size_t i = ext->bucket_cnt;
    for ( ; i != 0 ; ++bp, --i)
    {
        /* run through all entries attached to this bucket */
        for (const vm_lookup_val *entry = *bp ; entry != 0 ;
             entry = entry->nxt)
        {
            /* if the key is an object, mark it as referenced */
            val = &entry->key;
            if  (val->typ == VM_OBJ)
                G_obj_table->mark_all_refs(val->val.obj, state);

            /* if the entry is an object, mark it as referenced */
            val = &entry->val;
            if (val->typ == VM_OBJ)
                G_obj_table->mark_all_refs(val->val.obj, state);
        }
    }
}

/* 
 *   load from an image file 
 */
void CVmObjLookupTable::load_from_image(VMG_ vm_obj_id_t self,
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
void CVmObjLookupTable::reload_from_image(VMG_ vm_obj_id_t self,
                                          const char *ptr, size_t siz)
{
    /* load our image data */
    load_image_data(vmg_ ptr, siz);
}

/*
 *   load or reload data from the image 
 */
void CVmObjLookupTable::load_image_data(VMG_ const char *ptr, size_t siz)
{
    uint bucket_cnt;
    uint val_cnt;
    uint i;
    const char *ibp;
    vm_lookup_val **ebp;
    vm_lookup_val *eval;
    vm_lookup_ext *ext;

    /* free our existing extension, if we have one */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);

    /* read the bucket and value counts from the header */
    bucket_cnt = osrp2(ptr);
    val_cnt = osrp2(ptr + 2);

    /* allocate space for a copy of the image data */
    ext = vm_lookup_ext::alloc_ext(vmg_ this, bucket_cnt, val_cnt);
    ext_ = (char *)ext;

    /* initialize the free list pointer, given as a 1-based entry index */
    ext->first_free = ext->img_idx_to_val(osrp2(ptr + 4));

    /* initialize the table from the load image data */
    for (i = bucket_cnt, ibp = ptr + 6, ebp = ext->buckets ;
         i != 0 ; --i, ibp += 2, ++ebp)
    {
        /* translate the image file index to a value pointer */
        *ebp = ext->img_idx_to_val(osrp2(ibp));
    }

    /* initialize the value entries */
    for (i = val_cnt, eval = ext->idx_to_val(0) ;
         i != 0 ; --i, ibp += VMLOOKUP_VALUE_SIZE, ++eval)
    {
        /* read the key and value */
        vmb_get_dh(ibp, &eval->key);
        vmb_get_dh(ibp + VMB_DATAHOLDER, &eval->val);

        /* remember the next pointer, which is given as a 1-based index */
        eval->nxt = ext->img_idx_to_val(osrp2(ibp + VMB_DATAHOLDER*2));
    }

    /* if a default value is included, read it */
    ext->default_value.set_nil();
    if (ibp + VMB_DATAHOLDER <= ptr + siz)
    {
        /* read the default value and skip it in the source data */
        vmb_get_dh(ibp, &ext->default_value);
        ibp += VMB_DATAHOLDER;
    }
}

/* 
 *   save to a file 
 */
void CVmObjLookupTable::save_to_file(VMG_ class CVmFile *fp)
{
    vm_lookup_ext *ext = get_ext();
    uint i;
    vm_lookup_val **bp;
    vm_lookup_val *val;
    char buf[VMLOOKUP_VALUE_SIZE];

    /* write our bucket count, value count, and first-free index */
    fp->write_uint2(ext->bucket_cnt);
    fp->write_uint2(ext->value_cnt);
    fp->write_uint2(ext->val_to_img_idx(ext->first_free));

    /*
     *   If the image file was compiled for version 030003 or later of the
     *   LookupTable object, save the default value.  The default value
     *   wasn't added until 030003, so source code compiled against earlier
     *   versions of the metaclass spec can't change it from the 'nil'
     *   default.  Furthermore, these image files could be loaded on VMs that
     *   have older versions of the metaclass that won't know to look for a
     *   default value in the loaded data, so to be compatible with those
     *   readers we must omit the value entirely.  If the image file was
     *   compiled with 030003 or later, it can only be loaded by VMs with
     *   LookupTable implementations that know to read the value.  
     */
    if (image_file_version_ge(vmg_ "030003"))
    {
        vmb_put_dh(buf, &ext->default_value);
        fp->write_bytes(buf, VMB_DATAHOLDER);
    }

    /* write the buckets */
    for (i = ext->bucket_cnt, bp = ext->buckets ; i != 0 ; --i, ++bp)
        fp->write_uint2(ext->val_to_img_idx(*bp));

    /* write the values */
    for (i = ext->value_cnt, val = ext->idx_to_val(0) ; i != 0 ; --i, ++val)
    {
        uint idx;
        
        /* store the key, value, and index */
        vmb_put_dh(buf, &val->key);
        vmb_put_dh(buf + VMB_DATAHOLDER, &val->val);
        idx = ext->val_to_img_idx(val->nxt);
        oswp2(buf + VMB_DATAHOLDER*2, idx);

        /* write the data */
        fp->write_bytes(buf, VMLOOKUP_VALUE_SIZE);
    }
}

/* 
 *   restore from a file 
 */
void CVmObjLookupTable::restore_from_file(VMG_ vm_obj_id_t self,
                                          CVmFile *fp, CVmObjFixup *fixups)
{
    vm_lookup_ext *ext;
    char buf[64];
    size_t bucket_cnt;
    size_t val_cnt;
    size_t i;
    vm_lookup_val **bp;
    vm_lookup_val *entry;
    uint idx;
    
    /* free our existing extension, if we have one */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);

    /* read the fixed fields */
    fp->read_bytes(buf, 6);
    bucket_cnt = vmb_get_uint2(buf);
    val_cnt = vmb_get_uint2(buf + 2);

    /* allocate the extension structure */
    ext = vm_lookup_ext::alloc_ext(vmg_ this, bucket_cnt, val_cnt);
    ext_ = (char *)ext;

    /* store the free pointer */
    ext->first_free = ext->img_idx_to_val(vmb_get_uint2(buf + 4));

    /* 
     *   if the saver was using version 030003 or later, there's a default
     *   value; otherwise the default value is fixed at 'nil' 
     */
    ext->default_value.set_nil();
    if (image_file_version_ge(vmg_ "030003"))
    {
        /* read the default value */
        fp->read_bytes(buf, VMB_DATAHOLDER);

        /* apply fixups */
        fixups->fix_dh(vmg_ buf);

        /* decode it into the default value in the extension */
        vmb_get_dh(buf, &ext->default_value);
    }

    /* read the buckets */
    for (i = bucket_cnt, bp = ext->buckets ; i != 0 ; ++bp, --i)
    {
        /* read this bucket pointer and store it */
        idx = fp->read_uint2();
        *bp = ext->img_idx_to_val(idx);
    }

    /* read the value entries */
    for (i = val_cnt, entry = ext->idx_to_val(0) ; i != 0 ; --i, ++entry)
    {
        /* read this value entry */
        fp->read_bytes(buf, VMLOOKUP_VALUE_SIZE);

        /* fix up the key and value dataholders */
        fixups->fix_dh_array(vmg_ buf, 2);

        /* store the key and value */
        vmb_get_dh(buf, &entry->key);
        vmb_get_dh(buf + VMB_DATAHOLDER, &entry->val);

        /* store the next pointer */
        idx = osrp2(buf + VMB_DATAHOLDER*2);
        entry->nxt = ext->img_idx_to_val(idx);
    }
}

/* 
 *   create an iterator 
 */
void CVmObjLookupTable::new_iterator(VMG_ vm_val_t *retval,
                                     const vm_val_t *self_val)
{
    vm_val_t copy_val;

    /* save a copy of myself for protection against garbage collection */
    G_stk->push(self_val);

    /* 
     *   create a copy of my object, so that we can iterate over a snapshot
     *   of our state even if we subsequently change our state
     */
    copy_val.set_obj(create_copy(vmg0_));

    /* set up a new lookup table iterator object on the copy */
    retval->set_obj(CVmObjIterLookupTable::create_for_coll(vmg_ &copy_val));

    /* done with the gc protection */
    G_stk->discard();
}

/* 
 *   create a live iterator 
 */
void CVmObjLookupTable::new_live_iterator(VMG_ vm_val_t *retval,
                                          const vm_val_t *self_val)
{
    /* set up a new lookup table iterator directly on myself */
    retval->set_obj(CVmObjIterLookupTable::create_for_coll(vmg_ self_val));
}

/*
 *   add an undo record 
 */
void CVmObjLookupTable::add_undo_rec(VMG_ vm_obj_id_t self,
                                     lookuptab_undo_action action,
                                     const vm_val_t *key,
                                     const vm_val_t *old_entry_val)
{
    lookuptab_undo_rec *rec;
    vm_val_t nil_val;

    /* allocate our record extension */
    rec = (lookuptab_undo_rec *)t3malloc(sizeof(lookuptab_undo_rec));

    /* if either the key or old value are null pointers, use a nil value */
    nil_val.set_nil();
    if (key == 0)
        key = &nil_val;
    if (old_entry_val == 0)
        old_entry_val = &nil_val;

    /* set up the record */
    rec->action = action;
    rec->key = *key;

    /* add the record to the global undo stream */
    if (!G_undo->add_new_record_ptr_key(vmg_ self, rec, old_entry_val))
    {
        /* 
         *   we didn't add an undo record, so our extra undo information
         *   isn't actually going to be stored in the undo system - hence we
         *   must delete our extra information 
         */
        t3free(rec);
    }
}

/*
 *   Add an entry; does not generate undo.
 */
void CVmObjLookupTable::add_entry(VMG_ 
                                  const vm_val_t *key, const vm_val_t *val)
{
    uint hashval;
    vm_lookup_val *entry;

    /* push the key and value as gc protection for a moment */
    G_stk->push(key);
    G_stk->push(val);
    
    /* calculate the hash value for the key */
    hashval = calc_key_hash(vmg_ key);

    /* allocate an entry */
    entry = alloc_new_entry(vmg0_);

    /* link this entry into the chain at this hash value */
    entry->nxt = get_ext()->buckets[hashval];
    get_ext()->buckets[hashval] = entry;

    /* set the key and value for this entry */
    entry->key = *key;
    entry->val = *val;

    /* discard the gc protection */
    G_stk->discard(2);
}

/*
 *   Add an entry, generating undo 
 */
void CVmObjLookupTable::add_entry_undo(VMG_ vm_obj_id_t self,
                                       const vm_val_t *key,
                                       const vm_val_t *val)
{
    /* 
     *   Add undo for the change.  Since we're adding a new record, there's
     *   no old value for the record. 
     */
    add_undo_rec(vmg_ self, LOOKUPTAB_UNDO_ADD, key, 0);
    
    /* add the entry */
    add_entry(vmg_ key, val);
}


/*
 *   Delete an entry; does not generate undo.
 */
void CVmObjLookupTable::del_entry(VMG_ const vm_val_t *key)
{
    uint hashval;
    vm_lookup_val *entry;
    vm_lookup_val *prv_entry;

    /* find the entry for the key */
    entry = find_entry(vmg_ key, &hashval, &prv_entry);

    /* if we found it, unlink it */
    if (entry != 0)
        unlink_entry(vmg_ entry, hashval, prv_entry);
}

/*
 *   Unlink an entry 
 */
void CVmObjLookupTable::unlink_entry(VMG_ vm_lookup_val *entry, uint hashval,
                                     vm_lookup_val *prv_entry)
{
    vm_lookup_ext *ext = get_ext();
    
    /* 
     *   set the previous item's next pointer, or the head of the chain, as
     *   appropriate 
     */
    if (prv_entry == 0)
        ext->buckets[hashval] = entry->nxt;
    else
        prv_entry->nxt = entry->nxt;
    
    /* link the entry into the free list */
    entry->nxt = ext->first_free;
    ext->first_free = entry;
    
    /* set this entry's key and value to 'empty' to indicate that it's free */
    entry->key.set_empty();
    entry->val.set_empty();
}

/*
 *   Modify an entry; does not generate undo.
 */
void CVmObjLookupTable::mod_entry(VMG_ const vm_val_t *key,
                                  const vm_val_t *val)
{
    vm_lookup_val *entry;

    /* find the entry for the key */
    entry = find_entry(vmg_ key, 0, 0);

    /* if we found it, change the value to the given value */
    if (entry != 0)
        entry->val = *val;
}

/*
 *   Set an entry's value, generating undo 
 */
void CVmObjLookupTable::set_entry_val_undo(VMG_ vm_obj_id_t self,
                                           vm_lookup_val *entry,
                                           const vm_val_t *val)
{
    /* generate undo for the change */
    add_undo_rec(vmg_ self, LOOKUPTAB_UNDO_MOD, &entry->key, &entry->val);

    /* update the entry */
    entry->val = *val;
}

/*
 *   Find an entry for a given key.  Returns the entry index, and fills in
 *   the hash value and previous-entry output variables if provided.
 *   Returns zero if there is no such key.  
 */
vm_lookup_val *CVmObjLookupTable::find_entry(VMG_ const vm_val_t *key,
                                             uint *hashval_p,
                                             vm_lookup_val **prv_entry_p)
{
    vm_lookup_ext *ext = get_ext();
    uint hashval;
    vm_lookup_val *entry;
    vm_lookup_val *prv_entry;

    /* compute the hash value (and return it to the caller if desired) */
    hashval = calc_key_hash(vmg_ key);
    if (hashval_p != 0)
        *hashval_p = hashval;

    /* scan the hash chain for this entry */
    for (prv_entry = 0, entry = ext->buckets[hashval] ; entry != 0 ;
         prv_entry = entry, entry = entry->nxt)
    {
        /* if it matches the key we're looking for, this is the one */
        if (entry->key.equals(vmg_ key))
            break;
    }

    /* if the caller wanted to know the previous entry, tell them */
    if (prv_entry_p != 0)
        *prv_entry_p = prv_entry;

    /* return the entry where we found the key, if indeed we found it */
    return entry;
}


/*
 *   Calculate a hash value for a key
 */
uint CVmObjLookupTable::calc_key_hash(VMG_ const vm_val_t *key)
{
    uint hash;
    
    /* calculate the raw hash for the value */
    hash = key->calc_hash(vmg0_);

    /* reduce the hash value modulo the hash table size */
    hash %= get_bucket_count();

    /* return the hash value */
    return hash;
}

/*
 *   Allocate a new entry 
 */
vm_lookup_val *CVmObjLookupTable::alloc_new_entry(VMG0_)
{
    /* if necessary, add more entry slots to the table */
    expand_if_needed(vmg0_);

    /* get the first entry from the free list */
    vm_lookup_val *entry = get_first_free();

    /* unlink this entry from the free list */
    set_first_free(entry->nxt);
    entry->nxt = 0;

    /* return the free entry */
    return entry;
}

/*
 *   Check for free space, and expand the table if necessary.  
 */
void CVmObjLookupTable::expand_if_needed(VMG0_)
{
    /* if we have any free entries left, we need do nothing */
    if (get_first_free() != 0)
        return;

    /* increase the entry count by 50%, or 16 slots, whichever is more */
    uint new_entry_cnt = get_entry_count() + (get_entry_count() >> 1);
    if (new_entry_cnt < get_entry_count() + 16)
        new_entry_cnt = get_entry_count() + 16;

    /* reallocate the extension at the new size */
    ext_ = (char *)vm_lookup_ext::expand_ext(
        vmg_ this, get_ext(), new_entry_cnt);
}

/* ------------------------------------------------------------------------ */
/*
 *   Get a value by index 
 */
int CVmObjLookupTable::index_val_q(VMG_ vm_val_t *result,
                                    vm_obj_id_t self,
                                    const vm_val_t *index_val)
{
    vm_lookup_val *entry;
    
    /* find the entry */
    entry = find_entry(vmg_ index_val, 0, 0);

    /* if we found it, return it; otherwise, return nil */
    if (entry != 0)
    {
        /* get the value from the entry and hand it back as the result */
        *result = entry->val;
    }
    else
    {
        /* not found - return the default value */
        *result = get_ext()->default_value;
    }

    /* handled */
    return TRUE;
}

/*
 *   Get a value by index, with an indication of whether or not the value
 *   exists. 
 */
int CVmObjLookupTable::index_check(VMG_ vm_val_t *result, const vm_val_t *idx)
{
    /* find the entry */
    vm_lookup_val *entry = find_entry(vmg_ idx, 0, 0);

    /* if we found it, return it; otherwise, return nil */
    if (entry != 0)
    {
        /* get the indexed value */
        *result = entry->val;

        /* indicate that the key is present is in the table */
        return TRUE;
    }
    else
    {
        /* 
         *   not found - set a nil result value and indicate that the key
         *   isn't in the table 
         */
        result->set_nil();
        return FALSE;
    }
}


/*
 *   Set or add an entry, with no undo 
 */
void CVmObjLookupTable::set_or_add_entry(VMG_ const vm_val_t *key,
                                         const vm_val_t *val)
{
    vm_lookup_val *entry;

    /* look for an existing entry with this key */
    entry = find_entry(vmg_ key, 0, 0);

    /* if we found an existing entry, modify it; otherwise, add a new one */
    if (entry != 0)
    {   
        /* set the new value for this entry */
        entry->val = *val;
    }
    else
    {
        /* add a new entry with the given key */
        add_entry(vmg_ key, val);
    }
}

/*
 *   Set a value by index 
 */
int CVmObjLookupTable::set_index_val_q(VMG_ vm_val_t *new_container,
                                       vm_obj_id_t self,
                                       const vm_val_t *index_val,
                                       const vm_val_t *new_val)
{
    vm_lookup_val *entry;
    
    /* look for an existing entry with this key */
    entry = find_entry(vmg_ index_val, 0, 0);

    /* if we found an existing entry, modify it; otherwise, add a new one */
    if (entry != 0)
    {   
        /* set the new value for this entry */
        set_entry_val_undo(vmg_ self, entry, new_val);
    }
    else
    {
        /* add a new entry with the given key */
        add_entry_undo(vmg_ self, index_val, new_val);
    }

    /* we can be modified, hence the new container is the original one */
    new_container->set_obj(self);

    /* handled */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/*
 *   Property evaluator - determine if the given key is in the table 
 */
int CVmObjLookupTable::getp_key_present(VMG_ vm_obj_id_t self,
                                        vm_val_t *retval, uint *argc)
{
    vm_lookup_val *entry;
    vm_val_t key;
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the key */
    G_stk->pop(&key);
    
    /* find the entry for this key */
    entry = find_entry(vmg_ &key, 0, 0);

    /* return true if we found it, nil if not */
    retval->set_logical(entry != 0);

    /* handled */
    return TRUE;
}

/*
 *   Property evaluator - remove an entry given a key 
 */
int CVmObjLookupTable::getp_remove_entry(VMG_ vm_obj_id_t self,
                                         vm_val_t *retval, uint *argc)
{
    vm_lookup_val *entry;
    vm_val_t key;
    uint hashval;
    vm_lookup_val *prv_entry;
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the key */
    G_stk->pop(&key);

    /* find the entry for this key */
    entry = find_entry(vmg_ &key, &hashval, &prv_entry);

    /* if we found the entry, delete it */
    if (entry != 0)
    {
        /* the return value is the value stored at this key */
        *retval = entry->val;

        /* generate undo for the change */
        add_undo_rec(vmg_ self, LOOKUPTAB_UNDO_DEL, &key, retval);

        /* unlink the entry */
        unlink_entry(vmg_ entry, hashval, prv_entry);
    }
    else
    {
        /* not found - the return value is nil */
        retval->set_nil();
    }

    /* handled */
    return TRUE;
}

/*
 *   Property evaluator - apply a callback to each entry in the table 
 */
int CVmObjLookupTable::getp_apply_all(VMG_ vm_obj_id_t self,
                                      vm_val_t *retval, uint *argc)
{
    vm_val_t *fn;
    vm_lookup_val *entry;
    static CVmNativeCodeDesc desc(1);
    uint i;
    vm_rcdesc rc(vmg_ "LookupTable.applyAll", self, 3, G_stk->get(0), argc);
    
    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* the function to invoke is the top argument */
    fn = G_stk->get(0);

    /* push a self-reference for gc protection */
    G_stk->push()->set_obj(self);

    /* iterate over each entry */
    for (i = get_entry_count(), entry = get_ext()->idx_to_val(0) ;
         i != 0 ; ++entry, --i)
    {
        /* 
         *   if the entry is in use (it is if the key is non-empty), invoke
         *   the callback on it 
         */
        if (entry->key.typ != VM_EMPTY)
        {
            /* push the arguments - (key, val) - push in reverse order */
            G_stk->push(&entry->val);
            G_stk->push(&entry->key);
            
            /* invoke the callback */
            G_interpreter->call_func_ptr(vmg_ fn, 2, &rc, 0);
            
            /* replace this element with the result */
            set_entry_val_undo(vmg_ self, entry, G_interpreter->get_r0());
        }
    }

    /* discard the arguments and gc protection */
    G_stk->discard(2);

    /* no return value */
    retval->set_nil();

    /* handled */
    return TRUE;
}

/*
 *   Property evaluator - invoke a callback for each entry in the table 
 */
int CVmObjLookupTable::getp_for_each(VMG_ vm_obj_id_t self,
                                     vm_val_t *retval, uint *argc)
{
    /* call the general processor */
    vm_rcdesc rc(vmg_ "LookupTable.forEach", self, 4, G_stk->get(0), argc);
    return for_each_gen(vmg_ self, retval, argc, FALSE, &rc);
}

/*
 *   Property evaluator - invoke a callback for each entry in the table 
 */
int CVmObjLookupTable::getp_for_each_assoc(VMG_ vm_obj_id_t self,
                                           vm_val_t *retval, uint *argc)
{
    /* call the general processor */
    vm_rcdesc rc(vmg_ "List.forEachAssoc", self, 7, G_stk->get(0), argc);
    return for_each_gen(vmg_ self, retval, argc, TRUE, &rc);
}

/*
 *   general processor for forEach/forEachAssoc 
 */
int CVmObjLookupTable::for_each_gen(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *argc,
                                    int pass_key_to_cb, vm_rcdesc *rc)
{
    vm_val_t *fn;
    vm_lookup_val *entry;
    static CVmNativeCodeDesc desc(1);
    uint i;

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* the function to invoke is the top argument */
    fn = G_stk->get(0);

    /* push a self-reference for gc protection */
    G_stk->push()->set_obj(self);

    /* iterate over each bucket */
    for (i = get_entry_count(), entry = get_ext()->idx_to_val(0) ;
         i != 0 ; --i, ++entry)
    {
        /* 
         *   if it's in use (i.e., if the key is non-empty), invoke the
         *   callback for the entry 
         */
        if (entry->key.typ != VM_EMPTY)
        {
            /* push the current value argument */
            G_stk->push(&entry->val);

            /* push the key if desired */
            if (pass_key_to_cb)
                G_stk->push(&entry->key);
            
            /* invoke the callback */
            G_interpreter->call_func_ptr(vmg_ fn, pass_key_to_cb ? 2 : 1,
                                         rc, 0);
        }
    }

    /* discard the arguments and gc protection */
    G_stk->discard(2);

    /* no return value */
    retval->set_nil();

    /* handled */
    return TRUE;
}

/* 
 *   iterate over the table's contents through a callback 
 */
void CVmObjLookupTable::for_each(
    VMG_ void (*cb)(VMG_ const vm_val_t *key,
                    const vm_val_t *val, void *ctx), void *ctx)
{
    vm_lookup_val *entry;
    uint i;

    /* iterate over each bucket */
    for (i = get_entry_count(), entry = get_ext()->idx_to_val(0) ;
         i != 0 ; --i, ++entry)
    {
        /* 
         *   if it's in use (i.e., if the key is non-empty), invoke the
         *   callback for the entry 
         */
        if (entry->key.typ != VM_EMPTY)
            (*cb)(vmg_ &entry->key, &entry->val, ctx);
    }
}

/*
 *   Property evaluator - get the number of buckets
 */
int CVmObjLookupTable::getp_count_buckets(VMG_ vm_obj_id_t self,
                                          vm_val_t *retval, uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* return the number of buckets */
    retval->set_int(get_bucket_count());

    /* handled */
    return TRUE;
}

/*
 *   Property evaluator - get the number of entries in use
 */
int CVmObjLookupTable::getp_count_entries(VMG_ vm_obj_id_t self,
                                          vm_val_t *retval, uint *argc)
{
    static CVmNativeCodeDesc desc(0);
    uint i;
    int cnt;
    vm_lookup_val *entry;
    
    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* run through the table and count in-use entries */
    for (cnt = 0, i = get_entry_count(), entry = get_ext()->idx_to_val(0) ;
         i != 0 ; --i, ++entry)
    {
        /* if the entry is not marked as free, count it as used */
        if (entry->key.typ != VM_EMPTY)
            ++cnt;
    }

    /* return the number of in-use entries we counted */
    retval->set_int(cnt);

    /* handled */
    return TRUE;
}


/*
 *   Property evaluator - make a list of all of the keys in the table 
 */
int CVmObjLookupTable::getp_keys_to_list(VMG_ vm_obj_id_t self,
                                         vm_val_t *retval, uint *argc)
{
    /* use the general list maker, listing only the keys */
    return getp_make_list(vmg_ self, retval, argc, TRUE);
}

/*
 *   Property evaluator - make a list of all of the values in the table 
 */
int CVmObjLookupTable::getp_vals_to_list(VMG_ vm_obj_id_t self,
                                         vm_val_t *retval, uint *argc)
{
    /* use the general list maker, listing only the values */
    return getp_make_list(vmg_ self, retval, argc, FALSE);
}

/*
 *   General handler for keys_to_list and vals_to_list 
 */
int CVmObjLookupTable::getp_make_list(VMG_ vm_obj_id_t self,
                                      vm_val_t *retval, uint *argc,
                                      int store_keys)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* push self while we're working, for gc protection */
    G_stk->push()->set_obj(self);

    /* make the list */
    make_list(vmg_ retval, store_keys, 0);
    
    /* discard our gc protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   Low-level list maker.  Creates a list of all of the keys or values in
 *   the table. 
 */
void CVmObjLookupTable::make_list(VMG_ vm_val_t *retval, int store_keys,
                                  int (*filter)(VMG_ const vm_val_t *,
                                                const vm_val_t *))
{
    vm_lookup_val *entry;
    uint i;
    int cnt;
    CVmObjList *lst;

    /* run through the table and count in-use entries */
    for (cnt = 0, i = get_entry_count(), entry = get_ext()->idx_to_val(0) ;
         i != 0 ; ++entry, --i)
    {
        /* skip empties */
        if (entry->key.typ == VM_EMPTY)
            continue;

        /* skip values that the filter function rejects */
        if (filter != 0 && !filter(vmg_ &entry->key, &entry->val))
            continue;

        /* count it */
        ++cnt;
    }

    /* allocate a list to store the results */
    retval->set_obj(CVmObjList::create(vmg_ FALSE, cnt));

    /* get the list object */
    lst = (CVmObjList *)vm_objp(vmg_ retval->val.obj);
    lst->cons_clear();

    /* populate the list */
    for (cnt = 0, i = get_entry_count(), entry = get_ext()->idx_to_val(0) ;
         i != 0 ; ++entry, --i)
    {
        /* if the entry is not marked as free, count it as used */
        if (entry->key.typ != VM_EMPTY)
        {
            /* skip values that the filter function rejects */
            if (filter != 0 && !filter(vmg_ &entry->key, &entry->val))
                continue;

            /* store the key or value, as appropriate */
            if (store_keys)
            {
                /* store the key */
                lst->cons_set_element(cnt, &entry->key);
            }
            else
            {
                /* store the value */
                lst->cons_set_element(cnt, &entry->val);
            }

            /* update the destination index */
            ++cnt;
        }
    }
}


/*
 *   Property evaluator: get the default value 
 */
int CVmObjLookupTable::getp_get_def_val(VMG_ vm_obj_id_t self,
                                        vm_val_t *retval, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* return the default value */
    *retval = get_ext()->default_value;
    return TRUE;
}

/* get the default value - internal API version */
void CVmObjLookupTable::get_default_val(vm_val_t *val)
{
    *val = get_ext()->default_value;
}

/*
 *   Property evaluator: set the default value 
 */
int CVmObjLookupTable::getp_set_def_val(VMG_ vm_obj_id_t self,
                                        vm_val_t *retval, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* save undo */
    add_undo_rec(vmg_ self, LOOKUPTAB_UNDO_DEFVAL,
                 0, &get_ext()->default_value);

    /* set the default value to the argument value */
    G_stk->pop(&get_ext()->default_value);

    /* return self */
    retval->set_obj(self);
    return TRUE;
}

/*
 *   Property evaluator: get the nth key
 */
int CVmObjLookupTable::getp_nthKey(VMG_ vm_obj_id_t self,
                                   vm_val_t *retval, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the nth key value */
    get_nth_ele(vmg_ retval, 0, self, CVmBif::pop_long_val(vmg0_));

    /* handled */
    return TRUE;
}

/*
 *   Property evaluator: get the nth value
 */
int CVmObjLookupTable::getp_nthVal(VMG_ vm_obj_id_t self,
                                   vm_val_t *retval, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the nth value */
    get_nth_ele(vmg_ 0, retval, self, CVmBif::pop_long_val(vmg0_));

    /* handled */
    return TRUE;
}

/*
 *   Get the nth element's key and/or value 
 */
void CVmObjLookupTable::get_nth_ele(VMG_ vm_val_t *key, vm_val_t *val,
                                    vm_obj_id_t self, long idx)
{
    uint i;
    vm_lookup_val *entry;

    /* 
     *   if the index is zero, and we're asking for the value, return the
     *   default value 
     */
    if (idx == 0)
    {
        if (key != 0)
            key->set_nil();
        if (val != 0)
            *val = get_ext()->default_value;
        return;
    }
    
    /* iterate over our buckets */
    for (i = get_entry_count(), entry = get_ext()->idx_to_val(0) ;
         i != 0 ; --i, ++entry)
    {
        /* if it's in use (i.e., the key isn't empty), count it */
        if (entry->key.typ != VM_EMPTY)
        {
            /* count it */
            --idx;

            /* if this is the one we're looking for, return it */
            if (idx == 0)
            {
                /* fill in the key and/or value elements */
                if (key != 0)
                    *key = entry->key;
                if (val != 0)
                    *val = entry->val;

                /* done */
                return;
            }
        }
    }

    /* we didn't find it - the index is out of range */
    err_throw(VMERR_INDEX_OUT_OF_RANGE);
}

/* ------------------------------------------------------------------------ */
/*
 *   WeakRefLookupTable 
 */


/*  
 *   statics 
 */

/* metaclass registration object */
static CVmMetaclassWeakRefLookupTable weak_metaclass_reg_obj;
CVmMetaclass *CVmObjWeakRefLookupTable::metaclass_reg_ =
    &weak_metaclass_reg_obj;

/*
 *   create
 */
vm_obj_id_t CVmObjWeakRefLookupTable::
   create(VMG_ int in_root_set, uint bucket_count, uint init_capacity)
{
    vm_obj_id_t id;

    /* allocate the object ID */
    id = vm_new_id(vmg_ in_root_set, TRUE, TRUE);

    /* create the object */
    new (vmg_ id) CVmObjWeakRefLookupTable(vmg_ bucket_count, init_capacity);

    /* return the new ID */
    return id;
}

/* 
 *   create dynamically using stack arguments 
 */
vm_obj_id_t CVmObjWeakRefLookupTable::create_from_stack(
    VMG_ const uchar **pc_ptr, uint argc)
{
    vm_obj_id_t id;
    size_t bucket_count;
    size_t init_capacity;
    vm_val_t src_obj;

    /* parse the arguments */
    get_constructor_args(vmg_ argc, &bucket_count, &init_capacity, &src_obj);

    /* 
     *   allocate the object ID - this type of construction never creates a
     *   root object 
     */
    id = vm_new_id(vmg_ FALSE, TRUE, TRUE);

    /* create the object */
    new (vmg_ id) CVmObjWeakRefLookupTable(vmg_ bucket_count, init_capacity);

    /* populate it with the source list if desired */
    ((CVmObjLookupTable *)vm_objp(vmg_ id))->
        populate_from_list(vmg_ &src_obj);

    /* discard the source object gc protection */
    G_stk->discard();

    /* return the new ID */
    return id;
}


/*
 *   Mark undo references.  Keys are strongly referenced, so mark the key in
 *   the record, if present.  
 */
void CVmObjWeakRefLookupTable::
   mark_undo_ref(VMG_ struct CVmUndoRecord *undo_rec)
{
    lookuptab_undo_rec *rec;

    /* get our private record from the standard undo record */
    rec = (lookuptab_undo_rec *)undo_rec->id.ptrval;

    /* if the key in the record is an object, mark it referenced */
    if (rec->key.typ == VM_OBJ)
        G_obj_table->mark_all_refs(rec->key.val.obj, VMOBJ_REACHABLE);
}

/*
 *   Mark references.  Keys (but not values) are strongly referenced.  
 */
void CVmObjWeakRefLookupTable::mark_refs(VMG_ uint state)
{
    vm_lookup_val **bp;
    uint i;

    /* note that the default value is weakly referenced, so don't mark it */

    /* run through my buckets */
    for (i = get_ext()->bucket_cnt, bp = get_ext()->buckets ;
         i != 0 ; --i, ++bp)
    {
        const vm_lookup_val *entry;

        /* run through all entries attached to this bucket */
        for (entry = *bp ; entry != 0 ; entry = entry->nxt)
        {
            /* if the key is an object, mark it as referenced */
            if (entry->key.typ == VM_OBJ)
                G_obj_table->mark_all_refs(entry->key.val.obj, state);

            /* 
             *   note that we only reference our value weakly, so do not
             *   mark it here 
             */
        }
    }
}

/*
 *   Remove stale weak references from an undo record.  Value entries are
 *   weakly referenced.  
 */
void CVmObjWeakRefLookupTable::
   remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *undo_rec)
{
    int forget_record;

    /* presume we won't want to forget the record */
    forget_record = FALSE;

    /* 
     *   if the old value in the undo record refers to an object, forget
     *   about it 
     */
    if (G_obj_table->is_obj_deletable(&undo_rec->oldval))
    {
        /* 
         *   the object is being deleted - since we only weakly reference
         *   our objects, we must forget about this object now; this also
         *   means we can forget about the record entirely, since there is
         *   no need to apply it in the future now that it doesn't do
         *   anything 
         */
        undo_rec->oldval.set_nil();
        forget_record = TRUE;
    }

    /* delete our extended record if we're forgetting the record */
    if (undo_rec->id.ptrval != 0 && forget_record)
    {
        lookuptab_undo_rec *rec;

        /* get our private record from the standard undo record */
        rec = (lookuptab_undo_rec *)undo_rec->id.ptrval;

        /* we don't care about the key value in this record any more */
        rec->key.set_nil();

        /* mark the record as forgotten */
        rec->action = LOOKUPTAB_UNDO_NULL;
    }
}

/* 
 *   Remove stale weak references.  Our values are weak references, so
 *   remove any entries that have values that are about to be deleted.  
 */
void CVmObjWeakRefLookupTable::remove_stale_weak_refs(VMG0_)
{
    vm_lookup_val **bucket;
    uint i;
    uint hashval;

    /* remove the default value if it's gone stale */
    if (G_obj_table->is_obj_deletable(&get_ext()->default_value))
        get_ext()->default_value.set_nil();

    /* run through each bucket */
    for (hashval = 0, bucket = get_ext()->buckets, i = get_bucket_count() ;
         i != 0 ; ++bucket, --i, ++hashval)
    {
        vm_lookup_val *entry;
        vm_lookup_val *prv;
        vm_lookup_val *nxt;

        /* run through all entries attached to this bucket */
        for (prv = 0, entry = *bucket ; entry != 0 ; entry = nxt)
        {
            /* remember the next entry, in case we delete this one */
            nxt = entry->nxt;

            /* 
             *   if the value has an object reference that has become stale,
             *   delete the entry 
             */
            if (G_obj_table->is_obj_deletable(&entry->val))
            {
                /* it's deletable, so delete the entire record */
                unlink_entry(vmg_ entry, hashval, prv);

                /* 
                 *   note that we do NOT advance the prv pointer - we've
                 *   deleted this entry, so the one preceding it is now the
                 *   previous for the one following this one 
                 */
            }
            else
            {
                /* we're keeping this entry - make it the new previous */
                prv = entry;
            }
        }
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   LookupTable Iterator metaclass 
 */

/*
 *   statics 
 */

/* metaclass registration object */
static CVmMetaclassIterLookupTable iter_metaclass_reg_obj;
CVmMetaclass *CVmObjIterLookupTable::metaclass_reg_ = &iter_metaclass_reg_obj;


/* create a list with no initial contents */
vm_obj_id_t CVmObjIterLookupTable::create_for_coll(VMG_ const vm_val_t *coll)
{
    vm_obj_id_t id;

    /* push the collection object reference for gc protection */
    G_stk->push(coll);

    /* create a non-root-set object */
    id = vm_new_id(vmg_ FALSE, TRUE, TRUE);

    /* instantiate the iterator */
    new (vmg_ id) CVmObjIterLookupTable(vmg_ coll);

    /* done with the gc protection */
    G_stk->discard();

    /* return the new object's ID */
    return id;
}

/*
 *   constructor 
 */
CVmObjIterLookupTable::CVmObjIterLookupTable(VMG_ const vm_val_t *coll)
{
    /* allocate space for our extension data */
    ext_ = (char *)G_mem->get_var_heap()
           ->alloc_mem(VMOBJITERLOOKUPTABLE_EXT_SIZE, this);

    /* save the collection value */
    vmb_put_dh(ext_, coll);

    /* clear the flags */
    set_flags(0);

    /* 
     *   set up at index zero - we're not at a valid entry until initialized
     *   with getNext 
     */
    set_entry_index(0);
}

/*
 *   Find the first valid entry, starting at the given entry index.  If the
 *   given index refers to a valid entry, we return it.  If there is no
 *   valid next entry, we return zero.  
 */
uint CVmObjIterLookupTable::find_first_valid_entry(VMG_ uint entry) const
{
    CVmObjLookupTable *ltab;
    vm_val_t coll;

    /* get my lookup table object */
    get_coll_val(&coll);
    ltab = (CVmObjLookupTable *)vm_objp(vmg_ coll.val.obj);

    /* if we're already past the last item, we have nothing to find */
    if (entry > ltab->get_entry_count())
        return 0;

    /* 
     *   scan entries until we find one with a non-empty key (an empty key
     *   value indicates that the entry is free) 
     */
    for ( ; entry <= ltab->get_entry_count() ; ++entry)
    {
        vm_lookup_val *entryp;

        /* translate the 1-based index to an entry pointer */
        entryp = ltab->get_ext()->idx_to_val(entry - 1);

        /* if it's non-empty, this is the one we want */
        if (entryp->key.typ != VM_EMPTY)
            return entry;
    }

    /* we didn't find a valid entry */
    return 0;
}

/*
 *   notify of deletion 
 */
void CVmObjIterLookupTable::notify_delete(VMG_ int)
{
    /* free our extension */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);
}

/*
 *   property evaluator - get the next item 
 */
int CVmObjIterLookupTable::getp_get_next(VMG_ vm_obj_id_t self,
                                         vm_val_t *retval, uint *argc)
{
    uint entry;
    static CVmNativeCodeDesc desc(0);
    CVmObjLookupTable *ltab;
    vm_val_t coll;

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get my collection object */
    get_coll_val(&coll);
    ltab = (CVmObjLookupTable *)vm_objp(vmg_ coll.val.obj);

    /* advance to the next valid index after the current one */
    entry = find_first_valid_entry(vmg_ get_entry_index() + 1);

    /* 
     *   if we're past the end of the table, set the index to zero to
     *   indicate that we're done 
     */
    if (entry == 0)
        err_throw(VMERR_OUT_OF_RANGE);

    /* retrieve the current entry */
    *retval = ltab->get_ext()->idx_to_val(entry - 1)->val;

    /* update our internal state, saving undo */
    set_entry_index_undo(vmg_ self, entry);

    /* handled */
    return TRUE;
}

/* 
 *   property evaluator - get current value
 */
int CVmObjIterLookupTable::getp_get_cur_val(VMG_ vm_obj_id_t self,
                                            vm_val_t *retval, uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the entry's value */
    get_cur_entry(vmg_ retval, 0);

    /* handled */
    return TRUE;
}

/* 
 *   property evaluator - get current key
 */
int CVmObjIterLookupTable::getp_get_cur_key(VMG_ vm_obj_id_t self,
                                            vm_val_t *retval, uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the entry's key */
    get_cur_entry(vmg_ 0, retval);

    /* handled */
    return TRUE;
}

/*
 *   Get the current entry index, value, and key for the current entry.  Any
 *   of the output pointers can be null if the corresponding value is not
 *   needed by the caller.  
 */
void CVmObjIterLookupTable::get_cur_entry(VMG_ vm_val_t *valp,
                                          vm_val_t *keyp) const
{
    uint entry;
    vm_val_t coll;
    CVmObjLookupTable *ltab;
    vm_lookup_val *entryp;

    /* get my collection object */
    get_coll_val(&coll);
    ltab = (CVmObjLookupTable *)vm_objp(vmg_ coll.val.obj);

    /* make sure the index is in range */
    entry = get_entry_index();
    if (entry < 1 || entry > ltab->get_entry_count())
        err_throw(VMERR_OUT_OF_RANGE);

    /* translate our 1-based index to an entry pointer */
    entryp = ltab->get_ext()->idx_to_val(entry - 1);

    /* if the current entry has been deleted, we have no valid current item */
    if (entryp->key.typ == VM_EMPTY)
        err_throw(VMERR_OUT_OF_RANGE);

    /* retrieve this entry's value and key for the caller, as desired */
    if (valp != 0)
        *valp = entryp->val;
    if (keyp != 0)
        *keyp = entryp->key;
}

/* 
 *   property evaluator - is next value available?  
 */
int CVmObjIterLookupTable::getp_is_next_avail(VMG_ vm_obj_id_t self,
                                              vm_val_t *retval, uint *argc)
{
    uint entry;
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* find the index of the next valid entry */
    entry = find_first_valid_entry(vmg_ get_entry_index() + 1);

    /* return true if we have a valid index, false if not */
    retval->set_logical(entry != 0);

    /* handled */
    return TRUE;
}

/* 
 *   property evaluator - reset to first item 
 */
int CVmObjIterLookupTable::getp_reset_iter(VMG_ vm_obj_id_t self,
                                           vm_val_t *retval, uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* set our entry index back to zero, saving undo */
    set_entry_index_undo(vmg_ self, 0);

    /* no return value */
    retval->set_nil();

    /* handled */
    return TRUE;
}

/*
 *   Set the next index value, saving undo if necessary 
 */
void CVmObjIterLookupTable::set_entry_index_undo(
    VMG_ vm_obj_id_t self, uint entry_idx)
                                         
{
    /* save undo if necessary */
    if (G_undo != 0 && !(get_flags() & VMOBJITERLOOKUPTABLE_UNDO))
    {
        vm_val_t val;

        /* 
         *   Add the undo record.  Store the entry index as the key (it's
         *   not really a key, but it'll do as a place to store the data).
         *   We don't need anything in the payload, so store a dummy nil
         *   value.  
         */
        val.set_nil();
        G_undo->add_new_record_int_key(vmg_ self, get_entry_index(), &val);

        /* 
         *   set the undo bit so we don't save redundant undo for this
         *   savepoint 
         */
        set_flags(get_flags() | VMOBJITERLOOKUPTABLE_UNDO);
    }

    /* set the index */
    set_entry_index(entry_idx);
}

/*
 *   apply undo 
 */
void CVmObjIterLookupTable::apply_undo(VMG_ CVmUndoRecord *rec)
{
    /* 
     *   the integer key in the undo record is my saved index value (and is
     *   the only thing in our state that can ever change) 
     */
    set_entry_index((uint)rec->id.intval);
}

/*
 *   mark references 
 */
void CVmObjIterLookupTable::mark_refs(VMG_ uint state)
{
    vm_val_t coll;

    /* my collection is always an object - mark it as referenced */
    get_coll_val(&coll);
    G_obj_table->mark_all_refs(coll.val.obj, state);
}

/*
 *   load from an image file 
 */
void CVmObjIterLookupTable::load_from_image(VMG_ vm_obj_id_t self,
                                            const char *ptr, size_t siz)
{
    /* if we already have memory allocated, free it */
    if (ext_ != 0)
    {
        G_mem->get_var_heap()->free_mem(ext_);
        ext_ = 0;
    }

    /* 
     *   Allocate a new extension.  Make sure it's at least as large as the
     *   current standard extension size.  
     */
    ext_ = (char *)G_mem->get_var_heap()
           ->alloc_mem(siz < VMOBJITERLOOKUPTABLE_EXT_SIZE
                       ? VMOBJITERLOOKUPTABLE_EXT_SIZE : siz, this);

    /* copy the image data to our extension */
    memcpy(ext_, ptr, siz);

    /* 
     *   save our image data pointer in the object table, so that we can
     *   access it (without storing it ourselves) during a reload 
     */
    G_obj_table->save_image_pointer(self, ptr, siz);

    /* clear the undo flag */
    set_flags(get_flags() & ~VMOBJITERLOOKUPTABLE_UNDO);
}

/*
 *   re-load from an image file 
 */
void CVmObjIterLookupTable::reload_from_image(VMG_ vm_obj_id_t,
                                              const char *ptr, size_t siz)
{
    /* re-copy the image data */
    memcpy(ext_, ptr, siz);

    /* clear the undo flag */
    set_flags(get_flags() & ~VMOBJITERLOOKUPTABLE_UNDO);
}

/*
 *   save 
 */
void CVmObjIterLookupTable::save_to_file(VMG_ CVmFile *fp)
{
    /* write my extension */
    fp->write_bytes(ext_, VMOBJITERLOOKUPTABLE_EXT_SIZE);
}

/*
 *   restore 
 */
void CVmObjIterLookupTable::restore_from_file(VMG_ vm_obj_id_t,
                                              CVmFile *fp,
                                              CVmObjFixup *fixups)
{
    /* free any existing extension */
    if (ext_ != 0)
    {
        G_mem->get_var_heap()->free_mem(ext_);
        ext_ = 0;
    }

    /* allocate a new extension */
    ext_ = (char *)G_mem->get_var_heap()
           ->alloc_mem(VMOBJITERLOOKUPTABLE_EXT_SIZE, this);

    /* read my extension */
    fp->read_bytes(ext_, VMOBJITERLOOKUPTABLE_EXT_SIZE);

    /* fix up my collection object reference */
    fixups->fix_dh(vmg_ ext_);
}

