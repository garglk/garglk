/* $Header$ */

/* 
 *   Copyright (c) 2000, 2002 Michael J. Roberts.  All Rights Reserved.
 *   
 *   Please see the accompanying license file, LICENSE.TXT, for information
 *   on using and copying this software.  
 */
/*
Name
  vmbignum.h - big number metaclass
Function
  
Notes
  
Modified
  02/18/00 MJRoberts  - Creation
*/

#ifndef VMBIGNUM_H
#define VMBIGNUM_H

#include <stdlib.h>
#include <os.h>
#include "vmtype.h"
#include "vmobj.h"
#include "vmglob.h"

/* ------------------------------------------------------------------------ */
/*
 *   Big number temporary register cache.  Because big number values are
 *   of arbitrary precision, we can't know in advance how much space we'll
 *   need for temporary values.  Instead, we keep this cache; each time we
 *   need a temporary register, we'll look to see if we have one available
 *   with sufficient precision, and allocate a new register if not.
 */

/* internal register descriptor */
struct CVmBigNumCacheReg
{
    /* clear out the register values */
    void clear()
    {
        buf_ = 0;
        siz_ = 0;
        nxt_ = 0;
    }

    /* 
     *   Allocate memory for this register.  Returns true if we had to
     *   allocate memory, false if the register was already at the given
     *   size. 
     */
    int alloc_mem(size_t siz)
    {
        /* 
         *   if I'm already at least this large, there's no need to change
         *   anything 
         */
        if (siz_ >= siz)
            return FALSE;

        /* 
         *   round up the size a bit - this will avoid repeatedly
         *   reallocating at slightly different sizes, which could
         *   fragment the heap quite a bit; we'll use a little more memory
         *   than the caller actually asked for, but if they come back and
         *   ask for slightly more next time, an additional allocation
         *   probably won't be necessary, which will save memory in the
         *   long run 
         */
        siz = (siz + 63) & ~63;

        /* delete any existing memory */
        free_mem();
        
        /* remember the new size */
        siz_ = siz;

        /* allocate the memory */
        buf_ = (char *)t3malloc(siz_);

        /* indicate that we allocated memory */
        return TRUE;
    }

    /* free the memory associated with the register, if any */
    void free_mem()
    {
        /* if we have a buffer, delete it */
        if (buf_ != 0)
            t3free(buf_);
    }
    
    /* register's buffer */
    char *buf_;

    /* size of register's buffer */
    size_t siz_;

    /* next register in list */
    CVmBigNumCacheReg *nxt_;
};

/*
 *   register cache 
 */
class CVmBigNumCache
{
public:
    CVmBigNumCache(size_t max_regs);
    ~CVmBigNumCache();
    
    /* 
     *   Allocate a temporary register with a minimum of the given byte
     *   size.  Returns a pointer to the register's buffer, and fills in
     *   '*hdl' with the handle of the register, which must be used to
     *   relesae the register.  
     */
    char *alloc_reg(size_t siz, uint *hdl);

    /* release a previously-allocated register */
    void release_reg(uint hdl);

    /* release all registers */
    void release_all();

    /* 
     *   Get a special dedicated constant value register, reallocating it
     *   to the required precision if it's not already available at the
     *   required precision or greater.  We'll set '*expanded' to true if
     *   we had to expand the register, false if not.  
     */

    /* get the constant ln(10) register */
    char *get_ln10_reg(size_t siz, int *expanded)
        { return alloc_reg(&ln10_, siz, expanded); }
    
    /* get the constant pi register */
    char *get_pi_reg(size_t siz, int *expanded)
        { return alloc_reg(&pi_, siz, expanded); }

    /* get the constant e register */
    char *get_e_reg(size_t siz, int *expanded)
        { return alloc_reg(&e_, siz, expanded); }

private:
    /* make sure a register is allocated to a given size, and return it */
    char *alloc_reg(CVmBigNumCacheReg *reg, size_t siz, int *expanded)
    {
        /* make sure the register satisfies the size requested */
        *expanded = reg->alloc_mem(siz);

        /* return the register's buffer */
        return reg->buf_;
    }
    
    /* our register array */
    CVmBigNumCacheReg *reg_;

    /* head of free register list */
    CVmBigNumCacheReg *free_reg_;

    /* head of unallocated register list */
    CVmBigNumCacheReg *unalloc_reg_;

    /* constant register for ln(10) */
    CVmBigNumCacheReg ln10_;

    /* constant register for pi */
    CVmBigNumCacheReg pi_;

    /* constant register for e */
    CVmBigNumCacheReg e_;

    /* maximum number of registers we can create */
    size_t max_regs_;
};

/* ------------------------------------------------------------------------ */
/*
 *   We store a BigNumber value as a varying-length string of BCD-encoded
 *   digits; we store two digits in each byte.  Our bytes are stored from
 *   most significant to least significant, and each byte has the more
 *   significant half in the high part of the byte.
 *   
 *   UINT2 number_of_digits
 *.  INT2 exponent
 *.  BYTE flags
 *.  BYTE most_significant_byte
 *.  ...
 *.  BYTE least_significant_byte
 *   
 *   Note that the number of bytes of the varying length mantissa string
 *   is equal to (number_of_digits+1)/2, because one byte stores two
 *   digits.
 *   
 *   The flags are:
 *   
 *   (flags & 0x0001) - sign bit; zero->non-negative, nonzero->negative
 *   
 *   (flags & 0x0006):
 *.  0x0000 -> normal number
 *.  0x0002 -> NOT A NUMBER
 *.  0x0004 -> INFINITY (sign bit indicates sign of infinity)
 *.  0x0006 -> reserved - should always be zero for now
 *   
 *   (flags & 0x0008) - zero bit; if set, the number's value is zero
 *   
 *   All other flag bits are reserved and should be set to zero.
 *   
 *   The exponent field gives the base-10 exponent of the number.  This is
 *   a signed quantity; a negative value indicates that the mantissa is to
 *   be divided by (10 ^ abs(exponent)), and a positive value indicates
 *   that the mantissa is to be multiplied by (10 ^ exponent).
 *   
 *   There is an implicit decimal point before the first byte of the
 *   mantissa.  
 */

/* byte offsets in byte string of various parts */
#define VMBN_PREC    0
#define VMBN_EXP     2
#define VMBN_FLAGS   4
#define VMBN_MANT    5

/* flags masks */
#define VMBN_F_NEG        0x0001                  /* negative sign bit flag */
#define VMBN_F_TYPE_MASK  0x0006                        /* number type mask */
#define VMBN_F_ZERO       0x0008                               /* zero flag */

/* number types */
#define VMBN_T_NUM        0x0000                         /* ordinary number */
#define VMBN_T_NAN        0x0002                            /* NOT A NUMBER */
#define VMBN_T_INF        0x0004         /* INFINITY (negative or positive) */
#define VMBN_T_RSRVD      0x0006                                /* reserved */

/* ------------------------------------------------------------------------ */
/*
 *   Flags for cvt_to_string 
 */

/* always show a sign, even if positive */
#define VMBN_FORMAT_SIGN          0x0001

/* always use exponential format */
#define VMBN_FORMAT_EXP           0x0002

/* always show a sign in the exponent */
#define VMBN_FORMAT_EXP_SIGN      0x0004

/* always show at least a zero before the decimal point */
#define VMBN_FORMAT_LEADING_ZERO  0x0008

/* always show a decimal point */
#define VMBN_FORMAT_POINT         0x0010

/* show the exponential 'e' (if any) in upper-case */
#define VMBN_FORMAT_EXP_CAP       0x0020

/* insert commas to denote thousands, millions, etc */
#define VMBN_FORMAT_COMMAS        0x0020

/* show a leading space if the number is positive */
#define VMBN_FORMAT_POS_SPACE     0x0040

/* use European-style formatting */
#define VMBN_FORMAT_EUROSTYLE     0x0080

/* ------------------------------------------------------------------------ */
/*
 *   BigNumber metaclass - intrinsic function vector indices 
 */
enum vmobjbn_meta_fnset
{
    /* undefined function */
    VMOBJBN_UNDEF = 0,

    /* format to a string */
    VMOBJBN_FORMAT = 1,

    /* equal after rounding? */
    VMOBJBN_EQUAL_RND = 2,

    /* getPrecision */
    VMOBJBN_GET_PREC = 3,

    /* setPrecision */
    VMOBJBN_SET_PREC = 4,

    /* getFraction */
    VMOBJBN_FRAC = 5,

    /* getWhole */
    VMOBJBN_WHOLE = 6,

    /* round to a given number of decimal places */
    VMOBJBN_ROUND_DEC = 7,

    /* absolute value */
    VMOBJBN_ABS = 8,

    /* ceiling */
    VMOBJBN_CEIL = 9,

    /* floor */
    VMOBJBN_FLOOR = 10,

    /* getScale */
    VMOBJBN_GETSCALE = 11,

    /* scale */
    VMOBJBN_SCALE = 12,

    /* negate */
    VMOBJBN_NEGATE = 13,

    /* copySignFrom */
    VMOBJBN_COPYSIGN = 14,

    /* isNegative */
    VMOBJBN_ISNEG = 15,

    /* getRemainder */
    VMOBJBN_REMAINDER = 16,

    /* sine */
    VMOBJBN_SIN = 17,

    /* cosine */
    VMOBJBN_COS = 18,

    /* sine */
    VMOBJBN_TAN = 19,

    /* degreesToRadians */
    VMOBJBN_D2R = 20,

    /* radiansToDegrees */
    VMOBJBN_R2D = 21,

    /* arcsine */
    VMOBJBN_ASIN = 22,

    /* arccosine */
    VMOBJBN_ACOS = 23,

    /* arctangent */
    VMOBJBN_ATAN = 24,

    /* sqrt */
    VMOBJBN_SQRT = 25,
    
    /* natural log */
    VMOBJBN_LN = 26,

    /* exp */
    VMOBJBN_EXP = 27,

    /* log10 */
    VMOBJBN_LOG10 = 28,

    /* power */
    VMOBJBN_POW = 29,

    /* hyperbolic sine */
    VMOBJBN_SINH = 30,

    /* hyperbolic cosine */
    VMOBJBN_COSH = 31,

    /* hyperbolic tangent */
    VMOBJBN_TANH = 32,

    /* get pi (static method) */
    VMOBJBN_GET_PI = 33,

    /* get e (static method) */
    VMOBJBN_GET_E = 34
};

/* ------------------------------------------------------------------------ */
/*
 *   Big Number metaclass 
 */
class CVmObjBigNum: public CVmObject
{
    friend class CVmMetaclassBigNum;

public:
    /* metaclass registration object */
    static class CVmMetaclass *metaclass_reg_;
    class CVmMetaclass *get_metaclass_reg() const { return metaclass_reg_; }

    /* am I of the given metaclass? */
    virtual int is_of_metaclass(class CVmMetaclass *meta) const
    {
        /* try my own metaclass and my base class */
        return (meta == metaclass_reg_
                || CVmObject::is_of_metaclass(meta));
    }

    /* 
     *   write to a 'data' mode file - returns zero on success, non-zero on
     *   I/O or other error 
     */
    int write_to_data_file(osfildef *fp);

    /* 
     *   Read from a 'data' mode file, creating a new BigNumber object to
     *   hold the result.  Returns zero on success, non-zero on failure.  On
     *   success, *retval will be filled in with the new BigNumber object.  
     */
    static int read_from_data_file(VMG_ vm_val_t *retval, osfildef *fp);

    /* create dynamically using stack arguments */
    static vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr,
                                         uint argc);

    /* call a static property */
    static int call_stat_prop(VMG_ vm_val_t *result,
                              const uchar **pc_ptr, uint *argc,
                              vm_prop_id_t prop);

    /* reserve constant data */
    virtual void reserve_const_data(VMG_ class CVmConstMapper *mapper,
                                    vm_obj_id_t self);

    /* convert to constant data */
    virtual void convert_to_const_data(VMG_ class CVmConstMapper *mapper,
                                       vm_obj_id_t self);

    /* create with a given precision */
    static vm_obj_id_t create(VMG_ int in_root_set, size_t digits);

    /* create with a given value */
    static vm_obj_id_t create(VMG_ int in_root_set, long val, size_t digits);

    /* determine if an object is a BigNumber */
    static int is_bignum_obj(VMG_ vm_obj_id_t obj)
        { return vm_objp(vmg_ obj)->is_of_metaclass(metaclass_reg_); }

    /* notify of deletion */
    void notify_delete(VMG_ int in_root_set);

    /* set a property */
    void set_prop(VMG_ class CVmUndo *undo,
                  vm_obj_id_t self, vm_prop_id_t prop, const vm_val_t *val);

    /* get a property */
    int get_prop(VMG_ vm_prop_id_t prop, vm_val_t *val,
                 vm_obj_id_t self, vm_obj_id_t *source_obj, uint *argc);

    /* undo operations - we are immutable and hence keep no undo */
    void notify_new_savept() { }
    void apply_undo(VMG_ struct CVmUndoRecord *) { }
    void mark_undo_ref(VMG_ struct CVmUndoRecord *) { }
    void remove_stale_undo_weak_ref(VMG_ struct CVmUndoRecord *) { }    

    /* mark references - we have no references so this does nothing */
    void mark_refs(VMG_ uint) { }

    /* remove weak references */
    void remove_stale_weak_refs(VMG0_) { }

    /* load from an image file */
    void load_from_image(VMG_ vm_obj_id_t, const char *ptr, size_t)
        { ext_ = (char *)ptr; }

    /* rebuild for image file */
    virtual ulong rebuild_image(VMG_ char *buf, ulong buflen);

    /* save to a file */
    void save_to_file(VMG_ class CVmFile *fp);

    /* restore from a file */
    void restore_from_file(VMG_ vm_obj_id_t self,
                           class CVmFile *fp, class CVmObjFixup *fixup);

    /* add a value */
    void add_val(VMG_ vm_val_t *result,
                 vm_obj_id_t self, const vm_val_t *val);

    /* subtract a value */
    void sub_val(VMG_ vm_val_t *result,
                 vm_obj_id_t self, const vm_val_t *val);

    /* multiply */
    void mul_val(VMG_ vm_val_t *result,
                 vm_obj_id_t self, const vm_val_t *val);

    /* divide */
    void div_val(VMG_ vm_val_t *result,
                 vm_obj_id_t self, const vm_val_t *val);

    /* negate */
    void neg_val(VMG_ vm_val_t *result, vm_obj_id_t self);

    /* check a value for equality */
    int equals(VMG_ vm_obj_id_t self, const vm_val_t *val, int depth) const;

    /* calculate a hash value for the number */
    uint calc_hash(VMG_ vm_obj_id_t self, int depth) const;

    /* compare to another value */
    int compare_to(VMG_ vm_obj_id_t /*self*/, const vm_val_t *) const;

    /* 
     *   Create a string representation of the number.  We'll use a
     *   default format that uses no more than a maximum number of
     *   characters to represent the string.  We'll avoid exponential
     *   format if possible, but we'll fall back on exponential format if
     *   the non-exponential result would exceed our default maximum
     *   length.  
     */
    virtual const char *cast_to_string(VMG_ vm_obj_id_t self,
                                       vm_val_t *new_str) const;

    /* 
     *   Static method to convert big number data to a string.  We'll
     *   create a new string object and store a reference in new_str,
     *   returning a pointer to its data buffer.
     *   
     *   max_digits is the maximum number of digits we should produce.  If
     *   our precision is greater than this would allow, we'll round.  If
     *   we have more digits before the decimal point than this would
     *   allow, we'll use exponential notation.
     *   
     *   whole_places is the number of places before the decimal point
     *   that we should produce.  This is a minimum; if we need more
     *   places (and we're not in exponential notation), we'll take the
     *   additional places.  If, however, we don't manage to fill this
     *   quota, we'll pad with spaces to the left.  We ignore whole_places
     *   in exponential format.
     *   
     *   frac_digits is the number of digits after the decimal point that
     *   we should produce.  We'll round if we have more precision than
     *   this would allow, or pad with zeroes if we don't have enough
     *   precision.  If frac_digits is -1, we will produce as many
     *   fractional digits as we need up to the max_digits limit.
     *   
     *   If the VMBN_FORMAT_EXP flag isn't set, we'll format the number
     *   without an exponent as long as we have enough space in max_digits
     *   for the part before the decimal point, and we have enough space
     *   in max_digits and frac_digits that a number with a small absolute
     *   value wouldn't show up as all zeroes.
     *   
     *   If the VMBN_FORMAT_POINT flag is set, we'll show a decimal point
     *   for all numbers.  Otherwise, if frac_digits is zero, or
     *   frac_digits is -1 and the number has no fractional part, we'll
     *   suppress the decimal point.  This doesn't matter when frac_digits
     *   is greater than zero, or it's -1 and there's a fractional part to
     *   display.
     *   
     *   If exp_digits is non-zero, it specifies the minimum number of
     *   digits to display in the exponent.  We'll pad with zeroes to make
     *   this many digits if necessary.
     *   
     *   If lead_fill is provided, it must be a string value.  We'll fill
     *   the string with the characters from this string, looping to
     *   repeat the string if necessary.  If this string isn't provided,
     *   we'll use leading spaces.  This is only needed if the
     *   whole_places value requires us to insert filler.  
     */
    static const char *cvt_to_string(VMG_ vm_obj_id_t self, vm_val_t *new_str,
                                     const char *ext,
                                     int max_digits, int whole_places,
                                     int frac_digits, int exp_digits,
                                     ulong flags, vm_val_t *lead_fill);

    /* format the value into the given buffer */
    char *cvt_to_string_buf(VMG_ char *buf, size_t buflen, int max_digits,
                            int whole_places, int frac_digits, int exp_digits,
                            ulong flags);

    /* compute a sum */
    static void compute_sum(VMG_ vm_val_t *result,
                            const char *ext1, const char *ext2);

    /* compute a difference */
    static void compute_diff(VMG_ vm_val_t *result,
                             const char *ext1, const char *ext2);

    /* compute a product */
    static void compute_prod(VMG_ vm_val_t *result,
                             const char *ext1, const char *ext2);

    /* compute a quotient */
    static void compute_quotient(VMG_ vm_val_t *result,
                                 const char *ext1, const char *ext2);

    /* compute a remainder */
    static void compute_rem(VMG_ vm_val_t *result,
                            const char *ext1, const char *ext2);

    /* compute a natural logarithm */
    static void compute_ln_into(VMG_ char *dst, const char *src);

    /* compute e^x */
    static void compute_exp_into(VMG_ char *dst, const char *src);

    /* compute a hyperbolic sine or cosine */
    static void compute_sinhcosh_into(VMG_ char *dst, const char *src,
                                      int is_cosh, int is_tanh);

    /* 
     *   Determine if two values are exactly equal.  If one value has more
     *   precision than the other, we'll implicitly extend the shorter
     *   value with trailing zeroes. 
     */
    static int compute_eq_exact(const char *ext1, const char *ext2);

    /* 
     *   Determine if two values are equal with rounding.  If one value is
     *   less precise than the other, we'll round the more precise value
     *   to the shorter precision, and compare the shorter number to the
     *   rounded longer number. 
     */
    static int compute_eq_round(VMG_ const char *ext1, const char *ext2);

    /* 
     *   Create a rounded value, rounding to the given precision.  If
     *   always_create is true, we'll create a new number regardless of
     *   whether rounding is required; otherwise, when the caller can
     *   simply treat the old value as truncated, we'll set new_val to nil
     *   and return the original value.
     */
    static const char *round_val(VMG_ vm_val_t *new_val, const char *ext,
                                 size_t digits, int always_create);

    /* set my value to a given integer value */
    void set_int_val(long val);

    /* set my value to a given string */
    void set_str_val(const char *str, size_t len);

    /* get my data pointer */
    char *get_ext() const { return ext_; }

    /* convert to an integer value */
    long convert_to_int();

protected:
    /* create with no extension */
    CVmObjBigNum();
    
    /* create with a given precision */
    CVmObjBigNum(VMG_ size_t digits);

    /* create with a given precision, initializing with an integer value */
    CVmObjBigNum(VMG_ long val, size_t digits);

    /* create with a given precision, initializing with a string value */
    CVmObjBigNum(VMG_ const char *str, size_t len, size_t digits);

    /* convert a value to a BigNumber */
    int cvt_to_bignum(VMG_ vm_obj_id_t self, vm_val_t *val) const;

    /* 
     *   general string conversion routine - converts to a string, storing
     *   the result either in the caller's buffer, or in a new string
     *   created for the conversion 
     */
    static char *cvt_to_string_gen(VMG_ vm_val_t *new_str,
                                   const char *ext,
                                   int max_digits, int whole_places,
                                   int frac_digits, int exp_digits,
                                   ulong flags, vm_val_t *lead_fill,
                                   char *buf, size_t buflen);

    /* property evaluator - undefined property */
    int getp_undef(VMG_ vm_obj_id_t, vm_val_t *, uint *) { return FALSE; }

    /* property evaluator - formatString */
    int getp_format(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - equalRound */
    int getp_equal_rnd(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - getPrecision */
    int getp_get_prec(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - setPrecision */
    int getp_set_prec(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - getFraction */
    int getp_frac(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - getWhole */
    int getp_whole(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - roundToDecimal */
    int getp_round_dec(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - absolute value */
    int getp_abs(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - ceiling */
    int getp_ceil(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - floor */
    int getp_floor(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - getScale */
    int getp_get_scale(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - scale */
    int getp_scale(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - negate */
    int getp_negate(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - copySignFrom */
    int getp_copy_sign(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - isNegative */
    int getp_is_neg(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - remainder */
    int getp_remainder(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - sine */
    int getp_sin(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - cosine */
    int getp_cos(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - tangent */
    int getp_tan(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - radiansToDegrees */
    int getp_rad2deg(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - degreesToRadians */
    int getp_deg2rad(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - arcsine */
    int getp_asin(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - arccosine */
    int getp_acos(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - arcsine */
    int getp_atan(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - square root */
    int getp_sqrt(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - natural log */
    int getp_ln(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - exp */
    int getp_exp(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - log10 */
    int getp_log10(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - power */
    int getp_pow(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - hyperbolic sine */
    int getp_sinh(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - hyperbolic cosine */
    int getp_cosh(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - hyperbolic tangent */
    int getp_tanh(VMG_ vm_obj_id_t self, vm_val_t *val, uint *argc);

    /* property evaluator - get pi */
    int getp_pi(VMG_ vm_obj_id_t, vm_val_t *val, uint *argc)
        { return s_getp_pi(vmg_ val, argc); }

    /* property evaluator - get e */
    int getp_e(VMG_ vm_obj_id_t, vm_val_t *val, uint *argc)
        { return s_getp_e(vmg_ val, argc); }

    /* static property evaluator - get pi */
    static int s_getp_pi(VMG_ vm_val_t *val, uint *argc);

    /* static property evaluator - get e */
    static int s_getp_e(VMG_ vm_val_t *val, uint *argc);

    /* set up for a getp operation with zero arguments */
    int setup_getp_0(VMG_ vm_obj_id_t self, vm_val_t *retval,
                     uint *argc, char **new_ext);

    /* set up for a getp operation with one argument */
    int setup_getp_1(VMG_ vm_obj_id_t self, vm_val_t *retval,
                     uint *argc, char **new_ext,
                     vm_val_t *val2, const char **val2_ext,
                     int use_self_prec);

    /* set up a return value for a getp operation */
    int setup_getp_retval(VMG_ vm_obj_id_t self, vm_val_t *retval,
                          char **new_ext, size_t prec);

    /* common property evaluator for asin and acos */
    int calc_asincos(VMG_ vm_obj_id_t self,
                     vm_val_t *retval, uint *argc, int is_acos);

    /* common property evaluator - sinh, cosh, and tanh */
    int calc_sinhcosh(VMG_ vm_obj_id_t self,
                      vm_val_t *retval, uint *argc,
                      int is_cosh, int is_tanh);

    /* calculate asin or acos into the given buffer */
    static void calc_asincos_into(VMG_ char *new_ext, const char *ext,
                                  int is_acos);

    /* 
     *   Calculate the arcsin series expansion; valid only for small
     *   values of x (0 < x < 1/sqrt(2)).  The argument value is in ext1,
     *   and we return a pointer to the register containing the result.  
     */
    static char *calc_asin_series(char *ext1, char *ext2,
                                  char *ext3, char *ext4, char *ext5);

    /* 
     *   Compute the ln series expansion.  This is valid only for small
     *   arguments; the argument is in ext1 initially.  Returns a pointer
     *   to the register containing the result 
     */
    static char *compute_ln_series_into(VMG_ char *ext1, char *ext2,
                                        char *ext3, char *ext4, char *ext5);

    /* allocate space for a given number of decimal digits */
    void alloc_bignum(VMG_ size_t digits);

    /* calculate how much space we need for a given number of digits */
    static size_t calc_alloc(size_t digits);

    /* initialize a computation for a two-operand operator */
    static char *compute_init_2op(VMG_ vm_val_t *result,
                                  const char *ext1, const char *ext2);

    /* compute a square root */
    static void compute_sqrt_into(VMG_ char *new_ext, const char *ext);

    /* compute the sum of two operands into the given buffer */
    static void compute_sum_into(char *new_next,
                                 const char *ext1, const char *ext2);

    /* 
     *   Compute the sum of the absolute values of the operands into the
     *   given buffer.  The result is always positive.  The result buffer
     *   must have a precision at least as large as the larger of the two
     *   input precisions.  
     */
    static void compute_abs_sum_into(char *new_ext,
                                     const char *ext1, const char *ext2);

    /*
     *   Compute the difference of the absolute values of the operands
     *   into the given buffer.  The result is positive if the first value
     *   is larger than the second, negative if the first value is smaller
     *   than the second.  The result buffer must have precision at least
     *   as large as the larger of the two input precisions.  
     */
    static void compute_abs_diff_into(char *new_ext,
                                      const char *ext1, const char *ext2);

    /*
     *   Compute the product of the two values into the given buffer.  The
     *   result buffer must have precision at least as large as the larger
     *   of the two input precisions.  
     */
    static void compute_prod_into(char *new_ext,
                                  const char *ext1, const char *ext2);

    /*
     *   Compute the quotient of th etwo values into the given buffer If
     *   new_rem_ext is not null, we'll store the remainder there.  
     */
    static void compute_quotient_into(VMG_ char *new_ext,
                                      char *new_rem_ext,
                                      const char *ext1, const char *ext2);

    /*
     *   Compare the absolute values of two numbers.  If the first is
     *   greater than the second, we'll return a positive result.  If the
     *   two are equal, we'll return zero.  If the first is less than the
     *   second, we'll return a negative result.  This routine ignores NAN
     *   and INF values, so the caller must ensure that only ordinary
     *   numbers are passed to this routine.  
     */
    static int compare_abs(const char *ext1, const char *ext2);

    /* get/set the digit precision */
    static size_t get_prec(const char *ext)
        { return osrp2(ext + VMBN_PREC); }
    static void set_prec(char *ext, size_t prec)
        { oswp2(ext + VMBN_PREC, prec); }

    /* get/set the exponent */
    static int get_exp(const char *ext)
        { return osrp2s(ext + VMBN_EXP); }
    static void set_exp(char *ext, int exp)
        { oswp2(ext + VMBN_EXP, exp); }

    /* get the negative sign flag */
    static int get_neg(const char *ext)
        { return (ext[VMBN_FLAGS] & VMBN_F_NEG) != 0; }

    /* set/clear negative sign flag */
    static void set_neg(char *ext, int neg)
    {
        if (neg)
            ext[VMBN_FLAGS] |= VMBN_F_NEG;
        else
            ext[VMBN_FLAGS] &= ~VMBN_F_NEG;
    }

    /* get the number type */
    static int get_type(const char *ext)
        { return ext[VMBN_FLAGS] & VMBN_F_TYPE_MASK; }
    
    /* set the number type (to a VMBN_T_xxx value) */
    static void set_type(char *ext, int typ)
    {
        /* clear the old number type */
        ext[VMBN_FLAGS] &= ~VMBN_F_TYPE_MASK;
        
        /* set the new number type */
        ext[VMBN_FLAGS] |= typ;
    }

    /* get a digit at a particular index (0 = most significant) */
    static unsigned int get_dig(const char *ext, size_t i)
    {
        unsigned int pair;
        
        /* get the digit pair containing our digit */
        pair = ext[VMBN_MANT + i/2];

        /* 
         *   If it's an even index, we need the high half.  Otherwise, we
         *   need the low half.
         *   
         *   This is a bit tricky, all to avoid a condition branch.  If
         *   the index is even, (i & 1) will be 0, otherwise (i & 1) will
         *   be 1.  So, (1 - (i & 1)) will be 1 if even, 0 if odd.  That
         *   quantity shifted left twice will hence be 4 if the index is
         *   even, 0 if the index is odd.  Thus, we'll shift the pair
         *   right by 4 if the index is even, yielding the high part, or
         *   shift right by 0 if the index is odd, keeping the low part.  
         */
        pair >>= ((1 - (i & 1)) << 2);

        /* mask to one digit */
        return (pair & 0x0f);
    }

    /* set a digit at a particular index */
    static void set_dig(char *ext, size_t i, unsigned int dig)
    {
        unsigned char mask;

        /* make sure our input digit is just a digit */
        dig &= 0x0F;

        /* 
         *   If it's an even index, we need to store our digit in the high
         *   half.  Otherwise, we need to store it in the low half.  So,
         *   if we're storing in an even index, shift our number left 4
         *   bits so that it's in the high half of its low byte;
         *   otherwise, leave the number as-is.  
         */
        dig <<= ((1 - (i & 1)) << 2);

        /*
         *   We need a mask that we can AND the current value with to
         *   preserve the half we're not changing, but clear the other
         *   half.  So, we need 0x0F if we're setting the high half (even
         *   index), or 0xF0 if we're setting the low half (odd index).
         *   Use the same trick as above, with the shift sense inverted,
         *   so generate our mask.  
         */
        mask = (0x0F << ((i & 1) << 2));
        
        /* mask out our part from the pair */
        ext[VMBN_MANT + i/2] &= mask;

        /* OR in our digit now that we've masked the place clear */
        ext[VMBN_MANT + i/2] |= (unsigned char)dig;
    }
    
    /* shift mantissa left/right, leaving the exponent unchanged */
    static void shift_left(char *ext, unsigned int shift);
    static void shift_right(char *ext, unsigned int shift);

    /* multiply a number by a long integer value */
    static void mul_by_long(char *ext, unsigned long val);

    /* divide a number by a long integer value */
    static void div_by_long(char *ext, unsigned long val);

    /* increment a number's absolute value */
    static void increment_abs(char *ext);

    /* round a number's value up - increments the least significant digit */
    static void round_up_abs(char *ext);

    /* 
     *   copy a value - if the new value has greater precision than the
     *   old value, we'll extend with zeroes in the new least significance
     *   digits; if the new value has smaller precision than the old
     *   value, and 'round' is false, we'll simply truncate the value to
     *   the new precision.  If 'round' is true and we're reducing the
     *   precision, we'll round up the value instead of truncating it.  
     */
    static void copy_val(char *dst, const char *src, int round);

    /* normalize a number */
    static void normalize(char *ext);

    /* set a number to zero */
    static void set_zero(char *ext)
    {
        /* set the exponent to one */
        set_exp(ext, 1);

        /* set the zero flag */
        ext[VMBN_FLAGS] |= VMBN_F_ZERO;

        /* set the sign to non-negative */
        set_neg(ext, FALSE);

        /* set the type to ordinary number */
        set_type(ext, VMBN_T_NUM);

        /* set the mantissa to all zeroes */
        memset(ext + VMBN_MANT, 0, (get_prec(ext) + 1)/2);
    }

    /* determine if the number equals zero */
    static int is_zero(const char *ext)
        { return (ext[VMBN_FLAGS] & VMBN_F_ZERO) != 0; }

    /* negate a value */
    static void negate(char *ext)
    {
        /* only change the sign if the value is non-zero */
        if (!is_zero(ext))
        {
            /* it's not zero - invert the sign */
            set_neg(ext, !get_neg(ext));
        }
    }

    /* make a value negative */
    static void make_negative(char *ext)
    {
        /* only set the sign if the value is non-zero */
        if (!is_zero(ext))
            set_neg(ext, TRUE);
    }

    /* check to see if the fractional part is zero */
    static int is_frac_zero(const char *ext);

    /* 
     *   check for NAN and INF conditions - returns true if the number is
     *   a NAN or INF, false if it's an ordinary number 
     */
    static int is_nan(const char *ext)
    {
        /* if it's anything but an ordinary number, indicate NAN */
        return (get_type(ext) != VMBN_T_NUM);
    }

    /* calculate a Taylor series for sin(x) */
    void calc_sin_series(VMG_ char *new_ext, char *ext1, char *ext2,
                         char *ext3, char *ext4, char *ext5,
                         char *ext6, char *ext7);

    /* calculate a Taylor series for cos(x) */
    void calc_cos_series(VMG_ char *new_ext, char *ext1, char *ext2,
                         char *ext3, char *ext4, char *ext5,
                         char *ext6, char *ext7);

    /* 
     *   given an object number known to refer to a CVmBigNum object, get
     *   the object's extension 
     */
    static char *get_objid_ext(VMG_ vm_obj_id_t obj_id)
    {
        /* get the object pointer, cast it, and get the extension */
        return get_objid_obj(vmg_ obj_id)->get_ext();
    }

    /* 
     *   given an object number known to refer to a CVmBigNum object, get
     *   the object pointer
     */
    static CVmObjBigNum *get_objid_obj(VMG_ vm_obj_id_t obj_id)
    {
        /* get the object pointer and cast it */
        return (CVmObjBigNum *)vm_objp(vmg_ obj_id);
    }

    /* allocate a temporary register */
    static char *alloc_temp_reg(VMG_ size_t prec, uint *hdl);

    /* 
     *   Allocate a set of temporary registers; throws an error on
     *   failure.  For each register, there is an additional pair of
     *   arguments: a (char **) to receive a pointer to the register
     *   memory, and a (uint *) to receive the register handle.  
     */
    static void alloc_temp_regs(VMG_ size_t prec, size_t cnt, ...);

    /* 
     *   Release a set of temporary registers.  For each register, there
     *   is a uint argument giving the handle of the register to release. 
     */
    static void release_temp_regs(VMG_ size_t cnt, ...);

    /* release a temporary register */
    static void release_temp_reg(VMG_ uint hdl);

    /* 
     *   Get the natural logarithm of 10 to the required precision.  We'll
     *   return the cached value if available, or compute and cache the
     *   constant to (at least) the required precision if not.  
     */
    static const char *cache_ln10(VMG_ size_t prec);

    /* cache pi to the required precision */
    static const char *cache_pi(VMG_ size_t prec);

    /* cache e to the required precision */
    static const char *cache_e(VMG_ size_t prec);

    /* get the constant value 1 */
    static const char *get_one() { return (const char *)one_; }

    /* constant value 1 */
    static const unsigned char one_[];

    /* property evaluation function table */
    static int (CVmObjBigNum::*func_table_[])(VMG_ vm_obj_id_t self,
                                              vm_val_t *retval, uint *argc);
};

/* ------------------------------------------------------------------------ */
/*
 *   Registration table object 
 */
class CVmMetaclassBigNum: public CVmMetaclass
{
public:
    /* get the global name */
    const char *get_meta_name() const { return "bignumber/030000"; }

    /* create from image file */
    void create_for_image_load(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjBigNum();
        G_obj_table->set_obj_gc_characteristics(id, FALSE, FALSE);
    }

    /* create from restoring from saved state */
    void create_for_restore(VMG_ vm_obj_id_t id)
    {
        new (vmg_ id) CVmObjBigNum();
        G_obj_table->set_obj_gc_characteristics(id, FALSE, FALSE);
    }

    /* create dynamically using stack arguments */
    vm_obj_id_t create_from_stack(VMG_ const uchar **pc_ptr, uint argc)
        { return CVmObjBigNum::create_from_stack(vmg_ pc_ptr, argc); }

    /* call a static property */
    int call_stat_prop(VMG_ vm_val_t *result,
                       const uchar **pc_ptr, uint *argc,
                       vm_prop_id_t prop)
    {
        return CVmObjBigNum::call_stat_prop(vmg_ result, pc_ptr, argc, prop);
    }
};


#endif /* VMBIGNUM_H */

/*
 *   Register the class 
 */
VM_REGISTER_METACLASS(CVmObjBigNum)
