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
  vmbignum.cpp - big number metaclass
Function
  
Notes
  
Modified
  02/18/00 MJRoberts  - Creation
*/

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <limits.h>
#include <stdarg.h>
#include <math.h>
#include <float.h>

#include "t3std.h"
#include "vmtype.h"
#include "vmmcreg.h"
#include "vmbignum.h"
#include "vmobj.h"
#include "vmerr.h"
#include "vmerrnum.h"
#include "vmfile.h"
#include "vmpool.h"
#include "vmstack.h"
#include "utf8.h"
#include "vmstr.h"
#include "vmbif.h"
#include "vmmeta.h"
#include "vmlst.h"
#include "vmdatasrc.h"


/* ------------------------------------------------------------------------ */
/*
 *   statics 
 */

/* metaclass registration object */
static CVmMetaclassBigNum metaclass_reg_obj;
CVmMetaclass *CVmObjBigNum::metaclass_reg_ = &metaclass_reg_obj;

/* function table */
int (CVmObjBigNum::
     *CVmObjBigNum::func_table_[])(VMG_ vm_obj_id_t self,
                                   vm_val_t *retval, uint *argc) =
{
    &CVmObjBigNum::getp_undef,
    &CVmObjBigNum::getp_format,
    &CVmObjBigNum::getp_equal_rnd,
    &CVmObjBigNum::getp_get_prec,
    &CVmObjBigNum::getp_set_prec,
    &CVmObjBigNum::getp_frac,
    &CVmObjBigNum::getp_whole,
    &CVmObjBigNum::getp_round_dec,
    &CVmObjBigNum::getp_abs,
    &CVmObjBigNum::getp_ceil,
    &CVmObjBigNum::getp_floor,
    &CVmObjBigNum::getp_get_scale,
    &CVmObjBigNum::getp_scale,
    &CVmObjBigNum::getp_negate,
    &CVmObjBigNum::getp_copy_sign,
    &CVmObjBigNum::getp_is_neg,
    &CVmObjBigNum::getp_remainder,
    &CVmObjBigNum::getp_sin,
    &CVmObjBigNum::getp_cos,
    &CVmObjBigNum::getp_tan,
    &CVmObjBigNum::getp_deg2rad,
    &CVmObjBigNum::getp_rad2deg,
    &CVmObjBigNum::getp_asin,
    &CVmObjBigNum::getp_acos,
    &CVmObjBigNum::getp_atan,
    &CVmObjBigNum::getp_sqrt,
    &CVmObjBigNum::getp_ln,
    &CVmObjBigNum::getp_exp,
    &CVmObjBigNum::getp_log10,
    &CVmObjBigNum::getp_pow,
    &CVmObjBigNum::getp_sinh,
    &CVmObjBigNum::getp_cosh,
    &CVmObjBigNum::getp_tanh,
    &CVmObjBigNum::getp_pi,
    &CVmObjBigNum::getp_e,
    &CVmObjBigNum::getp_numType
};


/* constant value 1 */
const unsigned char CVmObjBigNum::one_[] =
{
    /* number of digits - we just need one to represent the integer 1 */
    0x01, 0x00,

    /* exponent */
    0x01, 0x00,

    /* flags */
    0x00,

    /* mantissa - just one digit is needed, but pad out the byte with zero */
    0x10
};


#if 0
/*
 *   Pi to 2048 digits 
 */
const unsigned char CVmObjBigNum::pi_[] =
{
    /* number of digits of precision */
    0x00, 0x08,

    /* base-10 exponent */
    0x01, 0x00,

    /* flags */
    0x00,

    /* 
     *   the first 2048 decimal digits of pi, packed two to a byte (typed
     *   in from memory - I hope I got everything right :) 
     */

    /* 1-100 */
    0x31, 0x41, 0x59, 0x26, 0x53, 0x58, 0x97, 0x93, 0x23, 0x84,
    0x62, 0x64, 0x33, 0x83, 0x27, 0x95, 0x02, 0x88, 0x41, 0x97,
    0x16, 0x93, 0x99, 0x37, 0x51, 0x05, 0x82, 0x09, 0x74, 0x94,
    0x45, 0x92, 0x30, 0x78, 0x16, 0x40, 0x62, 0x86, 0x20, 0x89,
    0x98, 0x62, 0x80, 0x34, 0x82, 0x53, 0x42, 0x11, 0x70, 0x67,

    /* 101-200 */
    0x98, 0x21, 0x48, 0x08, 0x65, 0x13, 0x28, 0x23, 0x06, 0x64,
    0x70, 0x93, 0x84, 0x46, 0x09, 0x55, 0x05, 0x82, 0x23, 0x17,
    0x25, 0x35, 0x94, 0x08, 0x12, 0x84, 0x81, 0x11, 0x74, 0x50,
    0x28, 0x41, 0x02, 0x70, 0x19, 0x38, 0x52, 0x11, 0x05, 0x55,
    0x96, 0x44, 0x62, 0x29, 0x48, 0x95, 0x49, 0x30, 0x38, 0x19,
    
    /* 201-300 */
    0x64, 0x42, 0x88, 0x10, 0x97, 0x56, 0x65, 0x93, 0x34, 0x46,
    0x12, 0x84, 0x75, 0x64, 0x82, 0x33, 0x78, 0x67, 0x83, 0x16,
    0x52, 0x71, 0x20, 0x19, 0x09, 0x14, 0x56, 0x48, 0x56, 0x69,
    0x23, 0x46, 0x03, 0x48, 0x61, 0x04, 0x54, 0x32, 0x66, 0x48,
    0x21, 0x33, 0x93, 0x60, 0x72, 0x60, 0x24, 0x91, 0x41, 0x27,

    /* 301-400 */
    0x37, 0x24, 0x58, 0x70, 0x06, 0x60, 0x63, 0x15, 0x58, 0x81,
    0x74, 0x88, 0x15, 0x20, 0x92, 0x09, 0x62, 0x82, 0x92, 0x54,
    0x09, 0x17, 0x15, 0x36, 0x43, 0x67, 0x89, 0x25, 0x90, 0x36,
    0x00, 0x11, 0x33, 0x05, 0x30, 0x54, 0x88, 0x20, 0x46, 0x65,
    0x21, 0x38, 0x41, 0x46, 0x95, 0x19, 0x41, 0x51, 0x16, 0x09,
    
    /* 401-500 */
    0x43, 0x30, 0x57, 0x27, 0x03, 0x65, 0x75, 0x95, 0x91, 0x95,
    0x30, 0x92, 0x18, 0x61, 0x17, 0x38, 0x19, 0x32, 0x61, 0x17,
    0x93, 0x10, 0x51, 0x18, 0x54, 0x80, 0x74, 0x46, 0x23, 0x79,
    0x96, 0x27, 0x49, 0x56, 0x73, 0x51, 0x88, 0x57, 0x52, 0x72,
    0x48, 0x91, 0x22, 0x79, 0x38, 0x18, 0x30, 0x11, 0x94, 0x91,
    
    /* 501-600 */
    0x29, 0x83, 0x36, 0x73, 0x36, 0x24, 0x40, 0x65, 0x66, 0x43,
    0x08, 0x60, 0x21, 0x39, 0x49, 0x46, 0x39, 0x52, 0x24, 0x73,
    0x71, 0x90, 0x70, 0x21, 0x79, 0x86, 0x09, 0x43, 0x70, 0x27,
    0x70, 0x53, 0x92, 0x17, 0x17, 0x62, 0x93, 0x17, 0x67, 0x52,
    0x38, 0x46, 0x74, 0x81, 0x84, 0x67, 0x66, 0x94, 0x05, 0x13,
    
    /* 601-700 */
    0x20, 0x00, 0x56, 0x81, 0x27, 0x14, 0x52, 0x63, 0x56, 0x08,
    0x27, 0x78, 0x57, 0x71, 0x34, 0x27, 0x57, 0x78, 0x96, 0x09,
    0x17, 0x36, 0x37, 0x17, 0x87, 0x21, 0x46, 0x84, 0x40, 0x90,
    0x12, 0x24, 0x95, 0x34, 0x30, 0x14, 0x65, 0x49, 0x58, 0x53,
    0x71, 0x05, 0x07, 0x92, 0x27, 0x96, 0x89, 0x25, 0x89, 0x23,

    /* 701-800 */
    0x54, 0x20, 0x19, 0x95, 0x61, 0x12, 0x12, 0x90, 0x21, 0x96,
    0x08, 0x64, 0x03, 0x44, 0x18, 0x15, 0x98, 0x13, 0x62, 0x97,
    0x74, 0x77, 0x13, 0x09, 0x96, 0x05, 0x18, 0x70, 0x72, 0x11,
    0x34, 0x99, 0x99, 0x99, 0x83, 0x72, 0x97, 0x80, 0x49, 0x95,
    0x10, 0x59, 0x73, 0x17, 0x32, 0x81, 0x60, 0x96, 0x31, 0x85,

    /* 801-900 */
    0x95, 0x02, 0x44, 0x59, 0x45, 0x53, 0x46, 0x90, 0x83, 0x02,
    0x64, 0x25, 0x22, 0x30, 0x82, 0x53, 0x34, 0x46, 0x85, 0x03,
    0x52, 0x61, 0x93, 0x11, 0x88, 0x17, 0x10, 0x10, 0x00, 0x31,
    0x37, 0x83, 0x87, 0x52, 0x88, 0x65, 0x87, 0x53, 0x32, 0x08,
    0x38, 0x14, 0x20, 0x61, 0x71, 0x77, 0x66, 0x91, 0x47, 0x30,

    /* 901-1000 */
    0x35, 0x98, 0x25, 0x34, 0x90, 0x42, 0x87, 0x55, 0x46, 0x87,
    0x31, 0x15, 0x95, 0x62, 0x86, 0x38, 0x82, 0x35, 0x37, 0x87,
    0x59, 0x37, 0x51, 0x95, 0x77, 0x81, 0x85, 0x77, 0x80, 0x53,
    0x21, 0x71, 0x22, 0x68, 0x06, 0x61, 0x30, 0x01, 0x92, 0x78,
    0x76, 0x61, 0x11, 0x95, 0x90, 0x92, 0x16, 0x42, 0x01, 0x98,

    /* 1001-1100 */
    0x93, 0x80, 0x95, 0x25, 0x72, 0x01, 0x06, 0x54, 0x85, 0x86,
    0x32, 0x78, 0x86, 0x59, 0x36, 0x15, 0x33, 0x81, 0x82, 0x79,
    0x68, 0x23, 0x03, 0x01, 0x95, 0x20, 0x35, 0x30, 0x18, 0x52,
    0x96, 0x89, 0x95, 0x77, 0x36, 0x22, 0x59, 0x94, 0x13, 0x89,
    0x12, 0x49, 0x72, 0x17, 0x75, 0x28, 0x34, 0x79, 0x13, 0x15,

    /* 1101-1200 */
    0x15, 0x57, 0x48, 0x57, 0x24, 0x24, 0x54, 0x15, 0x06, 0x95,
    0x95, 0x08, 0x29, 0x53, 0x31, 0x16, 0x86, 0x17, 0x27, 0x85,
    0x58, 0x89, 0x07, 0x50, 0x98, 0x38, 0x17, 0x54, 0x63, 0x74,
    0x64, 0x93, 0x93, 0x19, 0x25, 0x50, 0x60, 0x40, 0x09, 0x27,
    0x70, 0x16, 0x71, 0x13, 0x90, 0x09, 0x84, 0x88, 0x24, 0x01,

    /* 1201-1300 */
    0x28, 0x58, 0x36, 0x16, 0x03, 0x56, 0x37, 0x07, 0x66, 0x01,
    0x04, 0x71, 0x01, 0x81, 0x94, 0x29, 0x55, 0x59, 0x61, 0x98,
    0x94, 0x67, 0x67, 0x83, 0x74, 0x49, 0x44, 0x82, 0x55, 0x37,
    0x97, 0x74, 0x72, 0x68, 0x47, 0x10, 0x40, 0x47, 0x53, 0x46,
    0x46, 0x20, 0x80, 0x46, 0x68, 0x42, 0x59, 0x06, 0x94, 0x91,

    /* 1301-1400 */
    0x29, 0x33, 0x13, 0x67, 0x70, 0x28, 0x98, 0x91, 0x52, 0x10,
    0x47, 0x52, 0x16, 0x20, 0x56, 0x96, 0x60, 0x24, 0x05, 0x80,
    0x38, 0x15, 0x01, 0x93, 0x51, 0x12, 0x53, 0x38, 0x24, 0x30,
    0x03, 0x55, 0x87, 0x64, 0x02, 0x47, 0x49, 0x64, 0x73, 0x26,
    0x39, 0x14, 0x19, 0x92, 0x72, 0x60, 0x42, 0x69, 0x92, 0x27,

    /* 1401-1500 */
    0x96, 0x78, 0x23, 0x54, 0x78, 0x16, 0x36, 0x00, 0x93, 0x41,
    0x72, 0x16, 0x41, 0x21, 0x99, 0x24, 0x58, 0x63, 0x15, 0x03,
    0x02, 0x86, 0x18, 0x29, 0x74, 0x55, 0x57, 0x06, 0x74, 0x98,
    0x38, 0x50, 0x54, 0x94, 0x58, 0x85, 0x86, 0x92, 0x69, 0x95,
    0x69, 0x09, 0x27, 0x21, 0x07, 0x97, 0x50, 0x93, 0x02, 0x95,

    /* 1501-1600 */
    0x53, 0x21, 0x16, 0x53, 0x44, 0x98, 0x72, 0x02, 0x75, 0x59,
    0x60, 0x23, 0x64, 0x80, 0x66, 0x54, 0x99, 0x11, 0x98, 0x81,
    0x83, 0x47, 0x97, 0x75, 0x35, 0x66, 0x36, 0x98, 0x07, 0x42,
    0x65, 0x42, 0x52, 0x78, 0x62, 0x55, 0x18, 0x18, 0x41, 0x75,
    0x74, 0x67, 0x28, 0x90, 0x97, 0x77, 0x72, 0x79, 0x38, 0x00,

    /* 1601-1700 */
    0x08, 0x16, 0x47, 0x06, 0x00, 0x16, 0x14, 0x52, 0x49, 0x19,
    0x21, 0x73, 0x21, 0x72, 0x14, 0x77, 0x23, 0x50, 0x14, 0x14,
    0x41, 0x97, 0x35, 0x68, 0x54, 0x81, 0x61, 0x36, 0x11, 0x57,
    0x35, 0x25, 0x52, 0x13, 0x34, 0x75, 0x74, 0x18, 0x49, 0x46,
    0x84, 0x38, 0x52, 0x33, 0x23, 0x90, 0x73, 0x94, 0x14, 0x33,

    /* 1701-1800 */
    0x34, 0x54, 0x77, 0x62, 0x41, 0x68, 0x62, 0x51, 0x89, 0x83,
    0x56, 0x94, 0x85, 0x56, 0x20, 0x99, 0x21, 0x92, 0x22, 0x18,
    0x42, 0x72, 0x55, 0x02, 0x54, 0x25, 0x68, 0x87, 0x67, 0x17,
    0x90, 0x49, 0x46, 0x01, 0x65, 0x34, 0x66, 0x80, 0x49, 0x88,
    0x62, 0x72, 0x32, 0x79, 0x17, 0x86, 0x08, 0x57, 0x84, 0x38,

    /* 1801-1900 */
    0x38, 0x27, 0x96, 0x79, 0x76, 0x68, 0x14, 0x54, 0x10, 0x09,
    0x53, 0x88, 0x37, 0x86, 0x36, 0x09, 0x50, 0x68, 0x00, 0x64,
    0x22, 0x51, 0x25, 0x20, 0x51, 0x17, 0x39, 0x29, 0x84, 0x89,
    0x60, 0x84, 0x12, 0x84, 0x88, 0x62, 0x69, 0x45, 0x60, 0x42,
    0x41, 0x96, 0x52, 0x85, 0x02, 0x22, 0x10, 0x66, 0x11, 0x86,

    /* 1901-2000 */
    0x30, 0x67, 0x44, 0x27, 0x86, 0x22, 0x03, 0x91, 0x94, 0x94,
    0x50, 0x47, 0x12, 0x37, 0x13, 0x78, 0x69, 0x60, 0x95, 0x63,
    0x64, 0x37, 0x19, 0x17, 0x28, 0x74, 0x67, 0x76, 0x46, 0x57,
    0x57, 0x39, 0x62, 0x41, 0x38, 0x90, 0x86, 0x58, 0x32, 0x64,
    0x59, 0x95, 0x81, 0x33, 0x90, 0x47, 0x80, 0x27, 0x59, 0x00,

    /* 2001-2048 */
    0x99, 0x46, 0x57, 0x64, 0x07, 0x89, 0x51, 0x26, 0x94, 0x68,
    0x39, 0x83, 0x52, 0x59, 0x57, 0x09, 0x82, 0x58, 0x22, 0x62,
    0x05, 0x22, 0x48, 0x94
};
#endif

/* ------------------------------------------------------------------------ */
/*
 *   The BigNumber internal cache.  This is meant to be private to the
 *   vmbignum module, but since the module is actually split between
 *   vmbignum.cpp and vmbignumlib.cpp, we need to use a C++ global.  We use
 *   the S_ naming prefix to emphasize that it's conceptually a "static"
 *   variable, private to this two-file module.
 */
extern CVmBigNumCache *S_bignum_cache;

/* ------------------------------------------------------------------------ */
/*
 *   Static creation methods.  These routines allocate an object ID and
 *   create a new object.  
 */


/* create dynamically using stack arguments */
vm_obj_id_t CVmObjBigNum::create_from_stack(VMG_ const uchar **pc_ptr,
                                            uint argc)
{
    vm_obj_id_t id;
    vm_val_t *val;
    size_t digits;
    const char *strval = 0;
    const CVmObjBigNum *objval = 0;

    /* check arguments */
    if (argc < 1 || argc > 2)
        err_throw(VMERR_WRONG_NUM_OF_ARGS);

    /* 
     *   check the first argument, which gives the source value - this can be
     *   an integer, a string, or another BigNumber value 
     */
    val = G_stk->get(0);
    if (val->typ == VM_INT)
    {
        /* we'll use the integer value */
    }
    else if (val->typ == VM_OBJ && is_bignum_obj(vmg_ val->val.obj))
    {
        /* we'll use the BigNumber value as the source */
        objval = (CVmObjBigNum *)vm_objp(vmg_ val->val.obj);
    }
    else if ((strval = val->get_as_string(vmg0_)) != 0)
    {
        /* we'll parse the string value */
    }
    else
    {
        /* it's not a source type we accept */
        err_throw(VMERR_NUM_VAL_REQD);
    }

    /* 
     *   get the precision value, if provided; if not, infer it from the
     *   source value 
     */
    if (argc > 1)
    {
        /* make sure it's an integer */
        if (G_stk->get(1)->typ != VM_INT)
            err_throw(VMERR_INT_VAL_REQD);

        /* retrieve the value */
        digits = (size_t)G_stk->get(1)->val.intval;
    }
    else if (strval != 0)
    {
        /* parse the string to infer the precision */
        digits = precision_from_string(
            strval + VMB_LEN, vmb_get_len(strval), 10);
    }
    else if (objval != 0)
    {
        /* use the same precision as the source BigNumber value */
        digits = get_prec(objval->get_ext());
    }
    else
    {
        /* use a default precision */
        digits = 32;
    }

    /* create the value */
    if (strval != 0)
    {
        /* create the value from the string */
        id = vm_new_id(vmg_ FALSE, FALSE, FALSE);
        new (vmg_ id) CVmObjBigNum(vmg_ strval + VMB_LEN,
                                   vmb_get_len(strval), digits);
    }
    else if (objval != 0)
    {
        vm_val_t new_val;

        /* create the value based on the BigNumber value */
        round_val(vmg_ &new_val, objval->get_ext(), digits, TRUE);

        /* return the new object ID */
        id = new_val.val.obj;
    }
    else
    {
        /* create the value based on the integer value */
        id = vm_new_id(vmg_ FALSE, FALSE, FALSE);
        new (vmg_ id) CVmObjBigNum(vmg_ (long)val->val.intval, digits);
    }

    /* discard arguments */
    G_stk->discard(argc);

    /* return the new object */
    return id;
}

/* create with a given precision */
vm_obj_id_t CVmObjBigNum::create(VMG_ int in_root_set, size_t digits)
{
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
    new (vmg_ id) CVmObjBigNum(vmg_ digits);
    return id;
}

/* create from an integer value */
vm_obj_id_t CVmObjBigNum::create(VMG_ int in_root_set,
                                 long val, size_t digits)
{
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
    new (vmg_ id) CVmObjBigNum(vmg_ val, digits);
    return id;
}

/* create from an unsigned integer value */
vm_obj_id_t CVmObjBigNum::createu(VMG_ int in_root_set,
                                  ulong val, size_t digits)
{
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
    new (vmg_ id) CVmObjBigNum(vmg_ digits);
    set_uint_val(((CVmObjBigNum *)vm_objp(vmg_ id))->get_ext(), val);
    return id;
}

/* create from a 64-bit unsigned int expressed as two 32-bit segments */
vm_obj_id_t CVmObjBigNum::create_int64(VMG_ int in_root_set,
                                       uint32_t hi, uint32_t lo)
{
    /* 
     *   Store into our portable 8-byte format: the portable format is
     *   little-endian, so simply store the low part in the first four bytes,
     *   and the high part in the second four bytes.
     */
    char buf[8];
    oswp4(buf, lo);
    oswp4(buf+4, hi);

    /* 
     *   Now create the value from the buffer.  (This could be implemented a
     *   little more efficiently by working directly from the integers, but
     *   this is certainly not on any critical paths, and the create_rp8 code
     *   already exists and works.)
     */
    return create_rp8(vmg_ in_root_set, buf);
}

/* create from a 64-bit unsigned int expressed in portable 8-byte format */
vm_obj_id_t CVmObjBigNum::create_rp8(VMG_ int in_root_set,
                                     const char *buf)
{
    /* 
     *   create the BigNumber object big enough to hold a 64-bit integer
     *   value, which can have up to 20 digits 
     */
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
    new (vmg_ id) CVmObjBigNum(vmg_ 20);
    CVmObjBigNum *n = (CVmObjBigNum *)vm_objp(vmg_ id);

    /* set the value */
    n->set_rp8((const uchar *)buf);

    /* the source value is unsigned, so it's always non-negative */
    set_neg(n->get_ext(), FALSE);

    /* return the object */
    return id;
}

/* create from a 64-bit signed int */
vm_obj_id_t CVmObjBigNum::create_rp8s(VMG_ int in_root_set,
                                      const char *buf)
{
    /* 
     *   create the BigNumber object big enough to hold a 64-bit integer
     *   value, which can have up to 20 digits 
     */
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
    new (vmg_ id) CVmObjBigNum(vmg_ 20);
    CVmObjBigNum *n = (CVmObjBigNum *)vm_objp(vmg_ id);

    /* make a private copy of the source value */
    uchar p[8];
    memcpy(p, buf, 8);

    /* 
     *   If the value is negative, as indicated by the sign bit on the most
     *   significant byte, get the absolute value by computing the two's
     *   complement.  This makes the arithmetic easy because we can do the
     *   bit twiddling with unsigned values, and just set the BigNum sign bit
     *   when we're done with all of that.
     *   
     *   Note that this 2's complement arithmetic is truly portable - it
     *   doesn't matter whether the local machine's native int type is
     *   big-endian or little-endian or some bizarre interleaved-endian, or
     *   if it uses 2's complement notation or some other wacky bygone scheme
     *   (1's complement, sign bit only, etc).  The reason this code is
     *   portable regardless of the local notation is that we're not
     *   operating on local int types: we're operating on a buffer that's in
     *   a universal notation that we define as 64-bit 2's complement
     *   little-endian.  We're doing our work at the byte level, so as long
     *   as the local machine has 8+-bit bytes (which is a requirement for
     *   the platform to have a C compiler), this code is portable.  
     */
    int neg = (p[7] & 0x80) != 0;
    if (neg)
        twos_complement_p8(p);

    /* set the 64-bit int value */
    n->set_rp8(p);

    /* set the sign bit */
    set_neg(n->get_ext(), neg);

    /* return the object */
    return id;
}

/* set the value from an unsigned 64-bit little-endian buffer */
void CVmObjBigNum::set_rp8(const uchar *p)
{
    /* 
     *   set up temporary registers with enough precision for 64-bit ints (we
     *   need at most 20 digits for these values) 
     */
    char tmp1[5 + 20] = { 20, 0, 0, 0, 0 };
    char tmp2[5 + 20] = { 20, 0, 0, 0, 0 };

    /*
     *   shift in bits 24 at a time - this makes the math fit in 32-bit ints
     *   so that we don't have to worry about int overflows 
     */
    set_uint_val(get_ext(),
                 (((ulong)p[7]) << 16) | (((ulong)p[6]) << 8) | p[5]);

    mul_by_long(get_ext(), 0x01000000);
    set_uint_val(tmp1, (((ulong)p[4]) << 16) | (((ulong)p[3]) << 8) | p[2]);
    compute_sum_into(tmp2, get_ext(), tmp1);

    mul_by_long(tmp2, 0x10000);
    set_uint_val(tmp1, (((ulong)p[1]) << 8) | p[0]);
    compute_sum_into(get_ext(), tmp2, tmp1);
}

/* create from a string value */
vm_obj_id_t CVmObjBigNum::create(VMG_ int in_root_set,
                                 const char *str, size_t len, size_t digits)
{
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
    new (vmg_ id) CVmObjBigNum(vmg_ str, len, digits);
    return id;
}

/* create from a string value, inferring the required precision */
vm_obj_id_t CVmObjBigNum::create(VMG_ int in_root_set,
                                 const char *str, size_t len)
{
    int digits = precision_from_string(str, len, 10);
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
    new (vmg_ id) CVmObjBigNum(vmg_ str, len, digits);
    return id;
}

/* create from a string value in a given radix */
vm_obj_id_t CVmObjBigNum::create_radix(
    VMG_ int in_root_set, const char *str, size_t len, int radix)
{
    int digits = precision_from_string(str, len, radix);
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
    if (radix == 10)
        new (vmg_ id) CVmObjBigNum(vmg_ str, len, digits);
    else
    {
        new (vmg_ id) CVmObjBigNum(vmg_ digits);
        ((CVmObjBigNum *)vm_objp(vmg_ id))->set_str_val(str, len, radix);
    }
    return id;
}

/* create from a double value, with enough precision for a native double */
vm_obj_id_t CVmObjBigNum::create(VMG_ int in_root_set, double val)
{
    int digits = DBL_DIG + 2;
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
    new (vmg_ id) CVmObjBigNum(vmg_ val, digits);
    return id;
}

/* create from a double value, with specified precision */
vm_obj_id_t CVmObjBigNum::create(VMG_ int in_root_set, double val,
                                 size_t digits)
{
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
    new (vmg_ id) CVmObjBigNum(vmg_ val, digits);
    return id;
}

/*
 *   Create from a BER-encoded compressed integer value 
 */
vm_obj_id_t CVmObjBigNum::create_from_ber(VMG_ int in_root_set,
                                          const char *buf, size_t len)
{
    /* 
     *   Create the object. The source value has 7 bits of precision per
     *   byte.  We need log(2)/log(10) digits of precision per bit of
     *   precision, which is about 0.30103 digits per bit.
     */
    int prec = (int)(len*7*0.30103 + 0.5);
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
    new (vmg_ id) CVmObjBigNum(vmg_ prec);
    CVmObjBigNum *n = (CVmObjBigNum *)vm_objp(vmg_ id);
    char *ext = n->get_ext();

    /* allocate two temporary registers for intermediate sums */
    char *t1, *t2;
    uint hdl1, hdl2;
    alloc_temp_regs((size_t)prec, 2, &t1, &hdl1, &t2, &hdl2);

    /* zero the accumulator (start with t1) */
    set_zero(t1);

    /* set up a stack register for the base-128 digits */
    char dig[5 + 3] = { 3, 0, 0, 0, 0 };

    /* starting at the most significant byte, shift in the bits */
    for (size_t i = 0 ; i < len ; ++i)
    {
        /* multiply the accumulator by 2^7 */
        mul_by_long(t1, 128);

        /* add in the next base-128 digit */
        set_uint_val(dig, buf[i] & 0x7f);
        compute_sum_into(t2, t1, dig);

        /* swap registers so that t2 becomes the accumulator */
        char *tt = t1; t1 = t2; t2 = tt;
    }

    /* copy the accumulator into the result */
    copy_val(ext, t1, FALSE);

    /* done with the temporary registers */
    release_temp_regs(2, hdl1, hdl2);

    /* return the new ID */
    return id;
}

/*
 *   Create from a floating point value encoded in an IEEE 754-2008 binary
 *   interchange format, little-endian order.  We accept bit sizes of 16, 32,
 *   64, and 128 (half, single, double, and quad precision).
 *   
 */
vm_obj_id_t CVmObjBigNum::create_from_ieee754(
    VMG_ int in_root_set, const char *buf, int bits)
{
    /* 
     *   Create the object.  Use the decimal equivalent precision of the IEEE
     *   754 object.  Note that we use 17 digits for doubles; the binary type
     *   stores 53 bits of mantissa == 15.95 decimal digits, but there's a
     *   pathological case at 1+epsilon, which is 1 + 2e-16, which requires
     *   17 decimal digits to store.  
     */
    int prec = (bits == 16 ? 4 : bits == 32 ? 8 : bits == 64 ? 17 :
                bits == 128 ? 35 : 8);
    vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
    new (vmg_ id) CVmObjBigNum(vmg_ prec);
    CVmObjBigNum *n = (CVmObjBigNum *)vm_objp(vmg_ id);

    /* decode the IEEE 754 value */
    n->set_ieee754_value(vmg_ buf, bits);

    /* return the new object id */
    return id;
}



/* ------------------------------------------------------------------------ */
/*
 *   Cast a value to BigNumber 
 */
void CVmObjBigNum::cast_to_bignum(VMG_ vm_val_t *bnval,
                                  const vm_val_t *srcval)
{
    const char *str;
    if (srcval->typ == VM_INT)
    {
        /* create from the integer value */
        bnval->set_obj(create(
            vmg_ FALSE, (long)srcval->val.intval, (size_t)10));
    }
    else if ((str = srcval->get_as_string(vmg0_)) != 0)
    {
        /* get the string length and buffer pointer */
        size_t len = vmb_get_len(str);
        str += VMB_LEN;

        /* create from the string value */
        bnval->set_obj(create(vmg_ FALSE, str, len));
    }
    else if (srcval->typ == VM_OBJ && is_bignum_obj(vmg_ srcval->val.obj))
    {
        /* it's already a BigNumber - just return the same value */
        bnval->set_obj(srcval->val.obj);
    }
    else
    {
        /* can't cast to BigNumber */
        err_throw(VMERR_NO_BIGNUM_CONV);
    }
}


/*
 *   Promote an integer value to BigNumber 
 */
void CVmObjBigNum::promote_int(VMG_ vm_val_t *val) const
{
    val->set_obj(create(vmg_ FALSE, (long)val->val.intval, (size_t)10));
}


/* ------------------------------------------------------------------------ */
/*
 *   Constructors.  These are called indirectly through our static
 *   creation methods.  
 */

/*
 *   Create with no extension 
 */
CVmObjBigNum::CVmObjBigNum()
{
    /* no extension */
    ext_ = 0;
}

/*
 *   Create with a given precision 
 */
CVmObjBigNum::CVmObjBigNum(VMG_ size_t digits)
{
    /* allocate space */
    alloc_bignum(vmg_ digits);
}

/*
 *   Create with a given integer value 
 */
CVmObjBigNum::CVmObjBigNum(VMG_ long val, size_t digits)
{
    /* allocate space */
    alloc_bignum(vmg_ digits);

    /* set the value */
    set_int_val(ext_, val);
}

/*
 *   Create with a given string as the source value
 */
CVmObjBigNum::CVmObjBigNum(VMG_ const char *str, size_t len, size_t digits)
{
    /* allocate space */
    alloc_bignum(vmg_ digits);

    /* set the value */
    set_str_val(str, len);
}

/*
 *   Create from a double value 
 */
CVmObjBigNum::CVmObjBigNum(VMG_ double val, size_t digits)
{
    /* allocate space */
    alloc_bignum(vmg_ digits);

    /* set the value */
    set_double_val(ext_, val);
}

/* ------------------------------------------------------------------------ */
/*
 *   Delete 
 */
void CVmObjBigNum::notify_delete(VMG_ int in_root_set)
{
    /* 
     *   free our extension - do this only if it's not in the root set,
     *   because extension will be directly in the image data for a root
     *   set object 
     */
    if (ext_ != 0 && !in_root_set)
        G_mem->get_var_heap()->free_mem(ext_);
}


/* ------------------------------------------------------------------------ */
/*
 *   Allocate space for a given precision 
 */
void CVmObjBigNum::alloc_bignum(VMG_ size_t digits)
{
    /* allocate space for the given number of elements */
    ext_ = (char *)G_mem->get_var_heap()
           ->alloc_mem(calc_alloc(digits), this);

    /* set the precision */
    set_prec(ext_, digits);

    /* initialize the value to zero */
    set_int_val(ext_, 0);

    /* clear the flags */
    ext_[VMBN_FLAGS] = 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Write to a 'data' mode file 
 */
int CVmObjBigNum::write_to_data_file(CVmDataSource *fp)
{
    char buf[16];

    /* write the number of digits (i.e., the precision) */
    oswp2(buf, get_prec(ext_));
    if (fp->write(buf, 2))
        return 1;

    /* write our entire extension */
    if (fp->write(ext_, calc_alloc(get_prec(ext_))))
        return 1;

    /* success */
    return 0;
}

/*
 *   Read from a 'data' mode file and instantiate a new BigNumber object to
 *   hold the result 
 */
int CVmObjBigNum::read_from_data_file(VMG_ vm_val_t *retval, CVmDataSource *fp)
{
    char buf[16];
    size_t prec;
    CVmObjBigNum *bignum;

    /* read the precision */
    if (fp->read(buf, 2))
        return 1;
    prec = osrp2(buf);

    /* create a BigNumber with the required precision */
    retval->set_obj(create(vmg_ FALSE, prec));
    bignum = (CVmObjBigNum *)vm_objp(vmg_ retval->val.obj);

    /* read the bytes into the new object's extension */
    if (fp->read(bignum->get_ext(), calc_alloc(prec)))
        return 1;

    /* success */
    return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   Set my value to a string 
 */
void CVmObjBigNum::set_str_val(const char *str, size_t len)
{
    /* parse the string into my extension */
    parse_str_into(ext_, str, len);
}

/*
 *   Set my string value with a given radix.  If the radix is decimal, we'll
 *   use the regular floating point parser.  Otherwise we'll parse it as an
 *   integer value in the given radix.  
 */
void CVmObjBigNum::set_str_val(const char *str, size_t len, int radix)
{
    /* parse the string into my extension using the given radix */
    parse_str_into(ext_, str, len, radix);
}

/* ------------------------------------------------------------------------ */
/*
 *   IEEE 754-2008 binary interchange format buffer manipulation.  We arrange
 *   our buffer in little-endian order to match the idiosyncratic TADS
 *   pack/unpack formats, but otherwise we use the standard IEEE formats.  
 */

/* IEEE 754-2008 buffer */
struct ieee754
{
    ieee754(char *buf, int bits)
    {
        /* remember the buffer and bit size */
        this->buf = (unsigned char *)buf;
        this->bits = bits;
        this->bytes = bits/8;
        
        /* get the mantissa size (in bits) for the format size */
        mbits = (bits == 16 ? 10 :
                 bits == 32 ? 23 :
                 bits == 64 ? 52 :
                 bits == 128 ? 112 :
                 0);
        if (mbits == 0)
            err_throw(VMERR_NO_DOUBLE_CONV);
        
        /* the exponent uses the leftover bits, minus the sign bit */
        ebits = bits - mbits - 1;
        
        /* figure the exponent bias - it's the halfway point for the range */
        ebias = (1 << (ebits - 1)) - 1;
        
        /* figure the exponent range */
        emin = 1 - ebias;
        emax = ebias;
    }

    /* 
     *   Get the number of decimal digits this type can represent.  The
     *   decimal precision is fractional, since the IEEE type is binary; we
     *   round up to the next integer.  There's a 
     */
    int decimal_digits()
    {
        return bits == 16 ? 4 : bits == 32 ? 8 : bits == 64 ? 17 :
            bits == 128 ? 35 : 0;
    }

    /* get the maximum number of decimal digits in the exponent */
    int decimal_exp_digits()
    {
        return ebits <= 5 ? 2 : ebits <= 7 ? 3 : ebits <= 11 ? 4 : 5;
    }

    /* is the value infinity? */
    int is_infinity() const
    {
        /* infinity has an exponent of emax+1 and a zero mantissa */
        return get_exp() == emax + 1 && is_mantissa_zero();
    }

    /* set Infinity */
    void set_infinity(int neg)
    {
        /* 
         *   Infinity is represented with the maximum exponent plus one, and
         *   the mantissa set to all zeros.  Infinities can be positive or
         *   negative, so set the sign bit.  
         */
        memset(buf, 0, bytes);
        set_exp(emax + 1);
        set_sign(neg);
    }

    /* is the value zero? */
    int is_zero() const
    {
        /* 
         *   a zero is represented by all bits zero except possibly the sign
         *   bit, which is 0 for +0 and 1 for -0 
         */
        int sum = 0;
        for (int i = 0 ; i < bytes - 1 ; ++i)
            sum += buf[i];

        return sum == 0 && (buf[bytes-1] & 0x7f) == 0;
    }

    /* set Zero */
    void set_zero(int neg)
    {
        /* 
         *   Zero is represented with an exponent of zero and all zero bits
         *   in the mantissa.  Zeros can be positive or negative, so set the
         *   sign bit.  
         */
        memset(buf, 0, bytes);
        set_sign(neg);
    }

    /* is this a NAN value? */
    int is_nan() const
    {
        /* a NAN value has an exponent of emax+1 and a non-zero mantissa */
        return get_exp() == emax + 1 && !is_mantissa_zero();
    }

    /* is the mantissa zero? */
    int is_mantissa_zero() const
    {
        /* add up the full bytes of the mantissa, plus the partial high byte */
        int imsb = mantissa_msb_index();
        int sum = buf[imsb] & mantissa_msb_mask();
        for (int i = 0 ; i < imsb ; sum += buf[i++]) ;

        /* the mantissa is zero if the sum of the bytes is zero */
        return (sum == 0);
    }

    /* get the bit mask for the partial high-order byte of the mantissa */
    unsigned char mantissa_msb_mask() const
    {
        if (ebits <= 7)
            return (0x7f >> ebits);
        else if (ebits <= 15)
            return (0xff >> (ebits - 7));
        else
        {
            assert(FALSE);
            return 0;
        }
    }

    /* get the index of the most significant byte of the mantissa */
    int mantissa_msb_index() const
    {
        if (ebits <= 7)
            return bytes - 1;
        else if (ebits <= 15)
            return bytes - 2;
        else
        {
            assert(FALSE);
            return 0;
        }
    }

    /* set NAN (not a number) */
    void set_nan()
    {
        /* 
         *   NAN (Not A Number) is represented with the maximum exponent plus
         *   one, and a non-zero mantissa.  
         */
        set_exp(emax + 1);
        set_mbit(0, 1);
    }

    /* get the sign bit - false -> positive, true -> negative */
    int get_sign() const
    {
        return (buf[bytes-1] & 0x80) != 0;
    }

    /* set the sign bit */
    void set_sign(int neg)
    {
        if (neg)
            buf[bytes-1] |= 0x80;
        else
            buf[bytes-1] &= 0x7f;
    }

    /* get the byte index and bit mask for a mantissa bit */
    inline void get_mbit_ptr(int &byteidx, unsigned char &mask, int n) const
    {
        /* figure the raw bit vector index, past the sign and exponent bits */
        n += 1 + ebits;

        /* get the byte index (little-endian order) */
        byteidx = bytes - 1 - n/8;

        /* get the bit index within the byte, and the corresponding bit mask */
        n = 7 - (n % 8);
        mask = 1 << n;
    }

    /* set the nth bit of the mantissa (0 is the high-order bit) */
    void set_mbit(int n, int bit)
    {
        /* get the mantissa byte index and mask */
        int byteidx;
        unsigned char mask;
        get_mbit_ptr(byteidx, mask, n);

        /* set or clear the bit */
        if (bit)
            buf[byteidx] |= mask;
        else
            buf[byteidx] &= ~mask;
    }
    
    /* 
     *   Round up - add 1 to the lowest bit.  Returns true if we carried to
     *   out of the highest order bit, false if not. 
     *   
     *   Carrying means that we carried into the implied "1" above the first
     *   stored bit.  That bumps that bit to 0 and carries to the next bit,
     *   so we now have an implied "10" before the first bit.  The caller
     *   must renormalize simply by incrementing the exponent.  (The caller
     *   doesn't have to worry about whether this is a normal or subnormal
     *   number, because bumping the exponent works for both.  For
     *   subnormals, we carry into the implied "0" above the first bit, which
     *   bumps it to 1, which puts us in the normalized range, which we
     *   represent by bumping the exponent up one to emin.)  
     */
    int round_up()
    {
        /* add to the low byte, and carry as needed to whole bytes */
        int imsb = mantissa_msb_index();
        for (int i = 0 ; i < imsb ; ++i)
        {
            if (buf[i] == 0xff)
            {
                /* this overflows the byte - zero it and carry to the next */
                buf[i] = 0;
            }
            else
            {
                /* no overflow - increment this byte and return "no carry" */
                buf[i] += 1;
                return FALSE;
            }
        }
        
        /* 
         *   if we got this far, we've carried into the most significant
         *   byte, which is a partial byte - we need to use the msb bit mask
         *   for this arithmetic 
         */
        unsigned char mask = mantissa_msb_mask();
        int b = (buf[imsb] + 1) & mask;
        buf[imsb] &= ~mask;
        buf[imsb] |= b;

        /* if we wrapped that to zero, we carried into the implied lead bit */
        return (b == 0);
    }

    int get_mbit(int n) const
    {
        /* get the mantissa byte index and mask */
        int byteidx;
        unsigned char mask;
        get_mbit_ptr(byteidx, mask, n);

        /* return the bit value */
        return (buf[byteidx] & mask) != 0;
    }

    /* set the exponent, adjusting for the bias */
    void set_exp(int e)
    {
        /* add the bias, and set the raw exponent */
        set_raw_exp(ebias + e);
    }

    /* set the raw exponent, without the bias adjustment */
    void set_raw_exp(int e)
    {
        int mask;
        if (ebits <= 7)
        {
            /* it fits entirely in the first byte */
            mask = (0xff << (7 - ebits)) & 0x7f;
            buf[bytes-1] &= ~mask;
            buf[bytes-1] |= (e << (7 - ebits)) & mask;
        }
        else if (ebits <= 15)
        {
            /* it fits in the first two bytes */
            buf[bytes-1] &= 0x80;
            buf[bytes-1] |= (e >> (ebits - 7)) & 0x7f;
            
            mask = (0xff << (15 - ebits)) & 0xff;
            buf[bytes-2] &= ~mask;
            buf[bytes-2] |= (e << (15 - ebits)) & mask;
        }
        else
        {
            /* we don't currently handle anything above quad precision */
            assert(FALSE);
        }
    }

    /* get the exponent, adjusting for the bias */
    int get_exp() const { return get_raw_exp() - ebias; }

    /* get the raw exponent */
    int get_raw_exp() const
    {
        int mask;
        if (ebits <= 7)
        {
            /* it fits entirely in the first byte */
            mask = (0xff << (7 - ebits)) & 0x7f;
            return (int)(buf[bytes-1] & mask) >> (7 - ebits);
        }
        else if (ebits <= 15)
        {
            /* it fits in the first two bytes */
            int e = (int)(buf[bytes-1] & 0x7f) << (ebits - 7);

            mask = (0xff << (15 - ebits)) & 0xff;
            return e | (int)(buf[bytes-2] & mask) >> (15 - ebits);
        }
        else
        {
            /* we don't currently handle anything above quad precision */
            assert(FALSE);
            return 0;
        }
    }

    /* overall bit size (16, 32, 64, or 128) */
    int bits;

    /* overall byte length */
    int bytes;

    /* mantissa size in bits */
    int mbits;

    /* exponent size in bits */
    int ebits;

    /* minimum and maximum exponent values, and exponent bias */
    int emin, emax;
    int ebias;

    /* our buffer - the caller provides this */
    unsigned char *buf;
};

/* ------------------------------------------------------------------------ */
/*
 *   Convert to IEEE 754-2008 binary interchange format.  'bits' is the
 *   number of bits in the format; we accept all of the standard format sizes
 *   (16, 32, 64, 128).  Our output conforms to the IEEE standard for the
 *   format, except that we generate the buffer in the reverse byte order;
 *   this is for consistency with the idiosyncratic TADS pack/unpack formats,
 *   which use little-endian ordering for consistency with the standard TADS
 *   integer interchange formats.  To get the fully standard format, simply
 *   reverse the byte order of the buffer we return.  
 */
void CVmObjBigNum::convert_to_ieee754(VMG_ char *buf, int bits, int &ov)
{
    /* no overflow yet */
    ov = FALSE;

    /* set up the buffer descriptor */
    ieee754 val(buf, bits);

    /* 
     *   Check for zero, infinity, and NAN.  These all have special
     *   representations that don't follow from the arithmetic algorithm
     *   below.  
     */
    if (is_zero(ext_))
    {
        val.set_zero(get_neg(ext_));
        return;
    }
    else if (is_infinity(ext_))
    {
        val.set_infinity(get_neg(ext_));
        return;
    }
    else if (is_nan(ext_))
    {
        val.set_nan();
        return;
    }

    /* 
     *   Figure the precision we need for our intermediate calculations.  Our
     *   most demanding calculation is the log2 of the value; we need to be
     *   able to represent the full precision of the binary type in the
     *   *fractional* part of the log2 result.  The log2 result can have up
     *   to the maximum exponent as the whole part, so the precision we need
     *   for this calculation is (decimal digits of mantissa) + (decimal
     *   digits of exponent).  Add a couple of guard digits as usual so that
     *   we don't lose significant precision in rounding in intermediate
     *   calculations.  
     */
    int prec = val.decimal_digits() + val.decimal_exp_digits() + 2;

    /* 
     *   Set up some registers for intermediate calculations.  We don't have
     *   to allocate these since we know the upper bound to the precision
     *   we'll need (namely 35+2 for the 128-bit type). 
     */
    char t1[5+37] = { (char)prec, 0, 0, 0, 0 };
    char t2buf[5+37] = { (char)prec, 0, 0, 0, 0 }, *t2 = t2buf;
    char t3buf[5+37] = { (char)prec, 0, 0, 0, 0 }, *t3 = t3buf;

    /*
     *   The target format represents the number as A*2^B.  Call our value N.
     *   Observe that N == 2^(log2(N)) for any N.  Let's separate out log2(N)
     *   into its whole and fractional parts - call them W and F: so log2(N)
     *   == W+F, which makes N == 2^(W+F) == 2^W * 2^F == 2^F * 2^W.  Recall
     *   that the format we're looking for is A*2^B, which we now have: A is
     *   2^F, and B is W.
     *   
     *   That will give us *a* binary representation of the number, but not
     *   necessarily the canonical representation: we have to normalize by
     *   choosing the exponent such that the implied (but not stored) first
     *   bit of the mantissa is 1.  The ln2 calculation will get us close; we
     *   then need to adjust the exponent once we start pulling out the bits
     *   of the mantissa.  
     */

    /* get/cache ln2 to the required precision */
    const char *ln2 = cache_ln2(prec);

    /* 
     *   Get the absolute value of the argument.  Our format and the IEEE
     *   format both represent the value as a combination of an absolute
     *   value and a sign, so the only thing we need to do with the sign is
     *   to set the same sign on the result as on the input.  For the log
     *   calculation, though, we need to be working with a positive value. 
     */
    copy_val(t2, ext_, TRUE);
    set_neg(t2, FALSE);

    /* calculate t1=ln(self), t2=t1/ln(2) == ln2(self) */
    compute_ln_into(t1, t2);
    compute_quotient_into(t2, 0, t1, ln2);

    /* 
     *   Get the whole part as an integer - this is the "B" value as above.
     *   Note that this will definitely fit in a 32-bit integer type:
     *   BigNumber stores absolute values in the range 10^-32768 to 10^32767,
     *   so log2(self) can range from about -108000 to +108000.
     */
    copy_val(t1, t2, FALSE);
    compute_whole(t1);
    int32_t b = convert_to_int_base(t1, ov);
    if (get_neg(t1))
        b = -b;

    /* 
     *   Calculate 2^fractional part - this is the "A" value as above.  2^y =
     *   e^(y*ln2), so calculate t2*ln2 into t3, then e^t3 back into t2.  
     */
    compute_frac(t2);
    compute_prod_into(t1, t2, ln2);
    compute_exp_into(t2, t1);

    /*
     *   If 'b' is less than emin-1 (the subnormal exponent value), we need
     *   to raise it back up to emin-1.  Do this by halving the mantissa and
     *   incrementing 'b', repeating until 'b' is emin-1.
     *   
     *   If 'b' is less than emin-1 by more than the bit precision of the
     *   target type, we won't have any bits to store, so store zero.  
     */
    if (b < val.emin - 1 - val.mbits)
    {
        val.set_zero(get_neg(ext_));
        return;
    }
    while (b < val.emin - 1)
    {
        div_by_long(t2, 2);
        b += 1;
    }

    /* 
     *   Extract the bits of the mantissa.  Do this by repeatedly shifting
     *   and subtracting: starting with divisor = 1, if decimal mantissa >=
     *   divisor, subtract divisor and set the current bit in the output to
     *   1, otherwise set the output bit to 0; set divisor = divisor/2,
     *   repeat until we've filled the output mantissa.  A is 2^fraction,
     *   where -1 < fraction < 1, so 0.5 < A < 2.
     *   
     *   Normalization requires that the output mantissa start with an
     *   implied 1.  This means that we ignore leading zeros; for each
     *   leading zero, we decrement b but don't set an output bit.  When we
     *   reach the first 1 bit, we don't store it, either, since it's implied
     *   by normalization.  However, if we reach emin-1 for 'b', we're in the
     *   subnormalization range, which means we can't decrement the exponent
     *   any further; instead, start storing leading zeros.  (Subnormal
     *   means that the number is stored with an implied *0* leading bit, and
     *   the special stored exponent value of emin-1, which implies an actual
     *   exponent of emin.)  
     */
    copy_val(t1, (const char *)one_, FALSE);
    int mbit, sig;
    for (mbit = 0, sig = FALSE ; mbit < val.mbits ; div_by_long(t1, 2))
    {
        /* the current bit is 1 if t2 >= t1 */
        if (compare_abs(t2, t1) >= 0)
        {
            /* 
             *   it's a 1 bit - if it's the first 1 bit, note that we've
             *   found a significant bit, but don't store it, since the first
             *   bit is implied in the output format 
             */
            if (sig || b == val.emin - 1)
                val.set_mbit(mbit++, 1);
            else
                sig = TRUE;

            /* subtract t2 from t1 */
            compute_abs_diff_into(t3, t2, t1);

            /* swap t3 and t2 to keep the accumulator named t2 */
            char *ttmp = t2; t2 = t3; t3 = ttmp;
        }
        else
        {
            /* 
             *   It's a zero bit - store it if we've found a 1 already, OR
             *   the exponent is already at the subnormal value.  Otherwise
             *   just decrement the exponent.  
             */
            if (sig || b == val.emin - 1)
                val.set_mbit(mbit++, 0);
            else
                --b;
        }
    }

    /* 
     *   Round the result: if the remainder is less than half of the last
     *   bit's value (i.e., t2 < t1), round down (i.e., do nothing).  If the
     *   remainder is greater than half of the last bit's value (i.e. t2 >
     *   t1), round up (add 1 to the low-order bit).  If the remainder is
     *   exactly half of the last bit's value, round up if the last bit was a
     *   1, down if it was a 0.  
     */
    int r = compare_abs(t2, t1);
    if (r > 0 || (r == 0 && val.get_mbit(val.mbits-1)))
    {
        /* round up; if that carries out, increment the exponent */
        if (val.round_up())
            ++b;
    }

    /* if 'b' wound up above the maximum for the format, it's an overflow */
    if (b > val.emax)
    {
        val.set_infinity(get_neg(ext_));
        ov = TRUE;
        return;
    }

    /* store the final exponent and sign */
    val.set_exp(b);
    val.set_sign(get_neg(ext_));
}


/* ------------------------------------------------------------------------ */
/*
 *   Convert from IEEE 754-2008 format to BigNumber format.  This takes an
 *   IEEE 754-2008 binary interchange buffer in the given bit size (16, 32,
 *   64, or 128) and loads the value into the BigNumber.  The buffer is in
 *   the standard IEEE format, except that it's little endian, for
 *   consistency with the TADS pack/unpack formats.  To convert a buffer in
 *   the fully standard IEEE format, simply reverse the byte order before
 *   converting.  
 */
void CVmObjBigNum::set_ieee754_value(VMG_ const char *buf, int bits)
{
    /* set up the buffer descriptor */
    ieee754 val((char *)buf, bits);

    /* check for special values */
    if (val.is_zero())
    {
        set_zero(ext_);
        set_neg(ext_, val.get_sign());
        return;
    }
    else if (val.is_nan())
    {
        set_type(ext_, VMBN_T_NAN);
        return;
    }
    else if (val.is_infinity())
    {
        set_type(ext_, VMBN_T_INF);
        set_neg(ext_, val.get_sign());
        return;
    }

    /* 
     *   figure the precision for our intermediate calculations based on the
     *   precision of the source format in decimal digits 
     */
    int prec = val.decimal_digits() + 2;

    /* 
     *   Set up some registers for intermediate calculations.  We don't have
     *   to allocate these since we know the upper bound to the precision
     *   we'll need (namely 35+2 for the 128-bit type).  
     */
    char t1[5+37] = { (char)prec, 0, 0, 0, 0 };
    char t2buf[5+37] = { (char)prec, 0, 0, 0, 0 }, *t2 = t2buf;
    char t3buf[5+37] = { (char)prec, 0, 0, 0, 0 }, *t3 = t3buf;

    /*
     *   The IEEE value is represented as A * 2^B, for some non-zero value of
     *   A.  If the exponent is emin-1, A is represented as 0.bbbbb, where
     *   the b's are bits of the mantissa.  For any other exponent, A is
     *   represented as 1.bbbbb.  To compute our value, then, start with 2^B
     *   in register t1 and zero in the accumulator.  If the leading bit is 1
     *   (exponent >= emin), add r1 to the accumulator.  Divide t1 by 2 and
     *   loop: if the current bit is set, add t1 to the accumulator.  Repeat
     *   for each bit of the mantissa.  
     */
    
    /* calculate t1 = 2^B = e^(B*ln2) */
    const char *ln2 = cache_ln2(prec);
    int e = val.get_exp();
    set_int_val(t1, e);
    compute_prod_into(t2, t1, ln2);
    compute_exp_into(t1, t2);

    /* 
     *   If b > emin-1, there's an implied 1 bit for the whole part.  If b ==
     *   emin-1, we have a subnormal number, meaning that the implied lead
     *   bit is 0, and the actual exponent is val.emin.  
     */
    if (e > val.emin - 1)
    {
        /* normalized - add the implied leading "1" bit */
        copy_val(t3, t1, FALSE);
    }
    else
    {
        /* 
         *   subnormal - the implied leading bit is "0", but the actual
         *   exponent value is emin - one higher than it appears
         */
        set_zero(t3);
        mul_by_long(t1, 2);
    }

    /* add each bit of the mantissa */
    for (int i = 0 ; i < val.mbits ; ++i)
    {
        /* divide the power-of-two multiplier by 2 for the next lower place */
        div_by_long(t1, 2);

        /* if this mantissa bit is 1, add t1 to the accumulator */
        if (val.get_mbit(i))
        {
            /* t2 <- acc + power of two */
            compute_sum_into(t2, t3, t1);

            /* swap t2 and t3 so that t3 is the accumulator again */
            char *ttmp = t3; t3 = t2; t2 = ttmp;
        }
    }

    /* set the sign and normalize the value */
    set_neg(t3, val.get_sign());
    normalize(t3);

    /* copy the result into our extension */
    copy_val(ext_, t3, TRUE);
}



/* ------------------------------------------------------------------------ */
/*
 *   Create a string representation of the number
 */
const char *CVmObjBigNum::cast_to_string(VMG_ vm_obj_id_t self,
                                         vm_val_t *new_str) const
{
    /* use my static method */
    return cvt_to_string(vmg_ self, new_str, ext_, 100, -1, -1, -1, 0, 0);
}

/*
 *   Convert to string with a given radix 
 */
const char *CVmObjBigNum::explicit_to_string(
    VMG_ vm_obj_id_t self, vm_val_t *new_str, int radix, int flags) const
{
    /*
     *   If they want a non-decimal conversion, and we're a whole integer
     *   without a fractional part, generate the integer representation in
     *   the given radix.  If we have a fractional part, or the requested
     *   radix is decimal, use the floating-point formatter instead.
     *   
     *   Also use an integer conversion if we've been specfically asked for
     *   an integer conversion.  
     */
    if ((radix != 10 && is_frac_zero(ext_))
        || (flags & TOSTR_ROUND) != 0)
    {
        /* 
         *   it's all integer, and we have a non-decimal radix - format as an
         *   integer 
         */
        return cvt_to_string_in_radix(vmg_ self, new_str, radix);
    }
    else
    {   
        /* 
         *   no radix, or we have a fraction - use the basic floating-point
         *   formatter with default options 
         */
        return cvt_to_string(vmg_ self, new_str, ext_, 100, -1, -1, -1, 0, 0);
    }
}

/* ------------------------------------------------------------------------ */
/*
 *   String buffer allocator 
 */
char *CBigNumStringBufAlo::get_buf(size_t need)
{
    /* use the fixed buffer if it's big enough */
    if (len >= need)
        return buf;

    /* the fixed buffer isn't big enough, so allocate a new string */
    if (strval != 0)
    {
        VMGLOB_PTR(vmg);
        return CVmObjString::alloc_str_buf(vmg_ strval, 0, 0, need);
    }

    /* the buffer's too small, and we can't allocate, so fail */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   convert to a string, storing the result in the given buffer if it'll
 *   fit, otherwise in a new string 
 */
const char *CVmObjBigNum::cvt_to_string_buf(
    VMG_ vm_val_t *new_str, char *buf, size_t buflen,
    int max_digits, int whole_places, int frac_digits, int exp_digits,
    ulong flags)
{
    /* set up an allocator */
    CBigNumStringBufAlo alo(vmg_ new_str, buf, buflen);

    /* convert to a string into our buffer */
    return cvt_to_string_gen(
        &alo, ext_, max_digits, whole_places, frac_digits, exp_digits,
        flags, 0, 0);
}

/* ------------------------------------------------------------------------ */
/*
 *   Convert to a string, creating a new string object to hold the result 
 */
const char *CVmObjBigNum::cvt_to_string(
    VMG_ vm_obj_id_t self, vm_val_t *new_str, const char *ext,
    int max_digits, int whole_places, int frac_digits, int exp_digits,
    ulong flags, vm_val_t *lead_fill)
{
    /* push a self-reference for gc protection during allocation */
    G_stk->push()->set_obj(self);

    /* set up an allocator */
    CBigNumStringBufAlo alo(vmg_ new_str);

    /* if there's a lead fill parameter, get its string value */
    const char *lead_fill_str = 0;
    size_t lead_fill_len = 0;
    if (lead_fill != 0 && lead_fill->typ != VM_NIL)
    {
        /* get the string value */
        lead_fill_str = lead_fill->get_as_string(vmg0_);

        /* if it's not a string, it's an error */
        if (lead_fill_str == 0)
            err_throw(VMERR_STRING_VAL_REQD);

        /* read and skip the length prefix */
        lead_fill_len = vmb_get_len(lead_fill_str);
        lead_fill_str += VMB_LEN;

        /* if the length is zero, ignore the lead fill string entirely */
        if (lead_fill_len == 0)
            lead_fill_str = 0;
    }

    /* 
     *   convert to a string - don't pass in a buffer, since we want to
     *   create a new string to hold the result
     */
    const char *ret = cvt_to_string_gen(
        &alo, ext, max_digits, whole_places, frac_digits, exp_digits,
        flags, lead_fill_str, lead_fill_len);

    /* discard our gc protection */
    G_stk->discard();

    /* return the result */
    return ret;
}

/* ------------------------------------------------------------------------ */
/*
 *   Convert an integer value to a string in the given radix (2-36)
 */
const char *CVmObjBigNum::cvt_to_string_in_radix(
    VMG_ vm_obj_id_t self, vm_val_t *new_str, int radix) const
{
    /* check for special values: zeros, NaNs, and infinities */
    const char *fixed = 0;
    int neg = get_neg(ext_);
    if (is_zero(ext_))
    {
        /* show zero as "0" in all bases */
        fixed = "0";
    }
    else if (is_infinity(ext_))
    {
        /* 
         *   show infinity as "1.#INF" for positive or unsigned, "-1.#INF"
         *   for negative infinity 
         */
        fixed = (neg ? "-1.#INF" : "1.#INF");
    }
    else if (is_nan(ext_))
    {
        /* not a number */
        fixed = "1.#NAN";
    }
    else
    {
        /* make a temporary copy of the number */
        uint thdl;
        char *tmp = alloc_temp_reg(get_prec(ext_), &thdl);
        err_try
        {
            /* copy the value into our temporary register */
            copy_val(tmp, ext_, FALSE);

            /* if there's a fractional part, round to integer */
            round_to_int(tmp);

            /* 
             *   It's an ordinary non-zero number.  Figure how many digits
             *   the integer portion will take in the given radix.  This is
             *   fairly simple: a D digit number in decimal requires
             *   D/log10(R) digits in base R, rounding up.  Our storage
             *   format makes it easy to determine D - it's simply our
             *   base-10 exponent.  
             */
            int len = (int)ceil(get_exp(ext_) / log10((double)radix));

            /* if the value is zero, we need one digit for the zero */
            if (len == 0)
                len = 1;
            
            /* if it's negative, add space for the '-' */
            if (neg)
                len += 1;
            
            /* create the string */
            new_str->set_obj(CVmObjString::create(vmg_ FALSE, len));
            CVmObjString *str = (CVmObjString *)vm_objp(vmg_ new_str->val.obj);
            char *buf = str->cons_get_buf(), *p = buf;
            
            /* add the '-' sign if negative */
            if (neg)
            {
                *p++ = '-';
                ++buf;
                --len;
            }
            
            /* generate the digits */
            while (!is_zero(tmp) && p - buf < len)
            {
                /* get the next digit */
                unsigned long rem;
                div_by_long(tmp, radix, &rem);

                /* add it to the output */
                *p++ = (char)(rem < 10 ? rem + '0' : rem - 10 + 'A');
            }
            
            /* 
             *   We just generated the digits in reverse order.  Reverse the
             *   string so that they're in the right order. 
             */
            char *p1, *p2, tmp;
            for (p1 = buf, p2 = p - 1 ; p2 > p1 ; --p2, ++p1)
                tmp = *p1, *p1 = *p2, *p2 = tmp;

            /* if we generated no digits, generate a zero */
            if (p == buf)
                *p++ = '0';

            /* set the final length, in case rounding left us off by one */
            str->cons_set_len(p - str->cons_get_buf());
        }
        err_finally
        {
            /* release our temporary register */
            release_temp_reg(thdl);
        }
        err_end;
    }

    /* if we came up with a fixed source string, create the string object */
    if (fixed != 0)
        new_str->set_obj(CVmObjString::create(
            vmg_ FALSE, fixed, strlen(fixed)));

    /* return the new string buffer */
    return new_str->get_as_string(vmg0_);
}

/* ------------------------------------------------------------------------ */
/* 
 *   save to a file 
 */
void CVmObjBigNum::save_to_file(VMG_ class CVmFile *fp)
{
    size_t prec;
    
    /* get our precision */
    prec = get_prec(ext_);

    /* write the data */
    fp->write_bytes(ext_, calc_alloc(prec));
}

/* 
 *   restore from a file 
 */
void CVmObjBigNum::restore_from_file(VMG_ vm_obj_id_t,
                                     CVmFile *fp, CVmObjFixup *)
{
    size_t prec;
    
    /* read the precision */
    prec = fp->read_uint2();

    /* free any existing extension */
    if (ext_ != 0)
    {
        G_mem->get_var_heap()->free_mem(ext_);
        ext_ = 0;
    }

    /* allocate the space */
    alloc_bignum(vmg_ prec);

    /* store our precision */
    set_prec(ext_, prec);

    /* read the contents */
    fp->read_bytes(ext_ + VMBN_EXP, calc_alloc(prec) - VMBN_EXP);
}


/* ------------------------------------------------------------------------ */
/* 
 *   set a property 
 */
void CVmObjBigNum::set_prop(VMG_ class CVmUndo *,
                            vm_obj_id_t, vm_prop_id_t,
                            const vm_val_t *)
{
    /* we have no properties to set */
    err_throw(VMERR_INVALID_SETPROP);
}

/*
 *   get a static property 
 */
int CVmObjBigNum::call_stat_prop(VMG_ vm_val_t *result, const uchar **pc_ptr,
                                 uint *argc, vm_prop_id_t prop)
{
    /* translate the property into a function vector index */
    switch(G_meta_table
           ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop))
    {
    case VMOBJBN_GET_PI:
        return s_getp_pi(vmg_ result, argc);

    case VMOBJBN_GET_E:
        return s_getp_e(vmg_ result, argc);

    default:
        /* 
         *   we don't define this one - inherit the default from the base
         *   object metaclass 
         */
        return CVmObject::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
}

/* 
 *   get a property 
 */
int CVmObjBigNum::get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                           vm_obj_id_t self, vm_obj_id_t *source_obj,
                           uint *argc)
{
    uint func_idx;
    
    /* translate the property into a function vector index */
    func_idx = G_meta_table
               ->prop_to_vector_idx(metaclass_reg_->get_reg_idx(), prop);

    /* call the function */
    if ((this->*func_table_[func_idx])(vmg_ self, val, argc))
    {
        *source_obj = metaclass_reg_->get_class_obj(vmg0_);
        return TRUE;
    }

    /* inherit default handling */
    return CVmObject::get_prop(vmg_ prop, val, self, source_obj, argc);
}

/*
 *   Property evaluator - formatString 
 */
int CVmObjBigNum::getp_format(VMG_ vm_obj_id_t self,
                              vm_val_t *retval, uint *argc)
{
    int orig_argc = (argc != 0 ? *argc : 0);
    int max_digits = -1;
    uint flags = 0;
    int whole_places = -1;
    int frac_digits = -1;
    int exp_digits = -1;
    vm_val_t *lead_fill = 0;
    static CVmNativeCodeDesc desc(0, 6);
    
    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the maximum digit count */
    if (orig_argc >= 1)
        max_digits = CVmBif::pop_int_or_nil(vmg_ -1);

    /* get the flags */
    if (orig_argc >= 2)
        flags = CVmBif::pop_int_or_nil(vmg_ 0);

    /* get the whole places */
    if (orig_argc >= 3)
        whole_places = CVmBif::pop_int_or_nil(vmg_ -1);

    /* get the fraction digits */
    if (orig_argc >= 4)
        frac_digits = CVmBif::pop_int_or_nil(vmg_ -1);

    /* get the exponent digits */
    if (orig_argc >= 5)
        exp_digits = CVmBif::pop_int_or_nil(vmg_ -1);

    /* 
     *   get the lead fill string if provided (leave it on the stack to
     *   protect against garbage collection) 
     */
    if (orig_argc >= 6)
        lead_fill = G_stk->get(0);

    /* format the number, which will build the return string */
    cvt_to_string(vmg_ self, retval, ext_, max_digits, whole_places,
                  frac_digits, exp_digits, flags, lead_fill);

    /* discard the lead fill string, if provided */
    if (lead_fill != 0)
        G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   Property eval - equal after rounding
 */
int CVmObjBigNum::getp_equal_rnd(VMG_ vm_obj_id_t self,
                                 vm_val_t *retval, uint *argc)
{
    vm_val_t val2;
    static CVmNativeCodeDesc desc(1);
    
    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* pop the value to compare */
    G_stk->pop(&val2);

    /* convert it to BigNumber */
    if (!cvt_to_bignum(vmg_ self, &val2))
    {
        /* it's not a BigNumber, so it's not equal */
        retval->set_nil();
    }
    else
    {
        int ret;
        
        /* test for equality */
        ret = compute_eq_round(vmg_ ext_, get_objid_ext(vmg_ val2.val.obj));

        /* set the return value */
        retval->set_logical(ret);
    }

    /* handled */
    return TRUE;
}

/*
 *   property eval - get the precision 
 */
int CVmObjBigNum::getp_get_prec(VMG_ vm_obj_id_t self,
                                vm_val_t *retval, uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* return an integer giving my precision */
    retval->set_int(get_prec(ext_));

    /* handled */
    return TRUE;
}

/*
 *   property eval - set the precision 
 */
int CVmObjBigNum::getp_set_prec(VMG_ vm_obj_id_t self,
                                vm_val_t *retval, uint *argc)
{
    size_t digits;
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the number of digits for rounding */
    digits = CVmBif::pop_int_val(vmg0_);

    /* if I'm not a number, return myself unchanged */
    if (is_nan(ext_))
    {
        retval->set_obj(self);
        return TRUE;
    }

    /* push a self-reference while we're working */
    G_stk->push()->set_obj(self);

    /* create the rounded value */
    round_val(vmg_ retval, ext_, digits, TRUE);

    /* remove my self-reference */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   get pi (static method)
 */
int CVmObjBigNum::s_getp_pi(VMG_ vm_val_t *val, uint *argc)
{
    size_t prec;
    char *new_ext;
    const char *pi;
    static CVmNativeCodeDesc desc(1);
    
    /* check arguments */
    if (get_prop_check_argc(val, argc, &desc))
        return TRUE;

    /* get the precision argument */
    prec = CVmBif::pop_int_val(vmg0_);

    /* allocate the result */
    val->set_obj(create(vmg_ FALSE, prec));
    new_ext = get_objid_ext(vmg_ val->val.obj);

    /* cache pi to the required precision */
    pi = cache_pi(prec);

    /* return the value */
    copy_val(new_ext, pi, TRUE);

    /* handled */
    return TRUE;
}

/*
 *   get e (static method)
 */
int CVmObjBigNum::s_getp_e(VMG_ vm_val_t *val, uint *argc)
{
    size_t prec;
    char *new_ext;
    const char *e;
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(val, argc, &desc))
        return TRUE;

    /* get the precision argument */
    prec = CVmBif::pop_int_val(vmg0_);

    /* allocate the result */
    val->set_obj(create(vmg_ FALSE, prec));
    new_ext = get_objid_ext(vmg_ val->val.obj);

    /* cache e to the required precision */
    e = cache_e(prec);

    /* return the value */
    copy_val(new_ext, e, TRUE);

    /* handled */
    return TRUE;
}

/*
 *   Set up for a zero-argument operation that returns a BigNumber result.
 *   Returns true if the argument check indicates that the caller should
 *   simply return to its caller, false if the caller should proceed.
 *   
 *   After checking the argument count, we'll proceed to set up the return
 *   value as per setup_getp_retval().  
 */
int CVmObjBigNum::setup_getp_0(VMG_ vm_obj_id_t self, vm_val_t *retval,
                               uint *argc, char **new_ext)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* set up the return value */
    return setup_getp_retval(vmg_ self, retval, new_ext, get_prec(ext_));
}

/*
 *   Set up for a one-argument operation that takes a BigNumber value as
 *   the argument and returns a BigNumber result.  Does the work of
 *   setup_getp_0, but also pops the argument value and converts it to a
 *   BigNumber (throwing an error if the value is not convertible).
 *   
 *   Fills in val2 with the argument value, and fills in *ext2 with val2's
 *   extension buffer.
 *   
 *   The result value will have the larger of the precisions of self and
 *   the other value, unless use_self_prec is set, in which case we'll use
 *   self's precision unconditionally.  
 *   
 *   If either argument is not a number, we'll set the return value to the
 *   not-a-number argument unchanged, and return true.  
 */
int CVmObjBigNum::setup_getp_1(VMG_ vm_obj_id_t self,
                               vm_val_t *retval, uint *argc,
                               char **new_ext,
                               vm_val_t *val2, const char **ext2,
                               int use_self_prec)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(1);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* pop the argument value */
    G_stk->pop(val2);

    /* convert it to BigNumber */
    if (!cvt_to_bignum(vmg_ self, val2))
    {
        /* it's not a BigNumber - throw an error */
        err_throw(VMERR_BAD_TYPE_BIF);
    }

    /* get the other value's extension */
    *ext2 = get_objid_ext(vmg_ val2->val.obj);

    /* if the other value is not a number, return it as the result */
    if (is_nan(*ext2))
    {
        retval->set_obj(val2->val.obj);
        return TRUE;
    }

    /* 
     *   if val2's precision is higher than ours, use it, unless we've
     *   been specifically told to use our own precision for the result 
     */
    size_t prec = get_prec(ext_);
    if (!use_self_prec && get_prec(*ext2) > prec)
        prec = get_prec(*ext2);

    /* push the other result to protect it from garbage collection */
    G_stk->push(val2);

    /* allocate the return value */
    if (setup_getp_retval(vmg_ self, retval, new_ext, prec))
    {
        /* discard the val2 reference we pushed for gc protection */
        G_stk->discard();

        /* tell the caller we're done */
        return TRUE;
    }

    /* tell the caller to proceed */
    return FALSE;
}

/*
 *   Set up for an operation that returns a BigNumber result.  Returns
 *   true if we finished the operation, in which case the caller should
 *   simply return; returns false if the operation should still be carried
 *   out, in which case the caller should proceed as normal.
 *   
 *   If the 'self' value is not-a-number, we'll return it as the result
 *   (and return true to indicate that no further processing is required).
 *   
 *   If we return false, we'll have pushed a reference to 'self' onto the
 *   stack for protection against garbage collection.  The caller must
 *   discard this reference before returning.  We push no such
 *   self-reference if we return true.
 *   
 *   In addition, if we return false, we'll fill in '*new_ext' with a
 *   pointer to the extension buffer for the return value object that we
 *   allocate.  We'll allocate the return value with the same precision as
 *   'self'.
 *   
 *   Note that the caller should ensure that any argument sare removed
 *   from the stack before calling this routine, since we might push the
 *   self-reference onto the stack.
 */
int CVmObjBigNum::setup_getp_retval(VMG_ vm_obj_id_t self,
                                    vm_val_t *retval, char **new_ext,
                                    size_t prec)
{
    /* if I'm not a number, return myself unchanged */
    if (is_nan(ext_))
    {
        retval->set_obj(self);
        return TRUE;
    }

    /* push a self-reference while we're working */
    G_stk->push()->set_obj(self);

    /* create a new number with the same precision as the original */
    retval->set_obj(create(vmg_ FALSE, prec));
    *new_ext = get_objid_ext(vmg_ retval->val.obj);

    /* tell the caller to proceed */
    return FALSE;
}

/*
 *   property eval - get the fractional part
 */
int CVmObjBigNum::getp_frac(VMG_ vm_obj_id_t self,
                            vm_val_t *retval, uint *argc)
{
    char *new_ext;

    /* check arguments and allocate the result value */
    if (setup_getp_0(vmg_ self, retval, argc, &new_ext))
        return TRUE;
    
    /* make a copy in the new object, and get the fractional portion */
    memcpy(new_ext, ext_, calc_alloc(get_prec(ext_)));
    compute_frac(new_ext);
    
    /* remove my self-reference */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   property eval - get the whole part, with no rounding
 */
int CVmObjBigNum::getp_whole(VMG_ vm_obj_id_t self,
                             vm_val_t *retval, uint *argc)
{
    char *new_ext;

    /* check arguments and allocate the result value */
    if (setup_getp_0(vmg_ self, retval, argc, &new_ext))
        return TRUE;

    /* make a copy in the new object, and get the whole part */
    memcpy(new_ext, ext_, calc_alloc(get_prec(ext_)));
    compute_whole(new_ext);

    /* remove my self-reference */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   property eval - round to a given number of decimal places
 */
int CVmObjBigNum::getp_round_dec(VMG_ vm_obj_id_t self,
                                 vm_val_t *retval, uint *argc)
{
    int places;
    char *new_ext;
    int post_idx;
    int exp = get_exp(ext_);
    size_t prec = get_prec(ext_);
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the number of digits for rounding */
    places = CVmBif::pop_int_val(vmg0_);

    /* set up the return value */
    if (setup_getp_retval(vmg_ self, retval, &new_ext, prec))
        return TRUE;

    /* make a copy in the new object */
    memcpy(new_ext, ext_, calc_alloc(prec));

    /* 
     *   Determine if the first digit we're lopping off is actually
     *   represented in the number or not.  This digit position is the
     *   exponent plus the number of decimal places after the decimal to
     *   keep - if this value is at least zero and less than the
     *   precision, it's part of the number.  
     */
    post_idx = places + exp;
    if (post_idx < 0)
    {
        /* 
         *   the digit after the last digit to keep is actually before the
         *   beginning of the number, so the result of the rounding is
         *   simply zero 
         */
        set_zero(new_ext);
    }
    else if (post_idx >= (int)prec)
    {
        /* 
         *   the digit after the last digit to keep is past the end of the
         *   represented digits, so rounding has no effect at all - we'll
         *   simply return the same number 
         */
    }
    else
    {
        /* round it */
        round_to(new_ext, post_idx);

        /* normalize the result */
        normalize(new_ext);
    }

    /* remove my self-reference */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   property eval - get the absolute value
 */
int CVmObjBigNum::getp_abs(VMG_ vm_obj_id_t self,
                           vm_val_t *retval, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* compute the absolute value */
    abs_val(vmg_ retval, self);

    /* handled */
    return TRUE;
}

/*
 *   absolute value 
 */
int CVmObjBigNum::abs_val(VMG_ vm_val_t *retval, vm_obj_id_t self)
{
    /* 
     *   If I'm not an ordinary number or an infinity, or I'm already
     *   non-negative, return myself unchanged.  Note that we change negative
     *   infinity to infinity, even though this might not make a great deal
     *   of sense mathematically.  
     */
    if (!get_neg(ext_)
        || (get_type(ext_) != VMBN_T_NUM && get_type(ext_) != VMBN_T_INF))
    {
        retval->set_obj(self);
        return TRUE;
    }

    /* push a self-reference while we're working */
    G_stk->push()->set_obj(self);

    /* 
     *   if I'm negative infinity, we don't need any precision in the new
     *   value; otherwise use my original precision
     */
    size_t prec = (is_infinity(ext_) ? 1 : get_prec(ext_));

    /* create a new number with the same precision as the original */
    retval->set_obj(create(vmg_ FALSE, prec));
    char *new_ext = get_objid_ext(vmg_ retval->val.obj);

    /* make a copy in the new object */
    memcpy(new_ext, ext_, calc_alloc(prec));

    /* set the sign to positive */
    set_neg(new_ext, FALSE);

    /* remove my self-reference */
    G_stk->discard();

    /* success */
    return TRUE;
}

/*
 *   compute the sgn value
 */
int CVmObjBigNum::sgn_val(VMG_ vm_val_t *retval, vm_obj_id_t self)
{
    /* figure the sgn value */
    if (is_zero(ext_))
        retval->set_int(0);
    else if (get_neg(ext_))
        retval->set_int(-1);
    else
        retval->set_int(1);

    /* success */
    return TRUE;
}

/*
 *   property eval - ceiling (least integer greater than or equal to self)
 */
int CVmObjBigNum::getp_ceil(VMG_ vm_obj_id_t self,
                            vm_val_t *retval, uint *argc)
{
    char *new_ext;
    size_t idx;
    int exp = get_exp(ext_);
    size_t prec = get_prec(ext_);
    int frac_zero;
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* check arguments and allocate the result value */
    if (setup_getp_0(vmg_ self, retval, argc, &new_ext))
        return TRUE;

    /* make a copy in the new object */
    memcpy(new_ext, ext_, calc_alloc(prec));

    /* determine if the fractional part is non-zero */
    frac_zero = is_frac_zero(new_ext);

    /* check what we have */
    if (exp <= 0)
    {
        /* 
         *   it's an entirely fractional number - the result is zero if
         *   the number is negative or zero, one if the number is positive 
         */
        if (get_neg(new_ext) || frac_zero)
        {
            /* -1 < x <= 0 -> ceil(x) = 0 */
            set_zero(new_ext);
        }
        else
        {
            /* 0 < x < 1 -> ceil(x) = 1 */
            copy_val(new_ext, get_one(), FALSE);
        }
    }
    else
    {
        /* clear digits after the decimal point */
        for (idx = (size_t)exp ; idx < prec ; ++idx)
            set_dig(new_ext, idx, 0);

        /* 
         *   if there's a fractional part and it's positive, increment the
         *   number 
         */
        if (!frac_zero && !get_neg(new_ext))
            increment_abs(new_ext);
    }

    /* remove my self-reference */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   property eval - floor (greatest integer <= self) 
 */
int CVmObjBigNum::getp_floor(VMG_ vm_obj_id_t self,
                             vm_val_t *retval, uint *argc)
{
    char *new_ext;
    size_t idx;
    int exp = get_exp(ext_);
    size_t prec = get_prec(ext_);
    int frac_zero;

    /* check arguments and allocate the result value */
    if (setup_getp_0(vmg_ self, retval, argc, &new_ext))
        return TRUE;

    /* make a copy in the new object */
    memcpy(new_ext, ext_, calc_alloc(prec));

    /* determine if the fractional part is non-zero */
    frac_zero = is_frac_zero(new_ext);

    /* check what we have */
    if (exp <= 0)
    {
        /* 
         *   it's an entirely fractional number - the result is zero if
         *   the number is positive or zero, -1 if the number is negative 
         */
        if (!get_neg(new_ext) || frac_zero)
        {
            /* 0 <= x < 1 -> floor(x) = 0 */
            set_zero(new_ext);
        }
        else
        {
            /* -1 < x < 0 -> floor(x) = -1 */
            copy_val(new_ext, get_one(), FALSE);
            set_neg(new_ext, TRUE);
        }
    }
    else
    {
        /* clear digits after the decimal point */
        for (idx = (size_t)exp ; idx < prec ; ++idx)
            set_dig(new_ext, idx, 0);

        /* 
         *   if there's a fractional part and the number is negative,
         *   increment the number's absolute value 
         */
        if (!frac_zero && get_neg(new_ext))
            increment_abs(new_ext);
    }

    /* remove my self-reference */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   property eval - getScale
 */
int CVmObjBigNum::getp_get_scale(VMG_ vm_obj_id_t self,
                                 vm_val_t *retval, uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* check the type */
    if (is_nan(ext_))
    {
        /* it's not a number - return nil for the scale */
        retval->set_nil();
    }
    else
    {
        /* return an integer giving the number's scale */
        retval->set_int(get_exp(ext_));
    }
    
    /* handled */
    return TRUE;
}

/*
 *   property eval - scale
 */
int CVmObjBigNum::getp_scale(VMG_ vm_obj_id_t self,
                             vm_val_t *retval, uint *argc)
{
    char *new_ext;
    size_t prec = get_prec(ext_);
    int scale;
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the scaling argument */
    scale = CVmBif::pop_int_val(vmg0_);

    /* set up the return value */
    if (setup_getp_retval(vmg_ self, retval, &new_ext, prec))
        return TRUE;
    
    /* copy the value */
    memcpy(new_ext, ext_, calc_alloc(prec));

    /* adjust the exponent by the scale factor */
    set_exp(new_ext, get_exp(new_ext) + scale);

    /* discard the GC protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   property eval - negate
 */
int CVmObjBigNum::getp_negate(VMG_ vm_obj_id_t self,
                              vm_val_t *retval, uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* negate the value */
    neg_val(vmg_ retval, self);

    /* handled */
    return TRUE;
}

/*
 *   property eval - copy sign
 */
int CVmObjBigNum::getp_copy_sign(VMG_ vm_obj_id_t self,
                                 vm_val_t *retval, uint *argc)
{
    vm_val_t val2;
    char *new_ext;
    const char *ext2;
    size_t prec = get_prec(ext_);

    if (setup_getp_1(vmg_ self, retval, argc, &new_ext,
                     &val2, &ext2, TRUE))
        return TRUE;

    /* make a copy of my value in the new object */
    memcpy(new_ext, ext_, calc_alloc(prec));

    /* set the sign from the other object */
    set_neg(new_ext, get_neg(ext2));

    /* 
     *   normalize it (this is important when the value was zero to start
     *   with, since zero is always represented without a negative sign) 
     */
    normalize(new_ext);

    /* remove the GC protection */
    G_stk->discard(2);

    /* handled */
    return TRUE;
}

/*
 *   property eval - isNegative
 */
int CVmObjBigNum::getp_is_neg(VMG_ vm_obj_id_t self,
                              vm_val_t *retval, uint *argc)
{
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* if I'm not an ordinary number or an infinity, I'm not negative */
    if (get_type(ext_) != VMBN_T_NUM && get_type(ext_) != VMBN_T_INF)
    {
        /* I'm not negative, so return nil */
        retval->set_nil();
    }
    else
    {
        /* set the return value to true if I'm negative, nil if not */
        retval->set_logical(get_neg(ext_));
    }

    /* handled */
    return TRUE;
}

/*
 *   property eval - remainder
 */
int CVmObjBigNum::getp_remainder(VMG_ vm_obj_id_t self,
                                 vm_val_t *retval, uint *argc)
{
    vm_val_t val2;
    const char *ext2;
    char *quo_ext;
    char *rem_ext;
    vm_val_t rem_val;
    vm_val_t quo_val;
    CVmObjList *lst;
    static CVmNativeCodeDesc desc(1);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* pop the divisor */
    G_stk->pop(&val2);

    /* convert it to BigNumber */
    if (!cvt_to_bignum(vmg_ self, &val2))
    {
        /* it's not a BigNumber - throw an error */
        err_throw(VMERR_BAD_TYPE_BIF);
    }

    /* get the divisor's extension */
    ext2 = get_objid_ext(vmg_ val2.val.obj);

    /* push 'self' and the other value to protect against GC */
    G_stk->push()->set_obj(self);
    G_stk->push(&val2);

    /* create a quotient result value, and push it for safekeeping */
    quo_ext = compute_init_2op(vmg_ &quo_val, ext_, ext2);
    G_stk->push(&quo_val);

    /* create a remainder result value, and push it for safekeeping */
    rem_ext = compute_init_2op(vmg_ &rem_val, ext_, ext2);
    G_stk->push(&rem_val);

    /* 
     *   create a list for the return value - it will have two elements:
     *   the quotient and the remainder 
     */
    retval->set_obj(CVmObjList::create(vmg_ FALSE, 2));
    lst = (CVmObjList *)vm_objp(vmg_ retval->val.obj);

    /* set the list elements */
    lst->cons_set_element(0, &quo_val);
    lst->cons_set_element(1, &rem_val);

    /* calculate the quotient */
    compute_quotient_into(quo_ext, rem_ext, ext_, ext2);

    /* remove the GC protection */
    G_stk->discard(4);

    /* handled */
    return TRUE;
}

/*
 *   property eval - sine
 */
int CVmObjBigNum::getp_sin(VMG_ vm_obj_id_t self,
                           vm_val_t *retval, uint *argc)
{
    char *new_ext;
    size_t prec = get_prec(ext_);
    uint hdl1, hdl2, hdl3, hdl4, hdl5, hdl6, hdl7;
    char *ext1, *ext2, *ext3, *ext4, *ext5, *ext6, *ext7;
    int neg_result;
    const char *pi;

    /* check arguments and set up the result */
    if (setup_getp_0(vmg_ self, retval, argc, &new_ext))
        return TRUE;

    /* cache pi */
    pi = cache_pi(prec + 3);

    /* 
     *   Allocate our temporary registers.  We'll use 1 and 2 to calculate
     *   x^n - we'll start with x^(n-2) in one, and multiply by x^2 to put
     *   the result in the other.  3 we'll use to store n!.  4 we'll use
     *   to store the result of x^n/n!, and 5 and 6 we'll swap as the
     *   master accumulator.  7 we'll use to store x^2.
     *   
     *   Allocate the temporary registers with more digits of precision
     *   than we need in the result, to ensure that accumulated rounding
     *   errors don't affect the result.  
     */
    alloc_temp_regs(prec + 3, 7,
                    &ext1, &hdl1, &ext2, &hdl2, &ext3, &hdl3,
                    &ext4, &hdl4, &ext5, &hdl5, &ext6, &hdl6, &ext7, &hdl7);

    /* protect against errors so we definitely free the registers */
    err_try
    {
        char *tmp;

        /* copy our initial value to ext1 */
        copy_val(ext1, ext_, FALSE);

        /*
         *   Note that sin(-x) = -sin(x).  If x < 0, note the negative
         *   sign and then negate the value so that we calculate sin(x)
         *   and return the negative of the result.  
         */
        neg_result = get_neg(ext1);
        set_neg(ext1, FALSE);

        /* calculate 2*pi */
        copy_val(ext7, pi, TRUE);
        mul_by_long(ext7, 2);

        /*
         *   Because we'll use a Taylor series around 0 to calculate the
         *   result, we want our value as close to the expansion point (0)
         *   as possible to speed up convergence of the series.
         *   Fortunately this is especially easy for sin() because of the
         *   periodic nature of the function.
         *   
         *   First, note that sin(2*pi*i + x) = sin(x) for all integers i,
         *   so we can reduce the argument mod 2*pi until it's in the
         *   range 0 <= x < 2*pi (we might have to do this multiple times
         *   if the number's scale exceeds its precision).  Note that we
         *   already made sure the number is positive.  
         */
        while (compare_abs(ext1, ext7) > 0)
        {
            /* divide by 2*pi, storing the remainder in r2 */
            compute_quotient_into(ext6, ext2, ext1, ext7);

            /* swap r2 into r1 for the next round */
            tmp = ext1;
            ext1 = ext2;
            ext2 = tmp;
        }

        /*
         *   Next, note that sin(x+pi) = -sin(x).  If x > pi, negate the
         *   result (again if necessary) and reduce the argument by pi.
         *   This will reduce our range to 0 <= x <= pi.  
         */
        copy_val(ext7, pi, TRUE);
        if (compare_abs(ext1, ext7) > 0)
        {
            /* negate the result */
            neg_result = !neg_result;

            /* subtract pi from the argument */
            compute_abs_diff_into(ext2, ext1, ext7);

            /* swap the result into r1 */
            tmp = ext1;
            ext1 = ext2;
            ext2 = tmp;
        }

        /*
         *   Use the fact that sin(x + pi) = -sin(x) once again: if x >
         *   pi/2, subtract pi from x to adjust the range to -pi/2 <= x <=
         *   pi/2.  
         */
        div_by_long(ext7, 2);
        if (compare_abs(ext1, ext7) > 0)
        {
            /* negate the result */
            neg_result = !neg_result;
            
            /* subtract pi from the argument */
            copy_val(ext7, pi, TRUE);
            compute_abs_diff_into(ext2, ext1, ext7);

            /* swap the result into r1 */
            tmp = ext1;
            ext1 = ext2;
            ext2 = tmp;
        }

        /* 
         *   once again, reduce our range using the sign equivalence -
         *   this will limit our range to 0 <= x <= pi/2 
         */
        if (get_neg(ext1))
            neg_result = !neg_result;
        set_neg(ext1, FALSE);

        /*
         *   Next, observe that sin(x+pi/2) = cos(x).  If x > pi/4,
         *   calculate using the cosine series instead of the sine series.
         */
        copy_val(ext7, pi, TRUE);
        div_by_long(ext7, 4);
        if (compare_abs(ext1, ext7) > 0)
        {
            /* calculate pi/2 */
            copy_val(ext7, pi, TRUE);
            div_by_long(ext7, 2);
            
            /* 
             *   subtract pi/2 - this will give us a value in the range
             *   -pi/4 <= x <= pi/4 
             */
            compute_abs_diff_into(ext2, ext1, ext7);

            /* cos(-x) = cos(x), so we can ignore the sign */
            set_neg(ext1, FALSE);

            /* swap the result into r1 */
            tmp = ext1;
            ext1 = ext2;
            ext2 = tmp;

            /* calculate the cosine series */
            calc_cos_series(vmg_ new_ext,
                            ext1, ext2, ext3, ext4, ext5, ext6, ext7);
        }
        else
        {
            /*
             *   We now have a value in the range 0 <= x <= pi/4, which
             *   will converge quickly with our Taylor series 
             */
            calc_sin_series(vmg_ new_ext,
                            ext1, ext2, ext3, ext4, ext5, ext6, ext7);
        }

        /* negate the result if necessary */
        if (neg_result)
            set_neg(new_ext, !get_neg(new_ext));

        /* normalize the result */
        normalize(new_ext);
    }
    err_finally
    {
        /* release our temporary registers */
        release_temp_regs(7, hdl1, hdl2, hdl3, hdl4, hdl5, hdl6, hdl7);
    }
    err_end;

    /* remove my self-reference */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   property eval - cosine.  This works very much the same way as
 *   getp_sin() - refer to the more extensive comments in that routine for
 *   more detail on the algorithm.  
 */
int CVmObjBigNum::getp_cos(VMG_ vm_obj_id_t self,
                           vm_val_t *retval, uint *argc)
{
    char *new_ext;
    size_t prec = get_prec(ext_);
    uint hdl1, hdl2, hdl3, hdl4, hdl5, hdl6, hdl7;
    char *ext1, *ext2, *ext3, *ext4, *ext5, *ext6, *ext7;
    int neg_result;
    const char *pi;

    /* check arguments and set up the result */
    if (setup_getp_0(vmg_ self, retval, argc, &new_ext))
        return TRUE;

    /* cache pi to our working precision */
    pi = cache_pi(prec + 3);

    /* allocate our temporary registers, as per sin() */
    alloc_temp_regs(prec + 3, 7,
                    &ext1, &hdl1, &ext2, &hdl2, &ext3, &hdl3,
                    &ext4, &hdl4, &ext5, &hdl5, &ext6, &hdl6, &ext7, &hdl7);

    /* protect against errors so we definitely free the registers */
    err_try
    {
        char *tmp;

        /* presume the result sign will be correct */
        neg_result = FALSE;

        /* copy our initial value to ext1 */
        copy_val(ext1, ext_, FALSE);

        /* note that cos(-x) = cos(x) - if x < 0, use -x */
        set_neg(ext1, FALSE);

        /* reduce the argument modulo 2*pi */
        copy_val(ext7, pi, TRUE);
        mul_by_long(ext7, 2);
        while (compare_abs(ext1, ext7) > 0)
        {
            /* divide by 2*pi, storing the remainder in r2 */
            compute_quotient_into(ext6, ext2, ext1, ext7);

            /* swap r2 into r1 for the next round */
            tmp = ext1;
            ext1 = ext2;
            ext2 = tmp;
        }

        /* 
         *   Next, note that cos(x+pi) = -cos(x).  If x > pi, negate the
         *   result (again if necessary) and reduce the argument by pi.
         *   This will reduce our range to 0 <= x <= pi.  
         */
        copy_val(ext7, pi, TRUE);
        if (compare_abs(ext1, ext7) > 0)
        {
            /* negate the result */
            neg_result = !neg_result;

            /* subtract pi from the argument */
            compute_abs_diff_into(ext2, ext1, ext7);

            /* swap the result into r1 */
            tmp = ext1;
            ext1 = ext2;
            ext2 = tmp;
        }

        /*
         *   Use the fact that cos(x + pi) = -cos(x) once again: if x >
         *   pi/2, subtract pi from x to adjust the range to -pi/2 <= x <=
         *   pi/2.  
         */
        div_by_long(ext7, 2);
        if (compare_abs(ext1, ext7) > 0)
        {
            /* negate the result */
            neg_result = !neg_result;

            /* subtract pi from the argument */
            copy_val(ext7, pi, TRUE);
            compute_abs_diff_into(ext2, ext1, ext7);

            /* swap the result into r1 */
            tmp = ext1;
            ext1 = ext2;
            ext2 = tmp;
        }

        /*
         *   once again, reduce our range using the sign equivalence -
         *   this will limit our range to 0 <= x <= pi/2 
         */
        set_neg(ext1, FALSE);

        /*
         *   Next, observe that cos(x+pi/2) = -sin(x).  If x > pi/4,
         *   calculate using the sine series instead of the cosine series.
         */
        copy_val(ext7, pi, TRUE);
        div_by_long(ext7, 4);
        if (compare_abs(ext1, ext7) > 0)
        {
            /* negate the result */
            neg_result = !neg_result;
            
            /* calculate pi/2 */
            copy_val(ext7, pi, TRUE);
            div_by_long(ext7, 2);
            
            /* 
             *   subtract pi/2 - this will give us a value in the range
             *   -pi/4 <= x <= pi/4 
             */
            compute_abs_diff_into(ext2, ext1, ext7);

            /* sin(-x) = -sin(x) */
            if (get_neg(ext1))
                neg_result = !neg_result;
            set_neg(ext1, FALSE);

            /* swap the result into r1 */
            tmp = ext1;
            ext1 = ext2;
            ext2 = tmp;

            /* calculate the sine series */
            calc_sin_series(vmg_ new_ext,
                            ext1, ext2, ext3, ext4, ext5, ext6, ext7);
        }
        else
        {
            /*
             *   We now have a value in the range 0 <= x <= pi/4, which
             *   will converge quickly with our Taylor series 
             */
            calc_cos_series(vmg_ new_ext,
                            ext1, ext2, ext3, ext4, ext5, ext6, ext7);
        }

        /* negate the result if necessary */
        if (neg_result)
            set_neg(new_ext, !get_neg(new_ext));

        /* normalize the result */
        normalize(new_ext);
    }
    err_finally
    {
        /* release our temporary registers */
        release_temp_regs(7, hdl1, hdl2, hdl3, hdl4, hdl5, hdl6, hdl7);
    }
    err_end;

    /* remove my self-reference */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   property eval - tangent.  We calculate the sine and cosine, then
 *   compute the quotient to determine the tangent.  This routine works
 *   very much like the sin() and cos() routines; refer to getp_sin() for
 *   more thorough documentation.  
 */
int CVmObjBigNum::getp_tan(VMG_ vm_obj_id_t self,
                           vm_val_t *retval, uint *argc)
{
    char *new_ext;
    size_t prec = get_prec(ext_);
    uint hdl1, hdl2, hdl3, hdl4, hdl5, hdl6, hdl7, hdl8, hdl9;
    char *ext1, *ext2, *ext3, *ext4, *ext5, *ext6, *ext7, *ext8, *ext9;
    int neg_result;
    int invert_result;
    const char *pi;

    /* check arguments and set up the result */
    if (setup_getp_0(vmg_ self, retval, argc, &new_ext))
        return TRUE;

    /* cache pi to the working precision */
    pi = cache_pi(prec + 3);

    /* 
     *   Allocate our temporary registers for sin() and cos()
     *   calculations, plus two extra: one to hold the sine while we're
     *   calculating the cosine, and the other to hold the cosine result.
     *   
     *   (We could make do with fewer registers by copying values around,
     *   but if the numbers are of very high precision it's much cheaper
     *   to allocate more registers, since the registers come out of the
     *   register cache and probably won't require any actual memory
     *   allocation.)  
     */
    alloc_temp_regs(prec + 3, 9,
                    &ext1, &hdl1, &ext2, &hdl2, &ext3, &hdl3,
                    &ext4, &hdl4, &ext5, &hdl5, &ext6, &hdl6,
                    &ext7, &hdl7, &ext8, &hdl8, &ext9, &hdl9);

    /* protect against errors so we definitely free the registers */
    err_try
    {
        char *tmp;

        /* presume we won't have to invert our result */
        invert_result = FALSE;

        /* copy our initial value to ext1 */
        copy_val(ext1, ext_, FALSE);

        /* note that tan(-x) = -tan(x) - if x < 0, use -x */
        neg_result = get_neg(ext1);
        set_neg(ext1, FALSE);

        /* reduce the argument modulo 2*pi */
        copy_val(ext7, pi, TRUE);
        mul_by_long(ext7, 2);
        while (compare_abs(ext1, ext7) > 0)
        {
            /* divide by 2*pi, storing the remainder in r2 */
            compute_quotient_into(ext6, ext2, ext1, ext7);

            /* swap r2 into r1 for the next round */
            tmp = ext1;
            ext1 = ext2;
            ext2 = tmp;
        }

        /* 
         *   Next, note that tan(x+pi) = tan(x).  So, if x > pi, the
         *   argument by pi.  This will reduce our range to 0 <= x <= pi.  
         */
        copy_val(ext7, pi, TRUE);
        if (compare_abs(ext1, ext7) > 0)
        {
            /* subtract pi from the argument */
            compute_abs_diff_into(ext2, ext1, ext7);

            /* swap the result into r1 */
            tmp = ext1;
            ext1 = ext2;
            ext2 = tmp;
        }

        /*
         *   Use the fact that tan(x + pi) = tan(x) once again: if x >
         *   pi/2, subtract pi from x to adjust the range to -pi/2 <= x <=
         *   pi/2.  
         */
        div_by_long(ext7, 2);
        if (compare_abs(ext1, ext7) > 0)
        {
            /* subtract pi from the argument */
            copy_val(ext7, pi, TRUE);
            compute_abs_diff_into(ext2, ext1, ext7);

            /* swap the result into r1 */
            tmp = ext1;
            ext1 = ext2;
            ext2 = tmp;
        }

        /*
         *   once again, reduce our range using the sign equivalence -
         *   this will limit our range to 0 <= x <= pi/2 
         */
        if (get_neg(ext1))
            neg_result = !neg_result;
        set_neg(ext1, FALSE);

        /*
         *   Next, observe that tan(x+pi/2) = 1/tan(x).  If x > pi/4,
         *   invert the result.  
         */
        copy_val(ext7, pi, TRUE);
        div_by_long(ext7, 4);
        if (compare_abs(ext1, ext7) > 0)
        {
            /* calculate pi/2 */
            copy_val(ext7, pi, TRUE);
            div_by_long(ext7, 2);
            
            /* subtract pi/2 to get into range -pi/4 <= x <= pi/4 */
            compute_abs_diff_into(ext2, ext1, ext7);

            /* sin(-x) = -sin(x) */
            if (get_neg(ext1))
                neg_result = !neg_result;
            set_neg(ext1, FALSE);

            /* swap the result into r1 */
            tmp = ext1;
            ext1 = ext2;
            ext2 = tmp;

            /* note that we must invert the result */
            invert_result = TRUE;
        }

        /* 
         *   make a copy of our argument value in ext9 for safekeeping
         *   while we're calculating the sine 
         */
        copy_val(ext9, ext1, FALSE);

        /*
         *   We now have a value in the range 0 <= x <= pi/4, which will
         *   converge quickly with our Taylor series for sine and cosine.
         *   This will also ensure that both sin and cos are non-negative,
         *   so the sign we calculated for the tangent is all that matters
         *   for the sign.
         *   
         *   First, Calculate the sine and store the result in r8.  
         */
        calc_sin_series(vmg_ ext8,
                        ext1, ext2, ext3, ext4, ext5, ext6, ext7);

        /* 
         *   Calculate the cosine and store the result in r1.  Note that
         *   we saved the argument value in ext9 while we were working on
         *   the sine, so we can now use that value as the argument here.
         *   ext1 was trashed by the sine calculation, so just use it as
         *   the output register here.  
         */
        calc_cos_series(vmg_ ext1,
                        ext9, ext2, ext3, ext4, ext5, ext6, ext7);

        /* calculate the quotient sin/cos, or cos/sin if inverted */
        if (invert_result)
            compute_quotient_into(new_ext, 0, ext1, ext8);
        else
            compute_quotient_into(new_ext, 0, ext8, ext1);

        /* negate the result if necessary */
        set_neg(new_ext, neg_result);

        /* normalize the result */
        normalize(new_ext);
    }
    err_finally
    {
        /* release our temporary registers */
        release_temp_regs(9, hdl1, hdl2, hdl3, hdl4,
                          hdl5, hdl6, hdl7, hdl8, hdl9);
    }
    err_end;

    /* remove my self-reference */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   property evaluator - convert degrees to radians
 */
int CVmObjBigNum::getp_deg2rad(VMG_ vm_obj_id_t self,
                               vm_val_t *retval, uint *argc)
{
    char *new_ext;
    size_t prec = get_prec(ext_);
    uint hdl1;
    char *ext1;
    const char *pi;

    /* check arguments and set up the result */
    if (setup_getp_0(vmg_ self, retval, argc, &new_ext))
        return TRUE;

    /* cache pi to our required precision */
    pi = cache_pi(prec + 2);

    /* 
     *   allocate a temporary register for pi/180 - give it some extra
     *   precision 
     */
    alloc_temp_regs(prec + 2, 1, &ext1, &hdl1);

    /* get pi to our precision */
    copy_val(ext1, pi, TRUE);

    /* divide pi by 180 */
    div_by_long(ext1, 180);

    /* multiply our value by pi/180 */
    compute_prod_into(new_ext, ext_, ext1);

    /* release our temporary registers */
    release_temp_regs(1, hdl1);

    /* remove my self-reference */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   property evaluator - convert radians to degrees
 */
int CVmObjBigNum::getp_rad2deg(VMG_ vm_obj_id_t self,
                               vm_val_t *retval, uint *argc)
{
    char *new_ext;
    size_t prec = get_prec(ext_);
    uint hdl1;
    char *ext1;
    const char *pi;

    /* check arguments and set up the result */
    if (setup_getp_0(vmg_ self, retval, argc, &new_ext))
        return TRUE;

    /* cache pi to our working precision */
    pi = cache_pi(prec + 2);

    /* allocate a temporary register for pi/180 */
    alloc_temp_regs(prec + 2, 1, &ext1, &hdl1);

    /* catch errors so we can be sure to free registers */
    err_try
    {
        /* get pi to our precision */
        copy_val(ext1, pi, TRUE);

        /* divide pi by 180 */
        div_by_long(ext1, 180);

        /* divide by pi/180 */
        compute_quotient_into(new_ext, 0, ext_, ext1);
    }
    err_finally
    {
        /* release our temporary registers */
        release_temp_regs(1, hdl1);
    }
    err_end;

    /* remove my self-reference */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   property evaluator - arcsine
 */
int CVmObjBigNum::getp_asin(VMG_ vm_obj_id_t self,
                            vm_val_t *retval, uint *argc)
{
    /* calculate and return the arcsine */
    return calc_asincos(vmg_ self, retval, argc, FALSE);
}

/*
 *   property evaluator - arccosine 
 */
int CVmObjBigNum::getp_acos(VMG_ vm_obj_id_t self,
                            vm_val_t *retval, uint *argc)
{
    /* calculate and return the arcsine */
    return calc_asincos(vmg_ self, retval, argc, TRUE);
}

/*
 *   Common property evaluator routine - arcsine and arccosine.  arcsin
 *   and arccos are related by arccos(x) = pi/2 - arcsin(x).  So, to
 *   calculate an arccos, we can simply calculate the arcsin, then
 *   subtract the result from pi/2.  
 */
int CVmObjBigNum::calc_asincos(VMG_ vm_obj_id_t self,
                               vm_val_t *retval, uint *argc, int is_acos)
{
    char *new_ext;

    /* check arguments and set up the result */
    if (setup_getp_0(vmg_ self, retval, argc, &new_ext))
        return TRUE;

    /* calculate the arcsin/arccos into the result */
    calc_asincos_into(new_ext, ext_, is_acos);

    /* remove my self-reference */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   Calculate the arcsine or arccosine into the given result variable 
 */
void CVmObjBigNum::calc_asincos_into(char *dst, const char *src,
                                     int is_acos)
{
    size_t prec = get_prec(dst);
    uint hdl1, hdl2, hdl3, hdl4, hdl5;
    char *ext1, *ext2, *ext3, *ext4, *ext5;
    const char *pi;

    /* cache pi to our working precision */
    pi = cache_pi(prec + 3);

    /* 
     *   allocate our temporary registers - use some extra precision over
     *   what we need for the result, to reduce the effect of accumulated
     *   rounding error 
     */
    alloc_temp_regs(prec + 3, 5,
                    &ext1, &hdl1, &ext2, &hdl2, &ext3, &hdl3,
                    &ext4, &hdl4, &ext5, &hdl5);

    /* catch errors so we release our temp registers */
    err_try
    {
        char *tmp;
        static const unsigned char one_over_sqrt2[] =
        {
            /* precision = 10, scale = 0, flags = 0 */
            10, 0, 0, 0, 0,
            0x70, 0x71, 0x06, 0x78, 0x14
        };
        int use_sqrt;
        int sqrt_neg;
        
        /* get the initial value of x into our accumulator, r1 */
        copy_val(ext1, src, FALSE);

        /* 
         *   check to see if the absolute value of the argument is greater
         *   than 1 - if it is, it's not valid 
         */
        copy_val(ext2, get_one(), FALSE);
        if (compare_abs(ext1, ext2) > 0)
            err_throw(VMERR_OUT_OF_RANGE);

        /* presume we won't need to use the sqrt(1-x^2) method */
        use_sqrt = FALSE;

        /*
         *   Check to see if the absolute value of the argument is greater
         *   than 1/sqrt(2).  If it is, the series expansion converges too
         *   slowly (as slowly as never, if the value is exactly 1).  To
         *   speed things up, in these cases calculate pi/2 -
         *   asin(sqrt(1-x^2)) instead, which is equivalent but gives us a
         *   smaller asin() argument for quicker series convergence.
         *   
         *   We don't need to compare to 1/sqrt(2) in great precision;
         *   just use a few digits.  
         */
        copy_val(ext2, (const char *)one_over_sqrt2, TRUE);
        if (compare_abs(ext1, ext2) > 0)
        {
            /* flag that we're using the sqrt method */
            use_sqrt = TRUE;
            
            /* note the sign - we'll need to apply this to the result */
            sqrt_neg = get_neg(ext1);

            /* compute x^2 into r2 */
            compute_prod_into(ext2, ext1, ext1);

            /* subtract r2 from 1 (by adding -r2 to 1), storing in r4 */
            copy_val(ext3, get_one(), FALSE);
            make_negative(ext2);
            compute_sum_into(ext4, ext3, ext2);

            /* compute sqrt(1-x^2) (which is sqrt(r4)) into r1 */
            compute_sqrt_into(ext1, ext4);
        }

        /* compute the arcsine */
        ext1 = calc_asin_series(ext1, ext2, ext3, ext4, ext5);

        /* if we're using the sqrt method, finish the sqrt calculation */
        if (use_sqrt)
        {
            /* calculate pi/2 */
            copy_val(ext2, pi, TRUE);
            div_by_long(ext2, 2);

            /* compute pi/2 - r1 by negating r1 and adding it */
            negate(ext1);
            compute_sum_into(ext3, ext2, ext1);

            /* negate the result if the original value was negative */
            if (sqrt_neg)
                negate(ext3);

            /* swap the result back into r1 */
            tmp = ext1;
            ext1 = ext3;
            ext3 = tmp;
        }

        /* 
         *   We now have the arcsine in r1.  If we actually wanted the
         *   arccosine, subtract the arcsine from pi/2 to yield the
         *   arccosine.  
         */
        if (is_acos)
        {
            /* get pi/2 into r2 */
            copy_val(ext2, pi, TRUE);
            div_by_long(ext2, 2);
            
            /* negate r1 to get -asin */
            negate(ext1);
            
            /* add -asin to r2 to yield the arccosine in r3 */
            compute_sum_into(ext3, ext2, ext1);
            
            /* swap the result back into ext1 */
            tmp = ext3;
            ext3 = ext1;
            ext1 = tmp;
        }

        /* store the result, rounding if necessary */
        copy_val(dst, ext1, TRUE);
    }
    err_finally
    {
        /* release our temporary registers */
        release_temp_regs(5, hdl1, hdl2, hdl3, hdl4, hdl5);
    }
    err_end;
}

/*
 *   Calculate the asin series expansion.  This should only be called with
 *   argument values with absolute value less than 1/sqrt(2), because the
 *   series converges very slowly for larger values.  For operands above
 *   1/sqrt(2), the caller should instead compute the equivalent value
 *   
 *   +/- (pi/2 - asin(sqrt(1-x^2))).
 *   
 *   (+ if x > 0, - if x < 0).
 *   
 *   The argument value is in ext1, and we return a pointer to the
 *   register that contains the result (which will be one of the input
 *   registers).  We use all of the input registers as scratchpads, so
 *   their values are not retained.  
 */
char *CVmObjBigNum::calc_asin_series(char *ext1, char *ext2,
                                     char *ext3, char *ext4, char *ext5)
{
    ulong n;

    /* get the current power of x (1) into the x power register, r2 */
    copy_val(ext2, ext1, FALSE);
    
    /* 
     *   compute x^2 into r3 - we'll multiply the previous power by this
     *   to get the next power (we need x^1, x^3, x^5, etc) 
     */
    compute_prod_into(ext3, ext1, ext1);

    /* start at the first term */
    n = 1;

    /* keep going until we have enough precision in the result */
    for (;;)
    {
        ulong i;
        char *tmp;
        
        /* move on to the next term */
        n += 2;
        
        /* 
         *   compute the next weirdness factor into r4:
         *   
         *    1*3*5*...*(n-2)
         *.  -----------------
         *.  2*4*6*...*(n-1)*n 
         */

        /* start out with 1 in r4 */
        copy_val(ext4, get_one(), FALSE);

        /* multiply by odd numbers up to but not including 'n' */
        for (i = 3 ; i < n ; i += 2)
            mul_by_long(ext4, i);
        
        /* 
         *   Divide by even numbers up to and including n-1.  Note that we
         *   use div_by_long() because it's faster than using a BigNumber
         *   divisor.  div_by_long() has a limit of ULONG_MAX/10 for the
         *   divisor; we shouldn't have to worry about exceeding that, since
         *   our maximum precision is 64k digits.  The series will converge
         *   to our maximum precision long before 'i' gets close to
         *   ULONG_MAX/10.  (Even if not, computing billions of series terms
         *   would take so long that we'd never get there anyway.)  
         */
        for (i = 2 ; i < n ; i += 2)
            div_by_long(ext4, i);
        
        /* divide by n */
        div_by_long(ext4, n);
        
        /* 
         *   compute the next power of x - multiply our current power of x
         *   (r2) by x^2 (r3) 
         */
        compute_prod_into(ext5, ext2, ext3);
        
        /* swap r5 into r2 - this new power of x is now current */
        tmp = ext5;
        ext5 = ext2;
        ext2 = tmp;
        
        /* 
         *   multiply the current x power by the current weirdness factor
         *   - this will yield the current term into r5 
         */
        compute_prod_into(ext5, ext2, ext4);

        /* 
         *   if this value is too small to show up in our accumulator,
         *   we're done 
         */
        if (is_zero(ext5)
            || get_exp(ext1) - get_exp(ext5) > (int)get_prec(ext1))
            break;

        /* 
         *   we can trash the weird factor now - use it as a scratch pad
         *   for adding the accumulator so far (r1) to this term 
         */
        compute_sum_into(ext4, ext1, ext5);

        /* swap the result into r1, since it's the new accumulator */
        tmp = ext4;
        ext4 = ext1;
        ext1 = tmp;
    }

    /* return the accumulator register */
    return ext1;
}

/*
 *   property evaluator - arctangent
 */
int CVmObjBigNum::getp_atan(VMG_ vm_obj_id_t self,
                            vm_val_t *retval, uint *argc)
{
    char *new_ext;
    size_t prec = get_prec(ext_);
    uint hdl1, hdl2, hdl3, hdl4, hdl5;
    char *ext1, *ext2, *ext3, *ext4, *ext5;
    const char *pi;

    /* check arguments and set up the result */
    if (setup_getp_0(vmg_ self, retval, argc, &new_ext))
        return TRUE;

    /* cache pi to our working precision */
    pi = cache_pi(prec + 3);

    /* allocate some temporary registers */
    alloc_temp_regs(prec + 3, 5,
                    &ext1, &hdl1, &ext2, &hdl2, &ext3, &hdl3,
                    &ext4, &hdl4, &ext5, &hdl5);

    /* catch errors so we can be sure to free registers */
    err_try
    {
        /*
         *   If x has absolute value close to 1, either above or below,
         *   our two series don't converge very rapidly.  Happily, we can
         *   fall back on an alternative in these case by using the
         *   relation
         *   
         *   arctan(x) = (+/-)arccos(1/sqrt(x^2+1))
         *   
         *   (The sign of the result is the same as the sign of x.)  Since
         *   we already have routines for arccosine and square root, this
         *   calculation is easy.
         *   
         *   If x doesn't have absolute value close to 1, use the
         *   appropriate series, since they converge rapidly.  
         */
        if (get_exp(ext_) < -1 || get_exp(ext_) > 2)
        {
            int term_neg;
            ulong n;
            
            /* 
             *   exponent less than -1 -> the number has a small absolute
             *   value - use the small-x series expansion:
             *   
             *   x - x^3/3 + x^5/5 - x^7/7 ...
             *   
             *   OR
             *   
             *   exponent greater than 2 -> the number has a large
             *   absolute value, so the large-x series expansion should
             *   converge quickly:
             *   
             *   +/- pi/2 - 1/x + 1/3x^3 - 1/5x^5 ...
             *   
             *   (the sign of the leading pi/2 term is positive if x is
             *   positive, negative if x is negative)
             *   
             *   We can commonize these expressions by defining x' = x for
             *   small x and x' = 1/x for large x, defining C as 0 for
             *   small x and +/-pi/2 for large X, defining N as +1 for
             *   small x and -1 for large x, and rewriting the series as:
             *   
             *   C + Nx' - Nx'^3/3 + Nx'^5/5 + ...  
             */

            /* check for large or small value */
            if (get_exp(ext_) < 0)
            {
                /* small number - start with zero in the accumulator (r1) */
                set_zero(ext1);

                /* get the current power of x' into r2 - this is simply x */
                copy_val(ext2, ext_, FALSE);

                /* the first term (x) is positive */
                term_neg = FALSE;
            }
            else
            {
                /* large number - start with pi/2 in the accumulator (r1) */
                copy_val(ext1, pi, TRUE);
                div_by_long(ext1, 2);

                /* if x is negative, make that -pi/2 */
                set_neg(ext1, get_neg(ext_));

                /* get 1/x into r2 - this is our x' term value */
                compute_quotient_into(ext2, 0, get_one(), ext_);

                /* the first term (1/x) is negative */
                term_neg = TRUE;
            }

            /* start at the first term */
            n = 1;

            /* 
             *   get x'^2 into r3 - we'll use this to calculate each
             *   subsequent term (we need x', x'^3, x'^5, etc) 
             */
            compute_prod_into(ext3, ext2, ext2);

            /* iterate until we reach the desired precision */
            for (;;)
            {
                char *tmp;
                
                /* copy the current power term from r2 into r4 */
                copy_val(ext4, ext2, FALSE);

                /* 
                 *   divide by the current term constant (we'll never have
                 *   to compute billions of terms to reach our maximum
                 *   possible 64k digits of precision, so 'n' will always be
                 *   way less than the div_by_long limit of ULONG_MAX/10) 
                 */
                div_by_long(ext4, n);

                /* negate this term if necessary */
                if (term_neg)
                    set_neg(ext4, !get_neg(ext4));

                /* 
                 *   if we're under the radar on precision, stop looping
                 *   (don't stop on the first term, since the accumulator
                 *   hasn't been fully primed yet) 
                 */
                if (n != 1
                    && (is_zero(ext4)
                        || (get_exp(ext1) - get_exp(ext4)
                            > (int)get_prec(ext1))))
                    break;

                /* compute the sum of the accumulator and this term */
                compute_sum_into(ext5, ext1, ext4);

                /* swap the result back into the accumulator */
                tmp = ext1;
                ext1 = ext5;
                ext5 = tmp;

                /* 
                 *   move on to the next term - advance n by 2 and swap
                 *   the term sign 
                 */
                n += 2;
                term_neg = !term_neg;

                /* 
                 *   compute the next power term - multiply r2 (the latest
                 *   x' power) by r3 (x'^2) 
                 */
                compute_prod_into(ext4, ext2, ext3);

                /* swap r4 back into r2 - this is now the latest power */
                tmp = ext2;
                ext2 = ext4;
                ext4 = tmp;
            }

            /* store the result, rounding as needed */
            copy_val(new_ext, ext1, TRUE);
        }
        else
        {
            /* 
             *   We have a value of x from .01 to 10 - in this range, the
             *   rewrite using arccosine will give us excellent precision. 
             */

            /* calculate x^2 into r1 */
            compute_prod_into(ext1, ext_, ext_);
            
            /* add one (x^2 has to be positive, so don't worry about sign) */
            increment_abs(ext1);
            
            /* take the square root and put the result in r2 */
            compute_sqrt_into(ext2, ext1);
            
            /* divide that into 1, and put the result back in r1 */
            compute_quotient_into(ext1, 0, get_one(), ext2);
            
            /* 
             *   Compute the arccosine of this value - the result is the
             *   arctangent, so we can store it directly in the output
             *   register.  
             */
            calc_asincos_into(new_ext, ext1, TRUE);
            
            /* if the input was negative, invert the sign of the result */
            if (get_neg(ext_))
                negate(new_ext);
        }
    }
    err_finally
    {
        /* release our temporary registers */
        release_temp_regs(5, hdl1, hdl2, hdl3, hdl4, hdl5);
    }
    err_end;

    /* discard the GC protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   property evaluator - square root
 */
int CVmObjBigNum::getp_sqrt(VMG_ vm_obj_id_t self,
                            vm_val_t *retval, uint *argc)
{
    char *new_ext;

    /* check arguments and set up the result */
    if (setup_getp_0(vmg_ self, retval, argc, &new_ext))
        return TRUE;

    /* compute the square root into the result */
    compute_sqrt_into(new_ext, ext_);

    /* discard the GC protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   property evaluator - natural logarithm
 */
int CVmObjBigNum::getp_ln(VMG_ vm_obj_id_t self,
                          vm_val_t *retval, uint *argc)
{
    char *new_ext;

    /* zero or negative values are not allowed */
    if (is_zero(ext_) || get_neg(ext_))
        err_throw(VMERR_OUT_OF_RANGE);

    /* check arguments and set up the result */
    if (setup_getp_0(vmg_ self, retval, argc, &new_ext))
        return TRUE;

    /* compute the natural logarithm */
    compute_ln_into(new_ext, ext_);

    /* discard the GC protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   property evaluator - exp (exponentiate e, the base of the natural
 *   logarithm, to the power of this number) 
 */
int CVmObjBigNum::getp_exp(VMG_ vm_obj_id_t self,
                           vm_val_t *retval, uint *argc)
{
    char *new_ext;

    /* check arguments and set up the result */
    if (setup_getp_0(vmg_ self, retval, argc, &new_ext))
        return TRUE;

    /* compute exp(x) */
    compute_exp_into(new_ext, ext_);

    /* discard the GC protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   property evaluator - log10 (base-10 logarithm)
 */
int CVmObjBigNum::getp_log10(VMG_ vm_obj_id_t self,
                             vm_val_t *retval, uint *argc)
{
    char *new_ext;
    size_t prec = get_prec(ext_);
    uint hdl1, hdl2, hdl3;
    char *ext1, *ext2, *ext3;
    const char *ln10;

    /* check arguments and set up the result */
    if (setup_getp_0(vmg_ self, retval, argc, &new_ext))
        return TRUE;

    /* cache ln(10) to the required precision */
    ln10 = cache_ln10(prec + 3);

    /* allocate some temporary registers */
    alloc_temp_regs(prec + 3, 3,
                    &ext1, &hdl1, &ext2, &hdl2, &ext3, &hdl3);

    /* catch errors so we can be sure to free registers */
    err_try
    {
        /* compute the natural logarithm of the number */
        compute_ln_into(ext1, ext_);

        /* get ln(10), properly rounded */
        copy_val(ext2, ln10, TRUE);

        /* compute ln(x)/ln(10) to yield log10(x) */
        compute_quotient_into(ext3, 0, ext1, ext2);

        /* store the result, rounding as needed */
        copy_val(new_ext, ext3, TRUE);
    }
    err_finally
    {
        /* release our temporary registers */
        release_temp_regs(3, hdl1, hdl2, hdl3);
    }
    err_end;

    /* discard the GC protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   property evaluator - pow(y) - calculate x^y 
 */
int CVmObjBigNum::getp_pow(VMG_ vm_obj_id_t self,
                           vm_val_t *retval, uint *argc)
{
    vm_val_t val2;
    const char *val2_ext;
    char *new_ext;
    size_t prec;
    uint hdl1, hdl2;
    char *ext1, *ext2;

    /* check arguments and allocate the result value */
    if (setup_getp_1(vmg_ self, retval, argc, &new_ext,
                     &val2, &val2_ext, FALSE))
        return TRUE;

    /* use the precision of the result */
    prec = get_prec(new_ext);
    
    /* 
     *   Check for a special case: if the number we're exponentiating is
     *   zero, the result is 0 for any positive exponent, and an error for
     *   any non-positive exponent (0^0 is undefined, and 0^n where n<0 is
     *   equivalent to 1/0^n == 1/0, which is a divide-by-zero error).  
     */
    if (is_zero(ext_))
    {
        /* 0^0 is undefined */
        if (is_zero(val2_ext))
            err_throw(VMERR_OUT_OF_RANGE);

        /* 0^negative is a divide by zero error */
        if (get_neg(val2_ext))
            err_throw(VMERR_DIVIDE_BY_ZERO);
        
        /* set the result to one, and we're done */
        set_zero(new_ext);
        goto done;
    }

    /* allocate some temporary registers */
    alloc_temp_regs(prec + 3, 2,
                    &ext1, &hdl1, &ext2, &hdl2);

    /* catch errors so we can be sure to free registers */
    err_try
    {
        int result_neg;
        
        /*
         *   If a = e^b, then b = ln(a).  This means that x^y = e^ln(x^y)
         *   = e^(y * ln(x)).  So, we can compute the result in terms of
         *   natural logarithm and exponentiation of 'e', for which we
         *   have primitives we can call.  
         */

        /*
         *   If x is negative, we can only exponentiate the value to
         *   integer powers.  In this case, we can substitute x' = -x
         *   (hence x' will be positive), and rewrite the expression as
         *   
         *   (-1)^y * (x')^y
         *   
         *   We can only calculate (-1)^y for integer values of y, since
         *   the result is complex if y is not an integer.  
         */
        if (get_neg(ext_))
        {
            size_t idx;
            int units_dig;
            
            /* copy x into r2 */
            copy_val(ext2, ext_, FALSE);

            /* 
             *   calculate x' = (-x) - since x is negative, this will
             *   guarantee that x' is positive 
             */
            set_neg(ext2, FALSE);

            /* 
             *   make sure y is an integer - start at the first digit
             *   after the decimal point and check for any non-zero digits 
             */
            idx = (get_exp(val2_ext) < 0 ? 0 : (size_t)get_exp(val2_ext));
            for ( ; idx < get_prec(val2_ext) ; ++idx)
            {
                /* if this digit isn't a zero, it's not an integer */
                if (get_dig(val2_ext, idx) != 0)
                {
                    /* y isn't an integer, so we can't calculate (-1)^y */
                    err_throw(VMERR_OUT_OF_RANGE);
                }
            }

            /* get the first digit to the left of the decimal point */
            if (get_exp(val2_ext) <= 0
                || (size_t)get_exp(val2_ext) > get_prec(val2_ext))
            {
                /* the units digit isn't represented - zero is implied */
                units_dig = 0;
            }
            else
            {
                /* get the digit */
                units_dig = get_dig(val2_ext, (size_t)get_exp(val2_ext) - 1);
            }

            /* 
             *   if the units digit is even, the result will be positive;
             *   if it's odd, the result will be negative 
             */
            result_neg = ((units_dig & 1) != 0);

            /* calculate ln(x') into r1 */
            compute_ln_into(ext1, ext2);
        }
        else
        {
            /* calculate ln(x) into r1 */
            compute_ln_into(ext1, ext_);

            /* the result will be positive */
            result_neg = FALSE;
        }

        /* calculate y * ln(x) into r2 */
        compute_prod_into(ext2, val2_ext, ext1);

        /* calculate exp(r2) = exp(y*ln(x)) = x^y into r1 */
        compute_exp_into(ext1, ext2);

        /* negate the result if we had a negative x and an odd power */
        if (result_neg)
            negate(ext1);

        /* save the result, rounding as needed */
        copy_val(new_ext, ext1, TRUE);
    }
    err_finally
    {
        /* release our temporary registers */
        release_temp_regs(2, hdl1, hdl2);
    }
    err_end;

done:
    /* discard the GC protection */
    G_stk->discard(2);

    /* handled */
    return TRUE;
}

/*
 *   property evaluator - sinh
 */
int CVmObjBigNum::getp_sinh(VMG_ vm_obj_id_t self,
                            vm_val_t *retval, uint *argc)
{
    /* calculate the hyperbolic sine using the common evaluator */
    return calc_sinhcosh(vmg_ self, retval, argc, FALSE, FALSE);
}
    
/*
 *   property evaluator - cosh
 */
int CVmObjBigNum::getp_cosh(VMG_ vm_obj_id_t self,
                            vm_val_t *retval, uint *argc)
{
    /* calculate the hyperbolic cosine using the common evaluator */
    return calc_sinhcosh(vmg_ self, retval, argc, TRUE, FALSE);
}

int CVmObjBigNum::getp_tanh(VMG_ vm_obj_id_t self,
                            vm_val_t *retval, uint *argc)
{
    /* calculate the hyperbolic tangent using the common evaluator */
    return calc_sinhcosh(vmg_ self, retval, argc, FALSE, TRUE);
}

/*
 *   common property evaluator - compute a hyperbolic sine or cosine, or
 *   do both to calculate a hyperbolic tangent 
 */
int CVmObjBigNum::calc_sinhcosh(VMG_ vm_obj_id_t self,
                                vm_val_t *retval, uint *argc,
                                int is_cosh, int is_tanh)
{
    char *new_ext;

    /* check arguments and allocate the return value */
    if (setup_getp_0(vmg_ self, retval, argc, &new_ext))
        return TRUE;

    /* calculate the result */
    compute_sinhcosh_into(new_ext, ext_, is_cosh, is_tanh);

    /* discard the GC protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* 
 *   Compute the fractional part of a number (replacing the value in place).
 *   This effectively subtracts the integer portion of the number, leaving
 *   only the fractional portion.  
 */
void CVmObjBigNum::compute_frac(char *ext)
{
    /* get the exponent and precision */
    int exp = get_exp(ext);
    size_t prec = get_prec(ext);

    /* clear out the first n digits, where n is the exponent */
    for (size_t idx = 0 ; idx < prec && (int)idx < exp ; ++idx)
        set_dig(ext, idx, 0);

    /* normalize the result */
    normalize(ext);
}

/*
 *   Get the whole part of a number (replacing the value in place).  This
 *   truncates the number to its integer portion, with no rounding.  
 */
void CVmObjBigNum::compute_whole(char *ext)
{
    /* get the exponent and precision */
    int exp = get_exp(ext);
    size_t prec = get_prec(ext);

    /* check what we have */
    if (exp <= 0)
    {
        /* it's an entirely fractional number - the result is zero */
        set_zero(ext);
    }
    else
    {
        /* clear digits after the decimal point */
        for (size_t idx = (size_t)exp ; idx < prec ; ++idx)
            set_dig(ext, idx, 0);

        /* normalize the result */
        normalize(ext);
    }
}


/* ------------------------------------------------------------------------ */
/*
 *   property evaluator - numType: get the number type information
 */

/* 
 *   number types defined in bignum.h - these are part of the public API, so
 *   they can't be changed 
 */
#define NumTypeNum    0x0001
#define NumTypeNAN    0x0002
#define NumTypePInf   0x0004
#define NumTypeNInf   0x0008
#define NumTypePZero  0x0010
#define NumTypeNZero  0x0020


int CVmObjBigNum::getp_numType(VMG_ vm_obj_id_t self,
                               vm_val_t *retval, uint *argc)
{
    /* check arguments */
    static CVmNativeCodeDesc desc(0);
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* set the appropriate flags */
    switch (get_type(ext_))
    {
    case VMBN_T_NUM:
        retval->set_int(NumTypeNum
                        | (is_zero(ext_)
                           ? (get_neg(ext_) ? NumTypeNZero : NumTypePZero)
                           : 0));
        break;

    case VMBN_T_INF:
        retval->set_int(get_neg(ext_) ? NumTypeNInf : NumTypePInf);
        break;

    case VMBN_T_NAN:
    default:
        retval->set_int(NumTypeNAN);
        break;
    }

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Compute a natural logarithm 
 */
void CVmObjBigNum::compute_ln_into(char *dst, const char *src)
{
    uint hdl1, hdl2, hdl3, hdl4, hdl5;
    char *ext1, *ext2, *ext3, *ext4, *ext5;
    size_t prec = get_prec(dst);
    const char *ln10;

    /* cache the value of ln(10) */
    ln10 = cache_ln10(prec + 3);

    /* if the source value is zero, it's an error */
    if (is_zero(src))
        err_throw(VMERR_OUT_OF_RANGE);

    /* allocate some temporary registers */
    alloc_temp_regs(prec + 3, 5,
                    &ext1, &hdl1, &ext2, &hdl2, &ext3, &hdl3,
                    &ext4, &hdl4, &ext5, &hdl5);

    /* catch errors so we can be sure to free registers */
    err_try
    {
        int src_exp;

        /*
         *   Observe that we store our values as x = a*10^b.  We can thus
         *   rewrite ln(x) as ln(a*10^b) = ln(a)+ln(10^b) =
         *   log(a)+b*ln(10).  a is the mantissa, which due to
         *   normalization is in the range 0.1 <= a < 1.0.  Thus, a is an
         *   ideal argument for the Taylor series.  So, we can simply
         *   compute ln(mantissa), then add it to ln(10)*exponent.  
         */

        /* copy our argument into r1 */
        copy_val(ext1, src, FALSE);

        /* 
         *   remember the original exponent, and set the exponent of the
         *   series argument to zero - we'll correct for this later by
         *   adding the log of the exponent to the series result 
         */
        src_exp = get_exp(ext1);
        set_exp(ext1, 0);

        /*
         *   The series expansion is especially efficient for values near
         *   1.0.  We know the value is now in the range 0.1 <= a < 1.0.  So
         *   if the lead digit of the mantissa is 1, which means the value is
         *   0.1 and change, multiply by ten (thus making it 1.0 and change),
         *   and adjust the exponent accordingly.
         */
        if (get_dig(ext1, 0) == 1)
        {
            set_exp(ext1, 1);
            --src_exp;
        }

        /* compute the series expansion */
        ext1 = compute_ln_series_into(ext1, ext2, ext3, ext4, ext5);

        /* add in the input exponent, properly adjusted */
        if (src_exp != 0)
        {
            /* get ln10 into r2 */
            copy_val(ext2, ln10, TRUE);

            /* apply the exponent's sign if it's negative */
            if (src_exp < 0)
            {
                set_neg(ext2, TRUE);
                src_exp = -src_exp;
            }

            /* multiply by the exponent */
            mul_by_long(ext2, src_exp);

            /* add this value to the series expansion value */
            compute_sum_into(ext3, ext1, ext2);

            /* use this as the result */
            ext1 = ext3;
        }

        /* copy the result, rounding if needed */
        copy_val(dst, ext1, TRUE);
    }
    err_finally
    {
        /* release our temporary registers */
        release_temp_regs(5, hdl1, hdl2, hdl3, hdl4, hdl5);
    }
    err_end;
}

/*
 *   Compute the natural log series.  The argument value, initially in
 *   ext1, should be adjusted to a small value before this is called to
 *   ensure quick series convergence.  
 */
char *CVmObjBigNum::compute_ln_series_into(char *ext1, char *ext2,
                                           char *ext3, char *ext4,
                                           char *ext5)
{
    /* start at the first term of the series */
    ulong n = 1;
    
    /* subtract one from r1 to yield (x-1) in r2 */
    compute_abs_diff_into(ext2, ext1, get_one());
    
    /* add one to r1 to yield (x+1) */
    increment_abs(ext1);
    
    /* compute (x-1)/(x+1) into r3 - this will be our current power */
    compute_quotient_into(ext3, 0, ext2, ext1);
    
    /* 
     *   compute ((x-1)/(x+1))^2 into r4 - we'll multiply r3 by this on
     *   each iteration to produce the next required power, since we only
     *   need the odd powers 
     */
    compute_prod_into(ext4, ext3, ext3);
    
    /* start out with the first power in our accumulator (r1) */
    copy_val(ext1, ext3, FALSE);
    
    /* iterate until we have a good enough answer */
    for (;;)
    {
        /* compute the next power into r2 */
        compute_prod_into(ext2, ext3, ext4);
        
        /* copy the result into our current power register, r3 */
        copy_val(ext3, ext2, FALSE);
        
        /* advance n */
        n += 2;
        
        /* 
         *   divide the power by n (we'll never have to compute billions of
         *   terms to get our maximum 64k digits of precision, so 'n' will
         *   always be way less than the div_by_long limit of ULONG_MAX/10) 
         */
        div_by_long(ext2, n);
        
        /* if it's too small to notice, we're done */
        if (is_zero(ext2)
            || get_exp(ext1) - get_exp(ext2) > (int)get_prec(ext1))
            break;
        
        /* compute the sum with our accumulator into r5 */
        compute_sum_into(ext5, ext1, ext2);
        
        /* swap r5 and r1 - the new sum is our new accumulator */
        char *tmp = ext5;
        ext5 = ext1;
        ext1 = tmp;
    }
    
    /* multiply the result of the series by 2 */
    mul_by_long(ext1, 2);

    /* return the register containing the result */
    return ext1;
}

/*
 *   Compute e^x, where e is the base of the natural logarithm (2.818...) 
 */
void CVmObjBigNum::compute_exp_into(char *dst, const char *src)
{
    uint hdl1, hdl2, hdl3, hdl4, hdl5, hdl6;
    char *ext1, *ext2, *ext3, *ext4, *ext5, *ext6;
    size_t prec = get_prec(dst);
    const char *ln10;

    /* get the constant value of ln10 to the required precision */
    ln10 = cache_ln10(prec + 3);

    /* allocate temporary registers */
    alloc_temp_regs(prec + 3, 6,
                    &ext1, &hdl1, &ext2, &hdl2, &ext3, &hdl3,
                    &ext4, &hdl4, &ext5, &hdl5, &ext6, &hdl6);

    /* catch errors so we can be sure to free registers */
    err_try
    {
        char *tmp;
        ulong n;
        ulong n_fact;
        int use_int_fact;
        long new_exp;
        size_t idx;
        size_t decpt_idx;
        int twos;

        /*
         *   We want to calculate e^x.  Observe that a^x = e^(x*ln(x)), so
         *   10^x = e^(x*ln(10)).  We can rewrite our desired expression
         *   e^x as 
         *   
         *   e^[(x/ln(10)) * ln(10)]
         *   
         *   which is thus
         *   
         *   10^(x / ln(10))
         *   
         *   We store our numbers as a mantissa times 10 raised to an
         *   integer exponent.  Clearly the exponent of 10 in the formula
         *   above is not always an integer (in fact, it's extremely rare
         *   that it would be an integer), so we still have more work to
         *   do.  What we must do is obtain an integer exponent.  So, let
         *   us define y = x/ln(10); we can now rewrite the above as
         *   
         *   10^(int(y) + frac(y))
         *   
         *   where int(y) is the integer portion of y and frac(y) is the
         *   fractional portion (y - int(y)).  We can rewrite the above as
         *   
         *   10^frac(y) * 10^int(y)
         *   
         *   which we can further rewrite as
         *   
         *   e^(frac(y)*ln(10)) * 10^int(y)
         *   
         *   We haven't made the problem of finding an exponential
         *   disappear, but we've reduced the argument to a very
         *   manageable range, which is important because it makes the
         *   Taylor series converge quickly.  Furthermore, it's extremely
         *   inexpensive to separate out the problem like this, since it
         *   falls quite naturally out of the representation we use, so it
         *   doesn't add much overhead to do this preparation work.  
         */

        /* first, calculate x/ln(10) into r1 */
        compute_quotient_into(ext1, 0, src, ln10);

        /* 
         *   compute the integer portion of x/ln(10) - it has to fit in a
         *   16-bit integer, (-32768 to +32767), because this is going to
         *   be the exponent of the result (or roughly so, anyway) 
         */
        decpt_idx = get_exp(ext1) >= 0 ? (size_t)get_exp(ext1) : 0;
        for (new_exp = 0, idx = 0 ; idx < decpt_idx ; ++idx)
        {
            int dig;

            /* 
             *   get this digit if it's represented; if not, it's an
             *   implied trailing zero 
             */
            dig = (idx < get_prec(ext1) ? get_dig(ext1, idx) : 0);
            
            /* add this digit into the accumulator */
            new_exp *= 10;
            new_exp += dig;

            /* 
             *   Make sure we're still in range.  Note that, because our
             *   representation is 0.dddd*10^x, we need one more factor of
             *   ten than you'd think here, the adjust of the range from
             *   the expected -32768..32767 
             */
            if (new_exp > (get_neg(ext1) ? 32769L : 32766L))
                err_throw(VMERR_NUM_OVERFLOW);

            /* 
             *   zero out this digit, so that when we're done r1 has the
             *   fractional part only 
             */
            if (idx < get_prec(ext1))
                set_dig(ext1, idx, 0);
        }

        /* negate the exponent value if the source value is negative */
        if (get_neg(ext1))
            new_exp = -new_exp;

        /* normalize the fractional part, which remains in ext1 */
        normalize(ext1);

        /* 
         *   Multiply it by ln10, storing the result in r3.  This is the
         *   value we'll use with the Taylor series. 
         */
        compute_prod_into(ext3, ext1, ln10);

        /* 
         *   While our input value is greater than 0.5, divide it by two to
         *   make it smaller than 0.5.  This will speed up the series
         *   convergence.  When we're done, we'll correct for the divisions
         *   by squaring the result the same number of times that we halved
         *   'x', because e^2x = (e^x)^2.
         */
        copy_val(ext1, get_one(), FALSE);
        div_by_long(ext1, 2);
        for (twos = 0 ; compare_abs(ext3, ext1) > 0 ; ++twos)
            div_by_long(ext3, 2);

        /* 
         *   Start with 1+x in our accumulator (r1).  This unrolls the
         *   trivial first two steps of the loop, where n=0 (term=1) and n=1
         *   (term=x). 
         */
        copy_val(ext2, get_one(), FALSE);
        compute_sum_into(ext1, ext2, ext3);

        /* get term n=2 (x^2) into the current-power register (r2) */
        compute_prod_into(ext2, ext3, ext3);

        /* start with 2 in our factorial register (r4) */
        copy_val(ext4, get_one(), FALSE);
        mul_by_long(ext4, 2);

        /* start at term n=2, n! = 2 */
        n = 2;
        n_fact = 2;
        use_int_fact = TRUE;

        /* go until we reach the required precision */
        for (;;)
        {
            /* for efficiency, try integer division */
            if (use_int_fact)
            {
                /* 
                 *   we can still fit the factorial in an integer - divide
                 *   by the integer value of n!, since it's a lot faster 
                 */
                copy_val(ext5, ext2, FALSE);
                div_by_long(ext5, n_fact);

                /* calculate the next n! integer, if it'll fit in a long */
                if (n_fact > LONG_MAX/10/(n+1))
                {
                    /* 
                     *   it'll be too big next time - we'll have to start
                     *   using the full quotient calculation 
                     */
                    use_int_fact = FALSE;
                }
                else
                {
                    /* it'll still fit - calculate the next n! */
                    n_fact *= (n+1);
                }
            }
            else
            {
                /* compute x^n/n! (r2/r4) into r5 */
                compute_quotient_into(ext5, 0, ext2, ext4);
            }

            /* if we're below the required precision, we're done */
            if (is_zero(ext5)
                || get_exp(ext1) - get_exp(ext5) > (int)get_prec(ext1))
                break;

            /* compute the sum of the accumulator and this term into r6 */
            compute_sum_into(ext6, ext1, ext5);

            /* swap the result into the accumulator */
            tmp = ext1;
            ext1 = ext6;
            ext6 = tmp;

            /* on to the next term */
            ++n;

            /* compute the next factorial value */
            mul_by_long(ext4, n);

            /* compute the next power of x' into r5 */
            compute_prod_into(ext5, ext2, ext3);

            /* swap the result into our current-power register (r2) */
            tmp = ext2;
            ext2 = ext5;
            ext5 = tmp;
        }

        /* square the result as many times as we halved the argument */
        for ( ; twos != 0 ; --twos)
        {
            /* compute the square of r1 into r2 */
            compute_prod_into(ext2, ext1, ext1);

            /* swap the result back into r1 */
            tmp = ext1;
            ext1 = ext2;
            ext2 = tmp;
        }

        /* 
         *   set up our 10's exponent value - this is simply 1*10^new_exp,
         *   which we calculated earlier (which we represent as
         *   0.1*10^(new_exp+1)
         */
        copy_val(ext2, get_one(), FALSE);
        set_exp(ext2, (int)(new_exp + 1));

        /* multiply by the 10's exponent value */
        compute_prod_into(ext3, ext1, ext2);

        /* copy the result into the output register, rounding as needed */
        copy_val(dst, ext3, TRUE);
    }
    err_finally
    {
        /* release our temporary registers */
        release_temp_regs(6, hdl1, hdl2, hdl3, hdl4, hdl5, hdl6);
    }
    err_end;
}

/* ------------------------------------------------------------------------ */
/*
 *   Compute a hyperbolic sine or cosine 
 */
void CVmObjBigNum::compute_sinhcosh_into(char *dst, const char *src,
                                         int is_cosh, int is_tanh)
{
    size_t prec = get_prec(dst);
    uint hdl1, hdl2, hdl3, hdl4;
    char *ext1, *ext2, *ext3, *ext4;
    
    /* allocate some temporary registers */
    alloc_temp_regs(prec + 3, 4,
                    &ext1, &hdl1, &ext2, &hdl2, &ext3, &hdl3,
                    &ext4, &hdl4);
    
    /* catch errors so we can be sure to free registers */
    err_try
    {
        /* 
         *   sinh(x) = (e^x - e^(-x))/2
         *   
         *   cosh(x) = (e^x + e^(-x))/2
         *   
         *   The two differ only in the sign of the e^(-x) term, so most
         *   of the calculation is the same for both.  
         */
        
        /* compute e^x into r1 */
        compute_exp_into(ext1, src);

        /* 
         *   rather than calculating e^-x separately, simply invert our
         *   e^x value to yield e^-x (a simple division is much quicker
         *   than calculating another exponent, which involves an entire
         *   taylor series expansion) 
         */
        copy_val(ext3, get_one(), FALSE);
        compute_quotient_into(ext2, 0, ext3, ext1);

        /* 
         *   if we're calculating the tanh, we'll want both the sinh and
         *   cosh values 
         */
        if (is_tanh)
        {
            /* add the terms to get the cosh */
            compute_sum_into(ext4, ext1, ext2);

            /* subtract ext2 to get the sinh */
            negate(ext2);
            compute_sum_into(ext3, ext1, ext2);

            /* tanh is the quotient of sinh/cosh */
            compute_quotient_into(ext1, 0, ext3, ext4);

            /* our result is in ext1 - set ext3 to point there */
            ext3 = ext1;
        }
        else
        {
            /* 
             *   if this is sinh, the e^-x term is subtracted; if it's
             *   cosh, it's added 
             */
            if (!is_cosh)
                negate(ext2);
        
            /* compute r1 + r2 into r3 (e^x +/- e^(-x)) */
            compute_sum_into(ext3, ext1, ext2);
        
            /* halve the result */
            div_by_long(ext3, 2);
        }
        
        /* store the result */
        copy_val(dst, ext3, TRUE);
    }
    err_finally
    {
        /* release our temporary registers */
        release_temp_regs(4, hdl1, hdl2, hdl3, hdl4);
    }
    err_end;
}

/* ------------------------------------------------------------------------ */
/*
 *   Cache the natural logarithm of 10 to the given precision and return
 *   the value 
 */
const char *CVmObjBigNum::cache_ln10(size_t prec)
{
    char *ext;
    int expanded;
    uint hdl1, hdl2, hdl3, hdl4, hdl5;
    char *ext1, *ext2, *ext3, *ext4, *ext5;
    static const unsigned char ten[] =
    {
        /* number of digits */
        0x01, 0x00,

        /* exponent (0.1 * 10^2 = 10) */
        0x02, 0x00,

        /* flags */
        0x00,

        /* mantissa */
        0x10
    };

    /* 
     *   round up the precision a bit to ensure that we don't have to
     *   repeatedly recalculate this value if we're asked for a cluster of
     *   similar precisions 
     */
    prec = (prec + 7) & ~7;
    
    /* get the ln10 cache register */
    ext = S_bignum_cache->get_ln10_reg(calc_alloc(prec), &expanded);

    /* if that failed, throw an error */
    if (ext == 0)
        err_throw(VMERR_OUT_OF_MEMORY);

    /* 
     *   if we didn't have to allocate more memory, and the value in the
     *   register has at least the required precision, return the cached
     *   value 
     */
    if (!expanded && get_prec(ext) >= prec)
        return ext;

    /* 
     *   we have reallocated the register, or we just didn't have enough
     *   precision in the old value - set the new precision 
     */
    set_prec(ext, prec);

    /* allocate some temporary registers */
    alloc_temp_regs(prec + 3, 5,
                    &ext1, &hdl1, &ext2, &hdl2, &ext3, &hdl3,
                    &ext4, &hdl4, &ext5, &hdl5);

    /* catch errors so we can be sure to free registers */
    err_try
    {
        /* 
         *   Compute sqrt(10) - 10 is too large for the series to converge,
         *   but sqrt(10) is good.  We'll correct for this later by doubling
         *   the result of the series expansion, which gives us the correct
         *   result: ln(a^b) = b*ln(a), and sqrt(x) = x^(1/2), hence
         *   ln(sqrt(x)) = ln(x)/2, which means that ln(x) = 2*ln(sqrt(x)).
         *   
         *   Note that we have to do this the hard way, by explicitly doing
         *   the ln series rather than just calling compute_ln_into() to get
         *   the value directly.  Why?  Because compute_ln_into() needs the
         *   cached value of ln(10) to do its work.  If we called
         *   compute_ln_into() here, we'd get stuck in a recursion loop.  
         */

        /* compute sqrt(10), for quick series convergence */
        copy_val(ext2, (const char *)ten, FALSE);
        compute_sqrt_into(ext1, ext2);

        /* compute the series expansion */
        ext1 = compute_ln_series_into(ext1, ext2, ext3, ext4, ext5);

        /* double the result (to adjust for the sqrt) */
        mul_by_long(ext1, 2);

        /* store the result in the cache */
        copy_val(ext, ext1, TRUE);
    }
    err_finally
    {
        /* release our temporary registers */
        release_temp_regs(5, hdl1, hdl2, hdl3, hdl4, hdl5);
    }
    err_end;

    /* return the register pointer */
    return ext;
}

/* ------------------------------------------------------------------------ */
/*
 *   Cache the natural logarithm of 2 to the given precision and return the
 *   value 
 */
const char *CVmObjBigNum::cache_ln2(size_t prec)
{
    char *ext;
    int expanded;
    static const unsigned char two[] = { 0x01, 0x00, 0x01, 0x00, 0x00, 0x20 };

    /* round up the precision to minimize recalculations */
    prec = (prec + 7) & ~7;

    /* get the ln2 cache register */
    ext = S_bignum_cache->get_ln2_reg(calc_alloc(prec), &expanded);
    if (ext == 0)
        err_throw(VMERR_OUT_OF_MEMORY);

    /* if we had a cached value with enough precision, return it */
    if (!expanded && get_prec(ext) >= prec)
        return ext;

    /* reallocated - set the new precision and recalculate ln2 */
    set_prec(ext, prec);
    compute_ln_into(ext, (const char *)two);

    /* return the register pointer */
    return ext;
}

/* ------------------------------------------------------------------------ */
/*
 *   Cache pi to the required precision
 */
const char *CVmObjBigNum::cache_pi(size_t prec)
{
    char *ext;
    int expanded;
    uint hdl1, hdl2, hdl3, hdl4, hdl5;
    char *ext1, *ext2, *ext3, *ext4, *ext5;

    /* 
     *   round up the precision a bit to ensure that we don't have to
     *   repeatedly recalculate this value if we're asked for a cluster of
     *   similar precisions 
     */
    prec = (prec + 7) & ~7;

    /* get the pi cache register */
    ext = S_bignum_cache->get_pi_reg(calc_alloc(prec), &expanded);

    /* if that failed, throw an error */
    if (ext == 0)
        err_throw(VMERR_OUT_OF_MEMORY);

    /* 
     *   if we didn't have to allocate more memory, and the value in the
     *   register has at least the required precision, return the cached
     *   value 
     */
    if (!expanded && get_prec(ext) >= prec)
        return ext;

    /* 
     *   we have reallocated the register, or we just didn't have enough
     *   precision in the old value - set the new precision 
     */
    set_prec(ext, prec);

    /* allocate some temporary registers */
    alloc_temp_regs(prec + 3, 5,
                    &ext1, &hdl1, &ext2, &hdl2, &ext3, &hdl3,
                    &ext4, &hdl4, &ext5, &hdl5);

    /* catch errors so we can be sure to free registers */
    err_try
    {
        /* 
         *   Compute the arctangent of 1.  To do this, rewrite arctan(x) =
         *   arccos(1/sqrt(1+x^2)), so x=1 gives us arccos(1/sqrt(2)).
         *   
         *   Now, arccos(x) = pi/2 - arcsin(x), but this would defeat the
         *   purpose since we want to calculate pi, thus we don't want to
         *   depend upon pi in our calculations.  Fortunately, arcsin(x) =
         *   pi/2 - arcsin(sqrt(1-x^2)), hence
         *   
         *   pi/2 - arcsin(x) = pi/2 - (pi/2 - arcsin(sqrt(1-x^2)))
         *.  = arcsin(sqrt(1-x^2))
         *   
         *   So, we can rewrite arccos(1/sqrt(2)) as arcsin(sqrt(1/2)).
         */

        /* compute 1/2 into r2 */
        copy_val(ext2, get_one(), FALSE);
        div_by_long(ext2, 2);

        /* compute sqrt(1/2) into r1 */
        compute_sqrt_into(ext1, ext2);

        /* calculate the arcsin series for sqrt(1/2) */
        ext1 = calc_asin_series(ext1, ext2, ext3, ext4, ext5);

        /*  multiply the result by 4 */
        mul_by_long(ext1, 4);

        /* store the result in the cache */
        copy_val(ext, ext1, TRUE);
    }
    err_finally
    {
        /* release our temporary registers */
        release_temp_regs(5, hdl1, hdl2, hdl3, hdl4, hdl5);
    }
    err_end;

    /* return the register pointer */
    return ext;
}

/* ------------------------------------------------------------------------ */
/*
 *   Cache e to the required precision 
 */
const char *CVmObjBigNum::cache_e(size_t prec)
{
    /* 
     *   round up the precision a bit to ensure that we don't have to
     *   repeatedly recalculate this value if we're asked for a cluster of
     *   similar precisions 
     */
    prec = (prec + 7) & ~7;

    /* get the e cache register */
    int expanded;
    char *ext = S_bignum_cache->get_e_reg(calc_alloc(prec), &expanded);

    /* if that failed, throw an error */
    if (ext == 0)
        err_throw(VMERR_OUT_OF_MEMORY);

    /* 
     *   if we didn't have to allocate more memory, and the value in the
     *   register has at least the required precision, return the cached
     *   value 
     */
    if (!expanded && get_prec(ext) >= prec)
        return ext;

    /* 
     *   we have reallocated the register, or we just didn't have enough
     *   precision in the old value - set the new precision 
     */
    set_prec(ext, prec);

    /* exponentiate the base of the natural logarithm to the power 1 */
    compute_exp_into(ext, get_one());

    /* return the register pointer */
    return ext;
}

/* ------------------------------------------------------------------------ */
/*
 *   Compute a square root.  We use Newton's method (a reliable old method
 *   for extracting square roots, made better by the fact that, unlike the
 *   method's inventor, we are fortunate to have an electronic computer to
 *   carry out the tedious parts of the calculation for us).  
 */
void CVmObjBigNum::compute_sqrt_into(char *dst, const char *src)
{
    uint hdl1, hdl2, hdl3, hdl4;
    char *ext1, *ext2, *ext3, *ext4;
    size_t dst_prec = get_prec(dst);

    /* if the value is negative, it's an error */
    if (get_neg(src))
        err_throw(VMERR_OUT_OF_RANGE);
    
    /* allocate our scratchpad registers */
    alloc_temp_regs(get_prec(dst) + 3, 4,
                    &ext1, &hdl1, &ext2, &hdl2, &ext3, &hdl3,
                    &ext4, &hdl4);

    /* catch errors so we can free our registers */
    err_try
    {
        /* 
         *   Compute our initial guess.  Since our number is represented as
         *   
         *   (n * 10^exp),
         *   
         *   the square root can be written as
         *   
         *   sqrt(n) * sqrt(10^exp) = sqrt(n) * 10^(exp/2)
         *   
         *   Approximate sqrt(n) as simply n for our initial guess.  This
         *   will get us to the right order of magnitude plus or minus
         *   one, so we should converge pretty quickly.
         *   
         *   If we have an odd exponent, round up and divide the mantissa
         *   by 2 - this will be something like 0.456e7, which can be
         *   written as 4.56e6, whose square root is about 2e3, or .2e4.
         *   
         *   If we have an even exponent, multiply the mantissa by 2.
         *   This will be something like .456e8, whose square root is
         *   about .67e4.
         *   
         *   Note that it's well worth the trouble to make a good initial
         *   approximation, even with the multiply/divide, because these
         *   operations with longs are much more efficient than the full
         *   BigNum divide we'll have to do on each iteration.  
         */
        copy_val(ext1, src, TRUE);
        if ((get_exp(ext1) & 1) != 0)
        {
            /* odd exponent - round up and divide the mantissa by two */
            set_exp(ext1, (get_exp(ext1) + 1)/2);
            div_by_long(ext1, 2);
        }
        else
        {
            /* even exponent - multiply mantissa by two */
            set_exp(ext1, get_exp(ext1)/2);
            mul_by_long(ext1, 2);
        }

        /* iterate until we get close enough to the solution */
        for (;;)
        {
            char *tmp;
            size_t idx;
            
            /* 
             *   Calculate the next iteration's approximation, noting that r1
             *   contains the current iteration's value p:
             *   
             *   p' = p/2 + src/2p = (p + src/p)/2
             *   
             *   Note that if p == 0, we can't compute src/p, so we can't
             *   iterate any further.  
             */
            if (is_zero(ext1))
                break;

            /* calculate src/p into r3 */
            compute_quotient_into(ext3, 0, src, ext1);

            /* compute p + src/p into r4 */
            compute_sum_into(ext4, ext1, ext3);

            /* compute (p + src/p)/2 into r4 */
            div_by_long(ext4, 2);

            /* 
             *   check for convergence - if the new value equals the old
             *   value to the precision requested for the result, we are
             *   at the limit of our ability to distinguish differences in
             *   future terms, so we can stop 
             */
            if (get_neg(ext1) == get_neg(ext4)
                && get_exp(ext1) == get_exp(ext4))
            {
                /* 
                 *   they're the same sign and magnitude - compare the
                 *   digits to see where they first differ 
                 */
                for (idx = 0 ; idx < dst_prec + 1 ; ++idx)
                {
                    /* if they differ here, stop scanning */
                    if (get_dig(ext1, idx) != get_dig(ext4, idx))
                        break;
                }

                /* 
                 *   if we didn't find any difference up to the output
                 *   precision plus one digit (for rounding), further
                 *   iteration will be of no value 
                 */
                if (idx >= dst_prec + 1)
                    break;
            }

            /* swap the new value into r1 for the next round */
            tmp = ext1;
            ext1 = ext4;
            ext4 = tmp;
        }

        /* 
         *   copy the last iteration's value into the destination,
         *   rounding as needed 
         */
        copy_val(dst, ext1, TRUE);
    }
    err_finally
    {
        /* release our temporary registers */
        release_temp_regs(4, hdl1, hdl2, hdl3, hdl4);
    }
    err_end;
}

/* ------------------------------------------------------------------------ */
/*
 *   Calculate a Taylor expansion for sin().  The argument is in ext1, and
 *   we'll store the result in new_ext.  ext1 through ext7 are temporary
 *   registers that we'll overwrite with intermediate values.
 *   
 *   Before calling this function, the caller should reduce the value in
 *   ext1 to the range -pi/4 <= ext1 <= pi/4 to ensure quick convergence
 *   of the series.  
 */
void CVmObjBigNum::calc_sin_series(VMG_ char *new_ext, char *ext1,
                                   char *ext2, char *ext3, char *ext4,
                                   char *ext5, char *ext6, char *ext7)
{
    ulong n;
    int neg_term;
    char *tmp;

    /* start with 1! (i.e., 1) in r3 */
    copy_val(ext3, get_one(), FALSE);

    /* 
     *   calculate x^2 into r7 - we need x, x^3, x^5, etc, so we can just
     *   multiply the last value by x^2 to get the next power we need
     */
    compute_prod_into(ext7, ext1, ext1);
    
    /* start with x (reduced mod 2pi) in r5 (our accumulator) */
    copy_val(ext5, ext1, FALSE);
    
    /* start at term n=1 in our expansion */
    n = 1;
    
    /* the first term is positive */
    neg_term = FALSE;
    
    /* go until we have a precise enough value */
    for (;;)
    {
        /* 
         *   move on to the next term: multiply r1 by x^2 into r2 to yield
         *   the next power of x that we require 
         */
        compute_prod_into(ext2, ext1, ext7);
        
        /* swap r1 and r2 - r2 has the next power we need now */
        tmp = ext1;
        ext1 = ext2;
        ext2 = tmp;
        
        /* 
         *   multiply r3 by (n+1)*(n+2) - it currently has n!, so this
         *   will yield (n+2)!  
         */
        mul_by_long(ext3, (n+1)*(n+2));
        
        /* advance n to the next term */
        n += 2;
        
        /* each term swaps signs */
        neg_term = !neg_term;
        
        /* divide r1 by r3 to yield the next term in our series in r4 */
        compute_quotient_into(ext4, 0, ext1, ext3);
        
        /* 
         *   if this value is too small to count in our accumulator, we're
         *   done 
         */
        if (is_zero(ext4)
            || get_exp(ext5) - get_exp(ext4) > (int)get_prec(ext4))
            break;
        
        /* 
         *   invert the sign of the term in r4 if this is a negative term 
         */
        if (neg_term)
            set_neg(ext4, !get_neg(ext4));
        
        /* add r4 to our running accumulator in r5, yielding r6 */
        compute_sum_into(ext6, ext5, ext4);
        
        /* swap r5 and r6 to put the accumulated value back into r5 */
        tmp = ext5;
        ext5 = ext6;
        ext6 = tmp;
    }
    
    /* we're done - store the result, rounding if necessary */
    copy_val(new_ext, ext5, TRUE);
}

/*
 *   Calculate a Taylor expansion for cos().  The argument is in ext1, and
 *   we'll store the result in new_ext.  ext1 through ext7 are temporary
 *   registers that we'll overwrite with intermediate values.  
 *   
 *   Before calling this function, the caller should reduce the value in
 *   ext1 to the range -pi/4 <= ext1 <= pi/4 to ensure quick convergence
 *   of the series.  
 */
void CVmObjBigNum::calc_cos_series(VMG_ char *new_ext, char *ext1,
                                   char *ext2, char *ext3, char *ext4,
                                   char *ext5, char *ext6, char *ext7)
{
    ulong n;
    int neg_term;
    char *tmp;

    /* start with 2! (i.e., 2) in r3 */
    copy_val(ext3, get_one(), FALSE);
    set_dig(ext3, 0, 2);

    /* 
     *   calculate x^2 into r7 - we need x^2, x^4, x^6, etc, so we can
     *   just multiply the last value by x^2 to get the next power we need
     */
    compute_prod_into(ext7, ext1, ext1);

    /* 
     *   the first power we need is x^2, so copy it into r1 (our current
     *   power of x register) 
     */
    copy_val(ext1, ext7, FALSE);

    /* 
     *   start with 1 in r5, the accumulator (the first term of the series
     *   is the constant value 1) 
     */
    copy_val(ext5, get_one(), FALSE);

    /* 
     *   start at term n=2 in our expansion (the first term is just the
     *   constant value 1, so we can start at the second term) 
     */
    n = 2;

    /* 
     *   the first term we calculate (i.e., the second term in the actual
     *   series) is negative 
     */
    neg_term = TRUE;

    /* go until we have a precise enough value */
    for (;;)
    {
        /* divide r1 by r3 to yield the next term in our series in r4 */
        compute_quotient_into(ext4, 0, ext1, ext3);

        /* 
         *   if this value is too small to count in our accumulator, we're
         *   done 
         */
        if (is_zero(ext4)
            || get_exp(ext5) - get_exp(ext4) > (int)get_prec(ext4))
            break;

        /* 
         *   invert the sign of the term in r4 if this is a negative term 
         */
        if (neg_term)
            set_neg(ext4, !get_neg(ext4));

        /* add r4 to our running accumulator in r5, yielding r6 */
        compute_sum_into(ext6, ext5, ext4);

        /* swap r5 and r6 to put the accumulated value back into r5 */
        tmp = ext5;
        ext5 = ext6;
        ext6 = tmp;

        /* 
         *   move on to the next term: multiply r1 by x^2 into r2 to yield
         *   the next power of x that we require 
         */
        compute_prod_into(ext2, ext1, ext7);

        /* swap r1 and r2 to put our next required power back in r1 */
        tmp = ext1;
        ext1 = ext2;
        ext2 = tmp;

        /* 
         *   multiply r3 by (n+1)*(n+2) - it currently has n!, so this
         *   will yield (n+2)!  
         */
        mul_by_long(ext3, (n+1)*(n+2));

        /* advance n to the next term */
        n += 2;

        /* each term swaps signs */
        neg_term = !neg_term;
    }
    
    /* we're done - store the result, rounding if necessary */
    copy_val(new_ext, ext5, TRUE);
}

/* ------------------------------------------------------------------------ */
/* 
 *   add a value 
 */
int CVmObjBigNum::add_val(VMG_ vm_val_t *result,
                           vm_obj_id_t self, const vm_val_t *val)
{
    vm_val_t val2;

    /* convert it */
    val2 = *val;
    if (!cvt_to_bignum(vmg_ self, &val2))
    {
        /* this type is not convertible to BigNumber, so we can't add it */
        err_throw(VMERR_BAD_TYPE_ADD);
    }

    /* push 'self' and the other value to protect against GC */
    G_stk->push()->set_obj(self);
    G_stk->push(&val2);

    /* compute the sum */
    compute_sum(vmg_ result, ext_, get_objid_ext(vmg_ val2.val.obj));

    /* discard the GC protection items */
    G_stk->discard(2);

    /* handled */
    return TRUE;
}

/* 
 *   subtract a value 
 */
int CVmObjBigNum::sub_val(VMG_ vm_val_t *result,
                          vm_obj_id_t self, const vm_val_t *val)
{
    vm_val_t val2;

    /* convert it */
    val2 = *val;
    if (!cvt_to_bignum(vmg_ self, &val2))
    {
        /* this type is not convertible to BigNumber, so we can't use it */
        err_throw(VMERR_BAD_TYPE_SUB);
    }

    /* push 'self' and the other value to protect against GC */
    G_stk->push()->set_obj(self);
    G_stk->push(&val2);

    /* compute the difference */
    compute_diff(vmg_ result, ext_, get_objid_ext(vmg_ val2.val.obj));

    /* discard the GC protection items */
    G_stk->discard(2);

    /* handled */
    return TRUE;
}

/* 
 *   multiply a value 
 */
int CVmObjBigNum::mul_val(VMG_ vm_val_t *result,
                          vm_obj_id_t self, const vm_val_t *val)
{
    vm_val_t val2;

    /* convert it */
    val2 = *val;
    if (!cvt_to_bignum(vmg_ self, &val2))
    {
        /* this type is not convertible to BigNumber, so we can't add it */
        err_throw(VMERR_BAD_TYPE_ADD);
    }

    /* push 'self' and the other value to protect against GC */
    G_stk->push()->set_obj(self);
    G_stk->push(&val2);

    /* compute the product */
    compute_prod(vmg_ result, ext_, get_objid_ext(vmg_ val2.val.obj));

    /* discard the GC protection items */
    G_stk->discard(2);

    /* handled */
    return TRUE;
}

/* 
 *   divide a value 
 */
int CVmObjBigNum::div_val(VMG_ vm_val_t *result,
                          vm_obj_id_t self, const vm_val_t *val)
{
    /* convert it */
    vm_val_t val2 = *val;
    if (!cvt_to_bignum(vmg_ self, &val2))
    {
        /* this type is not convertible to BigNumber, so we can't add it */
        err_throw(VMERR_BAD_TYPE_ADD);
    }

    /* push 'self' and the other value to protect against GC */
    G_stk->push()->set_obj(self);
    G_stk->push(&val2);

    /* compute the quotient */
    compute_quotient(vmg_ result, ext_, get_objid_ext(vmg_ val2.val.obj));

    /* discard the GC protection items */
    G_stk->discard(2);

    /* handled */
    return TRUE;
}

/* 
 *   negate the number
 */
int CVmObjBigNum::neg_val(VMG_ vm_val_t *result, vm_obj_id_t self)
{
    char *new_ext;
    size_t prec = get_prec(ext_);

    /* 
     *   If I'm not an ordinary number or an infinity, return myself
     *   unchanged.  Note that we change sign for an infinity, even though
     *   this might not make a great deal of sense mathematically.
     *   
     *   If I'm zero, likewise return myself unchanged.  Negative zero is
     *   still zero.  
     */
    if ((get_type(ext_) != VMBN_T_NUM && get_type(ext_) != VMBN_T_INF)
        || is_zero(ext_))
    {
        /* return myself unchanged */
        result->set_obj(self);
        return TRUE;
    }

    /* push a self-reference while we're working */
    G_stk->push()->set_obj(self);

    /* if I'm an infinity, we don't need any precision in the result */
    if (is_infinity(ext_))
        prec = 1;

    /* create a new number with the same precision as the original */
    result->set_obj(create(vmg_ FALSE, prec));
    new_ext = get_objid_ext(vmg_ result->val.obj);

    /* make a copy in the new object */
    memcpy(new_ext, ext_, calc_alloc(prec));

    /* reverse the sign */
    set_neg(new_ext, !get_neg(new_ext));

    /* remove my self-reference */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/* 
 *   check a value for equality 
 */
int CVmObjBigNum::equals(VMG_ vm_obj_id_t self, const vm_val_t *val,
                         int /*depth*/) const
{
    vm_val_t val2;
    int ret;

    /* if the other value is a reference to self, we certainly match */
    if (val->typ == VM_OBJ && val->val.obj == self)
        return TRUE;

    /* convert it */
    val2 = *val;
    if (!cvt_to_bignum(vmg_ self, &val2))
    {
        /* this type is not convertible to BigNumber - it's not equal */
        return FALSE;
    }

    /* push our values for safekeeping from the garbage collector */
    G_stk->push()->set_obj(self);
    G_stk->push(&val2);

    /* check for equality and return the result */
    ret = compute_eq_exact(ext_,  get_objid_ext(vmg_ val2.val.obj));

    /* discard the stacked values */
    G_stk->discard(2);

    /* return the result */
    return ret;
}

/*
 *   Hash value calculation 
 */
uint CVmObjBigNum::calc_hash(VMG_ vm_obj_id_t self, int /*depth*/) const
{
    uint i;
    uint hash;

    /* add up the digits in the number */
    for (hash = 0, i = 0 ; i < get_prec(ext_) ; ++i)
    {
        /* add this digit into the hash so far */
        hash += get_dig(ext_, i);
    }

    /* add in the exponent as well */
    hash += (uint)get_exp(ext_);

    /* return the combined hash */
    return hash;
}

/* 
 *   compare to another value 
 */
int CVmObjBigNum::compare_to(VMG_ vm_obj_id_t self,
                             const vm_val_t *val) const
{
    vm_val_t val2;

    /* convert it */
    val2 = *val;
    if (!cvt_to_bignum(vmg_ self, &val2))
    {
        /* this type is not convertible to BigNumber - it's not comparable */
        err_throw(VMERR_INVALID_COMPARISON);
    }

    /* compare the values */
    return compare_ext(ext_, get_objid_ext(vmg_ val2.val.obj));
}

/* ------------------------------------------------------------------------ */
/*
 *   Initialize for a computation involving two operands.  Checks the
 *   operands for non-number values; if either is NAN or INF, allocates a
 *   result value that is the same non-number type and returns null.  If
 *   both are valid numbers, we'll allocate a result value with precision
 *   equal to the greater of the precisions of the operands, and we'll
 *   return a pointer to the new object's extension buffer.  
 */
char *CVmObjBigNum::compute_init_2op(VMG_ vm_val_t *result,
                                     const char *ext1, const char *ext2)
{
    size_t new_prec;
    char *new_ext;

    /* get the greater precision - this is the precision of the result */
    new_prec = get_prec(ext1);
    if (get_prec(ext2) > new_prec)
        new_prec = get_prec(ext2);

    /* 
     *   if either operand is not an ordinary number, we need minimal
     *   precision to represent the result, since we don't actually need
     *   to store a number 
     */
    if (is_nan(ext1) || is_nan(ext2))
        new_prec = 1;

    /* allocate a new object with the required precision */
    result->set_obj(create(vmg_ FALSE, new_prec));

    /* get the extension buffer */
    new_ext = get_objid_ext(vmg_ result->val.obj);

    /* check the first value for NAN/INF conditions */
    if (get_type(ext1) != VMBN_T_NUM)
    {
        /* set the result to the same non-number type and sign */
        set_type(new_ext, get_type(ext1));
        set_neg(new_ext, get_neg(ext1));

        /* indicate that no further calculation is necessary */
        return 0;
    }

    /* check the second number for NAN/INF conditions */
    if (get_type(ext2) != VMBN_T_NUM)
    {
        /* set the result to the same non-number type and sign */
        set_type(new_ext, get_type(ext2));
        set_neg(new_ext, get_neg(ext2));

        /* indicate that no further calculation is necessary */
        return 0;
    }

    /* the operands are valid - return the new extension buffer */
    return new_ext;
}

/*
 *   Compute a sum 
 */
void CVmObjBigNum::compute_sum(VMG_ vm_val_t *result,
                               const char *ext1, const char *ext2)
{
    char *new_ext;

    /* allocate our result value */
    new_ext = compute_init_2op(vmg_ result, ext1, ext2);

    /* we're done if we had a non-number operand */
    if (new_ext == 0)
        return;

    /* compute the sum into the result */
    compute_sum_into(new_ext, ext1, ext2);
}

/*
 *   Compute a difference 
 */
void CVmObjBigNum::compute_diff(VMG_ vm_val_t *result,
                                const char *ext1, const char *ext2)
{
    /* allocate our result value */
    char *new_ext = compute_init_2op(vmg_ result, ext1, ext2);

    /* we're done if we had a non-number operand */
    if (new_ext == 0)
        return;

    /* compute the difference into the result object */
    compute_diff_into(new_ext, ext1, ext2);
}

/*
 *   Compute a product 
 */
void CVmObjBigNum::compute_prod(VMG_ vm_val_t *result,
                                const char *ext1, const char *ext2)
{
    char *new_ext;

    /* allocate our result value */
    new_ext = compute_init_2op(vmg_ result, ext1, ext2);

    /* we're done if we had a non-number operand */
    if (new_ext == 0)
        return;

    /* compute the product */
    compute_prod_into(new_ext, ext1, ext2);
}

/*
 *   Compute a quotient
 */
void CVmObjBigNum::compute_quotient(VMG_ vm_val_t *result,
                                    const char *ext1, const char *ext2)
{
    char *new_ext;

    /* allocate our result value */
    new_ext = compute_init_2op(vmg_ result, ext1, ext2);

    /* we're done if we had a non-number operand */
    if (new_ext == 0)
        return;

    /* compute the quotient */
    compute_quotient_into(new_ext, 0, ext1, ext2);
}

/*
 *   Determine if two values are equal, rounding the value with greater
 *   precision to the shorter precision if the two are not of equal
 *   precision 
 */
int CVmObjBigNum::compute_eq_round(VMG_ const char *ext1, const char *ext2)
{
    size_t prec1 = get_prec(ext1);
    size_t prec2 = get_prec(ext2);
    const char *shorter;
    const char *longer;
    char *tmp_ext;
    uint tmp_hdl;
    int ret;
    
    /* 
     *   allocate a temporary register with a rounded copy of the more
     *   precise of the values 
     */
    if (prec1 > prec2)
    {
        /* the first one is longer */
        longer = ext1;
        shorter = ext2;
    }
    else if (prec1 < prec2)
    {
        /* the second one is longer */
        longer = ext2;
        shorter = ext1;
    }
    else
    {
        /* they're the same - do an exact comparison */
        return compute_eq_exact(ext1, ext2);
    }

    /* get a temp register for rounding the longer value */
    alloc_temp_regs(get_prec(shorter), 1, &tmp_ext, &tmp_hdl);

    /* make a rounded copy */
    copy_val(tmp_ext, longer, TRUE);

    /* compare the rounded copy of the longer value to the shorter value */
    ret = compute_eq_exact(shorter, tmp_ext);

    /* release the temporary register */
    release_temp_regs(1, tmp_hdl);

    /* return the result */
    return ret;
}

/*
 *   Make an exact comparison for equality.  If one value is more precise
 *   than the other, we'll implicitly extend the shorter value to the
 *   right with trailing zeros.  
 */
int CVmObjBigNum::compute_eq_exact(const char *ext1, const char *ext2)
{
    const char *longer;
    size_t min_prec;
    size_t max_prec;
    size_t prec1;
    size_t prec2;
    size_t idx;

    /* 
     *   if either is not an ordinary number, they are never equal to any
     *   other value (note that this means INF != INF and NAN != NAN,
     *   which is reasonable because these values cannot be meaningfully
     *   compared; one NAN might mean something totally different from
     *   another, and likewise various infinities are not comparable) 
     */
    if (is_nan(ext1) || is_nan(ext2))
        return FALSE;

    /* figure out if one is more precise than the other */
    prec1 = get_prec(ext1);
    prec2 = get_prec(ext2);
    if (prec1 > prec2)
    {
        /* ext1 is longer */
        longer = ext1;
        max_prec = prec1;
        min_prec = prec2;
    }
    else if (prec2 > prec1)
    {
        /* ext2 is longer */
        longer = ext2;
        max_prec = prec2;
        min_prec = prec1;
    }
    else
    {
        /* they're the same */
        longer = 0;
        min_prec = max_prec = prec1;
    }

    /* if the signs aren't the same, the numbers are not equal */
    if (get_neg(ext1) != get_neg(ext2))
        return FALSE;

    /* if the exponents aren't equal, the numbers are not equal */
    if (get_exp(ext1) != get_exp(ext2))
        return FALSE;

    /* 
     *   compare digits up to the smaller precision, then make sure that
     *   the larger-precision value's digits are all zeros from there out 
     */
    for (idx = 0 ; idx < min_prec ; ++idx)
    {
        /* if they don't match, return not-equal */
        if (get_dig(ext1, idx) != get_dig(ext2, idx))
            return FALSE;
    }

    /* check the longer one to make sure it's all zeros */
    if (longer != 0)
    {
        /* scan the remainder of the longer one */
        for ( ; idx < max_prec ; ++idx)
        {
            /* if this digit is non-zero, it's not a match */
            if (get_dig(longer, idx) != 0)
                return FALSE;
        }
    }
    
    /* it's a match */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Round a value 
 */
const char *CVmObjBigNum::round_val(VMG_ vm_val_t *new_val, const char *ext,
                                    size_t digits, int always_create)
{
    /* presume we need rounding */
    int need_round = TRUE;

    /* 
     *   if the value is already no longer than the requested precision,
     *   return the original value; similarly, if we don't have to do any
     *   rounding to truncate to the requested precision, do not change
     *   the original object; likewise, don't bother changing anything if
     *   it's not a number 
     */
    if (get_type(ext) != VMBN_T_NUM || !get_round_dir(ext, digits))
    {
        if (always_create)
        {
            /* 
             *   we must create a new object regardless, but it won't need
             *   rounding 
             */
            need_round = FALSE;
        }
        else
        {
            /* return the original value */
            new_val->set_nil();
            return ext;
        }
    }

    /* allocate a new object with the requested precision */
    new_val->set_obj(create(vmg_ FALSE, digits));
    char *new_ext = get_objid_ext(vmg_ new_val->val.obj);

    /* copy the sign, exponent, and type information */
    set_prec(new_ext, digits);
    set_neg(new_ext, get_neg(ext));
    set_exp(new_ext, get_exp(ext));
    set_type(new_ext, get_type(ext));

    /* 
     *   if we don't need rounding, just truncate the old mantissa and
     *   return the result 
     */
    if (!need_round)
    {
        /* if the new size is smaller, truncate it */
        if (digits <= get_prec(ext))
        {
            /* copy the mantissa up to the requested new size */
            memcpy(new_ext + VMBN_MANT, ext + VMBN_MANT, (digits + 1)/2);
        }
        else
        {
            /* it's growing - simply copy the old value */
            copy_val(new_ext, ext, FALSE);
        }

        /* return the new value */
        return new_ext;
    }
    
    /* copy the mantissa up to the requested new precision */
    memcpy(new_ext + VMBN_MANT, ext + VMBN_MANT, (digits + 1)/2);

    /* round it up */
    round_up_abs(new_ext, digits);

    /* return the new extension */
    return new_ext;
}

/*
 *   Convert a value to a big number value 
 */
int CVmObjBigNum::cvt_to_bignum(VMG_ vm_obj_id_t self, vm_val_t *val) const
{
    /* if it's an integer, convert it to a BigNum value */
    if (val->typ == VM_INT)
    {
        /* 
         *   put my own value on the stack to ensure I'm not garbage
         *   collected when creating the new object 
         */
        G_stk->push()->set_obj(self);
        
        /* it's an integer - convert it to a BigNum */
        val->set_obj(create(vmg_ FALSE, (long)val->val.intval, 32));

        /* done protecting my object reference */
        G_stk->discard();
    }

    /* if it's not a BigNumber object, we can't handle it */
    if (val->typ != VM_OBJ
        || (vm_objp(vmg_ val->val.obj)->get_metaclass_reg()
            != metaclass_reg_))
    {
        /* indicate that conversion was unsuccessful */
        return FALSE;
    }

    /* successful conversion */
    return TRUE;
}

