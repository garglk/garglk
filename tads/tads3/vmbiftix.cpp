#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* Copyright (c) 2005 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmbiftix.cpp - TADS Input/Output Extensions function set
Function
  
Notes
  
Modified
  03/02/05 MJRoberts  - Creation
*/

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "t3std.h"
#include "os.h"
#include "osifcext.h"
#include "utf8.h"
#include "charmap.h"
#include "vmbiftix.h"
#include "vmstack.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmglob.h"
#include "vmpool.h"
#include "vmobj.h"
#include "vmstr.h"
#include "vmlst.h"
#include "vmrun.h"
#include "vmhost.h"
#include "vmconsol.h"
#include "vmvec.h"


/* ------------------------------------------------------------------------ */
/*
 *   Show a popup menu 
 */
void CVmBifTIOExt::show_popup_menu(VMG_ uint argc)
{
    int x, y, default_pos;
    char *txt;
    os_event_info_t evt;
    int ret;
    int elecnt;
    vm_obj_id_t lst_obj;
    CVmObjList *lst;
    vm_val_t val;
    
    /* check arguments */
    check_argc(vmg_ argc, 3);

    /* get the x,y coordinates */
    if (G_stk->get(0)->typ == VM_NIL)
    {
        /* nil x,y - use default position */
        default_pos = TRUE;
        x = y = 0;

        /* discard the nil x,y values */
        G_stk->discard(2);
    }
    else
    {
        /* pop the x,y positions */
        x = pop_int_val(vmg0_);
        y = pop_int_val(vmg0_);
    }

    /* get the HTML text for the contents of the window */
    txt = pop_str_val_ui(vmg_ 0, 0);

    /* flush the console display output */
    G_console->flush_all(vmg_ VM_NL_NONE);

    /* show the window */
    ret = os_show_popup_menu(default_pos, x, y, txt, strlen(txt), &evt);

    /* free the HTML text buffer we allocated */
    t3free(txt);

    /* see what we have */
    switch (ret)
    {
    case OSPOP_FAIL:
    case OSPOP_CANCEL:
    case OSPOP_EOF:
    default:
        elecnt = 1;
        break;

    case OSPOP_HREF:
        elecnt = 2;
        break;
    }

    /* allocate the return list */
    lst_obj = CVmObjList::create(vmg_ FALSE, elecnt);
    lst = (CVmObjList *)vm_objp(vmg_ lst_obj);

    /* protect the list from garbage collection */
    val.set_obj(lst_obj);
    G_stk->push(&val);

    /* set the first element to the main return code */
    val.set_int(ret);
    lst->cons_set_element(0, &val);

    /* set additional elements according to the return code */
    switch (ret)
    {
    case OSPOP_HREF:
        /* add the HREF element */
        val.set_obj(str_from_ui_str(vmg_ evt.href));
        lst->cons_set_element(1, &val);
        break;

    default:
        /* there aren't any other elements for other return codes */
        break;
    }

    /* return the list */
    retval_obj(vmg_ lst_obj);

    /* discard the GC protection */
    G_stk->discard();
}

/* ------------------------------------------------------------------------ */
/*
 *   Enable one system menu command 
 */
static void enable_sys_menu_cmd_item(VMG_ const vm_val_t *idval, int stat)
{
    int id;

    /* retrieve the ID as an integer */
    G_stk->push(idval);
    id = CVmBif::pop_int_val(vmg0_);

    /* set it */
    os_enable_cmd_event(id, stat);
}

/*
 *   Enable/disable system menu commands
 */
void CVmBifTIOExt::enable_sys_menu_cmd(VMG_ uint argc)
{
    const char *lst;
    vm_val_t *valp;
    int stat;
    
    /* check arguments */
    check_argc(vmg_ argc, 2);

    /* the second argument is the new status - retrieve it as an integer */
    G_stk->push(G_stk->get(1));
    stat = pop_int_val(vmg0_);

    /* 
     *   The first argument is either a single menu ID, or a list of menu
     *   IDs.  Check what we have.  
     */
    valp = G_stk->get(0);
    if ((lst = valp->get_as_list(vmg0_)) != 0)
    {
        size_t cnt;
        vm_val_t ele;
        
        /* set the status for each element */
        for (cnt = vmb_get_len(lst), lst += VMB_LEN ; cnt != 0 ;
             --cnt, lst += VMB_DATAHOLDER)
        {
            /* get this element value */
            vmb_get_dh(lst, &ele);

            /* set the status */
            enable_sys_menu_cmd_item(vmg_ &ele, stat);
        }
    }
    else if (valp->typ == VM_INT)
    {
        /* it's a single value - handle it individually */
        enable_sys_menu_cmd_item(vmg_ valp, stat);
    }
    else if (valp->typ == VM_OBJ
             && CVmObjVector::is_vector_obj(vmg_ valp->val.obj))
    {
        CVmObjVector *vec;
        size_t cnt;
        size_t i;
        
        /* cast it to a vector */
        vec = (CVmObjVector *)vm_objp(vmg_ valp->val.obj);

        /* iterate over the elements */
        for (i = 1, cnt = vec->get_element_count() ; i <= cnt ; ++i)
        {
            vm_val_t idx;
            vm_val_t ele;
            
            /* get this element */
            idx.set_int(i);
            vec->index_val(vmg_ &ele, valp->val.obj, &idx);

            /* set the status */
            enable_sys_menu_cmd_item(vmg_ &ele, stat);
        }
    }

    /* discard the arguments, and we're done */
    G_stk->discard(2);
}

