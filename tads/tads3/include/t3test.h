#charset "us-ascii"
#pragma once

/* 
 *   Copyright (c) 1999, 2006 Michael J. Roberts
 *   
 *   This file is part of TADS 3
 *   
 *   This header defines the t3vmTEST function set.  This function set is
 *   intended primarily for use by the developers of the VM itself, for
 *   testing purposes.  User could should not use this function set, as the
 *   functionality here is not generally useful to "real" programs and is
 *   subject to future incompatible changes.  
 */

/* 
 *   T3 Test system interface 
 */
intrinsic 't3vmTEST/010000'
{
    t3test_get_obj_id(obj);
    t3test_get_obj_gc_state(id);
    t3test_get_charcode(c);
}

