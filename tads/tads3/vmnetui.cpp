         /* Copyright (c) 2010 by Michael J. Roberts.  All Rights Reserved. */
/*
Name
  vmnetui.cpp - TADS 3 networking, UI extensions
Function
  Implements UI-related routines for the TADS 3 networking subsystem.
Notes
  These routines are segregated from vmnet.cpp to untangle vmnet.cpp from
  the VM globals.  This allows that module to be used in build configurations
  that don't include the VM globals, such as for the front-end process for
  the stand-alone Web UI on Windows (tadsweb.exe).
Modified
  04/11/10 MJRoberts  - Creation
*/


#include <stdlib.h>
#include <string.h>

#include "t3std.h"
#include "os.h"
#include "osifcnet.h"
#include "vmnet.h"
#include "vmglob.h"
#include "vmerr.h"
#include "vmtype.h"
#include "vmobj.h"
#include "vmstr.h"
#include "vmlst.h"
#include "vmisaac.h"
#include "sha2.h"
#include "vmpredef.h"
#include "vmhttpreq.h"
#include "vmglob.h"
#include "vmmain.h"
#include "vmpredef.h"
#include "vmerrnum.h"
#include "vmrun.h"
#include "vmfile.h"


/* ------------------------------------------------------------------------ */
/*
 *   UI Close event - prepare the event object.
 */
vm_obj_id_t TadsUICloseEvent::prep_event_obj(VMG_ int *argc, int *evt_type)
{
    /* we're not adding any arguments */
    *argc = 0;

    /* the event type is NetEvUIClose (4 - see include/tadsnet.h) */
    *evt_type = 4;

    /* use the basic NetEvent class */
    return G_predef->net_event;
}
