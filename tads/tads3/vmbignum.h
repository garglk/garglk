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
     *   release the register.  
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
    
    /* get the constant ln(2) register */
    char *get_ln2_reg(size_t siz, int *expanded)
        { return alloc_reg(&ln2_, siz, expanded); }

    /* get the constant pi register */
    char *get_pi_reg(size_t siz, int *expanded)
        { return alloc_reg(&pi_, siz, expanded); }

    /* get the constant e register */
    char *get_e_reg(size_t siz, int *expanded)
        { return alloc_reg(&e_, siz, expanded); }

    /* get the DBL_MAX register */
    char *get_dbl_max_reg(size_t siz, int *expanded)
        { return alloc_reg(&dbl_max_, siz, expanded); }

    /* get the DBL_MIN register */
    char *get_dbl_min_reg(size_t siz, int *expanded)
        { return alloc_reg(&dbl_min_, siz, expanded); }

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

    /* constant register for ln(2) */
    CVmBigNumCacheReg ln2_;

    /* constant register for pi */
    CVmBigNumCacheReg pi_;

    /* constant register for e */
    CVmBigNumCacheReg e_;

    /* constant registers for DBL_MAX and DBL_MIN */
    CVmBigNumCacheReg dbl_max_;
    CVmBigNumCacheReg dbl_min_;

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
 *.  0x0006 -> reserved for future use
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

/* 
 *   Use scientific notation if it's more compact, a la C printf 'g' format.
 *   This automatically uses exponent notation if the exponent is less then
 *   -4 or greater than or equal to the number of digits after the decimal
 *   point.  
 */
#define VMBN_FORMAT_COMPACT       0x0100

/* max_digits counts only significant digits, not leading zeros */
#define VMBN_FORMAT_MAXSIG        0x0200

/* keep trailing zeros to fill out the max_digits count */
#define VMBN_FORMAT_TRAILING_ZEROS 0x0400


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
    VMOBJBN_GET_E = 34,

    /* numType */
    VMOBJN_numType = 35
};


/* ------------------------------------------------------------------------ */
/*
 *   String formatter output buffer interface. 
 */
class IBigNumStringBuf
{
public:
    /* 
     *   Get the string buffer to use, given the required string length.  If
     *   it's not possible to get a buffer of the required size, returns
     *   null, in which case the formatter will return failure as well.
     *   
     *   The formatter always fills in the first two bytes of the result
     *   buffer with a VMB_LEN length prefix, TADS-string style.  The space
     *   for the two-byte prefix is included in the 'need' value passed.
     */
    virtual char *get_buf(size_t need) = 0;
};

/*
 *   Simple output buffer with a fixed, pre-allocated string.
 */
class CBigNumStringBufFixed: public IBigNumStringBuf
{
public:
    CBigNumStringBufFixed(char *buf, size_t len) : buf(buf), len(len) { }
    char *get_buf(size_t need) { return len >= need ? buf : 0; }

    char *buf;
    size_t len;
};

/*
 *   Output buffer using an optional pre-allocated buffer, allocating a new
 *   CVmObjString object if the fixed buffer isn't big enough.  The caller
 *   can recover the new string value from our 'strval' member after the
 *   formatting is done; this is set to nil if the fixed buffer provided is
 *   sufficient.
 */
class CBigNumStringBufAlo: public CBigNumStringBufFixed
{
public:
    /* create with no buffer - always allocates a new string */
    CBigNumStringBufAlo(VMG_ vm_val_t *strval)
        : CBigNumStringBufFixed(0, 0), strval(strval)
    {
        vmg = VMGLOB_ADDR;
        if (strval != 0)
            strval->set_nil();
    }

    /* 
     *   create with a buffer - uses the buffer if it's big enough, otherwise
     *   allocates a new String object, storing the object in our 'strval'
     *   member 
     */
    CBigNumStringBufAlo(VMG_ vm_val_t *strval, char *buf, size_t len)
        : CBigNumStringBufFixed(buf, len), strval(strval)
    {
        vmg = VMGLOB_ADDR;
        if (strval != 0)
            strval->set_nil();
    }

    /* get the buffer */
    char *get_buf(size_t need);

    /* the caller's OUT variable to receive the allocated string, if needed */
    vm_val_t *strval;

    /* stashed globals */
    vm_globals *vmg;
};


/* ------------------------------------------------------------------------ */
/*
 *   we have to forward-declare bignum_t, since it uses templates
 */
template <int prec> class bignum_t;


/* ------------------------------------------------------------------------ */
/*
 *   Big Number metaclass 
 */
class CVmObjBigNum: public CVmObject
{
    friend class CVmMetaclassBigNum;
    template <int> friend class bignum_t;
    friend class vbignum_t;

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

    /* initialize/terminate the BigNumber cache */
    static void init_cache();
    static void term_cache();

    /* 
     *   write to a 'data' mode file - returns zero on success, non-zero on
     *   I/O or other error 
     */
    int write_to_data_file(class CVmDataSource *fp);

    /* 
     *   Read from a 'data' mode file, creating a new BigNumber object to
     *   hold the result.  Returns zero on success, non-zero on failure.  On
     *   success, *retval will be filled in with the new BigNumber object.  
     */
    static int read_from_data_file(
        VMG_ vm_val_t *retval, class CVmDataSource *fp);

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

    /* create from a given integer value */
    static vm_obj_id_t create(VMG_ int in_root_set, long val, size_t digits);

    /* create from a given unsigned integer value */
    static vm_obj_id_t createu(VMG_ int in_root_set, ulong val, size_t digits);

    /* create from a given string value, with a given precision */
    static vm_obj_id_t create(VMG_ int in_root_set,
                              const char *str, size_t len, size_t digits);

    /* create from a double, with enough precision for a native double */
    static vm_obj_id_t create(VMG_ int in_root_set, double val);

    /* create from a double, with a given precision */
    static vm_obj_id_t create(VMG_ int in_root_set, double val, size_t digits);

    /* create from a bignum_t */
    template <int prec> static vm_obj_id_t create(
        VMG_ int in_root_set, const bignum_t<prec> *b)
    {
        vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
        new (vmg_ id) CVmObjBigNum(vmg_ prec);
        CVmObjBigNum *n = (CVmObjBigNum *)vm_objp(vmg_ id);
        copy_val(n->ext_, b->ext, FALSE);
        return id;
    }

    /* create from a bignum_t */
    template <int prec> static vm_obj_id_t create(
        VMG_ int in_root_set, const bignum_t<prec> &b)
    {
        vm_obj_id_t id = vm_new_id(vmg_ in_root_set, FALSE, FALSE);
        new (vmg_ id) CVmObjBigNum(vmg_ prec);
        CVmObjBigNum *n = (CVmObjBigNum *)vm_objp(vmg_ id);
        copy_val(n->ext_, b.ext, FALSE);
        return id;
    }

    /* 
     *   Create from a 64-bit integer in portable little-endian notation.
     *   This is the 64-bit int equivalent of osrp2(), osrp4(), etc - the
     *   name is meant to parallel those names.  The buffer must be stored in
     *   our standard portable format: little-endian, two's complement.
     *   create_rp8() treats the value as unsigned, create_rp8s() treats it
     *   as signed.  
     */
    static vm_obj_id_t create_rp8(VMG_ int in_root_set, const char *buf);
    static vm_obj_id_t create_rp8s(VMG_ int in_root_set, const char *buf);

    /* 
     *   Create from a 64-bit integer expressed as two 32-bit portions.  We
     *   currently only provide an unsigned version, because (a) we don't
     *   have an immediate need for signed, and (b) breaking a signed 64-bit
     *   value into two 32-bit partitions requires making assumptions about
     *   the native integer representation (2's complement, 1s' complement,
     *   etc) that we'll probably have to farm out to os_xxx code.  Until we
     *   encounter a need we'll avoid the signed version.  The unsigned
     *   version will work with any binary integer representation, which
     *   makes it all but universal (it won't work on a BCD machine, for
     *   example... but would anyone ever notice, or care?).
     */
    static vm_obj_id_t create_int64(VMG_ int in_root_set,
                                    uint32_t hi, uint32_t lo);

    /* create from a BER-encoded compressed unsigned integer buffer */
    static vm_obj_id_t create_from_ber(VMG_ int in_root_set,
                                       const char *buf, size_t len);

    /* create from an IEEE 754-2008 binary interchange format buffer */
    static vm_obj_id_t create_from_ieee754(
        VMG_ int in_root_set, const char *buf, int bits);

    /* 
     *   create from a string value, using the precision required to hold the
     *   value in the string 
     */
    static vm_obj_id_t create(VMG_ int in_root_set,
                              const char *str, size_t len);

    /* 
     *   create from a string value in a given radix, using the precision
     *   required to hold the value in the string 
     */
    static vm_obj_id_t create_radix(VMG_ int in_root_set,
                                    const char *str, size_t len, int radix);

    /* determine if an object is a BigNumber */
    static int is_bignum_obj(VMG_ vm_obj_id_t obj)
        { return vm_objp(vmg_ obj)->is_of_metaclass(metaclass_reg_); }

    /* 
     *   Cast a value to a BigNumber.  We can convert strings, integers, and
     *   of course BigNumber values.  Fills in bnval with the BigNumber
     *   value, which will be newly allocated if the source isn't already a
     *   BigNumber (in which case bnval is just a copy of srcval).  
     */
    static void cast_to_bignum(VMG_ vm_val_t *bnval, const vm_val_t *srcval);

    /* 
     *   Parse a string value into an extension buffer.  If radix is 0, we
     *   infer the radix using radix_from_string() rules. 
     */
    static void parse_str_into(char *ext, const char *str, size_t len);
    static void parse_str_into(char *ext, const char *str, size_t len,
                               int radix);

    /* parse a string, allocating a buffer with 'new char[]' */
    static char *parse_str_alo(size_t &buflen,
                               const char *str, size_t len, int radix);

    /* 
     *   Get the precision required to hold the number with the given string
     *   representation.  If radix is 0, we infer the radix using
     *   radix_from_string() rules.
     */
    static int precision_from_string(const char *str, size_t len);
    static int precision_from_string(const char *str, size_t len, int radix);

    /* 
     *   Parse a string to determine its radix.  If the string starts with
     *   '0x' or '0', and contains no decimal point or 'E' exponent
     *   indicator, it's a hex or octal integer, respectively.  Otherwise
     *   it's a decimal integer or floating point value. 
     */
    static int radix_from_string(const char *str, size_t len);

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
    int add_val(VMG_ vm_val_t *result,
                vm_obj_id_t self, const vm_val_t *val);

    /* subtract a value */
    int sub_val(VMG_ vm_val_t *result,
                vm_obj_id_t self, const vm_val_t *val);

    /* multiply */
    int mul_val(VMG_ vm_val_t *result,
                vm_obj_id_t self, const vm_val_t *val);

    /* divide */
    int div_val(VMG_ vm_val_t *result,
                vm_obj_id_t self, const vm_val_t *val);

    /* negate */
    int neg_val(VMG_ vm_val_t *result, vm_obj_id_t self);

    /* get the absolute value */
    int abs_val(VMG_ vm_val_t *result, vm_obj_id_t self);

    /* get the sgn value (-1 if negative, 0 if zero, 1 if positive) */
    int sgn_val(VMG_ vm_val_t *result, vm_obj_id_t self);

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
     *   Explicitly cast to string with a given radix 
     */
    virtual const char *explicit_to_string(
        VMG_ vm_obj_id_t self, vm_val_t *new_str, int radix, int flags) const;

    /* 
     *   Static method to convert big number data to a string.  We'll create
     *   a new string object and store a reference in new_str, returning a
     *   pointer to its data buffer.
     *   
     *   max_digits is the maximum number of digits we should produce.  If
     *   our precision is greater than this would allow, we'll round.  If we
     *   have more digits before the decimal point than this would allow,
     *   we'll use exponential notation.
     *   
     *   If the flag VMBN_FORMAT_MAXSIG is set, max_digits is taken as the
     *   maximum number of *significant* digits to display. That is, we won't
     *   count leading zeros against the maximum.
     *   
     *   whole_places is the number of places before the decimal point that
     *   we should produce.  This is a minimum; if we need more places (and
     *   we're not in exponential notation), we'll take the additional
     *   places.  If, however, we don't manage to fill this quota, we'll pad
     *   with spaces to the left.  We ignore whole_places in exponential
     *   format.
     *   
     *   frac_digits is the number of digits after the decimal point that we
     *   should produce.  We'll round if we have more precision than this
     *   would allow, or pad with zeros if we don't have enough precision.
     *   If frac_digits is -1, we will produce as many fractional digits as
     *   we need up to the max_digits limit.
     *   
     *   If the VMBN_FORMAT_EXP flag isn't set, we'll format the number
     *   without an exponent as long as we have enough space in max_digits
     *   for the part before the decimal point, and we have enough space in
     *   max_digits and frac_digits that a number with a small absolute value
     *   wouldn't show up as all zeros.
     *   
     *   If the VMBN_FORMAT_POINT flag is set, we'll show a decimal point for
     *   all numbers.  Otherwise, if frac_digits is zero, or frac_digits is
     *   -1 and the number has no fractional part, we'll suppress the decimal
     *   point.  This doesn't matter when frac_digits is greater than zero,
     *   or it's -1 and there's a fractional part to display.
     *   
     *   If exp_digits is non-zero, it specifies the minimum number of digits
     *   to display in the exponent.  We'll pad with zeros to make this many
     *   digits if necessary.
     *   
     *   If lead_fill is provided, it must be a string value.  We'll fill the
     *   string with the characters from this string, looping to repeat the
     *   string if necessary.  If this string isn't provided, we'll use
     *   leading spaces.  This is only needed if the whole_places value
     *   requires us to insert filler.
     *   
     *   Note that VMG_ can be passed as VMGNULL_ (null global context) for
     *   the cvt_to_string_buf() functions IF 'new_str' and 'lead_fill' are
     *   null.
     */
    static const char *cvt_to_string(
        VMG_ vm_obj_id_t self, vm_val_t *new_str, const char *ext,
        int max_digits, int whole_places, int frac_digits, int exp_digits,
        ulong flags, vm_val_t *lead_fill);

    /* format the value into the given buffer */
    const char *cvt_to_string_buf(
        char *buf, size_t buflen, int max_digits,
        int whole_places, int frac_digits, int exp_digits, ulong flags);

    /* format BigNumber data (without a 'self') into the given buffer */
    static const char *cvt_to_string_buf(
        char *buf, size_t buflen, const char *ext,
        int max_digits, int whole_places, int frac_digits, int exp_digits,
        ulong flags, const char *lead_fill, size_t lead_fill_len);

    /* 
     *   format the value into the given buffer, or into a new String if the
     *   value overflows the buffer 
     */
    const char *cvt_to_string_buf(
        VMG_ vm_val_t *new_str, char *buf, size_t buflen, int max_digits,
        int whole_places, int frac_digits, int exp_digits, ulong flags);
        

    /* 
     *   format a regular integer value into a buffer as though it were a
     *   BigNumber value 
     */
    static const char *cvt_int_to_string_buf(
        char *buf, size_t buflen, int32_t intval,
        int max_digits, int whole_places, int frac_digits, int exp_digits,
        ulong flags);

    /* format as an integer in a given radix */
    const char *cvt_to_string_in_radix(
        VMG_ vm_obj_id_t self, vm_val_t *new_str, int radix) const;

    /* get the fractional part/whole part */
    static void compute_frac(char *ext);
    static void compute_whole(char *ext);

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
    static void compute_ln_into(char *dst, const char *src);

    /* compute e^x */
    static void compute_exp_into(char *dst, const char *src);

    /* compute a hyperbolic sine or cosine */
    static void compute_sinhcosh_into(char *dst, const char *src,
                                      int is_cosh, int is_tanh);

    /* 
     *   Determine if two values are exactly equal.  If one value has more
     *   precision than the other, we'll implicitly extend the shorter
     *   value with trailing zeros. 
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
    static void set_int_val(char *ext, long val);
    static void set_uint_val(char *ext, ulong val);

    /* set from a portable little-endian 64-bit unsigned int buffer */
    void set_rp8(const uchar *p);

    /* set my value to the given double */
    static void set_double_val(char *ext, double val);

    /* set my value to a given string */
    void set_str_val(const char *str, size_t len);
    void set_str_val(const char *str, size_t len, int radix);

    /* get my data pointer */
    char *get_ext() const { return ext_; }

    /* BigNumber is a numeric type */
    virtual int is_numeric() const { return TRUE; }

    /* cast to integer */
    virtual long cast_to_int(VMG0_) const
    {
        /* return the integer conversion */
        return convert_to_int();
    }

    /* get my integer value */
    virtual int get_as_int(long *val) const
    {
        /* return the integer conversion */
        *val = convert_to_int();
        return TRUE;
    }

    /* get my double value */
    virtual int get_as_double(VMG_ double *val) const
    {
        *val = convert_to_double();
        return TRUE;
    }

    /* promote an integer to a BigNumber */
    virtual void promote_int(VMG_ vm_val_t *val) const;

    /* 
     *   Convert to an integer value (signed or unsigned).  We set 'ov' to
     *   true if the value overflows the integer type. 
     */
    int32_t convert_to_int(int &ov) const { return ext_to_int(ext_, ov); }
    uint32_t convert_to_uint(int &ov) const;

    /* 
     *   convert to integer; sets 'ov' to true if the result doesn't fit an
     *   int32 
     */
    static int32_t ext_to_int(const char *ext, int &ov);

    /* convert to integer, throwing an error on overflow */
    int32_t convert_to_int() const
    {
        int ov;
        int32_t l = convert_to_int(ov);
        if (ov)
            err_throw(VMERR_NUM_OVERFLOW);
        return l;
    }
    uint32_t convert_to_uint() const
    {
        int ov;
        uint32_t l = convert_to_uint(ov);
        if (ov)
            err_throw(VMERR_NUM_OVERFLOW);
        return l;
    }

    /* cast to numeric */
    virtual void cast_to_num(VMG_ vm_val_t *val, vm_obj_id_t self) const
    {
        /* we're already numeric - just return myself unchanged */
        val->set_obj(self);
    }

    /* convert to double */
    double convert_to_double() const { return ext_to_double(ext_); }
    static double ext_to_double(const char *ext);

    /* 
     *   Convert to the IEEE 754-2008 binary interchange format with the
     *   given bit size.  These are a portable formats, which are not
     *   necessarily the same as any of the local system's native floating
     *   point types (although most modern hardware does use this format for
     *   its native types, modulo byte order).  The defined sizes are 16, 32,
     *   64, and 128 bits; 32 bits corresponds to the single-precision (C
     *   "float") type, and 64 bits corresponds to the double-precision (C
     *   "double") type.  We assume that the buffer is big enough for the
     *   requested bit size.  We use the standard IEEE format, *except* that
     *   we use little-endian byte order for consistency with the TADS
     *   pack/unpack formats. 
     */
    void convert_to_ieee754(VMG_ char *buf, int bits, int &ov);

    /* set the value from an IEEE 754 buffer */
    void set_ieee754_value(VMG_ const char *buf, int bits);

    /* 
     *   Write a 64-bit integer representation, using the standard TADS
     *   portable integer format: 8-bit bytes, little-endian, two's
     *   complement.  These are analogous to the osifc routines oswp2(),
     *   oswp2s(), etc., thus the names.
     *   
     *   (Why don't we need more osifc routines for these?  Because BigNumber
     *   internal representation is all under the control of the portable
     *   code, and the byte buffer representation is portable as well.  We
     *   don't need osifc code for a portable-to-portable conversion.  The
     *   integer routines do need osifc code because they convert to and from
     *   the local platform integer format, which obviously varies.  Even
     *   that could be done portably in principle, by using integer
     *   arithmetic; but we don't do it that way, because osifc code can be
     *   ruthlessly efficient on most platforms by directly accessing the
     *   byte structure of the local int type rather than doing a bunch of
     *   arithmetic.  There's no such efficiency gain possible with
     *   BigNumbers, which aren't native anywhere.)  
     */
    void wp8(char *buf, int &ov) const;
    void wp8s(char *buf, int &ov) const;

    /* write a BER compressed integer representation */
    void encode_ber(char *buf, size_t buflen, size_t &result_len, int &ov);

protected:
    /* create with no extension */
    CVmObjBigNum();
    
    /* create with a given precision */
    CVmObjBigNum(VMG_ size_t digits);

    /* create with a given precision, initializing with an integer value */
    CVmObjBigNum(VMG_ long val, size_t digits);

    /* create with a given precision, initializing with a string value */
    CVmObjBigNum(VMG_ const char *str, size_t len, size_t digits);

    /* create with a given precision, initializing with a double value */
    CVmObjBigNum(VMG_ double val, size_t digits);

    /* convert a value to a BigNumber */
    int cvt_to_bignum(VMG_ vm_obj_id_t self, vm_val_t *val) const;

    /* get the magnitude of the integer conversion, ignoring the sign bit */
    static uint32_t convert_to_int_base(const char *ext, int &ov);

    /* 
     *   general string conversion routine
     */
    static const char *cvt_to_string_gen(
        IBigNumStringBuf *buf, const char *ext, 
        int max_digits, int whole_places, int frac_digits, int exp_digits,
        ulong flags, const char *lead_fill, size_t lead_fill_len);

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

    /* property evaluator - get the number type */
    int getp_numType(VMG_ vm_obj_id_t, vm_val_t *val, uint *argc);

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
    static void calc_asincos_into(char *new_ext, const char *ext,
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
    static char *compute_ln_series_into(char *ext1, char *ext2,
                                        char *ext3, char *ext4, char *ext5);

    /* allocate space for a given number of decimal digits of precision */
    void alloc_bignum(VMG_ size_t prec);

    /* calculate how much space we need for a given precision */
    static size_t calc_alloc(size_t prec)
        { return (2 + 2 + 1) + ((prec + 1)/2); }

    /* initialize a computation for a two-operand operator */
    static char *compute_init_2op(VMG_ vm_val_t *result,
                                  const char *ext1, const char *ext2);

    /* compute a square root */
    static void compute_sqrt_into(char *new_ext, const char *ext);

    /* compute the sum of two operands into the given buffer */
    static void compute_sum_into(char *new_ext,
                                 const char *ext1, const char *ext2);

    /* 
     *   Compute the sum of the absolute values of the operands into the
     *   given buffer.  The result is always positive.  The result buffer
     *   must have a precision at least as large as the larger of the two
     *   input precisions.  
     */
    static void compute_abs_sum_into(char *new_ext,
                                     const char *ext1, const char *ext2);

    /* compute the difference of two operands into the given buffer */
    static void compute_diff_into(char *new_ext,
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
     *   Compute the quotient of the two values into the given buffer.  If
     *   new_rem_ext is not null, we'll store the remainder there.  
     */
    static void compute_quotient_into(char *new_ext,
                                      char *new_rem_ext,
                                      const char *ext1, const char *ext2);

    /* compare extensions - return 0 if equal, <0 if a<b, >0 if a>b */
    static int compare_ext(const char *a, const char *b);
    
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
        { oswp2s(ext + VMBN_EXP, exp); }

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
        /* get the digit pair containing our digit */
        unsigned int pair = ext[VMBN_MANT + i/2];

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

    /* 
     *   Divide a number by a long integer value.
     *   
     *   Important: 'val' must not exceed ULONG_MAX/10.  Our algorithm
     *   computes integer dividends from ext's digits; these can range up to
     *   10*val and have to fit in a ulong, thus the ULONG_MAX/10 limit on
     *   val.  
     */
    static void div_by_long(char *ext, unsigned long val,
                            unsigned long *remp = 0);

    /* divide by 2^32 */
    static void div_by_2e32(char *ext, uint32_t *remainder);

    /* 
     *   store the portable 64-bit integer representation of the absolute
     *   value of this number in the given buffer 
     */
    void wp8abs(char *buf, int &ov) const;

    /* compute the 2's complement of a p8 (64-bit little-endian int) buffer */
    static void twos_complement_p8(unsigned char *buf);

    /* increment a number's absolute value */
    static void increment_abs(char *ext);

    /* round for digits dropped during a calculation */
    static void round_for_dropped_digits(
        char *ext, int trail_dig, int trail_val);

    /* get the OR sum of digits from 'd' to least significant */
    static int get_ORdigs(const char *ext, int d);

    /* round an extension to the nearest integer */
    static void round_to_int(char *ext);

    /* round an extension to the given number of digits */
    static void round_to(char *ext, int digits);

    /* 
     *   get the rounding direction for rounding to the given number of
     *   digits: 1 means round up, 0 means round down, so you can simply add
     *   the return value to the last digit you're keeping 
     */
    static int get_round_dir(const char *ext, int digits);

    /* 
     *   round up: add 1 to the least significant digit of the number we're
     *   keeping (if not specified, we're keep all digits) 
     */
    static void round_up_abs(char *ext);
    static void round_up_abs(char *ext, int keep_digits);

    /* 
     *   copy a value - if the new value has greater precision than the
     *   old value, we'll extend with zeros in the new least significance
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

        /* set the mantissa to all zeros */
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

    /* check for infinities */
    static int is_infinity(const char *ext)
        { return get_type(ext) == VMBN_T_INF; }

    /* calculate a Taylor series for sin(x) */
    void calc_sin_series(VMG_ char *new_ext, char *ext1, char *ext2,
                         char *ext3, char *ext4, char *ext5,
                         char *ext6, char *ext7);

    /* calculate a Taylor series for cos(x) */
    void calc_cos_series(VMG_ char *new_ext, char *ext1, char *ext2,
                         char *ext3, char *ext4, char *ext5,
                         char *ext6, char *ext7);

    /* 
     *   given an object number known to refer to a CVmObjBigNum object, get
     *   the object's extension 
     */
    static char *get_objid_ext(VMG_ vm_obj_id_t obj_id)
    {
        /* get the object pointer, cast it, and get the extension */
        return get_objid_obj(vmg_ obj_id)->get_ext();
    }

    /* 
     *   given an object number known to refer to a CVmObjBigNum object, get
     *   the object pointer
     */
    static CVmObjBigNum *get_objid_obj(VMG_ vm_obj_id_t obj_id)
    {
        /* get the object pointer and cast it */
        return (CVmObjBigNum *)vm_objp(vmg_ obj_id);
    }

    /* allocate a temporary register */
    static char *alloc_temp_reg(size_t prec, uint *hdl);

    /* 
     *   Allocate a set of temporary registers; throws an error on
     *   failure.  For each register, there is an additional pair of
     *   arguments: a (char **) to receive a pointer to the register
     *   memory, and a (uint *) to receive the register handle.  
     */
    static void alloc_temp_regs(size_t prec, size_t cnt, ...);

    /* 
     *   Release a set of temporary registers.  For each register, there
     *   is a uint argument giving the handle of the register to release. 
     */
    static void release_temp_regs(size_t cnt, ...);

    /* release a temporary register */
    static void release_temp_reg(uint hdl);

    /* 
     *   Get the natural logarithm of 10 to the required precision.  We'll
     *   return the cached value if available, or compute and cache the
     *   constant to (at least) the required precision if not.  
     */
    static const char *cache_ln10(size_t prec);

    /* get ln(2) */
    static const char *cache_ln2(size_t prec);

    /* cache pi to the required precision */
    static const char *cache_pi(size_t prec);

    /* cache e to the required precision */
    static const char *cache_e(size_t prec);

    /* get the constant value 1 */
    static const char *get_one() { return (const char *)one_; }

    /* cache DBL_MAX, DBL_MIN */
    static const char *cache_dbl_max();
    static const char *cache_dbl_min();

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
    const char *get_meta_name() const { return "bignumber/030001"; }

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

/* ------------------------------------------------------------------------ */
/*
 *   A C++ template class for doing BigNumber arithmetic on the stack with
 *   fixed-precision buffers.  This can be used for high-precision arithmetic
 *   in C++ without creating any garbage-collected BigNumber objects.  It's
 *   not quite as flexible as the BigNumber class itself, but it allows C++
 *   code to perform decimal arithmetic with higher precision than doubles
 *   when needed.
 */
template <int prec> class bignum_t
{
    friend class CVmObjBigNum;
    
public:
    bignum_t() { init(); }
    bignum_t(long i) { init(); set(i); }
    bignum_t(double d) { init(); set(d); }
    bignum_t(VMG_ const vm_val_t *val) { init(); set(vmg_ val); }
    template <int precb> bignum_t(const bignum_t<precb> b)
        { init(); set(b); }
    template <int precb> bignum_t(const bignum_t<precb> *b)
        { init(); set(*b); }
    
        
    void set(long i) { CVmObjBigNum::set_int_val(ext, i); }
    void set(double d) { CVmObjBigNum::set_double_val(ext, d); }
    template <int bprec> void set(const bignum_t<bprec> &b)
        { CVmObjBigNum::copy_val(ext, b.ext, TRUE); }
    
    void set(VMG_ const vm_val_t *val)
    {
        CVmObjBigNum *b;
        if (val->typ == VM_INT)
            CVmObjBigNum::set_int_val(ext, val->val.intval);
        else if ((b = vm_val_cast(CVmObjBigNum, val)) != 0)
            CVmObjBigNum::copy_val(ext, b->get_ext(), TRUE);
        else if (val->is_numeric(vmg0_))
            CVmObjBigNum::set_double_val(ext, val->num_to_double(vmg0_));
        else
            err_throw(VMERR_NUM_VAL_REQD);
    }

    /* cast to int/double */
    operator int32_t()
    {
        int ov;
        int32_t ret = CVmObjBigNum::ext_to_int(ext, ov);
        if (ov)
            err_throw(VMERR_NUM_OVERFLOW);
        return ret;
    }
    operator double()
    {
        return CVmObjBigNum::ext_to_double(ext);
    }

    /* 
     *   addition operators
     */
    bignum_t operator +(long l) const
    {
        bignum_t<prec> bl(l);
        return *this + bl;
    }
    bignum_t operator +(double d) const
    {
        bignum_t<prec> bd(d);
        return *this + bd;
    }
    template <int precb> bignum_t operator +(const bignum_t<precb> &b) const
    {
        bignum_t<(precb > prec ? precb : prec)> result;
        CVmObjBigNum::compute_sum_into(result.ext, ext, b.ext);
        return result;
    }

    bignum_t &operator +=(long l) { set(*this + l); return *this; }
    bignum_t &operator +=(double d) { set(*this + d); return *this; }
    template <int precb> bignum_t &operator +=(bignum_t<precb> &b)
        { set(*this + b); return *this; }
    template <int precb> bignum_t &operator +=(bignum_t<precb> b)
        { set(*this + b); return *this; }

    /* 
     *   subtraction operators
     */
    bignum_t operator -(long l) const
    {
        bignum_t<prec> bl(l);
        return *this - bl;
    }
    bignum_t operator -(double d) const
    {
        bignum_t<prec> bd(d);
        return *this - bd;
    }
    template <int precb> bignum_t operator -(bignum_t<precb> &b) const
    {
        bignum_t<(precb > prec ? precb : prec)> result;
        CVmObjBigNum::compute_diff_into(result.ext, ext, b.ext);
        return result;
    }

    bignum_t &operator -=(long l) { set(*this - l); return *this; }
    bignum_t &operator -=(double d) { set(*this - d); return *this; }
    template <int precb> bignum_t &operator -=(bignum_t<precb> &b)
        { set(*this - b); return *this; }
    template <int precb> bignum_t &operator -=(bignum_t<precb> b)
        { set(*this - b); return *this; }

    /*
     *   Negation 
     */
    bignum_t<prec> operator -()
    {
        bignum_t<prec> result(this);
        CVmObjBigNum::negate(result.ext);
        return result;
    }

    /* 
     *   multiplication operators
     */
    bignum_t operator *(long l) const
    {
        bignum_t<prec> bl(l);
        return *this * bl;
    }
    bignum_t operator *(double d) const
    {
        bignum_t<prec> bd(d);
        return *this * bd;
    }
    template <int precb> bignum_t operator *(bignum_t<precb> &b) const
    {
        bignum_t<(precb > prec ? precb : prec)> result;
        CVmObjBigNum::compute_prod_into(result.ext, ext, b.ext);
        return result;
    }

    bignum_t &operator *=(long l) { set(*this * l); return *this; }
    bignum_t &operator *=(double d) { set(*this * d); return *this; }
    template <int precb> bignum_t &operator *=(bignum_t<precb> &b)
        { set(*this * b); return *this; }
    template <int precb> bignum_t &operator *=(bignum_t<precb> b)
        { set(*this * b); return *this; }

    /* 
     *   division operators
     */
    bignum_t operator /(long l) const
    {
        bignum_t<prec> bl(l);
        return *this / bl;
    }
    bignum_t operator /(double d) const
    {
        bignum_t<prec> bd(d);
        return *this / bd;
    }
    template <int precb> bignum_t operator /(bignum_t<precb> &b) const
    {
        bignum_t<(precb > prec ? precb : prec)> result;
        CVmObjBigNum::compute_quotient_into(result.ext, 0, ext, b.ext);
        return result;
    }

    bignum_t &operator /=(long l) { set(*this / l); return *this; }
    bignum_t &operator /=(double d) { set(*this / d); return *this; }
    template <int precb> bignum_t &operator /=(bignum_t<precb> &b)
        { set(*this / b); return *this; }
    template <int precb> bignum_t &operator /=(bignum_t<precb> b)
        { set(*this / b); return *this; }

    /* 
     *   modulo operators
     */
    bignum_t operator %(long l) const
    {
        bignum_t<prec> bl(l);
        return *this % bl;
    }
    bignum_t operator %(double d) const
    {
        bignum_t<prec> bd(d);
        return *this % bd;
    }
    template <int precb> bignum_t operator %(bignum_t<precb> &b) const
    {
        bignum_t<(precb > prec ? precb : prec)> quo, rem;
        CVmObjBigNum::compute_quotient_into(quo.ext, rem.ext, ext, b.ext);
        return rem;
    }
    template <int precb> bignum_t &operator %=(bignum_t<precb> &b)
        { set(*this % b); return *this; }
    template <int precb> bignum_t &operator %=(bignum_t<precb> b)
        { set(*this % b); return *this; }

    /*
     *   formatting 
     */
    void format(char *buf, size_t buflen)
    {
        CVmObjBigNum::cvt_to_string_buf(
            buf, buflen, ext, -1, -1, -1, -1, VMBN_FORMAT_POINT, 0, 0);
    }
    void format(char *buf, size_t buflen, int maxdigits, int fracdigits)
    {
        CVmObjBigNum::cvt_to_string_buf(
            buf, buflen, ext, maxdigits, -1, fracdigits, -1,
            VMBN_FORMAT_POINT, 0, 0);
    }
                                        
protected:
    void init()
    {
        oswp2(ext, prec);
        ext[VMBN_FLAGS] = 0;
    }
    char ext[VMBN_MANT + (prec+1)/2];
};

/*
 *   Yet another C++ BigNumber adapter class, this time for variable
 *   precision BigNumber values allocated on the C++ heap.  This is similar
 *   to bignum_t<int prec>, but allows the precision to be determined at
 *   run-time.  The trade-off is that this type must be allocated on the heap
 *   due to the dynamic precision.
 */
class vbignum_t
{
public:
    /* create from native types */
    vbignum_t(long l) { init(10); set(l); }
    vbignum_t(ulong l) { init(10); set(l); }
    vbignum_t(double d) { init(18); set(d); }
    vbignum_t(const char *str, size_t len)
    {
        init(CVmObjBigNum::precision_from_string(str, len));
        CVmObjBigNum::parse_str_into(ext_, str, len);
    }
    vbignum_t(const char *str, size_t len, int radix)
    {
        init(CVmObjBigNum::precision_from_string(str, len, radix));
        CVmObjBigNum::parse_str_into(ext_, str, len, radix);
    }

    /* delete */
    ~vbignum_t() { delete [] ext_; }

    /* set from native types */
    void set(long l) { CVmObjBigNum::set_int_val(ext_, l); }
    void set(ulong l) { CVmObjBigNum::set_uint_val(ext_, l); }
    void set(double d) { CVmObjBigNum::set_double_val(ext_, d); }

    /* get my precision in digits */
    int prec() const { return CVmObjBigNum::get_prec(ext_); }

    /* is the value zero? */
    int is_zero() const { return CVmObjBigNum::is_zero(ext_); }

    /* get the value as an int32; sets 'ov' to true on overflow */
    int32_t to_int(int &ov) { return CVmObjBigNum::ext_to_int(ext_, ov); }

    /* format */
    const char *format(IBigNumStringBuf *buf) const
    {
        return CVmObjBigNum::cvt_to_string_gen(
            buf, ext_, -1, -1, -1, -1, VMBN_FORMAT_POINT, 0, 0);
    }
    const char *format(IBigNumStringBuf *buf,
                       int maxdigits, int fracdigits) const
    {
        return CVmObjBigNum::cvt_to_string_gen(
            buf, ext_, maxdigits, -1, fracdigits, -1,
            VMBN_FORMAT_POINT, 0, 0);
    }
    const char *format(char *buf, size_t buflen) const
    {
        return CVmObjBigNum::cvt_to_string_buf(
            buf, buflen, ext_, -1, -1, -1, -1, VMBN_FORMAT_POINT, 0, 0);
    }
    const char *format(char *buf, size_t buflen,
                       int maxdigits, int fracdigits) const
    {
        return CVmObjBigNum::cvt_to_string_buf(
            buf, buflen, ext_, maxdigits, -1, fracdigits, -1,
            VMBN_FORMAT_POINT, 0, 0);
    }

    /* cast to int/double */
    operator int32_t()
    {
        int ov;
        int32_t ret = CVmObjBigNum::ext_to_int(ext_, ov);
        if (ov)
            err_throw(VMERR_NUM_OVERFLOW);
        return ret;
    }
    operator double()
    {
        return CVmObjBigNum::ext_to_double(ext_);
    }

    /* compare - returns 0 if equal, <0 if this<b, >0 if this>b */
    int compare(const vbignum_t &b) const
        { return CVmObjBigNum::compare_ext(ext_, b.ext_); }

    /* addition */
    vbignum_t *operator +(long l) const
    {
        vbignum_t bl(l);
        return *this + bl;
    }
    vbignum_t *operator +(const vbignum_t &b) const
    {
        vbignum_t *sum = new vbignum_t();
        sum->init(maxprec(this, &b));
        CVmObjBigNum::compute_sum_into(sum->ext_, ext_, b.ext_);
        return sum;
    }

    /* subtraction */
    vbignum_t *operator -(long l) const
    {
        vbignum_t bl(l);
        return *this - bl;
    }
    vbignum_t *operator -(const vbignum_t &b) const
    {
        vbignum_t *res = new vbignum_t();
        res->init(maxprec(this, &b));
        CVmObjBigNum::compute_diff_into(res->ext_, ext_, b.ext_);
        return res;
    }

    /* multiplication */
    vbignum_t *operator *(long l) const
    {
        vbignum_t bl(l);
        return *this * bl;
    }
    vbignum_t *operator *(const vbignum_t &b) const
    {
        vbignum_t *res = new vbignum_t();
        res->init(maxprec(this, &b));
        CVmObjBigNum::compute_prod_into(res->ext_, ext_, b.ext_);
        return res;
    }

    /* division */
    vbignum_t *operator /(long l) const
    {
        vbignum_t bl(l);
        return *this / bl;
    }
    vbignum_t *operator /(const vbignum_t &b) const
    {
        vbignum_t *res = new vbignum_t(), rem;
        res->init(maxprec(this, &b));
        rem.init(2);
        CVmObjBigNum::compute_quotient_into(res->ext_, rem.ext_, ext_, b.ext_);
        return res;
    }

    /* remainder */
    vbignum_t *operator %(long l) const
    {
        vbignum_t bl(l);
        return *this % bl;
    }
    vbignum_t *operator %(const vbignum_t &b) const
    {
        vbignum_t *res = new vbignum_t(), quo;
        res->init(maxprec(this, &b));
        quo.init(2);
        CVmObjBigNum::compute_quotient_into(quo.ext_, res->ext_, ext_, b.ext_);
        return res;
    }

    /* get the higher of two numbers' precisions */
    static int maxprec(const vbignum_t *a, const vbignum_t *b)
        { return a->prec() > b->prec() ? a->prec() : b->prec(); }

protected:
    /* create with no extension */
    vbignum_t() { ext_ = 0; }

    /* initialize with a given precision, allocating the extension */
    void init(int prec)
    {
        ext_ = new char[CVmObjBigNum::calc_alloc(prec)];
        oswp2(ext_, prec);
        ext_[VMBN_FLAGS] = 0;
    }

    /* extension buffer - allocated with new char[prec] */
    char *ext_;
};

#endif /* VMBIGNUM_H */

/*
 *   Register the class 
 */
VM_REGISTER_METACLASS(CVmObjBigNum)
