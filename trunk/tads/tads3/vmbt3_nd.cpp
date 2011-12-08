#ifdef RCSID
static char RCSid[] =
"$Header$";
#endif

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmbt3_nd.cpp - T3 VM system interface function set - non-debugger version
Function
  
Notes
  
Modified
  03/11/00 MJRoberts  - Creation
*/

#include <time.h>

#include "t3std.h"
#include "vmtype.h"
#include "vmbif.h"
#include "vmbift3.h"
#include "vmstack.h"
#include "vmrun.h"
#include "charmap.h"
#include "vmdatasrc.h"
#include "vmlog.h"


/*
 *   Debug Trace 
 */
void CVmBifT3::debug_trace(VMG_ uint argc)
{
    /* make sure we have at least one argument */
    if (argc < 1)
        err_throw(VMERR_WRONG_NUM_OF_ARGS);

    /* pop the flags and see what we're being asked to do */
    switch(pop_int_val(vmg0_))
    {
    case T3DBG_CHECK:
        /* check arguments */
        check_argc(vmg_ argc, 1);

        /* we're just being asked if the debugger is present - it's not */
        retval_nil(vmg0_);
        break;

    case T3DBG_BREAK:
        /* check arguments */
        check_argc(vmg_ argc, 1);

        /* tell the caller there's no debugger */
        retval_nil(vmg0_);
        break;

    case T3DBG_LOG:
        {
            /* check arguments and retrieve the string */
            check_argc(vmg_ argc, 2);
            const char *str = G_stk->get(0)->get_as_string(vmg0_);
            if (str == 0)
                err_throw(VMERR_BAD_TYPE_BIF);

            /* decode and skip the length prefix */
            int len = vmb_get_len(str);
            str += VMB_LEN;
            
            /* convert the string to the local file character set */
            char *lstr;
            size_t llen = G_cmap_to_file->map_utf8_alo(&lstr, str, len);

            /* log the message */
            vm_log(vmg_ lstr, llen);

            /* done with the mapped string */
            t3free(lstr);

            /* discard the string */
            G_stk->discard();
        }

        /* no return value */
        retval_nil(vmg0_);
        break;

    default:
        /* anything else just returns nil, to allow for future expansion */
        G_stk->discard(argc - 1);
        retval_nil(vmg0_);
        break;
    }
}

