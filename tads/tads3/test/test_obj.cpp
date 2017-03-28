#ifdef RCSID
static char RCSid[] =
"$Header: d:/cvsroot/tads/tads3/test/TEST_OBJ.CPP,v 1.3 1999/07/11 00:47:03 MJRoberts Exp $";
#endif

/* 
 *   Copyright (c) 1998, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  test_obj.cpp - test the object subsystem
Function
  
Notes
  
Modified
  11/06/98 MJRoberts  - Creation
*/


#include <stdio.h>
#include <stdlib.h>

#include "t3std.h"
#include "vmglob.h"
#include "vmtype.h"
#include "vmobj.h"
#include "vmlst.h"
#include "vmstr.h"
#include "vmtobj.h"
#include "vmundo.h"
#include "vmstack.h"
#include "vmpool.h"
#include "vmimage.h"
#include "vmrun.h"
#include "vminit.h"
#include "vmhostsi.h"
#include "vmmaincn.h"
#include "t3test.h"


const vm_prop_id_t PROP_A = 100;
const vm_prop_id_t PROP_B = 101;
const vm_prop_id_t PROP_C = 200;
const vm_prop_id_t PROP_D = 201;
const vm_prop_id_t PROP_E = 204;

class CImageFile: public CVmPoolBackingStore
{
    /* 
     *   CVmPoolBackingStore implementation 
     */
    
    size_t vmpbs_get_page_count() { return 1; }
    size_t vmpbs_get_common_page_size() { return 4096; }
    size_t vmpbs_get_page_size(pool_ofs_t, size_t) { return 4096; }
    void vmpbs_load_page(pool_ofs_t, size_t, size_t, char *) { }
    const char *vmpbs_alloc_and_load_page(pool_ofs_t ofs, size_t page_size,
                                          size_t load_size)
    {
        char *mem = (char *)t3malloc(load_size);
        vmpbs_load_page(ofs, page_size, load_size, mem);
        return mem;
    }
    void vmpbs_free_page(const char *mem, pool_ofs_t, size_t)
    {
        t3free((char *)mem);
    }
};

int main(int argc, char **argv)
{
    CImageFile *imagefp;
    vm_val_t val;
    vm_val_t *stkval;
    vm_globals *vmg__;
    CVmHostIfc *hostifc;
    CVmMainClientConsole clientifc;

    /* initialize for testing */
    test_init();

    /* initialize the VM */
    hostifc = new CVmHostIfcStdio(argv[0]);
    vm_init_options iniopts(hostifc, &clientifc, "us-ascii", "us_ascii");
    vm_initialize(&vmg__, &iniopts);

    /* create a fake host file object */
    imagefp = new CImageFile();

    /* create the constant pool */
    G_const_pool->attach_backing_store(imagefp);

    /* create a couple of objects and push them on the stack */
    val.set_obj(CVmObjTads::create(vmg_ FALSE, 0, 4));
    G_stk->push(&val);
    val.set_obj(CVmObjTads::create(vmg_ FALSE, 0, 4));
    G_stk->push(&val);

    /* 
     *   create another object, and store it in a property of the first
     *   object 
     */
    val.set_obj(CVmObjTads::create(vmg_ FALSE, 0, 4));
    stkval = G_stk->get(1);
    vm_objp(vmg_ stkval->val.obj)->
        set_prop(vmg_ G_undo, stkval->val.obj, PROP_A, &val);

    /* collect garbage - nothing should be deleted at this point */
    G_obj_table->gc_full(vmg0_);

    /* 
     *   forget about the second object by popping it off the stack, then
     *   collect garbage again -- the second object should be deleted at
     *   this point, because it's no longer reachable 
     */
    G_stk->discard();
    G_obj_table->gc_full(vmg0_);

    /*
     *   Force the first object's property table to expand by filling up
     *   its initial table 
     */
    stkval = G_stk->get(0);
    val.set_nil();
    vm_objp(vmg_ stkval->val.obj)->
        set_prop(vmg_ G_undo, stkval->val.obj, PROP_A, &val);
    vm_objp(vmg_ stkval->val.obj)->
        set_prop(vmg_ G_undo, stkval->val.obj, PROP_B, &val);
    vm_objp(vmg_ stkval->val.obj)->
        set_prop(vmg_ G_undo, stkval->val.obj, PROP_C, &val);
    vm_objp(vmg_ stkval->val.obj)->
        set_prop(vmg_ G_undo, stkval->val.obj, PROP_D, &val);
    vm_objp(vmg_ stkval->val.obj)->
        set_prop(vmg_ G_undo, stkval->val.obj, PROP_E, &val);

    /* set an existing property */
    vm_objp(vmg_ stkval->val.obj)->
        set_prop(vmg_ G_undo, stkval->val.obj, PROP_B, &val);
    
    /* we're done, so delete all of our managers */
    G_const_pool->detach_backing_store();
    delete imagefp;

    /* shut down the VM */
    vm_terminate(vmg__, &clientifc);

    /* delete the host interface */
    delete hostifc;

    /* terminate */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   dummy entrypoints - these are needed for linking, but we don't actually
 *   call anything that will end up invoking any of these 
 */

/* dummy implementation of dynamic link */
void CVmImageLoader::do_dynamic_link(VMG0_)
{
}

/* more image loader dummy implementations */
void CVmImageLoader::save_synth_exports(VMG_ class CVmFile *fp)
{
}

void CVmImageLoader::create_global_symtab_lookup_table(VMG0_)
{
}

int CVmImageLoader::restore_synth_exports(VMG_ class CVmFile *fp,
                                          class CVmObjFixup *)
{
    return 1;
}

void CVmImageLoader::run_static_init(VMG0_)
{
}

void CVmImageLoader::discard_synth_exports()
{
}

vm_prop_id_t CVmImageLoader::alloc_new_prop(VMG0_)
{
    return VM_INVALID_PROP;
}

vm_prop_id_t CVmImageLoader::get_last_prop(VMG0_)
{
    return (vm_prop_id_t)0;
}

