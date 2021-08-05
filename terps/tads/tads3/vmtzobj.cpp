#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2012 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmtzobj.cpp - CVmObjTimeZone object
Function
  
Notes
  
Modified
  02/06/12 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "t3std.h"
#include "vmtzobj.h"
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
#include "vmlst.h"
#include "vmtz.h"
#include "vmdate.h"
#include "utf8.h"


/* ------------------------------------------------------------------------ */
/*
 *   Allocate an extension structure 
 */
vm_tzobj_ext *vm_tzobj_ext::alloc_ext(VMG_ CVmObjTimeZone *self,
                                      CVmTimeZone *tz)
{
    /* calculate how much space we need */
    size_t siz = sizeof(vm_tzobj_ext);

    /* allocate the memory */
    vm_tzobj_ext *ext = (vm_tzobj_ext *)G_mem->get_var_heap()->alloc_mem(
        siz, self);

    /* save our TimeZone object */
    ext->tz = tz;

    /* return the new extension */
    return ext;
}

/* ------------------------------------------------------------------------ */
/*
 *   CVmObjTimeZone object statics 
 */

/* metaclass registration object */
static CVmMetaclassTimeZone metaclass_reg_obj;
CVmMetaclass *CVmObjTimeZone::metaclass_reg_ = &metaclass_reg_obj;

/* function table */
int (CVmObjTimeZone::*CVmObjTimeZone::func_table_[])(
    VMG_ vm_obj_id_t self, vm_val_t *retval, uint *argc) =
{
    &CVmObjTimeZone::getp_undef,                                       /* 0 */
    &CVmObjTimeZone::getp_getNames,                                    /* 1 */
    &CVmObjTimeZone::getp_getHistory,                                  /* 2 */
    &CVmObjTimeZone::getp_getRules,                                    /* 3 */
    &CVmObjTimeZone::getp_getLocation                                  /* 4 */
};


/* ------------------------------------------------------------------------ */
/*
 *   construction
 */
CVmObjTimeZone::CVmObjTimeZone(VMG_ CVmTimeZone *tz)
{
    /* allocate our extension structure */
    ext_ = (char *)vm_tzobj_ext::alloc_ext(vmg_ this, tz);
}

/* ------------------------------------------------------------------------ */
/*
 *   create dynamically using stack arguments 
 */
vm_obj_id_t CVmObjTimeZone::create_from_stack(
    VMG_ const uchar **pc_ptr, uint argc)
{
    /* parse the constructor arguments */
    CVmTimeZone *tz = parse_ctor_args(vmg_ G_stk->get(0), argc);

    /* allocate the object ID */
    vm_obj_id_t id = vm_new_id(vmg_ FALSE, FALSE, FALSE);

    /* create the object */
    new (vmg_ id) CVmObjTimeZone(vmg_ tz);

    /* return the new ID */
    return id;
}

/*
 *   Parse stack arguments for a timezone constructor, returning a
 *   CVmTimeZone object if the arguments specify a valid timezone.  If not,
 *   throws an error - we never return null.  This doesn't create the garbage
 *   collected TimeZone object; it just parses the arguments and creates the
 *   underlying cache object.
 */
CVmTimeZone *CVmObjTimeZone::parse_ctor_args(
    VMG_ const vm_val_t *argp, int argc)
{
    /* check arguments */
    const char *str;
    CVmTimeZone *tz = 0;
    if (argc <= 0)
    {
        /* no arguments; create a local timezone object */
        tz = G_tzcache->get_local_zone(vmg0_);
    }
    else if (argc == 1 && (str = argp->get_as_string(vmg0_)) != 0)
    {
        /* get the string buffer and length */
        size_t len = vmb_get_len(str);
        str += VMB_LEN;

        /* parse the zone name */
        tz = G_tzcache->parse_zone(vmg_ str, len);
    }
    else if (argc == 1 && argp->is_numeric(vmg0_))
    {
        /* construct from a gmt offset */
        tz = G_tzcache->get_gmtofs_zone(vmg_ argp->num_to_int(vmg0_));
    }
    else if (argc > 1)
    {
        /* wrong number of arguments */
        err_throw(VMERR_WRONG_NUM_OF_ARGS);
    }
    else
    {
        /* wrong type */
        err_throw(VMERR_BAD_TYPE_BIF);
    }

    /* if we didn't find a zone, it's an error */
    if (tz == 0)
        err_throw(VMERR_BAD_VAL_BIF);

    /* return the zone */
    return tz;
}

/* ------------------------------------------------------------------------ */
/*
 *   Create from a given CVmTimeZone object.
 */
vm_obj_id_t CVmObjTimeZone::create(VMG_ CVmTimeZone *tz)
{
    /* allocate the object ID and create the object */
    vm_obj_id_t id = vm_new_id(vmg_ FALSE, FALSE, FALSE);
    new (vmg_ id) CVmObjTimeZone(vmg_ tz);

    /* return the new ID */
    return id;
}

/* ------------------------------------------------------------------------ */
/* 
 *   notify of deletion 
 */
void CVmObjTimeZone::notify_delete(VMG_ int /*in_root_set*/)
{
    /* free our additional data, if we have any */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);
}

/* ------------------------------------------------------------------------ */
/* 
 *   set a property 
 */
void CVmObjTimeZone::set_prop(VMG_ class CVmUndo *undo,
                              vm_obj_id_t self, vm_prop_id_t prop,
                              const vm_val_t *val)
{
    /* no settable properties - throw an error */
    err_throw(VMERR_INVALID_SETPROP);
}

/* ------------------------------------------------------------------------ */
/* 
 *   get a property 
 */
int CVmObjTimeZone::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *retval,
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

/* ------------------------------------------------------------------------ */
/* 
 *   load from an image file 
 */
void CVmObjTimeZone::load_from_image(VMG_ vm_obj_id_t self,
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
void CVmObjTimeZone::reload_from_image(VMG_ vm_obj_id_t self,
                                       const char *ptr, size_t siz)
{
    /* load our image data */
    load_image_data(vmg_ self, ptr, siz);
}

/*
 *   load or reload data from the image 
 */
void CVmObjTimeZone::load_image_data(
    VMG_ vm_obj_id_t self, const char *ptr, size_t siz)
{
    /* free our existing extension, if we have one */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);

    /* look up our timezone object */
    CVmTimeZone *tz = G_tzcache->parse_zone(vmg_ ptr+9, osrp1(ptr+8));

    /* if we didn't find it, create a dummy entry for it */
    if (tz == 0)
    {
        /* 
         *   Read the default GMT offset and daylight savings offset.  Note
         *   that the saved values are in milliseconds, and os_tzinfo_t uses
         *   seconds, so adjust accordingly. 
         */
        os_tzinfo_t desc;
        memset(&desc, 0, sizeof(desc));
        desc.std_ofs = osrp4s(ptr) / 1000;
        desc.dst_ofs = desc.std_ofs + osrp4s(ptr+4) / 1000;

        /* read the default abbreviation */
        const char *abbr = ptr + 8 + osrp1(ptr+8) + 1;
        lib_strcpy(desc.std_abbr, sizeof(desc.std_abbr), abbr+1, osrp1(abbr));
        lib_strcpy(desc.dst_abbr, sizeof(desc.dst_abbr), abbr+1, osrp1(abbr));

        /* create the dummy */
        tz = G_tzcache->create_missing_zone(vmg_ ptr+9, osrp1(ptr+8), &desc);
    }

    /* allocate the extension */
    vm_tzobj_ext *ext = vm_tzobj_ext::alloc_ext(vmg_ this, tz);
    ext_ = (char *)ext;
}


/* ------------------------------------------------------------------------ */
/* 
 *   save to a file 
 */
void CVmObjTimeZone::save_to_file(VMG_ class CVmFile *fp)
{
    /* get our timezone name */
    CVmTimeZone *tz = get_ext()->tz;
    size_t namelen;
    const char *name = tz->get_name(namelen);

    /* if this is the system's local zone, use ":local" as the name */
    if (G_tzcache->is_local_zone(tz))
        name = ":local", namelen = 6;

    /* query the zone data */
    vmtzquery q;
    tz->query(&q);

    /* write our GMT offset, DST offset, abbreviation, and timezone name */
    fp->write_int4(q.gmtofs);
    fp->write_int4(q.save);
    fp->write_str_byte_prefix(q.abbr);
    fp->write_str_byte_prefix(name, namelen);
}

/* ------------------------------------------------------------------------ */
/* 
 *   restore from a file 
 */
void CVmObjTimeZone::restore_from_file(VMG_ vm_obj_id_t self,
                                       CVmFile *fp, CVmObjFixup *fixups)
{
    /* free our existing extension, if we have one */
    if (ext_ != 0)
        G_mem->get_var_heap()->free_mem(ext_);

    /* 
     *   Read the GMT and DST offsets (in case we need a synthetic entry).
     *   Note that the saved values are in milliseconds, but os_tzinfo_t
     *   works in seconds, so adjust accordingly. 
     */
    os_tzinfo_t desc;
    memset(&desc, 0, sizeof(desc));
    desc.std_ofs = fp->read_int4() / 1000;
    desc.dst_ofs = desc.std_ofs + fp->read_int4() / 1000;

    /* read the abbreviation */
    fp->read_str_byte_prefix(desc.std_abbr, sizeof(desc.std_abbr));
    strcpy(desc.dst_abbr, desc.std_abbr);

    /* read the length and name */
    char name[256];
    size_t len = fp->read_str_byte_prefix(name, sizeof(name));

    /* look up our timezone object */
    CVmTimeZone *tz = G_tzcache->parse_zone(vmg_ name, len);

    /* if we didn't find it, create a dummy entry for it */
    if (tz == 0)
        tz = G_tzcache->create_missing_zone(vmg_ name, len, &desc);

    /* allocate the extension structure */
    vm_tzobj_ext *ext = vm_tzobj_ext::alloc_ext(vmg_ this, tz);
    ext_ = (char *)ext;
}

/* ------------------------------------------------------------------------ */
/*
 *   cast to a string - returns the time zone name
 */
const char *CVmObjTimeZone::cast_to_string(
    VMG_ vm_obj_id_t self, vm_val_t *newstr) const
{
    /* get the time zone name */
    size_t len;
    const char *name = get_tz()->get_name(len);

    /* create the return string */
    newstr->set_obj(CVmObjString::create(vmg_ FALSE, name, len));
    return newstr->get_as_string(vmg0_);
}

/* ------------------------------------------------------------------------ */
/*
 *   getNames method
 */
int CVmObjTimeZone::getp_getNames(VMG_ vm_obj_id_t self,
                                  vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* create the name list */
    retval->set_obj(get_tz()->get_name_list(vmg0_));

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   getHistory method
 */
int CVmObjTimeZone::getp_getHistory(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0, 1);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* check for a Date argument */
    if (argc >= 1 && G_stk->get(0)->typ != VM_NIL)
    {
        /* there's a date - retrieve it */
        CVmObjDate *date = vm_val_cast(CVmObjDate, G_stk->get(0));
        if (date == 0)
            err_throw(VMERR_BAD_TYPE_BIF);

        /* get the history for the given date only */
        retval->set_obj(get_tz()->get_history_item(
            vmg_ date->get_dayno(), date->get_daytime()));
    }
    else
    {
        /* no date argument - get the full history */
        retval->set_obj(get_tz()->get_history_list(vmg0_));
    }

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   getRules method
 */
int CVmObjTimeZone::getp_getRules(VMG_ vm_obj_id_t self,
                                  vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get the rule list */
    retval->set_obj(get_tz()->get_rule_list(vmg0_));

    /* discard arguments */
    G_stk->discard(argc);

    /* handled */
    return TRUE;
}


/* ------------------------------------------------------------------------ */
/*
 *   getLocation method
 */
int CVmObjTimeZone::getp_getLocation(VMG_ vm_obj_id_t self,
                                     vm_val_t *retval, uint *oargc)
{
    /* check arguments */
    uint argc = (oargc != 0 ? *oargc : 0);
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, oargc, &desc))
        return TRUE;

    /* get our underlying CVmTimeZone object */
    CVmTimeZone *tz = get_ext()->tz;

    /* create the return list - [country, lat, long, desc] */
    retval->set_obj(CVmObjList::create(vmg_ FALSE, 4));
    CVmObjList *lst = (CVmObjList *)vm_objp(vmg_ retval->val.obj);
    G_stk->push(retval);
    lst->cons_clear();

    /* set the country string */
    vm_val_t ele;
    const char *str = tz->country();
    ele.set_obj(CVmObjString::create(vmg_ FALSE, str, strlen(str)));
    lst->cons_set_element(0, &ele);

    /* get the lat/long string */
    str = tz->coords();

    /* find the +/- that separates the lat/long portions */
    const char *p = str;
    if (*p == '+' || *p == '-')
        ++p;
    for ( ; *p != '\0' && *p != '-' && *p != '+' ; ++p) ;

    /* set the two substrings */
    ele.set_obj(CVmObjString::create(vmg_ FALSE, str, p - str));
    lst->cons_set_element(1, &ele);
    ele.set_obj(CVmObjString::create(vmg_ FALSE, p, strlen(p)));
    lst->cons_set_element(2, &ele);

    /* set the description string */
    str = tz->desc();
    if (str != 0 && str[0] != '\0')
        ele.set_obj(CVmObjString::create(vmg_ FALSE, str, strlen(str)));
    else
        ele.set_nil();
    lst->cons_set_element(3, &ele);

    /* discard arguments and gc protection */
    G_stk->discard(argc + 1);

    /* handled */
    return TRUE;
}
