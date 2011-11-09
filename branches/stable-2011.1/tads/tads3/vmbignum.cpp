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
    &CVmObjBigNum::getp_e
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
     *   in from memory, I hope I got everything right :) 
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
        utf8_ptr p;
        size_t rem;
        size_t prec;
        int pt;
        int significant;
        int digcnt;
        
        /* set up to scan the string */
        p.set((char *)strval + VMB_LEN);
        rem = vmb_get_len(strval);

        /* skip leading spaces */
        for ( ; rem != 0 && is_space(p.getch()) ; p.inc(&rem)) ;

        /* skip the sign if present */
        if (rem != 0 && (p.getch() == '-' || p.getch() == '+'))
            p.inc(&rem);

        /* we haven't yet found a significant leading digit */
        significant = FALSE;

        /* scan digits */
        for (prec = 0, digcnt = 0, pt = FALSE ; rem != 0 ; p.inc(&rem))
        {
            wchar_t ch;

            /* get this character */
            ch = p.getch();
            
            /* see what we have */
            if (is_digit(ch))
            {
                /* 
                 *   if it's not a zero, note that we've found a
                 *   significant digit 
                 */
                if (ch != '0')
                    significant = TRUE;
                
                /* 
                 *   if we have found a significant digit so far, count
                 *   this one - do not count leading zeroes, whether they
                 *   occur before or after the decimal point 
                 */
                if (significant)
                    ++prec;

                /* count the digit in any case */
                ++digcnt;
            }
            else if (ch == '.' && !pt)
            {
                /* decimal point - note it and keep going */
                pt = TRUE;
            }
            else if (ch == 'e' || ch == 'E')
            {
                /* exponent - that's the end of the mantissa */
                break;
            }
            else
            {
                /* we've reached the end of the number */
                break;
            }
        }

        /* 
         *   if the precision is zero, the number must be zero - use the
         *   actual number of digits for the default precision, so that a
         *   value specified as "0.0000000" has eight digits of precision
         */
        if (prec == 0)
            prec = digcnt;

        /* use the precision necessary to store the entire string */
        digits = prec;
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
        new (vmg_ id) CVmObjBigNum(vmg_ val->val.intval, digits);
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
    set_int_val(val);
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
    set_int_val(0);

    /* clear the flags */
    ext_[VMBN_FLAGS] = 0;
}

/*
 *   Calculate the amount of memory we need for a given number of digits
 *   of precision 
 */
size_t CVmObjBigNum::calc_alloc(size_t digits)
{
    /* 
     *   we need the header (UINT2, INT2, BYTE), plus one byte for each
     *   two decimal digits 
     */
    return (2 + 2 + 1) + ((digits + 1)/2);
}


/* ------------------------------------------------------------------------ */
/*
 *   Write to a 'data' mode file 
 */
int CVmObjBigNum::write_to_data_file(osfildef *fp)
{
    char buf[16];

    /* write the number of digits (i.e., the precision) */
    oswp2(buf, get_prec(ext_));
    if (osfwb(fp, buf, 2))
        return 1;

    /* write our entire extension */
    if (osfwb(fp, ext_, calc_alloc(get_prec(ext_))))
        return 1;

    /* success */
    return 0;
}

/*
 *   Read from a 'data' mode file and instantiate a new BigNumber object to
 *   hold the result 
 */
int CVmObjBigNum::read_from_data_file(VMG_ vm_val_t *retval, osfildef *fp)
{
    char buf[16];
    size_t prec;
    CVmObjBigNum *bignum;

    /* read the precision */
    if (osfrb(fp, buf, 2))
        return 1;
    prec = osrp2(buf);

    /* create a BigNumber with the required precision */
    retval->set_obj(create(vmg_ FALSE, prec));
    bignum = (CVmObjBigNum *)vm_objp(vmg_ retval->val.obj);

    /* read the bytes into the new object's extension */
    if (osfrb(fp, bignum->get_ext(), calc_alloc(prec)))
        return 1;

    /* success */
    return 0;
}


/* ------------------------------------------------------------------------ */
/*
 *   Set my value to a given integer value
 */
void CVmObjBigNum::set_int_val(long val)
{
    size_t prec;
    int exp;

    /* get the precision */
    prec = get_prec(ext_);
    
    /* set the type to number */
    set_type(ext_, VMBN_T_NUM);

    /* set the sign bit */
    if (val < 0)
    {
        /* set the value negative */
        set_neg(ext_, TRUE);

        /* use the absolute value for the mantissa */
        val = -val;
    }
    else
    {
        /* set the value positive */
        set_neg(ext_, FALSE);
    }

    /* initially zero the mantissa */
    memset(ext_ + VMBN_MANT, 0, (prec + 1)/2);

    /* initialize the exponent to zero */
    exp = 0;

    /* shift the integer value into the value */
    while (val != 0)
    {
        unsigned int dig;
        
        /* get the low-order digit of the value */
        dig = (unsigned int)(val % 10);

        /* shift the value one place */
        val /= 10;

        /* 
         *   shift our number one place right to accommodate this next
         *   higher-order digit 
         */
        shift_right(ext_, 1);

        /* set the new most significant digit */
        set_dig(ext_, 0, dig);

        /* that's another factor of 10 */
        ++exp;
    }

    /* set the exponent */
    set_exp(ext_, exp);

    /* normalize the number */
    normalize(ext_);
}

/*
 *   Set my value to a string 
 */
void CVmObjBigNum::set_str_val(const char *str, size_t len)
{
    size_t prec;
    int exp;
    utf8_ptr p;
    size_t rem;
    int neg;
    size_t idx;
    int pt;
    int significant;

    /* get the precision */
    prec = get_prec(ext_);

    /* set the type to number */
    set_type(ext_, VMBN_T_NUM);

    /* initially zero the mantissa */
    memset(ext_ + VMBN_MANT, 0, (prec + 1)/2);

    /* set up to scan the string */
    p.set((char *)str);
    rem = len;

    /* initialize the exponent to zero */
    exp = 0;

    /* presume it will be positive */
    neg = FALSE;

    /* skip leading spaces */
    while (rem != 0 && is_space(p.getch()))
        p.inc(&rem);

    /* check for a sign */
    if (rem != 0 && p.getch() == '+')
    {
        /* skip the sign */
        p.inc(&rem);
    }
    else if (rem != 0 && p.getch() == '-')
    {
        /* note the sign and skip it */
        neg = TRUE;
        p.inc(&rem);
    }

    /* set the sign */
    set_neg(ext_, neg);

    /* we haven't yet found a significant digit */
    significant = FALSE;

    /* shift the digits into the value */
    for (idx = 0, pt = FALSE ; rem != 0 ; p.inc(&rem))
    {
        wchar_t ch;

        /* get this character */
        ch = p.getch();

        /* check for a digit */
        if (is_digit(ch))
        {
            /* 
             *   if it's not a zero, we're definitely into the significant
             *   part of the number 
             */
            if (ch != '0')
                significant = TRUE;

            /* 
             *   if it's significant, add it to the number - skip leading
             *   zeroes, since they add no information to the number 
             */
            if (significant)
            {
                /* if we're not out of precision, add the digit */
                if (idx < prec)
                {
                    /* set the next digit */
                    set_dig(ext_, idx, value_of_digit(ch));
                    
                    /* move on to the next digit position */
                    ++idx;
                }
                
                /* 
                 *   that's another factor of 10 if we haven't found the
                 *   decimal point (whether or not we're out of precision
                 *   to actually store the digit, count the increase in
                 *   the exponent) 
                 */
                if (!pt)
                    ++exp;
            }
            else if (pt)
            {
                /* 
                 *   we haven't yet found a significant digit, so this is
                 *   a leading zero, but we have found the decimal point -
                 *   this reduces the exponent by one 
                 */
                --exp;
            }
        }
        else if (ch == '.' && !pt)
        {
            /* 
             *   this is the decimal point - note it; from now on, we
             *   won't increase the exponent as we add digits 
             */
            pt = TRUE;
        }
        else if (ch == 'e' || ch == 'E')
        {
            int acc;
            int exp_neg = FALSE;
            
            /* exponent - skip the 'e' */
            p.inc(&rem);

            /* check for a sign */
            if (rem != 0 && p.getch() == '+')
            {
                /* skip the sign */
                p.inc(&rem);
            }
            else if (rem != 0 && p.getch() == '-')
            {
                /* skip it and note it */
                p.inc(&rem);
                exp_neg = TRUE;
            }

            /* parse the exponent */
            for (acc = 0 ; rem != 0 ; p.inc(&rem))
            {
                wchar_t ch;
                
                /* if this is a digit, add it to the exponent */
                ch = p.getch();
                if (is_digit(ch))
                {
                    /* accumulate the digit */
                    acc *= 10;
                    acc += value_of_digit(ch);
                }
                else
                {
                    /* that's it for the exponent */
                    break;
                }
            }

            /* if it's negative, apply the sign */
            if (exp_neg)
                acc = -acc;

            /* add the exponent to the one we've calculated */
            exp += acc;

            /* after the exponent, we're done with the whole number */
            break;
        }
        else
        {
            /* it looks like we've reached the end of the number */
            break;
        }
    }

    /* set the exponent */
    set_exp(ext_, exp);

    /* normalize the number */
    normalize(ext_);
}


/* ------------------------------------------------------------------------ */
/*
 *   Convert to an integer value 
 */
long CVmObjBigNum::convert_to_int()
{
    size_t prec = get_prec(ext_);
    int is_neg = get_neg(ext_);
    int exp = get_exp(ext_);
    size_t idx;
    size_t stop_idx;
    long acc;
    int round_inc;
    
    /*
     *   T3 VM integer value limits.
     *   
     *   Note: The T3 VM integer type is ALWAYS a signed 32 bit value,
     *   regardless of the local integer size.  So we have to use HARD-CODED
     *   numbers here, NOT the limits.h values (LONG_MAX, LONG_MIN).  
     */
    const long long_max = 2147483647L;
    const long long_min = (-2147483647L - 1);

    /* start the accumulator at zero */
    acc = 0;

    /* presume no rounding */
    round_inc = 0;

    /* check to see if the first fractional digit is represented */
    if (exp >= 0 && (size_t)exp < prec)
    {
        /* if the digit is 5 or over, round up */
        if (get_dig(ext_, (size_t)exp) >= 5)
            round_inc = (is_neg ? -1 : 1);
    }

    /* stop at the first fractional digit */
    if (exp <= 0)
    {
        /* all digits are fractional */
        stop_idx = 0;
    }
    else if ((size_t)exp < prec)
    {
        /* stop at the first fractional digit */
        stop_idx = (size_t)exp;
    }
    else
    {
        /* all of the digits are in the whole part */
        stop_idx = prec;
    }

    /* convert the integer part digit by digit */
    if (stop_idx > 0)
    {
        /* loop over digits */
        for (idx = 0 ; idx < stop_idx ; ++idx)
        {
            /* get this digit */
            int dig = get_dig(ext_, idx);

            /* make sure that shifting the accumulator won't overflow */
            if (is_neg ? acc < (long_min/10) : acc > (long_max/10))
                err_throw(VMERR_NUM_OVERFLOW);

            /* shift the accumulator */
            acc *= 10;
            
            /* make sure this digit won't overflow the 32-bit VM int type */
            if (is_neg ? acc < (long_min + dig) : acc > (long_max - dig))
                err_throw(VMERR_NUM_OVERFLOW);

            /* add the digit */
            if (is_neg)
                acc -= dig;
            else
                acc += dig;
        }
    }

    /* make sure rounding won't overflow */
    if (is_neg ? acc < long_min - round_inc : acc > long_max - round_inc)
        err_throw(VMERR_NUM_OVERFLOW);

    /* return the result adjusted for rounding */
    return acc + round_inc;
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
 *   convert to a string, storing the result in the given buffer 
 */
char *CVmObjBigNum::cvt_to_string_buf(VMG_ char *buf, size_t buflen,
                                      int max_digits, int whole_places,
                                      int frac_digits, int exp_digits,
                                      ulong flags)
{
    /* convert to a string into our buffer */
    return cvt_to_string_gen(vmg_ 0, ext_, max_digits, whole_places,
                             frac_digits, exp_digits, flags, 0,
                             buf, buflen);
}

/*
 *   Convert to a string, creating a new string object to hold the result 
 */
const char *CVmObjBigNum::cvt_to_string(VMG_ vm_obj_id_t self,
                                        vm_val_t *new_str,
                                        const char *ext,
                                        int max_digits, int whole_places,
                                        int frac_digits, int exp_digits,
                                        ulong flags, vm_val_t *lead_fill)
{
    const char *ret;

    /* push a self-reference for gc protection during allocation */
    G_stk->push()->set_obj(self);

    /* 
     *   convert to a string - don't pass in a buffer, since we want to
     *   create a new string to hold the result
     */
    ret = cvt_to_string_gen(vmg_ new_str, ext, max_digits, whole_places,
                            frac_digits, exp_digits, flags, lead_fill, 0, 0);

    /* discard our gc protection */
    G_stk->discard();

    /* return the result */
    return ret;
}

/*
 *   Common routine to create a string representation.  If buf is null,
 *   we'll allocate a new string object, filling in new_str with the
 *   object reference; otherwise, we'll format into the given buffer.  
 */
char *CVmObjBigNum::cvt_to_string_gen(VMG_ vm_val_t *new_str,
                                      const char *ext,
                                      int max_digits, int whole_places,
                                      int frac_digits, int exp_digits,
                                      ulong flags, vm_val_t *lead_fill,
                                      char *buf, size_t buflen)
{
    int always_sign = ((flags & VMBN_FORMAT_SIGN) != 0);
    int always_sign_exp = ((flags & VMBN_FORMAT_EXP_SIGN) != 0);
    int always_exp = ((flags & VMBN_FORMAT_EXP) != 0);
    int lead_zero = ((flags & VMBN_FORMAT_LEADING_ZERO) != 0);
    int always_show_pt = ((flags & VMBN_FORMAT_POINT) != 0);
    int exp_caps = ((flags & VMBN_FORMAT_EXP_CAP) != 0);
    int pos_lead_space = ((flags & VMBN_FORMAT_POS_SPACE) != 0);
    int commas = ((flags & VMBN_FORMAT_COMMAS) != 0);
    int euro = ((flags & VMBN_FORMAT_EUROSTYLE) != 0);
    size_t req_chars;
    int prec = (int)get_prec(ext);
    int exp = get_exp(ext);
    int dig_before_pt;
    int dig_after_pt;
    int idx;
    unsigned int dig;
    char *p;
    int exp_carry;
    int show_pt;
    int whole_padding;
    int non_sci_digs;
    char decpt_char = (euro ? ',' : '.');
    char comma_char = (euro ? '.' : ',');
    const char *lead_fill_str = 0;
    size_t lead_fill_len;
    char *tmp_ext = 0;
    uint tmp_hdl;

    /* get the fill string, if a value was provided */
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
    else
    {
        /* no lead fill needed */
        lead_fill_len = 0;
    }
    
    /* 
     *   If we're not required to use exponential notation, but we don't
     *   have enough max_digits places for the part before the decimal
     *   point, use exponential anyway.  (The number of digits before the
     *   decimal point is simply the exponent if it's greater than zero,
     *   or zero otherwise.)  
     */
    if (exp > max_digits)
        always_exp = TRUE;

    /* 
     *   If we're not required to use exponential notation, but our
     *   absolute value is so small that we wouldn't show anything
     *   "0.00000..." (i.e., we'd have too many zeroes after the decimal
     *   point to show any actual digits of our number), use exponential
     *   notation.  If our exponent is negative, its absolute value is the
     *   number of zeroes we'd show after the decimal point before the
     *   first non-zero digit.  
     */
    if (exp < 0
        && (-exp > max_digits
            || (frac_digits != -1 && -exp > frac_digits)))
        always_exp = TRUE;

    /* calculate how many digits we'd need in non-scientific notation */
    if (exp < 0)
    {
        /* we have leading zeroes before the first significant digit */
        non_sci_digs = -exp + prec;
    }
    else if (exp > prec)
    {
        /* we have trailing zeroes after the last significant digit */
        non_sci_digs = exp + prec;
    }
    else
    {
        /* 
         *   we have no leading or trailing zeroes to represent - only the
         *   digits actually stored need to be displayed 
         */
        non_sci_digs = prec;
    }

    /* 
     *   Figure out how much space we need for our string: use the smaller
     *   of max_digits or the actual space we need for non-scientific
     *   notation, plus overhead space for the sign, a leading zero, a
     *   decimal point, an 'E' for the exponent, an exponent sign, and up
     *   to five digits for the exponent (16-bit integer -> -32768 to
     *   32767).  Also add one extra digit in case we need to add a digit
     *   due to rounding.  
     */
    if (max_digits < non_sci_digs)
        req_chars = max_digits;
    else
        req_chars = non_sci_digs;
    req_chars += 11;

    /*
     *   Make sure we leave enough room for the lead fill string - if they
     *   specified a number of whole places, and we're not using
     *   exponential format, we'll insert lead fill characters before the
     *   first non-zero whole digit. 
     */
    if (!always_exp && whole_places != -1 && exp < whole_places)
    {
        int extra;
        int char_size;
        
        /* 
         *   if the exponent is negative, we'll pad by the full amount;
         *   otherwise, we'll just pad by the difference between the
         *   number of places needed and the exponent 
         */
        extra = whole_places;
        if (exp > 0)
            extra -= exp;

        /* 
         *   Add the extra bytes: one byte per character if we're using
         *   the default space padding, or up to three bytes per character
         *   if a lead string was specified (each unicode character can
         *   take up to three bytes) 
         */
        char_size = (lead_fill_str != 0 ? 3 : 1);
        req_chars += extra * char_size;

        /* 
         *   add space for each padding character we could insert into a
         *   comma position (there's at most one comma per three fill
         *   characters) 
         */
        if (commas)
            req_chars += ((extra + 2)/3) * char_size;
    }

    /* 
     *   If we're using commas, and we're not using scientific notation,
     *   add space for a comma for each three digits before the decimal
     *   point 
     */
    if (commas && !always_exp)
    {
        /* add space for the commas */
        req_chars += ((exp + 2) / 3);
    }

    /* 
     *   if they requested a specific minimum number of exponent digits,
     *   and that number is greater than the allowance of 5 we already
     *   provided, add the extra space needed 
     */
    if (exp_digits > 5)
        req_chars += (exp_digits - 5);

    /*
     *   If they requested a specific number of digits after the decimal
     *   point, make sure we have room for those digits.
     */
    if (frac_digits != -1)
        req_chars += frac_digits;

    /* check to see if the caller passed in a buffer */
    if (buf != 0)
    {
        /* 
         *   the caller passed in a buffer - if it's not big enough to
         *   hold the result, return failure 
         */
        if (buflen < req_chars + VMB_LEN)
            return 0;
    }
    else
    {
        /* no buffer - allocate a new string */
        buf = CVmObjString::alloc_str_buf(vmg_ new_str, 0, 0, req_chars);
    }

    /* check for special values */
    switch(get_type(ext))
    {
    case VMBN_T_NUM:
        /* normal number - proceed */
        break;

    case VMBN_T_NAN:
        /* not a number - show "1.#NAN" */
        strcpy(buf + VMB_LEN, "1.#NAN");
        oswp2(buf, 6);
        return buf;

    case VMBN_T_INF:
        /* positive or negative infinity */
        if (get_neg(ext))
        {
            strcpy(buf + VMB_LEN, "-1.#INF");
            oswp2(buf, 7);
        }
        else
        {
            strcpy(buf + VMB_LEN, "1.#INF");
            oswp2(buf, 6);
        }
        return buf;

    default:
        /* other values are not valid */
        strcpy(buf + VMB_LEN, "1.#???");
        oswp2(buf, 6);
        return buf;
    }

    /*
     *   Allocate a temporary buffer to contain a copy of the number.  At
     *   most, we'll have to show max_digits of the number, or the current
     *   precision, whichever is lower.  
     */
    {
        int new_prec;

        /* 
         *   limit the new precision to the maximum digits to be shown, or
         *   our existing precision, whichever is lower 
         */
        new_prec = max_digits;
        if (prec < new_prec)
            new_prec = prec;

        /* allocate the space */
        alloc_temp_regs(vmg_ (size_t)new_prec, 1, &tmp_ext, &tmp_hdl);

        /* copy the value to the temp register, rounding the value */
        copy_val(tmp_ext, ext, TRUE);

        /* note the new precision */
        prec = new_prec;

        /* forget the original number and use the rounded version */
        ext = tmp_ext;
    }

start_over:
    /* 
     *   note the exponent, in case we rounded or otherwise adjusted the
     *   temporary number 
     */
    exp = get_exp(ext);
    
    /* presume we won't carry into the exponent */
    exp_carry = FALSE;

    /* no whole-part padding yet */
    whole_padding = 0;
    
    /* 
     *   Figure out where the decimal point goes in the display.  If we're
     *   displaying in exponential format, we'll always display exactly
     *   one digit before the decimal point.  Otherwise, we'll display the
     *   number given by our exponent if it's positive, or zero or one
     *   (depending on lead_zero) if it's negative or zero.  
     */
    if (always_exp)
        dig_before_pt = 1;
    else if (exp > 0)
        dig_before_pt = exp;
    else
        dig_before_pt = 0;

    /* 
     *   if the digits before the decimal point exceed our maximum number
     *   of digits allowed, we'll need to use exponential format
     */
    if (dig_before_pt > max_digits)
    {
        always_exp = TRUE;
        goto start_over;
    }

    /* 
     *   Limit digits after the decimal point according to the maximum
     *   digits allowed and the number we'll show before the decimal
     *   point.
     */
    dig_after_pt = max_digits - dig_before_pt;

    /* start writing after the buffer length prefix */
    p = buf + VMB_LEN;

    /* 
     *   if we're not in exponential mode, add leading spaces for the
     *   unused whole places 
     */
    if (!always_exp && dig_before_pt < whole_places)
    {
        int cnt;
        size_t src_rem;
        utf8_ptr src;
        utf8_ptr dst;
        int idx;

        /* start with the excess whole places */
        cnt = whole_places - dig_before_pt;

        /* if we're going to add a leading zero, that's one less space */
        if (dig_before_pt == 0 && lead_zero)
            --cnt;

        /*
         *   Increase the count by the number of comma positions in the
         *   padding area.  
         */
        if (commas)
        {
            /* scan over the positions to fill and count commas */
            for (idx = dig_before_pt ; idx < whole_places ; ++idx)
            {
                /* if this is a comma position, note it */
                if ((idx % 3) == 0)
                    ++cnt;
            }
        }

        /* set up our read and write pointers */
        src.set((char *)lead_fill_str);
        src_rem = lead_fill_len;
        dst.set(p);

        /* add (and count) the leading spaces */
        for ( ; cnt != 0 ; --cnt, ++whole_padding)
        {
            wchar_t ch;
            
            /* 
             *   if we have a lead fill string, read from it; otherwise,
             *   just use a space 
             */
            if (lead_fill_str != 0)
            {
                /* if we've exhausted the string, start over */
                if (src_rem == 0)
                {
                    src.set((char *)lead_fill_str);
                    src_rem = lead_fill_len;
                }

                /* get the next character */
                ch = src.getch();

                /* skip this character */
                src.inc(&src_rem);
            }
            else
            {
                /* no lead fill string - insert a space by default */
                ch = ' ';
            }

            /* write this character */
            dst.setch(ch);
        }

        /* resynchronize from our utf8 pointer */
        p = dst.getptr();
    }

    /* 
     *   if the number is negative, or we're always showing a sign, add
     *   the sign; if we're not adding a sign, but we're showing a leading
     *   space for positive numbers, add the leading space 
     */
    if (get_neg(ext))
        *p++ = '-';
    else if (always_sign)
        *p++ = '+';
    else if (pos_lead_space)
        *p++ = ' ';

    /* 
     *   if we have no digits before the decimal, and we're adding a
     *   leading zero, add it now 
     */
    if (dig_before_pt == 0 && lead_zero)
    {
        /* add the leading zero */
        *p++ = '0';

        /* reduce the limit on the digits after the decimal point */
        --dig_after_pt;
    }

    /*
     *   If we have limited the number of digits that we'll allow after the
     *   decimal point, due to the limit on the total number of digits and
     *   the number of digits we need to display before the decimal, start
     *   over in exponential format to ensure we get the after-decimal
     *   display we want.
     *   
     *   Note that we won't bother going into exponential format if the
     *   number of digits before the decimal point is zero or one, because
     *   exponential format won't give us any more room - in such cases we
     *   simply have an impossible request.  
     */
    if (!always_exp && frac_digits != -1 && dig_after_pt < frac_digits
        && dig_before_pt > 1)
    {
        /* switch to exponential format and start over */
        always_exp = TRUE;
        goto start_over;
    }

    /* display the digits before the decimal point */
    for (idx = 0 ; idx < dig_before_pt && idx < prec ; ++idx)
    {
        /* 
         *   if this isn't the first digit, and we're adding commas, and
         *   an even multiple of three more digits follow, insert a comma 
         */
        if (idx != 0 && commas && !always_exp
            && ((dig_before_pt - idx) % 3) == 0)
            *p++ = comma_char;
        
        /* get this digit */
        dig = get_dig(ext, idx);

        /* add it to the string */
        *p++ = (dig + '0');
    }

    /* if we ran out of precision, add zeroes */
    for ( ; idx < dig_before_pt ; ++idx)
        *p++ = '0';

    /* 
     *   Add the decimal point.  Show the decimal point unless
     *   always_show_pt is false, and either frac_digits is zero, or
     *   frac_digits is -1 and we have no fractional part. 
     */
    if (always_show_pt)
    {
        /* always showing the point */
        show_pt = TRUE;
    }
    else
    {
        if (frac_digits > 0)
        {
            /* we're showing fractional digits - always show a point */
            show_pt = TRUE;
        }
        else if (frac_digits == 0)
        {
            /* we're showing no fractional digits, so suppress the point */
            show_pt = FALSE;
        }
        else
        {
            /* 
             *   for now assume we'll show the point; we'll take it back
             *   out if we don't encounter anything but zeroes 
             */
            show_pt = TRUE;
        }
    }

    /* if we're showing the fractional part, show it */
    if (show_pt)
    {
        int frac_len;
        int frac_lim;
        char *last_non_zero;

        /* 
         *   remember the current position as the last trailing non-zero -
         *   if we don't find anything but zeroes and decide to remove the
         *   trailing zeroes, we'll also remove the decimal point by
         *   coming back here 
         */
        last_non_zero = p;
        
        /* add the point */
        *p++ = decpt_char;

        /* if we're always showing a decimal point, we can't remove it */
        if (always_show_pt)
            last_non_zero = p;

        /* if frac_digits is -1, there's no limit */
        if (frac_digits == -1)
            frac_lim = dig_after_pt;
        else
            frac_lim = frac_digits;

        /* 
         *   further limit the fractional digits according to the maximum
         *   digit allowance 
         */
        if (frac_lim > dig_after_pt)
            frac_lim = dig_after_pt;

        /* no fractional digits output yet */
        frac_len = 0;

        /* 
         *   if we haven't yet reached the first non-zero digit, display
         *   as many zeroes as necessary 
         */
        if (idx == 0 && exp < 0)
        {
            int cnt;

            /* 
             *   display leading zeroes based no the exponent: if exp is
             *   zero, we don't need any; if exp is -1, we need one; if
             *   exp is -2, we need two, and so on 
             */
            for (cnt = exp ; cnt != 0 && frac_len < frac_lim ;
                 ++cnt, ++frac_len)
            {
                /* add a zero */
                *p++ = '0';
            }
        }

        /* add the fractional digits */
        for ( ; idx < prec && frac_len < frac_lim ; ++idx, ++frac_len)
        {
            /* get this digit */
            dig = get_dig(ext, idx);

            /* add it */
            *p++ = (dig + '0');

            /* 
             *   if it's not a zero, note the location - if we decide to
             *   trim trailing zeroes, we'll want to keep at least this
             *   much, since this is a significant trailing digit 
             */
            if (dig != 0)
                last_non_zero = p;
        }

        /* 
         *   add the trailing zeroes if we ran out of precision before we
         *   filled the requested number of places 
         */
        if (frac_digits != -1)
        {
            /* fill out the remaining request length with zeroes */
            for ( ; frac_len < frac_lim ; ++frac_len)
                *p++ = '0';
        }
        else
        {
            char *p1;
            
            /* 
             *   if we're removing the decimal point, we're not showing a
             *   fractional part after all - so note 
             */
            if (last_non_zero < p && *last_non_zero == decpt_char)
                show_pt = FALSE;
            
            /* 
             *   We can use whatever length we like, so remove meaningless
             *   trailing zeroes.  Before we do this, though, make sure we
             *   aren't rounding up the last trailing zero - if the next
             *   digit is 5 or higher, we'll round the final zero to a 1.  
             */
            if (p > last_non_zero
                && idx < prec
                && get_dig(ext, idx) >= 5)
            {
                /* 
                 *   That last zero is significant after all, since we're
                 *   going to round it up to a 1 for display.  Don't actually
                 *   do the rounding now; simply note that the last zero is
                 *   significant so that we don't drop the digits leading up
                 *   to it. 
                 */
                last_non_zero = p;
            }

            /*   
             *   We've checked for rounding in the last digit, so we can now
             *   safely remove meaningless trailing zeroes.  If this leaves a
             *   completely empty buffer, not counting a sign and/or a
             *   decimal point, it means that we have a fractional number
             *   that we're showing without an exponent, and the number of
             *   digits we had for display was insufficient to reach the
             *   first non-zero fractional digit.  In this case, simply show
             *   '0' (or '0.', if a decimal point is required) as the result.
             *   To find out, scan for digits.  
             */
            p = last_non_zero;
            for (p1 = buf + VMB_LEN ; p1 < p && !is_digit(*p1) ; ++p1) ;

            /* if we didn't find any digits, add/insert a '0' */
            if (p1 == p)
            {
                /* 
                 *   if there's a decimal point, insert the '0' before it;
                 *   otherwise, just append the zero 
                 */
                if (p > buf + VMB_LEN && *(p-1) == decpt_char)
                {
                    *(p-1) = '0';
                    *p++ = decpt_char;
                }
                else
                    *p++ = '0';
            }
        }
    }

    /*
     *   Check for rounding.  If another digit remains, and that digit is
     *   greater than or equal to 5, round up.  
     */
    if (idx < prec && get_dig(ext, idx) >= 5)
    {
        char *rp;
        int need_carry;
        int found_pt;
        int dig_cnt;
        
        /* 
         *   go back through the number and add one to the last digit,
         *   carrying as needed 
         */
        for (dig_cnt = 0, found_pt = FALSE, need_carry = TRUE, rp = p - 1 ;
             rp >= buf + VMB_LEN ; rp = utf8_ptr::s_dec(rp))
        {
            /* if this is a digit, bump it up */
            if (is_digit(*rp))
            {
                /* count it */
                ++dig_cnt;
                
                /* if it's 9, we'll have to carry; otherwise it's easy */
                if (*rp == '9')
                {
                    /* set it to zero and keep going to do the carry */
                    *rp = '0';

                    /* 
                     *   if we haven't found the decimal point yet, and
                     *   we're not required to show a certain number of
                     *   fractional digits, we can simply remove this
                     *   trailing zero 
                     */
                    if (show_pt && !found_pt && frac_digits == -1)
                    {
                        /* remove the trailing zero */
                        p = utf8_ptr::s_dec(p);

                        /* remove it from the digit count */
                        --dig_cnt;
                    }
                }
                else
                {
                    /* bump it up one */
                    ++(*rp);

                    /* no more carrying is needed */
                    need_carry = FALSE;

                    /* we don't need to look any further */
                    break;
                }
            }
            else if (*rp == decpt_char)
            {
                /* note that we found the decimal point */
                found_pt = TRUE;
            }
        }

        /* 
         *   If we got to the start of the number and we still need a
         *   carry, we must have had all 9's.  In this case, we need to
         *   redo the entire number, since all of the layout (commas,
         *   leading spaces, etc) can change - it's way too much work to
         *   try to back-patch all of this stuff, so we'll just bump the
         *   number up and reformat it from scratch.  
         */
        if (need_carry)
        {
            int carry;

            /* 
             *   clear the digit that provoked the rounding - we don't
             *   want to round again on the next iteration 
             */
            set_dig(tmp_ext, idx, 0);
            
            /* round up the number starting at the previous digit */
            for (carry = TRUE ; idx != 0 ; )
            {
                /* move to the previous digit */
                --idx;

                /* if this digit is a 9, we'll need to carry */
                if (get_dig(tmp_ext, idx) == 9)
                {
                    /* adjust this digit and keep going */
                    set_dig(tmp_ext, idx, 0);
                }
                else
                {
                    /* bump this digit up one */
                    set_dig(tmp_ext, idx, get_dig(tmp_ext, idx) + 1);
                    
                    /* we're done */
                    carry = FALSE;
                    break;
                }
            }

            /* if we need to carry one more place, shift it */
            if (carry)
            {
                /* shift the number */
                shift_right(tmp_ext, 1);

                /* adjust the exponent accordingly */
                set_exp(tmp_ext, get_exp(tmp_ext) + 1);

                /* insert the leading 1 */
                set_dig(tmp_ext, 0, 1);
            }

            /* 
             *   if this pushes us over the maximum digit range, switch to
             *   scientific notation 
             */
            if (dig_cnt + 1 > max_digits)
                always_exp = TRUE;

            /* go back and start over */
            goto start_over;
        }
    }

    /* add the exponent */
    if (always_exp)
    {
        int disp_exp;
        
        /* add the 'E' */
        *p++ = (exp_caps ? 'E' : 'e');

        /* 
         *   calculate the display exponent - it's one less than the
         *   actual exponent, because we put the point after one digit 
         */
        disp_exp = exp - 1;

        /* 
         *   if we carried into the exponent due to rounding, increase the
         *   exponent by one 
         */
        if (exp_carry)
            ++disp_exp;

        /* add the sign */
        if (disp_exp < 0)
        {
            *p++ = '-';
            disp_exp = -disp_exp;
        }
        else if (always_sign_exp)
            *p++ = '+';

        /* 
         *   if the exponent is zero, just put zero (unless a more
         *   specific format was requested) 
         */
        if (disp_exp == 0 && exp_digits == -1)
        {
            /* add the zero exponent */
            *p++ = '0';
        }
        else
        {
            char buf[20];
            char *ep;
            int dig_cnt;

            /* build the exponent in reverse */
            for (dig_cnt = 0, ep = buf + sizeof(buf) ; disp_exp != 0 ;
                 disp_exp /= 10, ++dig_cnt)
            {
                /* move back one character */
                --ep;
                
                /* add one digit */
                *ep = (disp_exp % 10) + '0';
            }

            /* if necessary, add leading zeroes to the exponent */
            if (exp_digits != -1 && exp_digits > dig_cnt)
            {
                for ( ; dig_cnt < exp_digits ; ++dig_cnt)
                    *p++ = '0';
            }

            /* copy the exponent into the output */
            for ( ; ep < buf + sizeof(buf) ; ++ep)
                *p++ = *ep;
        }
    }

    /* set the string length */
    vmb_put_len(buf, p - (buf + VMB_LEN));

    /* if we allocated a temporary register, free it */
    if (tmp_ext != 0)
        release_temp_regs(vmg_ 1, tmp_hdl);

    /* return the string buffer */
    return buf;
}

/* ------------------------------------------------------------------------ */
/*
 *   Shift the value left (multiply by 10^shift)
 */
void CVmObjBigNum::shift_left(char *ext, unsigned int shift)
{
    size_t prec = get_prec(ext);
    size_t i;

    /* do nothing with a zero shift */
    if (shift == 0)
        return;

    /* if it's an even shift, it's especially easy */
    if ((shift & 1) == 0)
    {
        /* simply move the bytes left by the required amount */
        for (i = 0 ; i + shift/2 < (prec+1)/2 ; ++i)
            ext[VMBN_MANT + i] = ext[VMBN_MANT + i + shift/2];

        /* zero the remaining digits */
        for ( ; i < (prec+1)/2 ; ++i)
            ext[VMBN_MANT + i] = 0;

        /* 
         *   be sure to zero the last digit - if we have an odd precision,
         *   we will have copied the garbage digit from the final
         *   half-byte 
         */
        set_dig(ext, prec - shift, 0);
    }
    else
    {
        /* apply the shift to each digit */
        for (i = 0 ; i + shift < prec  ; ++i)
        {
            unsigned int dig;
            
            /* get this source digit */
            dig = get_dig(ext, i + shift);

            /* set this destination digit */
            set_dig(ext, i, dig);
        }

        /* zero the remaining digits */
        for ( ; i < prec ; ++i)
            set_dig(ext, i, 0);
    }
}

/*
 *   Shift the value right (divide by 10^shift)
 */
void CVmObjBigNum::shift_right(char *ext, unsigned int shift)
{
    size_t prec = get_prec(ext);
    size_t i;

    /* if it's an even shift, it's especially easy */
    if ((shift & 1) == 0)
    {
        /* simply move the bytes left by the required amount */
        for (i = (prec+1)/2 ; i > shift/2 ; --i)
            ext[VMBN_MANT + i-1] = ext[VMBN_MANT + i-1 - shift/2];

        /* zero the leading digits */
        for ( ; i > 0 ; --i)
            ext[VMBN_MANT + i-1] = 0;
    }
    else
    {
        /* apply the shift to each digit */
        for (i = prec ; i > shift  ; --i)
        {
            unsigned int dig;

            /* get this source digit */
            dig = get_dig(ext, i-1 - shift);

            /* set this destination digit */
            set_dig(ext, i-1, dig);
        }

        /* zero the remaining digits */
        for ( ; i >0 ; --i)
            set_dig(ext, i-1, 0);
    }
}

/*
 *   Increment a number 
 */
void CVmObjBigNum::increment_abs(char *ext)
{
    size_t idx;
    int exp = get_exp(ext);
    int carry;

    /* start at the one's place, if represented */
    idx = (exp <= 0 ? 0 : (size_t)exp);

    /* 
     *   if the units digit is to the right of the number (i.e., the
     *   number's scale is large), there's nothing to do 
     */
    if (idx > get_prec(ext))
        return;

    /* increment digits */
    for (carry = TRUE ; idx != 0 ; )
    {
        int dig;
        
        /* move to the next digit */
        --idx;

        /* get the digit value */
        dig = get_dig(ext, idx);

        /* increment it, checking for carry */
        if (dig == 9)
        {
            /* increment it to zero and keep going to carry */
            set_dig(ext, idx, 0);
        }
        else
        {
            /* increment this digit */
            set_dig(ext, idx, dig + 1);

            /* there's no carry out */
            carry = FALSE;

            /* done */
            break;
        }
    }

    /* if we carried past the end of the number, insert the leading 1 */
    if (carry)
    {
        /* 
         *   if we still haven't reached the units position, shift right
         *   until we do 
         */
        while (get_exp(ext) < 0)
        {
            /* shift it right and adjust the exponent */
            shift_right(ext, 1);
            set_exp(ext, get_exp(ext) + 1);
        }
        
        /* shift the number right and adjust the exponent */
        shift_right(ext, 1);
        set_exp(ext, get_exp(ext) + 1);

        /* insert the leading 1 */
        set_dig(ext, 0, 1);
    }

    /* we know the value is now non-zero */
    ext[VMBN_FLAGS] &= ~VMBN_F_ZERO;
}

/*
 *   Determine if the fractional part is zero 
 */
int CVmObjBigNum::is_frac_zero(const char *ext)
{
    size_t idx;
    int exp = get_exp(ext);
    size_t prec = get_prec(ext);

    /* start at the first fractional digit, if represented */
    idx = (exp <= 0 ? 0 : (size_t)exp);

    /* scan the digits for a non-zero digit */
    for ( ; idx < prec ; ++idx)
    {
        /* if this digit is non-zero, the fraction is non-zero */
        if (get_dig(ext, idx) != 0)
            return FALSE;
    }

    /* we didn't find any non-zero fractional digits */
    return TRUE;
}

/*
 *   Normalize a number - shift it so that the first digit is non-zero.
 *   If the number is zero, set the exponent to zero and clear the sign
 *   bit.  
 */
void CVmObjBigNum::normalize(char *ext)
{
    int idx;
    int prec = get_prec(ext);
    int all_zero;
    int nonzero_idx = 0;

    /* no work is necessary for anything but ordinary numbers */
    if (is_nan(ext))
        return;

    /* check for an all-zero mantissa */
    for (all_zero = TRUE, idx = 0 ; idx < prec ; ++idx)
    {
        /* check this digit */
        if (get_dig(ext, idx) != 0)
        {
            /* note the location of the first non-zero digit */
            nonzero_idx = idx;

            /* note that the number isn't all zeroes */
            all_zero = FALSE;

            /* no need to keep searching */
            break;
        }
    }

    /* if it's zero, set the canonical zero format */
    if (all_zero)
    {
        /* set the value to zero */
        set_zero(ext);
    }
    else
    {
        /* clear the zero flag */
        ext[VMBN_FLAGS] &= ~VMBN_F_ZERO;
        
        /* shift the mantissa left to make the first digit non-zero */
        if (nonzero_idx != 0)
            shift_left(ext, nonzero_idx);

        /* decrease the exponent to account for the mantissa shift */
        set_exp(ext, get_exp(ext) - nonzero_idx);
    }
}

/*
 *   Round the value up - increments the least significant digit
 */
void CVmObjBigNum::round_up_abs(char *ext)
{
    int idx;
    int carry;

    /* 
     *   Scan from least significant and apply the rounding.  Keep going
     *   until we reach the most significant digit.  
     */
    for (carry = TRUE, idx = get_prec(ext) ; idx != 0 ; )
    {
        int dig;
        
        /* move to the next position */
        --idx;

        /* get the digit at this position */
        dig = get_dig(ext, idx);

        /* check to see if we'll need to carry past this digit */
        if (dig == 9)
        {
            /* set it to zero and keep going to do the carry */
            set_dig(ext, idx, 0);
        }
        else
        {
            /* increment this digit */
            set_dig(ext, idx, dig + 1);

            /* no need to carry - note it and stop looping */
            carry = FALSE;
            break;
        }
    }

    /* 
     *   if we carried past the most significant digit, we must shift the
     *   value right, dropping the least significant digit, and insert a
     *   leading 1 
     */
    if (carry)
    {
        /* shift the mantissa */
        shift_right(ext, 1);

        /* compensate for the shift in the exponent */
        set_exp(ext, get_exp(ext) + 1);

        /* insert the leading 1 */
        set_dig(ext, 0, 1);
    }

    /* we know the value is non-zero now */
    ext[VMBN_FLAGS] &= ~VMBN_F_ZERO;
}

/*
 *   Copy a value, extending with zeroes if expanding, or truncating or
 *   rounding, as desired, if the precision changes 
 */
void CVmObjBigNum::copy_val(char *dst, const char *src, int round)
{
    size_t src_prec = get_prec(src);
    size_t dst_prec = get_prec(dst);
    
    /* check to see if we're growing or shrinking */
    if (dst_prec > src_prec)
    {
        /* 
         *   it's growing - copy the entire old mantissa, plus the flags,
         *   sign, and exponent 
         */
        memcpy(dst + VMBN_EXP, src + VMBN_EXP,
               (VMBN_MANT - VMBN_EXP) + (src_prec + 1)/2);
    
        /* clear the balance of the mantissa */
        memset(dst + VMBN_MANT + (src_prec + 1)/2,
               0, (dst_prec + 1)/2 - (src_prec + 1)/2);

        /* 
         *   clear the digit just after the last digit we copied - we
         *   might have missed this with the memset, since it only deals
         *   with even-numbered pairs of digits
         */
        set_dig(dst, src_prec, 0);
    }
    else
    {
        /* it's shrinking - truncate the mantissa to the new length */
        memcpy(dst + VMBN_EXP, src + VMBN_EXP,
               (VMBN_MANT - VMBN_EXP) + (dst_prec + 1)/2);

        /* check for rounding */
        if (round && dst_prec < src_prec && get_dig(src, dst_prec) >= 5)
        {
            /* round the value */
            round_up_abs(dst);
        }
    }
}

/*
 *   Multiply by an integer constant value 
 */
void CVmObjBigNum::mul_by_long(char *ext, unsigned long val)
{
    size_t idx;
    size_t prec = get_prec(ext);
    unsigned long carry = 0;
    int trail_dig = 0;
    
    /* 
     *   start at the least significant digit and work up through the
     *   digits 
     */
    for (idx = prec ; idx != 0 ; )
    {
        unsigned long prod;

        /* move to the next digit */
        --idx;
        
        /* 
         *   compute the product of this digit and the given value, and
         *   add in the carry from the last digit 
         */
        prod = (val * get_dig(ext, idx)) + carry;

        /* set this digit to the residue mod 10 */
        set_dig(ext, idx, prod % 10);

        /* carry the rest */
        carry = prod / 10;
    }

    /* if we have a carry left over, shift it in */
    while (carry != 0)
    {
        /* remember the digit we're dropping */
        trail_dig = get_dig(ext, prec - 1);

        /* shift the number and adjust the exponent */
        shift_right(ext, 1);
        set_exp(ext, get_exp(ext) + 1);

        /* shift in this digit of the carry */
        set_dig(ext, 0, carry % 10);

        /* take it out of the carry */
        carry /= 10;
    }

    /* round up if the dropped trailing digit is 5 or higher */
    if (trail_dig >= 5)
        round_up_abs(ext);

    /* normalize the result */
    normalize(ext);
}

/*
 *   Divide by an integer constant value 
 */
void CVmObjBigNum::div_by_long(char *ext, unsigned long val)
{
    size_t in_idx;
    size_t out_idx;
    int sig;
    size_t prec = get_prec(ext);
    unsigned long rem = 0;

    /*
     *   start at the most significant digit and work down 
     */
    for (rem = 0, sig = FALSE, in_idx = out_idx = 0 ;
         in_idx < prec || out_idx < prec ; ++in_idx)
    {
        long quo;
        
        /* 
         *   shift this digit into the remainder - if we're past the end
         *   of the number, shift in an implied trailing zero 
         */
        rem *= 10;
        rem += (in_idx < prec ? get_dig(ext, in_idx) : 0);

        /* calculate the quotient */
        quo = rem / val;

        /* if the quotient is non-zero, we've found a significant digit */
        if (quo != 0)
            sig = TRUE;

        /* 
         *   if we've found a significant digit, store it; otherwise, just
         *   reduce the exponent to account for an implied leading zero
         *   that we won't actually store 
         */
        if (sig)
        {
            /* store the digit */
            set_dig(ext, out_idx, (int)quo);

            /* move on to the next output digit */
            ++out_idx;
        }
        else
        {
            /* all leading zeroes so far - adjust the exponent */
            set_exp(ext, get_exp(ext) - 1);
        }

        /* calculate the remainder */
        rem %= val;

        /* 
         *   if we've reached the last input digit and the remainder is
         *   zero, we're done - fill out the rest of the number with
         *   trailing zeroes and stop looping
         */
        if (rem == 0 && in_idx >= prec)
        {
            /* check to see if we have any significant digits */
            if (sig)
            {
                /* fill out the rest of the number with zeroes */
                for ( ; out_idx < prec ; ++out_idx)
                    set_dig(ext, out_idx, 0);
            }
            else
            {
                /* no significant digits - the result is zero */
                set_zero(ext);
            }

            /* we have our result */
            break;
        }
    }
        
    /* 
     *   Round up if the next digit that we can't store is 5 or higher.
     *   The next digit can be calculated by shifting in the implied
     *   trailing zero (i.e., multiplying the remainder by 10 and adding
     *   zero) then dividing it by the divisor.
     */
    if ((rem * 10)/val >= 5)
        round_up_abs(ext);

    /* normalize the result */
    normalize(ext);
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
    int max_digits;
    uint flags = 0;
    int whole_places = -1;
    int frac_digits = -1;
    int exp_digits = -1;
    vm_val_t *lead_fill = 0;
    static CVmNativeCodeDesc desc(1, 5);
    
    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* get the maximum digit count */
    max_digits = CVmBif::pop_int_val(vmg0_);

    /* get the flags */
    if (orig_argc >= 2)
        flags = CVmBif::pop_int_val(vmg0_);

    /* get the whole places */
    if (orig_argc >= 3)
        whole_places = CVmBif::pop_int_val(vmg0_);

    /* get the fraction digits */
    if (orig_argc >= 4)
        frac_digits = CVmBif::pop_int_val(vmg0_);

    /* get the exponent digits */
    if (orig_argc >= 5)
        exp_digits = CVmBif::pop_int_val(vmg0_);

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
    pi = cache_pi(vmg_ prec);

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
    e = cache_e(vmg_ prec);

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
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
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
    size_t prec = get_prec(ext_);
    static CVmNativeCodeDesc desc(1);
    
    /* check arguments */
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
    size_t idx;
    int exp = get_exp(ext_);
    size_t prec = get_prec(ext_);

    /* check arguments and allocate the result value */
    if (setup_getp_0(vmg_ self, retval, argc, &new_ext))
        return TRUE;
    
    /* make a copy in the new object */
    memcpy(new_ext, ext_, calc_alloc(prec));

    /* clear out the first n digits, where n is the exponent */
    for (idx = 0 ; idx < prec && (int)idx < exp ; ++idx)
        set_dig(new_ext, idx, 0);

    /* normalize the result */
    normalize(new_ext);
    
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
    size_t idx;
    int exp = get_exp(ext_);
    size_t prec = get_prec(ext_);

    /* check arguments and allocate the result value */
    if (setup_getp_0(vmg_ self, retval, argc, &new_ext))
        return TRUE;

    /* make a copy in the new object */
    memcpy(new_ext, ext_, calc_alloc(prec));

    /* check what we have */
    if (exp <= 0)
    {
        /* it's an entirely fractional number - the result is zero */
        set_zero(new_ext);
    }
    else
    {
        /* clear digits after the decimal point */
        for (idx = (size_t)exp ; idx < prec ; ++idx)
            set_dig(new_ext, idx, 0);

        /* normalize the result */
        normalize(new_ext);
    }

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
        int need_to_round;
        size_t idx;
        
        /* 
         *   the digit after the last digit is part of the number - note
         *   it so we can tell if we need to round later
         */
        need_to_round = (get_dig(new_ext, post_idx) >= 5);
        
        /* set all of the digits to be elided to zero */
        for (idx = (size_t)post_idx ; idx < prec ; ++idx)
            set_dig(new_ext, idx, 0);

        /* if we need to round, do so now */
        if (need_to_round)
        {
            int carry;
            
            /* increment the last digit, and apply carry as far as needed */
            for (carry = TRUE, idx = (size_t)post_idx ; idx != 0 ; )
            {
                /* move to the next digit */
                --idx;

                /* check to see if we need to carry */
                if (get_dig(new_ext, idx) == 9)
                {
                    /* set it to zero, then keep going to carry */
                    set_dig(new_ext, idx, 0);
                }
                else
                {
                    /* increment the digit */
                    set_dig(new_ext, idx, get_dig(new_ext, idx) + 1);

                    /* no need to carry */
                    carry = FALSE;
                    break;
                }
            }

            /* if we needed to carry, insert a leading 1 */
            if (carry)
            {
                /* shift the number right one place */
                shift_right(new_ext, 1);

                /* adjust the exponent upwards */
                ++exp;
                set_exp(new_ext, exp);

                /* insert the leading 1 */
                set_dig(new_ext, 0, 1);
            }
        }

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
    char *new_ext;
    size_t prec = get_prec(ext_);
    static CVmNativeCodeDesc desc(0);

    /* check arguments */
    if (get_prop_check_argc(retval, argc, &desc))
        return TRUE;

    /* 
     *   If I'm not an ordinary number or an infinity, or I'm already
     *   non-negative, return myself unchanged.  Note that we change
     *   negative infinity to infinity, even though this might not make a
     *   great deal of sense mathematically.  
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
     *   value 
     */
    if (get_type(ext_) == VMBN_T_INF)
        prec = 1;

    /* create a new number with the same precision as the original */
    retval->set_obj(create(vmg_ FALSE, prec));
    new_ext = get_objid_ext(vmg_ retval->val.obj);

    /* make a copy in the new object */
    memcpy(new_ext, ext_, calc_alloc(prec));

    /* set the sign to positive */
    set_neg(new_ext, FALSE);

    /* remove my self-reference */
    G_stk->discard();

    /* handled */
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
    compute_quotient_into(vmg_ quo_ext, rem_ext, ext_, ext2);

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
    pi = cache_pi(vmg_ prec + 3);

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
    alloc_temp_regs(vmg_ prec + 3, 7,
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
            compute_quotient_into(vmg_ ext6, ext2, ext1, ext7);

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
        release_temp_regs(vmg_ 7, hdl1, hdl2, hdl3, hdl4, hdl5, hdl6, hdl7);
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
    pi = cache_pi(vmg_ prec + 3);

    /* allocate our temporary registers, as per sin() */
    alloc_temp_regs(vmg_ prec + 3, 7,
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
            compute_quotient_into(vmg_ ext6, ext2, ext1, ext7);

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
        release_temp_regs(vmg_ 7, hdl1, hdl2, hdl3, hdl4, hdl5, hdl6, hdl7);
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
    pi = cache_pi(vmg_ prec + 3);

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
    alloc_temp_regs(vmg_ prec + 3, 9,
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
            compute_quotient_into(vmg_ ext6, ext2, ext1, ext7);

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
            compute_quotient_into(vmg_ new_ext, 0, ext1, ext8);
        else
            compute_quotient_into(vmg_ new_ext, 0, ext8, ext1);

        /* negate the result if necessary */
        set_neg(new_ext, neg_result);

        /* normalize the result */
        normalize(new_ext);
    }
    err_finally
    {
        /* release our temporary registers */
        release_temp_regs(vmg_ 9, hdl1, hdl2, hdl3, hdl4,
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
    pi = cache_pi(vmg_ prec + 2);

    /* 
     *   allocate a temporary register for pi/180 - give it some extra
     *   precision 
     */
    alloc_temp_regs(vmg_ prec + 2, 1, &ext1, &hdl1);

    /* get pi to our precision */
    copy_val(ext1, pi, TRUE);

    /* divide pi by 180 */
    div_by_long(ext1, 180);

    /* go back to our working precision, rounding if necessary */
    set_prec(ext1, prec);
    if (get_dig(ext1, prec) >= 5)
        round_up_abs(ext1);

    /* multiply our value by pi/180 */
    compute_prod_into(new_ext, ext_, ext1);

    /* release our temporary registers */
    release_temp_regs(vmg_ 1, hdl1);

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
    pi = cache_pi(vmg_ prec + 2);

    /* allocate a temporary register for pi/180 */
    alloc_temp_regs(vmg_ prec + 2, 1, &ext1, &hdl1);

    /* catch errors so we can be sure to free registers */
    err_try
    {
        /* get pi to our precision */
        copy_val(ext1, pi, TRUE);

        /* divide pi by 180 */
        div_by_long(ext1, 180);

        /* go back to our working precision, rounding if necessary */
        set_prec(ext1, prec);
        if (get_dig(ext1, prec) >= 5)
            round_up_abs(ext1);

        /* divide by pi/180 */
        compute_quotient_into(vmg_ new_ext, 0, ext_, ext1);
    }
    err_finally
    {
        /* release our temporary registers */
        release_temp_regs(vmg_ 1, hdl1);
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
    calc_asincos_into(vmg_ new_ext, ext_, is_acos);

    /* remove my self-reference */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/*
 *   Calculate the arcsine or arccosine into the given result variable 
 */
void CVmObjBigNum::calc_asincos_into(VMG_ char *dst, const char *src,
                                     int is_acos)
{
    size_t prec = get_prec(dst);
    uint hdl1, hdl2, hdl3, hdl4, hdl5;
    char *ext1, *ext2, *ext3, *ext4, *ext5;
    const char *pi;

    /* cache pi to our working precision */
    pi = cache_pi(vmg_ prec + 3);

    /* 
     *   allocate our temporary registers - use some extra precision over
     *   what we need for the result, to reduce the effect of accumulated
     *   rounding error 
     */
    alloc_temp_regs(vmg_ prec + 3, 5,
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
            compute_sqrt_into(vmg_ ext1, ext4);
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
        release_temp_regs(vmg_ 5, hdl1, hdl2, hdl3, hdl4, hdl5);
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
        
        /* divide by even numbers up to and including n-1 */
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
    pi = cache_pi(vmg_ prec + 3);

    /* allocate some temporary registers */
    alloc_temp_regs(vmg_ prec + 3, 5,
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
                compute_quotient_into(vmg_ ext2, 0, get_one(), ext_);

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

                /* divide by the current term constant */
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
            compute_sqrt_into(vmg_ ext2, ext1);
            
            /* divide that into 1, and put the result back in r1 */
            compute_quotient_into(vmg_ ext1, 0, get_one(), ext2);
            
            /* 
             *   Compute the arccosine of this value - the result is the
             *   arctangent, so we can store it directly in the output
             *   register.  
             */
            calc_asincos_into(vmg_ new_ext, ext1, TRUE);
            
            /* if the input was negative, invert the sign of the result */
            if (get_neg(ext_))
                negate(new_ext);
        }
    }
    err_finally
    {
        /* release our temporary registers */
        release_temp_regs(vmg_ 5, hdl1, hdl2, hdl3, hdl4, hdl5);
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
    compute_sqrt_into(vmg_ new_ext, ext_);

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
    compute_ln_into(vmg_ new_ext, ext_);

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
    compute_exp_into(vmg_ new_ext, ext_);

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
    ln10 = cache_ln10(vmg_ prec + 3);

    /* allocate some temporary registers */
    alloc_temp_regs(vmg_ prec + 3, 3,
                    &ext1, &hdl1, &ext2, &hdl2, &ext3, &hdl3);

    /* catch errors so we can be sure to free registers */
    err_try
    {
        /* compute the natural logarithm of the number */
        compute_ln_into(vmg_ ext1, ext_);

        /* get ln(10), properly rounded */
        copy_val(ext2, ln10, TRUE);

        /* compute ln(x)/ln(10) to yield log10(x) */
        compute_quotient_into(vmg_ ext3, 0, ext1, ext2);

        /* store the result, rounding as needed */
        copy_val(new_ext, ext3, TRUE);
    }
    err_finally
    {
        /* release our temporary registers */
        release_temp_regs(vmg_ 3, hdl1, hdl2, hdl3);
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
    alloc_temp_regs(vmg_ prec + 3, 2,
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
            compute_ln_into(vmg_ ext1, ext2);
        }
        else
        {
            /* calculate ln(x) into r1 */
            compute_ln_into(vmg_ ext1, ext_);

            /* the result will be positive */
            result_neg = FALSE;
        }

        /* calculate y * ln(x) into r2 */
        compute_prod_into(ext2, val2_ext, ext1);

        /* calculate exp(r2) = exp(y*ln(x)) = x^y into r1 */
        compute_exp_into(vmg_ ext1, ext2);

        /* negate the result if we had a negative x and an odd power */
        if (result_neg)
            negate(ext1);

        /* save the result, rounding as needed */
        copy_val(new_ext, ext1, TRUE);
    }
    err_finally
    {
        /* release our temporary registers */
        release_temp_regs(vmg_ 2, hdl1, hdl2);
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
    compute_sinhcosh_into(vmg_ new_ext, ext_, is_cosh, is_tanh);

    /* discard the GC protection */
    G_stk->discard();

    /* handled */
    return TRUE;
}

/* ------------------------------------------------------------------------ */
/*
 *   Compute a natural logarithm 
 */
void CVmObjBigNum::compute_ln_into(VMG_ char *dst, const char *src)
{
    uint hdl1, hdl2, hdl3, hdl4, hdl5;
    char *ext1, *ext2, *ext3, *ext4, *ext5;
    size_t prec = get_prec(dst);
    const char *ln10;

    /* cache the value of ln(10) */
    ln10 = cache_ln10(vmg_ prec + 3);

    /* if the source value is zero, it's an error */
    if (is_zero(src))
        err_throw(VMERR_OUT_OF_RANGE);

    /* allocate some temporary registers */
    alloc_temp_regs(vmg_ prec + 3, 5,
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
         *   if the lead digit of the mantissa is 1, multiply the number
         *   by ten and adjust the exponent accordingly - the series is
         *   especially good at values near 1 
         */
        if (get_dig(ext1, 0) == 1)
        {
            set_exp(ext1, 1);
            --src_exp;
        }

        /* compute the series expansion */
        ext1 = compute_ln_series_into(vmg_ ext1, ext2, ext3, ext4, ext5);

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
        release_temp_regs(vmg_ 5, hdl1, hdl2, hdl3, hdl4, hdl5);
    }
    err_end;
}

/*
 *   Compute the natural log series.  The argument value, initially in
 *   ext1, should be adjusted to a small value before this is called to
 *   ensure quick series convergence.  
 */
char *CVmObjBigNum::compute_ln_series_into(VMG_ char *ext1, char *ext2,
                                           char *ext3, char *ext4,
                                           char *ext5)
{
    ulong n;
    char *tmp;

    /* start at the first term of the series */
    n = 1;
    
    /* subtract one from r1 to yield (x-1) in r2 */
    compute_abs_diff_into(ext2, ext1, get_one());
    
    /* add one to r1 to yield (x+1) */
    increment_abs(ext1);
    
    /* compute (x-1)/(x+1) into r3 - this will be our current power */
    compute_quotient_into(vmg_ ext3, 0, ext2, ext1);
    
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
        
        /* divide the power by n */
        div_by_long(ext2, n);
        
        /* if it's too small to notice, we're done */
        if (is_zero(ext2)
            || get_exp(ext1) - get_exp(ext2) > (int)get_prec(ext1))
            break;
        
        /* compute the sum with our accumulator into r5 */
        compute_sum_into(ext5, ext1, ext2);
        
        /* swap r5 and r1 - the new sum is our new accumulator */
        tmp = ext5;
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
void CVmObjBigNum::compute_exp_into(VMG_ char *dst, const char *src)
{
    uint hdl1, hdl2, hdl3, hdl4, hdl5, hdl6;
    char *ext1, *ext2, *ext3, *ext4, *ext5, *ext6;
    size_t prec = get_prec(dst);
    const char *ln10;

    /* get the constant value of ln10 to the required precision */
    ln10 = cache_ln10(vmg_ prec + 3);

    /* allocate some temporary registers */
    alloc_temp_regs(vmg_ prec + 3, 6,
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
        compute_quotient_into(vmg_ ext1, 0, src, ln10);

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
         *   value we will use with the Taylor series. 
         */
        compute_prod_into(ext3, ext1, ln10);

        /* 
         *   While our input value is greater than 0.5, divide it by two
         *   to make it smaller than 0.5.  This will speed up the series
         *   convergence.  When we're done, we'll correct for the
         *   divisions my squaring the result: e^2x = (e^x)^2 
         */
        copy_val(ext1, get_one(), FALSE);
        div_by_long(ext1, 2);
        for (twos = 0 ; compare_abs(ext3, ext1) > 0 ; ++twos)
            div_by_long(ext3, 2);

        /* start with 1 in our accumulator (r1) */
        copy_val(ext1, get_one(), FALSE);

        /* copy our series argument into the current-power register (r2) */
        copy_val(ext2, ext3, FALSE);

        /* start with 1 in our factorial register (r4) */
        copy_val(ext4, get_one(), FALSE);

        /* start at the first term */
        n = 1;
        n_fact = 1;
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
                if (n_fact > LONG_MAX/(n+1))
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
                compute_quotient_into(vmg_ ext5, 0, ext2, ext4);
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
        release_temp_regs(vmg_ 6, hdl1, hdl2, hdl3, hdl4, hdl5, hdl6);
    }
    err_end;
}

/* ------------------------------------------------------------------------ */
/*
 *   Compute a hyperbolic sine or cosine 
 */
void CVmObjBigNum::compute_sinhcosh_into(VMG_ char *dst, const char *src,
                                         int is_cosh, int is_tanh)
{
    size_t prec = get_prec(dst);
    uint hdl1, hdl2, hdl3, hdl4;
    char *ext1, *ext2, *ext3, *ext4;
    
    /* allocate some temporary registers */
    alloc_temp_regs(vmg_ prec + 3, 4,
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
        compute_exp_into(vmg_ ext1, src);

        /* 
         *   rather than calculating e^-x separately, simply invert our
         *   e^x value to yield e^-x (a simple division is much quicker
         *   than calculating another exponent, which involves an entire
         *   taylor series expansion) 
         */
        copy_val(ext3, get_one(), FALSE);
        compute_quotient_into(vmg_ ext2, 0, ext3, ext1);

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
            compute_quotient_into(vmg_ ext1, 0, ext3, ext4);

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
        release_temp_regs(vmg_ 4, hdl1, hdl2, hdl3, hdl4);
    }
    err_end;
}

/* ------------------------------------------------------------------------ */
/*
 *   Cache the natural logarithm of 10 to the given precision and return
 *   the value 
 */
const char *CVmObjBigNum::cache_ln10(VMG_ size_t prec)
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
    ext = G_bignum_cache->get_ln10_reg(calc_alloc(prec), &expanded);

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
    alloc_temp_regs(vmg_ prec + 3, 5,
                    &ext1, &hdl1, &ext2, &hdl2, &ext3, &hdl3,
                    &ext4, &hdl4, &ext5, &hdl5);

    /* catch errors so we can be sure to free registers */
    err_try
    {
        /* 
         *   Compute sqrt(10) - 10 is too large for the series to
         *   converge, but sqrt(10) is good.  We'll correct for this later
         *   by doubling the result of the series expansion, which gives
         *   us the correct result: ln(a^b) = b*ln(a), and sqrt(x) =
         *   x^(1/2), hence ln(sqrt(x)) = ln(x)/2, which means that ln(x)
         *   = 2*ln(sqrt(x)).  
         */

        /* compute sqrt(10), for quick series convergence */
        copy_val(ext2, (const char *)ten, FALSE);
        compute_sqrt_into(vmg_ ext1, ext2);

        /* compute the series expansion */
        ext1 = compute_ln_series_into(vmg_ ext1, ext2, ext3, ext4, ext5);

        /* double the result (to adjust for the sqrt) */
        mul_by_long(ext1, 2);

        /* store the result in the cache */
        copy_val(ext, ext1, TRUE);
    }
    err_finally
    {
        /* release our temporary registers */
        release_temp_regs(vmg_ 5, hdl1, hdl2, hdl3, hdl4, hdl5);
    }
    err_end;

    /* return the register pointer */
    return ext;
}

/* ------------------------------------------------------------------------ */
/*
 *   Cache pi to the required precision
 */
const char *CVmObjBigNum::cache_pi(VMG_ size_t prec)
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
    ext = G_bignum_cache->get_pi_reg(calc_alloc(prec), &expanded);

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
    alloc_temp_regs(vmg_ prec + 3, 5,
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
        compute_sqrt_into(vmg_ ext1, ext2);

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
        release_temp_regs(vmg_ 5, hdl1, hdl2, hdl3, hdl4, hdl5);
    }
    err_end;

    /* return the register pointer */
    return ext;
}

/* ------------------------------------------------------------------------ */
/*
 *   Cache e to the required precision 
 */
const char *CVmObjBigNum::cache_e(VMG_ size_t prec)
{
    char *ext;
    int expanded;

    /* 
     *   round up the precision a bit to ensure that we don't have to
     *   repeatedly recalculate this value if we're asked for a cluster of
     *   similar precisions 
     */
    prec = (prec + 7) & ~7;

    /* get the e cache register */
    ext = G_bignum_cache->get_e_reg(calc_alloc(prec), &expanded);

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
    compute_exp_into(vmg_ ext, get_one());

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
void CVmObjBigNum::compute_sqrt_into(VMG_ char *dst, const char *src)
{
    uint hdl1, hdl2, hdl3, hdl4;
    char *ext1, *ext2, *ext3, *ext4;
    size_t dst_prec = get_prec(dst);

    /* if the value is negative, it's an error */
    if (get_neg(src))
        err_throw(VMERR_OUT_OF_RANGE);
    
    /* allocate our scratchpad registers */
    alloc_temp_regs(vmg_ get_prec(dst) + 3, 4,
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
         *   divide we will have to do on each iteration.  
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
            compute_quotient_into(vmg_ ext3, 0, src, ext1);

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
        release_temp_regs(vmg_ 4, hdl1, hdl2, hdl3, hdl4);
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
        compute_quotient_into(vmg_ ext4, 0, ext1, ext3);
        
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
        compute_quotient_into(vmg_ ext4, 0, ext1, ext3);

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
void CVmObjBigNum::add_val(VMG_ vm_val_t *result,
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
}

/* 
 *   subtract a value 
 */
void CVmObjBigNum::sub_val(VMG_ vm_val_t *result,
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
}

/* 
 *   multiply a value 
 */
void CVmObjBigNum::mul_val(VMG_ vm_val_t *result,
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
}

/* 
 *   divide a value 
 */
void CVmObjBigNum::div_val(VMG_ vm_val_t *result,
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

    /* compute the quotient */
    compute_quotient(vmg_ result, ext_, get_objid_ext(vmg_ val2.val.obj));

    /* discard the GC protection items */
    G_stk->discard(2);
}

/* 
 *   negate the number
 */
void CVmObjBigNum::neg_val(VMG_ vm_val_t *result, vm_obj_id_t self)
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
        return;
    }

    /* push a self-reference while we're working */
    G_stk->push()->set_obj(self);

    /* if I'm an infinity, we don't need any precision in the result */
    if (get_type(ext_) == VMBN_T_INF)
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
    const char *ext2;
    int ret;

    /* convert it */
    val2 = *val;
    if (!cvt_to_bignum(vmg_ self, &val2))
    {
        /* this type is not convertible to BigNumber - it's not comparable */
        err_throw(VMERR_INVALID_COMPARISON);
    }

    /* get the other object's extension */
    ext2 = get_objid_ext(vmg_ val2.val.obj);

    /* if either one is not a number, they can't be compared */
    if (is_nan(ext_) || is_nan(ext2))
        err_throw(VMERR_INVALID_COMPARISON);

    /* if the signs differ, the positive one is greater */
    if (get_neg(ext_) != get_neg(ext2))
    {
        /* 
         *   if I'm negative, I'm the lesser number; otherwise I'm the
         *   greater number 
         */
        return (get_neg(ext_) ? -1 : 1);
    }

    /* the signs are the same - compare the absolute values */
    ret = compare_abs(ext_, ext2);

    /* 
     *   If the numbers are negative, and my absolute value is greater,
     *   I'm actually the lesser number; if they're negative and my
     *   absolute value is lesser, I'm the greater number.  So, if I'm
     *   negative, invert the sense of the result.  Otherwise, both
     *   numbers are positive, so the sense of the absolute value
     *   comparison is the same as the sense of the actual comparison, so
     *   just return the result directly.
     */
    return (get_neg(ext_) ? -ret : ret);
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
 *   Compute a sum into the given buffer 
 */
void CVmObjBigNum::compute_sum_into(char *new_ext,
                                    const char *ext1, const char *ext2)
{
    /* check to see if the numbers have the same sign */
    if (get_neg(ext1) == get_neg(ext2))
    {
        /*
         *   the two numbers have the same sign, so the sum has the same
         *   sign as the two values, and the magnitude is the sum of the
         *   absolute values of the operands
         */

        /* compute the sum of the absolute values */
        compute_abs_sum_into(new_ext, ext1, ext2);

        /* set the sign to match that of the operand */
        set_neg(new_ext, get_neg(ext1));
    }
    else
    {
        /* 
         *   one is positive and the other is negative - subtract the
         *   absolute value of the negative one from the absolute value of
         *   the positive one; the sign will be set correctly by the
         *   differencing 
         */
        if (get_neg(ext1))
            compute_abs_diff_into(new_ext, ext2, ext1);
        else
            compute_abs_diff_into(new_ext, ext1, ext2);
    }
}

/*
 *   Compute a difference 
 */
void CVmObjBigNum::compute_diff(VMG_ vm_val_t *result,
                                const char *ext1, const char *ext2)
{
    char *new_ext;

    /* allocate our result value */
    new_ext = compute_init_2op(vmg_ result, ext1, ext2);

    /* we're done if we had a non-number operand */
    if (new_ext == 0)
        return;

    /* check to see if the numbers have the same sign */
    if (get_neg(ext1) == get_neg(ext2))
    {
        /* 
         *   The two numbers have the same sign, so the difference is the
         *   difference in the magnitudes, and has a sign depending on the
         *   order of the difference and the signs of the original values.
         *   If both values are positive, the difference is negative if
         *   the second value is larger than the first.  If both values
         *   are negative, the difference is negative if the second value
         *   has larger absolute value than the first.  
         */

        /* compute the difference in magnitudes */
        compute_abs_diff_into(new_ext, ext1, ext2);

        /* 
         *   if the original values were negative, then the sign of the
         *   result is the opposite of the sign of the difference of the
         *   absolute values; otherwise, it's the same as the sign of the
         *   difference of the absolute values 
         */
        if (get_neg(ext1))
            negate(new_ext);
    }
    else
    {
        /* 
         *   one is positive and the other is negative - the result has
         *   the sign of the first operand, and has magnitude equal to the
         *   sum of the absolute values 
         */
        
        /* compute the sum of the absolute values */
        compute_abs_sum_into(new_ext, ext1, ext2);

        /* set the sign of the result to that of the first operand */
        if (!is_zero(new_ext))
            set_neg(new_ext, get_neg(ext1));
    }
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
    compute_quotient_into(vmg_ new_ext, 0, ext1, ext2);
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
    alloc_temp_regs(vmg_ get_prec(shorter), 1, &tmp_ext, &tmp_hdl);

    /* make a rounded copy */
    copy_val(tmp_ext, longer, TRUE);

    /* compare the rounded copy of the longer value to the shorter value */
    ret = compute_eq_exact(shorter, tmp_ext);

    /* release the temporary register */
    release_temp_regs(vmg_ 1, tmp_hdl);

    /* return the result */
    return ret;
}

/*
 *   Make an exact comparison for equality.  If one value is more precise
 *   than the other, we'll implicitly extend the shorter value to the
 *   right with trailing zeroes.  
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
     *   the larger-precision value's digits are all zeroes from there out 
     */
    for (idx = 0 ; idx < min_prec ; ++idx)
    {
        /* if they don't match, return not-equal */
        if (get_dig(ext1, idx) != get_dig(ext2, idx))
            return FALSE;
    }

    /* check the longer one to make sure it's all zeroes */
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
 *   Compute the sum of two absolute values into the given buffer
 */
void CVmObjBigNum::compute_abs_sum_into(char *new_ext,
                                        const char *ext1, const char *ext2)
{
    int max_exp;
    int lo1, hi1;
    int lo2, hi2;
    int lo3, hi3;
    int pos;
    int carry;
    int trail_dig;

    /* if one or the other is identically zero, return the other value */
    if (is_zero(ext1))
    {
        /* ext1 is zero - return ext2 */
        copy_val(new_ext, ext2, TRUE);
        return;
    }
    else if (is_zero(ext2))
    {
        /* ext2 is zero - return ext1 */
        copy_val(new_ext, ext1, TRUE);
        return;
    }

    /* 
     *   start the new value with the larger of the two exponents - this
     *   will have the desired effect of dropping the least significant
     *   digits if any digits must be dropped 
     */
    max_exp = get_exp(ext1);
    if (get_exp(ext2) > max_exp)
        max_exp = get_exp(ext2);
    set_exp(new_ext, max_exp);

    /* compute the digit positions at the bounds of each of our values */
    hi1 = get_exp(ext1) - 1;
    lo1 = get_exp(ext1) - get_prec(ext1);

    hi2 = get_exp(ext2) - 1;
    lo2 = get_exp(ext2) - get_prec(ext2);

    hi3 = get_exp(new_ext) - 1;
    lo3 = get_exp(new_ext) - get_prec(new_ext);

    /*
     *   If one of the values provides a digit one past the end of our
     *   result, remember that as the trailing digit that we're going to
     *   drop.  We'll check this when we're done to see if we need to
     *   round the number.  Since the result has precision at least as
     *   large as the larger of the two inputs, we can only be dropping
     *   significant digits from one of the two inputs - we can't be
     *   cutting off both inputs.  
     */
    trail_dig = 0;
    if (lo3-1 >= lo1 && lo3-1 <= hi1)
    {
        /* remember the digit */
        trail_dig = get_dig(ext1, get_exp(ext1) - (lo3-1) - 1);
    }
    else if (lo3-1 >= lo2 && lo3-1 <= hi2)
    {
        /* remember the digit */
        trail_dig = get_dig(ext2, get_exp(ext2) - (lo3-1) - 1);
    }

    /* no carry yet */
    carry = 0;

    /* add the digits */
    for (pos = lo3 ; pos <= hi3 ; ++pos)
    {
        int acc;

        /* start with the carry */
        acc = carry;

        /* add the first value digit if it's in range */
        if (pos >= lo1 && pos <= hi1)
            acc += get_dig(ext1, get_exp(ext1) - pos - 1);

        /* add the second value digit if it's in range */
        if (pos >= lo2 && pos <= hi2)
            acc += get_dig(ext2, get_exp(ext2) - pos - 1);

        /* check for carry */
        if (acc > 9)
        {
            /* reduce the accumulator and set the carry */
            acc -= 10;
            carry = 1;
        }
        else
        {
            /* no carry */
            carry = 0;
        }

        /* set the digit in the result */
        set_dig(new_ext, get_exp(new_ext) - pos - 1, acc);
    }

    /* 
     *   If we have a carry at the end, we must carry it out to a new
     *   digit.  In order to do this, we must shift the whole number right
     *   one place, then insert the one. 
     */
    if (carry)
    {
        /* 
         *   remember the last digit of the result, which we won't have
         *   space to store after the shift 
         */
        trail_dig = get_dig(new_ext, get_prec(new_ext) - 1);
        
        /* shift the result right */
        shift_right(new_ext, 1);

        /* increase the exponent to compensate for the shift */
        set_exp(new_ext, get_exp(new_ext) + 1);

        /* set the leading 1 */
        set_dig(new_ext, 0, 1);
    }

    /* the sum of two absolute values is always positive */
    set_neg(new_ext, FALSE);

    /* round up the value if the trailing digit is 5 or higher */
    if (trail_dig >= 5)
        round_up_abs(new_ext);

    /* normalize the number */
    normalize(new_ext);
}

/*
 *   Compute the difference of two absolute values into the given buffer 
 */
void CVmObjBigNum::compute_abs_diff_into(char *new_ext,
                                         const char *ext1, const char *ext2)
{
    int max_exp;
    int lo1, hi1;
    int lo2, hi2;
    int lo3, hi3;
    int pos;
    int result_neg = FALSE;
    int borrow;

    /* if we're subtracting zero or from zero, it's easy */
    if (is_zero(ext2))
    {
        /* 
         *   we're subtracting zero from another value - the result is
         *   simply the first value 
         */
        copy_val(new_ext, ext1, TRUE);
        return;
    }
    else if (is_zero(ext1))
    {
        /* 
         *   We're subtracting a value from zero - we know the value we're
         *   subtracting is non-zero (we already checked for that case
         *   above), and we're only considering the absolute values, so
         *   simply return the negative of the absolute value of the
         *   second operand.  
         */
        copy_val(new_ext, ext2, TRUE);
        set_neg(new_ext, TRUE);
        return;
    }

    /*
     *   Compare the absolute values of the two operands.  If the first
     *   value is larger than the second, subtract them in the given
     *   order.  Otherwise, reverse the order and note that the result is
     *   negative. 
     */
    if (compare_abs(ext1, ext2) < 0)
    {
        const char *tmp;
        
        /* the result will be negative */
        result_neg = TRUE;

        /* swap the order of the subtraction */
        tmp = ext1;
        ext1 = ext2;
        ext2 = tmp;
    }

    /* 
     *   start the new value with the larger of the two exponents - this
     *   will have the desired effect of dropping the least significant
     *   digits if any digits must be dropped 
     */
    max_exp = get_exp(ext1);
    if (get_exp(ext2) > max_exp)
        max_exp = get_exp(ext2);
    set_exp(new_ext, max_exp);

    /* compute the digit positions at the bounds of each of our values */
    hi1 = get_exp(ext1) - 1;
    lo1 = get_exp(ext1) - get_prec(ext1);

    hi2 = get_exp(ext2) - 1;
    lo2 = get_exp(ext2) - get_prec(ext2);

    hi3 = get_exp(new_ext) - 1;
    lo3 = get_exp(new_ext) - get_prec(new_ext);

    /* start off with no borrow */
    borrow = 0;

    /*
     *   Check for borrowing from before the least significant digit
     *   position in common to both numbers 
     */
    if (lo3-1 >= lo1 && lo3-1 <= hi1)
    {
        /* 
         *   In this case, we would be dropping precision from the end of
         *   the top number.  This case should never happen - the only way
         *   it could happen is for the bottom number to extend to the
         *   left of the top number at the most significant end.  This
         *   cannot happen because the bottom number always has small
         *   magnitude by this point (we guarantee this above).  So, we
         *   should never get here.
         */
        assert(FALSE);
    }
    else if (lo3-1 >= lo2 && lo3-1 <= hi2)
    {
        size_t idx;
        
        /*
         *   We're dropping precision from the bottom number, so we want
         *   to borrow into the subtraction if the rest of the number is
         *   greater than 5xxxx.  If it's exactly 5000..., do not borrow,
         *   since the result would end in 5 and we'd round up.
         *   
         *   If the next digit is 6 or greater, we know for a fact we'll
         *   have to borrow.  If the next digit is 4 or less, we know for
         *   a fact we won't have to borrow.  If the next digit is 5,
         *   though, we must look at the rest of the number to see if
         *   there's anything but trailing zeroes.  
         */
        idx = (size_t)(get_exp(ext2) - (lo3-1) - 1);
        if (get_dig(ext2, idx) >= 6)
        {
            /* definitely borrow */
            borrow = 1;
        }
        else if (get_dig(ext2, idx) == 5)
        {
            /* borrow only if we have something non-zero following */
            for (++idx ; idx < get_prec(ext2) ; ++idx)
            {
                /* if it's non-zero, we must borrow */
                if (get_dig(ext2, idx) != 0)
                {
                    /* note the borrow */
                    borrow = 1;

                    /* no need to keep scanning */
                    break;
                }
            }
        }
    }

    /* subtract the digits from least to most significant */
    for (pos = lo3 ; pos <= hi3 ; ++pos)
    {
        int acc;

        /* start with zero in the accumulator */
        acc = 0;

        /* start with the top-line value if it's represented here */
        if (pos >= lo1 && pos <= hi1)
            acc = get_dig(ext1, get_exp(ext1) - pos - 1);

        /* subtract the second value digit if it's represented here */
        if (pos >= lo2 && pos <= hi2)
            acc -= get_dig(ext2, get_exp(ext2) - pos - 1);

        /* subtract the borrow */
        acc -= borrow;

        /* check for borrow */
        if (acc < 0)
        {
            /* increase the accumulator */
            acc += 10;

            /* we must borrow from the next digit up */
            borrow = 1;
        }
        else
        {
            /* we're in range - no need to borrow */
            borrow = 0;
        }

        /* set the digit in the result */
        set_dig(new_ext, get_exp(new_ext) - pos - 1, acc);
    }

    /* set the sign of the result as calculated earlier */
    set_neg(new_ext, result_neg);

    /* normalize the number */
    normalize(new_ext);
}

/*
 *   Compute the product of the values into the given buffer 
 */
void CVmObjBigNum::compute_prod_into(char *new_ext,
                                     const char *ext1, const char *ext2)
{
    size_t prec1 = get_prec(ext1);
    size_t prec2 = get_prec(ext2);
    size_t new_prec = get_prec(new_ext);
    size_t idx1;
    size_t idx2;
    size_t out_idx;
    size_t start_idx;
    int out_exp;
    int trail_dig;
    
    /* start out with zero in the accumulator */
    memset(new_ext + VMBN_MANT, 0, (new_prec + 1)/2);

    /* 
     *   Initially write output in the same 'column' where the top number
     *   ends, so we start out with the same scale as the top number.  
     */
    start_idx = get_prec(ext1);

    /* 
     *   initial result exponent is the sum of the exponents, minus the
     *   number of digits in the bottom number (effectively, this lets us
     *   treat the bottom number as a whole number by scaling it enough to
     *   make it whole, soaking up the factors of ten into decimal point
     *   relocation) 
     */
    out_exp = get_exp(ext1) + get_exp(ext2) - prec2;

    /* there's no trailing accumulator digit yet */
    trail_dig = 0;

    /* 
     *   Loop over digits in the bottom number, from least significant to
     *   most significant - we'll multiply each digit of the bottom number
     *   by the top number and add the result into the accumulator.  
     */
    for (idx2 = prec2 ; idx2 != 0 ; )
    {
        int carry;
        int dig;
        int ext2_dig;

        /* no carry yet on this round */
        carry = 0;
        
        /* start writing this round at the output start index */
        out_idx = start_idx;

        /* move to the next digit */
        --idx2;

        /* get this digit of ext2 */
        ext2_dig = get_dig(ext2, idx2);

        /* 
         *   if this digit of ext2 is non-zero, multiply the digits of
         *   ext1 by the digit (obviously if the digit is zero, there's no
         *   need to iterate over the digits of ext1) 
         */
        if (ext2_dig != 0)
        {
            /* 
             *   loop over digits in the top number, from least to most
             *   significant 
             */
            for (idx1 = prec1 ; idx1 != 0 ; )
            {
                /* move to the next digit of the top number */
                --idx1;
                
                /* move to the next digit of the accumulator */
                --out_idx;
                
                /* 
                 *   compute the product of the current digits, and add in
                 *   the carry from the last digit, then add in the
                 *   current accumulator digit in this position 
                 */
                dig = (get_dig(ext1, idx1) * ext2_dig)
                      + carry
                      + get_dig(new_ext, out_idx);

                /* figure the carry to the next digit */
                carry = (dig / 10);
                dig = dig % 10;

                /* store the new digit */
                set_dig(new_ext, out_idx, dig);
            }
        }

        /* 
         *   Shift the result accumulator right in preparation for the
         *   next round.  One exception: if this is the last (most
         *   significant) digit of the bottom number, and there's no
         *   carry, there's no need to shift the number, since we'd just
         *   normalize away the leading zero anyway 
         */
        if (idx2 != 0 || carry != 0)
        {
            /* remember the trailing digit that we're going to drop */
            trail_dig = get_dig(new_ext, new_prec - 1);

            /* shift the accumulator */
            shift_right(new_ext, 1);

            /* increase the output exponent */
            ++out_exp;

            /* insert the carry as the lead digit */
            set_dig(new_ext, 0, carry);
        }
    }

    /* set the result exponent */
    set_exp(new_ext, out_exp);

    /* 
     *   set the sign - if both inputs have the same sign, the output is
     *   positive, otherwise it's negative 
     */
    set_neg(new_ext, get_neg(ext1) != get_neg(ext2));

    /* if the trailing digit is 5 or greater, round up */
    if (trail_dig >= 5)
        round_up_abs(new_ext);

    /* normalize the number */
    normalize(new_ext);
}

/*
 *   Compute a quotient into the given buffer.  If new_rem_ext is
 *   non-null, we'll save the remainder into this buffer.  We calculate
 *   the remainder only to the precision of the quotient.  
 */
void CVmObjBigNum::compute_quotient_into(VMG_ char *new_ext,
                                         char *new_rem_ext,
                                         const char *ext1, const char *ext2)
{
    char *rem_ext;
    uint rem_hdl;
    char *rem_ext2;
    uint rem_hdl2;
    int quo_exp;
    size_t quo_idx;
    size_t quo_prec = get_prec(new_ext);
    size_t dvd_prec = get_prec(ext1);
    size_t dvs_prec = get_prec(ext2);
    char *dvs_ext;
    uint dvs_hdl;
    char *dvs_ext2;
    uint dvs_hdl2;
    int lead_dig_set;
    int zero_remainder;
    int acc;
    size_t rem_prec;

    /* if the divisor is zero, throw an error */
    if (is_zero(ext2))
        err_throw(VMERR_DIVIDE_BY_ZERO);

    /* if the dividend is zero, the result is zero */
    if (is_zero(ext1))
    {
        /* set the result to zero */
        set_zero(new_ext);

        /* if they want the remainder, it's zero also */
        if (new_rem_ext != 0)
            set_zero(new_rem_ext);

        /* we're done */
        return;
    }

    /* 
     *   Calculate the precision we need for the running remainder.  We
     *   must retain in the remainder enough precision to calculate exact
     *   differences, so we need the greater of the precisions of the
     *   dividend and the divisor, plus enough extra digits for the
     *   maximum relative shifting.  We will have to shift at most one
     *   extra digit, but use two to be extra safe.  
     */
    rem_prec = dvd_prec;
    if (rem_prec < dvs_prec)
        rem_prec = dvs_prec;
    rem_prec += 2;

    /*   
     *   Make sure the precision is at least three, since it simplifies
     *   our digit approximation calculation.  
     */
    if (rem_prec < 3)
        rem_prec = 3;

    /* 
     *   Allocate two temporary registers for the running remainder, and
     *   one more for the multiplied divisor.  Note that we allocate the
     *   multiplied divisor at our higher precision so that we don't lose
     *   digits in our multiplier calculations.  
     */
    alloc_temp_regs(vmg_ rem_prec, 3,
                    &rem_ext, &rem_hdl, &rem_ext2, &rem_hdl2,
                    &dvs_ext2, &dvs_hdl2);

    /* 
     *   Allocate another temp register for the shifted divisor.  Note
     *   that we need a different precision here, which is why we must
     *   make a separate allocation call 
     */
    err_try
    {
        /* make the additional allocation */
        alloc_temp_regs(vmg_ dvs_prec, 1, &dvs_ext, &dvs_hdl);
    }
    err_catch(exc)
    {
        /* delete the first group of registers we allocated */
        release_temp_regs(vmg_ 2, rem_hdl, rem_hdl2);

        /* re-throw the exception */
        err_rethrow();
    }
    err_end;

    /* the dividend is the initial value of the running remainder */
    copy_val(rem_ext, ext1, TRUE);

    /* copy the initial divisor into our temp register */
    copy_val(dvs_ext, ext2, TRUE);

    /* we haven't set a non-zero leading digit yet */
    lead_dig_set = FALSE;

    /*
     *   scale the divisor so that the divisor and dividend have the same
     *   exponent, and absorb the multiplier in the quotient 
     */
    quo_exp = get_exp(ext1) - get_exp(ext2) + 1;
    set_exp(dvs_ext, get_exp(ext1));

    /* we don't have a zero remainder yet */
    zero_remainder = FALSE;

    /* 
     *   if the quotient is going to be entirely fractional, the dividend
     *   is the remainder, and the quotient is zero 
     */
    if (new_rem_ext != 0 && quo_exp <= 0)
    {
        /* copy the initial remainder into the output remainder */
        copy_val(new_rem_ext, rem_ext, TRUE);

        /* set the quotient to zero */
        set_zero(new_ext);

        /* we have the result - no need to do any more work */
        goto done;
    }

    /* 
     *   Loop over each digit of precision of the quotient.
     */
    for (quo_idx = 0 ; ; )
    {
        int rem_approx, dvs_approx;
        int dig_approx;
        char *tmp;
        int exp_diff;

        /* start out with 0 in our digit accumulator */
        acc = 0;

        /*
         *   Get the first few digits of the remainder, and the first few
         *   digits of the divisor, rounding up the divisor and rounding
         *   down the remainder.  Compute the quotient - this will give us
         *   a rough guess and a lower bound for the current digit's
         *   value.  
         */
        rem_approx = (get_dig(rem_ext, 0)*100
                      + get_dig(rem_ext, 1)*10
                      + get_dig(rem_ext, 2));
        dvs_approx = (get_dig(dvs_ext, 0)*100
                      + (dvs_prec >= 2 ? get_dig(dvs_ext, 1) : 0)*10
                      + (dvs_prec >= 3 ? get_dig(dvs_ext, 2) : 0)
                      + 1);

        /* adjust for differences in the scale */
        exp_diff = get_exp(rem_ext) - get_exp(dvs_ext);
        if (exp_diff > 0)
        {
            /* the remainder is larger - scale it up */
            for ( ; exp_diff > 0 ; --exp_diff)
                rem_approx *= 10;
        }
        else if (exp_diff <= -3)
        {
            /* 
             *   The divisor is larger by more than 10^3, which means that
             *   the result of our integer division is definitely going to
             *   be zero, so there's no point in actually doing the
             *   calculation.  This is only a special case because, for
             *   sufficiently large negative differences, we'd have to
             *   multiply our divisor approximation by 10 so many times
             *   that we'd overflow a native int type, at which point we'd
             *   get 0 in the divisor, which would result in a
             *   divide-by-zero.  To avoid this, just put 1000 in our
             *   divisor, which is definitely larger than anything we can
             *   have in rem_ext at this point (since it was just three
             *   decimal digits, after all).  
             */
            dvs_approx = 1000;
        }
        else if (exp_diff < 0)
        {
            /* the divisor is larger - scale it up */
            for ( ; exp_diff < 0 ; ++exp_diff)
                dvs_approx *= 10;
        }

        /* calculate our initial guess for this digit */
        dig_approx = rem_approx / dvs_approx;

        /*
         *   If this digit is above 2, it'll save us a lot of work to
         *   subtract digit*divisor once, rather than iteratively
         *   deducting the divisor one time per iteration.  (It costs us a
         *   little to calculate the digit*divisor product, so we don't
         *   want to do this for very small digit values.)  
         */
        if (dig_approx > 2)
        {
            /* put the approximate digit in the accumulator */
            acc = dig_approx;

            /* make a copy of the divisor */
            copy_val(dvs_ext2, dvs_ext, FALSE);

            /* 
             *   multiply it by the guess for the digit - we know this
             *   will always be less than or equal to the real value
             *   because of the way we did the rounding 
             */
            mul_by_long(dvs_ext2, (ulong)dig_approx);

            /* subtract it from the running remainder */
            compute_abs_diff_into(rem_ext2, rem_ext, dvs_ext2);

            /* if that leaves zero in the remainder, note it */
            if (is_zero(rem_ext2))
            {
                zero_remainder = TRUE;
                break;
            }

            /* 
             *   swap the remainder registers, since rem_ext2 is now the
             *   new running remainder value 
             */
            tmp = rem_ext;
            rem_ext = rem_ext2;
            rem_ext2 = tmp;

            /*
             *   Now we'll finish up the job by subtracting the divisor
             *   from the remainder as many more times as necessary for
             *   the remainder to fall below the divisor.  We can't be
             *   exact at this step because we're not considering all of
             *   the digits, but we should only have one more subtraction
             *   remaining at this point.  
             */
        }
        
        /* 
         *   subtract the divisor from the running remainder as many times
         *   as we can 
         */
        for ( ; ; ++acc)
        {
            int comp_res;

            /* compare the running remainder to the divisor */
            comp_res = compare_abs(rem_ext, dvs_ext);
            if (comp_res < 0)
            {
                /* 
                 *   the remainder is smaller than the divisor - we have
                 *   our result for this digit 
                 */
                break;
            }
            else if (comp_res == 0)
            {
                /* note that we have a zero remainder */
                zero_remainder = TRUE;

                /* count one more subtraction */
                ++acc;

                /* we have our result for this digit */
                break;
            }

            /* subtract it */
            compute_abs_diff_into(rem_ext2, rem_ext, dvs_ext);

            /* 
             *   swap the remainder registers, since rem_ext2 is now the
             *   new running remainder value 
             */
            tmp = rem_ext;
            rem_ext = rem_ext2;
            rem_ext2 = tmp;
        }

        /* store this digit of the quotient */
        if (quo_idx < quo_prec)
        {
            /* store the digit */
            set_dig(new_ext, quo_idx, acc);
        }
        else if (quo_idx == quo_prec)
        {
            /* set the quotient's exponent */
            set_exp(new_ext, quo_exp);

            /* 
             *   this is the last digit, which we calculated for rounding
             *   purposes only - if it's 5 or greater, round up the value,
             *   otherwise leave it as it is 
             */
            if (acc >= 5)
                round_up_abs(new_ext);

            /* we've reached the rounding digit - we can stop now */
            break;
        }

        /* 
         *   if this is a non-zero digit, we've found a significant
         *   leading digit 
         */
        if (acc != 0)
            lead_dig_set = TRUE;

        /* 
         *   if we've found a significant leading digit, move to the next
         *   digit of the quotient; if not, adjust the quotient exponent
         *   down one, and keep preparing to write the first digit at the
         *   first "column" of the quotient
         */
        if (lead_dig_set)
            ++quo_idx;
        else
            --quo_exp;

        /* 
         *   If we have an exact result (a zero remainder), we're done.
         *   
         *   Similarly, if we've reached the units digit, and the caller
         *   wants the remainder, stop now - the caller wants an integer
         *   result for the quotient, which we now have.
         *   
         *   Similarly, if we've reached the rounding digit, and the
         *   caller wants the remainder, skip the rounding step - the
         *   caller wants an unrounded result for the quotient so that the
         *   quotient times the divisor plus the remainder equals the
         *   dividend.  
         */
        if (zero_remainder
            || (new_rem_ext != 0
                && ((int)quo_idx == quo_exp || quo_idx == quo_prec)))
        {
            /* zero any remaining digits of the quotient */
            for ( ; quo_idx < quo_prec ; ++quo_idx)
                set_dig(new_ext, quo_idx, 0);

            /* set the quotient's exponent */
            set_exp(new_ext, quo_exp);

            /* that's it */
            break;
        }

        /* divide the divisor by ten */
        set_exp(dvs_ext, get_exp(dvs_ext) - 1);
    }

    /* store the remainder if the caller wants the value */
    if (new_rem_ext != 0)
    {
        /* save the remainder into the caller's buffer */
        if (zero_remainder)
        {
            /* the remainder is exactly zero */
            set_zero(new_rem_ext);
        }
        else
        {
            /* copy the running remainder */
            copy_val(new_rem_ext, rem_ext, TRUE);

            /* the remainder has the same sign as the dividend */
            set_neg(new_rem_ext, get_neg(ext1));

            /* normalize the remainder */
            normalize(new_rem_ext);
        }
    }

    /* 
     *   the quotient is positive if both the divisor and dividend have
     *   the same sign, negative if they're different 
     */
    set_neg(new_ext, get_neg(ext1) != get_neg(ext2));

    /* normalize the quotient */
    normalize(new_ext);

done:
    /* release the temporary registers */
    release_temp_regs(vmg_ 4, rem_hdl, rem_hdl2, dvs_hdl, dvs_hdl2);
}

/*
 *   Compare the absolute values of two numbers 
 */
int CVmObjBigNum::compare_abs(const char *ext1, const char *ext2)
{
    size_t idx;
    int zero1 = is_zero(ext1);
    int zero2 = is_zero(ext2);
    size_t prec1 = get_prec(ext1);
    size_t prec2 = get_prec(ext2);
    size_t max_prec;
    size_t min_prec;
    const char *max_ext;
    int result;

    /* 
     *   if one is zero and the other is not, the one that's not zero has
     *   a larger absolute value 
     */
    if (zero1)
        return (zero2 ? 0 : -1);
    if (zero2)
        return (zero1 ? 0 : 1);

    /* 
     *   if the exponents differ, the one with the larger exponent is the
     *   larger number (this is necessarily true because we keep all
     *   numbers normalized) 
     */
    if (get_exp(ext1) > get_exp(ext2))
        return 1;
    else if (get_exp(ext1) < get_exp(ext2))
        return -1;

    /* 
     *   The exponents are equal, so we must compare the mantissas digit
     *   by digit 
     */

    /* get the larger of the two precisions */
    min_prec = prec2;
    max_prec = prec1;
    max_ext = ext1;
    if (prec2 > max_prec)
    {
        min_prec = prec1;
        max_prec = prec2;
        max_ext = ext2;
    }

    /* 
     *   The digits are in order from most significant to least
     *   significant, which means we can use memcmp to compare the common
     *   parts.  However, we can only compare an even number of digits
     *   this way, so round down the common precision if it's odd. 
     */
    if (min_prec > 1
        && (result = memcmp(ext1 + VMBN_MANT, ext2 + VMBN_MANT,
                            min_prec/2)) != 0)
    {
        /* 
         *   they're different in the common memcmp-able parts, so return
         *   the memcmp result 
         */
        return result;
    }

    /* if the common precision is odd, compare the final common digit */
    if ((min_prec & 1) != 0
        && (result = ((int)get_dig(ext1, min_prec-1)
                      - (int)get_dig(ext2, min_prec-1))) != 0)
        return result;

    /* 
     *   the common parts of the mantissas are identical; check each
     *   remaining digit of the longer one to see if any are non-zero 
     */
    for (idx = min_prec ; idx < max_prec ; ++idx)
    {
        /* 
         *   if this digit is non-zero, the longer one is greater, because
         *   the shorter one has an implied zero in this position 
         */
        if (get_dig(max_ext, idx) != 0)
            return (int)prec1 - (int)prec2;
    }

    /* they're identical */
    return 0;
}

/* ------------------------------------------------------------------------ */
/*
 *   Round a value 
 */
const char *CVmObjBigNum::round_val(VMG_ vm_val_t *new_val, const char *ext,
                                    size_t digits, int always_create)
{
    char *new_ext;
    int idx;
    int need_carry;
    int need_round;

    /* presume we need rounding */
    need_round = TRUE;

    /* 
     *   if the value is already no longer than the requested precision,
     *   return the original value; similarly, if we don't have to do any
     *   rounding to truncate to the requested precision, do not change
     *   the original object; likewise, don't bother changing anything if
     *   it's not a number 
     */
    if (digits >= get_prec(ext) || get_dig(ext, digits) < 5
        || get_type(ext) != VMBN_T_NUM)
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
    new_ext = get_objid_ext(vmg_ new_val->val.obj);

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

    /* apply the rounding */
    for (need_carry = TRUE, idx = digits ; idx != 0 ; )
    {
        /* move to the previous index value */
        --idx;

        /* round up - if it's a 9, we need to carry */
        if (get_dig(new_ext, idx) == 9)
        {
            /* make it a zero, and keep going to carry it */
            set_dig(new_ext, idx, 0);
        }
        else
        {
            /* bump it up */
            set_dig(new_ext, idx, get_dig(new_ext, idx) + 1);

            /* no carrying required, so we can stop now */
            need_carry = FALSE;
            break;
        }
    }

    /* 
     *   if we need to carry, we must have had all nines, in which case we
     *   now have all zeroes - put a 1 in for the first digit, and
     *   increase the exponent to account for the change 
     */
    if (need_carry)
    {
        /* set the lead digit to 1 */
        set_dig(new_ext, 0, 1);

        /* increase the exponent */
        set_exp(new_ext, get_exp(new_ext) + 1);
    }

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
        val->set_obj(create(vmg_ FALSE, val->val.intval, 32));

        /* done protecting my object reference */
        G_stk->discard();
    }

    /* if it's not a BigNumberobject, we can't handle it */
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

/*
 *   Allocate a temporary register 
 */
char *CVmObjBigNum::alloc_temp_reg(VMG_ size_t prec, uint *hdl)
{
    char *p;
    
    /* allocate a register with enough space for the desired precision */
    p = G_bignum_cache->alloc_reg(calc_alloc(prec), hdl);

    /* if that succeeded, initialize the memory */
    if (p != 0)
    {
        /* set the desired precision */
        set_prec(p, prec);

        /* initialize the flags */
        p[VMBN_FLAGS] = 0;
    }

    /* return the register memory */
    return p;
}

/*
 *   release a temporary register 
 */
void CVmObjBigNum::release_temp_reg(VMG_ uint hdl)
{
    /* release the register to the cache */
    G_bignum_cache->release_reg(hdl);
}

/*
 *   Allocate a group of temporary registers 
 */
void CVmObjBigNum::alloc_temp_regs(VMG_ size_t prec, size_t cnt, ...)
{
    va_list marker;
    size_t i;
    int failed;
    char **ext_ptr;
    uint *hdl_ptr;

    /* set up to read varargs */
    va_start(marker, cnt);

    /* no failures yet */
    failed = FALSE;

    /* scan the varargs list */
    for (i = 0 ; i < cnt ; ++i)
    {
        /* get the next argument */
        ext_ptr = va_arg(marker, char **);
        hdl_ptr = va_arg(marker, uint *);

        /* allocate a register */
        *ext_ptr = alloc_temp_reg(vmg_ prec, hdl_ptr);

        /* if this allocation failed, note it, but keep going for now */
        if (*ext_ptr == 0)
            failed = TRUE;
    }

    /* done reading argument */
    va_end(marker);

    /* if we had any failures, free all of the registers we allocated */
    if (failed)
    {
        /* restart reading the varargs */
        va_start(marker, cnt);

        /* scan the varargs and free the successfully allocated registers */
        for (i = 0 ; i < cnt ; ++i)
        {
            /* get the next argument */
            ext_ptr = va_arg(marker, char **);
            hdl_ptr = va_arg(marker, uint *);

            /* free this register if we successfully allocated it */
            if (*ext_ptr != 0)
                release_temp_reg(vmg_ *hdl_ptr);
        }

        /* done reading varargs */
        va_end(marker);

        /* throw the error */
        err_throw(VMERR_BIGNUM_NO_REGS);
    }
}

/*
 *   Release a block of temporary registers 
 */
void CVmObjBigNum::release_temp_regs(VMG_ size_t cnt, ...)
{
    size_t i;
    va_list marker;
    
    /* start reading the varargs */
    va_start(marker, cnt);
    
    /* scan the varargs and free the listed registers */
    for (i = 0 ; i < cnt ; ++i)
    {
        uint hdl;

        /* get the next handle */
        hdl = va_arg(marker, uint);

        /* free this register */
        release_temp_reg(vmg_ hdl);
    }
    
    /* done reading varargs */
    va_end(marker);
}


/* ------------------------------------------------------------------------ */
/*
 *   Register cache 
 */

/*
 *   initialize 
 */
CVmBigNumCache::CVmBigNumCache(size_t max_regs)
{
    CVmBigNumCacheReg *p;
    size_t i;
    
    /* allocate our register array */
    reg_ = (CVmBigNumCacheReg *)t3malloc(max_regs * sizeof(reg_[0]));

    /* remember the number of registers */
    max_regs_ = max_regs;

    /* clear the list heads */
    free_reg_ = 0;
    unalloc_reg_ = 0;

    /* we haven't actually allocated any registers yet - clear them out */
    for (p = reg_, i = max_regs ; i != 0 ; ++p, --i)
    {
        /* clear this register descriptor */
        p->clear();

        /* link it into the unallocated list */
        p->nxt_ = unalloc_reg_;
        unalloc_reg_ = p;
    }

    /* we haven't allocated the constants registers yet */
    ln10_.clear();
    pi_.clear();
    e_.clear();
}

/*
 *   delete 
 */
CVmBigNumCache::~CVmBigNumCache()
{
    CVmBigNumCacheReg *p;
    size_t i;

    /* delete each of our allocated registers */
    for (p = reg_, i = max_regs_ ; i != 0 ; ++p, --i)
        p->free_mem();

    /* free the register list array */
    t3free(reg_);

    /* free the constant value registers */
    ln10_.free_mem();
    pi_.free_mem();
    e_.free_mem();
}

/*
 *   Allocate a register 
 */
char *CVmBigNumCache::alloc_reg(size_t siz, uint *hdl)
{
    CVmBigNumCacheReg *p;
    CVmBigNumCacheReg *prv;

    /* 
     *   search the free list for an available register satisfying the
     *   size requirements 
     */
    for (p = free_reg_, prv = 0 ; p != 0 ; prv = p, p = p->nxt_)
    {
        /* if it satisfies the size requirements, return it */
        if (p->siz_ >= siz)
        {
            /* unlink it from the free list */
            if (prv == 0)
                free_reg_ = p->nxt_;
            else
                prv->nxt_ = p->nxt_;

            /* it's no longer in the free list */
            p->nxt_ = 0;

            /* return it */
            *hdl = (uint)(p - reg_);
            return p->buf_;
        }
    }
    
    /* 
     *   if there's an unallocated register, allocate it and use it;
     *   otherwise, reallocate the smallest free register 
     */
    if (unalloc_reg_ != 0)
    {
        /* use the first unallocated register */
        p = unalloc_reg_;
        
        /* unlink it from the list */
        unalloc_reg_ = unalloc_reg_->nxt_;
    }
    else if (free_reg_ != 0)
    {
        CVmBigNumCacheReg *min_free_p;
        CVmBigNumCacheReg *min_free_prv = 0;

        /* search for the smallest free register */
        for (min_free_p = 0, p = free_reg_, prv = 0 ; p != 0 ;
             prv = p, p = p->nxt_)
        {
            /* if it's the smallest so far, remember it */
            if (min_free_p == 0 || p->siz_ < min_free_p->siz_)
            {
                /* remember it */
                min_free_p = p;
                min_free_prv = prv;
            }
        }

        /* use the smallest register we found */
        p = min_free_p;

        /* unlink it from the list */
        if (min_free_prv == 0)
            free_reg_ = p->nxt_;
        else
            min_free_prv->nxt_ = p->nxt_;
    }
    else
    {
        /* there are no free registers - return failure */
        return 0;
    }
    
    /* 
     *   we found a register that was either previously unallocated, or
     *   was previously allocated but was too small - allocate or
     *   reallocate the register at the new size 
     */
    p->alloc_mem(siz);

    /* return the new register */
    *hdl = (uint)(p - reg_);
    return p->buf_;
}

/*
 *   Release a register
 */
void CVmBigNumCache::release_reg(uint hdl)
{
    CVmBigNumCacheReg *p = &reg_[hdl];
    
    /* add the register to the free list */
    p->nxt_ = free_reg_;
    free_reg_ = p;
}

/*
 *   Release all registers 
 */
void CVmBigNumCache::release_all()
{
    size_t i;

    /* mark each of our registers as not in use */
    for (i = 0 ; i < max_regs_ ; ++i)
        release_reg(i);
}
